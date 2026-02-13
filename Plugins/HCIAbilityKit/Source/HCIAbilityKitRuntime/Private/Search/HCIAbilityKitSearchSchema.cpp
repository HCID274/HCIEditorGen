#include "Search/HCIAbilityKitSearchSchema.h"

#include "Containers/Set.h"

namespace
{
FString MakeSearchableText(const FString& Id, const FString& DisplayName)
{
	return (Id + TEXT(" ") + DisplayName).ToLower();
}

bool ContainsAnyKeyword(const FString& Text, const TArray<const TCHAR*>& Keywords)
{
	for (const TCHAR* Keyword : Keywords)
	{
		if (Text.Contains(Keyword))
		{
			return true;
		}
	}
	return false;
}

void AppendElementTokens(TSet<FString>& OutTokens, EHCIAbilityElement Element)
{
	switch (Element)
	{
	case EHCIAbilityElement::Fire:
		OutTokens.Add(TEXT("fire"));
		OutTokens.Add(TEXT("flame"));
		OutTokens.Add(TEXT("火"));
		break;
	case EHCIAbilityElement::Ice:
		OutTokens.Add(TEXT("ice"));
		OutTokens.Add(TEXT("frost"));
		OutTokens.Add(TEXT("冰"));
		break;
	case EHCIAbilityElement::Nature:
		OutTokens.Add(TEXT("nature"));
		OutTokens.Add(TEXT("forest"));
		OutTokens.Add(TEXT("森林"));
		break;
	case EHCIAbilityElement::Lightning:
		OutTokens.Add(TEXT("lightning"));
		OutTokens.Add(TEXT("thunder"));
		OutTokens.Add(TEXT("雷"));
		break;
	case EHCIAbilityElement::Physical:
		OutTokens.Add(TEXT("physical"));
		OutTokens.Add(TEXT("物理"));
		break;
	default:
		break;
	}
}

void AppendDamageTierTokens(TSet<FString>& OutTokens, EHCIAbilityDamageTier DamageTier)
{
	switch (DamageTier)
	{
	case EHCIAbilityDamageTier::Low:
		OutTokens.Add(TEXT("low_damage"));
		OutTokens.Add(TEXT("low"));
		OutTokens.Add(TEXT("不高"));
		break;
	case EHCIAbilityDamageTier::Medium:
		OutTokens.Add(TEXT("mid_damage"));
		OutTokens.Add(TEXT("medium"));
		OutTokens.Add(TEXT("中等"));
		break;
	case EHCIAbilityDamageTier::High:
		OutTokens.Add(TEXT("high_damage"));
		OutTokens.Add(TEXT("high"));
		OutTokens.Add(TEXT("高爆发"));
		break;
	default:
		break;
	}
}

void AppendControlTokens(TSet<FString>& OutTokens, EHCIAbilityControlProfile ControlProfile)
{
	switch (ControlProfile)
	{
	case EHCIAbilityControlProfile::SoftControl:
		OutTokens.Add(TEXT("soft_control"));
		OutTokens.Add(TEXT("slow"));
		OutTokens.Add(TEXT("减速"));
		break;
	case EHCIAbilityControlProfile::HardControl:
		OutTokens.Add(TEXT("hard_control"));
		OutTokens.Add(TEXT("stun"));
		OutTokens.Add(TEXT("硬控"));
		break;
	default:
		break;
	}
}

void AppendUsageSceneTokens(TSet<FString>& OutTokens, const TArray<EHCIAbilityUsageScene>& UsageScenes)
{
	for (const EHCIAbilityUsageScene Scene : UsageScenes)
	{
		switch (Scene)
		{
		case EHCIAbilityUsageScene::Forest:
			OutTokens.Add(TEXT("forest"));
			OutTokens.Add(TEXT("jungle"));
			OutTokens.Add(TEXT("森林"));
			break;
		case EHCIAbilityUsageScene::BossPhase2:
			OutTokens.Add(TEXT("boss"));
			OutTokens.Add(TEXT("phase2"));
			OutTokens.Add(TEXT("二阶段"));
			break;
		default:
			break;
		}
	}
}

template <typename TKey>
void RemoveIdFromBuckets(TMap<TKey, TArray<FString>>& Buckets, const TKey& Key, const FString& Id)
{
	if (TArray<FString>* Bucket = Buckets.Find(Key))
	{
		Bucket->Remove(Id);
		if (Bucket->Num() == 0)
		{
			Buckets.Remove(Key);
		}
	}
}
}

