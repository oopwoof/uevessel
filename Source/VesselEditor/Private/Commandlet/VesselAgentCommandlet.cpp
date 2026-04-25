// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Commandlet/VesselAgentCommandlet.h"

#include "VesselLog.h"
#include "Registry/VesselToolRegistry.h"
#include "Session/VesselAgentTemplates.h"
#include "Session/VesselApprovalClient.h"
#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"
#include "Session/VesselSessionTypes.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VesselAgentCommandletDetail
{
	static constexpr int32 kExitDone        = 0;
	static constexpr int32 kExitFailed      = 1;
	static constexpr int32 kExitBadArgs     = 2;

	static constexpr float kDefaultTimeoutSec = 1800.0f; // 30 min

	/** Pick the approval client based on -policy=approve|reject. */
	static TSharedRef<IVesselApprovalClient> MakePolicyClient(const FString& Policy)
	{
		if (Policy.Equals(TEXT("reject"), ESearchCase::IgnoreCase))
		{
			return MakeShared<FVesselAutoRejectClient>();
		}
		// Default is approve — caller validated the policy string already.
		return MakeShared<FVesselAutoApprovalClient>();
	}

	/**
	 * Pump the game thread until the future resolves, capped by Timeout.
	 * Returns true if the future resolved within the budget; false on
	 * timeout. Required because RunAsync's continuation hops back to the
	 * Game Thread via AsyncTask — a naive Future.Get() would deadlock.
	 */
	static bool WaitFutureWithTicking(
		TFuture<FVesselSessionOutcome>& Fut, float TimeoutSec)
	{
		const double Deadline = FPlatformTime::Seconds() + TimeoutSec;
		while (!Fut.IsReady())
		{
			FTSTicker::GetCoreTicker().Tick(0.01f);
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			if (FPlatformTime::Seconds() > Deadline)
			{
				return false;
			}
			FPlatformProcess::Sleep(0.01f);
		}
		return true;
	}

	static void EmitSummaryJson(
		const FVesselSessionOutcome& Outcome,
		const FString& SessionLogPath)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("outcome"),     SessionOutcomeKindToString(Outcome.Kind));
		Obj->SetStringField(TEXT("reason"),      Outcome.Reason);
		Obj->SetNumberField(TEXT("steps_executed"), Outcome.StepsExecuted);
		Obj->SetNumberField(TEXT("cost_usd_est"),   Outcome.TotalCostUsd);
		Obj->SetNumberField(TEXT("wall_ms"),     static_cast<double>(Outcome.WallTimeMs));
		Obj->SetStringField(TEXT("session_log"), SessionLogPath);
		if (!Outcome.LastStepTool.IsNone())
		{
			Obj->SetStringField(TEXT("last_step_tool"), Outcome.LastStepTool.ToString());
		}
		if (!Outcome.FinalAssistantText.IsEmpty())
		{
			Obj->SetStringField(TEXT("final_text"), Outcome.FinalAssistantText);
		}

		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		// Print to stdout (UE wraps via UE_LOG), but also FPlatformMisc::LocalPrint
		// keeps the line clean for CI parsers.
		FPlatformMisc::LocalPrint(*Out);
		FPlatformMisc::LocalPrint(TEXT("\n"));
	}
}

UVesselAgentCommandlet::UVesselAgentCommandlet()
{
	IsClient    = false;
	IsEditor    = true;
	IsServer    = false;
	LogToConsole = true;
}

