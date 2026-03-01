#pragma once

#include "CoreMinimal.h"

enum class EHCIDryRunRisk : uint8
{
	ReadOnly,
	Write,
	Destructive
};

enum class EHCIDryRunObjectType : uint8
{
	Asset,
	Actor
};

enum class EHCIDryRunLocateStrategy : uint8
{
	SyncBrowser,
	CameraFocus
};

struct HCIRUNTIME_API FHCIDryRunDiffSummary
{
	int32 TotalCandidates = 0;
	int32 Modifiable = 0;
	int32 Skipped = 0;
};

struct HCIRUNTIME_API FHCIDryRunDiffItem
{
	FString AssetPath;
	FString Field;
	FString Before;
	FString After;
	FString ToolName;
	EHCIDryRunRisk Risk = EHCIDryRunRisk::ReadOnly;
	FString SkipReason;

	EHCIDryRunObjectType ObjectType = EHCIDryRunObjectType::Asset;
	EHCIDryRunLocateStrategy LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
	FString EvidenceKey;
	FString ActorPath;
};

struct HCIRUNTIME_API FHCIDryRunDiffReport
{
	FString RequestId;
	FHCIDryRunDiffSummary Summary;
	TArray<FHCIDryRunDiffItem> DiffItems;
};

class HCIRUNTIME_API FHCIDryRunDiff
{
public:
	static FString RiskToString(EHCIDryRunRisk Risk);
	static FString ObjectTypeToString(EHCIDryRunObjectType ObjectType);
	static FString LocateStrategyToString(EHCIDryRunLocateStrategy LocateStrategy);

	static void NormalizeAndFinalize(FHCIDryRunDiffReport& InOutReport);
};



