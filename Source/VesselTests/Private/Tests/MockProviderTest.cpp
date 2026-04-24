// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Llm/VesselMockProvider.h"
#include "Llm/LlmProviderRegistry.h"

/**
 * Mock provider: deterministic fixture hit by last user-message content.
 * See ARCHITECTURE.md §2.4.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselMockProviderFixtureHit,
	"Vessel.Llm.MockProvider.FixtureHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselMockProviderFixtureHit::RunTest(const FString& /*Parameters*/)
{
	auto Mock = MakeShared<FVesselMockProvider>();

	FLlmResponse Fixture;
	Fixture.bOk = true;
	Fixture.Content = TEXT("mocked reply for hello");
	Mock->SetFixtureForLastUserMessage(TEXT("hello"), Fixture);

	FLlmRequest Req;
	Req.Messages.Add({ EVesselLlmRole::System, TEXT("sys prompt"), FString() });
	Req.Messages.Add({ EVesselLlmRole::User,   TEXT("hello"),      FString() });

	TFuture<FLlmResponse> Future = Mock->SendAsync(Req);
	Future.Wait();
	const FLlmResponse R = Future.Get();

	TestTrue(TEXT("Fixture hit returns bOk=true"), R.bOk);
	TestEqual(TEXT("Fixture content matches"), R.Content, FString(TEXT("mocked reply for hello")));
	TestEqual(TEXT("ProviderId stamped to mock"), R.ProviderId, FString(TEXT("mock")));
	TestEqual(TEXT("Call count recorded"), Mock->GetCallCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselMockProviderDefaultFallback,
	"Vessel.Llm.MockProvider.DefaultFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselMockProviderDefaultFallback::RunTest(const FString& /*Parameters*/)
{
	auto Mock = MakeShared<FVesselMockProvider>();

	FLlmRequest Req;
	Req.Messages.Add({ EVesselLlmRole::User, TEXT("no fixture for me"), FString() });

	TFuture<FLlmResponse> Future = Mock->SendAsync(Req);
	Future.Wait();
	const FLlmResponse R = Future.Get();

	TestFalse(TEXT("Unmatched request without default response returns bOk=false"), R.bOk);
	TestEqual(TEXT("ErrorCode is ConfigError"),
		static_cast<uint8>(R.ErrorCode),
		static_cast<uint8>(EVesselLlmErrorCode::ConfigError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselMockProviderRegistryLookup,
	"Vessel.Llm.MockProvider.RegistryLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselMockProviderRegistryLookup::RunTest(const FString& /*Parameters*/)
{
	// VesselCore::StartupModule registers a mock provider by default.
	auto Found = FLlmProviderRegistry::Get().FindProvider(TEXT("mock"));
	TestTrue(TEXT("Mock provider is registered at startup"), Found.IsValid());
	if (Found.IsValid())
	{
		TestEqual(TEXT("Registered mock provider reports correct id"),
			Found->GetProviderId(), FString(TEXT("mock")));
	}
	return true;
}
