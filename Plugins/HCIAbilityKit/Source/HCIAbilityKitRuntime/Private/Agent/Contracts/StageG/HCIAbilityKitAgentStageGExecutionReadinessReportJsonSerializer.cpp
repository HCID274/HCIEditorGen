#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer.h"

#include "Agent/Contracts/StageF/HCIAbilityKitAgentApplyRequest.h"
#include "Agent/Contracts/StageG/HCIAbilityKitAgentStageGExecutionReadinessReport.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FHCIAbilityKitAgentStageGExecutionReadinessReportJsonSerializer::SerializeToJsonString(
	const FHCIAbilityKitAgentStageGExecutionReadinessReport& Report,
	FString& OutJsonText)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Report.RequestId);
	Root->SetStringField(TEXT("stage_g_execute_archive_bundle_id"), Report.StageGExecuteArchiveBundleId);
	Root->SetStringField(TEXT("selection_digest"), Report.SelectionDigest);
	Root->SetStringField(TEXT("stage_g_execution_readiness_digest"), Report.StageGExecutionReadinessDigest);
	Root->SetStringField(TEXT("stage_g_execution_readiness_status"), Report.StageGExecutionReadinessStatus);
	Root->SetBoolField(TEXT("ready_for_h1_planner_integration"), Report.bReadyForH1PlannerIntegration);
	Root->SetStringField(TEXT("execution_mode"), Report.ExecutionMode);
	Root->SetStringField(TEXT("error_code"), Report.ErrorCode);
	Root->SetStringField(TEXT("reason"), Report.Reason);

	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("total_rows"), Report.Summary.TotalRows);
	Summary->SetNumberField(TEXT("selected_rows"), Report.Summary.SelectedRows);
	Summary->SetNumberField(TEXT("modifiable_rows"), Report.Summary.ModifiableRows);
	Summary->SetNumberField(TEXT("blocked_rows"), Report.Summary.BlockedRows);
	Summary->SetNumberField(TEXT("read_only_rows"), Report.Summary.ReadOnlyRows);
	Summary->SetNumberField(TEXT("write_rows"), Report.Summary.WriteRows);
	Summary->SetNumberField(TEXT("destructive_rows"), Report.Summary.DestructiveRows);
	Root->SetObjectField(TEXT("summary"), Summary);

	TArray<TSharedPtr<FJsonValue>> ItemsJson;
	ItemsJson.Reserve(Report.Items.Num());
	for (const FHCIAbilityKitAgentApplyRequestItem& Item : Report.Items)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("row_index"), Item.RowIndex);
		Obj->SetStringField(TEXT("tool_name"), Item.ToolName);
		Obj->SetStringField(TEXT("risk"), FHCIAbilityKitDryRunDiff::RiskToString(Item.Risk));
		Obj->SetStringField(TEXT("asset_path"), Item.AssetPath);
		Obj->SetStringField(TEXT("field"), Item.Field);
		Obj->SetStringField(TEXT("skip_reason"), Item.SkipReason);
		Obj->SetBoolField(TEXT("blocked"), Item.bBlocked);
		Obj->SetStringField(TEXT("object_type"), FHCIAbilityKitDryRunDiff::ObjectTypeToString(Item.ObjectType));
		Obj->SetStringField(TEXT("locate_strategy"), FHCIAbilityKitDryRunDiff::LocateStrategyToString(Item.LocateStrategy));
		Obj->SetStringField(TEXT("evidence_key"), Item.EvidenceKey);
		Obj->SetStringField(TEXT("actor_path"), Item.ActorPath);
		ItemsJson.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("items"), ItemsJson);

	OutJsonText.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Root, Writer);
}
