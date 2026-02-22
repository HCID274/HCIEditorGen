#include "Agent/HCIAbilityKitAgentPlanner.h"

#include "Agent/HCIAbilityKitAgentPlan.h"
#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Dom/JsonObject.h"

namespace
{
static EHCIAbilityKitAgentPlanRiskLevel HCI_ToPlanRiskLevel(EHCIAbilityKitToolCapability Capability)
{
	switch (Capability)
	{
	case EHCIAbilityKitToolCapability::ReadOnly:
		return EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	case EHCIAbilityKitToolCapability::Write:
		return EHCIAbilityKitAgentPlanRiskLevel::Write;
	case EHCIAbilityKitToolCapability::Destructive:
		return EHCIAbilityKitAgentPlanRiskLevel::Destructive;
	default:
		return EHCIAbilityKitAgentPlanRiskLevel::ReadOnly;
	}
}

static bool HCI_StepFromTool(
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const TCHAR* StepId,
	const TCHAR* ToolName,
	const TSharedPtr<FJsonObject>& Args,
	const TArray<FString>& ExpectedEvidence,
	FHCIAbilityKitAgentPlanStep& OutStep,
	FString& OutError)
{
	const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(FName(ToolName));
	if (!Tool)
	{
		OutError = FString::Printf(TEXT("tool not found in whitelist: %s"), ToolName);
		return false;
	}

	OutStep = FHCIAbilityKitAgentPlanStep();
	OutStep.StepId = StepId;
	OutStep.ToolName = Tool->ToolName;
	OutStep.Args = Args.IsValid() ? Args : MakeShared<FJsonObject>();
	OutStep.RiskLevel = HCI_ToPlanRiskLevel(Tool->Capability);
	OutStep.bRequiresConfirm = (Tool->Capability != EHCIAbilityKitToolCapability::ReadOnly) || Tool->bDestructive;
	OutStep.RollbackStrategy = TEXT("all_or_nothing");
	OutStep.ExpectedEvidence = ExpectedEvidence;
	return true;
}

static FString HCI_NormalizeText(const FString& InText)
{
	FString Text = InText;
	Text.TrimStartAndEndInline();
	Text.ToLowerInline();
	return Text;
}

static bool HCI_TextContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Keywords)
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

static TSharedPtr<FJsonObject> HCI_MakeNamingArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Temp/SM_RockTemp_01.SM_RockTemp_01")));
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Temp/T_DiffuseTemp_01.T_DiffuseTemp_01")));
	Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Args->SetStringField(TEXT("metadata_source"), TEXT("auto"));
	Args->SetStringField(TEXT("prefix_mode"), TEXT("auto_by_asset_class"));
	Args->SetStringField(TEXT("target_root"), TEXT("/Game/Art/Organized"));
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeLevelRiskArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->SetStringField(TEXT("scope"), TEXT("selected"));
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("missing_collision")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("default_material")));
	Args->SetArrayField(TEXT("checks"), Checks);
	Args->SetNumberField(TEXT("max_actor_count"), 500);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeTextureComplianceArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/Trees/T_Tree_01_D.T_Tree_01_D")));
	Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Args->SetNumberField(TEXT("max_size"), 1024);
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeLodComplianceArgs()
{
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> AssetPaths;
	AssetPaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Art/Props/SM_Rock_01.SM_Rock_01")));
	Args->SetArrayField(TEXT("asset_paths"), AssetPaths);
	Args->SetStringField(TEXT("lod_group"), TEXT("SmallProp"));
	return Args;
}

static TSharedPtr<FJsonObject> HCI_MakeScanAssetsArgs()
{
	return MakeShared<FJsonObject>();
}
}

