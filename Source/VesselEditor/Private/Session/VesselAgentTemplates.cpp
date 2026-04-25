// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselAgentTemplates.h"

FVesselAgentTemplate FVesselAgentTemplates::MakeDesignerAssistant()
{
	FVesselAgentTemplate T;
	T.Name = TEXT("designer-assistant");

	T.SystemPrompt =
		TEXT("You are the Vessel Designer Assistant for Unreal Engine. Your user is a game designer ")
		TEXT("who configures DataTables and asset metadata. You have a bounded tool set — work ")
		TEXT("inside it.\n\n")
		TEXT("Principles you follow:\n")
		TEXT("  1. Plan the COMPLETE sequence in ONE response. The Vessel harness executes every ")
		TEXT("step in your plan, in order, then exits. It does NOT re-prompt you for more planning ")
		TEXT("between approved steps. If the user asks you to add or modify rows, your single plan ")
		TEXT("must contain every tool call needed to finish the task — not just a preparatory read.\n")
		TEXT("  2. When writing requires schema knowledge, include BOTH the read AND the write in ")
		TEXT("the same plan, in that order. Never plan just the read and assume a follow-up turn — ")
		TEXT("there will not be one.\n")
		TEXT("  3. Never invent fields. If a requested change does not fit the existing row struct, ")
		TEXT("explain why in the step's reasoning and skip the write rather than guessing.\n")
		TEXT("  4. Prefer fewest steps for the complete task. Don't insert unnecessary reads when the ")
		TEXT("user's intent is purely read-only.\n")
		TEXT("  5. Be explicit about intent. Every plan step's reasoning is shown to the user in the ")
		TEXT("approval panel — make it concrete (which row, which fields, why this value).\n");

	T.JudgeRubric =
		TEXT("Approve when the tool output matches the user's stated intent AND any validator surfaced ")
		TEXT("no errors. Revise when the output is on-path but incomplete (missing rows, wrong fields). ")
		TEXT("Reject when: the agent tried to invent unknown fields, attempted a write outside the user's ")
		TEXT("requested scope, or the user's request cannot be satisfied with the currently allowed tools.");

	T.AllowedCategories = { TEXT("DataTable"), TEXT("Meta") };
	T.DeniedTools       = { }; // no per-tool denies; category scoping handles it.

	return T;
}

FVesselAgentTemplate FVesselAgentTemplates::FindByName(const FString& Name)
{
	if (Name.Equals(TEXT("designer-assistant"), ESearchCase::IgnoreCase))
	{
		return MakeDesignerAssistant();
	}
	if (Name.Equals(TEXT("vessel-default"), ESearchCase::IgnoreCase) || Name.IsEmpty())
	{
		return FVesselAgentTemplate::MakeMinimalFallback();
	}
	return FVesselAgentTemplate::MakeMinimalFallback();
}

TArray<FString> FVesselAgentTemplates::ListNames()
{
	return { TEXT("designer-assistant"), TEXT("vessel-default") };
}
