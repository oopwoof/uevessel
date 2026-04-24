// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Llm/VesselMockProvider.h"
#include "Async/Async.h"

FVesselMockProvider::FVesselMockProvider()
{
	DefaultResponse = FLlmResponse::MakeError(
		EVesselLlmErrorCode::ConfigError,
		TEXT("MockProvider has no fixture for this request (and no DefaultResponse was set). "
		     "Tests must call SetFixtureForLastUserMessage or SetDefaultResponse explicitly."));
	DefaultResponse.ProviderId = TEXT("mock");
}

void FVesselMockProvider::SetFixtureForLastUserMessage(
	const FString& UserMessageExact, const FLlmResponse& Response)
{
	FScopeLock Guard(&Lock);
	FLlmResponse Stamped = Response;
	if (Stamped.ProviderId.IsEmpty())
	{
		Stamped.ProviderId = TEXT("mock");
	}
	FixturesByLastUser.Add(UserMessageExact, Stamped);
}

void FVesselMockProvider::SetDefaultResponse(const FLlmResponse& Response)
{
	FScopeLock Guard(&Lock);
	DefaultResponse = Response;
	if (DefaultResponse.ProviderId.IsEmpty())
	{
		DefaultResponse.ProviderId = TEXT("mock");
	}
}

void FVesselMockProvider::ClearFixtures()
{
	FScopeLock Guard(&Lock);
	FixturesByLastUser.Empty();
	CallCount = 0;
}

int32 FVesselMockProvider::GetCallCount() const
{
	FScopeLock Guard(&Lock);
	return CallCount;
}

FString FVesselMockProvider::ExtractLastUserContent(const FLlmRequest& Request)
{
	for (int32 i = Request.Messages.Num() - 1; i >= 0; --i)
	{
		if (Request.Messages[i].Role == EVesselLlmRole::User)
		{
			return Request.Messages[i].Content;
		}
	}
	return FString();
}

TFuture<FLlmResponse> FVesselMockProvider::SendAsync(const FLlmRequest& Request)
{
	FLlmResponse Picked;
	{
		FScopeLock Guard(&Lock);
		++CallCount;

		const FString LastUser = ExtractLastUserContent(Request);
		if (const FLlmResponse* Hit = FixturesByLastUser.Find(LastUser))
		{
			Picked = *Hit;
		}
		else
		{
			Picked = DefaultResponse;
		}
	}

	// Return a pre-completed future — mock is synchronous.
	TPromise<FLlmResponse> Promise;
	TFuture<FLlmResponse> Future = Promise.GetFuture();
	Promise.SetValue(MoveTemp(Picked));
	return Future;
}
