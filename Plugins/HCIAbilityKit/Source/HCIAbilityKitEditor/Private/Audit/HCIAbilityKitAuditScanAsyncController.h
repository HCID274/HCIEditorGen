#pragma once

#include "AssetRegistry/AssetData.h"

enum class EHCIAbilityKitAuditScanAsyncPhase : uint8
{
	Idle,
	Running,
	Completed,
	Cancelled,
	Failed
};

struct FHCIAbilityKitAuditScanBatch
{
	bool bValid = false;
	int32 StartIndex = 0;
	int32 EndIndex = 0;
};

class FHCIAbilityKitAuditScanAsyncController
{
public:
	bool Start(TArray<FAssetData>&& InAssetDatas, int32 InBatchSize, int32 InLogTopN, FString& OutError);
	bool Retry(FString& OutError);
	bool Cancel(FString& OutError);

	FHCIAbilityKitAuditScanBatch DequeueBatch();

	void Complete();
	void Fail(const FString& InFailureReason);
	void Reset(bool bClearRetryContext);

	bool IsRunning() const { return Phase == EHCIAbilityKitAuditScanAsyncPhase::Running; }
	bool CanRetry() const { return RetryAssetDatas.Num() > 0 && Phase != EHCIAbilityKitAuditScanAsyncPhase::Running; }

	int32 GetBatchSize() const { return BatchSize; }
	int32 GetLogTopN() const { return LogTopN; }
	int32 GetTotalCount() const { return AssetDatas.Num(); }
	int32 GetNextIndex() const { return NextIndex; }
	int32 GetProgressPercent() const;

	const TArray<FAssetData>& GetAssetDatas() const { return AssetDatas; }
	EHCIAbilityKitAuditScanAsyncPhase GetPhase() const { return Phase; }
	const FString& GetLastFailureReason() const { return LastFailureReason; }

private:
	static bool ValidateArgs(int32 InBatchSize, int32 InLogTopN, FString& OutError);
	void StartFromStoredRetryContext();

	EHCIAbilityKitAuditScanAsyncPhase Phase = EHCIAbilityKitAuditScanAsyncPhase::Idle;
	int32 BatchSize = 256;
	int32 LogTopN = 10;
	int32 NextIndex = 0;
	TArray<FAssetData> AssetDatas;

	// Keep the previous payload for explicit retry after cancel/fail.
	TArray<FAssetData> RetryAssetDatas;
	int32 RetryBatchSize = 256;
	int32 RetryLogTopN = 10;

	FString LastFailureReason;
};
