#pragma once

#include "CoreMinimal.h"
#include "Audit/HCIAbilityKitAuditRule.h"

struct FHCIAbilityKitAuditScanSnapshot;

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditResultEntry
{
	FString AssetPath;
	FString AssetName;
	FString AssetClass;
	FString RuleId;
	EHCIAbilityKitAuditSeverity Severity = EHCIAbilityKitAuditSeverity::Info;
	FString SeverityText;
	FString Reason;
	FString Hint;
	FString TriangleSource;
	FString ScanState;
	FString SkipReason;
	TMap<FString, FString> Evidence;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditReport
{
	FString RunId;
	FDateTime GeneratedUtc;
	FString Source;
	TArray<FHCIAbilityKitAuditResultEntry> Results;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAuditReportBuilder
{
public:
	static FString SeverityToString(EHCIAbilityKitAuditSeverity Severity);
	static FHCIAbilityKitAuditReport BuildFromSnapshot(const FHCIAbilityKitAuditScanSnapshot& Snapshot, const FString& RunIdOverride = FString());
};

