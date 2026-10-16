import socket
import struct
import time

# Sends synthetic Velodyne VLP-16 data packets to 127.0.0.1:2368.
# 12 blocks x [FF EE][azimuth][32 x (distance, intensity)] + 6 tail bytes = 1206 bytes. 

DEST = ("127.0.0.1", 2368)

BLOCKS_PER_PACKET = 12
CHANNELS_PER_BLOCK = 32
AZIMUTH_STEP_DEG = 0.2          # per firing; a block advances 2 firings
RANGE_M = 10.0
RAW_DISTANCE = int(RANGE_M * 100 * 5)   # metres -> cm -> 2 mm units
INTENSITY = 100
PACKETS_PER_SECOND = 754        # real VLP-16 rate at 10 Hz rotation

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
print(f"Sending fake VLP-16 packets to {DEST[0]}:{DEST[1]} at ~{PACKETS_PER_SECOND}/s...")

azimuth_deg = 0.0
packet_count = 0
batch_start = time.monotonic()

while True:
    packet = bytearray()

    for _ in range(BLOCKS_PER_PACKET):
        packet += b"\xff\xee"
        packet += struct.pack("<H", int(azimuth_deg * 100) % 36000)
        for _ in range(CHANNELS_PER_BLOCK):
            packet += struct.pack("<H", RAW_DISTANCE)
            packet.append(INTENSITY)
        azimuth_deg = (azimuth_deg + 2 * AZIMUTH_STEP_DEG) % 360.0

    packet += struct.pack("<I", packet_count)   # timestamp
    packet += bytes([55, 34])                   # return mode "Strongest", model VLP-16

    assert len(packet) == 1206
    sock.sendto(packet, DEST)
    packet_count += 1

    if packet_count % 50 == 0:
        # Crude rate limiting in 50-packet batches
        elapsed = time.monotonic() - batch_start
        target = 50 / PACKETS_PER_SECOND
        if elapsed < target:
            time.sleep(target - elapsed)
        batch_start = time.monotonic()

    if packet_count % 1000 == 0:
        print(f"sent {packet_count} packets (azimuth now {azimuth_deg:.1f} deg)")
