#if WITH_DEV_AUTOMATION_TESTS

#include "Audit/HCIAuditReport.h"
#include "Audit/HCIAuditReportJsonSerializer.h"
#include "Audit/HCIAuditRuleRegistry.h"
#include "Audit/HCIAuditScanService.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditReportJsonSerializerTest,
	"HCI.Editor.AuditResults.JsonSerializerIncludesCoreTraceFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditReportJsonSerializerTest::RunTest(const FString& Parameters)
{
	FHCIAuditReport Report;
	Report.RunId = TEXT("audit_test_run_002");
	Report.GeneratedUtc = FDateTime(2026, 2, 22, 13, 0, 0);
	Report.Source = TEXT("asset_registry_fassetdata");

	FHCIAuditResultEntry& Result = Report.Results.Emplace_GetRef();
	Result.AssetPath = TEXT("/Game/HCI/Data/Ability_05.Ability_05");
	Result.AssetClass = TEXT("HCIAsset");
	Result.RuleId = TEXT("TriangleExpectedMismatchRule");
	Result.Severity = EHCIAuditSeverity::Warn;
	Result.SeverityText = TEXT("Warn");
	Result.Reason = TEXT("triangle mismatch");
	Result.Hint = TEXT("update expected");
	Result.TriangleSource = TEXT("tag_cached");
	Result.ScanState = TEXT("ok");
	Result.Evidence.Add(TEXT("triangle_source"), TEXT("tag_cached"));
	Result.Evidence.Add(TEXT("triangle_count_lod0_actual"), TEXT("196608"));

	FString JsonText;
	const bool bOk = FHCIAuditReportJsonSerializer::SerializeToJsonString(Report, JsonText);
	TestTrue(TEXT("SerializeToJsonString should succeed"), bOk);
	if (!bOk)
	{
		return false;
	}

	TestTrue(TEXT("JSON should contain run_id"), JsonText.Contains(TEXT("\"run_id\":\"audit_test_run_002\"")));
	TestTrue(TEXT("JSON should contain rule_id"), JsonText.Contains(TEXT("\"rule_id\":\"TriangleExpectedMismatchRule\"")));
	TestTrue(TEXT("JSON should contain severity"), JsonText.Contains(TEXT("\"severity\":\"Warn\"")));
	TestTrue(TEXT("JSON should contain top-level triangle_source"), JsonText.Contains(TEXT("\"triangle_source\":\"tag_cached\"")));
	TestTrue(TEXT("JSON should contain evidence object"), JsonText.Contains(TEXT("\"evidence\":{")));
	TestTrue(TEXT("JSON should contain evidence triangle_count_lod0_actual"), JsonText.Contains(TEXT("\"triangle_count_lod0_actual\":\"196608\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditSeverityStringTest,
	"HCI.Editor.AuditResults.SeverityStringMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditSeverityStringTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Info severity should map to Info"),
		FHCIAuditReportBuilder::SeverityToString(EHCIAuditSeverity::Info),
		FString(TEXT("Info")));
	TestEqual(
		TEXT("Warn severity should map to Warn"),
		FHCIAuditReportBuilder::SeverityToString(EHCIAuditSeverity::Warn),
		FString(TEXT("Warn")));
	TestEqual(
		TEXT("Error severity should map to Error"),
		FHCIAuditReportBuilder::SeverityToString(EHCIAuditSeverity::Error),
		FString(TEXT("Error")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditReportBuilderFlattensIssuesTest,
	"HCI.Editor.AuditResults.BuildReportFlattensIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditReportBuilderFlattensIssuesTest::RunTest(const FString& Parameters)
{
	FHCIAuditScanSnapshot Snapshot;
	Snapshot.Stats.Source = TEXT("test_snapshot");

	FHCIAuditAssetRow& Row = Snapshot.Rows.Emplace_GetRef();
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_04.Ability_04");
	Row.AssetClass = TEXT("HCIAsset");
	Row.ScanState = TEXT("ok");
	Row.TriangleSource = TEXT("tag_cached");

	FHCIAuditIssue& WarnIssue = Row.AuditIssues.Emplace_GetRef();
	WarnIssue.RuleId = TEXT("HighPolyAutoLODRule");
	WarnIssue.Severity = EHCIAuditSeverity::Warn;
	WarnIssue.Reason = TEXT("high triangle mesh is missing additional LODs");
	WarnIssue.Hint = TEXT("Create LODs");
	WarnIssue.AddEvidence(TEXT("triangle_count_lod0_actual"), TEXT("200000"));
	WarnIssue.AddEvidence(TEXT("triangle_source"), TEXT("tag_cached"));

	FHCIAuditIssue& ErrorIssue = Row.AuditIssues.Emplace_GetRef();
	ErrorIssue.RuleId = TEXT("TextureNPOTRule");
	ErrorIssue.Severity = EHCIAuditSeverity::Error;
	ErrorIssue.Reason = TEXT("texture dimensions are not power-of-two");
	ErrorIssue.Hint = TEXT("Resize texture");
	ErrorIssue.AddEvidence(TEXT("texture_width"), TEXT("1000"));
	ErrorIssue.AddEvidence(TEXT("texture_height"), TEXT("1024"));

	const FHCIAuditReport Report =
		FHCIAuditReportBuilder::BuildFromSnapshot(Snapshot, TEXT("audit_test_run_001"));

	TestEqual(TEXT("Report should preserve explicit run id"), Report.RunId, FString(TEXT("audit_test_run_001")));
	TestEqual(TEXT("Report should flatten two issues"), Report.Results.Num(), 2);
	if (Report.Results.Num() != 2)
	{
		return false;
	}

	const FHCIAuditResultEntry& First = Report.Results[0];
	TestEqual(TEXT("First result asset path"), First.AssetPath, Row.AssetPath);
	TestEqual(TEXT("First result rule id"), First.RuleId, FString(TEXT("HighPolyAutoLODRule")));
	TestEqual(TEXT("First result severity text"), First.SeverityText, FString(TEXT("Warn")));
	TestEqual(TEXT("First result scan state"), First.ScanState, FString(TEXT("ok")));
	TestEqual(TEXT("First result triangle source"), First.TriangleSource, FString(TEXT("tag_cached")));
	TestEqual(
		TEXT("First result evidence triangle_count_lod0_actual"),
		First.Evidence.FindRef(TEXT("triangle_count_lod0_actual")),
		FString(TEXT("200000")));

	const FHCIAuditResultEntry& Second = Report.Results[1];
	TestEqual(TEXT("Second result rule id"), Second.RuleId, FString(TEXT("TextureNPOTRule")));
	TestEqual(TEXT("Second result severity text"), Second.SeverityText, FString(TEXT("Error")));
	TestEqual(
		TEXT("Second result evidence texture_width"),
		Second.Evidence.FindRef(TEXT("texture_width")),
		FString(TEXT("1000")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditRuleRegistryMismatchWarnTest,
	"HCI.Editor.AuditRules.TriangleExpectedMismatchWarn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditRuleRegistryMismatchWarnTest::RunTest(const FString& Parameters)
{
	FHCIAuditRuleRegistry& Registry = FHCIAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_01.Ability_01");
	Row.TriangleCountLod0Actual = 500000;
	Row.TriangleCountLod0ExpectedJson = 500;
	Row.TriangleSource = TEXT("tag_cached");
	Row.TriangleSourceTagKey = TEXT("Triangles");

	const FHCIAuditContext Context{Row};
	TArray<FHCIAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	const FHCIAuditIssue* MismatchIssue = Issues.FindByPredicate(
		[](const FHCIAuditIssue& Issue)
		{
			return Issue.RuleId == FName(TEXT("TriangleExpectedMismatchRule"));
		});

	TestNotNull(TEXT("TriangleExpectedMismatchRule should emit a warn issue"), MismatchIssue);
	if (!MismatchIssue)
	{
		return false;
	}

	TestEqual(TEXT("Severity should be Warn"), static_cast<uint8>(MismatchIssue->Severity), static_cast<uint8>(EHCIAuditSeverity::Warn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditRuleRegistryNoIssueWhenExpectedMissingTest,
	"HCI.Editor.AuditRules.TriangleExpectedMissingShouldNotWarn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditRuleRegistryNoIssueWhenExpectedMissingTest::RunTest(const FString& Parameters)
{
	FHCIAuditRuleRegistry& Registry = FHCIAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_02.Ability_02");
	Row.TriangleCountLod0Actual = 32000;
	Row.TriangleCountLod0ExpectedJson = INDEX_NONE;

	const FHCIAuditContext Context{Row};
	TArray<FHCIAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	TestEqual(TEXT("No issue should be emitted without expected triangle"), Issues.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditRuleRegistryHighPolyAutoLodWarnTest,
	"HCI.Editor.AuditRules.HighPolyAutoLODWarn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditRuleRegistryHighPolyAutoLodWarnTest::RunTest(const FString& Parameters)
{
	FHCIAuditRuleRegistry& Registry = FHCIAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/HCI/Data/Ability_03.Ability_03");
	Row.RepresentingMeshPath = TEXT("/Game/Seed/SM_Test.SM_Test");
	Row.TriangleCountLod0Actual = 200000;
	Row.MeshLodCount = 1;
	Row.bMeshNaniteEnabled = false;
	Row.bMeshNaniteEnabledKnown = true;

	const FHCIAuditContext Context{Row};
	TArray<FHCIAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	const FHCIAuditIssue* Issue = Issues.FindByPredicate(
		[](const FHCIAuditIssue& Candidate)
		{
			return Candidate.RuleId == FName(TEXT("HighPolyAutoLODRule"));
		});
	TestNotNull(TEXT("HighPolyAutoLODRule should emit issue when high poly and no extra LOD"), Issue);
	if (!Issue)
	{
		return false;
	}

	TestEqual(TEXT("HighPolyAutoLODRule severity should be Warn"), static_cast<uint8>(Issue->Severity), static_cast<uint8>(EHCIAuditSeverity::Warn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAuditRuleRegistryTextureNpotErrorTest,
	"HCI.Editor.AuditRules.TextureNPOTError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAuditRuleRegistryTextureNpotErrorTest::RunTest(const FString& Parameters)
{
	FHCIAuditRuleRegistry& Registry = FHCIAuditRuleRegistry::Get();
	Registry.ResetToDefaults();

	FHCIAuditAssetRow Row;
	Row.AssetPath = TEXT("/Game/Textures/T_TestNPOT.T_TestNPOT");
	Row.AssetClass = TEXT("Texture2D");
	Row.TextureWidth = 1000;
	Row.TextureHeight = 1024;

	const FHCIAuditContext Context{Row};
	TArray<FHCIAuditIssue> Issues;
	Registry.Evaluate(Context, Issues);

	const FHCIAuditIssue* Issue = Issues.FindByPredicate(
		[](const FHCIAuditIssue& Candidate)
		{
			return Candidate.RuleId == FName(TEXT("TextureNPOTRule"));
		});
	TestNotNull(TEXT("TextureNPOTRule should emit issue for NPOT texture"), Issue);
	if (!Issue)
	{
		return false;
	}

	TestEqual(TEXT("TextureNPOTRule severity should be Error"), static_cast<uint8>(Issue->Severity), static_cast<uint8>(EHCIAuditSeverity::Error));
	return true;
}

#endif

