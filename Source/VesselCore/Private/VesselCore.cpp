// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "VesselCore.h"
#include "VesselLog.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogVessel);
DEFINE_LOG_CATEGORY(LogVesselRegistry);
DEFINE_LOG_CATEGORY(LogVesselSession);
DEFINE_LOG_CATEGORY(LogVesselHITL);
DEFINE_LOG_CATEGORY(LogVesselLlm);
DEFINE_LOG_CATEGORY(LogVesselCost);

void FVesselCoreModule::StartupModule()
{
	VESSEL_LOG(Log, TEXT("VesselCore module started. Version=0.1.0-alpha.1"));
}

void FVesselCoreModule::ShutdownModule()
{
	VESSEL_LOG(Log, TEXT("VesselCore module shutting down."));
}

IMPLEMENT_MODULE(FVesselCoreModule, VesselCore);
