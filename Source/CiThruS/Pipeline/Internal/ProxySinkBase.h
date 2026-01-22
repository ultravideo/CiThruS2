#pragma once

#include "ProxyBase.h"
#include "PipelineSink.h"

#include <vector>
#include <functional>

// Base class for scaffolding sinks that store other components inside themselves
template <uint8_t NInputs>
class ProxySinkBase : protected virtual ProxyBase, public virtual PipelineSink<NInputs>
{
	// Template implementation must be in the header file

public:
	virtual ~ProxySinkBase() { }


	virtual void Process() override
	{
		// This override seems pointless, but MSVC complains without it due to the
		// diamond inheritance of IPipelineComponent
		ProxyBase::Process();
	}

	virtual void OnInputPinsConnected() override
	{
		onInputPinsConnected_();
	}

protected:
	ProxySinkBase() { }

	// Saving a lambda function here means that the class does not have to remember what types the stored components are
	std::function<void()> onInputPinsConnected_;
};
