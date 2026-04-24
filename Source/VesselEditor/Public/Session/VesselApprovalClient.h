// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Session/VesselApprovalTypes.h"

/**
 * Abstracts the surface that renders an approval request and returns the
 * user's (or auto) decision asynchronously.
 *
 * Implementations in v0.1:
 *   - FVesselAutoApprovalClient  — blanket Approve (for tests / CI / batch mode)
 *   - FVesselAutoRejectClient    — blanket Reject (for failure-path tests)
 *   - FVesselScriptedClient      — deterministic per-request scripted response (for tests)
 *
 * The production Slate Dock Panel ships its own impl in Step 4c.
 */
class VESSELEDITOR_API IVesselApprovalClient
{
public:
	virtual ~IVesselApprovalClient() = default;

	/**
	 * Requests a decision. Implementations must resolve the future on the
	 * Game Thread (the Session Machine expects it).
	 */
	virtual TFuture<FVesselApprovalDecision> RequestDecisionAsync(
		const FVesselApprovalRequest& Request) = 0;
};

/** Always Approve. Useful for CI / automation tests. */
class VESSELEDITOR_API FVesselAutoApprovalClient : public IVesselApprovalClient
{
public:
	virtual TFuture<FVesselApprovalDecision> RequestDecisionAsync(
		const FVesselApprovalRequest& Request) override;
};

/** Always Reject with a configured reason. */
class VESSELEDITOR_API FVesselAutoRejectClient : public IVesselApprovalClient
{
public:
	explicit FVesselAutoRejectClient(FString InReason = TEXT("auto-reject (test)"));
	virtual TFuture<FVesselApprovalDecision> RequestDecisionAsync(
		const FVesselApprovalRequest& Request) override;

private:
	FString Reason;
};

/**
 * Scripted client for precise test control: caller maps tool name to a
 * pre-built decision; any unmapped tool falls back to DefaultDecision.
 */
class VESSELEDITOR_API FVesselScriptedApprovalClient : public IVesselApprovalClient
{
public:
	FVesselScriptedApprovalClient();

	void SetDecisionForTool(FName ToolName, const FVesselApprovalDecision& Decision);
	void SetDefault(const FVesselApprovalDecision& Decision);
	int32 GetRequestCount() const;

	virtual TFuture<FVesselApprovalDecision> RequestDecisionAsync(
		const FVesselApprovalRequest& Request) override;

private:
	mutable FCriticalSection Lock;
	TMap<FName, FVesselApprovalDecision> Mapped;
	FVesselApprovalDecision Default;
	int32 RequestCount = 0;
};
