#include "DdsPublisher.h"
#include "DdsParticipant.h"
#include "PubSubCommunicator.h"
#include "UObject/Class.h"

DdsPublisher::DdsPublisher(const std::string& nodeName, const uint32_t& maxMessagesPerTopicPerSecond)
    : maxMessageRateUs_(1000000 / maxMessagesPerTopicPerSecond), rosFrameIndex_(0)
{

}

DdsPublisher::~DdsPublisher()
{

}

void DdsPublisher::Publish(const std::string& topic, UStruct* structType, void* structData)
{
	if (topic.size() == 0 || !structData)
	{
		return;
	}

    if (structType->IsChildOf(FEgoMessage::StaticStruct()))
    {
        FEgoMessage* egoVehicleMessage = static_cast<FEgoMessage*>(structData);

#ifdef CITHRUS_FASTDDS_AVAILABLE
        FTrafficRotation orientationEuler = egoVehicleMessage->Vehicle.CurrentRotation;
        FQuat orientationQuat = FQuat::MakeFromEuler(FVector(orientationEuler.Pitch, orientationEuler.Roll, orientationEuler.Yaw));
        FTrafficVelocity linearAcceleration = egoVehicleMessage->Vehicle.LinearAcceleration;
        FTrafficRotation angularVelocity = egoVehicleMessage->Vehicle.AngularVelocity;
        FTrafficRotation angularAcceleration = egoVehicleMessage->Vehicle.AngularAcceleration;

        uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        builtin_interfaces::msg::Time rosTimestamp{};
        rosTimestamp.sec(nowNs / 1000000000);
        rosTimestamp.nanosec(nowNs % 1000000000);

        std_msgs::msg::Header rosHeader{};
        rosHeader.frame_id(std::to_string(rosFrameIndex_));
        rosHeader.stamp(rosTimestamp);

        geometry_msgs::msg::Quaternion rosOrientation{};

        rosOrientation.x(orientationQuat.X);
        rosOrientation.y(orientationQuat.Y);
        rosOrientation.z(orientationQuat.Z);
        rosOrientation.w(orientationQuat.W);

        geometry_msgs::msg::Vector3 rosLinearAcceleration{};

        rosLinearAcceleration.x(linearAcceleration.Lateral);
        rosLinearAcceleration.y(linearAcceleration.Longitudinal);
        rosLinearAcceleration.z(linearAcceleration.Vertical);

        geometry_msgs::msg::Vector3 rosAngularVelocity{};

        rosAngularVelocity.x(angularVelocity.Pitch * UE_PI / 180.0f);
        rosAngularVelocity.y(angularVelocity.Roll * UE_PI / 180.0f);
        rosAngularVelocity.z(angularVelocity.Yaw * UE_PI / 180.0f);

        geometry_msgs::msg::Vector3 rosAngularAcceleration{};

        rosAngularAcceleration.x(angularAcceleration.Pitch * UE_PI / 180.0f);
        rosAngularAcceleration.y(angularAcceleration.Roll * UE_PI / 180.0f);
        rosAngularAcceleration.z(angularAcceleration.Yaw * UE_PI / 180.0f);

        sensor_msgs::msg::Imu rosImu{};

        rosImu.header(rosHeader);
        rosImu.orientation(rosOrientation);
        rosImu.orientation_covariance(
            {
                0, 0, 0,
                0, 0, 0,
                0, 0, 0
            });
        rosImu.angular_velocity(rosAngularVelocity);
        rosImu.angular_velocity_covariance(
            {
                0, 0, 0,
                0, 0, 0,
                0, 0, 0
            });
        rosImu.linear_acceleration(rosLinearAcceleration);
        rosImu.linear_acceleration_covariance(
            {
                0, 0, 0,
                0, 0, 0,
                0, 0, 0
            });

        geometry_msgs::msg::Accel rosAccel{};

        rosAccel.linear(rosLinearAcceleration);
        rosAccel.angular(rosAngularAcceleration);

        geometry_msgs::msg::AccelStamped rosAccelStamped{};

        rosAccelStamped.header(rosHeader);
        rosAccelStamped.accel(rosAccel);

        sensor_msgs::msg::MagneticField rosMagneticField{};

        rosMagneticField.header(rosHeader);

        geometry_msgs::msg::Vector3 rosMagneticFieldVector{};

        // This assumes that the magnetometer acts like a compass pointing towards the north.
        // The Earth's magnetic field is between 30-60 uT
        float magneticFieldStrength = 0.000045f;

        rosMagneticFieldVector.x(magneticFieldStrength * std::sin(orientationEuler.Yaw * UE_PI / 180.0f));
        rosMagneticFieldVector.y(magneticFieldStrength * std::cos(orientationEuler.Yaw * UE_PI / 180.0f));
        rosMagneticFieldVector.z(0.0f);

        rosMagneticField.magnetic_field(rosMagneticFieldVector);
        rosMagneticField.magnetic_field_covariance();

        // Publish in the same format as the olixSense IMU X1
        DdsParticipant::Publish(topic + "/imu", rosImu);
        DdsParticipant::Publish(topic + "/acceleration", rosAccelStamped);
        DdsParticipant::Publish(topic + "/magnetometer", rosMagneticField);

        rosFrameIndex_++;
#endif // CITHRUS_FASTDDS_AVAILABLE

        return;
    }

    // TODO: Other data should also probably be sent, but how? May require new DDS message types

    /*std::vector<DdsMessage> messages;

    // Recursively separate the properties of the struct into their own topics.
    // This allows publishing the struct using the default basic data types
    // included with ROS, which is convenient because ROS can only receive
    // messages with data types that it recognizes. If we sent the whole struct
    // under one topic, we would have to create a custom IDL definition for it
    // and also give the definition to ROS to allow it to understand it
    DdsPublisher::UStructToMessages(structType, structData, messages, topic + "/");

    for (const DdsMessage& message : messages)
    {
        const std::string& topicName = message.GetTopicName();

        auto now = std::chrono::high_resolution_clock::now();

        if (messageLastSentToTopic_.find(topicName) != messageLastSentToTopic_.end()
            && std::chrono::duration_cast<std::chrono::microseconds>(now - messageLastSentToTopic_.at(topicName)).count() < maxMessageRateUs_)
        {
            continue;
        }

        DdsParticipant::Publish(topicName, message.GetTypeName(), message.GetData());

        messageLastSentToTopic_[topicName] = now;
    }*/
}