bool FHCIAbilitySearchDocument::IsValid() const
{
	return !Id.IsEmpty();
}

void FHCIAbilitySearchIndex::Reset()
{
	DocumentsById.Reset();
	IdsByElement.Reset();
	IdsByDamageTier.Reset();
	IdsByControlProfile.Reset();
	IdsByUsageScene.Reset();
	IdsByToken.Reset();
}

int32 FHCIAbilitySearchIndex::GetDocumentCount() const
{
	return DocumentsById.Num();
}

bool FHCIAbilitySearchIndex::ContainsId(const FString& InId) const
{
	return DocumentsById.Contains(InId);
}

bool FHCIAbilitySearchIndex::AddDocument(const FHCIAbilitySearchDocument& InDocument)
{
	if (!InDocument.IsValid() || DocumentsById.Contains(InDocument.Id))
	{
		return false;
	}

	DocumentsById.Add(InDocument.Id, InDocument);
	IdsByElement.FindOrAdd(InDocument.Element).Add(InDocument.Id);
	IdsByDamageTier.FindOrAdd(InDocument.DamageTier).Add(InDocument.Id);
	IdsByControlProfile.FindOrAdd(InDocument.ControlProfile).Add(InDocument.Id);

	for (const EHCIAbilityUsageScene Scene : InDocument.UsageScenes)
	{
		IdsByUsageScene.FindOrAdd(Scene).Add(InDocument.Id);
	}

	for (const FString& Token : InDocument.Tokens)
	{
		IdsByToken.FindOrAdd(Token).Add(InDocument.Id);
	}

	return true;
}

bool FHCIAbilitySearchIndex::RemoveDocumentById(const FString& InId)
{
	FHCIAbilitySearchDocument ExistingDocument;
	if (!DocumentsById.RemoveAndCopyValue(InId, ExistingDocument))
	{
		return false;
	}

	RemoveIdFromBuckets(IdsByElement, ExistingDocument.Element, InId);
	RemoveIdFromBuckets(IdsByDamageTier, ExistingDocument.DamageTier, InId);
	RemoveIdFromBuckets(IdsByControlProfile, ExistingDocument.ControlProfile, InId);

	for (const EHCIAbilityUsageScene Scene : ExistingDocument.UsageScenes)
	{
		RemoveIdFromBuckets(IdsByUsageScene, Scene, InId);
	}

	for (const FString& Token : ExistingDocument.Tokens)
	{
		RemoveIdFromBuckets(IdsByToken, Token, InId);
	}

	return true;
}

FHCIAbilitySearchDocument FHCIAbilitySearchSchema::BuildDocument(
	const FHCIAbilityKitParsedData& ParsedData,
	const FString& AssetPath)
{
	FHCIAbilitySearchDocument Document;
	Document.AssetPath = AssetPath;
	Document.Id = ParsedData.Id;
	Document.DisplayName = ParsedData.DisplayName;
	Document.Damage = ParsedData.Damage;
	Document.Element = ResolveElement(ParsedData.Id, ParsedData.DisplayName);
	Document.DamageTier = ResolveDamageTier(ParsedData.Damage);
	Document.ControlProfile = ResolveControlProfile(ParsedData.Id, ParsedData.DisplayName);
	Document.UsageScenes = ResolveUsageScenes(ParsedData.Id, ParsedData.DisplayName);
	Document.Tokens = BuildTokens(
		ParsedData.Id,
		ParsedData.DisplayName,
		Document.Element,
		Document.DamageTier,
		Document.ControlProfile,
		Document.UsageScenes);
	return Document;
}

