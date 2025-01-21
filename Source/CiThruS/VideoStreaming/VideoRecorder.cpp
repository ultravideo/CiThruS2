#include "VideoStreaming/VideoRecorder.h"
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
	// TODO
}

void VideoRecorder::SetInput(const IImageSource* source)
{
	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();
}
