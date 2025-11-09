#!/usr/bin/env python3
"""
SDP Server for HEVC RTP Streaming

This server:
1. Listens to RTP packets from Unreal (localhost:12300)
2. Extracts VPS/SPS/PPS from the first keyframe
3. Generates SDP with base64-encoded parameter sets
4. Serves SDP over HTTP for GStreamer clients
5. Forwards RTP packets to a different port for actual streaming

Usage:
    python3 sdp_server_example.py

Then in GStreamer clients:
    gst-launch-1.0 playbin uri=http://localhost:8080/stream.sdp
"""

import socket
import base64
import http.server
import socketserver
import threading
from typing import Optional, Tuple

class HEVCParameterSets:
    """Stores extracted HEVC parameter sets"""
    def __init__(self):
        self.vps: Optional[bytes] = None
        self.sps: Optional[bytes] = None
        self.pps: Optional[bytes] = None
        self.ready = False
    
    def is_complete(self) -> bool:
        return self.vps is not None and self.sps is not None and self.pps is not None

class RTPListener:
    """Listens for RTP packets and extracts HEVC parameter sets"""
    
    def __init__(self, listen_port: int, forward_port: int):
        self.listen_port = listen_port
        self.forward_port = forward_port
        self.params = HEVCParameterSets()
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', listen_port))
        self.forward_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
    def parse_nal_units(self, payload: bytes) -> list:
        """Parse Annex-B formatted payload into NAL units"""
        nals = []
        offset = 0
        
        while offset < len(payload):
            # Find start code
            if offset + 3 <= len(payload) and payload[offset:offset+3] == b'\x00\x00\x01':
                start_code_len = 3
            elif offset + 4 <= len(payload) and payload[offset:offset+4] == b'\x00\x00\x00\x01':
                start_code_len = 4
            else:
                break
            
            nal_start = offset + start_code_len
            
            # Find next start code
            next_offset = nal_start
            while next_offset < len(payload) - 2:
                if payload[next_offset:next_offset+3] == b'\x00\x00\x01' or \
                   (next_offset < len(payload) - 3 and payload[next_offset:next_offset+4] == b'\x00\x00\x00\x01'):
                    break
                next_offset += 1
            
            nal_end = next_offset if next_offset < len(payload) - 2 else len(payload)
            nal_data = payload[nal_start:nal_end]
            
            if nal_data:
                nal_type = (nal_data[0] & 0x7E) >> 1
                nals.append((nal_type, nal_data))
            
            offset = nal_end
        
        return nals
    
    def extract_rtp_payload(self, packet: bytes) -> Optional[bytes]:
        """Extract H.265 payload from RTP packet"""
        if len(packet) < 12:
            return None
        
        # RTP header is 12 bytes minimum
        # Skip header and any extensions
        rtp_version = (packet[0] >> 6) & 0x03
        has_padding = (packet[0] >> 5) & 0x01
        has_extension = (packet[0] >> 4) & 0x01
        csrc_count = packet[0] & 0x0F
        
        if rtp_version != 2:
            return None
        
        header_len = 12 + (csrc_count * 4)
        
        if has_extension:
            if len(packet) < header_len + 4:
                return None
            ext_len = int.from_bytes(packet[header_len+2:header_len+4], 'big') * 4
            header_len += 4 + ext_len
        
        if header_len >= len(packet):
            return None
        
        return packet[header_len:]
    
    def process_packet(self, packet: bytes):
        """Process RTP packet and extract parameter sets if found"""
        payload = self.extract_rtp_payload(packet)
        if not payload:
            return
        
        # Check if this is Annex-B format
        if not (payload.startswith(b'\x00\x00\x00\x01') or payload.startswith(b'\x00\x00\x01')):
            # Might be raw NAL units from uvgRTP - check NAL type
            if len(payload) > 0:
                nal_type = (payload[0] & 0x7E) >> 1
                if nal_type == 32 and not self.params.vps:
                    self.params.vps = payload
                    print(f"Extracted VPS: {len(payload)} bytes")
                elif nal_type == 33 and not self.params.sps:
                    self.params.sps = payload
                    print(f"Extracted SPS: {len(payload)} bytes")
                elif nal_type == 34 and not self.params.pps:
                    self.params.pps = payload
                    print(f"Extracted PPS: {len(payload)} bytes")
            
            # Check if all parameter sets are now complete
            if self.params.is_complete() and not self.params.ready:
                self.params.ready = True
                print("✅ All parameter sets extracted! SDP is ready.")
            return
        
        # Parse Annex-B formatted payload
        nals = self.parse_nal_units(payload)
        for nal_type, nal_data in nals:
            if nal_type == 32 and not self.params.vps:  # VPS
                self.params.vps = nal_data
                print(f"Extracted VPS: {len(nal_data)} bytes")
            elif nal_type == 33 and not self.params.sps:  # SPS
                self.params.sps = nal_data
                print(f"Extracted SPS: {len(nal_data)} bytes")
            elif nal_type == 34 and not self.params.pps:  # PPS
                self.params.pps = nal_data
                print(f"Extracted PPS: {len(nal_data)} bytes")
        
        if self.params.is_complete() and not self.params.ready:
            self.params.ready = True
            print("✅ All parameter sets extracted! SDP is ready.")
    
    def run(self):
        """Listen for RTP packets and forward them"""
        print(f"Listening for RTP packets on port {self.listen_port}...")
        print(f"Forwarding packets to port {self.forward_port}...")
        
        while True:
            try:
                packet, addr = self.socket.recvfrom(65535)
                
                # Extract parameter sets if not yet complete
                if not self.params.ready:
                    self.process_packet(packet)
                
                # Forward packet to GStreamer clients
                self.forward_socket.sendto(packet, ('127.0.0.1', self.forward_port))
                
            except Exception as e:
                print(f"Error processing packet: {e}")

