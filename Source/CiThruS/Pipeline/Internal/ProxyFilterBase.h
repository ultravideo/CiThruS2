#pragma once

#include "ProxySinkBase.h"
#include "ProxySourceBase.h"
#include "PipelineFilter.h"

#include <vector>
#include <functional>

// Base class for scaffolding filters that store other components inside themselves
template <uint8_t NInputs, uint8_t NOutputs>
class ProxyFilterBase : public virtual PipelineFilter<NInputs, NOutputs>, protected virtual ProxySinkBase<NInputs>, protected virtual ProxySourceBase<NOutputs>
{
	// Template implementation must be in the header file

public:
	virtual ~ProxyFilterBase() { }

	virtual void Process() override
	{
		// This override seems pointless, but MSVC complains without it due to the
		// diamond inheritance of IPipelineComponent
		ProxySourceBase<NOutputs>::Process();
	}

	virtual void OnInputPinsConnected() override
	{
		// This override seems pointless, but MSVC complains without it due to the
		// diamond inheritance of IPipelineComponent
		ProxySinkBase<NInputs>::OnInputPinsConnected();
	}

protected:
	ProxyFilterBase() { }
};