EHCIAbilityElement FHCIAbilitySearchSchema::ResolveElement(const FString& Id, const FString& DisplayName)
{
	const FString Text = MakeSearchableText(Id, DisplayName);

	if (ContainsAnyKeyword(Text, { TEXT("fire"), TEXT("flame"), TEXT("burn"), TEXT("火") }))
	{
		return EHCIAbilityElement::Fire;
	}
	if (ContainsAnyKeyword(Text, { TEXT("ice"), TEXT("frost"), TEXT("chill"), TEXT("冰") }))
	{
		return EHCIAbilityElement::Ice;
	}
	if (ContainsAnyKeyword(Text, { TEXT("nature"), TEXT("forest"), TEXT("vine"), TEXT("wood"), TEXT("森林") }))
	{
		return EHCIAbilityElement::Nature;
	}
	if (ContainsAnyKeyword(Text, { TEXT("lightning"), TEXT("thunder"), TEXT("shock"), TEXT("雷") }))
	{
		return EHCIAbilityElement::Lightning;
	}

	return EHCIAbilityElement::Physical;
}

EHCIAbilityDamageTier FHCIAbilitySearchSchema::ResolveDamageTier(const float Damage)
{
	if (Damage <= 120.0f)
	{
		return EHCIAbilityDamageTier::Low;
	}
	if (Damage <= 300.0f)
	{
		return EHCIAbilityDamageTier::Medium;
	}
	return EHCIAbilityDamageTier::High;
}

EHCIAbilityControlProfile FHCIAbilitySearchSchema::ResolveControlProfile(
	const FString& Id,
	const FString& DisplayName)
{
	const FString Text = MakeSearchableText(Id, DisplayName);

	if (ContainsAnyKeyword(Text, { TEXT("stun"), TEXT("freeze"), TEXT("root"), TEXT("knockup"), TEXT("眩晕"), TEXT("冰冻"), TEXT("禁锢") }))
	{
		return EHCIAbilityControlProfile::HardControl;
	}
	if (ContainsAnyKeyword(Text, { TEXT("slow"), TEXT("chill"), TEXT("snare"), TEXT("减速"), TEXT("迟缓") }))
	{
		return EHCIAbilityControlProfile::SoftControl;
	}
	return EHCIAbilityControlProfile::None;
}

TArray<EHCIAbilityUsageScene> FHCIAbilitySearchSchema::ResolveUsageScenes(
	const FString& Id,
	const FString& DisplayName)
{
	const FString Text = MakeSearchableText(Id, DisplayName);
	TArray<EHCIAbilityUsageScene> Scenes;
	Scenes.Add(EHCIAbilityUsageScene::General);

	if (ContainsAnyKeyword(Text, { TEXT("forest"), TEXT("jungle"), TEXT("wood"), TEXT("森林") }))
	{
		Scenes.Add(EHCIAbilityUsageScene::Forest);
	}
	if (ContainsAnyKeyword(Text, { TEXT("boss"), TEXT("phase2"), TEXT("p2"), TEXT("二阶段") }))
	{
		Scenes.Add(EHCIAbilityUsageScene::BossPhase2);
	}

	return Scenes;
}

TArray<FString> FHCIAbilitySearchSchema::BuildTokens(
	const FString& Id,
	const FString& DisplayName,
	const EHCIAbilityElement Element,
	const EHCIAbilityDamageTier DamageTier,
	const EHCIAbilityControlProfile ControlProfile,
	const TArray<EHCIAbilityUsageScene>& UsageScenes)
{
	TSet<FString> TokenSet;
	TokenSet.Reserve(16);

	const FString LowerId = Id.ToLower();
	if (!LowerId.IsEmpty())
	{
		TokenSet.Add(LowerId);
	}

	const FString LowerDisplayName = DisplayName.ToLower();
	if (!LowerDisplayName.IsEmpty())
	{
		TokenSet.Add(LowerDisplayName);
	}

	TArray<FString> DisplayNameTokens;
	LowerDisplayName.ParseIntoArrayWS(DisplayNameTokens);
	for (const FString& Token : DisplayNameTokens)
	{
		if (!Token.IsEmpty())
		{
			TokenSet.Add(Token);
		}
	}

	AppendElementTokens(TokenSet, Element);
	AppendDamageTierTokens(TokenSet, DamageTier);
	AppendControlTokens(TokenSet, ControlProfile);
	AppendUsageSceneTokens(TokenSet, UsageScenes);

	TArray<FString> Tokens = TokenSet.Array();
	Tokens.Sort();
	return Tokens;
}
