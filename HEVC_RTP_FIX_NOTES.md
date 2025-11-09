# HEVC RTP Transmission Diagnosis and Fix

## Problem Summary

HEVC-encoded video transmitted over RTP was experiencing severe corruption:
- **Choppy playback** with GStreamer's `vtdec` decoder  
- **Fast but corrupted (gray frames)** with GStreamer's `avdec_h265` decoder
- **Highly variable frame sizes** (700 bytes to 65KB) suggesting packetization issues
- Occurred on **all resolutions** and with both **VideoToolbox** and **Kvazaar** encoders

## Root Cause

**The RTP transmitter was using `RCE_FRAGMENT_GENERIC` flag**, which is intended for generic payloads (like raw audio) that don't have format-specific packetization. This flag caused uvgRTP to treat H.265 frames as opaque data blobs and fragment them generically, **breaking proper NAL unit boundaries and RFC 7798 H.265 RTP packetization**.

###How uvgRTP Handles H.265 (RFC 7798)

According to uvgRTP documentation (`util.hh` lines 149-154):

> By default, uvgRTP searches for start code prefixes (0x000001 or 0x00000001) from the frame to divide NAL units and remove the prefix. If you instead want to provide the NAL units without the start code prefix yourself, you may use `RTP_NO_H26X_SCL` flag to disable Start Code Lookup (SCL).

When `RTP_FORMAT_H265` is specified **without** `RCE_FRAGMENT_GENERIC`, uvgRTP:
1. Parses the input buffer for Annex-B start codes (0x00000001 or 0x000001)
2. Extracts individual NAL units (removing start codes)
3. Applies proper RFC 7798 H.265 RTP packetization:
   - **Single NAL Unit Packets** for small NALs (< MTU)
   - **Aggregation Packets (AP)** to combine multiple small NALs
   - **Fragmentation Units (FU)** for large NALs (> MTU)

## Changes Made

### 1. Added Comprehensive NAL Unit Logging (HevcEncoder.cpp)

Added helper functions to inspect and log HEVC NAL units:
- `StartsWithAnnexB()` - Detects Annex-B start codes
- `FindNextStartCode()` - Locates NAL boundaries
- `HevcNalType()` - Extracts NAL type from header
- `NalTypeName()` - Human-readable NAL type names
- `LogAnnexBAU()` - Parses and logs Annex-B access units
- `LogHvccAU()` - Parses and logs length-prefixed (HVCC) access units

### 2. Instrumented VideoToolbox Encoder (HevcEncoder.cpp)

Modified `HandleEncodedFrame()` to:
- Log parameter set injection (VPS/SPS/PPS) for keyframes
- Log first 8 bytes of output in hex for comparison
- Log complete NAL unit breakdown (types, sizes, flags)
- Verify VPS/SPS/PPS presence on IDR frames

Example log output:
```
HevcEncoder: Injecting parameter set NAL type=32 (VPS), size=23
HevcEncoder: Injecting parameter set NAL type=33 (SPS), size=42
HevcEncoder: Injecting parameter set NAL type=34 (PPS), size=8
HevcEncoder: Output 65126 bytes, head=00 00 00 01 40 01 0C 01
  NAL #0: type=32 (VPS), size=23
  NAL #1: type=33 (SPS), size=42
  NAL #2: type=34 (PPS), size=8
  NAL #3: type=19 (IDR_W_RADL), size=65024
HevcEncoder: AU size=65126, keyframe=1 (VPS=1 SPS=1 PPS=1 IDR=1), NALs=4
```

### 3. Added RTP Transmitter Logging (RtpTransmitter.cpp)

Added before `push_frame()`:
- Log payload size and first 8 bytes in hex
- Detect and log Annex-B format (true/false)
- Log first NAL type for Annex-B payloads
- Count and log number of NAL units in buffer

Example log output:
```
RtpTransmitter: sending 65126 bytes, head=00 00 00 01 40 01 0C 01
RtpTransmitter: annexb=true, firstNAL=32, nalCount=4
```

### 4. **Fixed RTP Packetization** (RtpTransmitter.cpp) ⭐

**CRITICAL FIX**: Removed `RCE_FRAGMENT_GENERIC` flag from stream creation:

```cpp
// BEFORE (BROKEN):
stream_ = streamSession_->create_stream(0, dstPort, RTP_FORMAT_H265, 
                                       RCE_FRAGMENT_GENERIC | RCE_PACE_FRAGMENT_SENDING);

// AFTER (CORRECT):
stream_ = streamSession_->create_stream(0, dstPort, RTP_FORMAT_H265, 
                                       RCE_PACE_FRAGMENT_SENDING);
```

Added extensive documentation explaining why `RCE_FRAGMENT_GENERIC` must not be used with H.265.

## Testing Instructions