class SDPHandler(http.server.BaseHTTPRequestHandler):
    """HTTP handler that serves SDP file"""
    
    params: Optional[HEVCParameterSets] = None
    forward_port: int = 12301
    
    def do_GET(self):
        if self.path == '/stream.sdp' or self.path == '/':
            if not self.params or not self.params.ready:
                self.send_response(503)
                self.end_headers()
                self.wfile.write(b"Parameter sets not yet extracted. Please start streaming from Unreal first.")
                return
            
            # Generate SDP with extracted parameter sets
            vps_b64 = base64.b64encode(self.params.vps).decode('ascii')
            sps_b64 = base64.b64encode(self.params.sps).decode('ascii')
            pps_b64 = base64.b64encode(self.params.pps).decode('ascii')
            
            sdp_content = f"""v=0
o=- 0 0 IN IP4 127.0.0.1
s=HEVC Stream from Unreal
c=IN IP4 127.0.0.1
t=0 0
m=video {self.forward_port} RTP/AVP 96
a=rtpmap:96 H265/90000
a=fmtp:96 sprop-vps={vps_b64}; sprop-sps={sps_b64}; sprop-pps={pps_b64}
"""
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/sdp')
            self.send_header('Content-Length', str(len(sdp_content)))
            self.end_headers()
            self.wfile.write(sdp_content.encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()
    
    def log_message(self, format, *args):
        # Suppress request logging
        pass

def main():
    UNREAL_PORT = 12300      # Port where Unreal sends RTP
    FORWARD_PORT = 12301     # Port where GStreamer receives RTP
    HTTP_PORT = 8080         # Port for SDP HTTP server
    
    # Start RTP listener/forwarder
    rtp_listener = RTPListener(UNREAL_PORT, FORWARD_PORT)
    rtp_thread = threading.Thread(target=rtp_listener.run, daemon=True)
    rtp_thread.start()
    
    # Set up HTTP server for SDP
    SDPHandler.params = rtp_listener.params
    SDPHandler.forward_port = FORWARD_PORT
    
    with socketserver.TCPServer(("", HTTP_PORT), SDPHandler) as httpd:
        print(f"\n{'='*60}")
        print(f"SDP Server started!")
        print(f"{'='*60}")
        print(f"1. Start your Unreal application (sending to 127.0.0.1:{UNREAL_PORT})")
        print(f"2. Wait for parameter sets to be extracted (VPS/SPS/PPS)")
        print(f"3. Connect GStreamer client:")
        print(f"   gst-launch-1.0 playbin uri=http://localhost:{HTTP_PORT}/stream.sdp")
        print(f"{'='*60}\n")
        
        httpd.serve_forever()

if __name__ == '__main__':
    main()
