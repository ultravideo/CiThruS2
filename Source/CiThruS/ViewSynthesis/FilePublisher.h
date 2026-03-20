#pragma once

#include "IPublisher.h"

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

// "Publishes" data into local files
class FilePublisher : public IPublisher
{
public:
	FilePublisher(
		const std::string& directoryPath,
		const uint32_t& maxMessagesPerTopicPerSecond = 1);
	virtual ~FilePublisher();

	virtual void Publish(
		const std::string& topic,
		uint8_t* data, const size_t& size) override;

protected:
	std::string directoryPath_;
	uint32_t maxMessageRateUs_;

	using TimePoint
		= std::chrono::time_point<std::chrono::high_resolution_clock>;

	struct TopicWrapper
	{
		std::string topic;
		TimePoint lastDataTimestamp;
		uint64_t messagesPublished;
	};

	std::unordered_map<std::string, TopicWrapper> publishedTopics_;

	std::mutex clientMutex_;
};
