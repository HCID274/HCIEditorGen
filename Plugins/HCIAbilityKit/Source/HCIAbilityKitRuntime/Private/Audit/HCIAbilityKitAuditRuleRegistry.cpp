#include "Audit/HCIAbilityKitAuditRuleRegistry.h"

#include "Audit/HCIAbilityKitAuditScanService.h"

namespace
{
class FHCIAbilityKitTriangleExpectedMismatchRule final : public IHCIAbilityKitAuditRule
{
public:
	virtual FName GetRuleId() const override
	{
		static const FName RuleId(TEXT("TriangleExpectedMismatchRule"));
		return RuleId;
	}

	virtual void Evaluate(const FHCIAbilityKitAuditContext& Context, TArray<FHCIAbilityKitAuditIssue>& OutIssues) const override
	{
		const FHCIAbilityKitAuditAssetRow& Row = Context.AssetRow;
		if (Row.TriangleCountLod0Actual < 0 || Row.TriangleCountLod0ExpectedJson < 0)
		{
			return;
		}

		const int32 Delta = Row.TriangleCountLod0Actual - Row.TriangleCountLod0ExpectedJson;
		if (Delta == 0)
		{
			return;
		}

		FHCIAbilityKitAuditIssue& Issue = OutIssues.Emplace_GetRef();
		Issue.RuleId = GetRuleId();
		Issue.Severity = EHCIAbilityKitAuditSeverity::Warn;
		Issue.Reason = TEXT("triangle_count_lod0 expected value mismatches actual mesh triangles");
		Issue.Hint = TEXT("Use mesh actual value as source of truth; update params.triangle_count_lod0 if this change is intentional.");
		Issue.AddEvidence(TEXT("asset_path"), Row.AssetPath);
		Issue.AddEvidence(TEXT("triangle_count_lod0_actual"), FString::FromInt(Row.TriangleCountLod0Actual));
		Issue.AddEvidence(TEXT("triangle_count_lod0_expected_json"), FString::FromInt(Row.TriangleCountLod0ExpectedJson));
		Issue.AddEvidence(TEXT("triangle_mismatch_delta"), FString::FromInt(Delta));
		Issue.AddEvidence(TEXT("triangle_source"), Row.TriangleSource);
		if (!Row.TriangleSourceTagKey.IsEmpty())
		{
			Issue.AddEvidence(TEXT("source_tag_key"), Row.TriangleSourceTagKey);
		}
	}
};
}

FHCIAbilityKitAuditRuleRegistry& FHCIAbilityKitAuditRuleRegistry::Get()
{
	static FHCIAbilityKitAuditRuleRegistry Instance;
	return Instance;
}

void FHCIAbilityKitAuditRuleRegistry::ResetToDefaults()
{
	Rules.Reset();
	RuleIndexById.Reset();
	bDefaultsInitialized = true;
	RegisterRule(MakeUnique<FHCIAbilityKitTriangleExpectedMismatchRule>());
}

bool FHCIAbilityKitAuditRuleRegistry::RegisterRule(TUniquePtr<IHCIAbilityKitAuditRule>&& InRule)
{
	if (!InRule.IsValid())
	{
		return false;
	}

	const FName RuleId = InRule->GetRuleId();
	if (RuleId.IsNone() || RuleIndexById.Contains(RuleId))
	{
		return false;
	}

	FRuleEntry& Entry = Rules.Emplace_GetRef();
	Entry.Rule = MoveTemp(InRule);
	Entry.bEnabled = true;
	RuleIndexById.Add(RuleId, Rules.Num() - 1);
	return true;
}

bool FHCIAbilityKitAuditRuleRegistry::SetRuleEnabled(const FName RuleId, const bool bEnabled)
{
	EnsureDefaultRules();

	const int32* FoundIndex = RuleIndexById.Find(RuleId);
	if (!FoundIndex || !Rules.IsValidIndex(*FoundIndex))
	{
		return false;
	}

	Rules[*FoundIndex].bEnabled = bEnabled;
	return true;
}

bool FHCIAbilityKitAuditRuleRegistry::IsRuleEnabled(const FName RuleId) const
{
	EnsureDefaultRules();

	const int32* FoundIndex = RuleIndexById.Find(RuleId);
	if (!FoundIndex || !Rules.IsValidIndex(*FoundIndex))
	{
		return false;
	}

	return Rules[*FoundIndex].bEnabled;
}

void FHCIAbilityKitAuditRuleRegistry::Evaluate(
	const FHCIAbilityKitAuditContext& Context,
	TArray<FHCIAbilityKitAuditIssue>& OutIssues) const
{
	EnsureDefaultRules();

	for (const FRuleEntry& Entry : Rules)
	{
		if (!Entry.bEnabled || !Entry.Rule.IsValid())
		{
			continue;
		}

		Entry.Rule->Evaluate(Context, OutIssues);
	}
}

TArray<FName> FHCIAbilityKitAuditRuleRegistry::GetRegisteredRuleIds() const
{
	EnsureDefaultRules();

	TArray<FName> RuleIds;
	RuleIds.Reserve(Rules.Num());
	for (const FRuleEntry& Entry : Rules)
	{
		if (!Entry.Rule.IsValid())
		{
			continue;
		}

		RuleIds.Add(Entry.Rule->GetRuleId());
	}
	return RuleIds;
}

void FHCIAbilityKitAuditRuleRegistry::EnsureDefaultRules() const
{
	if (bDefaultsInitialized)
	{
		return;
	}

	const_cast<FHCIAbilityKitAuditRuleRegistry*>(this)->ResetToDefaults();
}

