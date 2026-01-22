#pragma once

#include "IPipelineComponent.h"

#include <vector>

// Base class for scaffolding components that store other components inside themselves
class ProxyBase : public virtual IPipelineComponent
{
public:
	virtual ~ProxyBase()
	{
		for (int i = 0; i < components_.size(); i++)
		{
			delete components_[i];
		}

		components_.clear();
	}

	virtual void Process() override
	{
		for (int i = 0; i < components_.size(); i++)
		{
			components_[i]->Process();
		}
	}

protected:
	ProxyBase() { }

	std::vector<IPipelineComponent*> components_;
};
