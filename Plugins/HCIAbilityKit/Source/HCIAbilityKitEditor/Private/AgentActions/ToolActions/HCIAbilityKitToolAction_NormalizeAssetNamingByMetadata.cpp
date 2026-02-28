#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

#include "AgentActions/Support/HCIAbilityKitAssetNamingRules.h"
#include "AgentActions/Support/HCIAbilityKitAssetPathUtils.h"
#include "AgentActions/Support/HCIAbilityKitToolActionAssetPathNormalizer.h"
#include "AgentActions/Support/HCIAbilityKitToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIAbilityKitToolActionParamParser.h"
#include "AgentActions/ToolActions/HCIAbilityKitToolAction_NormalizeAssetNamingByMetadata_LegacyShared.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
class FHCIAbilityKitNormalizeAssetNamingByMetadataToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("NormalizeAssetNamingByMetadata");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

private:
	struct FHCINamingProposal
	{
		FString SourceAssetPath;
		FString SourceObjectPath;
		FString SourceAssetName;
		FString SourceDirectory;
		FString DestinationAssetPath;
		FString DestinationObjectPath;
		FString DestinationAssetName;
		FString DestinationDirectory;
		bool bNeedsRename = false;
		bool bNeedsMove = false;
	};

	static bool HCI_TryFindAvailableDestination(
		const FString& SourceObjectPath,
		const FString& DestinationDir,
		const FString& DesiredAssetName,
		const TSet<FString>& ReservedDestinationObjects,
		FString& OutDestinationAssetPath,
		FString& OutDestinationObjectPath,
		FString& OutResolvedAssetName)
	{
		OutDestinationAssetPath.Reset();
		OutDestinationObjectPath.Reset();
		OutResolvedAssetName.Reset();

		const FString TrimmedDir = HCIAbilityKitAssetPathUtils::TrimTrailingSlash(DestinationDir);
		if (TrimmedDir.IsEmpty() || !TrimmedDir.StartsWith(TEXT("/Game/")))
		{
			return false;
		}

		auto BuildPaths = [&TrimmedDir](const FString& AssetName, FString& OutAssetPath, FString& OutObjectPath)
		{
			OutAssetPath = FString::Printf(TEXT("%s/%s"), *TrimmedDir, *AssetName);
			OutObjectPath = HCIAbilityKitAssetPathUtils::ToObjectPath(OutAssetPath, AssetName);
		};

		// 0 = desired, then deconflict suffixes.
		for (int32 Attempt = 0; Attempt < 50; ++Attempt)
		{
			const FString CandidateName = (Attempt == 0)
				? DesiredAssetName
				: FString::Printf(TEXT("%s_%02d"), *DesiredAssetName, Attempt);

			FString CandidateAssetPath;
			FString CandidateObjectPath;
			BuildPaths(CandidateName, CandidateAssetPath, CandidateObjectPath);

			if (CandidateObjectPath.Equals(SourceObjectPath, ESearchCase::CaseSensitive))
			{
				// Already at desired location.
				OutDestinationAssetPath = CandidateAssetPath;
				OutDestinationObjectPath = CandidateObjectPath;
				OutResolvedAssetName = CandidateName;
				return true;
			}

			if (ReservedDestinationObjects.Contains(CandidateObjectPath))
			{
				continue;
			}

			if (UEditorAssetLibrary::DoesAssetExist(CandidateAssetPath))
			{
				continue;
			}

			OutDestinationAssetPath = CandidateAssetPath;
			OutDestinationObjectPath = CandidateObjectPath;
			OutResolvedAssetName = CandidateName;
			return true;
		}

		return false;
	}

	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		const FHCIAbilityKitAssetRoutingRules RoutingRules = HCI_LoadAssetRoutingRules();

		// Artists want immediate feedback during heavy rename/move operations; use a bottom-right throbber notification.
		struct FHCINamingProgressNotification
		{
			TSharedPtr<SNotificationItem> Item;
			bool bFinished = false;

			void Start(const FString& Title)
			{
				FNotificationInfo Info(FText::FromString(Title));
				Info.bFireAndForget = false;
				Info.bUseThrobber = true;
				Info.FadeOutDuration = 0.2f;
				Info.ExpireDuration = 0.0f;
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

			void Finish(const bool bOk, const FString& Text)
			{
				bFinished = true;
				if (!Item.IsValid())
				{
					return;
				}
				Item->SetText(FText::FromString(Text));
				Item->SetCompletionState(bOk ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
				Item->ExpireAndFadeout();
			}

			~FHCINamingProgressNotification()
			{
				if (Item.IsValid() && !bFinished)
				{
					Item->ExpireAndFadeout();
				}
			}
		};

		TArray<FString> AssetPaths;
		FString MetadataSource;
		FString PrefixMode;
		FString TargetRoot;
		const FHCIAbilityKitToolActionParamParser Params(Request.Args);
		if (!Params.TryGetRequiredStringArray(TEXT("asset_paths"), AssetPaths) ||
			!Params.TryGetRequiredString(TEXT("metadata_source"), MetadataSource) ||
			!Params.TryGetRequiredString(TEXT("prefix_mode"), PrefixMode) ||
			!Params.TryGetRequiredString(TEXT("target_root"), TargetRoot))
		{
			return FHCIAbilityKitToolActionEvidenceBuilder::FailRequiredArgMissing(OutResult);
		}

		if (!PrefixMode.Equals(TEXT("auto_by_asset_class"), ESearchCase::CaseSensitive))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4011");
			OutResult.Reason = TEXT("prefix_mode_not_supported");
			return false;
		}

		FHCINamingProgressNotification Notify;
		Notify.Start(bIsDryRun ? TEXT("HCIAbilityKit: 预览资产规范化/归档中...") : TEXT("HCIAbilityKit: 执行资产规范化/归档中..."));
		Notify.Update(FString::Printf(TEXT("阶段：准备中 (assets=%d)"), AssetPaths.Num()));

		TargetRoot = HCIAbilityKitAssetPathUtils::TrimTrailingSlash(TargetRoot);
		if (TargetRoot.IsEmpty() || !TargetRoot.StartsWith(TEXT("/Game/")))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("target_root_must_start_with_game");
			Notify.Finish(false, TEXT("失败：target_root 非 /Game/... 路径"));
			return false;
		}

		TArray<FHCINamingProposal> Proposals;
		TSet<FString> ReservedDestinationObjects;
		TArray<FString> ProposedRenameRows;
		TArray<FString> ProposedMoveRows;
		TArray<FString> FailedRows;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		struct FHCINormalizeItem
		{
			FString SourceAssetPath;
			FString SourceObjectPath;
			FString SourcePackagePath;
			FString SourceAssetName;
			FName PackageName;
			FAssetData AssetData;
			FString Prefix;
			FString BaseNameFromMetadata;
			EHCITextureRole TextureRole = EHCITextureRole::Unknown;
			FString GroupNameFromName;
			FString EffectiveGroupName;
			bool bIsAnchor = false;
			bool bIsShared = false;
		};

		TArray<FHCINormalizeItem> Items;
		Items.Reserve(AssetPaths.Num());
		TSet<FName> SelectionPackages;

		Notify.Update(TEXT("阶段：解析资产列表与元数据..."));
		for (const FString& RawPath : AssetPaths)
		{
			FHCINormalizeItem Item;

			FHCIAbilityKitToolActionAssetPathNormalizer::NormalizeAssetPathVariants(RawPath, Item.SourceAssetPath, Item.SourceObjectPath);

			if (!HCIAbilityKitAssetPathUtils::TrySplitObjectPath(Item.SourceObjectPath, Item.SourcePackagePath, Item.SourceAssetName))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (invalid_object_path)"), *Item.SourceObjectPath));
				continue;
			}

			Item.AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Item.SourceObjectPath));
			if (!Item.AssetData.IsValid())
			{
				// Fallback: confirm existence via editor library so user gets a clearer error.
				if (!UEditorAssetLibrary::DoesAssetExist(Item.SourceAssetPath))
				{
					FailedRows.Add(FString::Printf(TEXT("%s (not_found)"), *Item.SourceAssetPath));
				}
				else
				{
					FailedRows.Add(FString::Printf(TEXT("%s (asset_registry_missing)"), *Item.SourceObjectPath));
				}
				continue;
			}

			Item.PackageName = Item.AssetData.PackageName;
			SelectionPackages.Add(Item.PackageName);

			Item.Prefix = HCIAbilityKitAssetNamingRules::DeriveClassPrefixFromAssetData(Item.AssetData);
			Item.bIsAnchor = (Item.Prefix == TEXT("SM") || Item.Prefix == TEXT("SK"));

			FString FailureReason;
			if (!HCI_TryBuildNormalizedNameBaseFromAssetData(
					Item.AssetData,
					MetadataSource,
					Item.Prefix,
					Item.SourceAssetName,
					Item.BaseNameFromMetadata,
					FailureReason))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (%s)"), *Item.SourceObjectPath, *FailureReason));
				continue;
			}

			TArray<FString> Tokens;
			Item.BaseNameFromMetadata.ParseIntoArray(Tokens, TEXT("_"), true);
			TArray<FString> TokensLower;
			TokensLower.Reserve(Tokens.Num());
			for (const FString& Tok : Tokens)
			{
				TokensLower.Add(Tok.ToLower());
			}

			Item.TextureRole = EHCITextureRole::Unknown;
			if (Item.Prefix == TEXT("T"))
			{
				Item.TextureRole = HCI_DetectTextureRoleFromTokens(TokensLower);
				if (Item.TextureRole != EHCITextureRole::Unknown && TokensLower.Num() > 0)
				{
					TokensLower.Pop();
				}
			}

			HCI_TrimCommonTrailingNoiseTokens(TokensLower);

			FString GroupSanitized;
			if (TokensLower.Num() > 0)
			{
				GroupSanitized = FString::Join(TokensLower, TEXT("_"));
			}
			if (GroupSanitized.IsEmpty())
			{
				GroupSanitized = Item.BaseNameFromMetadata;
			}

			FString GroupName = HCIAbilityKitAssetNamingRules::SanitizeIdentifier(GroupSanitized);
			if (RoutingRules.bGroupNamePascalCase)
			{
				GroupName = HCI_ToPascalCase(GroupName);
				GroupName = HCIAbilityKitAssetNamingRules::SanitizeIdentifier(GroupName);
			}
			if (GroupName.IsEmpty())
			{
				GroupName = TEXT("Asset");
			}

			Item.GroupNameFromName = GroupName;
			Item.EffectiveGroupName = GroupName;
			Items.Add(MoveTemp(Item));
		}

		// Dependency-aware grouping: assign textures/materials into the mesh's asset-group folder, and move shared assets into Shared/.
		int32 AnchorCount = 0;
		int32 SharedCount = 0;
		if (RoutingRules.bDependencyAware)
		{
			TArray<FName> Anchors;
			TMap<FName, FString> AnchorGroupNames;
			for (const FHCINormalizeItem& Item : Items)
			{
				if (Item.bIsAnchor)
				{
					Anchors.Add(Item.PackageName);
					AnchorGroupNames.Add(Item.PackageName, Item.GroupNameFromName);
				}
			}

			AnchorCount = Anchors.Num();
			if (Anchors.Num() > 0 && SelectionPackages.Num() > 0)
			{
				TMap<FName, TArray<FName>> DepsByPkg;
				DepsByPkg.Reserve(SelectionPackages.Num());

				for (const FName& Pkg : SelectionPackages)
				{
					TArray<FName> Deps;
					AssetRegistry.GetDependencies(
						Pkg,
						Deps,
						UE::AssetRegistry::EDependencyCategory::Package,
						UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard));
					Deps.RemoveAll([&SelectionPackages](const FName& Dep) { return !SelectionPackages.Contains(Dep); });
					DepsByPkg.Add(Pkg, MoveTemp(Deps));
				}

				TMap<FName, int32> MemberCount;
				TMap<FName, FName> SoleAnchor;

				for (const FName& AnchorPkg : Anchors)
				{
					TArray<FName> Stack;
					Stack.Add(AnchorPkg);
					TSet<FName> Visited;
					Visited.Reserve(64);
					Visited.Add(AnchorPkg);

					while (Stack.Num() > 0)
					{
						const FName Current = Stack.Pop(EAllowShrinking::No);
						const TArray<FName>* Deps = DepsByPkg.Find(Current);
						if (!Deps)
						{
							continue;
						}
						for (const FName& Dep : *Deps)
						{
							if (Visited.Contains(Dep))
							{
								continue;
							}
							Visited.Add(Dep);
							Stack.Add(Dep);
						}
					}

					for (const FName& Member : Visited)
					{
						int32& C = MemberCount.FindOrAdd(Member);
						++C;
						if (C == 1)
						{
							SoleAnchor.Add(Member, AnchorPkg);
						}
						else
						{
							SoleAnchor.Add(Member, NAME_None);
						}
					}
				}

				for (FHCINormalizeItem& Item : Items)
				{
					if (Item.bIsAnchor)
					{
						continue;
					}

					const int32* C = MemberCount.Find(Item.PackageName);
					if (C && *C > 1)
					{
						Item.bIsShared = true;
						Item.EffectiveGroupName = RoutingRules.SharedRoot;
						++SharedCount;
						continue;
					}

					const FName* AnchorPkg = SoleAnchor.Find(Item.PackageName);
					if (AnchorPkg && AnchorPkg->IsValid())
					{
						const FString* AnchorGroup = AnchorGroupNames.Find(*AnchorPkg);
						if (AnchorGroup && !AnchorGroup->IsEmpty())
						{
							Item.EffectiveGroupName = *AnchorGroup;
						}
					}
				}
			}
		}

		for (const FHCINormalizeItem& Item : Items)
		{
			const FString& Prefix = Item.Prefix;

			FString GroupNameForNaming = Item.EffectiveGroupName;
			if (GroupNameForNaming.IsEmpty() || GroupNameForNaming == RoutingRules.SharedRoot)
			{
				GroupNameForNaming = Item.GroupNameFromName;
			}

			const FString RoleSuffix = HCI_TextureRoleSuffix(Item.TextureRole);

			FString AssetBaseName;
			if (Item.bIsShared)
			{
				AssetBaseName = HCIAbilityKitAssetNamingRules::SanitizeIdentifier(Item.BaseNameFromMetadata);
				if (RoutingRules.bGroupNamePascalCase)
				{
					AssetBaseName = HCI_ToPascalCase(AssetBaseName);
					AssetBaseName = HCIAbilityKitAssetNamingRules::SanitizeIdentifier(AssetBaseName);
				}
				if (Prefix == TEXT("T") && !RoleSuffix.IsEmpty() && !AssetBaseName.EndsWith(TEXT("_") + RoleSuffix, ESearchCase::IgnoreCase))
				{
					AssetBaseName = FString::Printf(TEXT("%s_%s"), *AssetBaseName, *RoleSuffix);
				}
			}
			else
			{
				AssetBaseName = GroupNameForNaming;
				if (Prefix == TEXT("T") && !RoleSuffix.IsEmpty())
				{
					AssetBaseName = FString::Printf(TEXT("%s_%s"), *GroupNameForNaming, *RoleSuffix);
				}
			}

			AssetBaseName = HCIAbilityKitAssetNamingRules::SanitizeIdentifier(AssetBaseName);
			if (AssetBaseName.IsEmpty())
			{
				FailedRows.Add(FString::Printf(TEXT("%s (target_name_invalid)"), *Item.SourceObjectPath));
				continue;
			}

			FString TargetAssetName = FString::Printf(TEXT("%s_%s"), *Prefix, *AssetBaseName);
			TargetAssetName = HCIAbilityKitAssetNamingRules::SanitizeIdentifier(TargetAssetName);
			if (TargetAssetName.IsEmpty())
			{
				FailedRows.Add(FString::Printf(TEXT("%s (target_name_invalid)"), *Item.SourceObjectPath));
				continue;
			}

			FString DestinationDir = TargetRoot;
			if (Item.bIsShared)
			{
				DestinationDir = FString::Printf(TEXT("%s/%s"), *DestinationDir, *RoutingRules.SharedRoot);
			}
			else if (RoutingRules.bPerAssetFolder)
			{
				DestinationDir = FString::Printf(TEXT("%s/%s"), *DestinationDir, *Item.EffectiveGroupName);
			}

			{
				const FString* Subfolder = RoutingRules.SubfoldersByPrefix.Find(Prefix);
				if (Subfolder && !Subfolder->IsEmpty())
				{
					DestinationDir = FString::Printf(TEXT("%s/%s"), *DestinationDir, **Subfolder);
				}
			}

			DestinationDir = HCIAbilityKitAssetPathUtils::TrimTrailingSlash(DestinationDir);

			FString DestinationAssetPath;
			FString DestinationObjectPath;
			FString ResolvedAssetName;
			if (!HCI_TryFindAvailableDestination(
					Item.SourceObjectPath,
					DestinationDir,
					TargetAssetName,
					ReservedDestinationObjects,
					DestinationAssetPath,
					DestinationObjectPath,
					ResolvedAssetName))
			{
				FailedRows.Add(FString::Printf(TEXT("%s (destination_unavailable)"), *Item.SourceObjectPath));
				continue;
			}

			if (DestinationObjectPath.Equals(Item.SourceObjectPath, ESearchCase::CaseSensitive))
			{
				continue;
			}

			FHCINamingProposal& Proposal = Proposals.AddDefaulted_GetRef();
			Proposal.SourceAssetPath = Item.SourceAssetPath;
			Proposal.SourceObjectPath = Item.SourceObjectPath;
			Proposal.SourceAssetName = Item.SourceAssetName;
			Proposal.SourceDirectory = HCIAbilityKitAssetPathUtils::GetDirectoryFromPackagePath(Item.SourcePackagePath);
			Proposal.DestinationAssetPath = DestinationAssetPath;
			Proposal.DestinationObjectPath = DestinationObjectPath;
			Proposal.DestinationAssetName = ResolvedAssetName.IsEmpty() ? TargetAssetName : ResolvedAssetName;
			Proposal.DestinationDirectory = DestinationDir;
			Proposal.bNeedsRename = !Item.SourceAssetName.Equals(Proposal.DestinationAssetName, ESearchCase::CaseSensitive);
			Proposal.bNeedsMove = !Proposal.SourceDirectory.Equals(DestinationDir, ESearchCase::CaseSensitive);
			ReservedDestinationObjects.Add(DestinationObjectPath);

			if (Proposal.bNeedsRename)
			{
				ProposedRenameRows.Add(FString::Printf(TEXT("%s -> %s"), *Proposal.SourceAssetName, *Proposal.DestinationAssetName));
			}
			if (Proposal.bNeedsMove)
			{
				ProposedMoveRows.Add(FString::Printf(TEXT("%s -> %s"), *Proposal.SourceAssetPath, *Proposal.DestinationAssetPath));
			}
		}

		Notify.Update(FString::Printf(TEXT("阶段：已生成提案 (proposals=%d)"), Proposals.Num()));
		int32 AppliedCount = 0;
		if (!bIsDryRun && Proposals.Num() > 0)
		{
			Notify.Update(FString::Printf(TEXT("阶段：执行移动/重命名... (ops=%d)"), Proposals.Num()));
			// Ensure destination directories exist before moving packages.
			UEditorAssetLibrary::MakeDirectory(TargetRoot);
			{
				TSet<FString> Dirs;
				for (const FHCINamingProposal& Proposal : Proposals)
				{
					if (!Proposal.DestinationDirectory.IsEmpty())
					{
						Dirs.Add(Proposal.DestinationDirectory);
					}
				}
				for (const FString& Dir : Dirs)
				{
					UEditorAssetLibrary::MakeDirectory(Dir);
				}
			}

			for (const FHCINamingProposal& Proposal : Proposals)
			{
				if (!HCI_MoveAssetWithAssetTools(Proposal.SourceObjectPath, Proposal.DestinationObjectPath))
				{
					FailedRows.Add(FString::Printf(TEXT("%s (rename_move_failed)"), *Proposal.SourceObjectPath));
					continue;
				}
				++AppliedCount;
				if ((AppliedCount % 10) == 0)
				{
					Notify.Update(FString::Printf(TEXT("阶段：执行移动/重命名... (%d/%d)"), AppliedCount, Proposals.Num()));
					// Keep editor UI responsive during large batch operations.
					FSlateApplication::Get().PumpMessages();
				}
			}
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		const int32 AffectedCount = bIsDryRun ? Proposals.Num() : AppliedCount;
		const bool bHasSuccess = AffectedCount > 0;
		const bool bAllFailed = !bHasSuccess && FailedRows.Num() > 0;
		OutResult.bSucceeded = !bAllFailed;
		OutResult.ErrorCode = bAllFailed ? TEXT("E4012") : FString();
		OutResult.Reason = bIsDryRun
			? (bAllFailed ? TEXT("normalize_asset_naming_by_metadata_dry_run_failed") : TEXT("normalize_asset_naming_by_metadata_dry_run_ok"))
			: (bAllFailed ? TEXT("normalize_asset_naming_by_metadata_execute_failed") : TEXT("normalize_asset_naming_by_metadata_execute_ok"));
		OutResult.EstimatedAffectedCount = AffectedCount;

		OutResult.Evidence.Add(TEXT("proposed_renames"), ProposedRenameRows.Num() > 0 ? FString::Join(ProposedRenameRows, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("proposed_moves"), ProposedMoveRows.Num() > 0 ? FString::Join(ProposedMoveRows, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("affected_count"), FString::FromInt(AffectedCount));
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);

		if (FailedRows.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedRows, TEXT(" | ")));
		}

		Notify.Finish(
			OutResult.bSucceeded,
			OutResult.bSucceeded ? TEXT("完成：资产规范化/归档") : TEXT("完成：资产规范化/归档（失败）"));
		return OutResult.bSucceeded;
	}
};
}

TSharedPtr<IHCIAbilityKitAgentToolAction> HCIAbilityKitToolActionFactories::MakeNormalizeAssetNamingByMetadataToolAction()
{
	return MakeShared<FHCIAbilityKitNormalizeAssetNamingByMetadataToolAction>();
}
