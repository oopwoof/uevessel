// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselSessionMachine.h"

#include "VesselLog.h"

#include "Llm/ILlmProvider.h"
#include "Llm/LlmProviderRegistry.h"
#include "Llm/VesselLlmTypes.h"

#include "Registry/VesselResult.h"
#include "Registry/VesselToolInvoker.h"
#include "Registry/VesselToolRegistry.h"

#include "Session/VesselPlannerPrompts.h"
#include "Session/VesselSessionLog.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace VesselSessionDetail
{
	static FString FormatErrorKey(const FName& Tool, const FString& Code)
	{
		return FString::Printf(TEXT("%s:%s"), *Tool.ToString(), *Code);
	}
}

FVesselSessionMachine::FVesselSessionMachine() = default;

FVesselSessionMachine::~FVesselSessionMachine()
{
#if WITH_EDITOR
	if (EditorCloseHandle.IsValid())
	{
		FEditorDelegates::OnEditorClose.Remove(EditorCloseHandle);
		EditorCloseHandle.Reset();
	}
#endif
	if (Log)
	{
		Log->Close();
	}
}

bool FVesselSessionMachine::Init(const FVesselSessionConfig& InConfig)
{
	checkf(IsInGameThread(), TEXT("FVesselSessionMachine must be Init'd on the Game Thread"));

	Config = InConfig;
	if (Config.SessionId.IsEmpty())
	{
		Config.SessionId = GenerateSessionId();
	}

	Provider = FLlmProviderRegistry::Get().FindProvider(Config.ProviderId);
	if (!Provider.IsValid())
	{
		UE_LOG(LogVesselSession, Error,
			TEXT("Init: provider '%s' not registered. Known providers: %s"),
			*Config.ProviderId,
			*FString::Join(FLlmProviderRegistry::Get().ListProviderIds(), TEXT(", ")));
		return false;
	}

	Log = MakeUnique<FVesselSessionLog>();
	if (!Log->Open(Config.SessionId))
	{
		UE_LOG(LogVesselSession, Error,
			TEXT("Init: could not open session log for '%s'"), *Config.SessionId);
		Log.Reset();
		return false;
	}

	TSharedRef<FJsonObject> Hdr = MakeShared<FJsonObject>();
	Hdr->SetStringField(TEXT("provider"),      Config.ProviderId);
	Hdr->SetStringField(TEXT("planner_model"), Config.PlannerModel);
	Hdr->SetStringField(TEXT("judge_model"),   Config.JudgeModel);
	Hdr->SetStringField(TEXT("agent"),         Config.AgentTemplate.Name);
	Hdr->SetNumberField(TEXT("max_steps"),     Config.Budget.MaxSteps);
	Hdr->SetNumberField(TEXT("max_cost_usd"),  Config.Budget.MaxCostUsd);
	Hdr->SetNumberField(TEXT("max_wall_sec"),  Config.Budget.MaxWallTimeSec);
	Log->AppendRecord(TEXT("SessionOpen"), Hdr);

#if WITH_EDITOR
	EditorCloseHandle = FEditorDelegates::OnEditorClose.AddLambda([WeakThis = AsWeak()]()
	{
		if (TSharedPtr<FVesselSessionMachine> Pinned = WeakThis.Pin())
		{
			Pinned->OnEditorClosingHook();
		}
	});
#endif

	return true;
}

FString FVesselSessionMachine::GetLogFilePath() const
{
	return Log ? Log->GetFilePath() : FString();
}

