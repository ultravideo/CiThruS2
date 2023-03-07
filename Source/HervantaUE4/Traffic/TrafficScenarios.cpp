#include "TrafficScenarios.h"
#include "Engine/World.h"
#include "NPCar.h"
#include "../Debug.h"

void ATrafficScenarios::ActivateScenario(const TrafficScenario& scenario)
{
	if (playerCar_ == nullptr || trafficController_ == nullptr)
	{
		Debug::Log("Player car or traffic controller null, aborting");
		return;
	}

	ResetTrafficScenario();
	playerCar_->SetActorLocation(scenario.playerCar.position);
	playerCar_->SetActorRotation(scenario.playerCar.rotation);
	
	for (const CarSpawnInfo& carInfo : scenario.cars)
	{
		trafficController_->SpawnCar(carInfo.position, carInfo.rotation, carInfo.simulate);
	}
}

void ATrafficScenarios::ResetTrafficScenario()
{
	trafficController_->DeleteAllCars();
	playerCar_->ResetCar();
}
