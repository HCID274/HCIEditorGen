#include "Audit/HCIAbilityKitAuditScanAsyncController.h"

#include "Math/UnrealMathUtility.h"

bool FHCIAbilityKitAuditScanAsyncController::Start(
	TArray<FAssetData>&& InAssetDatas,
	const int32 InBatchSize,
	const int32 InLogTopN,
	FString& OutError)
{
	return Start(MoveTemp(InAssetDatas), InBatchSize, InLogTopN, false, 0, OutError);
}

bool FHCIAbilityKitAuditScanAsyncController::Start(
	TArray<FAssetData>&& InAssetDatas,
	const int32 InBatchSize,
	const int32 InLogTopN,
	const bool bInDeepMeshCheckEnabled,
	FString& OutError)
{
	return Start(MoveTemp(InAssetDatas), InBatchSize, InLogTopN, bInDeepMeshCheckEnabled, 0, OutError);
}

bool FHCIAbilityKitAuditScanAsyncController::Start(
	TArray<FAssetData>&& InAssetDatas,
	const int32 InBatchSize,
	const int32 InLogTopN,
	const bool bInDeepMeshCheckEnabled,
	const int32 InGcEveryNBatches,
	FString& OutError)
{
	if (!ValidateArgs(InBatchSize, InLogTopN, OutError))
	{
		return false;
	}
	if (InGcEveryNBatches < 0)
	{
		OutError = TEXT("gc_every_n_batches must be >= 0");
		return false;
	}

	BatchSize = InBatchSize;
	LogTopN = InLogTopN;
	NextIndex = 0;
	bDeepMeshCheckEnabled = bInDeepMeshCheckEnabled;
	GcEveryNBatches = InGcEveryNBatches;
	AssetDatas = MoveTemp(InAssetDatas);
	Phase = EHCIAbilityKitAuditScanAsyncPhase::Running;
	LastFailureReason.Reset();

	RetryAssetDatas = AssetDatas;
	RetryBatchSize = BatchSize;
	RetryLogTopN = LogTopN;
	bRetryDeepMeshCheckEnabled = bDeepMeshCheckEnabled;
	RetryGcEveryNBatches = GcEveryNBatches;
	return true;
}

bool FHCIAbilityKitAuditScanAsyncController::Retry(FString& OutError)
{
	if (!CanRetry())
	{
		OutError = TEXT("Retry context is unavailable");
		return false;
	}

	StartFromStoredRetryContext();
	return true;
}

bool FHCIAbilityKitAuditScanAsyncController::Cancel(FString& OutError)
{
	if (Phase != EHCIAbilityKitAuditScanAsyncPhase::Running)
	{
		OutError = TEXT("Scan is not running");
		return false;
	}

	Phase = EHCIAbilityKitAuditScanAsyncPhase::Cancelled;
	return true;
}

FHCIAbilityKitAuditScanBatch FHCIAbilityKitAuditScanAsyncController::DequeueBatch()
{
	FHCIAbilityKitAuditScanBatch Batch;
	if (Phase != EHCIAbilityKitAuditScanAsyncPhase::Running)
	{
		return Batch;
	}

	if (NextIndex >= AssetDatas.Num())
	{
		return Batch;
	}

	Batch.bValid = true;
	Batch.StartIndex = NextIndex;
	Batch.EndIndex = FMath::Min(NextIndex + BatchSize, AssetDatas.Num());
	NextIndex = Batch.EndIndex;
	return Batch;
}

void FHCIAbilityKitAuditScanAsyncController::Complete()
{
	Phase = EHCIAbilityKitAuditScanAsyncPhase::Completed;
	LastFailureReason.Reset();
}

void FHCIAbilityKitAuditScanAsyncController::Fail(const FString& InFailureReason)
{
	Phase = EHCIAbilityKitAuditScanAsyncPhase::Failed;
	LastFailureReason = InFailureReason.IsEmpty()
		? TEXT("unknown_failure")
		: InFailureReason;
}

void FHCIAbilityKitAuditScanAsyncController::Reset(const bool bClearRetryContext)
{
	Phase = EHCIAbilityKitAuditScanAsyncPhase::Idle;
	BatchSize = 256;
	LogTopN = 10;
	NextIndex = 0;
	bDeepMeshCheckEnabled = false;
	GcEveryNBatches = 0;
	AssetDatas.Reset();
	LastFailureReason.Reset();

	if (bClearRetryContext)
	{
		RetryAssetDatas.Reset();
		RetryBatchSize = 256;
		RetryLogTopN = 10;
		bRetryDeepMeshCheckEnabled = false;
		RetryGcEveryNBatches = 0;
	}
}

int32 FHCIAbilityKitAuditScanAsyncController::GetProgressPercent() const
{
	const int32 TotalCount = AssetDatas.Num();
	if (TotalCount <= 0)
	{
		return 100;
	}

	return FMath::FloorToInt((static_cast<double>(NextIndex) / static_cast<double>(TotalCount)) * 100.0);
}

bool FHCIAbilityKitAuditScanAsyncController::ValidateArgs(
	const int32 InBatchSize,
	const int32 InLogTopN,
	FString& OutError)
{
	if (InBatchSize <= 0)
	{
		OutError = TEXT("batch_size must be >= 1");
		return false;
	}

	if (InLogTopN < 0)
	{
		OutError = TEXT("log_top_n must be >= 0");
		return false;
	}

	return true;
}

void FHCIAbilityKitAuditScanAsyncController::StartFromStoredRetryContext()
{
	BatchSize = RetryBatchSize;
	LogTopN = RetryLogTopN;
	NextIndex = 0;
	bDeepMeshCheckEnabled = bRetryDeepMeshCheckEnabled;
	GcEveryNBatches = RetryGcEveryNBatches;
	AssetDatas = RetryAssetDatas;
	Phase = EHCIAbilityKitAuditScanAsyncPhase::Running;
	LastFailureReason.Reset();
}
