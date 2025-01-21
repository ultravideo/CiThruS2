#include "LSAlign.h"

// Sets default values for this component's properties
ULSAlign::ULSAlign()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void ULSAlign::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ULSAlign::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction)
{
	Super::TickComponent(deltaTime, tickType, thisTickFunction);
}

void ULSAlign::Init()
{
	LS = Cast<ALandscape>(GetOwner());
}
