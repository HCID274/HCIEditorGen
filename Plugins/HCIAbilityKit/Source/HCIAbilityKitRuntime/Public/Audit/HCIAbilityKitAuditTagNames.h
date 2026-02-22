#pragma once

#include "CoreMinimal.h"

namespace HCIAbilityKitAuditTagNames
{
	inline const FName Id = FName(TEXT("hci_id"));
	inline const FName DisplayName = FName(TEXT("hci_display_name"));
	inline const FName Damage = FName(TEXT("hci_damage"));
	inline const FName RepresentingMesh = FName(TEXT("hci_representing_mesh"));
	inline const FName TrianglesLod0 = FName(TEXT("hci_triangles_lod0"));
	inline const FName TriangleExpectedLod0 = FName(TEXT("hci_triangle_expected_lod0"));

	inline const TArray<FName>& GetTriangleCountTagCandidates()
	{
		static const TArray<FName> CandidateTags = {
			TrianglesLod0,
			FName(TEXT("triangle_count_lod0")),
			FName(TEXT("triangles_lod0")),
			FName(TEXT("lod0_triangles")),
			FName(TEXT("Triangles")),
			FName(TEXT("NumTriangles"))};
		return CandidateTags;
	}

	inline const TArray<FName>& GetTriangleExpectedTagCandidates()
	{
		static const TArray<FName> CandidateTags = {
			TriangleExpectedLod0,
			FName(TEXT("triangle_count_lod0_expected")),
			FName(TEXT("triangle_expected_lod0")),
			FName(TEXT("hci_triangle_count_lod0_expected"))};
		return CandidateTags;
	}

	inline bool TryParseTriangleCountTagValue(const FString& RawValue, int32& OutTriangleCount)
	{
		FString Normalized = RawValue;
		Normalized.TrimStartAndEndInline();
		Normalized.ReplaceInline(TEXT(","), TEXT(""));
		Normalized.ReplaceInline(TEXT(" "), TEXT(""));

		int64 ParsedInt = 0;
		if (LexTryParseString(ParsedInt, *Normalized))
		{
			if (ParsedInt >= 0 && ParsedInt <= MAX_int32)
			{
				OutTriangleCount = static_cast<int32>(ParsedInt);
				return true;
			}
			return false;
		}

		double ParsedDouble = 0.0;
		if (LexTryParseString(ParsedDouble, *Normalized))
		{
			if (ParsedDouble >= 0.0 && ParsedDouble <= static_cast<double>(MAX_int32))
			{
				OutTriangleCount = FMath::RoundToInt(ParsedDouble);
				return true;
			}
			return false;
		}

		FString DigitsOnly;
		DigitsOnly.Reserve(Normalized.Len());
		for (const TCHAR Char : Normalized)
		{
			if (FChar::IsDigit(Char))
			{
				DigitsOnly.AppendChar(Char);
			}
		}

		if (DigitsOnly.IsEmpty())
		{
			return false;
		}

		if (LexTryParseString(ParsedInt, *DigitsOnly) && ParsedInt <= MAX_int32)
		{
			OutTriangleCount = static_cast<int32>(ParsedInt);
			return true;
		}
		return false;
	}

	template <typename TagGetterFuncType>
	bool TryResolveTriangleCountFromTags(
		TagGetterFuncType&& TryGetTagValue,
		int32& OutTriangleCount,
		FName& OutSourceTagKey)
	{
		for (const FName& CandidateTag : GetTriangleCountTagCandidates())
		{
			FString RawValue;
			if (!TryGetTagValue(CandidateTag, RawValue))
			{
				continue;
			}

			if (TryParseTriangleCountTagValue(RawValue, OutTriangleCount))
			{
				OutSourceTagKey = CandidateTag;
				return true;
			}
		}
		return false;
	}

	template <typename TagGetterFuncType>
	bool TryResolveTriangleExpectedFromTags(
		TagGetterFuncType&& TryGetTagValue,
		int32& OutTriangleCountExpected,
		FName& OutSourceTagKey)
	{
		for (const FName& CandidateTag : GetTriangleExpectedTagCandidates())
		{
			FString RawValue;
			if (!TryGetTagValue(CandidateTag, RawValue))
			{
				continue;
			}

			if (TryParseTriangleCountTagValue(RawValue, OutTriangleCountExpected))
			{
				OutSourceTagKey = CandidateTag;
				return true;
			}
		}

		return false;
	}
}
