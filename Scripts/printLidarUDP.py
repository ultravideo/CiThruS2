import socket
# Dumps MetaLidar / Velodyne UDP packets.
#
# MetaLidar never sends position packets (port 8308, GeneratePositionPacket is dead code),
# Only run one listener on this port at a time (this or CiThruS Lidar Visualizer):

UDP_IP = "0.0.0.0"
UDP_PORT = 2368

scan_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
scan_sock.bind((UDP_IP, UDP_PORT))
print(f"Listening on {UDP_IP}:{UDP_PORT}...")

packet_count = 0
while True:
    scan_data, scan_addr = scan_sock.recvfrom(2048)
    packet_count += 1
    # Velodyne data packets are 1206 bytes; print a heartbeat instead of drowning the console
    if packet_count % 100 == 1:
        print(f"#{packet_count}: {len(scan_data)} bytes from {scan_addr}, "
              f"first block: {scan_data[:8].hex(' ')}")