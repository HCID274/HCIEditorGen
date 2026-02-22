#if WITH_DEV_AUTOMATION_TESTS

#include "Audit/HCIAbilityKitAuditRuleRegistry.h"
#include "Audit/HCIAbilityKitAuditScanService.h"
#include "Misc/AutomationTest.h"

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

#endif