TFuture<FVesselSessionOutcome> FVesselSessionMachine::RunAsync(const FString& InUserInput)
{
	checkf(IsInGameThread(), TEXT("FVesselSessionMachine::RunAsync must be called on the Game Thread"));

	if (bRunInvoked)
	{
		TPromise<FVesselSessionOutcome> P;
		TFuture<FVesselSessionOutcome> F = P.GetFuture();
		P.SetValue(FVesselSessionOutcome::MakeFailed(
			TEXT("RunAsync called twice on the same session instance.")));
		return F;
	}
	bRunInvoked = true;

	if (!Provider.IsValid() || !Log)
	{
		TPromise<FVesselSessionOutcome> P;
		TFuture<FVesselSessionOutcome> F = P.GetFuture();
		P.SetValue(FVesselSessionOutcome::MakeFailed(TEXT("Session not initialized correctly.")));
		return F;
	}

	UserInput = InUserInput;
	StartedAt = FDateTime::UtcNow();

	TFuture<FVesselSessionOutcome> Future = OutcomePromise.GetFuture();
	EnterPlanning(/*ReviseDirective=*/FString());
	return Future;
}

void FVesselSessionMachine::RequestAbort(const FString& InReason)
{
	bAbortRequested = true;
	AbortReason = InReason;
}

// =========================================================================
// Budgets
// =========================================================================

bool FVesselSessionMachine::CheckBudgets(FString& OutReason) const
{
	if (StepsExecuted >= Config.Budget.MaxSteps)
	{
		OutReason = FString::Printf(TEXT("Step budget exceeded (%d/%d)."),
			StepsExecuted, Config.Budget.MaxSteps);
		return false;
	}
	if (TotalCostUsd >= Config.Budget.MaxCostUsd)
	{
		OutReason = FString::Printf(TEXT("Cost budget exceeded ($%.2f/$%.2f)."),
			TotalCostUsd, Config.Budget.MaxCostUsd);
		return false;
	}
	const int64 ElapsedSec = (FDateTime::UtcNow() - StartedAt).GetTotalSeconds();
	if (ElapsedSec >= Config.Budget.MaxWallTimeSec)
	{
		OutReason = FString::Printf(TEXT("Wall time budget exceeded (%llds/%ds)."),
			ElapsedSec, Config.Budget.MaxWallTimeSec);
		return false;
	}
	if (ConsecutiveReviseCount >= Config.Budget.MaxConsecutiveRevise)
	{
		OutReason = FString::Printf(TEXT("Consecutive revise limit hit (%d)."),
			Config.Budget.MaxConsecutiveRevise);
		return false;
	}
	return true;
}

int32 FVesselSessionMachine::BumpErrorCount(const FName& Tool, const FString& Code)
{
	const FString Key = VesselSessionDetail::FormatErrorKey(Tool, Code);
	int32& N = ErrorCounts.FindOrAdd(Key, 0);
	return ++N;
}

// =========================================================================
// Async dispatch helper
// =========================================================================

void FVesselSessionMachine::DispatchOnGameThread(TFunction<void()> Work)
{
	if (IsInGameThread())
	{
		Work();
		return;
	}
	TWeakPtr<FVesselSessionMachine> Weak = AsWeak();
	AsyncTask(ENamedThreads::GameThread, [Weak, Work = MoveTemp(Work)]()
	{
		if (TSharedPtr<FVesselSessionMachine> Pinned = Weak.Pin())
		{
			Work();
		}
	});
}

// =========================================================================
// State entry / transition methods
// =========================================================================

void FVesselSessionMachine::LogStateTransition(EVesselSessionState From, EVesselSessionState To)
{
	UE_LOG(LogVesselSession, Verbose, TEXT("Session %s: %s -> %s"),
		*Config.SessionId,
		SessionStateToString(From),
		SessionStateToString(To));

	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("from"), SessionStateToString(From));
	P->SetStringField(TEXT("to"),   SessionStateToString(To));
	if (Log) Log->AppendRecord(TEXT("StateTransition"), P);
}

