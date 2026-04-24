// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Fixtures/VesselTestToolFixture.h"

FString UVesselTestToolFixture::FixtureRead(const FString& Path, const TArray<FString>& Keys, int32 Limit)
{
	// Echo args into a JSON string so the ToolInvoker end-to-end test can
	// verify parameter marshalling round-tripped correctly.
	FString KeysJoined;
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		if (i > 0) KeysJoined += TEXT(",");
		KeysJoined += Keys[i];
	}
	return FString::Printf(
		TEXT("{\"path\":\"%s\",\"keys\":\"%s\",\"limit\":%d}"),
		*Path, *KeysJoined, Limit);
}

bool UVesselTestToolFixture::FixtureIrreversibleWrite(const FString& /*Target*/)
{
	return true;
}

int32 UVesselTestToolFixture::NotAnAgentTool(int32 X)
{
	return X;
}
