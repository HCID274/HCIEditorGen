#include "Search/HCIAbilityKitSearchQueryService.h"

#include "Algo/Sort.h"
#include "Containers/Set.h"

namespace
{
constexpr int32 MinTopK = 1;
constexpr int32 MaxTopK = 10;

FString MakeNormalizedText(const FString& Input)
{
	FString Output = Input.ToLower();
	static const TArray<const TCHAR*> ReplacedTokens = {
		TEXT(","), TEXT("，"), TEXT("."), TEXT("。"), TEXT(";"), TEXT("；"),
		TEXT(":"), TEXT("："), TEXT("/"), TEXT("\\"), TEXT("\""), TEXT("'"),
		TEXT("("), TEXT(")"), TEXT("["), TEXT("]"), TEXT("{"), TEXT("}"),
		TEXT("|"), TEXT("!"), TEXT("?"), TEXT("？")
	};

	for (const TCHAR* ReplacedToken : ReplacedTokens)
	{
		Output.ReplaceInline(ReplacedToken, TEXT(" "));
	}
	return Output;
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

TArray<FString> TokenizeQuery(const FString& NormalizedText)
{
	TArray<FString> RawTokens;
	NormalizedText.ParseIntoArrayWS(RawTokens);

	TSet<FString> TokenSet;
	for (const FString& Token : RawTokens)
	{
		if (!Token.IsEmpty())
		{
			TokenSet.Add(Token);
		}
	}

	TArray<FString> Tokens = TokenSet.Array();
	Tokens.Sort();
	return Tokens;
}

template <typename TEnum>
void IntersectWithBucket(
	const TMap<TEnum, TArray<FString>>& Buckets,
	const TEnum Key,
	TSet<FString>& InOutCandidateIds)
{
	const TArray<FString>* Bucket = Buckets.Find(Key);
	if (!Bucket)
	{
		InOutCandidateIds.Reset();
		return;
	}

	TSet<FString> BucketSet;
	BucketSet.Reserve(Bucket->Num());
	for (const FString& Id : *Bucket)
	{
		BucketSet.Add(Id);
	}
	for (auto It = InOutCandidateIds.CreateIterator(); It; ++It)
	{
		if (!BucketSet.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
}

const TCHAR* ToText(const EHCIAbilityElement Value)
{
	switch (Value)
	{
	case EHCIAbilityElement::Fire: return TEXT("fire");
	case EHCIAbilityElement::Ice: return TEXT("ice");
	case EHCIAbilityElement::Nature: return TEXT("nature");
	case EHCIAbilityElement::Lightning: return TEXT("lightning");
	case EHCIAbilityElement::Physical: return TEXT("physical");
	default: return TEXT("unknown");
	}
}

const TCHAR* ToText(const EHCIAbilityControlProfile Value)
{
	switch (Value)
	{
	case EHCIAbilityControlProfile::SoftControl: return TEXT("soft_control");
	case EHCIAbilityControlProfile::HardControl: return TEXT("hard_control");
	default: return TEXT("none");
	}
}

const TCHAR* ToText(const EHCIAbilityUsageScene Value)
{
	switch (Value)
	{
	case EHCIAbilityUsageScene::Forest: return TEXT("forest");
	case EHCIAbilityUsageScene::BossPhase2: return TEXT("boss_phase2");
	default: return TEXT("general");
	}
}

const TCHAR* ToText(const EHCIAbilityDamagePreference Value)
{
	switch (Value)
	{
	case EHCIAbilityDamagePreference::PreferLow: return TEXT("prefer_low");
	case EHCIAbilityDamagePreference::PreferMedium: return TEXT("prefer_medium");
	case EHCIAbilityDamagePreference::PreferHigh: return TEXT("prefer_high");
	default: return TEXT("any");
	}
}

float ScoreByDamagePreference(
	const EHCIAbilityDamagePreference Preference,
	const EHCIAbilityDamageTier CandidateTier)
{
	switch (Preference)
	{
	case EHCIAbilityDamagePreference::PreferLow:
		return CandidateTier == EHCIAbilityDamageTier::Low ? 2.0f : (CandidateTier == EHCIAbilityDamageTier::Medium ? 1.0f : -1.0f);
	case EHCIAbilityDamagePreference::PreferMedium:
		return CandidateTier == EHCIAbilityDamageTier::Medium ? 2.0f : 0.0f;
	case EHCIAbilityDamagePreference::PreferHigh:
		return CandidateTier == EHCIAbilityDamageTier::High ? 2.0f : -0.5f;
	default:
		return 0.0f;
	}
}

bool ContainsScene(const FHCIAbilitySearchDocument& Document, const EHCIAbilityUsageScene Scene)
{
	return Document.UsageScenes.Contains(Scene);
}
}

FString FHCIAbilitySearchQuery::ToSummaryString() const
{
	TArray<FString> Fields;
	Fields.Reserve(8);
	Fields.Add(FString::Printf(TEXT("topk=%d"), TopK));
	Fields.Add(FString::Printf(TEXT("damage=%s"), ToText(DamagePreference)));
	Fields.Add(FString::Printf(TEXT("require_any_control=%s"), bRequireAnyControl ? TEXT("true") : TEXT("false")));
	Fields.Add(FString::Printf(TEXT("lower_than_ref=%s"), bRequireLowerDamageThanReference ? TEXT("true") : TEXT("false")));

	if (RequiredElement.IsSet())
	{
		Fields.Add(FString::Printf(TEXT("element=%s"), ToText(RequiredElement.GetValue())));
	}
	if (RequiredControlProfile.IsSet())
	{
		Fields.Add(FString::Printf(TEXT("control=%s"), ToText(RequiredControlProfile.GetValue())));
	}
	if (RequiredScenes.Num() > 0)
	{
		TArray<FString> SceneTexts;
		for (const EHCIAbilityUsageScene Scene : RequiredScenes)
		{
			SceneTexts.Add(ToText(Scene));
		}
		Fields.Add(FString::Printf(TEXT("scenes=[%s]"), *FString::Join(SceneTexts, TEXT(","))));
	}
	if (!SimilarToId.IsEmpty())
	{
		Fields.Add(FString::Printf(TEXT("similar_to=%s"), *SimilarToId));
	}

	return FString::Join(Fields, TEXT(" "));
}

bool FHCIAbilitySearchResult::HasResults() const
{
	return Candidates.Num() > 0;
}

FString FHCIAbilitySearchResult::ToSummaryString() const
{
	TArray<FString> TopCandidateTexts;
	const int32 DisplayCount = FMath::Min(ParsedQuery.TopK, Candidates.Num());
	for (int32 Index = 0; Index < DisplayCount; ++Index)
	{
		const FHCIAbilitySearchCandidate& Candidate = Candidates[Index];
		TopCandidateTexts.Add(FString::Printf(TEXT("%s(%.2f)"), *Candidate.Id, Candidate.Score));
	}

	return FString::Printf(
		TEXT("candidates=%d topk=%d top_ids=%s"),
		Candidates.Num(),
		DisplayCount,
		*FString::Join(TopCandidateTexts, TEXT(",")));
}

FHCIAbilitySearchQuery FHCIAbilityKitSearchQueryService::ParseQuery(
	const FString& UserQuery,
	const int32 TopK,
	const FHCIAbilitySearchIndex& Index)
{
	FHCIAbilitySearchQuery Query;
	Query.RawText = UserQuery;
	Query.NormalizedText = MakeNormalizedText(UserQuery);
	Query.TopK = FMath::Clamp(TopK, MinTopK, MaxTopK);
	Query.QueryTokens = TokenizeQuery(Query.NormalizedText);

	const FString& Text = Query.NormalizedText;

	if (ContainsAnyKeyword(Text, { TEXT("fire"), TEXT("flame"), TEXT("burn"), TEXT("火") }))
	{
		Query.RequiredElement = EHCIAbilityElement::Fire;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("ice"), TEXT("frost"), TEXT("chill"), TEXT("冰") }))
	{
		Query.RequiredElement = EHCIAbilityElement::Ice;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("nature"), TEXT("forest"), TEXT("jungle"), TEXT("森林") }))
	{
		Query.RequiredElement = EHCIAbilityElement::Nature;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("lightning"), TEXT("thunder"), TEXT("shock"), TEXT("雷") }))
	{
		Query.RequiredElement = EHCIAbilityElement::Lightning;
	}

	if (ContainsAnyKeyword(Text, { TEXT("forest"), TEXT("jungle"), TEXT("森林") }))
	{
		Query.RequiredScenes.AddUnique(EHCIAbilityUsageScene::Forest);
	}
	if (ContainsAnyKeyword(Text, { TEXT("boss"), TEXT("phase2"), TEXT("二阶段") }))
	{
		Query.RequiredScenes.AddUnique(EHCIAbilityUsageScene::BossPhase2);
	}

	if (ContainsAnyKeyword(Text, { TEXT("hard control"), TEXT("hard_control"), TEXT("stun"), TEXT("freeze"), TEXT("眩晕"), TEXT("冰冻"), TEXT("硬控") }))
	{
		Query.RequiredControlProfile = EHCIAbilityControlProfile::HardControl;
		Query.bRequireAnyControl = true;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("soft control"), TEXT("soft_control"), TEXT("slow"), TEXT("减速"), TEXT("软控") }))
	{
		Query.RequiredControlProfile = EHCIAbilityControlProfile::SoftControl;
		Query.bRequireAnyControl = true;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("control"), TEXT("控制"), TEXT("控场"), TEXT("控制倾向") }))
	{
		Query.bRequireAnyControl = true;
	}

	if (ContainsAnyKeyword(Text, { TEXT("high burst"), TEXT("high_damage"), TEXT("高爆发"), TEXT("高伤"), TEXT("高伤害"), TEXT("爆发") }))
	{
		Query.DamagePreference = EHCIAbilityDamagePreference::PreferHigh;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("medium"), TEXT("中等") }))
	{
		Query.DamagePreference = EHCIAbilityDamagePreference::PreferMedium;
	}
	else if (ContainsAnyKeyword(Text, { TEXT("low"), TEXT("lower"), TEXT("不高"), TEXT("更低"), TEXT("低伤") }))
	{
		Query.DamagePreference = EHCIAbilityDamagePreference::PreferLow;
	}

	for (const TPair<FString, FHCIAbilitySearchDocument>& Pair : Index.DocumentsById)
	{
		if (Text.Contains(Pair.Key.ToLower()))
		{
			Query.SimilarToId = Pair.Key;
			break;
		}
	}

	if (!Query.SimilarToId.IsEmpty() && ContainsAnyKeyword(Text, { TEXT("lower"), TEXT("更低"), TEXT("不高"), TEXT("低") }))
	{
		Query.bRequireLowerDamageThanReference = true;
	}

	return Query;
}