void FVesselSessionMachine::EnterPlanning(const FString& ReviseDirective)
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::Planning;
	LogStateTransition(Prev, CurrentState);

	FString Reason;
	if (!CheckBudgets(Reason)) { EnterFailed(Reason); return; }
	if (bAbortRequested)      { EnterFailed(AbortReason.IsEmpty() ? TEXT("Aborted") : AbortReason,
									EVesselSessionOutcomeKind::AbortedByUser); return; }

	const TArray<FVesselToolSchema> Tools = FVesselToolRegistry::Get().GetAllSchemas();
	const FLlmRequest Request = FVesselPlannerPrompts::BuildPlanningRequest(
		Config, UserInput, Tools, ReviseDirective);

	TWeakPtr<FVesselSessionMachine> Weak = AsWeak();
	Provider->SendAsync(Request).Next([Weak](FLlmResponse Response)
	{
		if (TSharedPtr<FVesselSessionMachine> Pinned = Weak.Pin())
		{
			Pinned->DispatchOnGameThread([Pinned, Resp = MoveTemp(Response)]() mutable
			{
				Pinned->HandlePlanningComplete(Resp);
			});
		}
	});
}

void FVesselSessionMachine::HandlePlanningComplete(const FLlmResponse& Response)
{
	TotalCostUsd += Response.Usage.EstimatedCostUsd;
	CurrentPlan = FVesselPlannerPrompts::ParsePlanResponse(Response);
	LogPlanning(CurrentPlan);

	if (!CurrentPlan.bValid)
	{
		EnterFailed(FString::Printf(TEXT("Planner output invalid: %s"), *CurrentPlan.ErrorMessage));
		return;
	}
	if (CurrentPlan.Steps.Num() == 0)
	{
		// Valid, but empty — treat as a completed "no-op" session.
		EnterDone(TEXT("Planner returned an empty plan — nothing to execute."));
		return;
	}
	CurrentStepIndex = 0;
	EnterToolSelection();
}

void FVesselSessionMachine::LogPlanning(const FVesselPlan& Plan)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetBoolField(TEXT("valid"), Plan.bValid);
	P->SetNumberField(TEXT("step_count"), Plan.Steps.Num());
	if (!Plan.ErrorMessage.IsEmpty())
	{
		P->SetStringField(TEXT("error"), Plan.ErrorMessage);
	}
	P->SetStringField(TEXT("raw_response"), Plan.RawLlmResponse);
	if (Log) Log->AppendRecord(TEXT("PlanningResult"), P);
}

void FVesselSessionMachine::EnterToolSelection()
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::ToolSelection;
	LogStateTransition(Prev, CurrentState);

	FString Reason;
	if (!CheckBudgets(Reason)) { EnterFailed(Reason); return; }
	if (bAbortRequested) { EnterFailed(AbortReason, EVesselSessionOutcomeKind::AbortedByUser); return; }

	if (!CurrentPlan.Steps.IsValidIndex(CurrentStepIndex))
	{
		EnterDone(TEXT("All plan steps approved. Session complete."));
		return;
	}

	const FVesselPlanStep& Step = CurrentPlan.Steps[CurrentStepIndex];
	if (!FVesselToolRegistry::Get().FindSchema(Step.ToolName))
	{
		EnterFailed(FString::Printf(TEXT("Step %d references unknown tool '%s'."),
			Step.StepIndex, *Step.ToolName.ToString()));
		return;
	}

	EnterExecuting();
}

void FVesselSessionMachine::EnterExecuting()
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::Executing;
	LogStateTransition(Prev, CurrentState);

	const FVesselPlanStep& Step = CurrentPlan.Steps[CurrentStepIndex];

	FVesselToolInvoker::FInvokeOptions Options;
	Options.SessionId = Config.SessionId;

	const FVesselResult<FString> Result = FVesselToolInvoker::Invoke(
		Step.ToolName, Step.ArgsJson, Options);

	StepsExecuted++;

	if (!Result.bOk)
	{
		const FString Code = FString(VesselResultCodeToString(Result.Code));
		BumpErrorCount(Step.ToolName, Code);
		const int32 Count = ErrorCounts.FindRef(VesselSessionDetail::FormatErrorKey(Step.ToolName, Code));

		LogStepExecuted(Step, Result.Value, /*bWasError=*/true, Result.Message);

		if (Count >= Config.Budget.RepeatErrorLimit)
		{
			EnterFailed(FString::Printf(
				TEXT("Circuit-breaker: tool '%s' returned %s %d times."),
				*Step.ToolName.ToString(), *Code, Count));
			return;
		}

		// Treat tool errors as Revise: re-plan with the error message as directive.
		ConsecutiveReviseCount++;
		EnterPlanning(FString::Printf(
			TEXT("Previous step '%s' failed with %s: %s. Revise your plan to avoid this error."),
			*Step.ToolName.ToString(), *Code, *Result.Message));
		return;
	}

	LogStepExecuted(Step, Result.Value, /*bWasError=*/false, FString());
	EnterJudgeReview(Result.Value);
}

