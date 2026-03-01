#pragma once

#include "Audit/HCIAuditRule.h"

class HCIRUNTIME_API FHCIAuditRuleRegistry
{
public:
	static FHCIAuditRuleRegistry& Get();

	void ResetToDefaults();
	bool RegisterRule(TUniquePtr<IHCIAuditRule>&& InRule);
	bool SetRuleEnabled(FName RuleId, bool bEnabled);
	bool IsRuleEnabled(FName RuleId) const;

	void Evaluate(const FHCIAuditContext& Context, TArray<FHCIAuditIssue>& OutIssues) const;
	TArray<FName> GetRegisteredRuleIds() const;

private:
	FHCIAuditRuleRegistry() = default;
	~FHCIAuditRuleRegistry() = default;
	FHCIAuditRuleRegistry(const FHCIAuditRuleRegistry&) = delete;
	FHCIAuditRuleRegistry& operator=(const FHCIAuditRuleRegistry&) = delete;
	FHCIAuditRuleRegistry(FHCIAuditRuleRegistry&&) = delete;
	FHCIAuditRuleRegistry& operator=(FHCIAuditRuleRegistry&&) = delete;

	struct FRuleEntry
	{
		TUniquePtr<IHCIAuditRule> Rule;
		bool bEnabled = true;
	};

	void EnsureDefaultRules() const;

	mutable bool bDefaultsInitialized = false;
	mutable TArray<FRuleEntry> Rules;
	mutable TMap<FName, int32> RuleIndexById;
};


