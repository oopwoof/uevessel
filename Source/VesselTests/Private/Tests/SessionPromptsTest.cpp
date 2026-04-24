// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"

#include "Llm/VesselLlmTypes.h"
#include "Session/VesselPlannerPrompts.h"
#include "Session/VesselSessionConfig.h"

namespace VesselSessionPromptTestDetail
{
	static FLlmResponse MakeOkResponse(const FString& Content)
	{
		FLlmResponse R;
		R.bOk = true;
		R.Content = Content;
		R.ProviderId = TEXT("mock");
		return R;
	}
}

/**
 * Plan parser round-trips a well-formed response.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptPlanParseOk,
	"Vessel.Session.Prompts.PlanParseOk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptPlanParseOk::RunTest(const FString& /*Parameters*/)
{
	const FString Json = TEXT(
		"{\"plan\":[")
		TEXT("{\"tool\":\"ReadDataTable\",\"args\":{\"AssetPath\":\"/Game/x\",\"RowNames\":[]},\"reasoning\":\"need the table\"},")
		TEXT("{\"tool\":\"ListAssets\",\"args\":{\"ContentPath\":\"/Game\",\"bRecursive\":true},\"reasoning\":\"survey\"}")
		TEXT("]}");

	const FVesselPlan Plan = FVesselPlannerPrompts::ParsePlanResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(Json));
	TestTrue(TEXT("Plan is valid"), Plan.bValid);
	TestEqual(TEXT("Two steps"), Plan.Steps.Num(), 2);
	if (Plan.Steps.Num() == 2)
	{
		TestEqual(TEXT("Step 1 tool"),    Plan.Steps[0].ToolName, FName(TEXT("ReadDataTable")));
		TestEqual(TEXT("Step 1 index"),   Plan.Steps[0].StepIndex, 1);
		TestTrue(TEXT("Step 1 reasoning present"), Plan.Steps[0].Reasoning.Contains(TEXT("need the table")));
		TestEqual(TEXT("Step 2 tool"),    Plan.Steps[1].ToolName, FName(TEXT("ListAssets")));
		TestTrue(TEXT("Step 2 args JSON contains ContentPath"),
			Plan.Steps[1].ArgsJson.Contains(TEXT("ContentPath")));
	}
	return true;
}

/**
 * Plan parser tolerates markdown-fenced JSON and extracts the inner plan.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptPlanParseFenced,
	"Vessel.Session.Prompts.PlanParseFenced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptPlanParseFenced::RunTest(const FString& /*Parameters*/)
{
	const FString Raw =
		TEXT("```json\n")
		TEXT("{\"plan\":[{\"tool\":\"ReadDataTable\",\"args\":{},\"reasoning\":\"x\"}]}\n")
		TEXT("```");
	const FVesselPlan Plan = FVesselPlannerPrompts::ParsePlanResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(Raw));
	TestTrue(TEXT("Fenced plan parsed"), Plan.bValid);
	TestEqual(TEXT("Exactly one step"), Plan.Steps.Num(), 1);
	return true;
}

/**
 * Plan parser rejects missing 'plan' field with an LLM-readable error.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptPlanParseMissingField,
	"Vessel.Session.Prompts.PlanParseMissingField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptPlanParseMissingField::RunTest(const FString& /*Parameters*/)
{
	const FVesselPlan Plan = FVesselPlannerPrompts::ParsePlanResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(TEXT("{\"not_plan\":[]}")));
	TestFalse(TEXT("Invalid"), Plan.bValid);
	TestTrue(TEXT("Error names 'plan'"), Plan.ErrorMessage.Contains(TEXT("plan")));
	return true;
}