void FVesselSessionMachine::HandleStepResult(
	const FName& /*Tool*/, const FString& /*ResultJson*/,
	bool /*bWasError*/, const FString& /*ErrorMessage*/)
{
	// Retained hook for future async tool paths (Step 3c+). Currently unused:
	// tools are invoked synchronously inside EnterExecuting.
}

void FVesselSessionMachine::LogStepExecuted(
	const FVesselPlanStep& Step, const FString& ResultJson,
	bool bWasError, const FString& ErrorMessage)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("tool"),        Step.ToolName.ToString());
	P->SetStringField(TEXT("args_json"),   Step.ArgsJson);
	P->SetNumberField(TEXT("step_index"),  Step.StepIndex);
	P->SetBoolField(  TEXT("error"),       bWasError);
	if (bWasError)
	{
		P->SetStringField(TEXT("error_message"), ErrorMessage);
	}
	else
	{
		P->SetStringField(TEXT("result_json"), ResultJson);
	}
	if (Log) Log->AppendRecord(TEXT("StepExecuted"), P);
}

void FVesselSessionMachine::EnterJudgeReview(const FString& ToolResultJson)
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::JudgeReview;
	LogStateTransition(Prev, CurrentState);

	FString Reason;
	if (!CheckBudgets(Reason)) { EnterFailed(Reason); return; }
	if (bAbortRequested) { EnterFailed(AbortReason, EVesselSessionOutcomeKind::AbortedByUser); return; }

	const FVesselPlanStep& Step = CurrentPlan.Steps[CurrentStepIndex];
	const FLlmRequest Request = FVesselPlannerPrompts::BuildJudgeRequest(Config, Step, ToolResultJson);

	TWeakPtr<FVesselSessionMachine> Weak = AsWeak();
	Provider->SendAsync(Request).Next([Weak](FLlmResponse Response)
	{
		if (TSharedPtr<FVesselSessionMachine> Pinned = Weak.Pin())
		{
			Pinned->DispatchOnGameThread([Pinned, Resp = MoveTemp(Response)]() mutable
			{
				Pinned->HandleJudgeComplete(Resp);
			});
		}
	});
}

void FVesselSessionMachine::HandleJudgeComplete(const FLlmResponse& Response)
{
	TotalCostUsd += Response.Usage.EstimatedCostUsd;
	const FVesselJudgeVerdict Verdict = FVesselPlannerPrompts::ParseJudgeResponse(Response);
	LogJudgeVerdict(Verdict);

	switch (Verdict.Decision)
	{
		case EVesselJudgeDecision::Approve:
			ConsecutiveReviseCount = 0;
			EnterNextStep();
			return;

		case EVesselJudgeDecision::Revise:
			ConsecutiveReviseCount++;
			EnterPlanning(Verdict.ReviseDirective.IsEmpty()
				? FString::Printf(TEXT("Judge requested revision: %s"), *Verdict.Reasoning)
				: Verdict.ReviseDirective);
			return;

		case EVesselJudgeDecision::Reject:
		default:
			EnterFailed(FString::Printf(TEXT("Judge rejected: %s"),
				Verdict.RejectReason.IsEmpty() ? *Verdict.Reasoning : *Verdict.RejectReason));
			return;
	}
}

