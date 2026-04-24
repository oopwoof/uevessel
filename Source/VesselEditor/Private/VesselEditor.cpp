// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "VesselEditor.h"
#include "VesselLog.h"
#include "Modules/ModuleManager.h"

void FVesselEditorModule::StartupModule()
{
	UE_LOG(LogVessel, Log, TEXT("VesselEditor module started."));
}

void FVesselEditorModule::ShutdownModule()
{
	UE_LOG(LogVessel, Log, TEXT("VesselEditor module shutting down."));
}

IMPLEMENT_MODULE(FVesselEditorModule, VesselEditor);
