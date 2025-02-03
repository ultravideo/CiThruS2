#include "VideoRecorder.h"
#include "IImageSource.h"
#include "Misc/Debug.h"

VideoRecorder::VideoRecorder()
{
	// TODO
}

VideoRecorder::~VideoRecorder()
{
	// TODO
}

void VideoRecorder::Process()
{
	if (*inputFrame_ == nullptr)
	{
		return;
	}

	// TODO
}

bool VideoRecorder::SetInput(const IImageSource* source)
{
	if (source->GetOutputFormat() != "hevc")
	{
		return false;
	}

	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}
