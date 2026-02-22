#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAuditAssetRow;

enum class EHCIAbilityKitAuditSeverity : uint8
{
	Info,
	Warn,
	Error
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditEvidenceItem
{
	FString Key;
	FString Value;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditIssue
{
	FName RuleId;
	EHCIAbilityKitAuditSeverity Severity = EHCIAbilityKitAuditSeverity::Info;
	FString Reason;
	FString Hint;
	TArray<FHCIAbilityKitAuditEvidenceItem> Evidence;

	void AddEvidence(const FString& InKey, const FString& InValue);
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditContext
{
	const FHCIAbilityKitAuditAssetRow& AssetRow;
};

class HCIABILITYKITRUNTIME_API IHCIAbilityKitAuditRule
{
public:
	virtual ~IHCIAbilityKitAuditRule() = default;

	virtual FName GetRuleId() const = 0;
	virtual void Evaluate(const FHCIAbilityKitAuditContext& Context, TArray<FHCIAbilityKitAuditIssue>& OutIssues) const = 0;
};

