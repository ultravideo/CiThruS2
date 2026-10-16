#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "CithrusGameInstance.generated.h"

// Used to run code on simulation shutdown
UCLASS( Config=Game )
class CITHRUS_API UCithrusGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Shutdown() override;
};
