#include "DdsParticipant.h"
#include "Misc/Debug.h"

void DdsParticipant::Destroy()
{
    // Shared pointer so its old value gets deleted
    singleton_ = nullptr;
}

DdsParticipant::DdsParticipant()
    : initialized_(false)
#ifdef CITHRUS_FASTDDS_AVAILABLE
    ,
    participant_(nullptr),
    publisher_(nullptr),
    rosDiscoveryTopic_()
#endif // CITHRUS_FASTDDS_AVAILABLE
{
#ifdef CITHRUS_FASTDDS_AVAILABLE
    fastdds::DomainParticipantQos participantQos;
    participantQos.name("Participant_publisher");
    participant_ = fastdds::DomainParticipantFactory::get_instance()->create_participant_with_default_profile();

    if (participant_ == nullptr)
    {
        Debug::Log("Failed to create DDS participant");
        return;
    }

    rmw_dds_common::msg::Gid participantGid{};
    participantGid.data(DdsGuidToRosGid(participant_->guid()));
    rosDiscoveryInfo_.gid(participantGid);

    // DDS topic data types have to be registered before they can be used
    RegisterPubSubType<rmw_dds_common::msg::ParticipantEntitiesInfoPubSubType>();
    RegisterPubSubType<geometry_msgs::msg::Vector3PubSubType>();
    RegisterPubSubType<geometry_msgs::msg::QuaternionPubSubType>();
    RegisterPubSubType<sensor_msgs::msg::ImagePubSubType>();
    RegisterPubSubType<sensor_msgs::msg::CameraInfoPubSubType>();
    RegisterPubSubType<sensor_msgs::msg::ImuPubSubType>();
    RegisterPubSubType<geometry_msgs::msg::AccelStampedPubSubType>();
    RegisterPubSubType<sensor_msgs::msg::MagneticFieldPubSubType>();

    fastdds::PublisherQos pubQos = fastdds::PUBLISHER_QOS_DEFAULT;
    participant_->get_default_publisher_qos(pubQos);
    publisher_ = participant_->create_publisher(pubQos);

    if (publisher_ == nullptr)
    {
        Debug::Log("Failed to create DDS publisher");
        return;
    }

    // For ROS 2 to recognize CiThruS2 as a ROS node, it needs to be advertised on the DDS topic ros_discovery_info
    // https://design.ros2.org/articles/Node_to_Participant_mapping.html

    fastdds::TopicQos topicQos = fastdds::TOPIC_QOS_DEFAULT;
    participant_->get_default_topic_qos(topicQos);

    fastdds::Topic* ddsTopic = participant_->create_topic(
        "ros_discovery_info",
        GetPubSubTypeNameInternal<rmw_dds_common::msg::ParticipantEntitiesInfo>(),
        topicQos);

    if (ddsTopic == nullptr)
    {
        Debug::Log("Failed to create ROS 2 discovery topic! ROS will not be able to see CiThruS");
        return;
    }

    fastdds::DataWriterQos writerQos = fastdds::DATAWRITER_QOS_DEFAULT;
    writerQos.durability().durabilityKind(eprosima::fastdds::rtps::TRANSIENT_LOCAL);
    writerQos.history().kind = fastdds::KEEP_LAST_HISTORY_QOS;
    writerQos.history().depth = 1;
    writerQos.reliability().kind = fastdds::RELIABLE_RELIABILITY_QOS;

    // TODO ideally all topics should have a type hash like this, but
    // calculating them first requires converting the IDL to JSON in a specific
    // way and then calculating a SHA256 hash from the JSON text... ROS doesn't
    // seem to check the hash, though, so it should be okay if they're missing
    std::string userData = "typehash=RIHS01_91a0593bacdcc50ea9bdcf849a938b128412cc1ea821245c663bcd26f83c295e;";
    writerQos.user_data().setValue(std::vector<unsigned char>(userData.data(), userData.data() + userData.size()));

    fastdds::DataWriter* writer = publisher_->create_datawriter(ddsTopic, writerQos, &rosDiscoveryListener_);

    if (writer == nullptr)
    {
        Debug::Log("Failed to create ROS 2 discovery writer! ROS will not be able to see CiThruS");
        return;
    }

    rosDiscoveryTopic_ = TopicWrapper{ ddsTopic, writer };
    OnRosDiscoveryInfoUpdated();

    initialized_ = true;
#endif // CITHRUS_FASTDDS_AVAILABLE
}

