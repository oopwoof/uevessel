// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Types shared between the Session Machine and any ApprovalClient.
 *
 * The flow is: Session builds an FVesselApprovalRequest, hands it to an
 * IVesselApprovalClient, awaits an FVesselApprovalDecision, and reacts
 * per the documented HITL protocol. See HITL_PROTOCOL.md §2.2.
 */

enum class EVesselApprovalDecisionKind : uint8
{
	Approve,          // execute tool as planned
	Reject,           // abandon this step (and, at FSM level, fail the session)
	EditAndApprove,   // user modified args; execute with RevisedArgsJson
};

/** One pending approval request. Populated by SessionMachine, consumed by ApprovalClient. */
struct FVesselApprovalRequest
{
	FString SessionId;

	/** 1-based index into the current plan. */
	int32 StepIndex = 0;

	FName ToolName;

	/** Category string from the tool's schema (DataTable / DataTable/Write / Asset / ...). */
	FString ToolCategory;

	/** JSON args the agent proposed. */
	FString ArgsJson;

	/** LLM-emitted reasoning for this step, surfaced to the user. */
	FString Reasoning;

	/** Mirrors the tool schema — UI uses this to harden the warning. */
	bool bIrreversibleHint = false;
};

/** The user's (or auto-client's) decision on an approval request. */
struct FVesselApprovalDecision
{
	EVesselApprovalDecisionKind Kind = EVesselApprovalDecisionKind::Reject;

	/** Required when Kind=Reject. Sinks to AGENTS.md ## Known Rejections. */
	FString RejectReason;

	/** Required when Kind=EditAndApprove. Replaces the step's ArgsJson. */
	FString RevisedArgsJson;

	/** Free-form label, e.g. "auto", "user:<name>". Appears in session log + rejection sink. */
	FString DeciderId;

	static FVesselApprovalDecision MakeApprove(const FString& Decider = TEXT("auto"))
	{
		FVesselApprovalDecision D;
		D.Kind = EVesselApprovalDecisionKind::Approve;
		D.DeciderId = Decider;
		return D;
	}
	static FVesselApprovalDecision MakeReject(const FString& Reason, const FString& Decider = TEXT("auto"))
	{
		FVesselApprovalDecision D;
		D.Kind = EVesselApprovalDecisionKind::Reject;
		D.RejectReason = Reason;
		D.DeciderId = Decider;
		return D;
	}
	static FVesselApprovalDecision MakeEdit(const FString& NewArgsJson, const FString& Decider = TEXT("auto"))
	{
		FVesselApprovalDecision D;
		D.Kind = EVesselApprovalDecisionKind::EditAndApprove;
		D.RevisedArgsJson = NewArgsJson;
		D.DeciderId = Decider;
		return D;
	}
};

VESSELEDITOR_API const TCHAR* ApprovalDecisionKindToString(EVesselApprovalDecisionKind Kind);
