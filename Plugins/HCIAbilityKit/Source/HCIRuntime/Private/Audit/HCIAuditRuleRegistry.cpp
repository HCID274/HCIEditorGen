#include "Audit/HCIAuditRuleRegistry.h"

#include "Audit/HCIAuditScanService.h"

namespace
{
constexpr int32 GHighPolyAutoLodTriangleThreshold = 10000;

class FHCITextureNPOTRule final : public IHCIAuditRule
{
public:
	virtual FName GetRuleId() const override
	{
		static const FName RuleId(TEXT("TextureNPOTRule"));
		return RuleId;
	}

	virtual void Evaluate(const FHCIAuditContext& Context, TArray<FHCIAuditIssue>& OutIssues) const override
	{
		const FHCIAuditAssetRow& Row = Context.AssetRow;
		if (Row.TextureWidth <= 0 || Row.TextureHeight <= 0)
		{
			return;
		}

		if (FMath::IsPowerOfTwo(Row.TextureWidth) && FMath::IsPowerOfTwo(Row.TextureHeight))
		{
			return;
		}

		FHCIAuditIssue& Issue = OutIssues.Emplace_GetRef();
		Issue.RuleId = GetRuleId();
		Issue.Severity = EHCIAuditSeverity::Error;
		Issue.Reason = TEXT("texture dimensions are not power-of-two");
		Issue.Hint = TEXT("Resize texture to power-of-two dimensions (for example 1024x1024) or set a justified exception in pipeline rules.");
		Issue.AddEvidence(TEXT("asset_path"), Row.AssetPath);
		Issue.AddEvidence(TEXT("texture_width"), FString::FromInt(Row.TextureWidth));
		Issue.AddEvidence(TEXT("texture_height"), FString::FromInt(Row.TextureHeight));
		if (!Row.TextureDimensionsTagKey.IsEmpty())
		{
			Issue.AddEvidence(TEXT("source_tag_key"), Row.TextureDimensionsTagKey);
		}
	}
};

class FHCIHighPolyAutoLODRule final : public IHCIAuditRule
{
public:
	virtual FName GetRuleId() const override
	{
		static const FName RuleId(TEXT("HighPolyAutoLODRule"));
		return RuleId;
	}

	virtual void Evaluate(const FHCIAuditContext& Context, TArray<FHCIAuditIssue>& OutIssues) const override
	{
		const FHCIAuditAssetRow& Row = Context.AssetRow;
		if (Row.TriangleCountLod0Actual < 0 || Row.TriangleCountLod0Actual <= GHighPolyAutoLodTriangleThreshold)
		{
			return;
		}

		if (Row.RepresentingMeshPath.IsEmpty())
		{
			return;
		}

		if (Row.bMeshNaniteEnabledKnown && Row.bMeshNaniteEnabled)
		{
			return;
		}

		if (Row.MeshLodCount < 0)
		{
			return;
		}

		if (Row.MeshLodCount > 1)
		{
			return;
		}

		FHCIAuditIssue& Issue = OutIssues.Emplace_GetRef();
		Issue.RuleId = GetRuleId();
		Issue.Severity = EHCIAuditSeverity::Warn;
		Issue.Reason = TEXT("high triangle mesh is missing additional LODs");
		Issue.Hint = TEXT("Create LODs or plan a safe fix via SetMeshLODGroup(LevelArchitecture) in Stage E Dry-Run flow.");
		Issue.AddEvidence(TEXT("asset_path"), Row.AssetPath);
		Issue.AddEvidence(TEXT("representing_mesh_path"), Row.RepresentingMeshPath);
		Issue.AddEvidence(TEXT("triangle_count_lod0_actual"), FString::FromInt(Row.TriangleCountLod0Actual));
		Issue.AddEvidence(TEXT("triangle_threshold"), FString::FromInt(GHighPolyAutoLodTriangleThreshold));
		Issue.AddEvidence(TEXT("mesh_lod_count"), FString::FromInt(Row.MeshLodCount));
		Issue.AddEvidence(TEXT("suggested_lod_group"), TEXT("LevelArchitecture"));
		if (Row.bMeshNaniteEnabledKnown)
		{
			Issue.AddEvidence(TEXT("mesh_nanite_enabled"), Row.bMeshNaniteEnabled ? TEXT("true") : TEXT("false"));
		}
		if (!Row.MeshLodCountTagKey.IsEmpty())
		{
			Issue.AddEvidence(TEXT("mesh_lod_tag_key"), Row.MeshLodCountTagKey);
		}
		if (!Row.MeshNaniteTagKey.IsEmpty())
		{
			Issue.AddEvidence(TEXT("mesh_nanite_tag_key"), Row.MeshNaniteTagKey);
		}
	}
};

class FHCITriangleExpectedMismatchRule final : public IHCIAuditRule
{
public:
	virtual FName GetRuleId() const override
	{
		static const FName RuleId(TEXT("TriangleExpectedMismatchRule"));
		return RuleId;
	}

