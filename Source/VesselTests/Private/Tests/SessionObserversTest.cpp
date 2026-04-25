// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"

#include "Llm/LlmProviderRegistry.h"
#include "Llm/VesselMockProvider.h"
#include "Registry/VesselToolRegistry.h"

#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"
#include "Session/VesselSessionTypes.h"

/**
 * Coverage for FVesselSessionMachine's observation delegates
 * (OnPlanReady / OnStepExecuted / OnJudgeVerdict). These are the hooks
 * the Slate chat panel and any future telemetry consumer use to surface
 * intermediate session state without parsing the JSONL.
 *
 * Contract under test:
 *   1. OnPlanReady fires once per valid + non-empty plan, BEFORE the FSM
 *      enters ToolSelection.
 *   2. OnPlanReady does NOT fire on parse-failure or empty plans.
 *   3. OnStepExecuted fires once per InvokeStep() call, on both success
 *      and tool-error paths.
 *   4. OnJudgeVerdict fires once per Judge response, regardless of decision.
 *   5. The three delegates fire in declared FSM order:
 *      PlanReady → StepExecuted → JudgeVerdict (per step).
 */

namespace VesselSessionObserversDetail
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

	static FLlmResponse MakeOkContent(const FString& Content)
	{
		FLlmResponse R;
		R.bOk = true;
		R.Content = Content;
		R.ProviderId = TEXT("mock");
		return R;
	}

	static void DeleteLog(const FString& Path)
	{
		if (!Path.IsEmpty())
		{
			IFileManager::Get().Delete(*Path, /*RequireExists*/false, /*EvenReadOnly*/true);
		}
	}

	/** Standard single-step happy-path planner fixture (FixtureRead, smoke). */
	static const TCHAR* kHappyPlan =
		TEXT("{\"plan\":[{\"tool\":\"FixtureRead\",")
		TEXT("\"args\":{\"Path\":\"/observers\",\"Keys\":[\"x\"],\"Limit\":1},")
		TEXT("\"reasoning\":\"smoke\"}]}");

	static const TCHAR* kJudgeApprove =
		TEXT("{\"decision\":\"approve\",\"reasoning\":\"observer test\"}");

	static const TCHAR* kJudgeReject =
		TEXT("{\"decision\":\"reject\",\"reasoning\":\"observer test reject\","
			"\"reject_reason\":\"observer-fixture-reject\"}");
}

