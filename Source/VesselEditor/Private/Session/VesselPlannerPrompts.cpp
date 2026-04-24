// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselPlannerPrompts.h"

#include "VesselLog.h"
#include "Util/VesselJsonSanitizer.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace VesselPromptDetail
{
	/** Category prefix match — e.g. "Meta" allows "Meta", "Meta/Read". */
	static bool CategoryAllowed(const FString& ToolCategory, const TArray<FString>& AllowList)
	{
		if (AllowList.Num() == 0)
		{
			return true; // empty allow list → everything allowed
		}
		for (const FString& Entry : AllowList)
		{
			if (ToolCategory.Equals(Entry, ESearchCase::IgnoreCase))
			{
				return true;
			}
			if (ToolCategory.StartsWith(Entry + TEXT("/"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static TArray<FVesselToolSchema> FilterTools(
		const TArray<FVesselToolSchema>& All,
		const FVesselAgentTemplate& Agent)
	{
		TArray<FVesselToolSchema> Out;
		Out.Reserve(All.Num());
		for (const FVesselToolSchema& S : All)
		{
			if (!CategoryAllowed(S.Category, Agent.AllowedCategories))
			{
				continue;
			}
			if (Agent.DeniedTools.Contains(S.Name.ToString()))
			{
				continue;
			}
			Out.Add(S);
		}
		return Out;
	}

	static FString RenderSchemasAsJsonArray(const TArray<FVesselToolSchema>& Schemas)
	{
		FString Body;
		bool bFirst = true;
		for (const FVesselToolSchema& S : Schemas)
		{
			if (!bFirst)
			{
				Body += TEXT(",");
			}
			bFirst = false;

			TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"),        S.Name.ToString());
			Obj->SetStringField(TEXT("category"),    S.Category);
			Obj->SetStringField(TEXT("description"), S.Description);
			Obj->SetBoolField(  TEXT("requires_approval"), S.bRequiresApproval);

			TArray<TSharedPtr<FJsonValue>> ParamArr;
			for (const FVesselParameterSchema& P : S.Parameters)
			{
				TSharedRef<FJsonObject> ParamObj = MakeShared<FJsonObject>();
				ParamObj->SetStringField(TEXT("name"), P.Name.ToString());
				ParamObj->SetStringField(TEXT("schema"), P.TypeJson);
				ParamObj->SetBoolField(  TEXT("required"), P.bRequired);
				ParamArr.Add(MakeShared<FJsonValueObject>(ParamObj));
			}
			Obj->SetArrayField(TEXT("parameters"), ParamArr);

			FString ObjStr;
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ObjStr);
			FJsonSerializer::Serialize(Obj, Writer);
			Body += ObjStr;
		}
		return FString::Printf(TEXT("[%s]"), *Body);
	}

	static FString SerializeCondensed(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}
}

FLlmRequest FVesselPlannerPrompts::BuildPlanningRequest(
	const FVesselSessionConfig& Config,
	const FString& UserInput,
	const TArray<FVesselToolSchema>& AvailableTools,
	const FString& ReviseDirective)
{
	const TArray<FVesselToolSchema> Filtered =
		VesselPromptDetail::FilterTools(AvailableTools, Config.AgentTemplate);
	const FString ToolsJson = VesselPromptDetail::RenderSchemasAsJsonArray(Filtered);

	FString SystemText;
	SystemText += Config.AgentTemplate.SystemPrompt;
	SystemText += TEXT("\n\n");
	SystemText += TEXT("## Available tools (machine-readable schemas)\n");
	SystemText += ToolsJson;
	SystemText += TEXT("\n\n");
	SystemText += TEXT("## Output contract\n");
	SystemText += TEXT("Respond with EXACTLY this JSON object, and NOTHING else (no markdown fence, no prose):\n");
	SystemText += TEXT("{\n");
	SystemText += TEXT("  \"plan\": [\n");
	SystemText += TEXT("    {\"tool\": \"<tool name>\", \"args\": { ... }, \"reasoning\": \"<why this step>\"}\n");
	SystemText += TEXT("  ]\n");
	SystemText += TEXT("}\n\n");
	SystemText += TEXT("If the user request cannot be satisfied with the available tools, return an empty plan: {\"plan\":[]}.\n");
	SystemText += TEXT("Keep steps minimal. Prefer a one-step plan when the user's goal needs one tool call.\n");

	if (!ReviseDirective.IsEmpty())
	{
		SystemText += TEXT("\n## Previous attempt was rejected by the judge. Follow this directive:\n");
		SystemText += ReviseDirective;
	}

	FLlmRequest R;
	R.Model = Config.PlannerModel;
	R.Messages.Add(FLlmMessage{ EVesselLlmRole::System, SystemText, FString() });
	R.Messages.Add(FLlmMessage{ EVesselLlmRole::User,   UserInput,   FString() });
	R.MaxTokens = 4096;
	R.Temperature = 0.2f;
	return R;
}

FLlmRequest FVesselPlannerPrompts::BuildJudgeRequest(
	const FVesselSessionConfig& Config,
	const FVesselPlanStep& ExecutedStep,
	const FString& ToolResultJson)
{
	TSharedRef<FJsonObject> StepObj = MakeShared<FJsonObject>();
	StepObj->SetStringField(TEXT("tool"), ExecutedStep.ToolName.ToString());
	StepObj->SetStringField(TEXT("args"), ExecutedStep.ArgsJson);
	StepObj->SetStringField(TEXT("reasoning"), ExecutedStep.Reasoning);
	StepObj->SetNumberField(TEXT("step_index"), ExecutedStep.StepIndex);
	const FString StepJson = VesselPromptDetail::SerializeCondensed(StepObj);

	FString SystemText;
	SystemText += TEXT("You are the Judge for a Vessel agent step. Evaluate the executed tool call ");
	SystemText += TEXT("against the user's intent and these rules:\n\n");
	SystemText += Config.AgentTemplate.JudgeRubric;
	SystemText += TEXT("\n\n## Output contract\n");
	SystemText += TEXT("Respond with EXACTLY this JSON object (no prose, no markdown fence):\n");
	SystemText += TEXT("{\n");
	SystemText += TEXT("  \"decision\": \"approve\" | \"revise\" | \"reject\",\n");
	SystemText += TEXT("  \"reasoning\": \"<one sentence>\",\n");
	SystemText += TEXT("  \"revise_directive\": \"<only when decision=revise>\",\n");
	SystemText += TEXT("  \"reject_reason\": \"<only when decision=reject>\"\n");
	SystemText += TEXT("}\n");

	FString UserText;
	UserText += TEXT("Executed step:\n");
	UserText += StepJson;
	UserText += TEXT("\n\nTool result (JSON):\n");
	UserText += ToolResultJson;

	FLlmRequest R;
	R.Model = Config.JudgeModel.IsEmpty() ? Config.PlannerModel : Config.JudgeModel;
	R.Messages.Add(FLlmMessage{ EVesselLlmRole::System, SystemText, FString() });
	R.Messages.Add(FLlmMessage{ EVesselLlmRole::User,   UserText,   FString() });
	R.MaxTokens = 1024;
	R.Temperature = 0.0f;
	return R;
}

FVesselPlan FVesselPlannerPrompts::ParsePlanResponse(const FLlmResponse& Response)
{
	FVesselPlan Plan;
	Plan.RawLlmResponse = Response.Content;
	Plan.bValid = false;

	if (!Response.bOk)
	{
		Plan.ErrorMessage = FString::Printf(TEXT("Planner LLM failed: %s"), *Response.ErrorMessage);
		return Plan;
	}

	TSharedPtr<FJsonObject> Root;
	if (!FVesselJsonSanitizer::ParseAsObject(Response.Content, Root) || !Root.IsValid())
	{
		Plan.ErrorMessage =
			TEXT("Planner response is not a JSON object. Return ONLY a JSON object with a 'plan' field.");
		return Plan;
	}

	const TArray<TSharedPtr<FJsonValue>>* StepsArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("plan"), StepsArr) || !StepsArr)
	{
		Plan.ErrorMessage = TEXT("Missing 'plan' field (must be a JSON array).");
		return Plan;
	}

	int32 Index = 1;
	for (const TSharedPtr<FJsonValue>& V : *StepsArr)
	{
		const TSharedPtr<FJsonObject>* StepObj = nullptr;
		if (!V.IsValid() || !V->TryGetObject(StepObj) || !StepObj || !StepObj->IsValid())
		{
			Plan.ErrorMessage = FString::Printf(TEXT("Plan step %d is not a JSON object."), Index);
			return Plan;
		}

		FVesselPlanStep Step;
		Step.StepIndex = Index++;

		FString ToolStr;
		if (!(*StepObj)->TryGetStringField(TEXT("tool"), ToolStr) || ToolStr.IsEmpty())
		{
			Plan.ErrorMessage = FString::Printf(TEXT("Plan step %d missing 'tool' field."), Step.StepIndex);
			return Plan;
		}
		Step.ToolName = FName(*ToolStr);

		// Args: extract as JSON string for the Invoker to re-parse.
		const TSharedPtr<FJsonObject>* ArgsObj = nullptr;
		if ((*StepObj)->TryGetObjectField(TEXT("args"), ArgsObj) && ArgsObj && ArgsObj->IsValid())
		{
			Step.ArgsJson = VesselPromptDetail::SerializeCondensed(ArgsObj->ToSharedRef());
		}
		else
		{
			Step.ArgsJson = TEXT("{}");
		}

		(*StepObj)->TryGetStringField(TEXT("reasoning"), Step.Reasoning);

		Plan.Steps.Add(MoveTemp(Step));
	}

	Plan.bValid = true;
	return Plan;
}

FVesselJudgeVerdict FVesselPlannerPrompts::ParseJudgeResponse(const FLlmResponse& Response)
{
	FVesselJudgeVerdict V;

	if (!Response.bOk)
	{
		V.Decision = EVesselJudgeDecision::Reject;
		V.Reasoning = FString::Printf(TEXT("Judge LLM call failed: %s"), *Response.ErrorMessage);
		V.RejectReason = V.Reasoning;
		return V;
	}

	TSharedPtr<FJsonObject> Root;
	if (!FVesselJsonSanitizer::ParseAsObject(Response.Content, Root) || !Root.IsValid())
	{
		V.Decision = EVesselJudgeDecision::Reject;
		V.Reasoning = TEXT("Judge response was not valid JSON. Defaulting to Reject for safety.");
		V.RejectReason = V.Reasoning;
		return V;
	}

	FString DecisionStr;
	if (!Root->TryGetStringField(TEXT("decision"), DecisionStr))
	{
		V.Decision = EVesselJudgeDecision::Reject;
		V.Reasoning = TEXT("Judge response missing 'decision' field.");
		V.RejectReason = V.Reasoning;
		return V;
	}

	const FString Normalized = DecisionStr.ToLower();
	if (Normalized == TEXT("approve"))
	{
		V.Decision = EVesselJudgeDecision::Approve;
	}
	else if (Normalized == TEXT("revise"))
	{
		V.Decision = EVesselJudgeDecision::Revise;
	}
	else
	{
		V.Decision = EVesselJudgeDecision::Reject;
	}

	Root->TryGetStringField(TEXT("reasoning"),        V.Reasoning);
	Root->TryGetStringField(TEXT("revise_directive"), V.ReviseDirective);
	Root->TryGetStringField(TEXT("reject_reason"),    V.RejectReason);

	if (V.Decision == EVesselJudgeDecision::Reject && V.RejectReason.IsEmpty())
	{
		V.RejectReason = V.Reasoning;
	}
	return V;
}
