// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"

#include "Tools/VesselAssetTools.h"

/**
 * ListAssets returns a JSON array (possibly empty) for a valid content path.
 * We don't assert specific content — the test environment may not have any
 * assets under /Game — but the shape must be correct.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAssetToolsListShape,
	"Vessel.Tools.Asset.ListShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAssetToolsListShape::RunTest(const FString& /*Parameters*/)
{
	const FString Json = UVesselAssetTools::ListAssets(TEXT("/Game"), /*Recursive=*/true);
	TestTrue(TEXT("ListAssets returns JSON array (starts '[')"), Json.StartsWith(TEXT("[")));
	TestTrue(TEXT("ListAssets returns JSON array (ends ']')"),   Json.EndsWith(TEXT("]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAssetToolsListUnknownPath,
	"Vessel.Tools.Asset.ListUnknownPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAssetToolsListUnknownPath::RunTest(const FString& /*Parameters*/)
{
	const FString Json = UVesselAssetTools::ListAssets(TEXT("/Nonexistent_Path_9D4F"), true);
	TestEqual(TEXT("Unknown path returns empty array"), Json, FString(TEXT("[]")));
	return true;
}

/**
 * ReadAssetMetadata for a bogus path returns a JSON object with found=false.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAssetToolsMetadataMissing,
	"Vessel.Tools.Asset.MetadataMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAssetToolsMetadataMissing::RunTest(const FString& /*Parameters*/)
{
	const FString Json = UVesselAssetTools::ReadAssetMetadata(
		TEXT("/Game/_no_such_asset.Does_Not_Exist"));
	TestTrue(TEXT("Metadata JSON is an object"),
		Json.StartsWith(TEXT("{")) && Json.EndsWith(TEXT("}")));
	TestTrue(TEXT("Reports found=false"), Json.Contains(TEXT("\"found\":false")));
	TestTrue(TEXT("Echoes the requested path"),
		Json.Contains(TEXT("/Game/_no_such_asset.Does_Not_Exist")));
	return true;
}
