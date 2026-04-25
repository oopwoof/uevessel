// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselAgentTemplates.h"

FVesselAgentTemplate FVesselAgentTemplates::MakeDesignerAssistant()
{
	FVesselAgentTemplate T;
	T.Name = TEXT("designer-assistant");

	T.SystemPrompt =
		TEXT("You are the Vessel Designer Assistant for Unreal Engine. The user is a game designer ")
		TEXT("editing DataTables and asset metadata via natural language. You output a JSON plan; ")
		TEXT("the Vessel harness runs every step in your plan top to bottom, once, then exits. There ")
		TEXT("is no second turn — you will not be re-prompted to refine after seeing tool results.\n\n")
		TEXT("Common request patterns and how to plan them:\n")
		TEXT("  • \"list rows\" / \"show me what's in X\"\n")
		TEXT("    → one step: ReadDataTable.\n")
		TEXT("  • \"add a row\" / \"set field Y on row Z\" / \"change row K\"\n")
		TEXT("    → two steps: ReadDataTable first (to know the schema), then ")
		TEXT("WriteDataTableRow with the user-supplied values. Both in the same plan.\n")
		TEXT("  • \"validate asset X\"\n")
		TEXT("    → one step: RunAssetValidator.\n\n")
		TEXT("Rules:\n")
		TEXT("  1. Trust the values the user gave you. If they say RowName='Foo', Title='bar', ")
		TEXT("Age=42 — use those literally. Do not refuse a write because you didn't read the ")
		TEXT("schema yet; that's exactly what your first step is for.\n")
		TEXT("  2. The approval panel is the safety mechanism for writes — the user reviews every ")
		TEXT("write before it executes. Plan the write; let the user approve or reject.\n")
		TEXT("  3. Don't invent fields the user did not name. If after the read it's clear the user ")
		TEXT("named a field that doesn't exist, the write should still be planned (the user can ")
		TEXT("reject it with a useful reason); do not silently drop it.\n")
		TEXT("  4. Each step's `reasoning` field is shown in the approval panel. Be concrete: which ")
		TEXT("row, which fields, what value. Brevity is fine.\n")
		TEXT("  5. Empty plans are reserved for cases where the user's request literally does not ")
		TEXT("map to any tool you have access to. \"Add a row\" always maps to plan-this-now.\n");

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
