// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Plain structs (no UStruct / reflection) describing LLM I/O shapes.
 * Reflection is reserved for tool meta (see TOOL_REGISTRY.md); LLM plumbing
 * stays POD to keep VesselCore lean and Blueprint-free.
 */

enum class EVesselLlmRole : uint8
{
	System,
	User,
	Assistant,
	Tool,
};

struct FLlmMessage
{
	EVesselLlmRole Role = EVesselLlmRole::User;
	FString Content;
	/** For Role=Tool, the tool call id this message responds to. */
	FString ToolCallId;
};

struct FLlmToolSchema
{
	FString Name;
	FString Description;
	/** JSON-schema object describing parameters. Already sanitized; do not embed markdown. */
	FString ParametersJson;
};

struct FLlmToolCall
{
	FString Id;
	FString ToolName;
	/** JSON object string. Consumers should run through FVesselJsonSanitizer before parsing. */
	FString ArgsJson;
};

struct FLlmRequest
{
	FString Model;
	TArray<FLlmMessage> Messages;
	TArray<FLlmToolSchema> Tools;
	int32 MaxTokens = 4096;
	float Temperature = 0.7f;
	/** Optional per-request override of the provider endpoint (env overrides too). */
	FString EndpointOverride;
};

enum class EVesselLlmErrorCode : uint8
{
	None,
	ConfigError,
	HttpError,
	Timeout,
	Unauthorized,
	RateLimited,
	MalformedResponse,
	Internal,
};

struct FLlmUsage
{
	int32 InputTokens = 0;
	int32 OutputTokens = 0;
	int32 CacheReadTokens = 0;
	int32 CacheCreationTokens = 0;
	float EstimatedCostUsd = 0.0f;
};

struct FLlmResponse
{
	bool bOk = false;
	EVesselLlmErrorCode ErrorCode = EVesselLlmErrorCode::None;
	FString ErrorMessage;

	/** Assistant-role text content (may be empty if the model only issued tool calls). */
	FString Content;
	TArray<FLlmToolCall> ToolCalls;
	FLlmUsage Usage;

	/** Provider-id that produced this response. "mock" / "anthropic" / ... */
	FString ProviderId;

	static FLlmResponse MakeError(EVesselLlmErrorCode Code, const FString& Message)
	{
		FLlmResponse R;
		R.bOk = false;
		R.ErrorCode = Code;
		R.ErrorMessage = Message;
		return R;
	}
};
