#include "CoreMinimal.h"

#include "CithrusConfig.generated.h"

// Used to save and load persistent configuration values in Saved/Config/.../Game.ini
UCLASS( Config=Game )
class CITHRUS_API UCithrusConfig : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static void SetShowIntroduction(bool value);

	UFUNCTION(BlueprintCallable)
	static bool GetShowIntroduction();

private:
	UPROPERTY(Config)
	bool ShowIntroduction = true;
};
