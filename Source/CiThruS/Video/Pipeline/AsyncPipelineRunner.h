#pragma once

#include <thread>

class Pipeline;

// Runs a pipeline on a background thread until destroyed
class CITHRUS_API AsyncPipelineRunner
{
public:
	AsyncPipelineRunner(Pipeline* pipeline);
	~AsyncPipelineRunner();

protected:
	bool wantsStop_;
	std::thread thread_;

	void RunPipeline(Pipeline* pipeline);
};
