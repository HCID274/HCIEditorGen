#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitDryRunDiff.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentApplyRequestSummary
{
	int32 TotalRows = 0;
	int32 SelectedRows = 0;
	int32 ModifiableRows = 0;
	int32 BlockedRows = 0;
	int32 ReadOnlyRows = 0;
	int32 WriteRows = 0;
	int32 DestructiveRows = 0;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentApplyRequestItem
{
	int32 RowIndex = INDEX_NONE;
	FString ToolName;
	EHCIAbilityKitDryRunRisk Risk = EHCIAbilityKitDryRunRisk::ReadOnly;
	FString AssetPath;
	FString Field;
	FString SkipReason;
	bool bBlocked = false;
	EHCIAbilityKitDryRunObjectType ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
	EHCIAbilityKitDryRunLocateStrategy LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
	FString EvidenceKey;
	FString ActorPath;
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentApplyRequest
{
	FString RequestId;
	FString ReviewRequestId;
	FString SelectionDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	bool bReady = false;
	FHCIAbilityKitAgentApplyRequestSummary Summary;
	TArray<FHCIAbilityKitAgentApplyRequestItem> Items;
};