/**
 * Judge parser round-trips approve/revise/reject shapes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptJudgeParseApprove,
	"Vessel.Session.Prompts.JudgeParseApprove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptJudgeParseApprove::RunTest(const FString& /*Parameters*/)
{
	const FVesselJudgeVerdict V = FVesselPlannerPrompts::ParseJudgeResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(
			TEXT("{\"decision\":\"approve\",\"reasoning\":\"tool output matches intent\"}")));
	TestEqual(TEXT("Decision Approve"),
		static_cast<uint8>(V.Decision), static_cast<uint8>(EVesselJudgeDecision::Approve));
	TestTrue(TEXT("Reasoning round-trip"),
		V.Reasoning.Contains(TEXT("tool output matches intent")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptJudgeParseRevise,
	"Vessel.Session.Prompts.JudgeParseRevise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptJudgeParseRevise::RunTest(const FString& /*Parameters*/)
{
	const FVesselJudgeVerdict V = FVesselPlannerPrompts::ParseJudgeResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(
			TEXT("{\"decision\":\"revise\",\"reasoning\":\"close\",\"revise_directive\":\"narrow the row set\"}")));
	TestEqual(TEXT("Decision Revise"),
		static_cast<uint8>(V.Decision), static_cast<uint8>(EVesselJudgeDecision::Revise));
	TestEqual(TEXT("Revise directive round-trip"),
		V.ReviseDirective, FString(TEXT("narrow the row set")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptJudgeParseReject,
	"Vessel.Session.Prompts.JudgeParseReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptJudgeParseReject::RunTest(const FString& /*Parameters*/)
{
	const FVesselJudgeVerdict V = FVesselPlannerPrompts::ParseJudgeResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(
			TEXT("{\"decision\":\"reject\",\"reasoning\":\"cannot satisfy\",\"reject_reason\":\"no write tool allowed\"}")));
	TestEqual(TEXT("Decision Reject"),
		static_cast<uint8>(V.Decision), static_cast<uint8>(EVesselJudgeDecision::Reject));
	TestEqual(TEXT("Reject reason round-trip"),
		V.RejectReason, FString(TEXT("no write tool allowed")));
	return true;
}

/**
 * Judge falls back to Reject when the response is malformed — safety-by-default.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptJudgeParseMalformedDefaultsReject,
	"Vessel.Session.Prompts.JudgeParseMalformedDefaultsReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptJudgeParseMalformedDefaultsReject::RunTest(const FString& /*Parameters*/)
{
	const FVesselJudgeVerdict V = FVesselPlannerPrompts::ParseJudgeResponse(
		VesselSessionPromptTestDetail::MakeOkResponse(TEXT("not a JSON")));
	TestEqual(TEXT("Falls back to Reject on malformed input"),
		static_cast<uint8>(V.Decision), static_cast<uint8>(EVesselJudgeDecision::Reject));
	TestFalse(TEXT("Reject reason non-empty"), V.RejectReason.IsEmpty());
	return true;
}

/**
 * BuildPlanningRequest filters tools by the agent template's allowed categories.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptFilterByAllowedCategory,
	"Vessel.Session.Prompts.FilterByAllowedCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptFilterByAllowedCategory::RunTest(const FString& /*Parameters*/)
{
	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(TEXT("test-filter"));
	Cfg.AgentTemplate.AllowedCategories.Add(TEXT("DataTable"));

	FVesselToolSchema Allowed;
	Allowed.Name = FName(TEXT("ReadDataTable"));
	Allowed.Category = TEXT("DataTable");
	Allowed.Description = TEXT("read rows");
	FVesselToolSchema Denied;
	Denied.Name = FName(TEXT("ListAssets"));
	Denied.Category = TEXT("Meta");
	Denied.Description = TEXT("list assets");

	const FLlmRequest Request = FVesselPlannerPrompts::BuildPlanningRequest(
		Cfg, TEXT("do stuff"), { Allowed, Denied }, /*Revise*/ FString());

	// System message is the one before the user message and should embed the
	// JSON-rendered tools array.
	TestTrue(TEXT("Request has a system message"), Request.Messages.Num() >= 2);
	const FLlmMessage& Sys = Request.Messages[0];
	TestTrue(TEXT("System prompt mentions ReadDataTable"),
		Sys.Content.Contains(TEXT("ReadDataTable")));
	TestFalse(TEXT("System prompt excludes ListAssets (Meta not allowed)"),
		Sys.Content.Contains(TEXT("ListAssets")));
	return true;
}
