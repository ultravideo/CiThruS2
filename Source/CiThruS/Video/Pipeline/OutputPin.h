#pragma once

#include <string>
#include <stdexcept>

// Represents one output data stream of a pipeline component.
// Must be connected to an InputPin to pass data onward
class OutputPin
{
public:
	OutputPin()
		: data_(nullptr), dataSize_(0), dataFormat_("error"), connected_(false) { }
	~OutputPin() { }

	inline const uint8_t* GetData() const { return data_; }
	inline uint32_t GetSize() const { return dataSize_; }
	inline const std::string GetFormat() const { return dataFormat_; }

	inline void LockConnection() { connected_ = true; }

	inline void SetData(const uint8_t* data) { data_ = data; }
	inline void SetSize(const uint32_t& dataSize) { dataSize_ = dataSize; }

	inline void SetFormat(const std::string& format)
	{
		if (connected_)
		{
			throw std::logic_error("Format cannot be changed while the pin is connected");
			// The reason why it can't be changed is that currently there is no mechanism
			// for propagating and validating format changes after pipeline construction
		}

		dataFormat_ = format;
	}

protected:
	const uint8_t* data_;
	uint32_t dataSize_;
	std::string dataFormat_;

	bool connected_;
};
