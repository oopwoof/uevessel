// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Transaction/VesselTransactionScope.h"

#include "VesselLog.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

bool FVesselTransactionScope::ShouldOpenTransactionFor(const FVesselToolSchema& Schema)
{
	// Irreversible wins over everything — opening a transaction for file
	// deletions or remote HTTP effects gives false safety.
	if (Schema.bIrreversibleHint)
	{
		return false;
	}

	if (Schema.bRequiresApproval)
	{
		return true;
	}

	if (Schema.Category.Contains(TEXT("Write"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}

FVesselTransactionScope::FVesselTransactionScope(const FVesselToolSchema& Schema, const FString& SessionId)
{
	if (!ShouldOpenTransactionFor(Schema))
	{
		return;
	}

#if WITH_EDITOR
	// Description is shown in the Edit > Undo History menu.
	const FText Description = FText::FromString(
		FString::Printf(TEXT("Vessel · %s%s%s"),
			*Schema.Name.ToString(),
			SessionId.IsEmpty() ? TEXT("") : TEXT(" · "),
			*SessionId));

	Transaction = MakeUnique<FScopedTransaction>(Description);
	bActive = true;

	UE_LOG(LogVesselSession, Verbose,
		TEXT("TransactionScope opened for %s (session=%s)"),
		*Schema.Name.ToString(), *SessionId);
#endif
}

FVesselTransactionScope::~FVesselTransactionScope()
{
#if WITH_EDITOR
	if (bActive)
	{
		UE_LOG(LogVesselSession, Verbose, TEXT("TransactionScope closing."));
	}
	Transaction.Reset();
#endif
	bActive = false;
}
