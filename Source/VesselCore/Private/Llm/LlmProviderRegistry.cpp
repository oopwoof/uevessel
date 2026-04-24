// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Llm/LlmProviderRegistry.h"
#include "VesselLog.h"

FLlmProviderRegistry& FLlmProviderRegistry::Get()
{
	static FLlmProviderRegistry Instance;
	return Instance;
}

void FLlmProviderRegistry::RegisterProvider(const TSharedRef<ILlmProvider>& Provider)
{
	const FString Id = Provider->GetProviderId();
	if (Id.IsEmpty())
	{
		UE_LOG(LogVesselLlm, Error, TEXT("Refusing to register provider with empty id"));
		return;
	}

	FRWScopeLock WLock(Lock, SLT_Write);
	Providers.Add(Id, Provider);
	UE_LOG(LogVesselLlm, Log, TEXT("Registered LLM provider: %s"), *Id);
}

void FLlmProviderRegistry::InjectMock(const TSharedRef<ILlmProvider>& Mock)
{
	RegisterProvider(Mock);
}

TSharedPtr<ILlmProvider> FLlmProviderRegistry::FindProvider(const FString& Id) const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	if (const TSharedRef<ILlmProvider>* Found = Providers.Find(Id))
	{
		return TSharedPtr<ILlmProvider>(*Found);
	}
	return nullptr;
}

TArray<FString> FLlmProviderRegistry::ListProviderIds() const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	TArray<FString> Ids;
	Providers.GetKeys(Ids);
	return Ids;
}

void FLlmProviderRegistry::ClearAll()
{
	FRWScopeLock WLock(Lock, SLT_Write);
	Providers.Empty();
}
