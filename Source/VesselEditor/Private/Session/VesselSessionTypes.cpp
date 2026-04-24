// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselSessionTypes.h"

const TCHAR* SessionStateToString(EVesselSessionState State)
{
	switch (State)
	{
		case EVesselSessionState::Idle:          return TEXT("Idle");
		case EVesselSessionState::Planning:      return TEXT("Planning");
		case EVesselSessionState::ToolSelection: return TEXT("ToolSelection");
		case EVesselSessionState::Executing:     return TEXT("Executing");
		case EVesselSessionState::JudgeReview:   return TEXT("JudgeReview");
		case EVesselSessionState::NextStep:      return TEXT("NextStep");
		case EVesselSessionState::Done:          return TEXT("Done");
		case EVesselSessionState::Failed:        return TEXT("Failed");
	}
	return TEXT("Unknown");
}

const TCHAR* SessionOutcomeKindToString(EVesselSessionOutcomeKind Kind)
{
	switch (Kind)
	{
		case EVesselSessionOutcomeKind::Pending:              return TEXT("Pending");
		case EVesselSessionOutcomeKind::Done:                 return TEXT("Done");
		case EVesselSessionOutcomeKind::Failed:               return TEXT("Failed");
		case EVesselSessionOutcomeKind::AbortedOnEditorClose: return TEXT("AbortedOnEditorClose");
		case EVesselSessionOutcomeKind::AbortedByUser:        return TEXT("AbortedByUser");
	}
	return TEXT("Unknown");
}

const TCHAR* JudgeDecisionToString(EVesselJudgeDecision Decision)
{
	switch (Decision)
	{
		case EVesselJudgeDecision::Approve: return TEXT("Approve");
		case EVesselJudgeDecision::Revise:  return TEXT("Revise");
		case EVesselJudgeDecision::Reject:  return TEXT("Reject");
	}
	return TEXT("Unknown");
}
