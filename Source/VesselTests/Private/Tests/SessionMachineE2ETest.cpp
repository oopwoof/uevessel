// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#include "Llm/LlmProviderRegistry.h"
#include "Llm/VesselMockProvider.h"
#include "Registry/VesselToolRegistry.h"

#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"
#include "Session/VesselSessionTypes.h"

namespace VesselSessionE2ETestDetail
{
	/**
	 * Swap the production "mock" provider for a freshly configured one with
	 * Planner + Judge fixtures. Returns the test-owned instance so the caller
	 * can cycle fixtures per test.
	 */
	static TSharedRef<FVesselMockProvider> PushFreshMockProvider()
	{
		auto Fresh = MakeShared<FVesselMockProvider>();
		FLlmProviderRegistry::Get().InjectMock(Fresh);
		return Fresh;
	}

	/** After the test, restore an empty mock so later tests aren't polluted. */
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
}

/**
 * End-to-end: session runs Planner (mock) → Invoke FixtureRead → Judge (mock approve)
 * → Done. Validates outcome + session log contents.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionE2ESingleStepApprove,
	"Vessel.Session.E2E.SingleStepApprove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionE2ESingleStepApprove::RunTest(const FString& /*Parameters*/)
{
	// Ensure the FixtureRead tool is in the registry.
	FVesselToolRegistry::Get().ScanAll();
	TestNotNull(TEXT("FixtureRead registered"),
		FVesselToolRegistry::Get().FindSchema(FName(TEXT("FixtureRead"))));

	auto Mock = VesselSessionE2ETestDetail::PushFreshMockProvider();

	// Planner: for user "e2e-singlestep" → plan one FixtureRead call.
	Mock->SetFixtureForLastUserMessage(
		TEXT("e2e-singlestep"),
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"plan\":[{\"tool\":\"FixtureRead\",")
			TEXT("\"args\":{\"Path\":\"/e2e\",\"Keys\":[\"alpha\"],\"Limit\":3},")
			TEXT("\"reasoning\":\"smoke\"}]}")));

	// Judge: default response = approve (Judge user message is dynamic).
	Mock->SetDefaultResponse(
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"decision\":\"approve\",\"reasoning\":\"fixture returned expected shape\"}")));

	// Configure & run.
	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	TestTrue(TEXT("Machine inits"), Machine->Init(Cfg));

	const FString LogPath = Machine->GetLogFilePath();

	TFuture<FVesselSessionOutcome> Fut = Machine->RunAsync(TEXT("e2e-singlestep"));
	// Mock is synchronous; .Get() returns immediately without blocking the game thread.
	const FVesselSessionOutcome Outcome = Fut.Get();

	TestEqual(TEXT("Outcome = Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestEqual(TEXT("StepsExecuted == 1"), Outcome.StepsExecuted, 1);
	TestEqual(TEXT("Final state is Done"),
		static_cast<uint8>(Machine->GetCurrentState()),
		static_cast<uint8>(EVesselSessionState::Done));

	// Verify structured log contents.
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *LogPath))
	{
		bool bSawOpen     = false;
		bool bSawPlanning = false;
		bool bSawStep     = false;
		bool bSawVerdict  = false;
		bool bSawSummary  = false;
		for (const FString& L : Lines)
		{
			if (L.Contains(TEXT("\"type\":\"SessionOpen\"")))     bSawOpen     = true;
			if (L.Contains(TEXT("\"type\":\"PlanningResult\"")))  bSawPlanning = true;
			if (L.Contains(TEXT("\"type\":\"StepExecuted\"")))    bSawStep     = true;
			if (L.Contains(TEXT("\"type\":\"JudgeVerdict\"")))    bSawVerdict  = true;
			if (L.Contains(TEXT("\"type\":\"SessionSummary\""))) bSawSummary   = true;
		}
		TestTrue(TEXT("Log has SessionOpen"),     bSawOpen);
		TestTrue(TEXT("Log has PlanningResult"),  bSawPlanning);
		TestTrue(TEXT("Log has StepExecuted"),    bSawStep);
		TestTrue(TEXT("Log has JudgeVerdict"),    bSawVerdict);
		TestTrue(TEXT("Log has SessionSummary"), bSawSummary);
	}
	else
	{
		AddError(FString::Printf(TEXT("Could not read session log: %s"), *LogPath));
	}

	VesselSessionE2ETestDetail::DeleteLog(LogPath);
	VesselSessionE2ETestDetail::RestoreDefaultMockProvider();
	return true;
}