FHCIAbilitySearchResult FHCIAbilityKitSearchQueryService::RunQuery(
	const FString& UserQuery,
	const int32 TopK,
	const FHCIAbilitySearchIndex& Index)
{
	FHCIAbilitySearchResult Result;
	Result.ParsedQuery = ParseQuery(UserQuery, TopK, Index);

	if (Index.DocumentsById.Num() == 0)
	{
		Result.Suggestions.Add(TEXT("当前索引为空，请先导入或重导入至少一个 AbilityKit 资产。"));
		return Result;
	}

	TSet<FString> CandidateIds;
	for (const TPair<FString, FHCIAbilitySearchDocument>& Pair : Index.DocumentsById)
	{
		CandidateIds.Add(Pair.Key);
	}

	if (Result.ParsedQuery.RequiredElement.IsSet())
	{
		IntersectWithBucket(Index.IdsByElement, Result.ParsedQuery.RequiredElement.GetValue(), CandidateIds);
	}

	if (Result.ParsedQuery.RequiredControlProfile.IsSet())
	{
		IntersectWithBucket(Index.IdsByControlProfile, Result.ParsedQuery.RequiredControlProfile.GetValue(), CandidateIds);
	}
	else if (Result.ParsedQuery.bRequireAnyControl)
	{
		for (auto It = CandidateIds.CreateIterator(); It; ++It)
		{
			const FHCIAbilitySearchDocument* Document = Index.DocumentsById.Find(*It);
			if (!Document || Document->ControlProfile == EHCIAbilityControlProfile::None)
			{
				It.RemoveCurrent();
			}
		}
	}

	for (const EHCIAbilityUsageScene Scene : Result.ParsedQuery.RequiredScenes)
	{
		IntersectWithBucket(Index.IdsByUsageScene, Scene, CandidateIds);
	}

	const FHCIAbilitySearchDocument* ReferenceDocument = nullptr;
	if (!Result.ParsedQuery.SimilarToId.IsEmpty())
	{
		ReferenceDocument = Index.DocumentsById.Find(Result.ParsedQuery.SimilarToId);
	}

	if (Result.ParsedQuery.bRequireLowerDamageThanReference && ReferenceDocument)
	{
		for (auto It = CandidateIds.CreateIterator(); It; ++It)
		{
			const FHCIAbilitySearchDocument* Document = Index.DocumentsById.Find(*It);
			if (!Document || Document->Damage >= ReferenceDocument->Damage)
			{
				It.RemoveCurrent();
			}
		}
	}

	if (CandidateIds.Num() == 0)
	{
		Result.Suggestions.Add(TEXT("未命中候选：先放宽场景或控制条件再检索一次。"));
		if (Result.ParsedQuery.bRequireLowerDamageThanReference)
		{
			Result.Suggestions.Add(TEXT("可尝试取消“更低伤害”限制，先确认相似技能范围。"));
		}
		if (!Result.ParsedQuery.SimilarToId.IsEmpty() && !ReferenceDocument)
		{
			Result.Suggestions.Add(FString::Printf(TEXT("参考技能 %s 不存在，请确认资产 ID。"), *Result.ParsedQuery.SimilarToId));
		}
		return Result;
	}

	for (const FString& CandidateId : CandidateIds)
	{
		const FHCIAbilitySearchDocument* CandidateDocument = Index.DocumentsById.Find(CandidateId);
		if (!CandidateDocument)
		{
			continue;
		}

		float Score = 1.0f;

		if (Result.ParsedQuery.RequiredElement.IsSet() &&
			CandidateDocument->Element == Result.ParsedQuery.RequiredElement.GetValue())
		{
			Score += 3.0f;
		}

		for (const EHCIAbilityUsageScene Scene : Result.ParsedQuery.RequiredScenes)
		{
			if (ContainsScene(*CandidateDocument, Scene))
			{
				Score += 2.0f;
			}
		}

		if (Result.ParsedQuery.bRequireAnyControl &&
			CandidateDocument->ControlProfile != EHCIAbilityControlProfile::None)
		{
			Score += 1.5f;
		}
		if (Result.ParsedQuery.RequiredControlProfile.IsSet() &&
			CandidateDocument->ControlProfile == Result.ParsedQuery.RequiredControlProfile.GetValue())
		{
			Score += 1.0f;
		}

		Score += ScoreByDamagePreference(Result.ParsedQuery.DamagePreference, CandidateDocument->DamageTier);

		TSet<FString> CandidateTokenSet(CandidateDocument->Tokens);
		int32 OverlapCount = 0;
		for (const FString& QueryToken : Result.ParsedQuery.QueryTokens)
		{
			if (CandidateTokenSet.Contains(QueryToken))
			{
				++OverlapCount;
			}
		}
		Score += FMath::Min(static_cast<float>(OverlapCount) * 0.35f, 2.0f);

		if (ReferenceDocument)
		{
			if (CandidateDocument->Element == ReferenceDocument->Element)
			{
				Score += 1.0f;
			}
			if (CandidateDocument->ControlProfile == ReferenceDocument->ControlProfile)
			{
				Score += 1.0f;
			}
			for (const EHCIAbilityUsageScene Scene : ReferenceDocument->UsageScenes)
			{
				if (Scene != EHCIAbilityUsageScene::General && ContainsScene(*CandidateDocument, Scene))
				{
					Score += 0.5f;
				}
			}
		}

		FHCIAbilitySearchCandidate Candidate;
		Candidate.Id = CandidateId;
		Candidate.Score = Score;
		Result.Candidates.Add(Candidate);
	}

	Algo::Sort(Result.Candidates, [](const FHCIAbilitySearchCandidate& Lhs, const FHCIAbilitySearchCandidate& Rhs)
	{
		if (!FMath::IsNearlyEqual(Lhs.Score, Rhs.Score))
		{
			return Lhs.Score > Rhs.Score;
		}
		return Lhs.Id < Rhs.Id;
	});

	if (Result.Candidates.Num() > Result.ParsedQuery.TopK)
	{
		Result.Candidates.SetNum(Result.ParsedQuery.TopK);
	}

	if (Result.Candidates.Num() == 0)
	{
		Result.Suggestions.Add(TEXT("未命中候选：检查索引是否已刷新，并放宽至少一个过滤条件。"));
	}

	return Result;
}
