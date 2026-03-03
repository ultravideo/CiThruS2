#pragma once

#include <string>

// Generic interface for publishing data with a publish/subscribe protocol
class IPublisher
{
public:
	virtual void Publish(
		const std::string& topic,
		uint8_t* data, const size_t& size) = 0;

protected:
	IPublisher() { }
};
