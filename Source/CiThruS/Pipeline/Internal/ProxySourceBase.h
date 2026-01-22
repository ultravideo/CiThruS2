#pragma once

#include "ProxyBase.h"
#include "PipelineSource.h"

#include <vector>
#include <functional>

// Base class for scaffolding sources that store other components inside themselves
template <uint8_t NOutputs>
class ProxySourceBase : protected virtual ProxyBase, public virtual PipelineSource<NOutputs>
{
	// Template implementation must be in the header file

public:
	virtual ~ProxySourceBase() { }

	virtual void Process() override
	{
		ProxyBase::Process();

		pushOutputData_();
	}

protected:
	ProxySourceBase() { }

	// Saving a lambda function here means that the class does not have to remember what types the stored components are
	std::function<void()> pushOutputData_;
};
