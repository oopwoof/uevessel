// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Session/VesselApprovalClient.h"

/**
 * IVesselApprovalClient implementation that defers the decision to a Slate
 * widget (typically SVesselChatPanel). The client owns the TPromise for the
 * pending request and exposes a delegate the widget subscribes to.
 *
 * Usage:
 *   auto Client = MakeShared<FVesselSlateApprovalClient>();
 *   Client->OnApprovalRequested.BindLambda(
 *       [WeakPanel](auto Req, auto Promise) { ... });
 *   Session->SetApprovalClient(Client);
 *
 * Semantics:
 *   - Only one request may be outstanding at a time. A second request while
 *     the first is unresolved is immediately auto-rejected with a clear reason.
 *   - If the delegate is unbound when a request arrives, auto-reject with
 *     "no UI attached" — fail loud rather than hang the session forever.
 *   - Must be used on the Game Thread; the widget fulfills decisions synchronously.
 */
class VESSELEDITOR_API FVesselSlateApprovalClient
	: public IVesselApprovalClient
	, public TSharedFromThis<FVesselSlateApprovalClient>
{
public:
	DECLARE_DELEGATE_TwoParams(
		FOnApprovalRequested,
		const FVesselApprovalRequest& /*Request*/,
		TSharedRef<TPromise<FVesselApprovalDecision>> /*Promise*/);

	/** Widget binds this; receives the request and the promise to fulfill. */
	FOnApprovalRequested OnApprovalRequested;

	virtual TFuture<FVesselApprovalDecision> RequestDecisionAsync(
		const FVesselApprovalRequest& Request) override;

	/** True while a request is awaiting a decision. */
	bool HasPending() const { return bPending; }

private:
	bool bPending = false;
};
