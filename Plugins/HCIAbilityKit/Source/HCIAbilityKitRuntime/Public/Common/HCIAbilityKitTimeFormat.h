#pragma once

#include "CoreMinimal.h"

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitTimeFormat
{
	static constexpr int32 BeijingUtcOffsetHours = 8;

	// Keep internal timestamps in UTC, but emit a stable ISO-8601 string in Beijing time (+08:00).
	static FString FormatUtcAsBeijingIso8601(const FDateTime& UtcTime)
	{
		const FDateTime BeijingTime = UtcTime + FTimespan(BeijingUtcOffsetHours, 0, 0);
		return FString::Printf(
			TEXT("%04d-%02d-%02dT%02d:%02d:%02d+08:00"),
			BeijingTime.GetYear(),
			BeijingTime.GetMonth(),
			BeijingTime.GetDay(),
			BeijingTime.GetHour(),
			BeijingTime.GetMinute(),
			BeijingTime.GetSecond());
	}

	static FString FormatNowBeijingIso8601()
	{
		return FormatUtcAsBeijingIso8601(FDateTime::UtcNow());
	}
};