void FVesselSessionMachine::LogJudgeVerdict(const FVesselJudgeVerdict& Verdict)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("decision"),  JudgeDecisionToString(Verdict.Decision));
	P->SetStringField(TEXT("reasoning"), Verdict.Reasoning);
	if (!Verdict.ReviseDirective.IsEmpty()) P->SetStringField(TEXT("revise_directive"), Verdict.ReviseDirective);
	if (!Verdict.RejectReason.IsEmpty())    P->SetStringField(TEXT("reject_reason"),    Verdict.RejectReason);
	if (Log) Log->AppendRecord(TEXT("JudgeVerdict"), P);
}

void FVesselSessionMachine::EnterNextStep()
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::NextStep;
	LogStateTransition(Prev, CurrentState);

	CurrentStepIndex++;
	EnterToolSelection();
}

void FVesselSessionMachine::EnterDone(const FString& FinalText)
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::Done;
	LogStateTransition(Prev, CurrentState);

	FVesselSessionOutcome Outcome = FVesselSessionOutcome::MakeDone(FinalText);
	Outcome.StepsExecuted = StepsExecuted;
	Outcome.TotalCostUsd  = TotalCostUsd;
	Outcome.WallTimeMs    = (FDateTime::UtcNow() - StartedAt).GetTotalMilliseconds();
	if (CurrentPlan.Steps.IsValidIndex(CurrentStepIndex - 1))
	{
		Outcome.LastStepTool = CurrentPlan.Steps[CurrentStepIndex - 1].ToolName;
	}

	LogSessionSummary(Outcome);

	if (!bPromiseSet)
	{
		bPromiseSet = true;
		OutcomePromise.SetValue(MoveTemp(Outcome));
	}
}

void FVesselSessionMachine::EnterFailed(const FString& Reason, EVesselSessionOutcomeKind Kind)
{
	const EVesselSessionState Prev = CurrentState;
	CurrentState = EVesselSessionState::Failed;
	LogStateTransition(Prev, CurrentState);

	FVesselSessionOutcome Outcome;
	Outcome.Kind          = Kind;
	Outcome.Reason        = Reason;
	Outcome.StepsExecuted = StepsExecuted;
	Outcome.TotalCostUsd  = TotalCostUsd;
	Outcome.WallTimeMs    = (FDateTime::UtcNow() - StartedAt).GetTotalMilliseconds();
	if (CurrentPlan.Steps.IsValidIndex(CurrentStepIndex))
	{
		Outcome.LastStepTool = CurrentPlan.Steps[CurrentStepIndex].ToolName;
	}

	LogSessionSummary(Outcome);

	if (!bPromiseSet)
	{
		bPromiseSet = true;
		OutcomePromise.SetValue(MoveTemp(Outcome));
	}
}

void FVesselSessionMachine::LogSessionSummary(const FVesselSessionOutcome& Outcome)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("outcome"),   SessionOutcomeKindToString(Outcome.Kind));
	P->SetStringField(TEXT("reason"),    Outcome.Reason);
	P->SetNumberField(TEXT("steps"),     Outcome.StepsExecuted);
	P->SetNumberField(TEXT("cost_usd"),  Outcome.TotalCostUsd);
	P->SetNumberField(TEXT("wall_ms"),   static_cast<double>(Outcome.WallTimeMs));
	if (Log) Log->AppendRecord(TEXT("SessionSummary"), P);
}

// =========================================================================
// Editor close hook (Gemini review item)
// =========================================================================

void FVesselSessionMachine::OnEditorClosingHook()
{
	if (bPromiseSet)
	{
		return;
	}
	UE_LOG(LogVesselSession, Warning,
		TEXT("Session %s aborting due to editor close"), *Config.SessionId);
	EnterFailed(TEXT("Editor is closing."), EVesselSessionOutcomeKind::AbortedOnEditorClose);
}
