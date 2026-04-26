// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * POD types shared across the Session Machine. Deliberately non-UObject so
 * they move cheaply, serialize trivially, and avoid GC churn during tight
 * per-step cycles. UObject reflection is not useful here — session state
 * lives in memory for the duration of one session and is persisted via the
 * structured JSONL log.
 *
 * See docs/engineering/SESSION_MACHINE.md §1 for the state diagram.
 */

/** The 8 FSM states. Matches SESSION_MACHINE.md §1 verbatim. */
enum class EVesselSessionState : uint8
{
	Idle,
	Planning,
	ToolSelection,
	Executing,
	JudgeReview,
	NextStep,
	Done,
	Failed,
};

/** How a session ultimately terminated. */
enum class EVesselSessionOutcomeKind : uint8
{
	Pending,
	Done,
	Failed,
	AbortedOnEditorClose,
	AbortedByUser,
};

/** The Judge's ternary verdict. */
enum class EVesselJudgeDecision : uint8
{
	Approve,
	Revise,
	Reject,
};

/** One step inside a plan (tool + args + reasoning). */
struct FVesselPlanStep
{
	FName ToolName;

	/** JSON object string (passes through FVesselJsonSanitizer before invoke). */
	FString ArgsJson;

	/** LLM-emitted rationale — surfaced in HITL panel and session log, not used by Invoker. */
	FString Reasoning;

	/** 1-based index within the parent plan. Stable even under revise (original index preserved). */
	int32 StepIndex = 0;

	/**
	 * True iff the user explicitly modified ArgsJson at the HITL approval gate
	 * (EditAndApprove kind). When true, OriginalPlannedArgs holds the LLM's
	 * pre-edit args. The Judge prompt uses these two signals to treat the
	 * edited values as the user's authoritative intent for that step rather
	 * than flagging mismatch with the original chat prompt.
	 */
	bool bUserEditedArgs = false;

	/** Pre-edit args snapshot. Populated only when bUserEditedArgs=true. */
	FString OriginalPlannedArgs;
};

/** A Planner output. */
struct FVesselPlan
{
	TArray<FVesselPlanStep> Steps;

	/** The raw LLM response, preserved verbatim for audit / replay. */
	FString RawLlmResponse;

	/** True iff Steps parsed cleanly and every tool is in the Registry. */
	bool bValid = false;

	/** When bValid=false, explain what failed in LLM-readable language. */
	FString ErrorMessage;
};

/** A Judge verdict for one executed step. */
struct FVesselJudgeVerdict
{
	EVesselJudgeDecision Decision = EVesselJudgeDecision::Reject;

	/** Why the judge made this call. Required; empty → judge call is invalid. */
	FString Reasoning;

	/** When Decision=Revise, concrete directive passed back to Planner. */
	FString ReviseDirective;

	/** When Decision=Reject, the reason that sinks to AGENTS.md Known Rejections. */
	FString RejectReason;
};

/** Terminal state description for a session. */
struct FVesselSessionOutcome
{
	EVesselSessionOutcomeKind Kind = EVesselSessionOutcomeKind::Pending;

	/** Short human-readable reason, e.g. "step budget exceeded" or "judge Reject: ...". */
	FString Reason;

	int32 StepsExecuted = 0;
	double TotalCostUsd = 0.0;
	int64 WallTimeMs = 0;

	FName LastStepTool;

	/** Final assistant text shown to the user (may be empty if terminated early). */
	FString FinalAssistantText;

	static FVesselSessionOutcome MakeDone(const FString& FinalText)
	{
		FVesselSessionOutcome O;
		O.Kind = EVesselSessionOutcomeKind::Done;
		O.FinalAssistantText = FinalText;
		return O;
	}
	static FVesselSessionOutcome MakeFailed(const FString& InReason)
	{
		FVesselSessionOutcome O;
		O.Kind = EVesselSessionOutcomeKind::Failed;
		O.Reason = InReason;
		return O;
	}
};

// --- String mapping helpers ---

VESSELEDITOR_API const TCHAR* SessionStateToString(EVesselSessionState State);
VESSELEDITOR_API const TCHAR* SessionOutcomeKindToString(EVesselSessionOutcomeKind Kind);
VESSELEDITOR_API const TCHAR* JudgeDecisionToString(EVesselJudgeDecision Decision);