DdsParticipant::~DdsParticipant()
{
#ifdef CITHRUS_FASTDDS_AVAILABLE
    if (participant_ == nullptr)
    {
        return;
    }

    rosDiscoveryInfo_.node_entities_info_seq().clear();

    if (publisher_ != nullptr)
    {
        if (rosDiscoveryTopic_.writer != nullptr)
        {
            // Write the discovery data one more time so subscribers know our nodes have been destroyed
            OnRosDiscoveryInfoUpdated();
            publisher_->delete_datawriter(rosDiscoveryTopic_.writer);
            rosDiscoveryTopic_.writer = nullptr;
        }

        if (rosDiscoveryTopic_.topic != nullptr)
        {
            participant_->delete_topic(rosDiscoveryTopic_.topic);
            rosDiscoveryTopic_.topic = nullptr;
        }

        for (std::pair<std::string, TopicWrapper> topics : publishedTopics_)
        {
            publisher_->delete_datawriter(topics.second.writer);
            participant_->delete_topic(topics.second.topic);
        }

        publishedTopics_.clear();
        pubSubTypes_.clear();

        participant_->delete_publisher(publisher_);
        publisher_ = nullptr;
    }

    fastdds::DomainParticipantFactory::get_instance()->delete_participant(participant_);
    participant_ = nullptr;
#endif // CITHRUS_FASTDDS_AVAILABLE
}

#ifdef CITHRUS_FASTDDS_AVAILABLE
void DdsParticipant::PublishInternal(const std::string& topicName, const std::string& typeName, const void* data)
{
    std::string nodeName = "";
    size_t firstSlash = topicName.find_first_of('/');

    if (firstSlash != std::string::npos
        && firstSlash != topicName.size() - 1)
    {
        nodeName = topicName.substr(0, firstSlash);
    }

    auto rosNode = publishedRosNodes_.find(nodeName);
    int rosNodeIndex = 0;

    if (rosNode == publishedRosNodes_.end())
    {
        rosNodeIndex = rosDiscoveryInfo_.node_entities_info_seq().size();
        publishedRosNodes_.insert({ nodeName, rosDiscoveryInfo_.node_entities_info_seq().size() });

        rmw_dds_common::msg::NodeEntitiesInfo nodeInfo{};

        nodeInfo.node_name(nodeName);

        rosDiscoveryInfo_.node_entities_info_seq().push_back(nodeInfo);

        OnRosDiscoveryInfoUpdated();
    }
    else
    {
        rosNodeIndex = rosNode->second;
    }

    auto publishedTopic = publishedTopics_.find(topicName);

    if (publishedTopic == publishedTopics_.end())
    {
        fastdds::TopicQos topicQos = fastdds::TOPIC_QOS_DEFAULT;
        participant_->get_default_topic_qos(topicQos);

        // DDS topics with rt/ at the beginning are interpreted as ROS topics by ROS 2
        // https://design.ros2.org/articles/topic_and_service_names.html
        std::string rosTopicName = "rt/" + topicName;

        fastdds::Topic* ddsTopic = participant_->create_topic(rosTopicName, typeName, topicQos);

        if (ddsTopic == nullptr)
        {
            Debug::Log("Failed to create DDS topic " + rosTopicName);

            return;
        }

        fastdds::DataWriterQos writerQos = fastdds::DATAWRITER_QOS_DEFAULT;
        writerQos.durability().durabilityKind(eprosima::fastdds::rtps::TRANSIENT_LOCAL);
        writerQos.history().kind = fastdds::KEEP_LAST_HISTORY_QOS;
        writerQos.history().depth = 1;
        writerQos.reliability().kind = fastdds::RELIABLE_RELIABILITY_QOS;

        fastdds::DataWriter* writer = publisher_->create_datawriter(ddsTopic, writerQos);

        if (writer == nullptr)
        {
            Debug::Log("Failed to create DDS writer for topic " + rosTopicName);

            return;
        }

        publishedTopic = publishedTopics_.insert(std::pair{ topicName, TopicWrapper{ ddsTopic, writer } }).first;

        // Advertise new writers on the ROS discovery topic so that it sees them
        rmw_dds_common::msg::Gid newTopicGid{};
        newTopicGid.data(DdsGuidToRosGid(writer->guid()));

        rosDiscoveryInfo_.node_entities_info_seq().at(rosNodeIndex).writer_gid_seq().push_back(newTopicGid);

        OnRosDiscoveryInfoUpdated();
    }

    TopicWrapper& topicWrapper = publishedTopic->second;

    topicWrapper.writer->write(data);
}

std::array<uint8_t, 16> DdsParticipant::DdsGuidToRosGid(const eprosima::fastdds::rtps::GUID_t& guid) const
{
    std::array<uint8_t, 16> guidBytes{};

    std::copy(guid.guidPrefix.value, guid.guidPrefix.value + 12, guidBytes.data());
    std::copy(guid.entityId.value, guid.entityId.value + 4, guidBytes.data() + 12);

    return guidBytes;
}

void DdsParticipant::OnRosDiscoveryInfoUpdated()
{
    rosDiscoveryTopic_.writer->write(&rosDiscoveryInfo_);
}
#endif // CITHRUS_FASTDDS_AVAILABLE
