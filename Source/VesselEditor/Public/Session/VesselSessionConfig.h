// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Hard budgets that can terminate a session early. Values are the
 * industrial-scene defaults from SESSION_MACHINE.md §5.
 */
struct FVesselSessionBudget
{
	/** Maximum FSM steps before the session auto-aborts. Batch ops count as 1. */
	int32 MaxSteps = 100;

	/** Hard USD cap (sum of Planner + Judge LLM calls). */
	double MaxCostUsd = 30.0;

	/** Wall-clock cap from session start. Exceeding → Failed. */
	int32 MaxWallTimeSec = 3600;

	/** Circuit-break N identical (tool, ErrorCode) failures → Failed. */
	int32 RepeatErrorLimit = 5;

	/** N consecutive Judge Revise decisions with no progress → Failed (agent loop detected). */
	int32 MaxConsecutiveRevise = 5;
};

/**
 * Per-agent behavioral config. Shipped alongside the templates yaml in v0.2;
 * for v0.1 this is populated programmatically or loaded from AGENTS.md.
 */
struct FVesselAgentTemplate
{
	/** Stable identifier, e.g. "designer-assistant" / "dev-chat". */
	FString Name;

	/** Role / behavior description, baked into Planner system prompt. */
	FString SystemPrompt;

	/** Ruler text given to the Judge LLM ("approve if X and Y, revise if ..."). */
	FString JudgeRubric;

	/** Optional allowlist of tool categories this agent can use (empty = all allowed). */
	TArray<FString> AllowedCategories;

	/** Explicit deny list (beats AllowedCategories if a tool matches). */
	TArray<FString> DeniedTools;

	static FVesselAgentTemplate MakeMinimalFallback()
	{
		FVesselAgentTemplate T;
		T.Name = TEXT("vessel-default");
		T.SystemPrompt = TEXT("You are Vessel, an assistant that uses registered tools "
			"to complete Unreal Engine development tasks. Before writing anything, explain "
			"your plan. Prefer fewest tool calls.");
		T.JudgeRubric = TEXT("Approve when the tool output satisfies the user intent AND "
			"no validator returned errors. Revise when the output is close but incorrect. "
			"Reject when the user intent cannot be met with the available tools.");
		return T;
	}
};

/**
 * Merged, resolved config for one session. Produced by
 * UVesselProjectSettings overrides → agent template overrides.
 * Once a session starts, the config is frozen — do not mutate mid-run.
 */
struct FVesselSessionConfig
{
	/** Unique session identifier. Format: vs-YYYY-MM-DD-NNNN (4-digit per-day counter). */
	FString SessionId;

	/** Resolved template (agent-level). */
	FVesselAgentTemplate AgentTemplate;

	/** Resolved budget (post-merge). */
	FVesselSessionBudget Budget;

	/** Provider id of the LLM provider (e.g. "anthropic", "mock"). */
	FString ProviderId;

	/** Model for the Planner turn. */
	FString PlannerModel;

	/** Model for the Judge turn (often a cheaper tier — Opus plan, Haiku judge is typical). */
	FString JudgeModel;
};

/**
 * Build a default session config from UVesselProjectSettings.
 * Uses `vessel-default` agent template. Caller can override fields afterward.
 */
VESSELEDITOR_API FVesselSessionConfig MakeDefaultSessionConfig(const FString& InSessionId);

/** Generate an ISO-style session id from the current time. Not globally unique; counter is per-process. */
VESSELEDITOR_API FString GenerateSessionId();
