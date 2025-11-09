# GStreamer Gray Frames Solution

## Problem Summary

HEVC RTP streaming from Unreal Engine showed:
- ✅ **VLC**: Works perfectly
- ❌ **GStreamer**: Gray/garbled frames with `avdec_h265`

## Root Cause

**GStreamer's `rtph265depay` was defaulting to `hvc1` (HVCC/length-prefixed) stream format instead of `byte-stream` (Annex-B) format.**

When rtph265depay negotiates with downstream elements, it prefers `hvc1` format because:
1. It's more compact (no start codes, length-prefixed NAL units)
2. Many decoders support both formats

However, `avdec_h265` (FFmpeg's libavcodec) in GStreamer expects **byte-stream format with Annex-B start codes** when receiving data without explicit codec configuration.

### Debug Evidence

```bash
GST_DEBUG=rtph265depay:6 gst-launch-1.0 udpsrc port=12300 ! ...
```

Output showed:
```
DEBUG rtph265depay: allowed caps: video/x-h265, stream-format=(string)hvc1, alignment=(string)au; video/x-h265, stream-format=(string)byte-stream, alignment=(string){ nal, au }
DEBUG rtph265depay: downstream wants stream-format hvc1
```

## Solution

**Force byte-stream format by adding explicit caps after rtph265depay:**

```bash
gst-launch-1.0 \
  udpsrc port=12300 \
  ! application/x-rtp,media=video,encoding-name=H265,clock-rate=90000,payload=96 \
  ! rtph265depay \
  ! video/x-h265,stream-format=byte-stream \
  ! h265parse \
  ! avdec_h265 \
  ! autovideosink sync=false
```

The key addition is:
```bash
! video/x-h265,stream-format=byte-stream \
```

This forces rtph265depay to output in Annex-B format, which avdec_h265 handles correctly.

## Why This Works

1. **rtph265depay** receives RTP packets with individual NAL units
2. When outputting as `byte-stream`, it:
   - Adds Annex-B start codes (0x00 0x00 0x00 0x01) before each NAL
   - Maintains parameter sets (VPS/SPS/PPS) in-stream
   - Outputs proper Annex-B format that matches what the encoder produces
3. **h265parse** validates the stream
4. **avdec_h265** decodes correctly

## Alternative: Use vtdec_hw (Hardware Decoder)

On macOS, you can also use VideoToolbox hardware decoder which handles both formats:

```bash
gst-launch-1.0 \
  udpsrc port=12300 \
  ! application/x-rtp,media=video,encoding-name=H265,clock-rate=90000,payload=96 \
  ! rtph265depay \
  ! h265parse \
  ! vtdec_hw \
  ! autovideosink sync=false
```

However, `vtdec_hw` may have different latency characteristics.

## Testing

Run the provided test script:

```bash
./test_gstreamer.sh
```

Expected result: **Smooth playback with no gray frames**

## Notes

1. The "Approach 3" workaround (injecting parameter sets on every frame) is still beneficial as it ensures GStreamer can start decoding immediately without waiting for a keyframe
2. The real issue was stream format negotiation, not missing parameter sets
3. VLC worked because it automatically handles both `hvc1` and `byte-stream` formats transparently

## Implementation in Your Tooling

If you're building GStreamer-based tools, ensure your pipeline includes:

```python
# Python GStreamer example
pipeline_str = """
    udpsrc port=12300
    ! application/x-rtp,media=video,encoding-name=H265,clock-rate=90000,payload=96
    ! rtph265depay
    ! video/x-h265,stream-format=byte-stream
    ! h265parse
    ! avdec_h265
    ! ... your processing ...
"""
```

Or in C:
```c
GstElement *depay = gst_element_factory_make("rtph265depay", NULL);
GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
GstCaps *caps = gst_caps_from_string("video/x-h265,stream-format=byte-stream");
g_object_set(capsfilter, "caps", caps, NULL);
gst_caps_unref(caps);

// Link: ... -> depay -> capsfilter -> h265parse -> ...
```

## References

- GStreamer rtph265depay: https://gstreamer.freedesktop.org/documentation/rtp/rtph265depay.html
- RFC 7798: RTP Payload Format for HEVC
- H.265 Annex-B byte stream format: ITU-T H.265 specification

---

**Date**: 2025-11-09  
**Status**: ✅ Resolved
