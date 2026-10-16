#include "NullSource.h"

NullSource::NullSource(const std::string& dataFormatName)
{
	GetOutputPin<0>().Initialize(this, dataFormatName);
	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

NullSource::~NullSource()
{

}

void NullSource::Process()
{

}
