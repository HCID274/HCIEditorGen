#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitDryRunDiff.h"
#include "Agent/HCIAbilityKitDryRunDiffJsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitDryRunDiffNormalizeLocateStrategyTest,
	"HCIAbilityKit.Editor.AgentDryRun.NormalizeLocateStrategy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitDryRunDiffNormalizeLocateStrategyTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_test_e2_001");

	FHCIAbilityKitDryRunDiffItem& AssetItem = Report.DiffItems.Emplace_GetRef();
	AssetItem.AssetPath = TEXT("/Game/HCI/Data/TestSkill.TestSkill");
	AssetItem.Field = TEXT("max_texture_size");
	AssetItem.Before = TEXT("4096");
	AssetItem.After = TEXT("1024");
	AssetItem.ToolName = TEXT("SetTextureMaxSize");
	AssetItem.Risk = EHCIAbilityKitDryRunRisk::Write;

	FHCIAbilityKitDryRunDiffItem& ActorItem = Report.DiffItems.Emplace_GetRef();
	ActorItem.AssetPath = TEXT("/Game/Maps/TestMap.TestMap:PersistentLevel.StaticMeshActor_1");
	ActorItem.ActorPath = ActorItem.AssetPath;
	ActorItem.Field = TEXT("collision_state");
	ActorItem.Before = TEXT("missing");
	ActorItem.After = TEXT("would_fix");
	ActorItem.ToolName = TEXT("ScanLevelMeshRisks");
	ActorItem.Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	ActorItem.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
	ActorItem.SkipReason = TEXT("read_only_scan_preview");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);

	TestEqual(TEXT("total_candidates should match diff item count"), Report.Summary.TotalCandidates, 2);
	TestEqual(TEXT("modifiable count should exclude skipped item"), Report.Summary.Modifiable, 1);
	TestEqual(TEXT("skipped count should include skipped item"), Report.Summary.Skipped, 1);

	TestEqual(
		TEXT("Asset item should use sync_browser locate strategy"),
		FHCIAbilityKitDryRunDiff::LocateStrategyToString(AssetItem.LocateStrategy),
		FString(TEXT("sync_browser")));
	TestEqual(TEXT("Asset item evidence key default"), AssetItem.EvidenceKey, FString(TEXT("asset_path")));

	TestEqual(
		TEXT("Actor item should use camera_focus locate strategy"),
		FHCIAbilityKitDryRunDiff::LocateStrategyToString(ActorItem.LocateStrategy),
		FString(TEXT("camera_focus")));
	TestEqual(TEXT("Actor item evidence key default"), ActorItem.EvidenceKey, FString(TEXT("actor_path")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitDryRunDiffJsonSerializerCoreFieldsTest,
	"HCIAbilityKit.Editor.AgentDryRun.JsonSerializerIncludesCoreFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitDryRunDiffJsonSerializerCoreFieldsTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitDryRunDiffReport Report;
	Report.RequestId = TEXT("req_test_e2_002");

	FHCIAbilityKitDryRunDiffItem& Item = Report.DiffItems.Emplace_GetRef();
	Item.AssetPath = TEXT("/Game/Textures/T_Tree_01_D.T_Tree_01_D");
	Item.Field = TEXT("max_texture_size");
	Item.Before = TEXT("4096");
	Item.After = TEXT("1024");
	Item.ToolName = TEXT("SetTextureMaxSize");
	Item.Risk = EHCIAbilityKitDryRunRisk::Write;
	Item.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	Item.EvidenceKey = TEXT("asset_path");

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(Report);

	FString JsonText;
	const bool bOk = FHCIAbilityKitDryRunDiffJsonSerializer::SerializeToJsonString(Report, JsonText);
	TestTrue(TEXT("SerializeToJsonString should succeed"), bOk);
	if (!bOk)
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TestTrue(TEXT("Serialized JSON should parse"), FJsonSerializer::Deserialize(Reader, RootObject));
	if (!RootObject.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("request_id should match"), RootObject->GetStringField(TEXT("request_id")), FString(TEXT("req_test_e2_002")));

	const TSharedPtr<FJsonObject>* SummaryObject = nullptr;
	TestTrue(TEXT("summary object should exist"), RootObject->TryGetObjectField(TEXT("summary"), SummaryObject));
	if (!SummaryObject || !SummaryObject->IsValid())
	{
		return false;
	}
	TestEqual(TEXT("total_candidates should be 1"), static_cast<int32>((*SummaryObject)->GetNumberField(TEXT("total_candidates"))), 1);

	const TArray<TSharedPtr<FJsonValue>>* DiffItems = nullptr;
	TestTrue(TEXT("diff_items array should exist"), RootObject->TryGetArrayField(TEXT("diff_items"), DiffItems));
	if (!DiffItems || DiffItems->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ItemObject = nullptr;
	TestTrue(TEXT("diff_items[0] should be object"), (*DiffItems)[0]->TryGetObject(ItemObject));
	if (!ItemObject || !ItemObject->IsValid())
	{
		return false;
	}

	TestEqual(TEXT("asset_path should match"), (*ItemObject)->GetStringField(TEXT("asset_path")), FString(TEXT("/Game/Textures/T_Tree_01_D.T_Tree_01_D")));
	TestEqual(TEXT("tool_name should match"), (*ItemObject)->GetStringField(TEXT("tool_name")), FString(TEXT("SetTextureMaxSize")));
	TestEqual(TEXT("risk should match"), (*ItemObject)->GetStringField(TEXT("risk")), FString(TEXT("write")));
	TestEqual(TEXT("locate_strategy should match"), (*ItemObject)->GetStringField(TEXT("locate_strategy")), FString(TEXT("sync_browser")));
	TestEqual(TEXT("evidence_key should match"), (*ItemObject)->GetStringField(TEXT("evidence_key")), FString(TEXT("asset_path")));
	return true;
}

#endif
