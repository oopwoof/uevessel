// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Registry/VesselToolSchema.h"

#if WITH_EDITOR
class FScopedTransaction;
#endif

/**
 * RAII wrapper that wraps a Vessel tool invocation in `FScopedTransaction`
 * when (and only when) the schema's policy warrants it.
 *
 * Policy (mirrors docs/engineering/ARCHITECTURE.md §2.2):
 *   - bIrreversibleHint=true          → NO transaction (would be false safety)
 *   - Category contains "Write"       → transaction
 *   - bRequiresApproval=true          → transaction
 *   - Otherwise                       → no transaction (pure reads)
 *
 * IMPORTANT: a `FScopedTransaction` only records UObject state changes where
 * `Target->Modify()` was called. Tool implementations are still responsible
 * for that call — see TOOL_REGISTRY.md §4 "Tool author responsibility".
 *
 * The scope does nothing outside `WITH_EDITOR` (runtime builds don't have
 * the transaction system).
 */
class VESSELCORE_API FVesselTransactionScope
{
public:
	/** Open a transaction for this tool invocation, if policy demands. */
	explicit FVesselTransactionScope(const FVesselToolSchema& Schema, const FString& SessionId = FString());

	/** Closes the transaction; changes recorded via Modify() become undoable. */
	~FVesselTransactionScope();

	/** True if a transaction is currently open. */
	bool IsActive() const { return bActive; }

	/** Non-copyable, non-movable — this is a strict RAII guard. */
	FVesselTransactionScope(const FVesselTransactionScope&) = delete;
	FVesselTransactionScope& operator=(const FVesselTransactionScope&) = delete;
	FVesselTransactionScope(FVesselTransactionScope&&) = delete;
	FVesselTransactionScope& operator=(FVesselTransactionScope&&) = delete;

	/** Pure predicate — exposed for unit tests so we can assert the policy. */
	static bool ShouldOpenTransactionFor(const FVesselToolSchema& Schema);

private:
	bool bActive = false;

#if WITH_EDITOR
	TUniquePtr<FScopedTransaction> Transaction;
#endif
};
