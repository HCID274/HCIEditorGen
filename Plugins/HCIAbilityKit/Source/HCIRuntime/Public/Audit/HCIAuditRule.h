#pragma once

#include "CoreMinimal.h"

struct FHCIAuditAssetRow;

enum class EHCIAuditSeverity : uint8
{
	Info,
	Warn,
	Error
};

struct HCIRUNTIME_API FHCIAuditEvidenceItem
{
	FString Key;
	FString Value;
};

struct HCIRUNTIME_API FHCIAuditIssue
{
	FName RuleId;
	EHCIAuditSeverity Severity = EHCIAuditSeverity::Info;
	FString Reason;
	FString Hint;
	TArray<FHCIAuditEvidenceItem> Evidence;

	void AddEvidence(const FString& InKey, const FString& InValue);
};

struct HCIRUNTIME_API FHCIAuditContext
{
	const FHCIAuditAssetRow& AssetRow;
};

class HCIRUNTIME_API IHCIAuditRule
{
public:
	virtual ~IHCIAuditRule() = default;

	virtual FName GetRuleId() const = 0;
	virtual void Evaluate(const FHCIAuditContext& Context, TArray<FHCIAuditIssue>& OutIssues) const = 0;
};



