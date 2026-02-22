#if WITH_DEV_AUTOMATION_TESTS

#include "Audit/HCIAbilityKitAuditReport.h"
#include "Audit/HCIAbilityKitAuditRuleRegistry.h"
#include "Audit/HCIAbilityKitAuditScanService.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditSeverityStringTest,
	"HCIAbilityKit.Editor.AuditResults.SeverityStringMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditSeverityStringTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Info severity should map to Info"),
		FHCIAbilityKitAuditReportBuilder::SeverityToString(EHCIAbilityKitAuditSeverity::Info),
		FString(TEXT("Info")));
	TestEqual(
		TEXT("Warn severity should map to Warn"),
		FHCIAbilityKitAuditReportBuilder::SeverityToString(EHCIAbilityKitAuditSeverity::Warn),
		FString(TEXT("Warn")));
	TestEqual(
		TEXT("Error severity should map to Error"),
		FHCIAbilityKitAuditReportBuilder::SeverityToString(EHCIAbilityKitAuditSeverity::Error),
		FString(TEXT("Error")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditReportBuilderFlattensIssuesTest,
	"HCIAbilityKit.Editor.AuditResults.BuildReportFlattensIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditReportBuilderFlattensIssuesTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditScanSnapshot Snapshot;
	Snapshot.Stats.Source = TEXT("test_snapshot");

	FHCIAbilityKitAuditAssetRow& Row = Snapshot.Rows.Emplace_GetRef();
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_04.Ability_04");
	Row.AssetClass = TEXT("HCIAbilityKitAsset");
	Row.ScanState = TEXT("ok");
	Row.TriangleSource = TEXT("tag_cached");

	FHCIAbilityKitAuditIssue& WarnIssue = Row.AuditIssues.Emplace_GetRef();
	WarnIssue.RuleId = TEXT("HighPolyAutoLODRule");
	WarnIssue.Severity = EHCIAbilityKitAuditSeverity::Warn;
	WarnIssue.Reason = TEXT("high triangle mesh is missing additional LODs");
	WarnIssue.Hint = TEXT("Create LODs");
	WarnIssue.AddEvidence(TEXT("triangle_count_lod0_actual"), TEXT("200000"));
	WarnIssue.AddEvidence(TEXT("triangle_source"), TEXT("tag_cached"));

	FHCIAbilityKitAuditIssue& ErrorIssue = Row.AuditIssues.Emplace_GetRef();
	ErrorIssue.RuleId = TEXT("TextureNPOTRule");
	ErrorIssue.Severity = EHCIAbilityKitAuditSeverity::Error;
	ErrorIssue.Reason = TEXT("texture dimensions are not power-of-two");
	ErrorIssue.Hint = TEXT("Resize texture");
	ErrorIssue.AddEvidence(TEXT("texture_width"), TEXT("1000"));
	ErrorIssue.AddEvidence(TEXT("texture_height"), TEXT("1024"));

	const FHCIAbilityKitAuditReport Report =
		FHCIAbilityKitAuditReportBuilder::BuildFromSnapshot(Snapshot, TEXT("audit_test_run_001"));

	TestEqual(TEXT("Report should preserve explicit run id"), Report.RunId, FString(TEXT("audit_test_run_001")));
	TestEqual(TEXT("Report should flatten two issues"), Report.Results.Num(), 2);
	if (Report.Results.Num() != 2)
	{
		return false;
	}

	const FHCIAbilityKitAuditResultEntry& First = Report.Results[0];
	TestEqual(TEXT("First result asset path"), First.AssetPath, Row.AssetPath);
	TestEqual(TEXT("First result rule id"), First.RuleId, FString(TEXT("HighPolyAutoLODRule")));
	TestEqual(TEXT("First result severity text"), First.SeverityText, FString(TEXT("Warn")));
	TestEqual(TEXT("First result scan state"), First.ScanState, FString(TEXT("ok")));
	TestEqual(TEXT("First result triangle source"), First.TriangleSource, FString(TEXT("tag_cached")));
	TestEqual(
		TEXT("First result evidence triangle_count_lod0_actual"),
		First.Evidence.FindRef(TEXT("triangle_count_lod0_actual")),
		FString(TEXT("200000")));

	const FHCIAbilityKitAuditResultEntry& Second = Report.Results[1];
	TestEqual(TEXT("Second result rule id"), Second.RuleId, FString(TEXT("TextureNPOTRule")));
	TestEqual(TEXT("Second result severity text"), Second.SeverityText, FString(TEXT("Error")));
	TestEqual(
		TEXT("Second result evidence texture_width"),
		Second.Evidence.FindRef(TEXT("texture_width")),
		FString(TEXT("1000")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditRuleRegistryMismatchWarnTest,
	"HCIAbilityKit.Editor.AuditRules.TriangleExpectedMismatchWarn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditRuleRegistryMismatchWarnTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditRuleRegistry& Registry = FHCIAbilityKitAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_01.Ability_01");
	Row.TriangleCountLod0Actual = 500000;
	Row.TriangleCountLod0ExpectedJson = 500;
	Row.TriangleSource = TEXT("tag_cached");
	Row.TriangleSourceTagKey = TEXT("Triangles");

	const FHCIAbilityKitAuditContext Context{Row};
	TArray<FHCIAbilityKitAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	const FHCIAbilityKitAuditIssue* MismatchIssue = Issues.FindByPredicate(
		[](const FHCIAbilityKitAuditIssue& Issue)
		{
			return Issue.RuleId == FName(TEXT("TriangleExpectedMismatchRule"));
		});

	TestNotNull(TEXT("TriangleExpectedMismatchRule should emit a warn issue"), MismatchIssue);
	if (!MismatchIssue)
	{
		return false;
	}

	TestEqual(TEXT("Severity should be Warn"), static_cast<uint8>(MismatchIssue->Severity), static_cast<uint8>(EHCIAbilityKitAuditSeverity::Warn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditRuleRegistryNoIssueWhenExpectedMissingTest,
	"HCIAbilityKit.Editor.AuditRules.TriangleExpectedMissingShouldNotWarn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditRuleRegistryNoIssueWhenExpectedMissingTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditRuleRegistry& Registry = FHCIAbilityKitAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_02.Ability_02");
	Row.TriangleCountLod0Actual = 32000;
	Row.TriangleCountLod0ExpectedJson = INDEX_NONE;

	const FHCIAbilityKitAuditContext Context{Row};
	TArray<FHCIAbilityKitAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	TestEqual(TEXT("No issue should be emitted without expected triangle"), Issues.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditRuleRegistryHighPolyAutoLodWarnTest,
	"HCIAbilityKit.Editor.AuditRules.HighPolyAutoLODWarn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditRuleRegistryHighPolyAutoLodWarnTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditRuleRegistry& Registry = FHCIAbilityKitAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_03.Ability_03");
	Row.RepresentingMeshPath = TEXT("/Game/Seed/SM_Test.SM_Test");
	Row.TriangleCountLod0Actual = 200000;
	Row.MeshLodCount = 1;
	Row.bMeshNaniteEnabled = false;
	Row.bMeshNaniteEnabledKnown = true;

	const FHCIAbilityKitAuditContext Context{Row};
	TArray<FHCIAbilityKitAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	const FHCIAbilityKitAuditIssue* Issue = Issues.FindByPredicate(
		[](const FHCIAbilityKitAuditIssue& Candidate)
		{
			return Candidate.RuleId == FName(TEXT("HighPolyAutoLODRule"));
		});
	TestNotNull(TEXT("HighPolyAutoLODRule should emit issue when high poly and no extra LOD"), Issue);
	if (!Issue)
	{
		return false;
	}

	TestEqual(TEXT("HighPolyAutoLODRule severity should be Warn"), static_cast<uint8>(Issue->Severity), static_cast<uint8>(EHCIAbilityKitAuditSeverity::Warn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitAuditRuleRegistryTextureNpotErrorTest,
	"HCIAbilityKit.Editor.AuditRules.TextureNPOTError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitAuditRuleRegistryTextureNpotErrorTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitAuditRuleRegistry& Registry = FHCIAbilityKitAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAbilityKitAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/Textures/T_TestNPOT.T_TestNPOT");
	Row.AssetClass = TEXT("Texture2D");
	Row.TextureWidth = 1000;
	Row.TextureHeight = 1024;

	const FHCIAbilityKitAuditContext Context{Row};
	TArray<FHCIAbilityKitAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	const FHCIAbilityKitAuditIssue* Issue = Issues.FindByPredicate(
		[](const FHCIAbilityKitAuditIssue& Candidate)
		{
			return Candidate.RuleId == FName(TEXT("TextureNPOTRule"));
		});
	TestNotNull(TEXT("TextureNPOTRule should emit issue for NPOT texture"), Issue);
	if (!Issue)
	{
		return false;
	}

	TestEqual(TEXT("TextureNPOTRule severity should be Error"), static_cast<uint8>(Issue->Severity), static_cast<uint8>(EHCIAbilityKitAuditSeverity::Error));
	return true;
}

#endif
