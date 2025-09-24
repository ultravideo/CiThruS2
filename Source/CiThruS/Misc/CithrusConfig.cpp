#include "CithrusConfig.h"

void UCithrusConfig::SetShowIntroduction(bool value)
{
	UCithrusConfig* config = GetMutableDefault<UCithrusConfig>();

	config->ShowIntroduction = value;
	config->SaveConfig();
}

bool UCithrusConfig::GetShowIntroduction()
{
	UCithrusConfig* config = GetMutableDefault<UCithrusConfig>();

	return config->ShowIntroduction;
}