/**
 * Malformed planner response → session Fails with a readable reason.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionE2EPlannerMalformed,
	"Vessel.Session.E2E.PlannerMalformed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionE2EPlannerMalformed::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselSessionE2ETestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("e2e-planner-bad"),
		VesselSessionE2ETestDetail::MakeOkContent(TEXT("this is not JSON at all")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->Init(Cfg);
	const FString LogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("e2e-planner-bad")).Get();

	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));
	TestTrue(TEXT("Failure reason mentions planner"),
		Outcome.Reason.Contains(TEXT("Planner")) || Outcome.Reason.Contains(TEXT("plan")));

	VesselSessionE2ETestDetail::DeleteLog(LogPath);
	VesselSessionE2ETestDetail::RestoreDefaultMockProvider();
	return true;
}

/**
 * Planner returns plan referencing a tool that does NOT exist in the registry.
 * Expectation: session Fails with an error naming the unknown tool.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionE2EUnknownTool,
	"Vessel.Session.E2E.UnknownTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionE2EUnknownTool::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselSessionE2ETestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("e2e-unknown"),
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"plan\":[{\"tool\":\"NotInRegistry_XYZ\",\"args\":{},\"reasoning\":\"boom\"}]}")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->Init(Cfg);
	const FString LogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("e2e-unknown")).Get();

	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));
	TestTrue(TEXT("Failure names the unknown tool"),
		Outcome.Reason.Contains(TEXT("NotInRegistry_XYZ")));

	VesselSessionE2ETestDetail::DeleteLog(LogPath);
	VesselSessionE2ETestDetail::RestoreDefaultMockProvider();
	return true;
}

/**
 * Judge returns Reject → session Fails, reject reason surfaces.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionE2EJudgeReject,
	"Vessel.Session.E2E.JudgeReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionE2EJudgeReject::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselSessionE2ETestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("e2e-reject"),
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"plan\":[{\"tool\":\"FixtureRead\",")
			TEXT("\"args\":{\"Path\":\"/r\",\"Keys\":[],\"Limit\":0},\"reasoning\":\"go\"}]}")));

	Mock->SetDefaultResponse(
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"decision\":\"reject\",\"reasoning\":\"wrong path\",")
			TEXT("\"reject_reason\":\"The /r path is outside allowed scope\"}")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->Init(Cfg);
	const FString LogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("e2e-reject")).Get();

	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));
	TestTrue(TEXT("Reject reason surfaces"),
		Outcome.Reason.Contains(TEXT("outside allowed scope")));
	TestEqual(TEXT("StepsExecuted still counts the executed-then-rejected step"),
		Outcome.StepsExecuted, 1);

	VesselSessionE2ETestDetail::DeleteLog(LogPath);
	VesselSessionE2ETestDetail::RestoreDefaultMockProvider();
	return true;
}

/**
 * Budget: MaxConsecutiveRevise=1, Judge revises forever → Failed with budget reason.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionE2EConsecutiveReviseBudget,
	"Vessel.Session.E2E.ConsecutiveReviseBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionE2EConsecutiveReviseBudget::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselSessionE2ETestDetail::PushFreshMockProvider();

	// Planner always returns the same single-step plan.
	Mock->SetFixtureForLastUserMessage(
		TEXT("e2e-revise-loop"),
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"plan\":[{\"tool\":\"FixtureRead\",")
			TEXT("\"args\":{\"Path\":\"/x\",\"Keys\":[],\"Limit\":0},\"reasoning\":\"try\"}]}")));

	// Judge always revises.
	Mock->SetDefaultResponse(
		VesselSessionE2ETestDetail::MakeOkContent(
			TEXT("{\"decision\":\"revise\",\"reasoning\":\"try again\",")
			TEXT("\"revise_directive\":\"try different inputs\"}")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");
	Cfg.Budget.MaxConsecutiveRevise = 2;
	Cfg.Budget.MaxSteps = 50;   // prevent step budget from triggering first

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->Init(Cfg);
	const FString LogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("e2e-revise-loop")).Get();

	TestEqual(TEXT("Outcome = Failed"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));
	TestTrue(TEXT("Failure reason mentions revise"),
		Outcome.Reason.Contains(TEXT("revise")) || Outcome.Reason.Contains(TEXT("Consecutive")));

	VesselSessionE2ETestDetail::DeleteLog(LogPath);
	VesselSessionE2ETestDetail::RestoreDefaultMockProvider();
	return true;
}

/**
 * Empty plan (planner returns {"plan":[]}) → session completes Done without executing anything.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionE2EEmptyPlan,
	"Vessel.Session.E2E.EmptyPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionE2EEmptyPlan::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselSessionE2ETestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("e2e-empty"),
		VesselSessionE2ETestDetail::MakeOkContent(TEXT("{\"plan\":[]}")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->Init(Cfg);
	const FString LogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("e2e-empty")).Get();

	TestEqual(TEXT("Empty plan completes as Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestEqual(TEXT("No steps executed"), Outcome.StepsExecuted, 0);

	VesselSessionE2ETestDetail::DeleteLog(LogPath);
	VesselSessionE2ETestDetail::RestoreDefaultMockProvider();
	return true;
}
