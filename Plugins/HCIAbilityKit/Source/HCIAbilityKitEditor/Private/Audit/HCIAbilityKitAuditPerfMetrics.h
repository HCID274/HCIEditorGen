#pragma once

#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"

struct FHCIAbilityKitAuditPerfMetrics
{
	static double AssetsPerSecond(const int32 AssetCount, const double DurationMs)
	{
		if (AssetCount <= 0 || DurationMs <= 0.0)
		{
			return 0.0;
		}

		return (static_cast<double>(AssetCount) * 1000.0) / DurationMs;
	}

	static double PercentileNearestRank(const TArray<double>& Samples, const double Percentile)
	{
		if (Samples.Num() <= 0)
		{
			return 0.0;
		}

		TArray<double> Sorted = Samples;
		Sorted.Sort();

		const double ClampedPercentile = FMath::Clamp(Percentile, 0.0, 100.0);
		if (ClampedPercentile <= 0.0)
		{
			return Sorted[0];
		}

		const int32 RankOneBased = FMath::Clamp(
			FMath::CeilToInt((ClampedPercentile / 100.0) * static_cast<double>(Sorted.Num())),
			1,
			Sorted.Num());
		return Sorted[RankOneBased - 1];
	}
};
