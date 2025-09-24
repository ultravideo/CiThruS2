#include "AsyncPipelineRunner.h"
#include "Pipeline.h"
#include "Misc/Debug.h"

#include <stdexcept>

AsyncPipelineRunner::AsyncPipelineRunner(Pipeline* pipeline)
	: wantsStop_(false),
	  thread_(std::thread(&AsyncPipelineRunner::RunPipeline, this, pipeline))
{

}

AsyncPipelineRunner::~AsyncPipelineRunner()
{
	wantsStop_ = true;

	thread_.join();
}

void AsyncPipelineRunner::RunPipeline(Pipeline* pipeline)
{
	try
	{
		while (!wantsStop_)
		{
			pipeline->Run();
		}
	}
	catch (const std::exception& exception)
	{
		Debug::Log("Pipeline crashed: " + std::string(exception.what()));
	}

	delete pipeline;
}
