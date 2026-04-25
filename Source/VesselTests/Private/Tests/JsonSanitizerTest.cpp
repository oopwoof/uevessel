// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Util/VesselJsonSanitizer.h"
#include "Dom/JsonObject.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerBareObject,
	"Vessel.Util.JsonSanitizer.BareObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerBareObject::RunTest(const FString& /*Parameters*/)
{
	const FString Raw = TEXT("{\"a\":1,\"b\":\"hi\"}");
	FString Out;
	TestTrue(TEXT("ExtractFirstJsonObject succeeds on bare object"),
		FVesselJsonSanitizer::ExtractFirstJsonObject(Raw, Out));
	TestEqual(TEXT("Bare object passes through unchanged"), Out, Raw);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerFencedJson,
	"Vessel.Util.JsonSanitizer.FencedJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerFencedJson::RunTest(const FString& /*Parameters*/)
{
	const FString Raw =
		TEXT("```json\n")
		TEXT("{\"name\":\"ReadDataTable\",\"args\":{\"row_names\":[\"R1\"]}}\n")
		TEXT("```");
	FString Out;
	TestTrue(TEXT("Extracts JSON from ```json fenced block"),
		FVesselJsonSanitizer::ExtractFirstJsonObject(Raw, Out));

	TSharedPtr<FJsonObject> Obj;
	TestTrue(TEXT("Sanitized output parses as JSON object"),
		FVesselJsonSanitizer::ParseAsObject(Raw, Obj));
	if (Obj.IsValid())
	{
		FString Name;
		TestTrue(TEXT("Parsed object has name field"), Obj->TryGetStringField(TEXT("name"), Name));
		TestEqual(TEXT("name field value correct"), Name, FString(TEXT("ReadDataTable")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerPreludeText,
	"Vessel.Util.JsonSanitizer.PreludeText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerPreludeText::RunTest(const FString& /*Parameters*/)
{
	const FString Raw =
		TEXT("Sure! Here is the JSON you asked for:\n")
		TEXT("{\"ok\":true}\n")
		TEXT("Let me know if you need more.");
	FString Out;
	TestTrue(TEXT("Extracts JSON despite surrounding prose"),
		FVesselJsonSanitizer::ExtractFirstJsonObject(Raw, Out));
	TestEqual(TEXT("Extracted exactly the JSON object"), Out, FString(TEXT("{\"ok\":true}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerNested,
	"Vessel.Util.JsonSanitizer.Nested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerNested::RunTest(const FString& /*Parameters*/)
{
	const FString Raw = TEXT("{\"outer\":{\"inner\":{\"x\":1}},\"tail\":2}");
	FString Out;
	TestTrue(TEXT("Nested objects: brace matcher walks all depths"),
		FVesselJsonSanitizer::ExtractFirstJsonObject(Raw, Out));
	TestEqual(TEXT("Full object returned"), Out, Raw);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerBracesInStrings,
	"Vessel.Util.JsonSanitizer.BracesInStrings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerBracesInStrings::RunTest(const FString& /*Parameters*/)
{
	// Braces inside string literals must not trigger early termination.
	const FString Raw = TEXT("{\"msg\":\"hello } world {\",\"done\":true}");
	FString Out;
	TestTrue(TEXT("Matcher respects string quoting"),
		FVesselJsonSanitizer::ExtractFirstJsonObject(Raw, Out));
	TestEqual(TEXT("Full object preserved including quoted braces"), Out, Raw);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerUnbalanced,
	"Vessel.Util.JsonSanitizer.Unbalanced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerUnbalanced::RunTest(const FString& /*Parameters*/)
{
	const FString Raw = TEXT("{\"unclosed\":\"whoops");
	FString Out;
	TestFalse(TEXT("Unclosed JSON reports failure"),
		FVesselJsonSanitizer::ExtractFirstJsonObject(Raw, Out));
	TestTrue(TEXT("OutJson stays empty on failure"), Out.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerNoObject,
	"Vessel.Util.JsonSanitizer.NoObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerNoObject::RunTest(const FString& /*Parameters*/)
{
	TSharedPtr<FJsonObject> Obj;
	TestFalse(TEXT("Plain text with no JSON fails"),
		FVesselJsonSanitizer::ParseAsObject(TEXT("not a JSON at all"), Obj));
	TestFalse(TEXT("Fenced but empty fails gracefully"),
		FVesselJsonSanitizer::ParseAsObject(TEXT("```json\n```"), Obj));
	return true;
}

// =============================================================================
// Layer-B repair: unescaped " inside string values
// =============================================================================
//
// Regression: Sonnet emitted `{"reasoning":"用户要求添加一行"典型数据""}` —
// inner ASCII " breaks JSON. Layer A is a prompt rule (use 「」/'/escape);
// Layer B is the sanitizer attempting one repair pass when strict parse fails.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerRepairInnerQuotes,
	"Vessel.Util.JsonSanitizer.RepairInnerUnescapedQuotes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerRepairInnerQuotes::RunTest(const FString& /*Parameters*/)
{
	// Real Sonnet output, copy-pasted from session vs-2026-04-24-0026.
	const FString Raw =
		TEXT("{\"plan\":[{\"tool\":\"ReadDataTable\",\"args\":")
		TEXT("{\"AssetPath\":\"/Game/X\",\"RowNames\":[]},")
		TEXT("\"reasoning\":\"用户要求添加一行\"典型数据\"，但未指定具体字段值。\"}]}");

	TSharedPtr<FJsonObject> Obj;
	TestTrue(TEXT("ParseAsObject succeeds via repair pass"),
		FVesselJsonSanitizer::ParseAsObject(Raw, Obj));
	if (!Obj.IsValid()) return false;

	const TArray<TSharedPtr<FJsonValue>>* Plan = nullptr;
	TestTrue(TEXT("Plan field present"),
		Obj->TryGetArrayField(TEXT("plan"), Plan));
	TestEqual(TEXT("One step parsed"), Plan ? Plan->Num() : -1, 1);
	if (!Plan || Plan->Num() != 1) return false;

	const TSharedPtr<FJsonObject>& Step = (*Plan)[0]->AsObject();
	TestEqual(TEXT("Step tool name preserved"),
		Step->GetStringField(TEXT("tool")),
		FString(TEXT("ReadDataTable")));

	// The reasoning content survives (with escaped inner quotes).
	const FString Reasoning = Step->GetStringField(TEXT("reasoning"));
	TestTrue(TEXT("Reasoning has user-text continuation"),
		Reasoning.Contains(TEXT("典型数据")) &&
		Reasoning.Contains(TEXT("但未指定")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerRepairLeavesValidUntouched,
	"Vessel.Util.JsonSanitizer.RepairLeavesValidUntouched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerRepairLeavesValidUntouched::RunTest(const FString& /*Parameters*/)
{
	// A perfectly valid JSON should pass strict parse and never need repair.
	// We assert idempotence of the repair pass on a clean blob — guards
	// against the heuristic accidentally mangling a string that contains
	// quote-like Chinese punctuation or properly-escaped quotes.
	const FString CleanRaw =
		TEXT("{\"plan\":[{\"tool\":\"ReadDataTable\",\"reasoning\":")
		TEXT("\"用户「典型数据」需要补字段\"}]}");

	TSharedPtr<FJsonObject> Obj;
	TestTrue(TEXT("Strict parse already succeeds"),
		FVesselJsonSanitizer::ParseAsObject(CleanRaw, Obj));

	const FString Repaired =
		FVesselJsonSanitizer::RepairUnescapedInnerQuotes(CleanRaw);
	TestEqual(TEXT("Repair pass leaves valid JSON byte-for-byte"),
		Repaired, CleanRaw);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselJsonSanitizerRepairHandlesEscapedQuote,
	"Vessel.Util.JsonSanitizer.RepairHandlesAlreadyEscaped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselJsonSanitizerRepairHandlesEscapedQuote::RunTest(const FString& /*Parameters*/)
{
	// JSON with a properly-escaped \" must NOT be double-escaped by the
	// repair walker.
	const FString Raw =
		TEXT("{\"k\":\"He said \\\"hi\\\".\"}");
	TSharedPtr<FJsonObject> Obj;
	TestTrue(TEXT("Already-escaped quotes parse on first try"),
		FVesselJsonSanitizer::ParseAsObject(Raw, Obj));
	if (!Obj.IsValid()) return false;
	TestEqual(TEXT("Value preserves the inner quote characters"),
		Obj->GetStringField(TEXT("k")),
		FString(TEXT("He said \"hi\".")));
	return true;
}
