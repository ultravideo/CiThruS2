#pragma once

#include "OutputPin.h"

#include <string>
#include <vector>
#include <stdexcept>

// Represents one input data stream of a pipeline component.
// Must be connected to an OutputPin to receive data
class InputPin
{
public:
	InputPin()
		: connectedPin_(nullptr), acceptedFormats_(), acceptAnyFormat_(false) { }
	~InputPin() { }

	inline const uint8_t* GetData() const { return connectedPin_->GetData(); }
	inline uint32_t GetSize() const { return connectedPin_->GetSize(); }
	inline std::string GetFormat() const { return connectedPin_->GetFormat(); }

	inline OutputPin& GetConnectedPin() { return *connectedPin_; }

	inline void SetAcceptedFormats(const std::vector<std::string> acceptedFormats)
	{
		if (connectedPin_)
		{
			throw std::logic_error("Accepted formats cannot be changed while the pin is connected");
			// The reason why they can't be changed is that currently there is no mechanism
			// for propagating and validating format changes after pipeline construction
		}

		acceptedFormats_ = acceptedFormats;
	}

	inline void SetAcceptedFormat(const std::string& acceptedFormat)
	{
		SetAcceptedFormats(std::vector<std::string> { acceptedFormat });
	}

	inline void AcceptAnyFormat()
	{
		if (connectedPin_)
		{
			throw std::logic_error("Accepted formats cannot be changed while the pin is connected");
			// The reason why they can't be changed is that currently there is no mechanism
			// for propagating and validating format changes after pipeline construction
		}

		acceptAnyFormat_ = true;
	}

	inline void ConnectToOutputPin(OutputPin& outputPin)
	{
		std::string outputFormat = outputPin.GetFormat();

		bool formatAcceptable = acceptAnyFormat_;

		for (const std::string& acceptedFormat : acceptedFormats_)
		{
			if (outputFormat == acceptedFormat)
			{
				formatAcceptable = true;

				break;
			}
		}

		if (!formatAcceptable)
		{
			throw std::logic_error("This pin does not accept " + outputFormat + " data. The pipeline components must be rearranged so that the data formats are compatible");
		}

		connectedPin_ = &outputPin;
		outputPin.LockConnection();
	}

protected:
	OutputPin* connectedPin_;

	std::vector<std::string> acceptedFormats_;
	bool acceptAnyFormat_;
};
