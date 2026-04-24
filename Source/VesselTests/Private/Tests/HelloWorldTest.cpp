// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

/**
 * Vessel.Smoke.HelloWorld — verifies the plugin loads. If this fails, nothing
 * else in Vessel will work; fix module dependencies first. See BUILD.md §6.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselHelloWorldTest,
	"Vessel.Smoke.HelloWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselHelloWorldTest::RunTest(const FString& /*Parameters*/)
{
	TestTrue(TEXT("VesselCore module is loaded"),
		FModuleManager::Get().IsModuleLoaded(TEXT("VesselCore")));
	TestTrue(TEXT("VesselEditor module is loaded"),
		FModuleManager::Get().IsModuleLoaded(TEXT("VesselEditor")));
	return true;
}
