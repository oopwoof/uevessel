// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Settings/VesselAuth.h"
#include "Settings/VesselUserSettings.h"
#include "Settings/VesselProjectSettings.h"
#include "HAL/PlatformMisc.h"

FString FVesselAuth::GetEnvOrEmpty(const TCHAR* VarName)
{
	return FPlatformMisc::GetEnvironmentVariable(VarName);
}

FString FVesselAuth::GetAnthropicApiKey()
{
	const FString FromEnv = GetEnvOrEmpty(TEXT("VESSEL_ANTHROPIC_API_KEY"));
	if (!FromEnv.IsEmpty())
	{
		return FromEnv;
	}
	return UVesselUserSettings::GetRef().AnthropicApiKey;
}

FString FVesselAuth::GetGatewayAuthorization()
{
	const FString FromEnv = GetEnvOrEmpty(TEXT("VESSEL_GATEWAY_TOKEN"));
	if (!FromEnv.IsEmpty())
	{
		return FromEnv;
	}
	return UVesselUserSettings::GetRef().GatewayAuthorization;
}

FString FVesselAuth::GetAzureApiKey()
{
	const FString FromEnv = GetEnvOrEmpty(TEXT("VESSEL_AZURE_API_KEY"));
	if (!FromEnv.IsEmpty())
	{
		return FromEnv;
	}
	return UVesselUserSettings::GetRef().AzureApiKey;
}

bool FVesselAuth::IsEndpointPermitted(const FString& Endpoint)
{
	if (Endpoint.IsEmpty())
	{
		return true;
	}
	if (Endpoint.StartsWith(TEXT("https://"), ESearchCase::CaseSensitive))
	{
		return true;
	}
	if (!Endpoint.StartsWith(TEXT("http://"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	const bool bLocalhost =
		Endpoint.StartsWith(TEXT("http://localhost"), ESearchCase::CaseSensitive)
		|| Endpoint.StartsWith(TEXT("http://127.0.0.1"), ESearchCase::CaseSensitive);
	if (!bLocalhost)
	{
		return false;
	}

	return UVesselProjectSettings::GetRef().bAllowHttp;
}

FString FVesselAuth::Redact(const FString& Secret)
{
	if (Secret.IsEmpty())
	{
		return TEXT("<empty>");
	}
	return FString::Printf(TEXT("<redacted:%d>"), Secret.Len());
}
