// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Delegates/IDelegateInstance.h"

#include "Session/VesselApprovalTypes.h"
#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionTypes.h"

class ILlmProvider;
class IVesselApprovalClient;
class FVesselSessionLog;
struct FLlmResponse;

/**
 * The agent session FSM. Runs Planner → Executor → Judge loops, enforces
 * budgets, writes structured JSONL log, and resolves a TFuture<Outcome>
 * when it reaches Done / Failed / Aborted.
 *
 * Thread affinity: all public methods must be called on the Game Thread.
 * Async LLM callbacks are hopped back to the Game Thread via AsyncTask
 * before driving state transitions.
 *
 * Lifecycle:
 *   FVesselSessionMachine M;
 *   M.Init(Config);                 // loads provider, opens log, registers editor hook
 *   auto Future = M.RunAsync("...");// transitions Idle → Planning immediately
 *   // wait / observe / abort
 *   const auto Outcome = Future.Get();
 *
 * See docs/engineering/SESSION_MACHINE.md for the state diagram and budget
 * semantics. Single-session object: one RunAsync per instance.
 */
class VESSELEDITOR_API FVesselSessionMachine : public TSharedFromThis<FVesselSessionMachine>
{
public:
	FVesselSessionMachine();
	~FVesselSessionMachine();

	/** Resolve provider, open session log, register editor-close hook. */
	bool Init(const FVesselSessionConfig& InConfig);

	/**
	 * Kick off the session with the initial user input. Returns a future
	 * that resolves when the session reaches a terminal state. Calling more
	 * than once per instance returns a pre-completed error future.
	 */
	TFuture<FVesselSessionOutcome> RunAsync(const FString& UserInput);

	/** Request cooperative abort; takes effect at the next state boundary. */
	void RequestAbort(const FString& Reason);

	/** Current FSM state, for observers / tests. Reads game-thread state without locking. */
	EVesselSessionState GetCurrentState() const { return CurrentState; }

	/** Session id (stable across the run). */
	const FString& GetSessionId() const { return Config.SessionId; }

	/** Path to the JSONL log file for this session. */
	FString GetLogFilePath() const;

	/** Plan as of the latest planning turn (empty before Planning completes). */
	const FVesselPlan& GetCurrentPlan() const { return CurrentPlan; }

	/**
	 * Install the client that fulfills HITL approval requests. Must be set
	 * before RunAsync is called. If left null, the session defaults to
	 * FVesselAutoApprovalClient — safe for mock-driven automated tests, but
	 * never for interactive use.
	 */
	void SetApprovalClient(TSharedRef<IVesselApprovalClient> InClient);

	/**
	 * Pure predicate: does a step that resolves to this tool schema require
	 * a HITL approval round-trip? Triggers per HITL_PROTOCOL.md §1.1:
	 * RequiresApproval, IrreversibleHint, or Category contains "Write".
	 *
	 * Public so callers (UI previews, tests) can ask without touching a live
	 * machine. Pure schema-level — no FSM state read or written.
	 */
	static bool StepNeedsApproval(const struct FVesselToolSchema& Schema);

	// Non-copyable, non-movable — holds file handles + delegate registrations.
	FVesselSessionMachine(const FVesselSessionMachine&) = delete;
	FVesselSessionMachine& operator=(const FVesselSessionMachine&) = delete;
	FVesselSessionMachine(FVesselSessionMachine&&) = delete;
	FVesselSessionMachine& operator=(FVesselSessionMachine&&) = delete;

private:
	// --- State transitions ---
	void EnterPlanning(const FString& ReviseDirective);
	void HandlePlanningComplete(const FLlmResponse& Response);
	void EnterToolSelection();
	void EnterExecuting();

	/** HITL gate helpers (see HITL_PROTOCOL.md §1). Predicate is public above. */
	void RequestApprovalForStep(const FVesselPlanStep& Step, const FVesselToolSchema& Schema);
	void HandleApprovalDecision(FVesselPlanStep Step, FVesselApprovalDecision Decision);
	void InvokeStep(const FVesselPlanStep& Step);

	void HandleStepResult(const FName& Tool, const FString& ResultJson,
		bool bWasError, const FString& ErrorMessage);
	void EnterJudgeReview(const FString& ToolResultJson);
	void HandleJudgeComplete(const FLlmResponse& Response);
	void EnterNextStep();
	void EnterDone(const FString& FinalText);
	void EnterFailed(const FString& Reason, EVesselSessionOutcomeKind Kind = EVesselSessionOutcomeKind::Failed);

	// --- Budget / abort ---
	bool CheckBudgets(FString& OutReason) const;
	int32 BumpErrorCount(const FName& Tool, const FString& ErrorCode);

	// --- Logging helpers ---
	void LogStateTransition(EVesselSessionState From, EVesselSessionState To);
	void LogPlanning(const FVesselPlan& Plan);
	void LogStepExecuted(const FVesselPlanStep& Step, const FString& ResultJson,
		bool bWasError, const FString& ErrorMessage);
	void LogJudgeVerdict(const FVesselJudgeVerdict& Verdict);
	void LogApprovalDecision(const FVesselApprovalRequest& Request,
		const FVesselApprovalDecision& Decision);
	void LogSessionSummary(const FVesselSessionOutcome& Outcome);

	// --- Async dispatch helper ---
	void DispatchOnGameThread(TFunction<void()> Work);

	// --- Editor close hook ---
	void OnEditorClosingHook();

private:
	FVesselSessionConfig Config;
	EVesselSessionState CurrentState = EVesselSessionState::Idle;

	TPromise<FVesselSessionOutcome> OutcomePromise;
	bool bPromiseSet = false;
	bool bRunInvoked = false;

	FString UserInput;
	FVesselPlan CurrentPlan;
	int32 CurrentStepIndex = 0;      // 0-based into CurrentPlan.Steps
	int32 StepsExecuted = 0;
	int32 ConsecutiveReviseCount = 0;
	TMap<FString, int32> ErrorCounts; // key = "Tool:ErrorCode"

	double TotalCostUsd = 0.0;
	FDateTime StartedAt;

	TSharedPtr<ILlmProvider> Provider;
	TSharedPtr<IVesselApprovalClient> ApprovalClient;
	TUniquePtr<FVesselSessionLog> Log;

	FString AbortReason;
	bool bAbortRequested = false;

	FDelegateHandle EditorCloseHandle;
};
