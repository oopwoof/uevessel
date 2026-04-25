// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#include "Fixtures/VesselTestToolFixture.h"

#include "Llm/LlmProviderRegistry.h"
#include "Llm/VesselMockProvider.h"

#include "Registry/VesselToolRegistry.h"
#include "Registry/VesselToolSchema.h"

#include "Session/VesselApprovalClient.h"
#include "Session/VesselApprovalTypes.h"
#include "Session/VesselRejectionSink.h"
#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"

namespace VesselHITLTestDetail
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

	static void DeletePath(const FString& Path)
	{
		if (!Path.IsEmpty())
		{
			IFileManager::Get().Delete(*Path, /*RequireExists*/false, /*EvenReadOnly*/true);
		}
	}

	/** Plan body that calls FixtureIrreversibleWrite (RequiresApproval=true, Irreversible=true). */
	static FString PlanCallsIrreversibleWrite()
	{
		return TEXT("{\"plan\":[{\"tool\":\"FixtureIrreversibleWrite\",")
		       TEXT("\"args\":{\"Target\":\"/hitl/path\"},")
		       TEXT("\"reasoning\":\"write to target\"}]}");
	}
}

// =============================================================================
// StepNeedsApproval — pure predicate coverage
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselHITLPredicateRequiresApproval,
	"Vessel.HITL.Predicate.RequiresApproval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselHITLPredicateRequiresApproval::RunTest(const FString& /*Parameters*/)
{
	FVesselToolSchema A;
	A.bRequiresApproval = true;
	TestTrue(TEXT("RequiresApproval=true triggers HITL"),
		FVesselSessionMachine::StepNeedsApproval(A));

	FVesselToolSchema B;
	B.bRequiresApproval = false;
	B.bIrreversibleHint = true;
	TestTrue(TEXT("IrreversibleHint=true triggers HITL even without RequiresApproval"),
		FVesselSessionMachine::StepNeedsApproval(B));

	FVesselToolSchema C;
	C.Category = TEXT("DataTable/Write");
	TestTrue(TEXT("Category containing 'Write' triggers HITL"),
		FVesselSessionMachine::StepNeedsApproval(C));

	FVesselToolSchema D;
	D.Category = TEXT("Meta");
	D.bRequiresApproval = false;
	D.bIrreversibleHint = false;
	TestFalse(TEXT("Plain read-only tool does not trigger HITL"),
		FVesselSessionMachine::StepNeedsApproval(D));

	return true;
}

// =============================================================================
// End-to-end: Approve path
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselHITLApprovePath,
	"Vessel.HITL.E2E.ApprovePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselHITLApprovePath::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselHITLTestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("hitl-approve"),
		VesselHITLTestDetail::MakeOkContent(VesselHITLTestDetail::PlanCallsIrreversibleWrite()));
	Mock->SetDefaultResponse(
		VesselHITLTestDetail::MakeOkContent(
			TEXT("{\"decision\":\"approve\",\"reasoning\":\"write accepted\"}")));

	auto Scripted = MakeShared<FVesselScriptedApprovalClient>();
	Scripted->SetDecisionForTool(FName(TEXT("FixtureIrreversibleWrite")),
		FVesselApprovalDecision::MakeApprove(TEXT("test-user")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->SetApprovalClient(Scripted);
	TestTrue(TEXT("Init succeeds"), Machine->Init(Cfg));

	const FString SessionLogPath = Machine->GetLogFilePath();
	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("hitl-approve")).Get();

	TestEqual(TEXT("Session completes as Done"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestEqual(TEXT("ApprovalClient was queried exactly once"),
		Scripted->GetRequestCount(), 1);

	// Log contains ApprovalRequested + ApprovalDecision markers.
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *SessionLogPath))
	{
		bool bReq = false, bDec = false;
		for (const FString& L : Lines)
		{
			if (L.Contains(TEXT("\"type\":\"ApprovalRequested\""))) bReq = true;
			if (L.Contains(TEXT("\"type\":\"ApprovalDecision\"")))  bDec = true;
		}
		TestTrue(TEXT("ApprovalRequested logged"), bReq);
		TestTrue(TEXT("ApprovalDecision logged"),  bDec);
	}

	VesselHITLTestDetail::DeletePath(SessionLogPath);
	VesselHITLTestDetail::RestoreDefaultMockProvider();
	return true;
}

