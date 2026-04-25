// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"

#include "Llm/LlmProviderRegistry.h"
#include "Llm/VesselLlmTypes.h"
#include "Llm/VesselMockProvider.h"
#include "Registry/VesselToolRegistry.h"

#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"
#include "Session/VesselSessionTypes.h"

/**
 * Coverage for FVesselSessionMachine::TotalCostUsd accumulation. The
 * pricing math itself is covered by Vessel.Llm.Pricing.* — this file
 * verifies that cost flows correctly through the FSM:
 *   - sums every Planner + Judge response that returns a usage block
 *   - terminates with the running total in FVesselSessionOutcome
 *   - includes the failed-plan cost when Planner returns invalid JSON
 *
 * Caught as gap #7 in Gemini's v0.2 review.
 */

namespace VesselCostAccDetail
{
	static TSharedRef<FVesselMockProvider> PushFreshMockProvider()
	{
		auto Fresh = MakeShared<FVesselMockProvider>();
		FLlmProviderRegistry::Get().InjectMock(Fresh);
		return Fresh;
	}

	static void RestoreDefaultMockProvider()
	{
		FLlmProviderRegistry::Get().InjectMock(MakeShared<FVesselMockProvider>());
	}

	static FLlmResponse MakeOkContentWithCost(const FString& Content, float CostUsd)
	{
		FLlmResponse R;
		R.bOk = true;
		R.Content = Content;
		R.ProviderId = TEXT("mock");
		R.Usage.EstimatedCostUsd = CostUsd;
		return R;
	}

	static void DeleteLog(const FString& Path)
	{
		if (!Path.IsEmpty())
		{
			IFileManager::Get().Delete(*Path, /*RequireExists*/false, /*EvenReadOnly*/true);
		}
	}

	static const TCHAR* kHappyPlan =
		TEXT("{\"plan\":[{\"tool\":\"FixtureRead\",")
		TEXT("\"args\":{\"Path\":\"/cost\",\"Keys\":[\"x\"],\"Limit\":1},")
		TEXT("\"reasoning\":\"smoke\"}]}");

	static const TCHAR* kJudgeApprove =
		TEXT("{\"decision\":\"approve\",\"reasoning\":\"cost test\"}");
}

/* ─── Test 1: Single-step session sums Planner + Judge costs ─────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionCostHappyPath,
	"Vessel.Session.Cost.AccumulatesAcrossPlannerAndJudge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionCostHappyPath::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselCostAccDetail;
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = PushFreshMockProvider();
	// Planner cost = $0.01000 (set on the user-message-keyed fixture)
	Mock->SetFixtureForLastUserMessage(
		TEXT("cost-happy"),
		MakeOkContentWithCost(kHappyPlan, 0.01f));
	// Judge cost = $0.00500 (set on the default response — mock will use
	// it for every call that is not the planner user message above).
	Mock->SetDefaultResponse(
		MakeOkContentWithCost(kJudgeApprove, 0.005f));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	const FString SessionLogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("cost-happy")).Get();

	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));

	// Allow tiny float-conversion drift since EstimatedCostUsd is float
	// but TotalCostUsd accumulates as double.
	const double Expected = 0.01 + 0.005; // planner + judge
	const double Got = Machine->GetTotalCostUsd();
	TestTrue(FString::Printf(
		TEXT("TotalCostUsd ≈ planner+judge (got %.6f, expected %.6f)"),
		Got, Expected),
		FMath::IsNearlyEqual(Got, Expected, 1e-5));

	// Outcome carries its own snapshot of the total — assert parity.
	TestTrue(TEXT("Outcome.TotalCostUsd ≈ Machine.GetTotalCostUsd()"),
		FMath::IsNearlyEqual(Outcome.TotalCostUsd, Got, 1e-9));

	DeleteLog(SessionLogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 2: Failed Planner still records its own cost ─────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionCostFailedPlanner,
	"Vessel.Session.Cost.IncludesFailedPlannerCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionCostFailedPlanner::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselCostAccDetail;
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = PushFreshMockProvider();
	// Planner returns garbage but still has a usage cost — the call
	// *was* made to the API, the API was just told to be wrong.
	Mock->SetFixtureForLastUserMessage(
		TEXT("cost-fail"),
		MakeOkContentWithCost(TEXT("not even json"), 0.02f));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	const FString SessionLogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("cost-fail")).Get();

	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));

	// Even on parse failure we counted the planner call.
	TestTrue(TEXT("Failed-plan cost still booked"),
		FMath::IsNearlyEqual(Machine->GetTotalCostUsd(), 0.02, 1e-5));
	TestTrue(TEXT("Outcome carries the failed-plan cost"),
		FMath::IsNearlyEqual(Outcome.TotalCostUsd, 0.02, 1e-5));

	DeleteLog(SessionLogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 3: Empty plan path doesn't double-count the Planner ──────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionCostEmptyPlan,
	"Vessel.Session.Cost.EmptyPlanCountsPlannerOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionCostEmptyPlan::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselCostAccDetail;
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("cost-empty"),
		MakeOkContentWithCost(TEXT("{\"plan\":[]}"), 0.003f));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	const FString SessionLogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("cost-empty")).Get();

	TestEqual(TEXT("Outcome = Done (empty plan = no-op success)"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestTrue(TEXT("Empty-plan path still books the Planner call"),
		FMath::IsNearlyEqual(Machine->GetTotalCostUsd(), 0.003, 1e-5));
	TestTrue(TEXT("No Judge call counted (no step ran)"),
		Machine->GetTotalCostUsd() < 0.004 - 1e-9);

	DeleteLog(SessionLogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 4: Cost is observable from inside an OnPlanReady handler ──── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionCostVisibleAtPlanReady,
	"Vessel.Session.Cost.VisibleAtObservationTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionCostVisibleAtPlanReady::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselCostAccDetail;
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("cost-observable"),
		MakeOkContentWithCost(kHappyPlan, 0.007f));
	Mock->SetDefaultResponse(MakeOkContentWithCost(kJudgeApprove, 0.002f));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	double CostAtPlanReady = -1.0;
	double CostAtJudge = -1.0;
	TWeakPtr<FVesselSessionMachine> Weak = Machine;
	Machine->OnPlanReady.AddLambda([Weak, &CostAtPlanReady](const FVesselPlan&)
	{
		if (TSharedPtr<FVesselSessionMachine> P = Weak.Pin())
		{
			CostAtPlanReady = P->GetTotalCostUsd();
		}
	});
	Machine->OnJudgeVerdict.AddLambda([Weak, &CostAtJudge](const FVesselJudgeVerdict&)
	{
		if (TSharedPtr<FVesselSessionMachine> P = Weak.Pin())
		{
			CostAtJudge = P->GetTotalCostUsd();
		}
	});

	const FString SessionLogPath = Machine->GetLogFilePath();
	(void)Machine->RunAsync(TEXT("cost-observable")).Get();

	// At OnPlanReady time only the Planner cost is in the running total.
	TestTrue(FString::Printf(
		TEXT("OnPlanReady saw planner cost only (got %.6f)"), CostAtPlanReady),
		FMath::IsNearlyEqual(CostAtPlanReady, 0.007, 1e-5));

	// At OnJudgeVerdict time both Planner and Judge are summed.
	TestTrue(FString::Printf(
		TEXT("OnJudgeVerdict saw planner + judge total (got %.6f)"), CostAtJudge),
		FMath::IsNearlyEqual(CostAtJudge, 0.007 + 0.002, 1e-5));

	DeleteLog(SessionLogPath);
	RestoreDefaultMockProvider();
	return true;
}
