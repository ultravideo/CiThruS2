#pragma once

#include "CoreMinimal.h"
#include "Pipeline/Internal/PipelineSink.h"

// Sink that does nothing with the received data.
// Can be used to satisfy pipeline structures that don't need specific outputs
class CITHRUS_API NullSink : public PipelineSink<1>
{
public:
	NullSink();
	virtual ~NullSink();

	virtual void Process() override;
};
