#pragma once

#include "CoreMinimal.h"
#include "Audit/HCIAuditRule.h"

struct FHCIAuditScanSnapshot;

struct HCIRUNTIME_API FHCIAuditResultEntry
{
	FString AssetPath;
	FString AssetName;
	FString AssetClass;
	FString RuleId;
	EHCIAuditSeverity Severity = EHCIAuditSeverity::Info;
	FString SeverityText;
	FString Reason;
	FString Hint;
	FString TriangleSource;
	FString ScanState;
	FString SkipReason;
	TMap<FString, FString> Evidence;
};

struct HCIRUNTIME_API FHCIAuditReport
{
	FString RunId;
	FDateTime GeneratedUtc;
	FString Source;
	TArray<FHCIAuditResultEntry> Results;
};

class HCIRUNTIME_API FHCIAuditReportBuilder
{
public:
	static FString SeverityToString(EHCIAuditSeverity Severity);
	static FHCIAuditReport BuildFromSnapshot(const FHCIAuditScanSnapshot& Snapshot, const FString& RunIdOverride = FString());
};



