#pragma once

#include "CoreMinimal.h"
#include "Pipeline/Internal/PipelineSource.h"

#include <string>

// Source that never generates any data.
// Can be used to satisfy pipeline structures that don't need specific inputs
class CITHRUS_API NullSource : public PipelineSource<1>
{
public:
	NullSource(const std::string& dataFormatName);
	~NullSource();

	virtual void Process() override;
};
