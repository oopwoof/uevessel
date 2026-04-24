// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "VesselCore.h"
#include "VesselLog.h"

#include "Llm/LlmProviderRegistry.h"
#include "Llm/VesselMockProvider.h"
#include "Llm/AnthropicProvider.h"

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

	// Register built-in LLM providers.
	FLlmProviderRegistry& Registry = FLlmProviderRegistry::Get();
	Registry.RegisterProvider(MakeShared<FVesselMockProvider>());
	Registry.RegisterProvider(MakeShared<FAnthropicProvider>());
}

void FVesselCoreModule::ShutdownModule()
{
	// Drop providers so tests running post-shutdown do not see stale registrations.
	FLlmProviderRegistry::Get().ClearAll();

	VESSEL_LOG(Log, TEXT("VesselCore module shutting down."));
}

// Private include paths inside VesselCore must be declared to the Build System.
// We do this via Private/ path conventions; AnthropicProvider lives in Private/Llm/
// and is included by path relative to VesselCore/Private. UBT auto-adds Private/ to
// the module's include search path.

IMPLEMENT_MODULE(FVesselCoreModule, VesselCore);
