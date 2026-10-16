#include "NetworkingUtility.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4SubnetMask.h"

bool UNetworkingUtility::IsValidIPv4Address(const FString& address)
{
	FIPv4Address ipAddress;
	return FIPv4Address::Parse(address, ipAddress);
}

bool UNetworkingUtility::IsValidPort(const FString& portString)
{
    if (!portString.IsNumeric() || portString.IsEmpty())
        return false;

    int32 port = FCString::Atoi(*portString);
    return port > 0 && port <= 65535;
}