/* ─── Test 1: PlanReady fires once on valid non-empty plan ───────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversPlanReadyOnValid,
	"Vessel.Session.Observers.PlanReadyOnValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversPlanReadyOnValid::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(TEXT("obs-valid"), MakeOkContent(kHappyPlan));
	Mock->SetDefaultResponse(MakeOkContent(kJudgeApprove));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	int32 PlanReadyCount = 0;
	int32 ObservedStepCount = -1;
	FName ObservedFirstTool;
	Machine->OnPlanReady.AddLambda(
		[&PlanReadyCount, &ObservedStepCount, &ObservedFirstTool]
		(const FVesselPlan& Plan)
		{
			++PlanReadyCount;
			ObservedStepCount = Plan.Steps.Num();
			if (Plan.Steps.Num() > 0) ObservedFirstTool = Plan.Steps[0].ToolName;
		});

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-valid")).Get();

	TestEqual(TEXT("OnPlanReady fired exactly once"), PlanReadyCount, 1);
	TestEqual(TEXT("Plan had 1 step"), ObservedStepCount, 1);
	TestEqual(TEXT("First step tool = FixtureRead"),
		ObservedFirstTool.ToString(), FString(TEXT("FixtureRead")));
	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 2: PlanReady does NOT fire on planner parse failure ───────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversPlanReadyNotOnInvalid,
	"Vessel.Session.Observers.PlanReadyNotOnInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversPlanReadyNotOnInvalid::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("obs-invalid"),
		MakeOkContent(TEXT("not even json")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	int32 PlanReadyCount = 0;
	Machine->OnPlanReady.AddLambda(
		[&PlanReadyCount](const FVesselPlan&) { ++PlanReadyCount; });

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-invalid")).Get();

	TestEqual(TEXT("OnPlanReady never fired on invalid plan"), PlanReadyCount, 0);
	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 3: PlanReady does NOT fire on empty plan ──────────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversPlanReadyNotOnEmpty,
	"Vessel.Session.Observers.PlanReadyNotOnEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversPlanReadyNotOnEmpty::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("obs-empty"),
		MakeOkContent(TEXT("{\"plan\":[]}")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	int32 PlanReadyCount = 0;
	Machine->OnPlanReady.AddLambda(
		[&PlanReadyCount](const FVesselPlan&) { ++PlanReadyCount; });

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-empty")).Get();

	TestEqual(TEXT("OnPlanReady not fired on empty plan"), PlanReadyCount, 0);
	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestEqual(TEXT("StepsExecuted == 0"), Outcome.StepsExecuted, 0);

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 4: StepExecuted fires once on success ────────────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversStepExecutedSuccess,
	"Vessel.Session.Observers.StepExecutedSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversStepExecutedSuccess::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(TEXT("obs-step"), MakeOkContent(kHappyPlan));
	Mock->SetDefaultResponse(MakeOkContent(kJudgeApprove));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	int32 StepCount = 0;
	bool bAnyError = false;
	FName ObservedTool;
	FString ObservedResult;
	Machine->OnStepExecuted.AddLambda(
		[&StepCount, &bAnyError, &ObservedTool, &ObservedResult]
		(const FVesselPlanStep& Step, const FString& ResultJson,
		 bool bWasError, const FString& /*ErrorMessage*/)
		{
			++StepCount;
			if (bWasError) bAnyError = true;
			ObservedTool = Step.ToolName;
			ObservedResult = ResultJson;
		});

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-step")).Get();

	TestEqual(TEXT("OnStepExecuted fired exactly once"), StepCount, 1);
	TestFalse(TEXT("bWasError = false on happy path"), bAnyError);
	TestEqual(TEXT("Step tool = FixtureRead"),
		ObservedTool.ToString(), FString(TEXT("FixtureRead")));
	TestTrue(TEXT("Result JSON is non-empty"), !ObservedResult.IsEmpty());
	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 5: JudgeVerdict fires on Approve ─────────────────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversJudgeApprove,
	"Vessel.Session.Observers.JudgeVerdictApprove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversJudgeApprove::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(TEXT("obs-judge"), MakeOkContent(kHappyPlan));
	Mock->SetDefaultResponse(MakeOkContent(kJudgeApprove));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	int32 VerdictCount = 0;
	EVesselJudgeDecision LastDecision = EVesselJudgeDecision::Reject;
	FString LastReasoning;
	Machine->OnJudgeVerdict.AddLambda(
		[&VerdictCount, &LastDecision, &LastReasoning]
		(const FVesselJudgeVerdict& V)
		{
			++VerdictCount;
			LastDecision = V.Decision;
			LastReasoning = V.Reasoning;
		});

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-judge")).Get();

	TestEqual(TEXT("OnJudgeVerdict fired exactly once"), VerdictCount, 1);
	TestEqual(TEXT("Decision = Approve"),
		static_cast<uint8>(LastDecision),
		static_cast<uint8>(EVesselJudgeDecision::Approve));
	TestTrue(TEXT("Reasoning preserved"),
		LastReasoning.Contains(TEXT("observer test")));
	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 6: JudgeVerdict fires even on Reject (Failed outcome) ─────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversJudgeRejectFires,
	"Vessel.Session.Observers.JudgeVerdictRejectFires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversJudgeRejectFires::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(TEXT("obs-rej"), MakeOkContent(kHappyPlan));
	Mock->SetDefaultResponse(MakeOkContent(kJudgeReject));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	int32 VerdictCount = 0;
	EVesselJudgeDecision LastDecision = EVesselJudgeDecision::Approve;
	Machine->OnJudgeVerdict.AddLambda(
		[&VerdictCount, &LastDecision](const FVesselJudgeVerdict& V)
		{
			++VerdictCount;
			LastDecision = V.Decision;
		});

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-rej")).Get();

	TestEqual(TEXT("OnJudgeVerdict fired exactly once"), VerdictCount, 1);
	TestEqual(TEXT("Decision = Reject"),
		static_cast<uint8>(LastDecision),
		static_cast<uint8>(EVesselJudgeDecision::Reject));
	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}

/* ─── Test 7: Fire ordering Plan → Step → Verdict ────────────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionObserversFireOrder,
	"Vessel.Session.Observers.FireOrderPlanThenStepThenVerdict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionObserversFireOrder::RunTest(const FString& /*Parameters*/)
{
	using namespace VesselSessionObserversDetail;

	FVesselToolRegistry::Get().ScanAll();
	auto Mock = PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(TEXT("obs-order"), MakeOkContent(kHappyPlan));
	Mock->SetDefaultResponse(MakeOkContent(kJudgeApprove));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	TArray<FString> FireSequence;
	Machine->OnPlanReady.AddLambda(
		[&FireSequence](const FVesselPlan&) { FireSequence.Add(TEXT("plan")); });
	Machine->OnStepExecuted.AddLambda(
		[&FireSequence](const FVesselPlanStep&, const FString&, bool, const FString&)
		{ FireSequence.Add(TEXT("step")); });
	Machine->OnJudgeVerdict.AddLambda(
		[&FireSequence](const FVesselJudgeVerdict&) { FireSequence.Add(TEXT("verdict")); });

	const FString LogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("obs-order")).Get();

	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestEqual(TEXT("Three events fired"), FireSequence.Num(), 3);
	if (FireSequence.Num() == 3)
	{
		TestEqual(TEXT("[0] = plan"),    FireSequence[0], FString(TEXT("plan")));
		TestEqual(TEXT("[1] = step"),    FireSequence[1], FString(TEXT("step")));
		TestEqual(TEXT("[2] = verdict"), FireSequence[2], FString(TEXT("verdict")));
	}

	DeleteLog(LogPath);
	RestoreDefaultMockProvider();
	return true;
}
