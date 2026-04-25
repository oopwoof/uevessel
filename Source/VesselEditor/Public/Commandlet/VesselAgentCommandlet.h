// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VesselAgentCommandlet.generated.h"

/**
 * Headless Vessel session driver. Lets a Vessel agent run inside CI / nightly
 * pipelines without requiring the editor UI.
 *
 * Invocation (from a CI job):
 *   UnrealEditor-Cmd.exe <project>.uproject -run=VesselAgent
 *     -prompt="Validate /Game/Maps and report any validator errors."
 *     -agent=asset-pipeline
 *     -policy=approve            (or "reject")
 *     -timeout=600                (seconds, default 1800)
 *
 * Exit codes:
 *   0 — session ended Done
 *   1 — session ended Failed / Aborted (any non-Done outcome)
 *   2 — bad arguments (missing -prompt, invalid -policy, etc.)
 *
 * stdout: a single JSON object summarising the outcome
 *   {
 *     "outcome":"Done"|"Failed"|...,
 *     "reason":"...",
 *     "steps_executed":N,
 *     "cost_usd_est":F,
 *     "wall_ms":I,
 *     "session_log":"<absolute jsonl path>"
 *   }
 *
 * Approval policy:
 *   approve — auto-approve every HITL gate. Use for read-only or
 *             trusted-write workflows; never for unattended writes
 *             into shared assets.
 *   reject  — auto-reject every HITL gate. Useful for "read-only
 *             dry-run" sweeps where any write attempt should fail
 *             the CI job.
 */
UCLASS()
class VESSELEDITOR_API UVesselAgentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVesselAgentCommandlet();

	virtual int32 Main(const FString& Params) override;
};
