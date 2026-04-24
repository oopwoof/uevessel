// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Session/VesselApprovalTypes.h"

/**
 * Persists reject-reasons so the next Planner turn learns from them.
 * See HITL_PROTOCOL.md §4 and AGENTS.md §7 Known Rejections.
 *
 * Two outputs per reject:
 *   1. `<Project>/AGENTS.md` — human-readable, appended to an auto-managed
 *      "## Known Rejections" section. Created if the file does not exist.
 *   2. `<Project>/Saved/VesselRejectionArchive/<yyyy-mm>.jsonl` — structured
 *      monthly archive for analytics / replay.
 *
 * Both outputs are best-effort. If either IO path fails, the session still
 * proceeds as Failed per the FSM; the sink logs a warning but does not
 * throw (exceptions are disabled in VesselCore / VesselEditor).
 */
class VESSELEDITOR_API FVesselRejectionSink
{
public:
	/**
	 * Persist a reject reason to AGENTS.md + the monthly JSONL archive.
	 * Returns true if at least one of the two destinations succeeded.
	 */
	static bool Record(
		const FVesselApprovalRequest& Request,
		const FVesselApprovalDecision& Decision);

	/** AGENTS.md target path (project root). */
	static FString GetAgentsMdPath();

	/** Archive path for the current month. */
	static FString GetArchivePathForMonth(const FDateTime& UtcNow);

	/** Parse a full AGENTS.md blob and return true if it contains the managed header. */
	static bool AgentsMdHasRejectionsSection(const FString& Contents);

private:
	static bool AppendToAgentsMd(
		const FVesselApprovalRequest& Request,
		const FVesselApprovalDecision& Decision);
	static bool AppendToArchive(
		const FVesselApprovalRequest& Request,
		const FVesselApprovalDecision& Decision);

	/** Human-readable block that appears in AGENTS.md for one entry. */
	static FString FormatAgentsMdEntry(
		const FVesselApprovalRequest& Request,
		const FVesselApprovalDecision& Decision);
};
