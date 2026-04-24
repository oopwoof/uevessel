// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/VesselSlateApprovalClient.h"

#include "VesselLog.h"
#include "Async/Async.h"

TFuture<FVesselApprovalDecision> FVesselSlateApprovalClient::RequestDecisionAsync(
	const FVesselApprovalRequest& Request)
{
	TSharedRef<TPromise<FVesselApprovalDecision>> Promise =
		MakeShared<TPromise<FVesselApprovalDecision>>();
	TFuture<FVesselApprovalDecision> Future = Promise->GetFuture();

	// Atomic CAS: set bPending true only if it's currently false.
	bool Expected = false;
	if (!bPending.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
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
		bPending.store(false, std::memory_order_release);
		Promise->SetValue(FVesselApprovalDecision::MakeReject(
			TEXT("No Vessel panel is open to approve this request. Open Window → Vessel Chat and try again."),
			TEXT("slate-client:auto")));
		return Future;
	}

	// Wrap the widget's promise so we can clear bPending when it resolves.
	TSharedRef<TPromise<FVesselApprovalDecision>> WrappedPromise =
		MakeShared<TPromise<FVesselApprovalDecision>>();
	TFuture<FVesselApprovalDecision> WrappedFuture = WrappedPromise->GetFuture();

	TWeakPtr<FVesselSlateApprovalClient> WeakThis = AsShared();
	WrappedFuture.Next([WeakThis, Promise](FVesselApprovalDecision Decision)
	{
		if (TSharedPtr<FVesselSlateApprovalClient> Pinned = WeakThis.Pin())
		{
			Pinned->bPending.store(false, std::memory_order_release);
		}
		Promise->SetValue(MoveTemp(Decision));
	});

	// Slate is not thread-safe. If we're already on the game thread, fire the
	// delegate synchronously (keeps unit tests deterministic). Otherwise hop.
	auto ExecuteOnUI = [WeakThis, Request, WrappedPromise]()
	{
		if (TSharedPtr<FVesselSlateApprovalClient> Pinned = WeakThis.Pin())
		{
			if (Pinned->OnApprovalRequested.IsBound())
			{
				Pinned->OnApprovalRequested.Execute(Request, WrappedPromise);
				return;
			}
		}
		// Panel / client gone between schedule and execute — fail-safe.
		WrappedPromise->SetValue(FVesselApprovalDecision::MakeReject(
			TEXT("UI gone before decision could be surfaced."),
			TEXT("slate-client:gone")));
	};

	if (IsInGameThread())
	{
		ExecuteOnUI();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(ExecuteOnUI));
	}
	return Future;
}
