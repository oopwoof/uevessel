// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Registry/VesselToolSchema.h"
#include "Session/VesselAgentTemplates.h"
#include "Session/VesselPlannerPrompts.h"
#include "Session/VesselSessionConfig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAgentTemplatesDesignerShape,
	"Vessel.Session.AgentTemplates.DesignerShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAgentTemplatesDesignerShape::RunTest(const FString& /*Parameters*/)
{
	const FVesselAgentTemplate T = FVesselAgentTemplates::MakeDesignerAssistant();
	TestEqual(TEXT("Name is designer-assistant"), T.Name, FString(TEXT("designer-assistant")));
	TestTrue(TEXT("System prompt non-empty"),     !T.SystemPrompt.IsEmpty());
	TestTrue(TEXT("Judge rubric non-empty"),      !T.JudgeRubric.IsEmpty());
	TestTrue(TEXT("Allowed includes DataTable"),  T.AllowedCategories.Contains(TEXT("DataTable")));
	TestTrue(TEXT("Allowed includes Meta"),       T.AllowedCategories.Contains(TEXT("Meta")));
	TestFalse(TEXT("Write not granted by default"),
		T.AllowedCategories.Contains(TEXT("DataTable/Write")));

	// Prevent regression of three load-bearing prompt properties (each caught
	// the hard way during v0.2 L5 testing — without all three, Sonnet either
	// plans only a preparatory read OR returns an empty plan):
	//
	//   (a) No-second-turn: harness does not call Planner again after
	//       approved steps; LLM must plan everything up-front.
	//   (b) Read-then-write pattern is named explicitly so the LLM knows
	//       to bundle both calls in one plan.
	//   (c) Approval-panel-is-the-safety-net so the LLM doesn't preemptively
	//       refuse a write out of "safety" — the user has the veto.
	TestTrue(TEXT("System prompt names 'no second turn' / single-shot semantic"),
		T.SystemPrompt.Contains(TEXT("no second turn"))
		|| T.SystemPrompt.Contains(TEXT("not re-prompt"))
		|| T.SystemPrompt.Contains(TEXT("does NOT re-prompt")));
	TestTrue(TEXT("System prompt names the read-then-write two-step pattern"),
		T.SystemPrompt.Contains(TEXT("ReadDataTable first"))
		&& T.SystemPrompt.Contains(TEXT("WriteDataTableRow")));
	TestTrue(TEXT("System prompt names approval panel as the safety mechanism"),
		T.SystemPrompt.Contains(TEXT("approval panel"))
		&& (T.SystemPrompt.Contains(TEXT("safety"))
			|| T.SystemPrompt.Contains(TEXT("reviews"))
			|| T.SystemPrompt.Contains(TEXT("approve or reject"))));

	// Pin the "vague request → read-only plan" guidance. Without this rule
	// the LLM will hallucinate field values for a write step it has to plan
	// before the read step has executed (single-shot FSM constraint surfaced
	// in v0.2 Gemini review).
	TestTrue(TEXT("System prompt distinguishes vague vs explicit value requests"),
		T.SystemPrompt.Contains(TEXT("VAGUE"))
		&& T.SystemPrompt.Contains(TEXT("EXPLICIT")));
	TestTrue(TEXT("System prompt forbids inventing field values"),
		T.SystemPrompt.Contains(TEXT("Never invent values"))
		|| T.SystemPrompt.Contains(TEXT("invent values for fields"))
		|| T.SystemPrompt.Contains(TEXT("fabricate a Title")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAgentTemplatesLookup,
	"Vessel.Session.AgentTemplates.Lookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAgentTemplatesLookup::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("Designer lookup by name"),
		FVesselAgentTemplates::FindByName(TEXT("designer-assistant")).Name,
		FString(TEXT("designer-assistant")));
	TestEqual(TEXT("Asset-pipeline lookup by name"),
		FVesselAgentTemplates::FindByName(TEXT("asset-pipeline")).Name,
		FString(TEXT("asset-pipeline")));

	// Unknown name falls back to the minimal built-in.
	const FVesselAgentTemplate Unknown =
		FVesselAgentTemplates::FindByName(TEXT("no-such-agent-xyz"));
	TestEqual(TEXT("Unknown name falls back to vessel-default"),
		Unknown.Name, FString(TEXT("vessel-default")));

	// Listing returns the known names in stable order.
	const TArray<FString> All = FVesselAgentTemplates::ListNames();
	TestTrue(TEXT("ListNames contains designer-assistant"),
		All.Contains(TEXT("designer-assistant")));
	TestTrue(TEXT("ListNames contains asset-pipeline"),
		All.Contains(TEXT("asset-pipeline")));
	return true;
}

/**
 * Asset Pipeline persona contract:
 *   - Allowed: Asset + Validator (NOT DataTable — that's Designer's scope).
 *   - Judge rubric calls validator output ground truth.
 *   - System prompt distinguishes pipeline scope from designer scope so
 *     the LLM doesn't drift into row writes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAgentTemplatesAssetPipelineShape,
	"Vessel.Session.AgentTemplates.AssetPipelineShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAgentTemplatesAssetPipelineShape::RunTest(const FString& /*Parameters*/)
{
	const FVesselAgentTemplate T = FVesselAgentTemplates::MakeAssetPipelineAgent();

	TestEqual(TEXT("Name is asset-pipeline"), T.Name, FString(TEXT("asset-pipeline")));
	TestTrue(TEXT("System prompt non-empty"),  !T.SystemPrompt.IsEmpty());
	TestTrue(TEXT("Judge rubric non-empty"),   !T.JudgeRubric.IsEmpty());

	// Scope discrimination: Asset+Validator allowed, DataTable NOT allowed.
	TestTrue(TEXT("Allowed includes Asset"),
		T.AllowedCategories.Contains(TEXT("Asset")));
	TestTrue(TEXT("Allowed includes Validator"),
		T.AllowedCategories.Contains(TEXT("Validator")));
	TestFalse(TEXT("Allowed does NOT include DataTable"),
		T.AllowedCategories.Contains(TEXT("DataTable")));
	TestFalse(TEXT("Allowed does NOT include DataTable/Write"),
		T.AllowedCategories.Contains(TEXT("DataTable/Write")));

	// Prompt explicitly tells LLM it has no DataTable tools to discourage drift.
	TestTrue(TEXT("Prompt mentions DataTable scope is Designer's, not Pipeline's"),
		T.SystemPrompt.Contains(TEXT("DataTable"))
		&& (T.SystemPrompt.Contains(TEXT("Designer Assistant"))
			|| T.SystemPrompt.Contains(TEXT("designer-assistant"))));

	// Judge rubric pins validator-as-ground-truth philosophy.
	TestTrue(TEXT("Judge rubric calls validator output ground truth"),
		T.JudgeRubric.Contains(TEXT("validator")));
	return true;
}

