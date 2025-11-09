#!/bin/bash
#
# Test GStreamer HEVC RTP reception with forced byte-stream format
#

echo "====================================================================="
echo "Testing GStreamer H.265 RTP Reception"
echo "====================================================================="
echo ""
echo "SOLUTION: Force byte-stream format after rtph265depay"
echo ""
echo "The issue was that rtph265depay was defaulting to 'hvc1' (HVCC/length-prefixed)"
echo "format instead of 'byte-stream' (Annex-B) format. This caused avdec_h265 to fail."
echo ""
echo "Pipeline:"
echo "  udpsrc → rtph265depay → video/x-h265,stream-format=byte-stream → h265parse → avdec_h265 → autovideosink"
echo ""
echo "====================================================================="
echo ""

gst-launch-1.0 -v \
  udpsrc port=12300 \
  ! application/x-rtp,media=video,encoding-name=H265,clock-rate=90000,payload=96 \
  ! rtph265depay \
  ! video/x-h265,stream-format=byte-stream \
  ! h265parse \
  ! avdec_h265 \
  ! fpsdisplaysink text-overlay=false sync=false

echo ""
echo "====================================================================="
echo "Test complete"
echo "====================================================================="