	virtual void Evaluate(const FHCIAuditContext& Context, TArray<FHCIAuditIssue>& OutIssues) const override
	{
		const FHCIAuditAssetRow& Row = Context.AssetRow;
		if (Row.TriangleCountLod0Actual < 0 || Row.TriangleCountLod0ExpectedJson < 0)
		{
			return;
		}

		const int32 Delta = Row.TriangleCountLod0Actual - Row.TriangleCountLod0ExpectedJson;
		if (Delta == 0)
		{
			return;
		}

		FHCIAuditIssue& Issue = OutIssues.Emplace_GetRef();
		Issue.RuleId = GetRuleId();
		Issue.Severity = EHCIAuditSeverity::Warn;
		Issue.Reason = TEXT("triangle_count_lod0 expected value mismatches actual mesh triangles");
		Issue.Hint = TEXT("Use mesh actual value as source of truth; update params.triangle_count_lod0 if this change is intentional.");
		Issue.AddEvidence(TEXT("asset_path"), Row.AssetPath);
		Issue.AddEvidence(TEXT("triangle_count_lod0_actual"), FString::FromInt(Row.TriangleCountLod0Actual));
		Issue.AddEvidence(TEXT("triangle_count_lod0_expected_json"), FString::FromInt(Row.TriangleCountLod0ExpectedJson));
		Issue.AddEvidence(TEXT("triangle_mismatch_delta"), FString::FromInt(Delta));
		Issue.AddEvidence(TEXT("triangle_source"), Row.TriangleSource);
		if (!Row.TriangleSourceTagKey.IsEmpty())
		{
			Issue.AddEvidence(TEXT("source_tag_key"), Row.TriangleSourceTagKey);
		}
	}
};
}

FHCIAuditRuleRegistry& FHCIAuditRuleRegistry::Get()
{
	static FHCIAuditRuleRegistry Instance;
	return Instance;
}

void FHCIAuditRuleRegistry::ResetToDefaults()
{
	Rules.Reset();
	RuleIndexById.Reset();
	bDefaultsInitialized = true;
	RegisterRule(MakeUnique<FHCITextureNPOTRule>());
	RegisterRule(MakeUnique<FHCIHighPolyAutoLODRule>());
	RegisterRule(MakeUnique<FHCITriangleExpectedMismatchRule>());
}

bool FHCIAuditRuleRegistry::RegisterRule(TUniquePtr<IHCIAuditRule>&& InRule)
{
	if (!InRule.IsValid())
	{
		return false;
	}

	const FName RuleId = InRule->GetRuleId();
	if (RuleId.IsNone() || RuleIndexById.Contains(RuleId))
	{
		return false;
	}

	FRuleEntry& Entry = Rules.Emplace_GetRef();
	Entry.Rule = MoveTemp(InRule);
	Entry.bEnabled = true;
	RuleIndexById.Add(RuleId, Rules.Num() - 1);
	return true;
}

bool FHCIAuditRuleRegistry::SetRuleEnabled(const FName RuleId, const bool bEnabled)
{
	EnsureDefaultRules();

	const int32* FoundIndex = RuleIndexById.Find(RuleId);
	if (!FoundIndex || !Rules.IsValidIndex(*FoundIndex))
	{
		return false;
	}

	Rules[*FoundIndex].bEnabled = bEnabled;
	return true;
}

bool FHCIAuditRuleRegistry::IsRuleEnabled(const FName RuleId) const
{
	EnsureDefaultRules();

	const int32* FoundIndex = RuleIndexById.Find(RuleId);
	if (!FoundIndex || !Rules.IsValidIndex(*FoundIndex))
	{
		return false;
	}

	return Rules[*FoundIndex].bEnabled;
}

void FHCIAuditRuleRegistry::Evaluate(
	const FHCIAuditContext& Context,
	TArray<FHCIAuditIssue>& OutIssues) const
{
	EnsureDefaultRules();

	for (const FRuleEntry& Entry : Rules)
	{
		if (!Entry.bEnabled || !Entry.Rule.IsValid())
		{
			continue;
		}

		Entry.Rule->Evaluate(Context, OutIssues);
	}
}

TArray<FName> FHCIAuditRuleRegistry::GetRegisteredRuleIds() const
{
	EnsureDefaultRules();

	TArray<FName> RuleIds;
	RuleIds.Reserve(Rules.Num());
	for (const FRuleEntry& Entry : Rules)
	{
		if (!Entry.Rule.IsValid())
		{
			continue;
		}

		RuleIds.Add(Entry.Rule->GetRuleId());
	}
	return RuleIds;
}

void FHCIAuditRuleRegistry::EnsureDefaultRules() const
{
	if (bDefaultsInitialized)
	{
		return;
	}

	const_cast<FHCIAuditRuleRegistry*>(this)->ResetToDefaults();
}

