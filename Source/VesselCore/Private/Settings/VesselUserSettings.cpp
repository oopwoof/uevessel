// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Settings/VesselUserSettings.h"

const UVesselUserSettings& UVesselUserSettings::GetRef()
{
	const UVesselUserSettings* Instance = GetDefault<UVesselUserSettings>();
	check(Instance);
	return *Instance;
}
