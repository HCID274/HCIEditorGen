#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"
#include "AgentActions/HCIAbilityKitAgentToolActions.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
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
	const FString RootDir = TEXT("/Game/__HCI_Auto");
	const FString ProbeDir = TEXT("/Game/__HCI_Auto/M_New");
	const FString ProbeAssetPath = TEXT("/Game/__HCI_Auto/M_New/SM_HCI_SearchProbe");

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
	const FString RootDir = TEXT("/Game/__HCI_Auto");
	const FString ProbeDir = TEXT("/Game/__HCI_Auto/Temp_AliasCase");
	const FString ProbeAssetPath = TEXT("/Game/__HCI_Auto/Temp_AliasCase/SM_HCI_SearchAliasProbe");

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
	UEditorAssetLibrary::DeleteDirectory(TEXT("/Game/__HCI_Auto"));
	return true;
}

#endif
