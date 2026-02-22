#pragma once

#include "Audit/HCIAbilityKitAuditRule.h"

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditRuleRegistry
{
public:
	static FHCIAbilityKitAuditRuleRegistry& Get();

	void ResetToDefaults();
	bool RegisterRule(TUniquePtr<IHCIAbilityKitAuditRule>&& InRule);
	bool SetRuleEnabled(FName RuleId, bool bEnabled);
	bool IsRuleEnabled(FName RuleId) const;

	void Evaluate(const FHCIAbilityKitAuditContext& Context, TArray<FHCIAbilityKitAuditIssue>& OutIssues) const;
	TArray<FName> GetRegisteredRuleIds() const;

private:
	FHCIAbilityKitAuditRuleRegistry() = default;
	~FHCIAbilityKitAuditRuleRegistry() = default;
	FHCIAbilityKitAuditRuleRegistry(const FHCIAbilityKitAuditRuleRegistry&) = delete;
	FHCIAbilityKitAuditRuleRegistry& operator=(const FHCIAbilityKitAuditRuleRegistry&) = delete;
	FHCIAbilityKitAuditRuleRegistry(FHCIAbilityKitAuditRuleRegistry&&) = delete;
	FHCIAbilityKitAuditRuleRegistry& operator=(FHCIAbilityKitAuditRuleRegistry&&) = delete;

	struct FRuleEntry
	{
		TUniquePtr<IHCIAbilityKitAuditRule> Rule;
		bool bEnabled = true;
	};

	void EnsureDefaultRules() const;

	mutable bool bDefaultsInitialized = false;
	mutable TArray<FRuleEntry> Rules;
	mutable TMap<FName, int32> RuleIndexById;
};
