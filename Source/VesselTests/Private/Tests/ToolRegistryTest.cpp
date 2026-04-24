// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Fixtures/VesselTestToolFixture.h"
#include "Registry/VesselToolRegistry.h"
#include "Registry/VesselResult.h"

/**
 * Registry populated by ScanAll actually contains the fixture tools.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselToolRegistryScanAll,
	"Vessel.Registry.ToolRegistry.ScanAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselToolRegistryScanAll::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry& Reg = FVesselToolRegistry::Get();
	Reg.ScanAll();

	TestTrue(TEXT("Registry non-empty after scan"), Reg.Num() > 0);

	const FVesselToolSchema* Found = Reg.FindSchema(FName(TEXT("FixtureRead")));
	TestNotNull(TEXT("FixtureRead exists in registry"), Found);
	if (Found)
	{
		TestEqual(TEXT("Category"), Found->Category, FString(TEXT("Test")));
	}
	return true;
}

/**
 * Injection path: tests that don't want real reflection state can seed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselToolRegistryInject,
	"Vessel.Registry.ToolRegistry.Inject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselToolRegistryInject::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry& Reg = FVesselToolRegistry::Get();
	Reg.ClearAll();
	TestEqual(TEXT("Registry empty after ClearAll"), Reg.Num(), 0);

	FVesselToolSchema Synthetic;
	Synthetic.Name = FName(TEXT("SyntheticTool"));
	Synthetic.Category = TEXT("Synthetic");
	Synthetic.Description = TEXT("Hand-crafted for test");
	Synthetic.bRequiresApproval = true;
	Synthetic.SourceClassName = TEXT("<test>");
	Synthetic.SourceFunctionName = TEXT("SyntheticTool");

	Reg.InjectSchemaForTest(Synthetic);

	TestEqual(TEXT("Registry has one tool after inject"), Reg.Num(), 1);
	const FVesselToolSchema* Got = Reg.FindSchema(FName(TEXT("SyntheticTool")));
	TestNotNull(TEXT("SyntheticTool found"), Got);
	if (Got)
	{
		TestEqual(TEXT("Category round-trips"), Got->Category, FString(TEXT("Synthetic")));
	}

	// Restore real state so later tests see the normal scan results.
	Reg.ScanAll();
	return true;
}

/**
 * Registry JSON emission is valid and mentions known tool fields.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselToolRegistryJsonShape,
	"Vessel.Registry.ToolRegistry.JsonShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselToolRegistryJsonShape::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry& Reg = FVesselToolRegistry::Get();
	Reg.ScanAll();

	const FString Json = Reg.ToJsonString();
	TestTrue(TEXT("JSON starts with ["),   Json.StartsWith(TEXT("[")));
	TestTrue(TEXT("JSON ends with ]"),     Json.EndsWith(TEXT("]")));
	TestTrue(TEXT("Contains FixtureRead"), Json.Contains(TEXT("FixtureRead")));
	TestTrue(TEXT("Contains requires_approval field"),
		Json.Contains(TEXT("requires_approval")));
	TestTrue(TEXT("Contains source object"), Json.Contains(TEXT("\"source\":")));
	return true;
}

/**
 * FVesselResult basic ergonomics — Ok/Err and VesselResultCodeToString stability.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselResultBasics,
	"Vessel.Registry.Result.Basics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselResultBasics::RunTest(const FString& /*Parameters*/)
{
	FVesselResult<int32> Happy = FVesselResult<int32>::Ok(42);
	TestTrue(TEXT("Happy bOk"), Happy.bOk);
	TestEqual(TEXT("Happy code"), static_cast<uint8>(Happy.Code), static_cast<uint8>(EVesselResultCode::Ok));
	TestEqual(TEXT("Happy value"), Happy.Value, 42);

	FVesselResult<FString> Sad = FVesselResult<FString>::Err(
		EVesselResultCode::ValidationError,
		TEXT("Parameter X expected int, received string"));
	TestFalse(TEXT("Sad bOk"), Sad.bOk);
	TestEqual(TEXT("Sad code"), static_cast<uint8>(Sad.Code),
		static_cast<uint8>(EVesselResultCode::ValidationError));
	TestTrue(TEXT("Sad message non-empty"), !Sad.Message.IsEmpty());

	// String round-trip
	TestEqual(TEXT("Code-to-string Ok"),
		FString(VesselResultCodeToString(EVesselResultCode::Ok)),
		FString(TEXT("Ok")));
	TestEqual(TEXT("Code-to-string ValidationError"),
		FString(VesselResultCodeToString(EVesselResultCode::ValidationError)),
		FString(TEXT("ValidationError")));
	return true;
}