/*void DdsPublisher::UStructToMessages(UStruct* structType, void* structData, std::vector<DdsMessage>& messages, std::string topicPrefix) const
{
#ifdef CITHRUS_FASTDDS_AVAILABLE
    // Recursively iterate the properties of the UStruct
    for (TFieldIterator<FProperty> it(structType); it; ++it)
    {
        FProperty* property = *it;

        std::string propertyName = TCHAR_TO_UTF8(*property->GetName());

        if (FStructProperty* structProperty = CastField<FStructProperty>(property))
        {
            UStructToMessages(structProperty->Struct, structProperty->ContainerPtrToValuePtr<void>(&structData), messages, topicPrefix + propertyName + "/");
        }
        else if (FIntProperty* intProperty = CastField<FIntProperty>(property))
        {
            std_msgs::msg::Int32 rosInt32;

            rosInt32.data(*intProperty->ContainerPtrToValuePtr<int32>(&structData));

            DdsMessage message = DdsMessage(
                topicPrefix + propertyName,
                DdsParticipant::GetPubSubTypeName<std_msgs::msg::Int32>(),
                rosInt32);

            messages.push_back(message);
        }
        else if (FDoubleProperty* doubleProperty = CastField<FDoubleProperty>(property))
        {
            std_msgs::msg::Float64 rosFloat64;

            rosFloat64.data(*doubleProperty->ContainerPtrToValuePtr<double>(&structData));

            DdsMessage message = DdsMessage(
                topicPrefix + propertyName,
                DdsParticipant::GetPubSubTypeName<std_msgs::msg::Float64>(),
                rosFloat64);

            messages.push_back(message);
        }
        else if (FBoolProperty* boolProperty = CastField<FBoolProperty>(property))
        {
            std_msgs::msg::Bool rosBool;

            rosBool.data(*boolProperty->ContainerPtrToValuePtr<bool>(&structData));

            DdsMessage message = DdsMessage(
                topicPrefix + propertyName,
                DdsParticipant::GetPubSubTypeName<std_msgs::msg::Bool>(),
                rosBool);

            messages.push_back(message);
        }
        else if (FStrProperty* stringProperty = CastField<FStrProperty>(property))
        {
            std_msgs::msg::String rosString;

            rosString.data(TCHAR_TO_UTF8(**stringProperty->ContainerPtrToValuePtr<FString>(&structData)));

            DdsMessage message = DdsMessage(
                topicPrefix + propertyName,
                DdsParticipant::GetPubSubTypeName<std_msgs::msg::String>(),
                rosString);

            messages.push_back(message);
        }
        else
        {
            FString propertyTypeString;

            if (FArrayProperty* arrayProperty = CastField<FArrayProperty>(property))
            {
                FString arrayTemplateParamsText;
                FString arrayTypeText = arrayProperty->GetCPPType(&arrayTemplateParamsText, CPPF_None);
                propertyTypeString = arrayTypeText + arrayTemplateParamsText;
            }
            else
            {
                propertyTypeString = property->GetCPPType();
            }

            Debug::Log("Can't publish property " + property->GetName() + " in struct type " + structType->GetName() + " because of its unsupported type " + propertyTypeString);
        }
    }
#endif // CITHRUS_FASTDDS_AVAILABLE
}*/
