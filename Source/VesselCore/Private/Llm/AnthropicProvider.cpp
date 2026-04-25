// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Llm/AnthropicProvider.h"

#include "VesselLog.h"
#include "Llm/VesselLlmPricing.h"
#include "Settings/VesselAuth.h"
#include "Settings/VesselProjectSettings.h"
#include "Settings/VesselUserSettings.h"
#include "Util/VesselJsonSanitizer.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
	const TCHAR* kPublicEndpoint = TEXT("https://api.anthropic.com/v1/messages");
	const TCHAR* kApiVersion     = TEXT("2023-06-01");
}

FString FAnthropicProvider::ResolveEndpoint(const FString& Override)
{
	if (!Override.IsEmpty())
	{
		return Override;
	}
	const FString FromProject = UVesselProjectSettings::GetRef().Endpoint;
	return FromProject.IsEmpty() ? FString(kPublicEndpoint) : FromProject;
}

FString FAnthropicProvider::BuildRequestBodyJson(const FLlmRequest& Request)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), Request.Model);
	Root->SetNumberField(TEXT("max_tokens"), Request.MaxTokens);
	Root->SetNumberField(TEXT("temperature"), Request.Temperature);

	TArray<TSharedPtr<FJsonValue>> Messages;
	FString SystemText;
	for (const FLlmMessage& M : Request.Messages)
	{
		if (M.Role == EVesselLlmRole::System)
		{
			// Anthropic takes system as a top-level field, not in messages.
			if (!SystemText.IsEmpty())
			{
				SystemText += TEXT("\n\n");
			}
			SystemText += M.Content;
			continue;
		}
		TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
		switch (M.Role)
		{
			case EVesselLlmRole::User:      Msg->SetStringField(TEXT("role"), TEXT("user"));      break;
			case EVesselLlmRole::Assistant: Msg->SetStringField(TEXT("role"), TEXT("assistant")); break;
			case EVesselLlmRole::Tool:      Msg->SetStringField(TEXT("role"), TEXT("user"));      break; // TODO(step4): tool_result content blocks
			default:                        Msg->SetStringField(TEXT("role"), TEXT("user"));      break;
		}
		Msg->SetStringField(TEXT("content"), M.Content);
		Messages.Add(MakeShared<FJsonValueObject>(Msg));
	}
	Root->SetArrayField(TEXT("messages"), Messages);
	if (!SystemText.IsEmpty())
	{
		Root->SetStringField(TEXT("system"), SystemText);
	}

	// TODO(step4): serialize Request.Tools into Anthropic tool_use schema.
	// For v0.1 scaffold we only round-trip messages.

	FString Body;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

FLlmResponse FAnthropicProvider::ParseResponse(int32 HttpCode, const FString& Body)
{
	FLlmResponse Out;
	Out.ProviderId = TEXT("anthropic");

	if (HttpCode == 401 || HttpCode == 403)
	{
		return FLlmResponse::MakeError(EVesselLlmErrorCode::Unauthorized,
			FString::Printf(TEXT("Anthropic returned %d — check API key."), HttpCode));
	}
	if (HttpCode == 429)
	{
		return FLlmResponse::MakeError(EVesselLlmErrorCode::RateLimited,
			TEXT("Anthropic rate-limited the request."));
	}
	if (HttpCode >= 500 || HttpCode == 0)
	{
		return FLlmResponse::MakeError(EVesselLlmErrorCode::HttpError,
			FString::Printf(TEXT("Anthropic HTTP %d."), HttpCode));
	}

	TSharedPtr<FJsonObject> Root;
	if (!FVesselJsonSanitizer::ParseAsObject(Body, Root) || !Root.IsValid())
	{
		return FLlmResponse::MakeError(EVesselLlmErrorCode::MalformedResponse,
			TEXT("Response body is not a parseable JSON object."));
	}

	// Extract text content blocks.
	const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
	if (Root->TryGetArrayField(TEXT("content"), Content) && Content)
	{
		FString Joined;
		for (const TSharedPtr<FJsonValue>& V : *Content)
		{
			const TSharedPtr<FJsonObject>* Block = nullptr;
			if (V.IsValid() && V->TryGetObject(Block) && Block && Block->IsValid())
			{
				FString BlockType;
				if ((*Block)->TryGetStringField(TEXT("type"), BlockType) && BlockType == TEXT("text"))
				{
					FString Text;
					(*Block)->TryGetStringField(TEXT("text"), Text);
					Joined += Text;
				}
				// TODO(step4): handle tool_use blocks → populate Out.ToolCalls
			}
		}
		Out.Content = Joined;
	}

	// Anthropic returns the served model id at top level — use that for
	// pricing rather than the request's preferred id (the two can differ
	// briefly during a model rotation).
	FString ServedModel;
	Root->TryGetStringField(TEXT("model"), ServedModel);

	const TSharedPtr<FJsonObject>* UsageObj = nullptr;
	if (Root->TryGetObjectField(TEXT("usage"), UsageObj) && UsageObj && UsageObj->IsValid())
	{
		int32 In = 0, OutTok = 0, CacheRead = 0, CacheCreate = 0;
		(*UsageObj)->TryGetNumberField(TEXT("input_tokens"),                In);
		(*UsageObj)->TryGetNumberField(TEXT("output_tokens"),               OutTok);
		(*UsageObj)->TryGetNumberField(TEXT("cache_read_input_tokens"),     CacheRead);
		(*UsageObj)->TryGetNumberField(TEXT("cache_creation_input_tokens"), CacheCreate);
		Out.Usage.InputTokens         = In;
		Out.Usage.OutputTokens        = OutTok;
		Out.Usage.CacheReadTokens     = CacheRead;
		Out.Usage.CacheCreationTokens = CacheCreate;
		Out.Usage.EstimatedCostUsd    = FVesselLlmPricing::EstimateCostUsd(
			ServedModel, In, OutTok, CacheRead, CacheCreate);
	}

	Out.bOk = true;
	Out.ErrorCode = EVesselLlmErrorCode::None;
	return Out;
}

