#include "Agent/HCIAbilityKitAgentPlanJsonSerializer.h"

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentPlanJsonSerializer::SerializeToJsonString(const FHCIAbilityKitAgentPlan& Plan, FString& OutJsonText)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("plan_version"), Plan.PlanVersion);
	Root->SetStringField(TEXT("request_id"), Plan.RequestId);
	Root->SetStringField(TEXT("intent"), Plan.Intent);

	TArray<TSharedPtr<FJsonValue>> StepsJson;
	StepsJson.Reserve(Plan.Steps.Num());
	for (const FHCIAbilityKitAgentPlanStep& Step : Plan.Steps)
	{
		TSharedRef<FJsonObject> StepObject = MakeShared<FJsonObject>();
		StepObject->SetStringField(TEXT("step_id"), Step.StepId);
		StepObject->SetStringField(TEXT("tool_name"), Step.ToolName.ToString());
		StepObject->SetObjectField(TEXT("args"), Step.Args.IsValid() ? Step.Args.ToSharedRef() : MakeShared<FJsonObject>());
		StepObject->SetStringField(TEXT("risk_level"), FHCIAbilityKitAgentPlanContract::RiskLevelToString(Step.RiskLevel));
		StepObject->SetBoolField(TEXT("requires_confirm"), Step.bRequiresConfirm);
		StepObject->SetStringField(TEXT("rollback_strategy"), Step.RollbackStrategy);

		TArray<TSharedPtr<FJsonValue>> EvidenceJson;
		EvidenceJson.Reserve(Step.ExpectedEvidence.Num());
		for (const FString& EvidenceField : Step.ExpectedEvidence)
		{
			EvidenceJson.Add(MakeShared<FJsonValueString>(EvidenceField));
		}
		StepObject->SetArrayField(TEXT("expected_evidence"), EvidenceJson);

		StepsJson.Add(MakeShared<FJsonValueObject>(StepObject));
	}
	Root->SetArrayField(TEXT("steps"), StepsJson);

	OutJsonText.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}

