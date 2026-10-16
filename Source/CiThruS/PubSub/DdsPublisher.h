#pragma once

#include "Optional/FastDds.h"
#include "IPublisher.h"
#include "Misc/Debug.h"

#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>

// Publishes data using DDS
class DdsPublisher : public IPublisher
{
public:
	DdsPublisher(const std::string& nodeName, const uint32_t& maxMessagesPerTopicPerSecond = 1);
	virtual ~DdsPublisher();

	virtual void Publish(
		const std::string& topic,
        UStruct* structType, void* structData) override;

protected:
    struct DdsMessage
    {
    public:
        template<typename T>
        DdsMessage(
            const std::string& topicName,
            const std::string& typeName,
            const T& value)
        {
            topicName_ = topicName;
            typeName_ = typeName;
            data_ = new T(value);
            // Clang doesn't like deleting bare void pointers so cast using a template first
            dataDestructor_ = [](DdsMessage* message) { delete static_cast<T*>(message->data_); };
        }

        ~DdsMessage()
        {
            dataDestructor_(this);
        }

        const std::string& GetTopicName() const { return topicName_; }
        const std::string& GetTypeName() const { return typeName_; }
        const void* GetData() const { return data_; }

    private:
        std::string topicName_;
        std::string typeName_;
        void* data_;

        void (*dataDestructor_)(DdsMessage*);
    };

    uint32_t maxMessageRateUs_;
    uint32_t rosFrameIndex_;

    std::unordered_map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> messageLastSentToTopic_;

    //void UStructToMessages(UStruct* uStruct, void* structData, std::vector<DdsMessage>& messages, std::string topicPrefix) const;
};
