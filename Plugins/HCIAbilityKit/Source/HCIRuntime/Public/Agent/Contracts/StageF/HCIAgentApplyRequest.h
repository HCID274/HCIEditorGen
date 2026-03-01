#pragma once

#include "CoreMinimal.h"

#include "Agent/Executor/HCIDryRunDiff.h"

struct HCIRUNTIME_API FHCIAgentApplyRequestSummary
{
	int32 TotalRows = 0;
	int32 SelectedRows = 0;
	int32 ModifiableRows = 0;
	int32 BlockedRows = 0;
	int32 ReadOnlyRows = 0;
	int32 WriteRows = 0;
	int32 DestructiveRows = 0;
};

struct HCIRUNTIME_API FHCIAgentApplyRequestItem
{
	int32 RowIndex = INDEX_NONE;
	FString ToolName;
	EHCIDryRunRisk Risk = EHCIDryRunRisk::ReadOnly;
	FString AssetPath;
	FString Field;
	FString SkipReason;
	bool bBlocked = false;
	EHCIDryRunObjectType ObjectType = EHCIDryRunObjectType::Asset;
	EHCIDryRunLocateStrategy LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
	FString EvidenceKey;
	FString ActorPath;
};

struct HCIRUNTIME_API FHCIAgentApplyRequest
{
	FString RequestId;
	FString ReviewRequestId;
	FString SelectionDigest;
	FString GeneratedUtc;
	FString ExecutionMode;
	bool bReady = false;
	FHCIAgentApplyRequestSummary Summary;
	TArray<FHCIAgentApplyRequestItem> Items;
};


