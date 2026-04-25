// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Loads project-level agent guides for injection into the Planner system
 * prompt. This closes the "Reject → AGENTS.md → next session avoids it"
 * loop that Vessel's Guides+Sensors thesis depends on:
 *
 *   1. HITL Gate appends a Reject entry to <Project>/AGENTS.md
 *      under "## Known Rejections" (FVesselRejectionSink).
 *   2. Next session's BuildPlanningRequest calls FVesselGuidesLoader,
 *      which reads AGENTS.md and packs (a) the user-edited preamble
 *      and (b) a compacted list of recent rejections into a single
 *      block.
 *   3. Planner sees prior rejections and avoids them.
 *
 * Without this loader, AGENTS.md is write-only and the loop is broken.
 */
class VESSELEDITOR_API FVesselGuidesLoader
{
public:
	/**
	 * Build the full guides block for injection into the Planner system
	 * prompt. Returns empty string if the project has no AGENTS.md or it
	 * is empty.
	 *
	 * Format (LLM-readable):
	 *   ## Project guides
	 *   <user preamble — everything above "## Known Rejections">
	 *
	 *   ## Past rejections (avoid these)
	 *   - tool=Foo on /Game/X: reason
	 *   - tool=Bar on /Game/Y: reason
	 *   ...
	 *
	 * @param MaxRejections  Cap on rejection entries (most recent first).
	 *                       Older entries silently dropped to bound prompt size.
	 * @param MaxPreambleChars Cap on user-preamble length. Excess truncated.
	 */
	static FString BuildProjectGuidesBlock(
		int32 MaxRejections = 20,
		int32 MaxPreambleChars = 2000);

	// --- Internals exposed for tests ---

	/** Resolve <Project>/AGENTS.md absolute path. */
	static FString GetAgentsMdPath();

	/** Read AGENTS.md fully; returns empty string if missing. */
	static FString ReadAgentsMd();

	/**
	 * Split AGENTS.md text into (preamble, rejections-section).
	 * Preamble = everything above "## Known Rejections" header.
	 * Rejections section = the header line + everything after.
	 * Either may be empty if AGENTS.md hasn't been touched yet.
	 */
	static void SplitPreambleAndRejections(
		const FString& Full,
		FString& OutPreamble,
		FString& OutRejectionsSection);

	/**
	 * Parse the "## Known Rejections" section, extract tool / target /
	 * reason triples, drop noise (timestamp, session, rejecter), return
	 * up to MaxEntries entries in chronological order (oldest first), so
	 * caller can decide whether to take the tail (most recent first).
	 */
	struct FRejectionEntry
	{
		FString Tool;
		FString Target;
		FString Reason;
	};
	static TArray<FRejectionEntry> ParseRejections(
		const FString& RejectionsSection,
		int32 MaxEntries = INT32_MAX);
};
