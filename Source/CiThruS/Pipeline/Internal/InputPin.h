#pragma once

#include "OutputPin.h"

#include <string>
#include <vector>
#include <stdexcept>
#include <initializer_list>

// Represents one input data stream of a pipeline component.
// Must be connected to an OutputPin to receive data
class InputPin
{
public:
	struct AcceptedFormats
	{
		AcceptedFormats(const char* format)
		{
			Formats = { format };
		}

		AcceptedFormats(const std::string& format)
		{
			Formats = { format };
		}

		AcceptedFormats(const std::vector<std::string>& formats)
		{
			Formats = { formats };
		}

		AcceptedFormats(const std::initializer_list<std::string>& formats)
		{
			Formats = { formats };
		}

		std::vector<std::string> Formats;
	};

	InputPin()
		: connectedPin_(nullptr), acceptedFormats_(), acceptAnyFormat_(false), initialized_(false), ownerName_(""), ownerIndex_(-1) { }
	~InputPin() { }

	inline const uint8_t* GetData() const { return connectedPin_->GetData(); }
	inline uint32_t GetSize() const { return connectedPin_->GetSize(); }
	inline std::string GetFormat() const { return connectedPin_->GetFormat(); }

	inline OutputPin& GetConnectedPin() { return *connectedPin_; }

	template<class TOwner>
	inline void Initialize(const TOwner* owner, const AcceptedFormats& acceptedFormats)
	{
		if (initialized_)
		{
			throw std::logic_error("Pin has already been initialized");
		}

		acceptedFormats_ = acceptedFormats.Formats;
		initialized_ = true;

		SetOwner(owner);
	}

	template<class TOwner>
	inline void Initialize(const TOwner* owner)
	{
		if (initialized_)
		{
			throw std::logic_error("Pin has already been initialized");
		}

		acceptAnyFormat_ = true;
		initialized_ = true;

		SetOwner(owner);
	}

	inline void ConnectToOutputPin(OutputPin& outputPin)
	{
		if (!initialized_)
		{
			throw std::logic_error("Pin has not been initialized");
		}

		if (connectedPin_)
		{
			throw std::logic_error("Pin is already connected");
		}

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
			throw std::logic_error("Pin " + GetDescriptor() + " does not accept " + outputFormat + " data from pin " + outputPin.GetDescriptor() + ". The pipeline must be rearranged so that the pins are compatible");
		}

		connectedPin_ = &outputPin;
		outputPin.LockConnection();
	}

protected:
	OutputPin* connectedPin_;

	std::vector<std::string> acceptedFormats_;
	bool acceptAnyFormat_;

	bool initialized_;

	std::string ownerName_;
	int ownerIndex_;

	inline void SetIndex(const int& index)
	{
		ownerIndex_ = index;
	}

	inline std::string GetDescriptor() const
	{
		return std::to_string(ownerIndex_) + " of " + ownerName_;
	}

	template <class TOwner>
	inline void SetOwner(const TOwner* owner)
	{
		// Trick to get the name of the owner class using TOwner in the function signature
#ifdef _MSC_VER
		std::string funcName = __FUNCSIG__;

		int start = funcName.find("<class ") + 7;
		int end = funcName.find(">(", start);

		ownerName_ = std::string(funcName).substr(start, end - start);
#else
		std::string funcName = __PRETTY_FUNCTION__;

		int start = funcName.find("T = ") + 4;
		int end = funcName.find(']', start);

		ownerName_ = std::string(funcName).substr(start, end - start);
#endif
	}

	template <uint8_t NInputs>
	friend class PipelineSink;
};
