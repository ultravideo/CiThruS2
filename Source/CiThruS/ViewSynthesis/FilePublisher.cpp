#include "FilePublisher.h"

#include <filesystem>
#include <fstream>

FilePublisher::FilePublisher(
	const std::string& directoryPath,
	const uint32_t& maxMessagesPerTopicPerSecond)
	: directoryPath_(directoryPath), maxMessageRateUs_(1000000 / maxMessagesPerTopicPerSecond)
{
	if (directoryPath_[directoryPath_.size() - 1] == '/'
		|| directoryPath_[directoryPath_.size() - 1] == '\\')
	{
		directoryPath_ = directoryPath_.substr(0, directoryPath_.size() - 1);
	}

	if (!std::filesystem::exists(directoryPath_))
	{
		std::filesystem::create_directories(directoryPath_);
	}
	else if (!std::filesystem::is_directory(directoryPath_))
	{
		throw std::invalid_argument("Given path is a file, not a directory");
	}
}

FilePublisher::~FilePublisher()
{

}

void FilePublisher::Publish(const std::string& topic, uint8_t* data, const size_t& size)
{
	if (topic.size() == 0 || !data || size == 0)
	{
		return;
	}

	std::unique_lock<std::mutex> lock(clientMutex_);

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto publishedTopic = publishedTopics_.find(topic);

	if (publishedTopic == publishedTopics_.end())
	{
		publishedTopic = publishedTopics_.insert(std::pair{ topic, TopicWrapper{ topic, {}, 0 } }).first;
	}

	TopicWrapper& topicWrapper = publishedTopic->second;

	if (std::chrono::duration_cast<std::chrono::microseconds>(currentTime - topicWrapper.lastDataTimestamp).count() < maxMessageRateUs_)
	{
		return;
	}

	std::string fullPath = directoryPath_ + '/' + topic + '/' + std::to_string(topicWrapper.messagesPublished) + ".json";

	std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

	std::string fullDirectoryPath = fullPath.substr(0, fullPath.find_last_of('/'));

	if (!std::filesystem::exists(fullDirectoryPath))
	{
		std::filesystem::create_directories(fullDirectoryPath);
	}

	std::ofstream stream(fullPath);

	stream.write(reinterpret_cast<char*>(data), size);

	stream.close();

	topicWrapper.lastDataTimestamp = currentTime;
	topicWrapper.messagesPublished++;
}
