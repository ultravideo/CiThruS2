#include "TimeAndWeatherController.h"
#include "TimeAndWeatherDataContainers.h"
#include "Debug.h"

void ATimeAndWeatherController::SetTimeOfDay(float timeOfDay, bool enableProgression, float dayLengthInSeconds)
{
	// Check validity of input
	if (timeOfDay < 0 || timeOfDay > 24)
	{
		timeOfDay = -1;
	}
	if (dayLengthInSeconds < 0)
	{
		dayLengthInSeconds = 2000;
	}

	// Setup setup & call blueprint
	FTimeSetup setup;
	setup.timeOfDay = timeOfDay;
	setup.enableProgression = enableProgression;
	setup.dayLengthInSeconds = dayLengthInSeconds;

	SetTimeOfDay(setup);
}

void ATimeAndWeatherController::SetWeather(EWeatherTypes weatherType)
{
	// Setup setup & call blueprint
	FWeatherSetup setup;
	setup.weather = weatherType;

	SetWeather(setup);
}

void ATimeAndWeatherController::ApplyPreset(FTimeAndWeatherDataPreset preset)
{	
	FString loadMessage = "Load preset: " + preset.description;
	Debug::Log(loadMessage);

	// Apply time & weather
	SetWeather(preset.Weather);
	SetTimeOfDay(preset.Time);
}

void ATimeAndWeatherController::ApplyPreset(int index)
{
	if (index >= 0 && index < presets_.Num())
	{
		ApplyPreset(presets_[index]);
	}		
	else
	{
		Debug::Log("Can't load time & weather preset");
	}
}

TArray<FString> ATimeAndWeatherController::GetPresetDescriptions()
{
	TArray<FString> ret; 

	for (FTimeAndWeatherDataPreset& p : presets_)
	{
		ret.Add(p.description);
	}

	return ret;
}
