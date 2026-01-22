#pragma once

#include <string>
#include <stdexcept>

// Represents one output data stream of a pipeline component.
// Must be connected to an InputPin to pass data onward
class OutputPin
{
public:
	OutputPin()
		: data_(nullptr), dataSize_(0), dataFormat_("error"), initialized_(false), connected_(false), ownerName_(""), ownerIndex_(-1) { }
	~OutputPin() { }

	inline const uint8_t* GetData() const { return data_; }
	inline uint32_t GetSize() const { return dataSize_; }
	inline const std::string GetFormat() const { return dataFormat_; }

	inline void SetData(const uint8_t* data) { data_ = data; }
	inline void SetSize(const uint32_t& dataSize) { dataSize_ = dataSize; }

	template<class TOwner>
	inline void Initialize(const TOwner* owner, const std::string& format)
	{
		if (initialized_)
		{
			throw std::logic_error("Pin has already been initialized");
		}

		dataFormat_ = format;
		initialized_ = true;

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

protected:
	const uint8_t* data_;
	uint32_t dataSize_;
	std::string dataFormat_;

	bool initialized_;
	bool connected_;

	std::string ownerName_;
	int ownerIndex_;

	inline void SetIndex(const int& index)
	{
		ownerIndex_ = index;
	}

	inline void LockConnection()
	{
		if (!initialized_)
		{
			throw std::logic_error("Pin has not been initialized");
		}

		connected_ = true;
	}

	inline std::string GetDescriptor() const
	{
		return std::to_string(ownerIndex_) + " of " + ownerName_;
	}

	friend class InputPin;
	template <uint8_t NOutputs>
	friend class PipelineSource;
};
