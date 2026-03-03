#include "MqttPublisher.h"
#include "Misc/Debug.h"

MqttPublisher::MqttPublisher(
	const std::string& serverUri,
	const std::string& username, const std::string& password,
	const uint32_t& maxMessagesPerTopicPerSecond)
	: connectionStarted_(false), connectionExists_(false),
	maxMessageRateUs_(1000000 / maxMessagesPerTopicPerSecond)
{
#ifdef CITHRUS_PAHOCPP_AVAILABLE
	std::unique_lock<std::mutex> lock(clientMutex_);

	connectionLostTimestamp_ = {};

	// In case creating the client fails
	client_ = nullptr;

	try
	{
		client_ = new mqtt::async_client(serverUri);

		client_->set_connected_handler(
			[this](const std::string& cause)
			{
				connectionStarted_ = false;
				connectionExists_ = true;

				Debug::Log("Connected to MQTT broker: " + cause);
			});

		client_->set_disconnected_handler(
			[this](const mqtt::properties& properties, mqtt::ReasonCode reasonCode)
			{
				connectionStarted_ = false;
				connectionExists_ = false;

				Debug::Log("Disconnected from MQTT broker: " + mqtt::to_string(reasonCode));
				ScheduleConnectionRetry();
			});

		client_->set_connection_lost_handler(
			[this](const std::string& cause)
			{
				connectionStarted_ = false;
				connectionExists_ = false;

				Debug::Log("Lost connection to MQTT broker: " + cause);
				ScheduleConnectionRetry();
			});
	}
	catch (mqtt::exception exception)
	{
		Debug::Log("Creating MQTT client failed: " + mqtt::exception::error_str(exception.get_return_code()));

		delete client_;
		client_ = nullptr;

		return;
	}

	mqtt::ssl_options sslOptions{};

	// declare values for ssl options, here we use only the ones necessary for TLS, but you can optionally define a lot more
	// look here for an example: https://github.com/eclipse/paho.mqtt.c/blob/master/src/samples/paho_c_sub.c
	sslOptions.set_enable_server_cert_auth(false);
	sslOptions.set_verify(true);
	sslOptions.set_ca_path("");
	sslOptions.set_key_store("");
	sslOptions.set_trust_store("");
	sslOptions.set_private_key("");
	sslOptions.set_private_key_password("");
	sslOptions.set_enabled_cipher_suites("");

	mqtt::connect_options connectOptions{};

	connectOptions.set_ssl(sslOptions);
	connectOptions.set_user_name(username);
	connectOptions.set_password(password);

	connectOptions_ = connectOptions;

	TryResuscitateClient();

#endif // CITHRUS_PAHOCPP_AVAILABLE
}

MqttPublisher::~MqttPublisher()
{
#ifdef CITHRUS_PAHOCPP_AVAILABLE
	std::unique_lock<std::mutex> lock(clientMutex_);

	for (std::pair<std::string, TopicWrapper> topics : publishedTopics_)
	{
		delete topics.second.topic;
	}

	publishedTopics_.clear();

	if (client_)
	{
		client_->disconnect()->wait();

		Debug::Log("Disconnected from MQTT broker: Publisher destroyed");

		delete client_;
		client_ = nullptr;
	}
#endif // CITHRUS_PAHOCPP_AVAILABLE
}

void MqttPublisher::Publish(const std::string& topic, uint8_t* data, const size_t& size)
{
	if (topic.size() == 0 || !data || size == 0)
	{
		return;
	}

#ifdef CITHRUS_PAHOCPP_AVAILABLE
	std::unique_lock<std::mutex> lock(clientMutex_);

	if (!TryResuscitateClient())
	{
		return;
	}

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto publishedTopic = publishedTopics_.find(topic);

	if (publishedTopic == publishedTopics_.end())
	{
		publishedTopic = publishedTopics_.insert(std::pair{ topic, TopicWrapper{ new mqtt::topic(*client_, topic), {} } }).first;
	}

	TopicWrapper& topicWrapper = publishedTopic->second;

	if (std::chrono::duration_cast<std::chrono::microseconds>(currentTime - topicWrapper.lastDataTimestamp).count() < maxMessageRateUs_)
	{
		return;
	}

	try
	{
		topicWrapper.topic->publish(data, size);
	}
	catch (mqtt::exception exception)
	{
		Debug::Log("Publishing to MQTT broker failed: " + mqtt::exception::error_str(exception.get_return_code()));

		return;
	}

	topicWrapper.lastDataTimestamp = currentTime;
#endif // CITHRUS_PAHOCPP_AVAILABLE
}

bool MqttPublisher::TryResuscitateClient()
{
	if (connectionExists_)
	{
		return true;
	}

#ifdef CITHRUS_PAHOCPP_AVAILABLE
	if (client_ == nullptr)
	{
		return false;
	}

	auto currentTime = std::chrono::high_resolution_clock::now();

	if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - connectionLostTimestamp_).count() < CONNECTION_RETRY_DELAY_MS)
	{
		return false;
	}

	if (!connectionStarted_)
	{
		Debug::Log("Connecting to MQTT broker...");

		connectionStarted_ = true;
		clientConnected_ = client_->connect(connectOptions_);
	}
#endif // CITHRUS_PAHOCPP_AVAILABLE

	return false;
}

void MqttPublisher::ScheduleConnectionRetry()
{
#ifdef CITHRUS_PAHOCPP_AVAILABLE
	Debug::Log("Retrying connection in " + std::to_string(CONNECTION_RETRY_DELAY_MS / 1000) + " seconds");

	connectionLostTimestamp_ = std::chrono::high_resolution_clock::now();
#endif // CITHRUS_PAHOCPP_AVAILABLE
}
