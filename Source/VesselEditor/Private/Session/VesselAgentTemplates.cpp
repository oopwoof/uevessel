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
		TEXT("  • \"add a row\" / \"set field Y on row Z\" / \"change row K\" — and the user ")
		TEXT("provides EXPLICIT values (RowName='Foo', Title='bar', Age=42)\n")
		TEXT("    → two steps: ReadDataTable first (to know the schema), then ")
		TEXT("WriteDataTableRow with the user's literal values. Both in the same plan.\n")
		TEXT("  • \"add a typical row\" / \"fill in defaults\" / \"copy row K with tweaks\" — ")
		TEXT("the user is VAGUE about the actual values\n")
		TEXT("    → one step only: ReadDataTable. End the plan after the read and put a clear ")
		TEXT("note in its `reasoning` saying \"user needs to specify field values before I can ")
		TEXT("safely write\" — do NOT bundle a write you would have to invent values for, even ")
		TEXT("if the pattern above seems to demand it.\n")
		TEXT("  • \"validate asset X\"\n")
		TEXT("    → one step: RunAssetValidator.\n\n")
		TEXT("Rules:\n")
		TEXT("  1. Trust the values the user gave you. If they say RowName='Foo', Title='bar', ")
		TEXT("Age=42 — use those literally. The read step confirms the field structure; the ")
		TEXT("user's literal values fill the row.\n")
		TEXT("  2. The approval panel is the safety mechanism for writes — the user reviews every ")
		TEXT("write before it executes. Plan the write; let the user approve or reject.\n")
		TEXT("  3. Never invent values for fields the user didn't specify. The single-shot FSM ")
		TEXT("means you commit to write args at plan time, before seeing the read result. If you'd ")
		TEXT("have to fabricate a Title or guess an Age, plan only the read and let the user supply ")
		TEXT("specifics in their next message.\n")
		TEXT("  4. Each step's `reasoning` field is shown in the approval panel. Be concrete: which ")
		TEXT("row, which fields, what value. Brevity is fine.\n")
		TEXT("  5. Empty plans are reserved for cases where the user's request literally does not ")
		TEXT("map to any tool you have access to. \"Add a row\" with explicit values always maps to ")
		TEXT("plan-this-now (read+write).\n");

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
