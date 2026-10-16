#include "CithrusGameInstance.h"
#include "PubSub/DdsParticipant.h"

void UCithrusGameInstance::Shutdown()
{
	DdsParticipant::Destroy();

	Super::Shutdown();
}
