#include "ScenariosDataHandler.h"
#include "DataStructures/ScenarioData.h"
#include "Serialization/BufferArchive.h"

#define FILEPATH(scenario) (FString(FPlatformProcess::UserDir()) + "\\CiThruS2\\Scenarios\\" + scenario + ".sav")

bool ScenariosDataHandler::SaveData(FScenarioData data)
{
	FString SavePath = FILEPATH(data.scenarioName);

	FBufferArchive ToBinary;
	ToBinary << data;

	if (FFileHelper::SaveArrayToFile(ToBinary, *SavePath))
	{
		ToBinary.FlushCache();
		ToBinary.Empty();

		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Error saving File %s"), *SavePath);
		ToBinary.FlushCache();
		ToBinary.Empty();

		return false;
	}
}

FScenarioData ScenariosDataHandler::LoadData(FString ScenarioName)
{
	FString SavePath = FILEPATH(ScenarioName);

	TArray<uint8> BinaryArray;
	if (!FFileHelper::LoadFileToArray(BinaryArray, *SavePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Error loading Scenario"));
		return FScenarioData();
	}

	if (BinaryArray.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Error loading Scenario"));
		return FScenarioData();
	}

	FMemoryReader FromBinary = FMemoryReader(BinaryArray, true);
	FromBinary.Seek(0);

	FScenarioData data;
	FromBinary << data;

	//Cleanup
	FromBinary.FlushCache();
	FromBinary.Close();
	BinaryArray.Empty(); 

	return data;
}
