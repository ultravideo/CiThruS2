#pragma once

#include <string>

class UStruct;

// Generic interface for publishing data with a publish/subscribe protocol
class IPublisher
{
public:
	virtual void Publish(
		const std::string& topic,
		UStruct* structType, void* structData) = 0;

protected:
	IPublisher() { }
};
