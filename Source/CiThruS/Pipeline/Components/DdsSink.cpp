#include "DdsSink.h"
#include "Optional/FastDds.h"
#include "PubSub/DdsParticipant.h"
#include "Misc/Debug.h"

#include <array>
#include <chrono>

DdsSink::DdsSink(const std::string& topic,
    const uint16_t& frameWidth, const uint16_t& frameHeight,
    const float& verticalFov)
    : topic_(topic),
    frameWidth_(frameWidth), frameHeight_(frameHeight),
    frameStride_(0), verticalFov_(verticalFov),
    aspectRatio_(static_cast<float>(frameWidth) / frameHeight),
    frameIndex_(0), rosImageFormat_("error"), isBigEndian_(false)
{
    uint32_t endiannessTest = 0xFF000000;
    isBigEndian_ = reinterpret_cast<uint8_t*>(&endiannessTest)[0] == 0xFF;

	GetInputPin<0>().Initialize(this, { "rgba", "bgra", "yuv422" });
}

DdsSink::~DdsSink()
{

}

void DdsSink::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	size_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

#ifdef CITHRUS_FASTDDS_AVAILABLE
    uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    builtin_interfaces::msg::Time rosTimestamp{};
    rosTimestamp.sec(nowNs / 1000000000);
    rosTimestamp.nanosec(nowNs % 1000000000);

    std_msgs::msg::Header rosHeader{};
    rosHeader.frame_id(std::to_string(frameIndex_));
    rosHeader.stamp(rosTimestamp);

    // https://docs.ros.org/en/jade/api/sensor_msgs/html/msg/Image.html
    sensor_msgs::msg::Image rosImage{};

    rosImage.header(rosHeader);
    rosImage.height(frameHeight_);
    rosImage.width(frameWidth_);
    rosImage.encoding(rosImageFormat_);
    rosImage.is_bigendian(isBigEndian_);
    rosImage.step(frameStride_);
    rosImage.data(std::vector<uint8_t>(inputData, inputData + inputSize));

    sensor_msgs::msg::RegionOfInterest rosRoi{};

    rosRoi.x_offset(0);
    rosRoi.y_offset(0);
    rosRoi.height(frameHeight_);
    rosRoi.width(frameWidth_);
    rosRoi.do_rectify(false);

    // https://docs.ros.org/en/jade/api/sensor_msgs/html/msg/CameraInfo.html
    sensor_msgs::msg::CameraInfo rosCameraInfo{};

    rosCameraInfo.header(rosHeader);
    rosCameraInfo.height(frameHeight_);
    rosCameraInfo.width(frameWidth_);
    rosCameraInfo.distortion_model("plumb_bob");

    // d contains the distortion coefficients from here. All zeros means no distortion
    // https://docs.opencv.org/4.13.0/dc/dbb/tutorial_py_calibration.html
    rosCameraInfo.d({ 0, 0, 0, 0, 0 });

    // Focal lengths
    std::array<float, 2> f =
        {
            1.0f / (2.0f * std::tan(verticalFov_ * (UE_PI / 180.0f) / 2.0f)),
            1.0f / (2.0f * std::tan(verticalFov_ / aspectRatio_ * (UE_PI / 180.0f) / 2.0f))
        };

    // Principal point (the center of the image assuming it hasn't been cropped)
    std::array<float, 2> c =
    {
        frameWidth_ / 2,
        frameHeight_ / 2
    };

    // Camera matrix for raw image
    rosCameraInfo.k(
        {
            f[0],    0, c[0],
               0, f[1], c[1],
               0,    0,    1
        });

    // Unused, only for stereo cameras
    rosCameraInfo.r(
        {
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        });

    // Camera matrix for rectified image (same as k with non-stereo cameras)
    rosCameraInfo.p(
        {
            f[0],    0, c[0], 0,
               0, f[1], c[1], 0,
               0,    0,    1, 0
        });

    rosCameraInfo.binning_x(0);
    rosCameraInfo.binning_y(0);
    rosCameraInfo.roi(rosRoi);

    DdsParticipant::Publish(topic_ + "/image_raw", rosImage);
    DdsParticipant::Publish(topic_ + "/camera_info", rosCameraInfo);

    frameIndex_++;

#endif // CITHRUS_FASTDDS_AVAILABLE
}

void DdsSink::OnInputPinsConnected()
{
    std::string format = GetInputPin<0>().GetFormat();

    // https://docs.ros.org/en/jade/api/sensor_msgs/html/image__encodings_8h_source.html
    if (format == "rgba")
    {
        rosImageFormat_ = "rgba8";
        frameStride_ = frameWidth_ * 4;
    }
    else if (format == "bgra")
    {
        rosImageFormat_ = "bgra8";
        frameStride_ = frameWidth_ * 4;
    }
    else if (format == "yuv422")
    {
        rosImageFormat_ = "yuv422";
        frameStride_ = frameWidth_ * 2;
    }
    else
    {
        throw std::runtime_error("DdsSink does not support input format " + format);
    }
}
