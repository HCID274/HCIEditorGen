#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FHCIToolActionParamParser
{
public:
	explicit FHCIToolActionParamParser(const TSharedPtr<FJsonObject>& InArgs);

	bool TryGetRequiredString(const TCHAR* Field, FString& OutValue) const;
	bool TryGetRequiredInt(const TCHAR* Field, int32& OutValue) const;
	bool TryGetRequiredStringArray(const TCHAR* Field, TArray<FString>& OutValue) const;
	bool TryGetOptionalStringArray(const TCHAR* Field, TArray<FString>& OutValue) const;

	// Optional helper for the common "directory" arg.
	// NOTE: This intentionally does NOT trim the value (保持历史语义：带空格会视为无效并回退默认值)。
	bool TryGetOptionalStringFieldRaw(const TCHAR* Field, FString& OutValue) const;

private:
	TSharedPtr<FJsonObject> Args;
};


