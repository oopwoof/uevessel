// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselSessionConfig.h"
#include "Settings/VesselProjectSettings.h"

#include "Misc/DateTime.h"
#include "HAL/PlatformAtomics.h"

namespace VesselSessionConfigDetail
{
	/** Per-process monotonic counter for session ids in the same second. */
	static int32 GSessionCounter = 0;
}

FString GenerateSessionId()
{
	const int32 Counter = FPlatformAtomics::InterlockedIncrement(&VesselSessionConfigDetail::GSessionCounter);
	const FDateTime Now = FDateTime::Now();
	return FString::Printf(TEXT("vs-%04d-%02d-%02d-%04d"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(), Counter);
}

FVesselSessionConfig MakeDefaultSessionConfig(const FString& InSessionId)
{
	FVesselSessionConfig C;
	C.SessionId = InSessionId.IsEmpty() ? GenerateSessionId() : InSessionId;
	C.AgentTemplate = FVesselAgentTemplate::MakeMinimalFallback();
	C.Budget = FVesselSessionBudget{};

	// Pull sensible defaults from project-level settings when available.
	if (const UVesselProjectSettings* Settings = GetDefault<UVesselProjectSettings>())
	{
		C.ProviderId = Settings->Provider.IsEmpty() ? FString(TEXT("anthropic")) : Settings->Provider.ToLower();
		C.PlannerModel = Settings->Model.IsEmpty() ? FString(TEXT("claude-sonnet-4-6")) : Settings->Model;
	}
	else
	{
		C.ProviderId = TEXT("anthropic");
		C.PlannerModel = TEXT("claude-sonnet-4-6");
	}
	// Judge default is the cheapest production-quality tier.
	C.JudgeModel = TEXT("claude-haiku-4-5");

	return C;
}
