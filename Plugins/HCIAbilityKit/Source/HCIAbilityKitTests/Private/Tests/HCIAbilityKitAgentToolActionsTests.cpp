#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"
#include "AgentActions/HCIAbilityKitAgentToolActions.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"

namespace
{
static bool HCI_TryDuplicateProbeAsset(const FString& TargetAssetPath)
{
	const TCHAR* CandidateSources[] = {
		TEXT("/Engine/BasicShapes/Cube"),
		TEXT("/Engine/EngineMeshes/Cube")};

	for (const TCHAR* SourceAssetPath : CandidateSources)
	{
		if (UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
		{
			if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath, TargetAssetPath))
			{
				return true;
			}
		}
	}
	return false;
}

static bool HCI_TryDuplicateProbeTexture(const FString& TargetAssetPath)
{
	const TCHAR* CandidateSources[] = {
		TEXT("/Engine/EngineResources/DefaultTexture"),
		TEXT("/Engine/EngineResources/WhiteSquareTexture"),
		TEXT("/Engine/EngineMaterials/DefaultDiffuse")};

	for (const TCHAR* SourceAssetPath : CandidateSources)
	{
		if (UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
		{
			if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath, TargetAssetPath))
			{
				return true;
			}
		}
	}
	return false;
}

static void HCI_DeleteAssetIfExists(const FString& AssetPath)
{
	if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		UEditorAssetLibrary::DeleteAsset(AssetPath);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSearchPathFuzzyMatchTest,
	"HCIAbilityKit.Editor.AgentTools.SearchPathFuzzyMatchesUnderscoreDirectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitSearchPathFuzzyMatchTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/Test_SearchPathFuzzy");
	const FString ProbeDir = TEXT("/Game/__HCI_Auto/Test_SearchPathFuzzy/M_New");
	const FString ProbeAssetPath = TEXT("/Game/__HCI_Auto/Test_SearchPathFuzzy/M_New/SM_HCI_SearchProbe");

	if (UEditorAssetLibrary::DoesAssetExist(ProbeAssetPath))
	{
		UEditorAssetLibrary::DeleteAsset(ProbeAssetPath);
	}
	UEditorAssetLibrary::DeleteDirectory(ProbeDir);
	UEditorAssetLibrary::MakeDirectory(ProbeDir);
	if (!HCI_TryDuplicateProbeAsset(ProbeAssetPath))
	{
		UEditorAssetLibrary::DeleteDirectory(ProbeDir);
		AddError(TEXT("Failed to create probe asset for SearchPath fuzzy matching test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* SearchAction = Actions.Find(TEXT("SearchPath"));
	if (SearchAction == nullptr || !SearchAction->IsValid())
	{
		UEditorAssetLibrary::DeleteAsset(ProbeAssetPath);
		UEditorAssetLibrary::DeleteDirectory(ProbeDir);
		AddError(TEXT("SearchPath action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_searchpath_fuzzy");
	Request.StepId = TEXT("step_search");
	Request.ToolName = TEXT("SearchPath");
	Request.Args = MakeShared<FJsonObject>();
	Request.Args->SetStringField(TEXT("keyword"), TEXT("mnew"));

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*SearchAction)->DryRun(Request, Result);

	if (UEditorAssetLibrary::DoesAssetExist(ProbeAssetPath))
	{
		UEditorAssetLibrary::DeleteAsset(ProbeAssetPath);
	}
	UEditorAssetLibrary::DeleteDirectory(ProbeDir);
	UEditorAssetLibrary::DeleteDirectory(RootDir);

	TestTrue(TEXT("SearchPath dry-run call should succeed"), bCallOk);
	TestTrue(TEXT("SearchPath result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("SearchPath reason"), Result.Reason, FString(TEXT("search_path_ok")));

	const FString MatchedDirectories = Result.Evidence.FindRef(TEXT("matched_directories"));
	TestFalse(TEXT("matched_directories should not be empty"), MatchedDirectories.IsEmpty());

	TArray<FString> MatchedList;
	MatchedDirectories.ParseIntoArray(MatchedList, TEXT("|"), true);
	TestTrue(
		TEXT("Keyword 'mnew' should fuzzy-match '/Game/__HCI_Auto/M_New'"),
		MatchedList.ContainsByPredicate(
			[&ProbeDir](const FString& Candidate)
			{
				return Candidate.Equals(ProbeDir, ESearchCase::IgnoreCase);
			}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSearchPathSemanticKeywordAliasTest,
	"HCIAbilityKit.Editor.AgentTools.SearchPathSemanticKeywordAliasMatchesTempDirectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitSearchPathSemanticKeywordAliasTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/Test_SearchPathAlias");
	const FString ProbeDir = TEXT("/Game/__HCI_Auto/Test_SearchPathAlias/Temp_AliasCase");
	const FString ProbeAssetPath = TEXT("/Game/__HCI_Auto/Test_SearchPathAlias/Temp_AliasCase/SM_HCI_SearchAliasProbe");

	HCI_DeleteAssetIfExists(ProbeAssetPath);
	UEditorAssetLibrary::DeleteDirectory(ProbeDir);
	UEditorAssetLibrary::MakeDirectory(ProbeDir);
	if (!HCI_TryDuplicateProbeAsset(ProbeAssetPath))
	{
		UEditorAssetLibrary::DeleteDirectory(ProbeDir);
		AddError(TEXT("Failed to create probe asset for SearchPath semantic keyword alias test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* SearchAction = Actions.Find(TEXT("SearchPath"));
	if (SearchAction == nullptr || !SearchAction->IsValid())
	{
		HCI_DeleteAssetIfExists(ProbeAssetPath);
		UEditorAssetLibrary::DeleteDirectory(ProbeDir);
		AddError(TEXT("SearchPath action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_searchpath_alias");
	Request.StepId = TEXT("step_search_alias");
	Request.ToolName = TEXT("SearchPath");
	Request.Args = MakeShared<FJsonObject>();
	Request.Args->SetStringField(TEXT("keyword"), TEXT("临时目录"));

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*SearchAction)->DryRun(Request, Result);

	HCI_DeleteAssetIfExists(ProbeAssetPath);
	UEditorAssetLibrary::DeleteDirectory(ProbeDir);
	UEditorAssetLibrary::DeleteDirectory(RootDir);

	TestTrue(TEXT("SearchPath dry-run call should succeed"), bCallOk);
	TestTrue(TEXT("SearchPath result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("SearchPath reason"), Result.Reason, FString(TEXT("search_path_ok")));

	const FString MatchedDirectories = Result.Evidence.FindRef(TEXT("matched_directories"));
	TestFalse(TEXT("matched_directories should not be empty"), MatchedDirectories.IsEmpty());
	TestTrue(
		TEXT("expanded keyword evidence should include Temp"),
		Result.Evidence.FindRef(TEXT("keyword_expanded")).Contains(TEXT("Temp")));
	TestTrue(
		TEXT("semantic_fallback_used evidence should exist"),
		Result.Evidence.Contains(TEXT("semantic_fallback_used")));
	TestTrue(
		TEXT("best_directory should not be '-'"),
		!Result.Evidence.FindRef(TEXT("best_directory")).Equals(TEXT("-"), ESearchCase::CaseSensitive));

	TArray<FString> MatchedList;
	MatchedDirectories.ParseIntoArray(MatchedList, TEXT("|"), true);
	TestTrue(
		TEXT("Keyword '临时目录' should semantic-match '/Game/__HCI_Auto/Temp_AliasCase'"),
		MatchedList.ContainsByPredicate(
			[&ProbeDir](const FString& Candidate)
			{
				return Candidate.Equals(ProbeDir, ESearchCase::IgnoreCase);
			}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitRenameAssetExecuteTest,
	"HCIAbilityKit.Editor.AgentTools.RenameAssetExecuteRenamesRealAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitScanLevelMeshRisksSelectedWithoutSelectionFailsTest,
	"HCIAbilityKit.Editor.AgentTools.ScanLevelMeshRisksSelectedWithoutSelectionFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSetTextureMaxSizeExecuteTest,
	"HCIAbilityKit.Editor.AgentTools.SetTextureMaxSizeExecuteModifiesRealTexture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSetMeshLODGroupExecuteTest,
	"HCIAbilityKit.Editor.AgentTools.SetMeshLODGroupExecuteModifiesRealStaticMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitSetMeshLODGroupNaniteBlockedTest,
	"HCIAbilityKit.Editor.AgentTools.SetMeshLODGroupExecuteBlocksNaniteMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitScanMeshTriangleCountDryRunTest,
	"HCIAbilityKit.Editor.AgentTools.ScanMeshTriangleCountDryRunReturnsMeshTriangleEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitNormalizeNamingDryRunTest,
	"HCIAbilityKit.Editor.AgentTools.NormalizeAssetNamingByMetadataDryRunBuildsProposals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitNormalizeNamingExecuteTest,
	"HCIAbilityKit.Editor.AgentTools.NormalizeAssetNamingByMetadataExecuteRenamesAndMovesAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitScanLevelMeshRisksSelectedWithoutSelectionFailsTest::RunTest(const FString& Parameters)
{
	if (GEditor != nullptr)
	{
		GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true, /*WarnAboutManyActors*/ false);
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* ScanAction = Actions.Find(TEXT("ScanLevelMeshRisks"));
	if (ScanAction == nullptr || !ScanAction->IsValid())
	{
		AddError(TEXT("ScanLevelMeshRisks action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_level_risk_selected_empty");
	Request.StepId = TEXT("step_level_risk_selected_empty");
	Request.ToolName = TEXT("ScanLevelMeshRisks");
	Request.Args = MakeShared<FJsonObject>();
	Request.Args->SetStringField(TEXT("scope"), TEXT("selected"));
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("missing_collision")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("default_material")));
	Request.Args->SetArrayField(TEXT("checks"), Checks);
	Request.Args->SetNumberField(TEXT("max_actor_count"), 500);

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*ScanAction)->DryRun(Request, Result);

	TestFalse(TEXT("ScanLevelMeshRisks selected scan should fail when no actors are selected"), bCallOk);
	TestFalse(TEXT("Result should be marked as failed"), Result.bSucceeded);
	TestEqual(TEXT("ErrorCode"), Result.ErrorCode, FString(TEXT("no_actors_selected")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("no_actors_selected")));
	return true;
}

bool FHCIAbilityKitSetTextureMaxSizeExecuteTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/J2_Texture");
	const FString TextureAssetPath = TEXT("/Game/__HCI_Auto/J2_Texture/T_J2_TextureSrc");
	const FString TextureObjectPath = TEXT("/Game/__HCI_Auto/J2_Texture/T_J2_TextureSrc.T_J2_TextureSrc");

	HCI_DeleteAssetIfExists(TextureAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	UEditorAssetLibrary::MakeDirectory(RootDir);
	if (!HCI_TryDuplicateProbeTexture(TextureAssetPath))
	{
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to prepare probe texture asset for SetTextureMaxSize execute test."));
		return false;
	}

	UTexture2D* TextureAsset = LoadObject<UTexture2D>(nullptr, *TextureObjectPath);
	if (!TextureAsset)
	{
		HCI_DeleteAssetIfExists(TextureAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to load probe texture asset."));
		return false;
	}
	if (TextureAsset->MaxTextureSize == 1024)
	{
		TextureAsset->Modify();
		TextureAsset->MaxTextureSize = 512;
		TextureAsset->PostEditChange();
		UEditorAssetLibrary::SaveAsset(TextureAssetPath, false);
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* TextureAction = Actions.Find(TEXT("SetTextureMaxSize"));
	if (TextureAction == nullptr || !TextureAction->IsValid())
	{
		HCI_DeleteAssetIfExists(TextureAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("SetTextureMaxSize action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_j2_texture_execute");
	Request.StepId = TEXT("step_texture");
	Request.ToolName = TEXT("SetTextureMaxSize");
	Request.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TextureObjectPath));
	Request.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Request.Args->SetNumberField(TEXT("max_size"), 1024);

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*TextureAction)->Execute(Request, Result);

	TextureAsset = LoadObject<UTexture2D>(nullptr, *TextureObjectPath);
	TestTrue(TEXT("SetTextureMaxSize execute call should succeed"), bCallOk);
	TestTrue(TEXT("SetTextureMaxSize result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("SetTextureMaxSize reason"), Result.Reason, FString(TEXT("set_texture_max_size_execute_ok")));
	TestEqual(TEXT("Texture MaxTextureSize should be updated to 1024"), TextureAsset ? TextureAsset->MaxTextureSize : -1, 1024);
	TestEqual(TEXT("modified_count evidence"), Result.Evidence.FindRef(TEXT("modified_count")), FString(TEXT("1")));

	HCI_DeleteAssetIfExists(TextureAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	return true;
}

bool FHCIAbilityKitSetMeshLODGroupExecuteTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/J2_LOD");
	const FString MeshAssetPath = TEXT("/Game/__HCI_Auto/J2_LOD/SM_J2_LODSrc");
	const FString MeshObjectPath = TEXT("/Game/__HCI_Auto/J2_LOD/SM_J2_LODSrc.SM_J2_LODSrc");

	HCI_DeleteAssetIfExists(MeshAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	UEditorAssetLibrary::MakeDirectory(RootDir);
	if (!HCI_TryDuplicateProbeAsset(MeshAssetPath))
	{
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to prepare probe static mesh for SetMeshLODGroup execute test."));
		return false;
	}

	UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, *MeshObjectPath);
	if (!MeshAsset)
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to load probe static mesh asset."));
		return false;
	}
	MeshAsset->Modify();
	MeshAsset->NaniteSettings.bEnabled = false;
	MeshAsset->LODGroup = TEXT("SmallProp");
	MeshAsset->PostEditChange();
	UEditorAssetLibrary::SaveAsset(MeshAssetPath, false);

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* LodAction = Actions.Find(TEXT("SetMeshLODGroup"));
	if (LodAction == nullptr || !LodAction->IsValid())
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("SetMeshLODGroup action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_j2_lod_execute");
	Request.StepId = TEXT("step_lod");
	Request.ToolName = TEXT("SetMeshLODGroup");
	Request.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(MeshObjectPath));
	Request.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Request.Args->SetStringField(TEXT("lod_group"), TEXT("LevelArchitecture"));

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*LodAction)->Execute(Request, Result);

	MeshAsset = LoadObject<UStaticMesh>(nullptr, *MeshObjectPath);
	TestTrue(TEXT("SetMeshLODGroup execute call should succeed"), bCallOk);
	TestTrue(TEXT("SetMeshLODGroup result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("SetMeshLODGroup reason"), Result.Reason, FString(TEXT("set_mesh_lod_group_execute_ok")));
	TestEqual(TEXT("StaticMesh LODGroup should be updated"), MeshAsset ? MeshAsset->LODGroup : FName(), FName(TEXT("LevelArchitecture")));
	TestEqual(TEXT("modified_count evidence"), Result.Evidence.FindRef(TEXT("modified_count")), FString(TEXT("1")));

	HCI_DeleteAssetIfExists(MeshAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	return true;
}

bool FHCIAbilityKitScanMeshTriangleCountDryRunTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/J4_TriangleScan");
	const FString MeshAssetPath = TEXT("/Game/__HCI_Auto/J4_TriangleScan/SM_J4_TriangleProbe");
	const FString TextureAssetPath = TEXT("/Game/__HCI_Auto/J4_TriangleScan/T_J4_TriangleProbe");

	HCI_DeleteAssetIfExists(MeshAssetPath);
	HCI_DeleteAssetIfExists(TextureAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	UEditorAssetLibrary::MakeDirectory(RootDir);
	if (!HCI_TryDuplicateProbeAsset(MeshAssetPath) || !HCI_TryDuplicateProbeTexture(TextureAssetPath))
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		HCI_DeleteAssetIfExists(TextureAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to prepare probe assets for ScanMeshTriangleCount dry-run test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* ScanAction = Actions.Find(TEXT("ScanMeshTriangleCount"));
	if (ScanAction == nullptr || !ScanAction->IsValid())
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		HCI_DeleteAssetIfExists(TextureAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("ScanMeshTriangleCount action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_j4_triangle_scan");
	Request.StepId = TEXT("step_triangle_scan");
	Request.ToolName = TEXT("ScanMeshTriangleCount");
	Request.Args = MakeShared<FJsonObject>();
	Request.Args->SetStringField(TEXT("directory"), RootDir);

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*ScanAction)->DryRun(Request, Result);

	TestTrue(TEXT("ScanMeshTriangleCount dry-run call should succeed"), bCallOk);
	TestTrue(TEXT("ScanMeshTriangleCount result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("ScanMeshTriangleCount reason"), Result.Reason, FString(TEXT("scan_mesh_triangle_count_ok")));
	TestEqual(TEXT("scan_root evidence"), Result.Evidence.FindRef(TEXT("scan_root")), RootDir);
	TestTrue(
		TEXT("scanned_count should include created mesh+texture"),
		FCString::Atoi(*Result.Evidence.FindRef(TEXT("scanned_count"))) >= 2);
	TestEqual(TEXT("mesh_count evidence"), Result.Evidence.FindRef(TEXT("mesh_count")), FString(TEXT("1")));
	TestTrue(
		TEXT("max_triangle_count should be positive"),
		FCString::Atoi(*Result.Evidence.FindRef(TEXT("max_triangle_count"))) > 0);
	TestTrue(
		TEXT("top_meshes should include probe mesh path"),
		Result.Evidence.FindRef(TEXT("top_meshes")).Contains(TEXT("SM_J4_TriangleProbe")));

	HCI_DeleteAssetIfExists(MeshAssetPath);
	HCI_DeleteAssetIfExists(TextureAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	return true;
}

bool FHCIAbilityKitNormalizeNamingDryRunTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = TEXT("/Game/__HCI_Auto/J3_DryRun");
	const FString TargetRoot = TEXT("/Game/__HCI_Auto/J3_DryRun_Organized");
	const FString MeshAssetPath = TEXT("/Game/__HCI_Auto/J3_DryRun/RawMeshJ3");
	const FString MeshObjectPath = TEXT("/Game/__HCI_Auto/J3_DryRun/RawMeshJ3.RawMeshJ3");

	HCI_DeleteAssetIfExists(MeshAssetPath);
	UEditorAssetLibrary::DeleteDirectory(SourceRoot);
	UEditorAssetLibrary::DeleteDirectory(TargetRoot);
	UEditorAssetLibrary::MakeDirectory(SourceRoot);
	if (!HCI_TryDuplicateProbeAsset(MeshAssetPath))
	{
		UEditorAssetLibrary::DeleteDirectory(SourceRoot);
		AddError(TEXT("Failed to prepare probe static mesh for NormalizeAssetNamingByMetadata dry-run test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* NamingAction = Actions.Find(TEXT("NormalizeAssetNamingByMetadata"));
	if (NamingAction == nullptr || !NamingAction->IsValid())
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		UEditorAssetLibrary::DeleteDirectory(SourceRoot);
		AddError(TEXT("NormalizeAssetNamingByMetadata action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_j3_naming_dryrun");
	Request.StepId = TEXT("step_naming_dryrun");
	Request.ToolName = TEXT("NormalizeAssetNamingByMetadata");
	Request.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(MeshObjectPath));
	Request.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Request.Args->SetStringField(TEXT("metadata_source"), TEXT("auto"));
	Request.Args->SetStringField(TEXT("prefix_mode"), TEXT("auto_by_asset_class"));
	Request.Args->SetStringField(TEXT("target_root"), TargetRoot);

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*NamingAction)->DryRun(Request, Result);

	TestTrue(TEXT("Normalize naming dry-run call should succeed"), bCallOk);
	TestTrue(TEXT("Normalize naming dry-run result should succeed"), Result.bSucceeded);
	TestEqual(
		TEXT("Normalize naming dry-run reason"),
		Result.Reason,
		FString(TEXT("normalize_asset_naming_by_metadata_dry_run_ok")));
	TestTrue(
		TEXT("Dry-run should produce at least one affected proposal"),
		FCString::Atoi(*Result.Evidence.FindRef(TEXT("affected_count"))) > 0);
	TestTrue(
		TEXT("proposed_renames should include class prefix"),
		Result.Evidence.FindRef(TEXT("proposed_renames")).Contains(TEXT("SM_")));

	HCI_DeleteAssetIfExists(MeshAssetPath);
	UEditorAssetLibrary::DeleteDirectory(SourceRoot);
	UEditorAssetLibrary::DeleteDirectory(TargetRoot);
	return true;
}

bool FHCIAbilityKitNormalizeNamingExecuteTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = TEXT("/Game/__HCI_Auto/J3_Execute");
	const FString TargetRoot = TEXT("/Game/__HCI_Auto/J3_Execute_Organized");
	const FString MeshAssetPath = TEXT("/Game/__HCI_Auto/J3_Execute/RawMeshJ3");
	const FString MeshObjectPath = TEXT("/Game/__HCI_Auto/J3_Execute/RawMeshJ3.RawMeshJ3");
	const FString TextureAssetPath = TEXT("/Game/__HCI_Auto/J3_Execute/RawTextureJ3");
	const FString TextureObjectPath = TEXT("/Game/__HCI_Auto/J3_Execute/RawTextureJ3.RawTextureJ3");

	HCI_DeleteAssetIfExists(MeshAssetPath);
	HCI_DeleteAssetIfExists(TextureAssetPath);
	UEditorAssetLibrary::DeleteDirectory(SourceRoot);
	UEditorAssetLibrary::DeleteDirectory(TargetRoot);
	UEditorAssetLibrary::MakeDirectory(SourceRoot);
	if (!HCI_TryDuplicateProbeAsset(MeshAssetPath) || !HCI_TryDuplicateProbeTexture(TextureAssetPath))
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		HCI_DeleteAssetIfExists(TextureAssetPath);
		UEditorAssetLibrary::DeleteDirectory(SourceRoot);
		AddError(TEXT("Failed to prepare probe assets for NormalizeAssetNamingByMetadata execute test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* NamingAction = Actions.Find(TEXT("NormalizeAssetNamingByMetadata"));
	if (NamingAction == nullptr || !NamingAction->IsValid())
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		HCI_DeleteAssetIfExists(TextureAssetPath);
		UEditorAssetLibrary::DeleteDirectory(SourceRoot);
		AddError(TEXT("NormalizeAssetNamingByMetadata action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_j3_naming_execute");
	Request.StepId = TEXT("step_naming_execute");
	Request.ToolName = TEXT("NormalizeAssetNamingByMetadata");
	Request.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(MeshObjectPath));
	AssetPaths.Add(MakeShared<FJsonValueString>(TextureObjectPath));
	Request.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Request.Args->SetStringField(TEXT("metadata_source"), TEXT("auto"));
	Request.Args->SetStringField(TEXT("prefix_mode"), TEXT("auto_by_asset_class"));
	Request.Args->SetStringField(TEXT("target_root"), TargetRoot);

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*NamingAction)->Execute(Request, Result);
	const TArray<FString> TargetAssets = UEditorAssetLibrary::ListAssets(TargetRoot, true, false);
	const bool bHasMeshPrefix = TargetAssets.ContainsByPredicate(
		[](const FString& Path)
		{
			return Path.Contains(TEXT("/SM_"));
		});
	const bool bHasTexturePrefix = TargetAssets.ContainsByPredicate(
		[](const FString& Path)
		{
			return Path.Contains(TEXT("/T_"));
		});

	TestTrue(TEXT("Normalize naming execute call should succeed"), bCallOk);
	TestTrue(TEXT("Normalize naming execute result should succeed"), Result.bSucceeded);
	TestEqual(
		TEXT("Normalize naming execute reason"),
		Result.Reason,
		FString(TEXT("normalize_asset_naming_by_metadata_execute_ok")));
	TestTrue(
		TEXT("Execute should move at least one asset"),
		FCString::Atoi(*Result.Evidence.FindRef(TEXT("affected_count"))) > 0);
	TestTrue(TEXT("Organized root should contain a SM_ asset"), bHasMeshPrefix);
	TestTrue(TEXT("Organized root should contain a T_ asset"), bHasTexturePrefix);
	TestFalse(TEXT("Original mesh path should no longer exist"), UEditorAssetLibrary::DoesAssetExist(MeshAssetPath));
	TestFalse(TEXT("Original texture path should no longer exist"), UEditorAssetLibrary::DoesAssetExist(TextureAssetPath));

	UEditorAssetLibrary::DeleteDirectory(SourceRoot);
	UEditorAssetLibrary::DeleteDirectory(TargetRoot);
	UEditorAssetLibrary::DeleteDirectory(TEXT("/Game/__HCI_Auto"));
	return true;
}

bool FHCIAbilityKitSetMeshLODGroupNaniteBlockedTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/J2_LOD_Nanite");
	const FString MeshAssetPath = TEXT("/Game/__HCI_Auto/J2_LOD_Nanite/SM_J2_LODNanite");
	const FString MeshObjectPath = TEXT("/Game/__HCI_Auto/J2_LOD_Nanite/SM_J2_LODNanite.SM_J2_LODNanite");

	HCI_DeleteAssetIfExists(MeshAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	UEditorAssetLibrary::MakeDirectory(RootDir);
	if (!HCI_TryDuplicateProbeAsset(MeshAssetPath))
	{
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to prepare Nanite probe static mesh for SetMeshLODGroup execute test."));
		return false;
	}

	UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, *MeshObjectPath);
	if (!MeshAsset)
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("Failed to load Nanite probe static mesh asset."));
		return false;
	}

	const FName BeforeLodGroup = MeshAsset->LODGroup;
	MeshAsset->Modify();
	MeshAsset->NaniteSettings.bEnabled = true;
	MeshAsset->PostEditChange();
	UEditorAssetLibrary::SaveAsset(MeshAssetPath, false);

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* LodAction = Actions.Find(TEXT("SetMeshLODGroup"));
	if (LodAction == nullptr || !LodAction->IsValid())
	{
		HCI_DeleteAssetIfExists(MeshAssetPath);
		UEditorAssetLibrary::DeleteDirectory(RootDir);
		AddError(TEXT("SetMeshLODGroup action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_j2_lod_nanite_block");
	Request.StepId = TEXT("step_lod_nanite_block");
	Request.ToolName = TEXT("SetMeshLODGroup");
	Request.Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(MeshObjectPath));
	Request.Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Request.Args->SetStringField(TEXT("lod_group"), TEXT("LevelArchitecture"));

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*LodAction)->Execute(Request, Result);

	MeshAsset = LoadObject<UStaticMesh>(nullptr, *MeshObjectPath);
	TestFalse(TEXT("SetMeshLODGroup execute call should fail on Nanite mesh"), bCallOk);
	TestFalse(TEXT("SetMeshLODGroup result should fail on Nanite mesh"), Result.bSucceeded);
	TestEqual(TEXT("Nanite block error code"), Result.ErrorCode, FString(TEXT("E4010")));
	TestEqual(TEXT("Nanite block reason"), Result.Reason, FString(TEXT("lod_tool_nanite_enabled_blocked")));
	TestEqual(TEXT("LODGroup should remain unchanged when Nanite blocked"), MeshAsset ? MeshAsset->LODGroup : FName(), BeforeLodGroup);

	HCI_DeleteAssetIfExists(MeshAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	return true;
}

bool FHCIAbilityKitRenameAssetExecuteTest::RunTest(const FString& Parameters)
{
	const FString RootDir = TEXT("/Game/__HCI_Auto/I3_Rename");
	const FString SourceObjectPath = TEXT("/Game/__HCI_Auto/I3_Rename/SM_I3_RenameSrc.SM_I3_RenameSrc");
	const FString SourceAssetPath = TEXT("/Game/__HCI_Auto/I3_Rename/SM_I3_RenameSrc");
	const FString DestObjectPath = TEXT("/Game/__HCI_Auto/I3_Rename/SM_I3_RenameDst.SM_I3_RenameDst");
	const FString DestAssetPath = TEXT("/Game/__HCI_Auto/I3_Rename/SM_I3_RenameDst");

	HCI_DeleteAssetIfExists(SourceAssetPath);
	HCI_DeleteAssetIfExists(DestAssetPath);
	UEditorAssetLibrary::MakeDirectory(RootDir);
	if (!HCI_TryDuplicateProbeAsset(SourceAssetPath))
	{
		AddError(TEXT("Failed to prepare source asset for RenameAsset execute test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* RenameAction = Actions.Find(TEXT("RenameAsset"));
	if (RenameAction == nullptr || !RenameAction->IsValid())
	{
		HCI_DeleteAssetIfExists(SourceAssetPath);
		HCI_DeleteAssetIfExists(DestAssetPath);
		AddError(TEXT("RenameAsset action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_i3_rename_execute");
	Request.StepId = TEXT("step_rename");
	Request.ToolName = TEXT("RenameAsset");
	Request.Args = MakeShared<FJsonObject>();
	Request.Args->SetStringField(TEXT("asset_path"), SourceObjectPath);
	Request.Args->SetStringField(TEXT("new_name"), TEXT("SM_I3_RenameDst"));

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*RenameAction)->Execute(Request, Result);

	TestTrue(TEXT("Rename execute call should succeed"), bCallOk);
	TestTrue(TEXT("Rename result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("Rename reason"), Result.Reason, FString(TEXT("rename_execute_ok")));
	TestTrue(TEXT("Destination asset should exist"), UEditorAssetLibrary::DoesAssetExist(DestAssetPath));
	TestEqual(TEXT("Evidence after path"), Result.Evidence.FindRef(TEXT("after")), DestObjectPath);
	TestTrue(TEXT("Redirector fixup evidence should be recorded"), Result.Evidence.Contains(TEXT("redirector_fixup")));

	HCI_DeleteAssetIfExists(SourceAssetPath);
	HCI_DeleteAssetIfExists(DestAssetPath);
	UEditorAssetLibrary::DeleteDirectory(RootDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitMoveAssetExecuteTest,
	"HCIAbilityKit.Editor.AgentTools.MoveAssetExecuteMovesRealAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitMoveAssetExecuteTest::RunTest(const FString& Parameters)
{
	const FString SourceDir = TEXT("/Game/__HCI_Auto/I3_MoveSrc");
	const FString TargetDir = TEXT("/Game/__HCI_Auto/I3_MoveDst");
	const FString SourceObjectPath = TEXT("/Game/__HCI_Auto/I3_MoveSrc/SM_I3_MoveSrc.SM_I3_MoveSrc");
	const FString SourceAssetPath = TEXT("/Game/__HCI_Auto/I3_MoveSrc/SM_I3_MoveSrc");
	const FString DestObjectPath = TEXT("/Game/__HCI_Auto/I3_MoveDst/SM_I3_MoveSrc.SM_I3_MoveSrc");
	const FString DestAssetPath = TEXT("/Game/__HCI_Auto/I3_MoveDst/SM_I3_MoveSrc");

	HCI_DeleteAssetIfExists(SourceAssetPath);
	HCI_DeleteAssetIfExists(DestAssetPath);
	UEditorAssetLibrary::DeleteDirectory(SourceDir);
	UEditorAssetLibrary::DeleteDirectory(TargetDir);
	UEditorAssetLibrary::MakeDirectory(SourceDir);
	UEditorAssetLibrary::MakeDirectory(TargetDir);
	if (!HCI_TryDuplicateProbeAsset(SourceAssetPath))
	{
		AddError(TEXT("Failed to prepare source asset for MoveAsset execute test."));
		return false;
	}

	TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>> Actions;
	HCIAbilityKitAgentToolActions::BuildStageIDraftActions(Actions);
	const TSharedPtr<IHCIAbilityKitAgentToolAction>* MoveAction = Actions.Find(TEXT("MoveAsset"));
	if (MoveAction == nullptr || !MoveAction->IsValid())
	{
		HCI_DeleteAssetIfExists(SourceAssetPath);
		HCI_DeleteAssetIfExists(DestAssetPath);
		AddError(TEXT("MoveAsset action is not registered."));
		return false;
	}

	FHCIAbilityKitAgentToolActionRequest Request;
	Request.RequestId = TEXT("req_test_i3_move_execute");
	Request.StepId = TEXT("step_move");
	Request.ToolName = TEXT("MoveAsset");
	Request.Args = MakeShared<FJsonObject>();
	Request.Args->SetStringField(TEXT("asset_path"), SourceObjectPath);
	Request.Args->SetStringField(TEXT("target_path"), TargetDir);

	FHCIAbilityKitAgentToolActionResult Result;
	const bool bCallOk = (*MoveAction)->Execute(Request, Result);

	TestTrue(TEXT("Move execute call should succeed"), bCallOk);
	TestTrue(TEXT("Move result should succeed"), Result.bSucceeded);
	TestEqual(TEXT("Move reason"), Result.Reason, FString(TEXT("move_execute_ok")));
	TestTrue(TEXT("Destination asset should exist"), UEditorAssetLibrary::DoesAssetExist(DestAssetPath));
	TestEqual(TEXT("Evidence after path"), Result.Evidence.FindRef(TEXT("after")), DestObjectPath);
	TestTrue(TEXT("Redirector fixup evidence should be recorded"), Result.Evidence.Contains(TEXT("redirector_fixup")));

	HCI_DeleteAssetIfExists(SourceAssetPath);
	HCI_DeleteAssetIfExists(DestAssetPath);
	UEditorAssetLibrary::DeleteDirectory(SourceDir);
	UEditorAssetLibrary::DeleteDirectory(TargetDir);
	return true;
}

#endif
