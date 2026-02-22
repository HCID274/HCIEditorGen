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
	inline const FName MeshLods = FName(TEXT("LODs"));
	inline const FName MeshNaniteEnabled = FName(TEXT("NaniteEnabled"));
	inline const FName TextureDimensions = FName(TEXT("Dimensions"));

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

	inline const TArray<FName>& GetMeshLodCountTagCandidates()
	{
		static const TArray<FName> CandidateTags = {
			MeshLods,
			FName(TEXT("NumLODs"))};
		return CandidateTags;
	}

	inline const TArray<FName>& GetMeshNaniteEnabledTagCandidates()
	{
		static const TArray<FName> CandidateTags = {
			MeshNaniteEnabled,
			FName(TEXT("bNaniteEnabled"))};
		return CandidateTags;
	}

	inline const TArray<FName>& GetTextureDimensionsTagCandidates()
	{
		static const TArray<FName> CandidateTags = {
			TextureDimensions,
			FName(TEXT("ImportedSize"))};
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

	inline bool TryParseBoolTagValue(const FString& RawValue, bool& OutValue)
	{
		FString Normalized = RawValue;
		Normalized.TrimStartAndEndInline();

		if (Normalized.Equals(TEXT("True"), ESearchCase::IgnoreCase)
			|| Normalized.Equals(TEXT("1"), ESearchCase::IgnoreCase))
		{
			OutValue = true;
			return true;
		}

		if (Normalized.Equals(TEXT("False"), ESearchCase::IgnoreCase)
			|| Normalized.Equals(TEXT("0"), ESearchCase::IgnoreCase))
		{
			OutValue = false;
			return true;
		}

		return false;
	}

	inline bool TryParseTextureDimensionsTagValue(const FString& RawValue, int32& OutWidth, int32& OutHeight)
	{
		FString Normalized = RawValue;
		Normalized.TrimStartAndEndInline();

		int32 SeparatorIndex = INDEX_NONE;
		if (!Normalized.FindChar(TEXT('x'), SeparatorIndex) && !Normalized.FindChar(TEXT('X'), SeparatorIndex))
		{
			return false;
		}

		const FString Left = Normalized.Left(SeparatorIndex).TrimStartAndEnd();
		const FString Right = Normalized.Mid(SeparatorIndex + 1).TrimStartAndEnd();

		int32 ParsedWidth = INDEX_NONE;
		int32 ParsedHeight = INDEX_NONE;
		if (!LexTryParseString(ParsedWidth, *Left) || !LexTryParseString(ParsedHeight, *Right))
		{
			return false;
		}

		if (ParsedWidth <= 0 || ParsedHeight <= 0)
		{
			return false;
		}

		OutWidth = ParsedWidth;
		OutHeight = ParsedHeight;
		return true;
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

	template <typename TagGetterFuncType>
	bool TryResolveMeshLodCountFromTags(
		TagGetterFuncType&& TryGetTagValue,
		int32& OutMeshLodCount,
		FName& OutSourceTagKey)
	{
		for (const FName& CandidateTag : GetMeshLodCountTagCandidates())
		{
			FString RawValue;
			if (!TryGetTagValue(CandidateTag, RawValue))
			{
				continue;
			}

			if (TryParseTriangleCountTagValue(RawValue, OutMeshLodCount))
			{
				OutSourceTagKey = CandidateTag;
				return true;
			}
		}

		return false;
	}

	template <typename TagGetterFuncType>
	bool TryResolveMeshNaniteEnabledFromTags(
		TagGetterFuncType&& TryGetTagValue,
		bool& OutNaniteEnabled,
		FName& OutSourceTagKey)
	{
		for (const FName& CandidateTag : GetMeshNaniteEnabledTagCandidates())
		{
			FString RawValue;
			if (!TryGetTagValue(CandidateTag, RawValue))
			{
				continue;
			}

			if (TryParseBoolTagValue(RawValue, OutNaniteEnabled))
			{
				OutSourceTagKey = CandidateTag;
				return true;
			}
		}

		return false;
	}

	template <typename TagGetterFuncType>
	bool TryResolveTextureDimensionsFromTags(
		TagGetterFuncType&& TryGetTagValue,
		int32& OutWidth,
		int32& OutHeight,
		FName& OutSourceTagKey)
	{
		for (const FName& CandidateTag : GetTextureDimensionsTagCandidates())
		{
			FString RawValue;
			if (!TryGetTagValue(CandidateTag, RawValue))
			{
				continue;
			}

			if (TryParseTextureDimensionsTagValue(RawValue, OutWidth, OutHeight))
			{
				OutSourceTagKey = CandidateTag;
				return true;
			}
		}

		return false;
	}
}
