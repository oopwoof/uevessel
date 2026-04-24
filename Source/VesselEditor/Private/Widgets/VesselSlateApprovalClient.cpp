// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/VesselSlateApprovalClient.h"

#include "VesselLog.h"

TFuture<FVesselApprovalDecision> FVesselSlateApprovalClient::RequestDecisionAsync(
	const FVesselApprovalRequest& Request)
{
	TSharedRef<TPromise<FVesselApprovalDecision>> Promise =
		MakeShared<TPromise<FVesselApprovalDecision>>();
	TFuture<FVesselApprovalDecision> Future = Promise->GetFuture();

	if (bPending)
	{
		UE_LOG(LogVesselHITL, Warning,
			TEXT("SlateApprovalClient received a second request while the first is unresolved — auto-rejecting."));
		Promise->SetValue(FVesselApprovalDecision::MakeReject(
			TEXT("Another approval is already pending. Resolve the current one first."),
			TEXT("slate-client:auto")));
		return Future;
	}

	if (!OnApprovalRequested.IsBound())
	{
		UE_LOG(LogVesselHITL, Error,
			TEXT("SlateApprovalClient has no UI delegate bound — auto-rejecting request for tool '%s'."),
			*Request.ToolName.ToString());
		Promise->SetValue(FVesselApprovalDecision::MakeReject(
			TEXT("No Vessel panel is open to approve this request. Open Window → Vessel Chat and try again."),
			TEXT("slate-client:auto")));
		return Future;
	}

	bPending = true;

	// Wrap the promise so we can clear bPending when the widget fulfills it.
	TSharedRef<TPromise<FVesselApprovalDecision>> WrappedPromise =
		MakeShared<TPromise<FVesselApprovalDecision>>();
	TFuture<FVesselApprovalDecision> WrappedFuture = WrappedPromise->GetFuture();

	WrappedFuture.Next([WeakThis = TWeakPtr<FVesselSlateApprovalClient>(AsShared()),
	                    Promise](FVesselApprovalDecision Decision)
	{
		if (TSharedPtr<FVesselSlateApprovalClient> Pinned = WeakThis.Pin())
		{
			Pinned->bPending = false;
		}
		Promise->SetValue(MoveTemp(Decision));
	});

	OnApprovalRequested.Execute(Request, WrappedPromise);
	return Future;
}
