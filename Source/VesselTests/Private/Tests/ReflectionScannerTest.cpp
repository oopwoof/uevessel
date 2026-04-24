// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Fixtures/VesselTestToolFixture.h"
#include "Registry/VesselReflectionScanner.h"

/**
 * Scanner discovers UFUNCTIONs tagged AgentTool="true" but skips untagged ones.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselReflectionScannerDiscovers,
	"Vessel.Registry.Scanner.Discovers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselReflectionScannerDiscovers::RunTest(const FString& /*Parameters*/)
{
	const TArray<FVesselToolSchema> Schemas = FVesselReflectionScanner::BuildToolSchemas();

	const FVesselToolSchema* FixtureRead = Schemas.FindByPredicate(
		[](const FVesselToolSchema& S) { return S.SourceFunctionName == TEXT("FixtureRead"); });
	TestNotNull(TEXT("FixtureRead is discovered"), FixtureRead);

	const FVesselToolSchema* FixtureIrr = Schemas.FindByPredicate(
		[](const FVesselToolSchema& S) { return S.SourceFunctionName == TEXT("FixtureIrreversibleWrite"); });
	TestNotNull(TEXT("FixtureIrreversibleWrite is discovered"), FixtureIrr);

	const FVesselToolSchema* NotATool = Schemas.FindByPredicate(
		[](const FVesselToolSchema& S) { return S.SourceFunctionName == TEXT("NotAnAgentTool"); });
	TestNull(TEXT("NotAnAgentTool is NOT discovered (no AgentTool meta)"), NotATool);

	return true;
}

/**
 * Schema extraction: meta values from FixtureRead map correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselReflectionScannerMetaReadback,
	"Vessel.Registry.Scanner.MetaReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselReflectionScannerMetaReadback::RunTest(const FString& /*Parameters*/)
{
	UClass* OwningClass = UVesselTestToolFixture::StaticClass();
	UFunction* Func = OwningClass->FindFunctionByName(TEXT("FixtureRead"));
	TestNotNull(TEXT("FixtureRead UFunction resolves via reflection"), Func);
	if (!Func) { return false; }

	const FVesselToolSchema S = FVesselReflectionScanner::BuildSchemaForFunction(OwningClass, Func);
	TestEqual(TEXT("Tool name"), S.Name, FName(TEXT("FixtureRead")));
	TestEqual(TEXT("Category"), S.Category, FString(TEXT("Test")));
	TestFalse(TEXT("RequiresApproval=false"), S.bRequiresApproval);
	TestFalse(TEXT("IrreversibleHint defaults to false"), S.bIrreversibleHint);
	TestEqual(TEXT("Source class matches"), S.SourceClassName, FString(TEXT("VesselTestToolFixture")));

	TestEqual(TEXT("Parameter count (Path + Keys + Limit)"), S.Parameters.Num(), 3);

	const FVesselParameterSchema* Path = S.Parameters.FindByPredicate(
		[](const FVesselParameterSchema& P) { return P.Name == FName(TEXT("Path")); });
	TestNotNull(TEXT("Path parameter present"), Path);
	if (Path)
	{
		TestTrue(TEXT("Path is string-typed"), Path->TypeJson.Contains(TEXT("\"string\"")));
	}

	const FVesselParameterSchema* Keys = S.Parameters.FindByPredicate(
		[](const FVesselParameterSchema& P) { return P.Name == FName(TEXT("Keys")); });
	TestNotNull(TEXT("Keys parameter present"), Keys);
	if (Keys)
	{
		TestTrue(TEXT("Keys is array-of-string"),
			Keys->TypeJson.Contains(TEXT("\"array\"")) && Keys->TypeJson.Contains(TEXT("\"string\"")));
	}

	const FVesselParameterSchema* Limit = S.Parameters.FindByPredicate(
		[](const FVesselParameterSchema& P) { return P.Name == FName(TEXT("Limit")); });
	TestNotNull(TEXT("Limit parameter present"), Limit);
	if (Limit)
	{
		TestTrue(TEXT("Limit is integer"), Limit->TypeJson.Contains(TEXT("\"integer\"")));
	}

	TestTrue(TEXT("Return type is string (FString return)"),
		S.ReturnTypeJson.Contains(TEXT("\"string\"")));
	return true;
}

/**
 * Policy flags round-trip for FixtureIrreversibleWrite.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselReflectionScannerPolicyFlags,
	"Vessel.Registry.Scanner.PolicyFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselReflectionScannerPolicyFlags::RunTest(const FString& /*Parameters*/)
{
	UClass* OwningClass = UVesselTestToolFixture::StaticClass();
	UFunction* Func = OwningClass->FindFunctionByName(TEXT("FixtureIrreversibleWrite"));
	if (!Func) { return false; }

	const FVesselToolSchema S = FVesselReflectionScanner::BuildSchemaForFunction(OwningClass, Func);
	TestTrue(TEXT("RequiresApproval=true"), S.bRequiresApproval);
	TestTrue(TEXT("IrreversibleHint=true"), S.bIrreversibleHint);
	TestTrue(TEXT("BatchEligible=true"), S.bBatchEligible);
	TestTrue(TEXT("Tags contain 'fixture'"), S.Tags.Contains(FString(TEXT("fixture"))));
	TestTrue(TEXT("Tags contain 'irreversible'"), S.Tags.Contains(FString(TEXT("irreversible"))));
	return true;
}
