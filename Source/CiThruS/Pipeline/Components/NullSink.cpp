#include "NullSink.h"

NullSink::NullSink()
{
	GetInputPin<0>().Initialize(this);
}

NullSink::~NullSink()
{
	
}

void NullSink::Process()
{
	
}
