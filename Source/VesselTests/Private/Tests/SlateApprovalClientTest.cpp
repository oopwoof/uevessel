// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Widgets/VesselSlateApprovalClient.h"

/**
 * When no delegate is bound, the client auto-rejects so the session cannot
 * hang indefinitely waiting for a UI that does not exist.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSlateApprovalClientNoDelegateAutoRejects,
	"Vessel.HITL.Slate.NoDelegateAutoRejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSlateApprovalClientNoDelegateAutoRejects::RunTest(const FString& /*Parameters*/)
{
	auto Client = MakeShared<FVesselSlateApprovalClient>();

	FVesselApprovalRequest Req;
	Req.ToolName = FName(TEXT("AnyTool"));
	Req.ArgsJson = TEXT("{}");
	Req.StepIndex = 1;

	const FVesselApprovalDecision Decision = Client->RequestDecisionAsync(Req).Get();
	TestEqual(TEXT("Unbound client returns Reject"),
		static_cast<uint8>(Decision.Kind),
		static_cast<uint8>(EVesselApprovalDecisionKind::Reject));
	TestTrue(TEXT("Reject reason is actionable"),
		Decision.RejectReason.Contains(TEXT("Vessel panel")));
	return true;
}

/**
 * When a delegate is bound, it receives the request + a promise it can fulfill.
 * Approving resolves the future with Approve; no further pending request.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSlateApprovalClientDelegateFulfills,
	"Vessel.HITL.Slate.DelegateFulfills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSlateApprovalClientDelegateFulfills::RunTest(const FString& /*Parameters*/)
{
	auto Client = MakeShared<FVesselSlateApprovalClient>();
	bool bDelegateCalled = false;

	Client->OnApprovalRequested.BindLambda(
		[&bDelegateCalled](const FVesselApprovalRequest& /*Req*/,
		                   TSharedRef<TPromise<FVesselApprovalDecision>> Promise)
		{
			bDelegateCalled = true;
			Promise->SetValue(FVesselApprovalDecision::MakeApprove(TEXT("test-harness")));
		});

	FVesselApprovalRequest Req;
	Req.ToolName = FName(TEXT("AnyTool"));
	Req.StepIndex = 1;

	const FVesselApprovalDecision Decision = Client->RequestDecisionAsync(Req).Get();
	TestTrue(TEXT("Delegate was called"), bDelegateCalled);
	TestEqual(TEXT("Decision is Approve"),
		static_cast<uint8>(Decision.Kind),
		static_cast<uint8>(EVesselApprovalDecisionKind::Approve));
	TestEqual(TEXT("Decider round-trips"),
		Decision.DeciderId, FString(TEXT("test-harness")));
	TestFalse(TEXT("No pending after fulfillment"), Client->HasPending());
	return true;
}

/**
 * A second request made while the first is in flight is immediately auto-rejected
 * rather than queued. Session serializes approvals; queue policy is UI's problem.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSlateApprovalClientSecondRequestAutoRejects,
	"Vessel.HITL.Slate.SecondRequestAutoRejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSlateApprovalClientSecondRequestAutoRejects::RunTest(const FString& /*Parameters*/)
{
	auto Client = MakeShared<FVesselSlateApprovalClient>();

	// First request: delegate parks the promise and does NOT fulfill yet.
	TSharedPtr<TPromise<FVesselApprovalDecision>> ParkedPromise;
	Client->OnApprovalRequested.BindLambda(
		[&ParkedPromise](const FVesselApprovalRequest& /*Req*/,
		                 TSharedRef<TPromise<FVesselApprovalDecision>> Promise)
		{
			ParkedPromise = Promise;
		});

	FVesselApprovalRequest Req;
	Req.ToolName = FName(TEXT("First"));
	TFuture<FVesselApprovalDecision> FirstFuture = Client->RequestDecisionAsync(Req);
	TestTrue(TEXT("Client has pending after first"), Client->HasPending());
	TestFalse(TEXT("First future not yet ready"), FirstFuture.IsReady());

	// Second request: client should auto-reject synchronously.
	FVesselApprovalRequest Req2;
	Req2.ToolName = FName(TEXT("Second"));
	const FVesselApprovalDecision SecondDecision = Client->RequestDecisionAsync(Req2).Get();
	TestEqual(TEXT("Second is Reject"),
		static_cast<uint8>(SecondDecision.Kind),
		static_cast<uint8>(EVesselApprovalDecisionKind::Reject));

	// Now fulfill the first to prove the parked promise still works.
	if (ParkedPromise.IsValid())
	{
		ParkedPromise->SetValue(FVesselApprovalDecision::MakeApprove(TEXT("test")));
	}
	FirstFuture.Wait();
	const FVesselApprovalDecision FirstDecision = FirstFuture.Get();
	TestEqual(TEXT("First ultimately Approved"),
		static_cast<uint8>(FirstDecision.Kind),
		static_cast<uint8>(EVesselApprovalDecisionKind::Approve));
	TestFalse(TEXT("Pending cleared after fulfillment"), Client->HasPending());
	return true;
}
