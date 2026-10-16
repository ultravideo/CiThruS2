#pragma once

#if __has_include("fastdds/config.hpp")
#define CITHRUS_FASTDDS_AVAILABLE

#ifdef _WIN32
// Fast DDS needs this but doesn't link with it automatically for some reason
#pragma comment(lib, "shlwapi.lib")
#endif // _WIN32

// Some Fast DDS classes are missing virtual destructors which triggers this
// MSVC warning, but it doesn't seem to be a real problem
#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4265 )
#endif // _MSC_VER

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

// If your compiler is complaining that these header files are missing, they
// should be generated from the corresponding .idl files using Fast DDS-Gen.
// The setup script builds Fast DDS-Gen and puts it in ThirdParty/FastDDS/Bin
// from where it should be automatically called by UnrealBuildTool as a
// pre-build step defined in CiThruS.Target.cs and CiThruSEditor.Target.cs
#include "PubSub/DdsMessages/ParticipantEntitiesInfoPubSubTypes.hpp"
#include "PubSub/DdsMessages/Vector3PubSubTypes.hpp"
#include "PubSub/DdsMessages/QuaternionPubSubTypes.hpp"
#include "PubSub/DdsMessages/ImagePubSubTypes.hpp"
#include "PubSub/DdsMessages/CameraInfoPubSubTypes.hpp"
#include "PubSub/DdsMessages/ImuPubSubTypes.hpp"
#include "PubSub/DdsMessages/AccelStampedPubSubTypes.hpp"
#include "PubSub/DdsMessages/MagneticFieldPubSubTypes.hpp"

// So that we don't have to write the long namespace every time
namespace fastdds = eprosima::fastdds::dds;

#ifdef _MSC_VER
#pragma warning ( pop )
#endif // _MSC_VER

#else
#pragma message (__FILE__ ": warning: FastDDS not found, DDS and ROS 2 support are unavailable")
#endif // __has_include(...)

namespace Optional
{
	inline bool FastDdsAvailable()
	{
#ifdef CITHRUS_FASTDDS_AVAILABLE
		return true;
#else
		return false;
#endif // CITHRUS_FASTDDS_AVAILABLE
	}
}