TFuture<FLlmResponse> FAnthropicProvider::SendAsync(const FLlmRequest& Request)
{
	TSharedRef<TPromise<FLlmResponse>> Promise = MakeShared<TPromise<FLlmResponse>>();
	TFuture<FLlmResponse> Future = Promise->GetFuture();

	const FString ApiKey = FVesselAuth::GetAnthropicApiKey();
	if (ApiKey.IsEmpty())
	{
		Promise->SetValue(FLlmResponse::MakeError(
			EVesselLlmErrorCode::ConfigError,
			TEXT("Anthropic API key not set. Configure in Editor Preferences → Vessel "
			     "or export VESSEL_ANTHROPIC_API_KEY.")));
		return Future;
	}

	const FString EffectiveEndpoint = ResolveEndpoint(Request.EndpointOverride);
	if (!FVesselAuth::IsEndpointPermitted(EffectiveEndpoint))
	{
		Promise->SetValue(FLlmResponse::MakeError(
			EVesselLlmErrorCode::ConfigError,
			FString::Printf(TEXT("Endpoint '%s' is not permitted — non-localhost HTTP is always blocked."),
				*EffectiveEndpoint)));
		return Future;
	}

	const FString EffectiveModel = Request.Model.IsEmpty()
		? UVesselProjectSettings::GetRef().Model
		: Request.Model;

	FLlmRequest Effective = Request;
	Effective.Model = EffectiveModel;
	const FString Body = BuildRequestBodyJson(Effective);

	UE_LOG(LogVesselLlm, Verbose, TEXT("Anthropic POST %s model=%s key=%s body_len=%d"),
		*EffectiveEndpoint,
		*EffectiveModel,
		*FVesselAuth::Redact(ApiKey),
		Body.Len());

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Http = FHttpModule::Get().CreateRequest();
	Http->SetURL(EffectiveEndpoint);
	Http->SetVerb(TEXT("POST"));
	Http->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Http->SetHeader(TEXT("x-api-key"), ApiKey);
	Http->SetHeader(TEXT("anthropic-version"), kApiVersion);
	for (const TPair<FString, FString>& H : UVesselProjectSettings::GetRef().NonSecretHeaders)
	{
		Http->SetHeader(H.Key, H.Value);
	}
	Http->SetContentAsString(Body);

	Http->OnProcessRequestComplete().BindLambda(
		[Promise](FHttpRequestPtr /*Req*/, FHttpResponsePtr Res, bool bSuccess)
		{
			if (!bSuccess || !Res.IsValid())
			{
				Promise->SetValue(FLlmResponse::MakeError(
					EVesselLlmErrorCode::HttpError,
					TEXT("HTTP request failed before a response was returned.")));
				return;
			}
			Promise->SetValue(ParseResponse(Res->GetResponseCode(), Res->GetContentAsString()));
		});

	Http->ProcessRequest();
	return Future;
}
