// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Llm/ILlmProvider.h"

/**
 * Lightweight process-global registry of LLM providers by id.
 * Populated during VesselCore::StartupModule. Tests can InjectMock / ClearAll.
 *
 * Read path is hot (planner + judge queries) so we use FRWLock — writers are
 * rare (startup + test fixtures).
 */
class VESSELCORE_API FLlmProviderRegistry
{
public:
	static FLlmProviderRegistry& Get();

	void RegisterProvider(const TSharedRef<ILlmProvider>& Provider);

	/** Alias of RegisterProvider with clearer intent for test setup. */
	void InjectMock(const TSharedRef<ILlmProvider>& Mock);

	TSharedPtr<ILlmProvider> FindProvider(const FString& Id) const;

	TArray<FString> ListProviderIds() const;

	/** Test-only: drop all registered providers. Production code must not call this. */
	void ClearAll();

private:
	FLlmProviderRegistry() = default;

	mutable FRWLock Lock;
	TMap<FString, TSharedRef<ILlmProvider>> Providers;
};