// =============================================================================
// End-to-end: Reject path persists to AGENTS.md + archive JSONL
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselHITLRejectSinksToAgentsMd,
	"Vessel.HITL.E2E.RejectSinksToAgentsMd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselHITLRejectSinksToAgentsMd::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	// Snapshot existing AGENTS.md so we can restore it and not pollute the repo.
	const FString AgentsPath = FVesselRejectionSink::GetAgentsMdPath();
	FString AgentsBefore;
	const bool bHadBefore = FFileHelper::LoadFileToString(AgentsBefore, *AgentsPath);

	auto Mock = VesselHITLTestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("hitl-reject"),
		VesselHITLTestDetail::MakeOkContent(VesselHITLTestDetail::PlanCallsIrreversibleWrite()));

	const FString Reason = TEXT("test-reject-reason: target outside scope");
	auto Scripted = MakeShared<FVesselScriptedApprovalClient>();
	Scripted->SetDecisionForTool(FName(TEXT("FixtureIrreversibleWrite")),
		FVesselApprovalDecision::MakeReject(Reason, TEXT("test-user")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->SetApprovalClient(Scripted);
	Machine->Init(Cfg);
	const FString SessionLogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("hitl-reject")).Get();

	TestEqual(TEXT("Session fails on HITL reject"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Failed));
	TestTrue(TEXT("Failure reason mentions HITL"),
		Outcome.Reason.Contains(TEXT("HITL")) || Outcome.Reason.Contains(Reason));

	// AGENTS.md contains the reason
	FString AgentsAfter;
	if (FFileHelper::LoadFileToString(AgentsAfter, *AgentsPath))
	{
		TestTrue(TEXT("AGENTS.md contains '## Known Rejections'"),
			AgentsAfter.Contains(TEXT("## Known Rejections")));
		TestTrue(TEXT("AGENTS.md contains reject reason"),
			AgentsAfter.Contains(Reason));
		TestTrue(TEXT("AGENTS.md records tool name"),
			AgentsAfter.Contains(TEXT("FixtureIrreversibleWrite")));
	}
	else
	{
		AddError(FString::Printf(TEXT("AGENTS.md was not written to '%s'"), *AgentsPath));
	}

	// Monthly archive contains a JSON record
	const FString ArchivePath = FVesselRejectionSink::GetArchivePathForMonth(FDateTime::UtcNow());
	FString ArchiveContents;
	if (FFileHelper::LoadFileToString(ArchiveContents, *ArchivePath))
	{
		TestTrue(TEXT("Archive mentions reject reason"),
			ArchiveContents.Contains(Reason));
		TestTrue(TEXT("Archive has tool name"),
			ArchiveContents.Contains(TEXT("FixtureIrreversibleWrite")));
	}

	// Restore AGENTS.md to pre-test state (preserve repo cleanliness)
	if (bHadBefore)
	{
		FFileHelper::SaveStringToFile(AgentsBefore, *AgentsPath);
	}
	else
	{
		VesselHITLTestDetail::DeletePath(AgentsPath);
	}
	VesselHITLTestDetail::DeletePath(SessionLogPath);
	// Archive retained across tests — it's a monthly append log, other tests
	// may still want it. Safe to leave.
	VesselHITLTestDetail::RestoreDefaultMockProvider();
	return true;
}

