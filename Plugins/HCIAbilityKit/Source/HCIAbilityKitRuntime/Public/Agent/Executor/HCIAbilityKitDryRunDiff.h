#pragma once

#include "CoreMinimal.h"

enum class EHCIAbilityKitDryRunRisk : uint8
{
	ReadOnly,
	Write,
	Destructive
};

enum class EHCIAbilityKitDryRunObjectType : uint8
{
	Asset,
	Actor
};

enum class EHCIAbilityKitDryRunLocateStrategy : uint8
{
	SyncBrowser,
	CameraFocus
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiffSummary
{
	int32 TotalCandidates = 0;
	int32 Modifiable = 0;
	int32 Skipped = 0;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiffItem
{
	FString AssetPath;
	FString Field;
	FString Before;
	FString After;
	FString ToolName;
	EHCIAbilityKitDryRunRisk Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	FString SkipReason;

	EHCIAbilityKitDryRunObjectType ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	EHCIAbilityKitDryRunLocateStrategy LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
	FString EvidenceKey;
	FString ActorPath;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiffReport
{
	FString RequestId;
	FHCIAbilityKitDryRunDiffSummary Summary;
	TArray<FHCIAbilityKitDryRunDiffItem> DiffItems;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitDryRunDiff
{
public:
	static FString RiskToString(EHCIAbilityKitDryRunRisk Risk);
	static FString ObjectTypeToString(EHCIAbilityKitDryRunObjectType ObjectType);
	static FString LocateStrategyToString(EHCIAbilityKitDryRunLocateStrategy LocateStrategy);

	static void NormalizeAndFinalize(FHCIAbilityKitDryRunDiffReport& InOutReport);
};

