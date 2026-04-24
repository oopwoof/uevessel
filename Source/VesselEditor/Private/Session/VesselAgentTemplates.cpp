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
		TEXT("  1. Prefer fewest tool calls. When one step suffices, plan one step.\n")
		TEXT("  2. Never invent fields. If a requested change does not fit the existing row struct, ")
		TEXT("explain why instead of issuing a write.\n")
		TEXT("  3. Before writing, read. If you don't know the current shape of a DataTable, call ")
		TEXT("ReadDataTable first.\n")
		TEXT("  4. Be explicit about intent. Every plan step must include concise reasoning the user ")
		TEXT("can verify in the approval panel.\n");

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
