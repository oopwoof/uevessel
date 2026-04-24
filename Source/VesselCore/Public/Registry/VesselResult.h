// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Canonical Vessel error codes. Values are stable across Vessel releases and
 * match the semantics documented in docs/engineering/TOOL_REGISTRY.md §5.1.
 *
 * Adding a new code REQUIRES updating that doc first (see CONTRIBUTING §3).
 */
enum class EVesselResultCode : uint8
{
	Ok = 0,
	ValidationError,   // Arg validation rejected by Registry, before the tool body ran
	NotFound,          // Tool / asset / row missing
	IoError,           // File or network IO failed and is retry-worthy
	Timeout,           // Tool exceeded its declared ToolTimeoutSec
	PermissionDenied,  // Tool Policy blocked the call (project-level deny list)
	Internal,          // Tool body threw or otherwise corrupted state — do NOT retry
	UserRejected,      // HITL Gate received a reject decision from the user
};

VESSELCORE_API const TCHAR* VesselResultCodeToString(EVesselResultCode Code);

/**
 * Result type returned by Vessel tools and pipeline stages.
 *
 * Errors carry an actionable message. "invalid args" is a bad Message —
 * "Parameter RowNames expected array<string>, received string 'foo'" is good.
 * See TOOL_REGISTRY.md §5.4.
 */
template <typename T>
struct FVesselResult
{
	bool bOk = false;
	EVesselResultCode Code = EVesselResultCode::Internal;
	FString Message;
	T Value{};

	static FVesselResult<T> Ok(T InValue)
	{
		FVesselResult<T> R;
		R.bOk = true;
		R.Code = EVesselResultCode::Ok;
		R.Value = MoveTemp(InValue);
		return R;
	}

	static FVesselResult<T> Err(EVesselResultCode InCode, FString InMessage)
	{
		FVesselResult<T> R;
		R.bOk = false;
		R.Code = InCode;
		R.Message = MoveTemp(InMessage);
		return R;
	}
};

/**
 * Void-valued variant (no payload on success). Use when a pipeline stage
 * succeeds/fails but has nothing to return.
 */
struct VESSELCORE_API FVesselVoidResult
{
	bool bOk = true;
	EVesselResultCode Code = EVesselResultCode::Ok;
	FString Message;

	static FVesselVoidResult Ok()
	{
		return FVesselVoidResult{};
	}

	static FVesselVoidResult Err(EVesselResultCode InCode, FString InMessage)
	{
		FVesselVoidResult R;
		R.bOk = false;
		R.Code = InCode;
		R.Message = MoveTemp(InMessage);
		return R;
	}
};