bool FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(
	const FString& UserText,
	const FString& RequestId,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentPlan& OutPlan,
	FString& OutRouteReason,
	FString& OutError)
{
	OutPlan = FHCIAbilityKitAgentPlan();
	OutPlan.RequestId = RequestId;
	OutRouteReason.Reset();
	OutError.Reset();

	const FString Text = HCI_NormalizeText(UserText);
	if (Text.IsEmpty())
	{
		OutError = TEXT("empty_input");
		return false;
	}

	const bool bNamingIntent =
		HCI_TextContainsAny(Text, {TEXT("整理"), TEXT("归档"), TEXT("重命名"), TEXT("命名"), TEXT("organize"), TEXT("archive"), TEXT("rename"), TEXT("naming")}) &&
		HCI_TextContainsAny(Text, {TEXT("临时"), TEXT("temp"), TEXT("tmp")});

	const bool bLevelRiskIntent =
		HCI_TextContainsAny(Text, {TEXT("关卡"), TEXT("场景"), TEXT("level")}) &&
		HCI_TextContainsAny(Text, {TEXT("碰撞"), TEXT("collision"), TEXT("材质丢失"), TEXT("默认材质"), TEXT("default material")});

	const bool bAssetComplianceIntent =
		HCI_TextContainsAny(Text, {TEXT("贴图"), TEXT("texture"), TEXT("分辨率"), TEXT("npot"), TEXT("面数"), TEXT("lod")});

	if (bNamingIntent)
	{
		OutPlan.Intent = TEXT("normalize_temp_assets_by_metadata");
		OutRouteReason = TEXT("naming_traceability_temp_assets");

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
				TEXT("NormalizeAssetNamingByMetadata"),
				HCI_MakeNamingArgs(),
				{TEXT("asset_path"), TEXT("before"), TEXT("after")},
				Step,
				OutError))
		{
			return false;
		}
	}
	else if (bLevelRiskIntent)
	{
		OutPlan.Intent = TEXT("scan_level_mesh_risks");
		OutRouteReason = TEXT("level_risk_collision_material");

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
				TEXT("ScanLevelMeshRisks"),
				HCI_MakeLevelRiskArgs(),
				{TEXT("actor_path"), TEXT("issue"), TEXT("evidence")},
				Step,
				OutError))
		{
			return false;
		}
	}
	else if (bAssetComplianceIntent)
	{
		OutPlan.Intent = TEXT("batch_fix_asset_compliance");
		OutRouteReason = TEXT("asset_compliance_texture_lod");

		FHCIAbilityKitAgentPlanStep* TextureStep = nullptr;
		if (HCI_TextContainsAny(Text, {TEXT("贴图"), TEXT("texture"), TEXT("分辨率"), TEXT("npot")}))
		{
			TextureStep = &OutPlan.Steps.AddDefaulted_GetRef();
			if (!HCI_StepFromTool(
					ToolRegistry,
					TEXT("s1"),
					TEXT("SetTextureMaxSize"),
					HCI_MakeTextureComplianceArgs(),
					{TEXT("asset_path"), TEXT("before"), TEXT("after")},
					*TextureStep,
					OutError))
			{
				return false;
			}
		}

		if (HCI_TextContainsAny(Text, {TEXT("面数"), TEXT("lod")}))
		{
			const TCHAR* StepId = TextureStep ? TEXT("s2") : TEXT("s1");
			FHCIAbilityKitAgentPlanStep& LodStep = OutPlan.Steps.AddDefaulted_GetRef();
			if (!HCI_StepFromTool(
					ToolRegistry,
					StepId,
					TEXT("SetMeshLODGroup"),
					HCI_MakeLodComplianceArgs(),
					{TEXT("asset_path"), TEXT("before"), TEXT("after")},
					LodStep,
					OutError))
			{
				return false;
			}
		}

		if (OutPlan.Steps.Num() == 0)
		{
			OutError = TEXT("asset_compliance_route_produced_no_steps");
			return false;
		}
	}
	else
	{
		OutPlan.Intent = TEXT("scan_assets");
		OutRouteReason = TEXT("fallback_scan_assets");

		FHCIAbilityKitAgentPlanStep& Step = OutPlan.Steps.AddDefaulted_GetRef();
		if (!HCI_StepFromTool(
				ToolRegistry,
				TEXT("s1"),
				TEXT("ScanAssets"),
				HCI_MakeScanAssetsArgs(),
				{TEXT("asset_path"), TEXT("result")},
				Step,
				OutError))
		{
			return false;
		}
	}

	FString ContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(OutPlan, ContractError))
	{
		OutError = ContractError;
		return false;
	}

	return true;
}
