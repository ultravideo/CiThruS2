#include "CWeatherPreset.h"

FVector UCWeatherPreset::GetScaledWindSpeed() const
{
	return WindDirection * WindStrength;
}
