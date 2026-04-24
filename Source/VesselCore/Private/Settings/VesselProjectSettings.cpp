// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Settings/VesselProjectSettings.h"

UVesselProjectSettings::UVesselProjectSettings()
	: Provider(TEXT("anthropic"))
	, Model(TEXT("claude-sonnet-4-6"))
	, bAllowHttp(false)
{
}

const UVesselProjectSettings& UVesselProjectSettings::GetRef()
{
	const UVesselProjectSettings* Instance = GetDefault<UVesselProjectSettings>();
	check(Instance);
	return *Instance;
}
