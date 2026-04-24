// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselApprovalClient.h"

const TCHAR* ApprovalDecisionKindToString(EVesselApprovalDecisionKind Kind)
{
	switch (Kind)
	{
		case EVesselApprovalDecisionKind::Approve:        return TEXT("Approve");
		case EVesselApprovalDecisionKind::Reject:         return TEXT("Reject");
		case EVesselApprovalDecisionKind::EditAndApprove: return TEXT("EditAndApprove");
	}
	return TEXT("Unknown");
}

// =============================================================================
// FVesselAutoApprovalClient
// =============================================================================

TFuture<FVesselApprovalDecision> FVesselAutoApprovalClient::RequestDecisionAsync(
	const FVesselApprovalRequest& /*Request*/)
{
	TPromise<FVesselApprovalDecision> Promise;
	TFuture<FVesselApprovalDecision> Future = Promise.GetFuture();
	Promise.SetValue(FVesselApprovalDecision::MakeApprove());
	return Future;
}

// =============================================================================
// FVesselAutoRejectClient
// =============================================================================

FVesselAutoRejectClient::FVesselAutoRejectClient(FString InReason)
	: Reason(MoveTemp(InReason))
{
}

TFuture<FVesselApprovalDecision> FVesselAutoRejectClient::RequestDecisionAsync(
	const FVesselApprovalRequest& /*Request*/)
{
	TPromise<FVesselApprovalDecision> Promise;
	TFuture<FVesselApprovalDecision> Future = Promise.GetFuture();
	Promise.SetValue(FVesselApprovalDecision::MakeReject(Reason));
	return Future;
}

// =============================================================================
// FVesselScriptedApprovalClient
// =============================================================================

FVesselScriptedApprovalClient::FVesselScriptedApprovalClient()
{
	// Conservative default: Reject with an actionable message.
	Default = FVesselApprovalDecision::MakeReject(
		TEXT("Scripted client has no mapping for this tool; set one explicitly."));
}

void FVesselScriptedApprovalClient::SetDecisionForTool(FName ToolName, const FVesselApprovalDecision& Decision)
{
	FScopeLock Guard(&Lock);
	Mapped.Add(ToolName, Decision);
}

void FVesselScriptedApprovalClient::SetDefault(const FVesselApprovalDecision& Decision)
{
	FScopeLock Guard(&Lock);
	Default = Decision;
}

int32 FVesselScriptedApprovalClient::GetRequestCount() const
{
	FScopeLock Guard(&Lock);
	return RequestCount;
}

TFuture<FVesselApprovalDecision> FVesselScriptedApprovalClient::RequestDecisionAsync(
	const FVesselApprovalRequest& Request)
{
	FVesselApprovalDecision Picked;
	{
		FScopeLock Guard(&Lock);
		++RequestCount;
		if (const FVesselApprovalDecision* Found = Mapped.Find(Request.ToolName))
		{
			Picked = *Found;
		}
		else
		{
			Picked = Default;
		}
	}
	TPromise<FVesselApprovalDecision> Promise;
	TFuture<FVesselApprovalDecision> Future = Promise.GetFuture();
	Promise.SetValue(MoveTemp(Picked));
	return Future;
}
