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
