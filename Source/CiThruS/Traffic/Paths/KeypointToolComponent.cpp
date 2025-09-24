#include "KeypointToolComponent.h"
#include "Misc/FileHelper.h"
#include "Traffic/TrafficController.h"
#include <Kismet/GameplayStatics.h>
#include "Misc/Debug.h"

TArray<FKeypointToolKP> AKeypointToolComponent::GetKeypoints(EKeypointToolGraph graph)
{
	KeypointGraph roadGraph = GetCorrectGraph(graph);

	TArray<FKeypointToolKP> kps;

	for (int i = 0; i < roadGraph.KeypointCount(); i++)
	{
		kps.Add(FKeypointToolKP(i, roadGraph.GetKeypointPosition(i), roadGraph.GetOutBoundKeypoints(i), roadGraph.GetInboundKeypoints(i), roadGraph.GetKeypointRules(i)));
	}

	return kps;
}

bool AKeypointToolComponent::SaveToFile(TArray<FKeypointToolKP> keypointsData, EKeypointToolGraph graph)
{
	// Construct a new graph from the updated data
	KeypointGraph newGraph;

	for (int i = 0; i < keypointsData.Num(); i++)
	{
		newGraph.AddKeypoint(keypointsData[i].location);
	}

	for (int i = 0; i < keypointsData.Num(); i++)
	{
		for (int j : keypointsData[i].outbound)
		{
			newGraph.LinkKeypoints(i, j);
		}
	}

	KeypointGraph roadGraph = GetCorrectGraph(graph);

	for (int i = 0; i < roadGraph.SpawnPointCount(); i++)
	{
		newGraph.MarkSpawnPoint(roadGraph.GetSpawnPoint(i));
	}

	if (graph == EKeypointToolGraph::E_Car)
	{
		for (std::pair<std::pair<int, int>, std::pair<int, int>> ot : roadGraph.GetOvertakes())
		{
			newGraph.OvertakeSetup(ot.first.first, ot.first.second, ot.second.first, ot.second.second);
		}
	}

	for (int i = 0; i < keypointsData.Num(); i++)
	{
		if (keypointsData[i].rules != 0)
		{
			newGraph.SetKeypointRules(i, keypointsData[i].rules);
		}
	}

	// Save to file
	FString filePath = FPaths::ProjectDir() + "/DataFiles/KeypointToolSaves/";

	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!platformFile.DirectoryExists(*filePath))
	{
		platformFile.CreateDirectory(*filePath);
	}

	switch (graph)
	{
	case EKeypointToolGraph::E_Car:
		filePath += "newRoadGraph.data";
		break;
	case EKeypointToolGraph::E_Pedestrian:
		filePath += "newSharedUseGraph.data";
		break;
	case EKeypointToolGraph::E_Tram:
		filePath += "newTramwayTrackGraph.data";
		break;
	case EKeypointToolGraph::E_Bicycle:
		filePath += "newBicycleGraph.data";
		break;
	default:
		filePath += "newDefaultGraph.data";
		break;
	}

	return newGraph.SaveToFile(TCHAR_TO_UTF8(*filePath));
}

void AKeypointToolComponent::InitZoneController()
{
	// Get ref to controller
	TArray<AActor*> find;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficController::StaticClass(), find);

	if (find.Num() <= 0)
	{
		Debug::Log("No Traffic Controller placed");
		return;
	}

	ATrafficController* controller = Cast<ATrafficController>(find[0]);
	controller->EDITOR_InitRegulationCollisions();
}

int32 AKeypointToolComponent::GetZoneRulesForPoint(FVector position)
{
	// Get ref to controller
	TArray<AActor*> find;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficController::StaticClass(), find);

	if (find.Num() <= 0)
	{
		Debug::Log("No Traffic Controller placed");
		return 0;
	}

	ATrafficController* controller = Cast<ATrafficController>(find[0]);
	return controller->GetKeypointRegulationRulesAtPoint(position);
}

void AKeypointToolComponent::WriteLine(FArchive* fileWriter, FString line)
{
	// Convert the FString to ANSI TCHAR array
	const TCHAR* LineData = *line; 

	// Write the line to the file
	fileWriter->Serialize(const_cast<TCHAR*>(LineData), line.Len() * sizeof(TCHAR));
	fileWriter->Serialize(const_cast<TCHAR*>(TEXT("\r\n")), sizeof(TCHAR) * 2); // Add line break
}

KeypointGraph AKeypointToolComponent::GetCorrectGraph(EKeypointToolGraph eg)
{
	KeypointGraph roadGraph;

	switch (eg)
	{
	case EKeypointToolGraph::E_Car:
		roadGraph.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "DataFiles/roadGraph.data")));
		break;
	case EKeypointToolGraph::E_Pedestrian:
		roadGraph.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "DataFiles/sharedUseGraph.data")));
		break;
	case EKeypointToolGraph::E_Tram:
		roadGraph.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "DataFiles/tramwayTrackGraph.data")));
		break;
	case EKeypointToolGraph::E_Bicycle:
		roadGraph.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "DataFiles/bicycleGraph.data")));
		break;
	default:
		roadGraph.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "DataFiles/roadGraph.data")));
		break;
	}

	return roadGraph;
}