int32 UVesselAgentCommandlet::Main(const FString& Params)
{
	using namespace VesselAgentCommandletDetail;

	// ---- Parse args -------------------------------------------------------
	// FParse::Value handles `-key=value` and quoted forms `-key="a b c"` —
	// matches CI engineers' muscle memory for UE commandlets.
	FString Prompt, Agent, Policy, TimeoutS;
	FParse::Value(*Params, TEXT("prompt="),  Prompt);
	FParse::Value(*Params, TEXT("agent="),   Agent);
	FParse::Value(*Params, TEXT("policy="),  Policy);
	FParse::Value(*Params, TEXT("timeout="), TimeoutS);

	if (Prompt.IsEmpty())
	{
		UE_LOG(LogVessel, Error, TEXT(
			"VesselAgent: -prompt is required. "
			"Example: -prompt=\"Validate /Game/Maps\" -agent=asset-pipeline -policy=approve"));
		return kExitBadArgs;
	}
	if (!Policy.IsEmpty()
		&& !Policy.Equals(TEXT("approve"), ESearchCase::IgnoreCase)
		&& !Policy.Equals(TEXT("reject"),  ESearchCase::IgnoreCase))
	{
		UE_LOG(LogVessel, Error, TEXT(
			"VesselAgent: invalid -policy '%s' (must be approve | reject)."),
			*Policy);
		return kExitBadArgs;
	}

	float TimeoutSec = kDefaultTimeoutSec;
	if (!TimeoutS.IsEmpty())
	{
		TimeoutSec = FCString::Atof(*TimeoutS);
		if (TimeoutSec <= 0.0f) { TimeoutSec = kDefaultTimeoutSec; }
	}

	// ---- Ensure registry populated ---------------------------------------
	// PostEngineInit may not have populated the registry in commandlet context
	// (depends on phase ordering). Force-scan defensively — ScanAll is idempotent.
	FVesselToolRegistry::Get().ScanAll();
	UE_LOG(LogVessel, Log, TEXT("VesselAgent: registry has %d tool(s)."),
		FVesselToolRegistry::Get().Num());

	// ---- Build config ----------------------------------------------------
	FVesselSessionConfig Config = MakeDefaultSessionConfig(FString());
	const FString ResolvedAgent = Agent.IsEmpty() ? TEXT("designer-assistant") : Agent;
	Config.AgentTemplate = FVesselAgentTemplates::FindByName(ResolvedAgent);

	UE_LOG(LogVessel, Log, TEXT(
		"VesselAgent: agent='%s'  provider='%s'  policy='%s'  timeout=%.0fs"),
		*Config.AgentTemplate.Name,
		*Config.ProviderId,
		Policy.IsEmpty() ? TEXT("approve(default)") : *Policy,
		TimeoutSec);

	// ---- Run session ------------------------------------------------------
	TSharedRef<FVesselSessionMachine> Machine = MakeShared<FVesselSessionMachine>();
	Machine->SetApprovalClient(MakePolicyClient(Policy));
	if (!Machine->Init(Config))
	{
		UE_LOG(LogVessel, Error, TEXT("VesselAgent: Machine->Init failed."));
		return kExitFailed;
	}
	const FString SessionLogPath = Machine->GetLogFilePath();

	TFuture<FVesselSessionOutcome> Fut = Machine->RunAsync(Prompt);
	if (!WaitFutureWithTicking(Fut, TimeoutSec))
	{
		UE_LOG(LogVessel, Error, TEXT(
			"VesselAgent: timeout after %.0fs — aborting session."), TimeoutSec);
		Machine->RequestAbort(TEXT("Commandlet timeout"));
		// Give the abort one short chance to settle.
		WaitFutureWithTicking(Fut, 5.0f);
	}

	const FVesselSessionOutcome Outcome = Fut.IsReady()
		? Fut.Get()
		: FVesselSessionOutcome::MakeFailed(TEXT("Commandlet wait did not resolve"));

	EmitSummaryJson(Outcome, SessionLogPath);
	UE_LOG(LogVessel, Log, TEXT("VesselAgent: outcome=%s steps=%d cost~$%.4f log=%s"),
		SessionOutcomeKindToString(Outcome.Kind),
		Outcome.StepsExecuted, Outcome.TotalCostUsd, *SessionLogPath);

	return Outcome.Kind == EVesselSessionOutcomeKind::Done ? kExitDone : kExitFailed;
}