// =============================================================================
// End-to-end: Edit-and-Approve replaces args
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselHITLEditAndApprove,
	"Vessel.HITL.E2E.EditAndApprove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselHITLEditAndApprove::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselHITLTestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("hitl-edit"),
		VesselHITLTestDetail::MakeOkContent(VesselHITLTestDetail::PlanCallsIrreversibleWrite()));
	Mock->SetDefaultResponse(
		VesselHITLTestDetail::MakeOkContent(
			TEXT("{\"decision\":\"approve\",\"reasoning\":\"ok with revised args\"}")));

	auto Scripted = MakeShared<FVesselScriptedApprovalClient>();
	Scripted->SetDecisionForTool(FName(TEXT("FixtureIrreversibleWrite")),
		FVesselApprovalDecision::MakeEdit(
			TEXT("{\"Target\":\"/revised/path\"}"),
			TEXT("test-user")));

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->SetApprovalClient(Scripted);
	Machine->Init(Cfg);
	const FString SessionLogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("hitl-edit")).Get();

	TestEqual(TEXT("Session completes Done after edit"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));

	// Session's stored plan should reflect the edit so replay sees reality.
	const FVesselPlan& Plan = Machine->GetCurrentPlan();
	if (Plan.Steps.Num() >= 1)
	{
		TestTrue(TEXT("Stored plan args reflect the edit"),
			Plan.Steps[0].ArgsJson.Contains(TEXT("/revised/path")));
	}

	// Log contains the revised args
	FString Contents;
	if (FFileHelper::LoadFileToString(Contents, *SessionLogPath))
	{
		TestTrue(TEXT("Log notes EditAndApprove decision"),
			Contents.Contains(TEXT("EditAndApprove")));
		TestTrue(TEXT("Log carries the revised args"),
			Contents.Contains(TEXT("/revised/path")));
	}

	VesselHITLTestDetail::DeletePath(SessionLogPath);
	VesselHITLTestDetail::RestoreDefaultMockProvider();
	return true;
}

// =============================================================================
// Non-HITL tool does not consult the ApprovalClient at all
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselHITLReadOnlyBypass,
	"Vessel.HITL.E2E.ReadOnlyBypass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselHITLReadOnlyBypass::RunTest(const FString& /*Parameters*/)
{
	FVesselToolRegistry::Get().ScanAll();

	auto Mock = VesselHITLTestDetail::PushFreshMockProvider();
	Mock->SetFixtureForLastUserMessage(
		TEXT("hitl-bypass"),
		VesselHITLTestDetail::MakeOkContent(
			TEXT("{\"plan\":[{\"tool\":\"FixtureRead\",")
			TEXT("\"args\":{\"Path\":\"/x\",\"Keys\":[],\"Limit\":0},")
			TEXT("\"reasoning\":\"read only\"}]}")));
	Mock->SetDefaultResponse(
		VesselHITLTestDetail::MakeOkContent(
			TEXT("{\"decision\":\"approve\",\"reasoning\":\"ok\"}")));

	auto Scripted = MakeShared<FVesselScriptedApprovalClient>();
	// Do not map the tool; default is Reject. If approval is asked, we Fail.

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(FString());
	Cfg.ProviderId = TEXT("mock");

	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->SetApprovalClient(Scripted);
	Machine->Init(Cfg);
	const FString SessionLogPath = Machine->GetLogFilePath();

	const FVesselSessionOutcome Outcome = Machine->RunAsync(TEXT("hitl-bypass")).Get();

	TestEqual(TEXT("Read-only session completes without HITL"),
		static_cast<uint8>(Outcome.Kind),
		static_cast<uint8>(EVesselSessionOutcomeKind::Done));
	TestEqual(TEXT("ApprovalClient was NOT queried for the read-only step"),
		Scripted->GetRequestCount(), 0);

	VesselHITLTestDetail::DeletePath(SessionLogPath);
	VesselHITLTestDetail::RestoreDefaultMockProvider();
	return true;
}
