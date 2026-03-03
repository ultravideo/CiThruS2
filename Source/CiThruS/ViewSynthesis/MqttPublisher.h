#pragma once

#include "Optional/PahoCpp.h"
#include "IPublisher.h"

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

// Publishes data using MQTT
class MqttPublisher : public IPublisher
{
public:
	MqttPublisher(
		const std::string& serverUri,
		const std::string& username, const std::string& password,
		const uint32_t& maxMessagesPerTopicPerSecond = 1);
	virtual ~MqttPublisher();

	virtual void Publish(
		const std::string& topic,
		uint8_t* data, const size_t& size) override;

protected:
	const static uint32_t CONNECTION_RETRY_DELAY_MS = 5000;

	bool connectionStarted_;
	bool connectionExists_;
	uint32_t maxMessageRateUs_;

	using MqttTimePoint
		= std::chrono::time_point<std::chrono::high_resolution_clock>;

#ifdef CITHRUS_PAHOCPP_AVAILABLE
	struct TopicWrapper
	{
		mqtt::topic* topic;
		MqttTimePoint lastDataTimestamp;
	};

	mqtt::async_client* client_;
	mqtt::connect_options connectOptions_;
	mqtt::token_ptr clientConnected_;

	MqttTimePoint connectionLostTimestamp_;

	std::unordered_map<std::string, TopicWrapper> publishedTopics_;

	std::mutex clientMutex_;
#endif // CITHRUS_PAHOCPP_AVAILABLE

	bool TryResuscitateClient();
	void ScheduleConnectionRetry();
};
