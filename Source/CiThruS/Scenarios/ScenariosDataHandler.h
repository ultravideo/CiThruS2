#pragma once

#include "CoreMinimal.h"

struct FScenarioData;

// Handles file saving/loading & other related stuff for Traffic Scenarios
class CITHRUS_API ScenariosDataHandler
{
public:
	// Would be nice to implement this using generics, 
	// to be used with different type of data also,
	// but I don't know if it's possible / how to do it in c++
	// Currently need to override parameter in new function
	// e.g static bool SaveData(FOtherData data);
	// Implementation should be exactly the same
	static bool SaveData(FScenarioData data);

	static FScenarioData LoadData(FString ScenarioName);
};
