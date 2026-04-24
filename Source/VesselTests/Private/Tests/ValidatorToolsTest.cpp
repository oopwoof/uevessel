// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"

#include "Tools/VesselValidatorTools.h"

/**
 * RunAssetValidator on a missing asset returns a JSON object with ok=false.
 * Actual validator behavior against real assets is out of scope here —
 * project-specific validators are what produce meaningful output, and the
 * test project may have none. We only verify the tool wires correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselValidatorToolsMissingAsset,
	"Vessel.Tools.Validator.MissingAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselValidatorToolsMissingAsset::RunTest(const FString& /*Parameters*/)
{
	const FString Json = UVesselValidatorTools::RunAssetValidator(
		TEXT("/Game/_absent.ThisAssetDoesNotExist"));
	TestTrue(TEXT("Returns a JSON object"),
		Json.StartsWith(TEXT("{")) && Json.EndsWith(TEXT("}")));
	TestTrue(TEXT("Reports ok=false for missing asset"), Json.Contains(TEXT("\"ok\":false")));
	TestTrue(TEXT("Echoes asset path"),
		Json.Contains(TEXT("/Game/_absent.ThisAssetDoesNotExist")));
	return true;
}

/**
 * Shape test: RunAssetValidator's JSON skeleton always includes the expected
 * top-level fields when it succeeds. We can't guarantee a valid asset exists
 * in CI, so we just verify the error-path report has the expected shape.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselValidatorToolsReportShape,
	"Vessel.Tools.Validator.ReportShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselValidatorToolsReportShape::RunTest(const FString& /*Parameters*/)
{
	const FString Json = UVesselValidatorTools::RunAssetValidator(TEXT("/Game/whatever"));
	TestTrue(TEXT("JSON contains 'asset' key"), Json.Contains(TEXT("\"asset\":")));
	TestTrue(TEXT("JSON contains 'ok' key"),    Json.Contains(TEXT("\"ok\":")));
	return true;
}
