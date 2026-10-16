#pragma once

#include "Optional/FastDds.h"
#include "Misc/Debug.h"

#include <string>
#include <atomic>
#include <cstdint>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <stdexcept>

// Creates a global DDS participant that any part of CiThruS can use
class DdsParticipant
{
public:
	~DdsParticipant();

    template<class T>
    static void Publish(const std::string& topic, T message)
    {
#ifdef CITHRUS_FASTDDS_AVAILABLE
        DdsParticipant* singleton = Get();

        if (!singleton->initialized_ || topic.size() == 0)
        {
            return;
        }

        singleton->PublishInternal(topic, singleton->GetPubSubTypeNameInternal<T>(), &message);
#endif // CITHRUS_FASTDDS_AVAILABLE
    }

    static void Publish(const std::string& topic, const std::string& typeName, const void* data)
    {
#ifdef CITHRUS_FASTDDS_AVAILABLE
        DdsParticipant* singleton = Get();

        if (!singleton->initialized_ || topic.size() == 0)
        {
            return;
        }

        singleton->PublishInternal(topic, typeName, data);
#endif // CITHRUS_FASTDDS_AVAILABLE
    }

    template<class T>
    static std::string GetPubSubTypeName()
    {
#ifdef CITHRUS_FASTDDS_AVAILABLE
        DdsParticipant* singleton = Get();

        return singleton->GetPubSubTypeNameInternal<T>();
#else
        return "";
#endif
    }

    static void Destroy();

private:
    static inline std::shared_ptr<DdsParticipant> singleton_ = nullptr;

    bool initialized_;

    static DdsParticipant* Get()
    {
        if (singleton_ == nullptr)
        {
            singleton_ = std::shared_ptr<DdsParticipant>(new DdsParticipant());
        }

        return singleton_.get();
    }

    DdsParticipant();

#ifdef CITHRUS_FASTDDS_AVAILABLE
    struct TopicWrapper
    {
        fastdds::Topic* topic;
        fastdds::DataWriter* writer;
    };

    class PubListener : public fastdds::DataWriterListener
    {
    public:

        PubListener()
            : matched_(0)
        {
        }

        ~PubListener() override
        {
        }

        void on_publication_matched(
            fastdds::DataWriter*,
            const fastdds::PublicationMatchedStatus& info) override
        {
            if (info.current_count_change == 1)
            {
                matched_ = info.total_count;
                Debug::Log("DDS publisher matched");
            }
            else if (info.current_count_change == -1)
            {
                matched_ = info.total_count;
                Debug::Log("DDS publisher unmatched");
            }
            else
            {
                Debug::Log("Invalid value for PublicationMatchedStatus current count change: " + FString::FromInt(info.current_count_change));
            }
        }

        std::atomic_int matched_;
    };

    fastdds::DomainParticipant* participant_;
    fastdds::Publisher* publisher_;

    rmw_dds_common::msg::ParticipantEntitiesInfo rosDiscoveryInfo_;
    TopicWrapper rosDiscoveryTopic_;
    PubListener rosDiscoveryListener_;

    std::unordered_map<const char*, fastdds::TypeSupport> pubSubTypes_;
    std::unordered_map<std::string, int> publishedRosNodes_;
    std::unordered_map<std::string, TopicWrapper> publishedTopics_;

    template <class T>
    static const char* GetClassId()
    {
        // typeid() would be a less janky way to do this, but it caused issues with Clang
        // We just need a key that is unique for each class and can be derived from a template parameter
        static const char dummy = 0;
        return &dummy;
    }

    template<class T>
    void RegisterPubSubType()
    {
        static_assert(std::is_base_of<fastdds::TopicDataType, T>::value, "T must inherit from eprosima::fastdds::dds::TopicDataType");

        fastdds::TypeSupport typeSupport = fastdds::TypeSupport(new T());
        typeSupport.register_type(participant_);

        pubSubTypes_.insert({ GetClassId<typename T::type>(), typeSupport });
    }

    template<class T>
    std::string GetPubSubTypeNameInternal()
    {
        auto pubSubType = pubSubTypes_.find(GetClassId<T>());

        if (pubSubType == pubSubTypes_.end())
        {
            throw std::runtime_error("DDS PubSub message type not registered for type!");
        }

        return pubSubType->second.get_type_name();
    }

    void PublishInternal(const std::string& topicName, const std::string& typeName, const void* data);
    std::array<uint8_t, 16> DdsGuidToRosGid(const eprosima::fastdds::rtps::GUID_t& guid) const;
    void OnRosDiscoveryInfoUpdated();
#endif // CITHRUS_FASTDDS_AVAILABLE
};
