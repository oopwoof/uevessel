// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"

#include "Fixtures/VesselTestToolFixture.h"
#include "Registry/VesselResult.h"
#include "Registry/VesselToolInvoker.h"
#include "Registry/VesselToolRegistry.h"
#include "Transaction/VesselTransactionScope.h"

/**
 * NotFound case — invoker reports a structured error when tool is missing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselInvokerNotFound,
	"Vessel.Registry.Invoker.NotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselInvokerNotFound::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	const auto Result = FVesselToolInvoker::Invoke(
		FName(TEXT("_DoesNotExist")),
		TEXT("{}"));

	TestFalse(TEXT("NotFound: bOk false"), Result.bOk);
	TestEqual(TEXT("NotFound: code"),
		static_cast<uint8>(Result.Code),
		static_cast<uint8>(EVesselResultCode::NotFound));
	TestTrue(TEXT("NotFound: message non-empty"), !Result.Message.IsEmpty());
	return true;
}

/**
 * Args marshalling happy path — FixtureRead echoes all three inputs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselInvokerRoundTrip,
	"Vessel.Registry.Invoker.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselInvokerRoundTrip::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	const FString Args = TEXT("{\"Path\":\"/x/y\",\"Keys\":[\"alpha\",\"beta\"],\"Limit\":7}");
	const auto Result = FVesselToolInvoker::Invoke(FName(TEXT("FixtureRead")), Args);

	TestTrue(TEXT("RoundTrip: bOk"), Result.bOk);
	if (!Result.bOk) { AddError(Result.Message); return false; }

	// FixtureRead returns an FString that is itself a JSON object (escaped in
	// our return serializer). JSON serialization of a string produces
	// "\"{...}\"", so Value will contain the quoted form.
	TestTrue(TEXT("RoundTrip: return contains path"),    Result.Value.Contains(TEXT("/x/y")));
	TestTrue(TEXT("RoundTrip: return contains keys"),    Result.Value.Contains(TEXT("alpha,beta")));
	TestTrue(TEXT("RoundTrip: return contains limit 7"), Result.Value.Contains(TEXT("7")));
	return true;
}

/**
 * Missing required parameter triggers ValidationError with actionable message.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselInvokerMissingParam,
	"Vessel.Registry.Invoker.MissingParam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselInvokerMissingParam::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	const FString Args = TEXT("{\"Path\":\"/x\"}"); // missing Keys + Limit
	const auto Result = FVesselToolInvoker::Invoke(FName(TEXT("FixtureRead")), Args);

	TestFalse(TEXT("MissingParam: bOk false"), Result.bOk);
	TestEqual(TEXT("MissingParam: code"),
		static_cast<uint8>(Result.Code),
		static_cast<uint8>(EVesselResultCode::ValidationError));
	TestTrue(TEXT("MissingParam: names the missing param"),
		Result.Message.Contains(TEXT("Keys")) || Result.Message.Contains(TEXT("Limit")));
	TestTrue(TEXT("MissingParam: message mentions type"),
		Result.Message.Contains(TEXT("type")) || Result.Message.Contains(TEXT("Array")) || Result.Message.Contains(TEXT("int")));
	return true;
}

/**
 * Wrong argument type triggers ValidationError; LLM-readable message.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselInvokerWrongType,
	"Vessel.Registry.Invoker.WrongType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselInvokerWrongType::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	// Limit expects int; pass a string.
	const FString Args = TEXT("{\"Path\":\"/x\",\"Keys\":[],\"Limit\":\"not-a-number\"}");
	const auto Result = FVesselToolInvoker::Invoke(FName(TEXT("FixtureRead")), Args);

	TestFalse(TEXT("WrongType: bOk false"), Result.bOk);
	TestEqual(TEXT("WrongType: code"),
		static_cast<uint8>(Result.Code),
		static_cast<uint8>(EVesselResultCode::ValidationError));
	return true;
}

/**
 * Args JSON with markdown fence is accepted (sanitizer runs).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselInvokerFencedArgs,
	"Vessel.Registry.Invoker.FencedArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselInvokerFencedArgs::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	const FString Args =
		TEXT("```json\n")
		TEXT("{\"Path\":\"/z\",\"Keys\":[],\"Limit\":0}\n")
		TEXT("```");
	const auto Result = FVesselToolInvoker::Invoke(FName(TEXT("FixtureRead")), Args);

	TestTrue(TEXT("FencedArgs: sanitizer unwrapped the fence and invoker succeeded"), Result.bOk);
	return true;
}

/**
 * Transaction policy predicate: verify the open/skip decision for representative schemas.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselTransactionPolicyPredicate,
	"Vessel.Registry.Invoker.TransactionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselTransactionPolicyPredicate::RunTest(const FString& /*Parameters*/)
{
	FVesselToolSchema PureRead;
	PureRead.Category = TEXT("DataTable");
	PureRead.bRequiresApproval = false;
	PureRead.bIrreversibleHint = false;
	TestFalse(TEXT("Pure read → no transaction"),
		FVesselTransactionScope::ShouldOpenTransactionFor(PureRead));

	FVesselToolSchema Write;
	Write.Category = TEXT("DataTable/Write");
	Write.bRequiresApproval = true;
	Write.bIrreversibleHint = false;
	TestTrue(TEXT("Approval + Write → transaction"),
		FVesselTransactionScope::ShouldOpenTransactionFor(Write));

	FVesselToolSchema Irr;
	Irr.Category = TEXT("Asset/Write");
	Irr.bRequiresApproval = true;
	Irr.bIrreversibleHint = true;
	TestFalse(TEXT("IrreversibleHint wins over approval/write (no false safety)"),
		FVesselTransactionScope::ShouldOpenTransactionFor(Irr));

	return true;
}