1. **Build and run** the updated code with Unreal Engine
2. **Start GStreamer receiver**:
   ```bash
   gst-launch-1.0 -v udpsrc port=12300 ! application/x-rtp,payload=96 \
   ! rtph265depay ! h265parse ! avdec_h265 ! fpsdisplaysink text-overlay=false
   ```
3. **Check logs** for:
   - **HevcEncoder**: Keyframes should show VPS/SPS/PPS/IDR present; inter-frames smaller (700-1600 bytes is normal for P-frames)
   - **RtpTransmitter**: Confirm `annexb=true` and reasonable NAL counts (1-4 for keyframes, 1 for inter-frames)
4. **Verify playback**: Should be smooth with no gray frames or choppiness

## Expected Behavior

### Keyframe (I-frame, ~60-frame GOP)
- **Size**: 30KB - 100KB (resolution-dependent)
- **NALs**: VPS (type 32), SPS (type 33), PPS (type 34), IDR (type 19 or 20)
- **RTP**: Multiple RTP packets using Single NAL or Fragmentation Units

### Inter-frame (P-frame / B-frame)
- **Size**: 500 bytes - 5KB (depends on motion)
- **NALs**: TRAIL_R (type 1) or TRAIL_N (type 0)
- **RTP**: Typically 1 RTP packet (Single NAL), or few Fragmentation Units if larger

## Additional Notes

- **Annex-B format is correct**: uvgRTP expects and handles it properly for H.265
- **MTU = 1200**: Conservative for WAN; can increase to 1452 for LANs
- **Clock rate = 90000**: Standard for H.265 (90 kHz)
- **Payload type = 96**: Dynamic payload type for H.265
- **RCE_PACE_FRAGMENT_SENDING**: Spreads fragment sending within frame interval (good for network smoothness)

## References

- **RFC 7798**: RTP Payload Format for High Efficiency Video Coding (HEVC)
- **uvgRTP Documentation**: `/ThirdParty/uvgRTP/Include/util.hh`
- **HEVC/H.265 NAL Unit Types**: ITU-T H.265 specification section 7.4.2.2

## Performance Metrics

After fix, expect logs similar to:
```
HevcEncoder PERF (30 frames avg): BufCreate=0.14ms, YUVConv=0.56ms, Encode=13.79ms, Total=14.49ms (69.0 fps, 1104 bytes)
```

The varying sizes (1104, 1381, 65126, 29817 bytes) are **normal**:
- Small sizes = P-frames between keyframes
- Large sizes = I-frames with parameter sets

## GStreamer Configuration Issue (SOLUTION)

If NAL units are being sent correctly but gray frames persist, the issue is **GStreamer's rtph265depay struggling with packet reassembly**.

### Solution 1: Use out-of-band parameter sets (SDP)

GStreamer prefers parameter sets delivered out-of-band via SDP rather than in-band with keyframes:

```bash
# Extract VPS/SPS/PPS from first keyframe (hex)
# VPS, SPS, PPS need to be base64 encoded for SDP

# Then use:
gst-launch-1.0 udpsrc port=12300 caps="application/x-rtp,media=video,clock-rate=90000,encoding-name=H265,payload=96,sprop-vps=(string)<base64_vps>,sprop-sps=(string)<base64_sps>,sprop-pps=(string)<base64_pps>" ! rtph265depay ! h265parse ! avdec_h265 ! autovideosink
```

### Solution 2: Use config-interval in sender

Alternatively, configure uvgRTP or use a different RTP library that supports config-interval to repeatedly send parameter sets.

### Solution 3: Test with ffplay (simpler)

ffplay has better tolerance for RTP streams:

```bash
# Create SDP file
cat > stream.sdp << EOF
v=0
o=- 0 0 IN IP4 127.0.0.1
s=HEVC Stream
c=IN IP4 127.0.0.1
t=0 0
m=video 12300 RTP/AVP 96
a=rtpmap:96 H265/90000
EOF

ffplay -protocol_whitelist "file,udp,rtp" stream.sdp
```

### Solution 4: Switch to lower-level testing

Use VLC or another player that's more forgiving with RTP:
```bash
vlc rtp://127.0.0.1:12300
```

## Troubleshooting

If issues persist after applying the fix:

1. **Check NAL types in logs**: Ensure IDR frames (19/20) appear periodically ✅ (Working)
2. **Verify parameter sets**: Every IDR should have VPS/SPS/PPS ✅ (Working)
3. **Verify NAL parsing**: Should show "Parsed X NAL units" ✅ (Working - 4-5 NALs per keyframe)
4. **Check push_frame errors**: Should be no RTP_ERROR ✅ (Working - no errors)
5. **GStreamer compatibility**: Try ffplay or VLC if GStreamer fails ⚠️ (Current issue)
6. **Test with file dump**: Enable `CITHRUS_HEVC_DUMP` to dump .h265 file and test with `ffplay`
7. **Network issues**: Check for packet loss with `iftop` or `tcpdump`

---

**Date**: 2025-01-08  
**Author**: Warp AI Assistant  
**Status**: Ready for testing
