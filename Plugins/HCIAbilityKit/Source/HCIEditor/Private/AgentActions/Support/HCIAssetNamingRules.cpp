#include "AgentActions/Support/HCIAssetNamingRules.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

namespace HCIAssetNamingRules
{
FString SanitizeIdentifier(const FString& InText)
{
	FString Out;
	Out.Reserve(InText.Len());
	bool bPrevUnderscore = false;
	for (TCHAR Ch : InText)
	{
		if (FChar::IsAlnum(Ch))
		{
			Out.AppendChar(Ch);
			bPrevUnderscore = false;
			continue;
		}

		if (!bPrevUnderscore)
		{
			Out.AppendChar(TEXT('_'));
			bPrevUnderscore = true;
		}
	}

	while (Out.StartsWith(TEXT("_")))
	{
		Out.RightChopInline(1, false);
	}
	while (Out.EndsWith(TEXT("_")))
	{
		Out.LeftChopInline(1, false);
	}

	if (Out.IsEmpty())
	{
		return FString();
	}

	if (FChar::IsDigit(Out[0]))
	{
		Out = FString::Printf(TEXT("A_%s"), *Out);
	}
	return Out;
}

FString RemoveKnownPrefixToken(const FString& InName)
{
	const int32 UnderscoreIndex = InName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (UnderscoreIndex <= 0 || UnderscoreIndex + 1 >= InName.Len())
	{
		return InName;
	}

	const FString PrefixToken = InName.Left(UnderscoreIndex).ToUpper();
	static const TSet<FString> KnownPrefixes = {
		TEXT("SM"),
		TEXT("SK"),
		TEXT("T"),
		TEXT("M"),
		TEXT("MI"),
		TEXT("BP"),
		TEXT("S"),
		TEXT("FX")};
	if (!KnownPrefixes.Contains(PrefixToken))
	{
		return InName;
	}

	return InName.Mid(UnderscoreIndex + 1);
}

FString DeriveClassPrefix(const UObject* Asset)
{
	if (Asset == nullptr)
	{
		return TEXT("A");
	}
	if (Asset->IsA(UStaticMesh::StaticClass()))
	{
		return TEXT("SM");
	}
	if (Asset->IsA(UTexture2D::StaticClass()))
	{
		return TEXT("T");
	}

	const FString ClassName = Asset->GetClass()->GetName();
	if (ClassName.Contains(TEXT("MaterialInstance")))
	{
		return TEXT("MI");
	}
	if (ClassName.Contains(TEXT("Material")))
	{
		return TEXT("M");
	}
	if (ClassName.Contains(TEXT("SkeletalMesh")))
	{
		return TEXT("SK");
	}
	return TEXT("A");
}

FString DeriveClassPrefixFromAssetData(const FAssetData& AssetData)
{
	const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
	if (ClassName.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase))
	{
		return TEXT("SM");
	}
	if (ClassName.Equals(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase))
	{
		return TEXT("SK");
	}
	if (ClassName.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase))
	{
		return TEXT("T");
	}
	if (ClassName.Contains(TEXT("MaterialInstance"), ESearchCase::IgnoreCase))
	{
		return TEXT("MI");
	}
	if (ClassName.Contains(TEXT("Material"), ESearchCase::IgnoreCase))
	{
		return TEXT("M");
	}
	return TEXT("A");
}
}


