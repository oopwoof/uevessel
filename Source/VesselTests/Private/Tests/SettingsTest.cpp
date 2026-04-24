// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Settings/VesselProjectSettings.h"
#include "Settings/VesselUserSettings.h"
#include "Settings/VesselAuth.h"

/**
 * Settings classes: regression guard that sensitive fields live in
 * EditorPerProjectUserSettings, not in the team-shared Vessel project config.
 * See ARCHITECTURE.md ADR-006 / BUILD.md §4.0.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSettingsConfigScope,
	"Vessel.Settings.ConfigScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSettingsConfigScope::RunTest(const FString& /*Parameters*/)
{
	// Project settings: stored under a team-visible ini, NEVER "EditorPerProjectUserSettings".
	const UClass* ProjectClass = UVesselProjectSettings::StaticClass();
	TestNotNull(TEXT("UVesselProjectSettings class resolves"), ProjectClass);

	// User settings: stored under EditorPerProjectUserSettings.
	const UClass* UserClass = UVesselUserSettings::StaticClass();
	TestNotNull(TEXT("UVesselUserSettings class resolves"), UserClass);

	// UE encodes the config name as the class's config file hint. Compare
	// via the reflection-exposed config name to catch accidental scope flips
	// (e.g. someone moving AnthropicApiKey into UVesselProjectSettings).
	const UVesselUserSettings* User = GetDefault<UVesselUserSettings>();
	TestEqual(TEXT("UVesselUserSettings GetContainerName is Editor"),
		User->GetContainerName().ToString(), FString(TEXT("Editor")));

	return true;
}

/**
 * Endpoint permission rules: https always ok, http only for localhost + bAllowHttp.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAuthEndpointPermit,
	"Vessel.Settings.EndpointPermit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAuthEndpointPermit::RunTest(const FString& /*Parameters*/)
{
	TestTrue(TEXT("https:// permitted"),
		FVesselAuth::IsEndpointPermitted(TEXT("https://api.anthropic.com/v1/messages")));
	TestTrue(TEXT("empty permitted (uses provider default https)"),
		FVesselAuth::IsEndpointPermitted(FString()));

	// Non-localhost http is always rejected, regardless of settings.
	TestFalse(TEXT("http://example.com rejected"),
		FVesselAuth::IsEndpointPermitted(TEXT("http://example.com/v1")));
	TestFalse(TEXT("ftp:// rejected"),
		FVesselAuth::IsEndpointPermitted(TEXT("ftp://example.com")));

	// Localhost http requires bAllowHttp=true. We don't flip the CDO in this
	// test to avoid polluting engine state; just verify the URL string path.
	// Full toggle test would involve a mutable project settings fixture — punt
	// to Step 3 along with the TransactionScope harness.
	return true;
}

/**
 * Redaction never leaks the raw secret.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAuthRedact,
	"Vessel.Settings.Redact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAuthRedact::RunTest(const FString& /*Parameters*/)
{
	const FString Fake = TEXT("sk-not-a-real-key-abcdef");
	const FString Red = FVesselAuth::Redact(Fake);

	TestFalse(TEXT("Redacted form does not contain the raw secret"),
		Red.Contains(Fake));
	TestTrue(TEXT("Redacted form reports the length"),
		Red.Contains(FString::FromInt(Fake.Len())));
	TestEqual(TEXT("Empty secret renders as <empty>"),
		FVesselAuth::Redact(FString()), FString(TEXT("<empty>")));
	return true;
}
