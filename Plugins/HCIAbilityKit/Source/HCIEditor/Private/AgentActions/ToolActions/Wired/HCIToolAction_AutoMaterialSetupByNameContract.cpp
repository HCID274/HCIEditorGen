#include "AgentActions/ToolActions/HCIToolActionFactories.h"

#include "AgentActions/Support/HCIAssetNamingRules.h"
#include "AgentActions/Support/HCIAssetPathUtils.h"
#include "AgentActions/Support/HCIToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIToolActionParamParser.h"

#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
class FHCIAutoMaterialSetupByNameContractToolAction final : public IHCIAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("AutoMaterialSetupByNameContract");
	}

	virtual bool DryRun(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

private:
	struct FHCIMatLinkFixtureReport
	{
		FString SnapshotMasterMaterialObjectPath;
		TArray<FString> TextureParameterNames;
	};

	struct FHCIContractGroup
	{
		FString Id;
		FString MeshObjectPath;
		FString TextureBCObjectPath;
		FString TextureNObjectPath;
		FString TextureORMObjectPath;
	};

	static bool HCI_TryLoadMatLinkFixtureReport(FHCIMatLinkFixtureReport& OutReport)
	{
		OutReport = FHCIMatLinkFixtureReport();

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("HCI/TestFixtures/MatLink/latest.json"));

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ReportPath))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		Root->TryGetStringField(TEXT("snapshot_master_material_object_path"), OutReport.SnapshotMasterMaterialObjectPath);

		const TArray<TSharedPtr<FJsonValue>>* Params = nullptr;
		if (Root->TryGetArrayField(TEXT("texture_parameter_names"), Params) && Params != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Val : *Params)
			{
				if (Val.IsValid() && Val->Type == EJson::String)
				{
					FString Name = Val->AsString();
					Name.TrimStartAndEndInline();
					if (!Name.IsEmpty())
					{
						OutReport.TextureParameterNames.Add(MoveTemp(Name));
					}
				}
			}
		}

		return true;
	}

	static FString HCI_ExtractAssetNameFromObjectOrAssetPath(const FString& InPath)
	{
		FString PackagePath;
		FString AssetName;
		if (HCIAssetPathUtils::TrySplitObjectPath(InPath, PackagePath, AssetName))
		{
			return AssetName;
		}
		return FString();
	}

	static bool HCI_TryParseContractMeshId(const FString& AssetName, FString& OutId)
	{
		OutId.Reset();
		if (!AssetName.StartsWith(TEXT("SM_")) || AssetName.Len() <= 3)
		{
			return false;
		}

		OutId = AssetName.Mid(3);
		OutId.TrimStartAndEndInline();
		OutId = HCIAssetNamingRules::SanitizeIdentifier(OutId);
		return !OutId.IsEmpty();
	}

	static bool HCI_TryParseContractTextureIdAndRole(const FString& AssetName, FString& OutId, FString& OutRole)
	{
		OutId.Reset();
		OutRole.Reset();

		if (!AssetName.StartsWith(TEXT("T_")) || AssetName.Len() <= 2)
		{
			return false;
		}

		TArray<FString> Parts;
		AssetName.ParseIntoArray(Parts, TEXT("_"), true);
		if (Parts.Num() < 3)
		{
			return false;
		}

		// T_<ID>_<ROLE>
		const FString Role = Parts.Last().ToUpper();
		if (!(Role == TEXT("BC") || Role == TEXT("N") || Role == TEXT("ORM") || Role == TEXT("RMA") || Role == TEXT("ARM")))
		{
			return false;
		}

		FString Id;
		for (int32 Index = 1; Index < Parts.Num() - 1; ++Index)
		{
			if (!Id.IsEmpty())
			{
				Id += TEXT("_");
			}
			Id += Parts[Index];
		}

		Id = HCIAssetNamingRules::SanitizeIdentifier(Id);
		if (Id.IsEmpty())
		{
			return false;
		}

		OutId = MoveTemp(Id);
		OutRole = (Role == TEXT("RMA") || Role == TEXT("ARM")) ? TEXT("ORM") : Role;
		return true;
	}

	static FString HCI_BuildStrictContractOrphanReason(const FString& AssetName)
	{
		if (AssetName.StartsWith(TEXT("T_")))
		{
			TArray<FString> Parts;
			AssetName.ParseIntoArray(Parts, TEXT("_"), true);
			const FString Tail = Parts.Num() > 0 ? Parts.Last().ToUpper() : FString();
			if (Tail == TEXT("COLOR") || Tail == TEXT("BASECOLOR") || Tail == TEXT("ALBEDO") || Tail == TEXT("DIFFUSE"))
			{
				return TEXT("贴图后缀不满足契约（末尾为 Color，期望: _BC）");
			}
			if (Tail == TEXT("NORMAL") || Tail == TEXT("NORMALMAP"))
			{
				return TEXT("贴图后缀不满足契约（末尾为 Normal，期望: _N）");
			}
			if (Tail.Contains(TEXT("ORM")) || Tail.Contains(TEXT("RMA")) || Tail.Contains(TEXT("ARM")))
			{
				return TEXT("贴图后缀不满足契约（期望: _ORM 或 _RMA）");
			}
			return TEXT("贴图命名不满足严格契约（期望: T_<ID>_BC / T_<ID>_N / T_<ID>_ORM）");
		}

		if (AssetName.StartsWith(TEXT("SM_")))
		{
			return TEXT("Mesh 命名不满足严格契约（期望: SM_<ID>）");
		}

		return TEXT("命名不满足严格契约（期望: SM_<ID> 或 T_<ID>_BC/N/ORM）");
	}

	static FString HCI_BuildReasonEvidenceString(const TMap<FString, FString>& ReasonByPath)
	{
		if (ReasonByPath.Num() <= 0)
		{
			return TEXT("none");
		}

		TArray<FString> Keys;
		ReasonByPath.GetKeys(Keys);
		Keys.Sort([](const FString& A, const FString& B) { return A < B; });

		TArray<FString> Rows;
		Rows.Reserve(Keys.Num());
		for (const FString& Key : Keys)
		{
			const FString* Reason = ReasonByPath.Find(Key);
			if (Reason == nullptr || Reason->IsEmpty())
			{
				continue;
			}
			Rows.Add(FString::Printf(TEXT("%s => %s"), *Key, **Reason));
		}

		return Rows.Num() > 0 ? FString::Join(Rows, TEXT(" | ")) : TEXT("none");
	}

	static void HCI_SelectTextureParameterNamesForRoles(
		const TArray<FString>& InCandidates,
		TArray<FName>& OutBaseColorParams,
		TArray<FName>& OutNormalParams,
		TArray<FName>& OutOrmParams)
	{
		OutBaseColorParams.Reset();
		OutNormalParams.Reset();
		OutOrmParams.Reset();

		auto ScoreParam = [](const FString& NameLower) -> int32
		{
			if (NameLower.Contains(TEXT("basecolor")))
			{
				return 100;
			}
			if (NameLower.Contains(TEXT("albedo")) || NameLower.Contains(TEXT("diffuse")) || NameLower.Contains(TEXT("color")))
			{
				return 80;
			}
			return 0;
		};

		FString BestBase;
		int32 BestBaseScore = -1;
		for (const FString& Candidate : InCandidates)
		{
			const FString Lower = Candidate.ToLower();
			const int32 Score = ScoreParam(Lower);
			if (Score > BestBaseScore)
			{
				BestBaseScore = Score;
				BestBase = Candidate;
			}
			if (Lower.Contains(TEXT("normal")))
			{
				OutNormalParams.Add(FName(*Candidate));
			}
			if (Lower.Contains(TEXT("metallicroughness")) || Lower.Contains(TEXT("metallic")) || Lower.Contains(TEXT("roughness")) || Lower.Contains(TEXT("occlusion")) || Lower.Contains(TEXT("orm")))
			{
				OutOrmParams.Add(FName(*Candidate));
			}
		}

		if (!BestBase.IsEmpty())
		{
			OutBaseColorParams.Add(FName(*BestBase));
		}

		// De-dupe.
		auto SortUnique = [](TArray<FName>& InOut)
		{
			InOut.Sort(FNameLexicalLess());
			for (int32 Index = InOut.Num() - 1; Index > 0; --Index)
			{
				if (InOut[Index].IsEqual(InOut[Index - 1], ENameCase::CaseSensitive))
				{
					InOut.RemoveAt(Index, 1, false);
				}
			}
		};

		SortUnique(OutNormalParams);
		SortUnique(OutOrmParams);
	}

	static bool HCI_TryResolveDefaultMasterMaterialPath(FString& InOutMasterMaterialPath, TArray<FString>& OutParamNameCandidates)
	{
		OutParamNameCandidates.Reset();

		FHCIMatLinkFixtureReport Report;
		if (HCI_TryLoadMatLinkFixtureReport(Report))
		{
			if (!Report.SnapshotMasterMaterialObjectPath.IsEmpty())
			{
				InOutMasterMaterialPath = Report.SnapshotMasterMaterialObjectPath;
			}
			OutParamNameCandidates = MoveTemp(Report.TextureParameterNames);
		}

		if (InOutMasterMaterialPath.IsEmpty())
		{
			// Fallback for demo projects that haven't run MatLinkBuildSnapshot yet.
			InOutMasterMaterialPath = TEXT("/Game/__HCI_Test/Masters/M_MatLink_Master.M_MatLink_Master");
		}

		return InOutMasterMaterialPath.StartsWith(TEXT("/Game/"));
	}

	static bool RunInternal(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		FString TargetRoot;
		const FHCIToolActionParamParser Params(Request.Args);
		if (!Params.TryGetRequiredStringArray(TEXT("asset_paths"), AssetPaths) ||
			!Params.TryGetRequiredString(TEXT("target_root"), TargetRoot))
		{
			return FHCIToolActionEvidenceBuilder::FailRequiredArgMissing(OutResult);
		}

		TargetRoot = HCIAssetPathUtils::TrimTrailingSlash(TargetRoot);
		if (!TargetRoot.StartsWith(TEXT("/Game/")))
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("target_root_must_start_with_game");
			return false;
		}

		FString MasterMaterialPath;
		if (Request.Args.IsValid())
		{
			Request.Args->TryGetStringField(TEXT("master_material_path"), MasterMaterialPath);
			MasterMaterialPath.TrimStartAndEndInline();
		}

		TArray<FString> ParamNameCandidates;
		if (!HCI_TryResolveDefaultMasterMaterialPath(MasterMaterialPath, ParamNameCandidates))
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("master_material_path_invalid");
			return false;
		}

		UObject* MasterObj = UEditorAssetLibrary::LoadAsset(MasterMaterialPath);
		UMaterialInterface* MasterMaterial = Cast<UMaterialInterface>(MasterObj);
		if (!MasterMaterial)
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4401");
			OutResult.Reason = TEXT("master_material_not_found_or_invalid");
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return false;
		}

		// If report doesn't provide param names (or is missing), fall back to scanning the master.
		if (ParamNameCandidates.Num() <= 0)
		{
			TArray<FMaterialParameterInfo> TextureParams;
			TArray<FGuid> TextureParamGuids;
			MasterMaterial->GetAllTextureParameterInfo(TextureParams, TextureParamGuids);
			for (const FMaterialParameterInfo& Info : TextureParams)
			{
				ParamNameCandidates.Add(Info.Name.ToString());
			}
		}

		TArray<FName> BaseColorParams;
		TArray<FName> NormalParams;
		TArray<FName> OrmParams;
		HCI_SelectTextureParameterNamesForRoles(ParamNameCandidates, BaseColorParams, NormalParams, OrmParams);

		if (BaseColorParams.Num() <= 0 || NormalParams.Num() <= 0)
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4402");
			OutResult.Reason = TEXT("master_material_texture_params_unrecognized");
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return false;
		}

		// Build strict contract groups.
		TMap<FString, FHCIContractGroup> GroupsById;
		TArray<FString> MissingGroups;
		TArray<FString> OrphanAssets;
		TSet<FString> UnresolvedAssetSet;
		TMap<FString, FString> OrphanReasonByPath;
		TMap<FString, FString> UnresolvedReasonByPath;

		for (const FString& Path : AssetPaths)
		{
			const FString AssetName = HCI_ExtractAssetNameFromObjectOrAssetPath(Path);
			if (AssetName.IsEmpty())
			{
				continue;
			}

			FString Id;
			if (HCI_TryParseContractMeshId(AssetName, Id))
			{
				FHCIContractGroup& Group = GroupsById.FindOrAdd(Id);
				Group.Id = Id;
				if (!Group.MeshObjectPath.IsEmpty() && !Group.MeshObjectPath.Equals(Path, ESearchCase::CaseSensitive))
				{
					MissingGroups.Add(FString::Printf(TEXT("%s (duplicate_mesh)"), *Id));
					continue;
				}
				Group.MeshObjectPath = Path;
				continue;
			}

			FString Role;
			if (HCI_TryParseContractTextureIdAndRole(AssetName, Id, Role))
			{
				FHCIContractGroup& Group = GroupsById.FindOrAdd(Id);
				Group.Id = Id;
				if (Role == TEXT("BC"))
				{
					Group.TextureBCObjectPath = Path;
				}
				else if (Role == TEXT("N"))
				{
					Group.TextureNObjectPath = Path;
				}
				else if (Role == TEXT("ORM"))
				{
					Group.TextureORMObjectPath = Path;
				}
				continue;
			}

			// Not matching strict contract (common in real projects): treat as orphan so artists can locate & fix.
			if (!Path.IsEmpty())
			{
				OrphanAssets.Add(Path);
				if (!OrphanReasonByPath.Contains(Path))
				{
					OrphanReasonByPath.Add(Path, HCI_BuildStrictContractOrphanReason(AssetName));
				}
			}
		}

		// Textures that look contract-ish but have no mesh partner are also orphans in practice.
		for (const TPair<FString, FHCIContractGroup>& Pair : GroupsById)
		{
			const FHCIContractGroup& Group = Pair.Value;
			if (!Group.MeshObjectPath.IsEmpty())
			{
				continue;
			}
			if (!Group.TextureBCObjectPath.IsEmpty())
			{
				OrphanAssets.AddUnique(Group.TextureBCObjectPath);
				OrphanReasonByPath.FindOrAdd(Group.TextureBCObjectPath) = FString::Printf(TEXT("贴图疑似契约（ID=%s），但缺少配对 Mesh：SM_%s"), *Group.Id, *Group.Id);
			}
			if (!Group.TextureNObjectPath.IsEmpty())
			{
				OrphanAssets.AddUnique(Group.TextureNObjectPath);
				OrphanReasonByPath.FindOrAdd(Group.TextureNObjectPath) = FString::Printf(TEXT("贴图疑似契约（ID=%s），但缺少配对 Mesh：SM_%s"), *Group.Id, *Group.Id);
			}
			if (!Group.TextureORMObjectPath.IsEmpty())
			{
				OrphanAssets.AddUnique(Group.TextureORMObjectPath);
				OrphanReasonByPath.FindOrAdd(Group.TextureORMObjectPath) = FString::Printf(TEXT("贴图疑似契约（ID=%s），但缺少配对 Mesh：SM_%s"), *Group.Id, *Group.Id);
			}
		}

		TArray<FHCIContractGroup> ReadyGroups;
		ReadyGroups.Reserve(GroupsById.Num());
		for (const TPair<FString, FHCIContractGroup>& Pair : GroupsById)
		{
			const FHCIContractGroup& Group = Pair.Value;
			if (Group.MeshObjectPath.IsEmpty())
			{
				continue;
			}
			if (Group.TextureBCObjectPath.IsEmpty() || Group.TextureNObjectPath.IsEmpty() || Group.TextureORMObjectPath.IsEmpty())
			{
				TArray<FString> MissingRoles;
				if (Group.TextureBCObjectPath.IsEmpty()) { MissingRoles.Add(TEXT("BC")); }
				if (Group.TextureNObjectPath.IsEmpty()) { MissingRoles.Add(TEXT("N")); }
				if (Group.TextureORMObjectPath.IsEmpty()) { MissingRoles.Add(TEXT("ORM")); }
				const FString MissingLabel = MissingRoles.Num() > 0 ? FString::Join(MissingRoles, TEXT("/")) : TEXT("BC/N/ORM");

				// Partial match: we found a mesh (and maybe some textures), but cannot complete a full PBR set.
				UnresolvedAssetSet.Add(Group.MeshObjectPath);
				UnresolvedReasonByPath.FindOrAdd(Group.MeshObjectPath) = FString::Printf(TEXT("Mesh ID=%s 缺少贴图：T_%s_%s"), *Group.Id, *Group.Id, *MissingLabel);
				if (!Group.TextureBCObjectPath.IsEmpty())
				{
					UnresolvedAssetSet.Add(Group.TextureBCObjectPath);
					UnresolvedReasonByPath.FindOrAdd(Group.TextureBCObjectPath) = FString::Printf(TEXT("该组 ID=%s 缺少贴图：%s"), *Group.Id, *MissingLabel);
				}
				if (!Group.TextureNObjectPath.IsEmpty())
				{
					UnresolvedAssetSet.Add(Group.TextureNObjectPath);
					UnresolvedReasonByPath.FindOrAdd(Group.TextureNObjectPath) = FString::Printf(TEXT("该组 ID=%s 缺少贴图：%s"), *Group.Id, *MissingLabel);
				}
				if (!Group.TextureORMObjectPath.IsEmpty())
				{
					UnresolvedAssetSet.Add(Group.TextureORMObjectPath);
					UnresolvedReasonByPath.FindOrAdd(Group.TextureORMObjectPath) = FString::Printf(TEXT("该组 ID=%s 缺少贴图：%s"), *Group.Id, *MissingLabel);
				}

				MissingGroups.Add(FString::Printf(TEXT("%s (missing_textures)"), *Group.Id));
				continue;
			}
			ReadyGroups.Add(Group);
		}

		TArray<FString> UnresolvedAssets;
		UnresolvedAssets.Reserve(UnresolvedAssetSet.Num());
		for (const FString& P : UnresolvedAssetSet)
		{
			UnresolvedAssets.Add(P);
		}
		UnresolvedAssets.Sort([](const FString& A, const FString& B) { return A < B; });
		OrphanAssets.Sort([](const FString& A, const FString& B) { return A < B; });
		for (int32 Index = OrphanAssets.Num() - 1; Index > 0; --Index)
		{
			if (OrphanAssets[Index].Equals(OrphanAssets[Index - 1], ESearchCase::CaseSensitive))
			{
				OrphanAssets.RemoveAt(Index, 1, false);
			}
		}
		for (int32 Index = UnresolvedAssets.Num() - 1; Index > 0; --Index)
		{
			if (UnresolvedAssets[Index].Equals(UnresolvedAssets[Index - 1], ESearchCase::CaseSensitive))
			{
				UnresolvedAssets.RemoveAt(Index, 1, false);
			}
		}

		if (ReadyGroups.Num() <= 0)
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4403");
			OutResult.Reason = TEXT("material_setup_no_contract_groups_found");
			OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("group_count"), TEXT("0"));
			if (MissingGroups.Num() > 0)
			{
				OutResult.Evidence.Add(TEXT("missing_groups"), FString::Join(MissingGroups, TEXT(" | ")));
			}
			OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
			OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return false;
		}

		// Proposals.
		const FString MaterialsDir = FString::Printf(TEXT("%s/Materials"), *TargetRoot);
		TArray<FString> ProposedInstances;
		TArray<FString> ProposedAssignments;
		TArray<FString> ProposedBindings;

		for (const FHCIContractGroup& Group : ReadyGroups)
		{
			const FString MiName = FString::Printf(TEXT("MI_%s"), *Group.Id);
			const FString MiPackagePath = FString::Printf(TEXT("%s/%s"), *MaterialsDir, *MiName);
			const FString MiObjectPath = HCIAssetPathUtils::ToObjectPath(MiPackagePath, MiName);
			ProposedInstances.Add(FString::Printf(TEXT("%s -> %s"), *MiName, *MiObjectPath));

			const FString MeshName = HCI_ExtractAssetNameFromObjectOrAssetPath(Group.MeshObjectPath);
			ProposedAssignments.Add(FString::Printf(TEXT("%s (Slot0) -> %s"), *MeshName, *MiName));

			for (const FName& Param : BaseColorParams)
			{
				ProposedBindings.Add(FString::Printf(TEXT("%s.%s <- %s"), *MiName, *Param.ToString(), *HCI_ExtractAssetNameFromObjectOrAssetPath(Group.TextureBCObjectPath)));
			}
			for (const FName& Param : NormalParams)
			{
				ProposedBindings.Add(FString::Printf(TEXT("%s.%s <- %s"), *MiName, *Param.ToString(), *HCI_ExtractAssetNameFromObjectOrAssetPath(Group.TextureNObjectPath)));
			}
			for (const FName& Param : OrmParams)
			{
				ProposedBindings.Add(FString::Printf(TEXT("%s.%s <- %s"), *MiName, *Param.ToString(), *HCI_ExtractAssetNameFromObjectOrAssetPath(Group.TextureORMObjectPath)));
			}
		}

		if (bIsDryRun)
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = true;
			OutResult.Reason = TEXT("auto_material_setup_dry_run_ok");
			OutResult.EstimatedAffectedCount = ReadyGroups.Num();
			OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
			OutResult.Evidence.Add(TEXT("proposed_material_instances"), ProposedInstances.Num() > 0 ? FString::Join(ProposedInstances, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("proposed_mesh_assignments"), ProposedAssignments.Num() > 0 ? FString::Join(ProposedAssignments, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("proposed_parameter_bindings"), ProposedBindings.Num() > 0 ? FString::Join(ProposedBindings, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("missing_groups"), MissingGroups.Num() > 0 ? FString::Join(MissingGroups, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
			OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return true;
		}

		// Execute.
		struct FHCIMaterialSetupProgressNotification
		{
			TSharedPtr<SNotificationItem> Item;

			void Start(const FString& Text)
			{
				FNotificationInfo Info(FText::FromString(Text));
				Info.bFireAndForget = false;
				Info.FadeOutDuration = 0.2f;
				Info.ExpireDuration = 0.0f;
				Info.bUseThrobber = true;
				Info.bUseSuccessFailIcons = true;
				Item = FSlateNotificationManager::Get().AddNotification(Info);
				if (Item.IsValid())
				{
					Item->SetCompletionState(SNotificationItem::CS_Pending);
				}
			}

			void Update(const FString& Text)
			{
				if (Item.IsValid())
				{
					Item->SetText(FText::FromString(Text));
				}
			}

			void Finish(const bool bSucceeded, const FString& Text)
			{
				if (Item.IsValid())
				{
					Item->SetText(FText::FromString(Text));
					Item->SetCompletionState(bSucceeded ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
					Item->ExpireAndFadeout();
				}
			}
		};

		FHCIMaterialSetupProgressNotification Notify;
		Notify.Start(FString::Printf(TEXT("Auto-Material：准备中... (groups=%d)"), ReadyGroups.Num()));

		// Preflight: ensure destination doesn't already exist, and required assets load as expected.
		for (const FHCIContractGroup& Group : ReadyGroups)
		{
			const FString MiName = FString::Printf(TEXT("MI_%s"), *Group.Id);
			const FString MiAssetPath = FString::Printf(TEXT("%s/Materials/%s"), *TargetRoot, *MiName);
			if (UEditorAssetLibrary::DoesAssetExist(MiAssetPath))
			{
				Notify.Finish(false, TEXT("完成：Auto-Material（失败）"));
				OutResult = FHCIAgentToolActionResult();
				OutResult.bSucceeded = false;
				OutResult.ErrorCode = TEXT("E4404");
				OutResult.Reason = TEXT("material_instance_already_exists");
				OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
				OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
				OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
				OutResult.Evidence.Add(TEXT("missing_groups"), FString::Join(MissingGroups, TEXT(" | ")));
				OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
				OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
				OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
				return false;
			}

			if (!Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Group.MeshObjectPath)) ||
				!Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(Group.TextureBCObjectPath)) ||
				!Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(Group.TextureNObjectPath)) ||
				!Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(Group.TextureORMObjectPath)))
			{
				Notify.Finish(false, TEXT("完成：Auto-Material（失败）"));
				OutResult = FHCIAgentToolActionResult();
				OutResult.bSucceeded = false;
				OutResult.ErrorCode = TEXT("E4405");
				OutResult.Reason = TEXT("required_assets_load_failed");
				OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
				OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
				OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
				OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
				OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
				OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
				return false;
			}
		}

		UEditorAssetLibrary::MakeDirectory(MaterialsDir);

		TArray<FString> CreatedInstances;
		TArray<FString> AppliedAssignments;
		TArray<FString> FailedRows;

		int32 Completed = 0;
		for (const FHCIContractGroup& Group : ReadyGroups)
		{
			++Completed;
			Notify.Update(FString::Printf(TEXT("Auto-Material：执行中... (%d/%d)"), Completed, ReadyGroups.Num()));
			// Keep editor UI responsive during batch operations.
			FSlateApplication::Get().PumpMessages();

			const FString MiName = FString::Printf(TEXT("MI_%s"), *Group.Id);
			const FString MiDir = MaterialsDir;

			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent = MasterMaterial;

			UObject* Created = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateAsset(
				MiName,
				MiDir,
				UMaterialInstanceConstant::StaticClass(),
				Factory);

			UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Created);
			if (!MI)
			{
				FailedRows.Add(FString::Printf(TEXT("%s (create_mi_failed)"), *MiName));
				break;
			}

			UTexture* TexBC = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Group.TextureBCObjectPath));
			UTexture* TexN = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Group.TextureNObjectPath));
			UTexture* TexORM = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Group.TextureORMObjectPath));

			MI->Modify();
			for (const FName& Param : BaseColorParams)
			{
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), TexBC);
			}
			for (const FName& Param : NormalParams)
			{
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), TexN);
			}
			for (const FName& Param : OrmParams)
			{
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), TexORM);
			}
			MI->PostEditChange();
			UEditorAssetLibrary::SaveLoadedAsset(MI, false);

			CreatedInstances.Add(MI->GetPathName());

			UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Group.MeshObjectPath));
			if (!Mesh)
			{
				FailedRows.Add(FString::Printf(TEXT("%s (mesh_load_failed)"), *Group.MeshObjectPath));
				break;
			}

			Mesh->Modify();
			Mesh->SetMaterial(0, MI);
			Mesh->PostEditChange();
			UEditorAssetLibrary::SaveLoadedAsset(Mesh, false);
			AppliedAssignments.Add(FString::Printf(TEXT("%s (Slot0) -> %s"), *Mesh->GetPathName(), *MI->GetPathName()));
		}

		OutResult = FHCIAgentToolActionResult();
		const bool bAllOk = FailedRows.Num() == 0;
		OutResult.bSucceeded = bAllOk;
		OutResult.ErrorCode = bAllOk ? FString() : TEXT("E4406");
		OutResult.Reason = bAllOk ? TEXT("auto_material_setup_execute_ok") : TEXT("auto_material_setup_execute_failed");
		OutResult.EstimatedAffectedCount = CreatedInstances.Num();
		OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
		OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
		OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
		OutResult.Evidence.Add(TEXT("proposed_material_instances"), ProposedInstances.Num() > 0 ? FString::Join(ProposedInstances, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("proposed_mesh_assignments"), ProposedAssignments.Num() > 0 ? FString::Join(ProposedAssignments, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("proposed_parameter_bindings"), ProposedBindings.Num() > 0 ? FString::Join(ProposedBindings, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("missing_groups"), MissingGroups.Num() > 0 ? FString::Join(MissingGroups, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
		OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
		if (FailedRows.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedRows, TEXT(" | ")));
		}

		Notify.Finish(bAllOk, bAllOk ? TEXT("完成：Auto-Material") : TEXT("完成：Auto-Material（失败）"));
		return bAllOk;
	}
};
}

TSharedPtr<IHCIAgentToolAction> HCIToolActionFactories::MakeAutoMaterialSetupByNameContractToolAction()
{
	return MakeShared<FHCIAutoMaterialSetupByNameContractToolAction>();
}

