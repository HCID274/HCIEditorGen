#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitAgentToolAction.h"
#include "AgentActions/HCIAbilityKitAgentToolActions.h"
#include "Dom/JsonObject.h"
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

#endif
