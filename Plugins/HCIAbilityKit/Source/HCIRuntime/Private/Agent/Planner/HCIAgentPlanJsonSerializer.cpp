#include "Agent/Planner/HCIAgentPlanJsonSerializer.h"

#include "Agent/Planner/HCIAgentPlan.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAgentPlanJsonSerializer::SerializeToJsonString(const FHCIAgentPlan& Plan, FString& OutJsonText)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("plan_version"), Plan.PlanVersion);
	Root->SetStringField(TEXT("request_id"), Plan.RequestId);
	Root->SetStringField(TEXT("intent"), Plan.Intent);
	if (!Plan.AssistantMessage.IsEmpty())
	{
		Root->SetStringField(TEXT("assistant_message"), Plan.AssistantMessage);
	}

	TArray<TSharedPtr<FJsonValue>> StepsJson;
	StepsJson.Reserve(Plan.Steps.Num());
	for (const FHCIAgentPlanStep& Step : Plan.Steps)
	{
		TSharedRef<FJsonObject> StepObject = MakeShared<FJsonObject>();
		StepObject->SetStringField(TEXT("step_id"), Step.StepId);
		StepObject->SetStringField(TEXT("tool_name"), Step.ToolName.ToString());
		StepObject->SetObjectField(TEXT("args"), Step.Args.IsValid() ? Step.Args.ToSharedRef() : MakeShared<FJsonObject>());
		StepObject->SetStringField(TEXT("risk_level"), FHCIAgentPlanContract::RiskLevelToString(Step.RiskLevel));
		StepObject->SetBoolField(TEXT("requires_confirm"), Step.bRequiresConfirm);
		StepObject->SetStringField(TEXT("rollback_strategy"), Step.RollbackStrategy);

		TArray<TSharedPtr<FJsonValue>> EvidenceJson;
		EvidenceJson.Reserve(Step.ExpectedEvidence.Num());
		for (const FString& EvidenceField : Step.ExpectedEvidence)
		{
			EvidenceJson.Add(MakeShared<FJsonValueString>(EvidenceField));
		}
		StepObject->SetArrayField(TEXT("expected_evidence"), EvidenceJson);

		if (Step.UiPresentation.HasAnyField())
		{
			TSharedRef<FJsonObject> UiObject = MakeShared<FJsonObject>();
			if (Step.UiPresentation.bHasStepSummary)
			{
				UiObject->SetStringField(TEXT("step_summary"), Step.UiPresentation.StepSummary);
			}
			if (Step.UiPresentation.bHasIntentReason)
			{
				UiObject->SetStringField(TEXT("intent_reason"), Step.UiPresentation.IntentReason);
			}
			if (Step.UiPresentation.bHasRiskWarning)
			{
				UiObject->SetStringField(TEXT("risk_warning"), Step.UiPresentation.RiskWarning);
			}
			StepObject->SetObjectField(TEXT("ui_presentation"), UiObject);
		}

		StepsJson.Add(MakeShared<FJsonValueObject>(StepObject));
	}
	Root->SetArrayField(TEXT("steps"), StepsJson);

	OutJsonText.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}

