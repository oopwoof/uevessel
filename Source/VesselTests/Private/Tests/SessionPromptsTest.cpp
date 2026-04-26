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

/**
 * BuildJudgeRequest is now uniform across every path that reaches it: there
 * is no `bUserEditedArgs` branch in the prompt, because the FSM short-circuits
 * user-edited steps to a deterministic Approve before reaching the LLM Judge
 * (see VesselSessionMachine::EnterJudgeReview). This test pins that the
 * uniform shape did not regress — the Judge prompt always contains the
 * agent rubric and the four step-fields, and never carries the historical
 * `user_edited_args` / `original_planned_args` markers from the v0.2
 * iteration, which would cause cross-test confusion if they leaked back in.
 *
 * The end-to-end behavioural test for the edit-bypass lives in
 * HITLGateTest (Vessel.HITL.E2E.EditAndApproveBypassesLlmJudge) — that is
 * the FSM-level contract; this is the prompt-level contract.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselPromptJudgeUniformShape,
	"Vessel.Session.Prompts.JudgeUniformShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselPromptJudgeUniformShape::RunTest(const FString& /*Parameters*/)
{
	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(TEXT("test-judge-uniform"));
	Cfg.AgentTemplate.JudgeRubric = TEXT("PIN-RUBRIC-MARKER-XYZZY");

	FVesselPlanStep Step;
	Step.StepIndex = 1;
	Step.ToolName  = FName(TEXT("ReadDataTable"));
	Step.Reasoning = TEXT("read schema");
	Step.ArgsJson  = TEXT("{\"AssetPath\":\"/Game/x\"}");
	// bUserEditedArgs intentionally NOT set: BuildJudgeRequest is never
	// called for edited steps anymore (FSM bypass), so we test only the
	// non-edited shape — the only shape the Judge LLM ever sees.

	const FLlmRequest Req = FVesselPlannerPrompts::BuildJudgeRequest(
		Cfg, Step, TEXT("{\"rows\":[]}"));

	TestTrue(TEXT("Request has system + user messages"), Req.Messages.Num() >= 2);
	const FLlmMessage& Sys  = Req.Messages[0];
	const FLlmMessage& User = Req.Messages[1];

	TestTrue(TEXT("System prompt embeds the agent's Judge rubric"),
		Sys.Content.Contains(TEXT("PIN-RUBRIC-MARKER-XYZZY")));
	TestTrue(TEXT("System prompt declares standard intent-evaluation framing"),
		Sys.Content.Contains(TEXT("Evaluate the executed tool call")));
	TestTrue(TEXT("System prompt names the output contract"),
		Sys.Content.Contains(TEXT("Output contract")));
	TestTrue(TEXT("User message carries the four step fields"),
		User.Content.Contains(TEXT("ReadDataTable"))
		&& User.Content.Contains(TEXT("AssetPath")));

	// Regression guard: the v0.2 iteration's edit-aware fields and prompt
	// sections must NOT appear — they were retired in favour of the FSM
	// bypass. If they reappear here it means someone re-introduced the
	// branching and likely regressed the Judge against user overrides.
	TestFalse(TEXT("Uniform Judge prompt does NOT carry user_edited_args"),
		User.Content.Contains(TEXT("user_edited_args")));
	TestFalse(TEXT("Uniform Judge prompt does NOT carry original_planned_args"),
		User.Content.Contains(TEXT("original_planned_args")));
	TestFalse(TEXT("System prompt does NOT carry the retired tool-output-validation reframe"),
		Sys.Content.Contains(TEXT("tool-output validation")));
	return true;
}
