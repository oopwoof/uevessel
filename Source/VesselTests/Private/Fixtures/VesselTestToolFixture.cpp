// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Fixtures/VesselTestToolFixture.h"

FString UVesselTestToolFixture::FixtureRead(const FString& /*Path*/, const TArray<FString>& /*Keys*/, int32 /*Limit*/)
{
	return TEXT("{}");
}

bool UVesselTestToolFixture::FixtureIrreversibleWrite(const FString& /*Target*/)
{
	return true;
}

int32 UVesselTestToolFixture::NotAnAgentTool(int32 X)
{
	return X;
}
