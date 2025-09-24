#include "Pipeline.h"
#include "IPipelineComponent.h"

Pipeline::~Pipeline()
{
	for (int i = 0; i < components_.size(); i++)
	{
		delete components_[i];
	}

	components_.clear();
}

void Pipeline::Run()
{
	for (int i = 0; i < components_.size(); i++)
	{
		components_[i]->Process();
	}
}
