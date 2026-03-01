#include "Audit/HCIAuditScanAsyncController.h"

#include "Math/UnrealMathUtility.h"

bool FHCIAuditScanAsyncController::Start(
	TArray<FAssetData>&& InAssetDatas,
	const int32 InBatchSize,
	const int32 InLogTopN,
	FString& OutError)
{
	return Start(MoveTemp(InAssetDatas), InBatchSize, InLogTopN, false, 0, OutError);
}

bool FHCIAuditScanAsyncController::Start(
	TArray<FAssetData>&& InAssetDatas,
	const int32 InBatchSize,
	const int32 InLogTopN,
	const bool bInDeepMeshCheckEnabled,
	FString& OutError)
{
	return Start(MoveTemp(InAssetDatas), InBatchSize, InLogTopN, bInDeepMeshCheckEnabled, 0, OutError);
}

bool FHCIAuditScanAsyncController::Start(
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
	Phase = EHCIAuditScanAsyncPhase::Running;
	LastFailureReason.Reset();

	RetryAssetDatas = AssetDatas;
	RetryBatchSize = BatchSize;
	RetryLogTopN = LogTopN;
	bRetryDeepMeshCheckEnabled = bDeepMeshCheckEnabled;
	RetryGcEveryNBatches = GcEveryNBatches;
	return true;
}

bool FHCIAuditScanAsyncController::Retry(FString& OutError)
{
	if (!CanRetry())
	{
		OutError = TEXT("Retry context is unavailable");
		return false;
	}

	StartFromStoredRetryContext();
	return true;
}

bool FHCIAuditScanAsyncController::Cancel(FString& OutError)
{
	if (Phase != EHCIAuditScanAsyncPhase::Running)
	{
		OutError = TEXT("Scan is not running");
		return false;
	}

	Phase = EHCIAuditScanAsyncPhase::Cancelled;
	return true;
}

FHCIAuditScanBatch FHCIAuditScanAsyncController::DequeueBatch()
{
	FHCIAuditScanBatch Batch;
	if (Phase != EHCIAuditScanAsyncPhase::Running)
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

void FHCIAuditScanAsyncController::Complete()
{
	Phase = EHCIAuditScanAsyncPhase::Completed;
	LastFailureReason.Reset();
}

void FHCIAuditScanAsyncController::Fail(const FString& InFailureReason)
{
	Phase = EHCIAuditScanAsyncPhase::Failed;
	LastFailureReason = InFailureReason.IsEmpty()
		? TEXT("unknown_failure")
		: InFailureReason;
}

void FHCIAuditScanAsyncController::Reset(const bool bClearRetryContext)
{
	Phase = EHCIAuditScanAsyncPhase::Idle;
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

int32 FHCIAuditScanAsyncController::GetProgressPercent() const
{
	const int32 TotalCount = AssetDatas.Num();
	if (TotalCount <= 0)
	{
		return 100;
	}

	return FMath::FloorToInt((static_cast<double>(NextIndex) / static_cast<double>(TotalCount)) * 100.0);
}

bool FHCIAuditScanAsyncController::ValidateArgs(
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

void FHCIAuditScanAsyncController::StartFromStoredRetryContext()
{
	BatchSize = RetryBatchSize;
	LogTopN = RetryLogTopN;
	NextIndex = 0;
	bDeepMeshCheckEnabled = bRetryDeepMeshCheckEnabled;
	GcEveryNBatches = RetryGcEveryNBatches;
	AssetDatas = RetryAssetDatas;
	Phase = EHCIAuditScanAsyncPhase::Running;
	LastFailureReason.Reset();
}

