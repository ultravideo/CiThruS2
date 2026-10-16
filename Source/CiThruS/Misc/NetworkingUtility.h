#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "NetworkingUtility.generated.h"

UCLASS()
class CITHRUS_API UNetworkingUtility : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Networking Utilities")
    static bool IsValidIPv4Address(const FString& address);
    UFUNCTION(BlueprintCallable, Category = "Networking Utilities")
    static bool IsValidPort(const FString& portString);
};