/**
 * Prefix-match semantics: an AllowedCategories entry of "DataTable" must
 * cover tools tagged "DataTable/Write" (and any deeper sub-category). This
 * is what lets the Designer Assistant actually run WriteDataTableRow even
 * though its AllowedCategories only literally lists the parent.
 *
 * Prior review (Gemini, Step 4c.2 pass) flagged the apparent
 * "Designer can't write" misread of this semantic; this test exists to
 * lock the behavior and short-circuit the same confusion in the future.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAgentTemplatesAllowedCategoryPrefixMatch,
	"Vessel.Session.AgentTemplates.AllowedCategoryPrefixMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAgentTemplatesAllowedCategoryPrefixMatch::RunTest(const FString& /*Parameters*/)
{
	// Build a minimal config whose agent allows category "DataTable" only.
	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(TEXT("prefix-match-test"));
	Cfg.AgentTemplate = FVesselAgentTemplates::MakeDesignerAssistant();
	TestTrue(TEXT("Designer allows 'DataTable'"),
		Cfg.AgentTemplate.AllowedCategories.Contains(TEXT("DataTable")));
	TestFalse(TEXT("Designer does NOT literally list 'DataTable/Write'"),
		Cfg.AgentTemplate.AllowedCategories.Contains(TEXT("DataTable/Write")));

	// Fabricate two tool schemas: a pure parent match and a sub-category child.
	FVesselToolSchema ReadTool;
	ReadTool.Name        = FName(TEXT("ReadDataTable"));
	ReadTool.Category    = TEXT("DataTable");
	ReadTool.Description = TEXT("read");

	FVesselToolSchema WriteTool;
	WriteTool.Name        = FName(TEXT("WriteDataTableRow"));
	WriteTool.Category    = TEXT("DataTable/Write");
	WriteTool.Description = TEXT("write");

	FVesselToolSchema UnrelatedTool;
	UnrelatedTool.Name        = FName(TEXT("ListAssets"));
	UnrelatedTool.Category    = TEXT("Asset");
	UnrelatedTool.Description = TEXT("list");

	const FLlmRequest Request = FVesselPlannerPrompts::BuildPlanningRequest(
		Cfg, TEXT("test"), { ReadTool, WriteTool, UnrelatedTool }, FString());

	// The system message contains the JSON-rendered tool catalog.
	TestTrue(TEXT("Request includes a system message"), Request.Messages.Num() >= 1);
	const FLlmMessage& Sys = Request.Messages[0];

	TestTrue(TEXT("ReadDataTable visible (exact category match)"),
		Sys.Content.Contains(TEXT("ReadDataTable")));
	TestTrue(TEXT("WriteDataTableRow visible (prefix match via 'DataTable/')"),
		Sys.Content.Contains(TEXT("WriteDataTableRow")));
	TestFalse(TEXT("ListAssets NOT visible (Asset is not in Allowed list)"),
		Sys.Content.Contains(TEXT("ListAssets")));
	return true;
}
