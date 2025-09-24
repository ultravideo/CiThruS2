#pragma once

#include "CoreMinimal.h"
#include "PipelineSink.h"

#include <string>
#include <thread>
#include <mutex>
#include <stdio.h>

// Records generic data into files
class CITHRUS_API FileSink : public PipelineSink<1>
{
public:
	FileSink(const std::string& filePath);
	virtual ~FileSink();

	virtual void Process() override;

protected:
	FILE* fileHandle_;
};
