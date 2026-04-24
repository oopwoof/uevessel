// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

/**
 * VesselTests — thin module hosting automation tests.
 * Type=DeveloperTool in Vessel.uplugin so it does not ship with cooked builds.
 */
class FVesselTestsModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FVesselTestsModule, VesselTests);
