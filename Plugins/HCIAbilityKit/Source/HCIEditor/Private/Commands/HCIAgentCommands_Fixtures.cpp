#include "Commands/HCIAgentDemoConsoleCommands.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectRedirector.h"
#include "Engine/StaticMesh.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIFixtures, Log, All);

namespace
{
static const TCHAR* HCI_SeedRoot = TEXT("/Game/Seed");
static const TCHAR* HCI_SnapshotRoot = TEXT("/Game/__HCI_Test/Fixtures/SeedChaosSnapshot");
static const TCHAR* HCI_IncomingRoot = TEXT("/Game/__HCI_Test/Incoming/SeedChaos");
static const TCHAR* HCI_OrganizedRoot = TEXT("/Game/__HCI_Test/Organized/SeedClean");

static const TCHAR* HCI_MatLinkSnapshotRoot = TEXT("/Game/__HCI_Test/Fixtures/MatLinkSnapshot");
static const TCHAR* HCI_MatLinkMastersRoot = TEXT("/Game/__HCI_Test/Masters");
static const TCHAR* HCI_MatLinkChaosRoot = TEXT("/Game/__HCI_Test/Incoming/MatLinkChaos");
static const TCHAR* HCI_MatLinkCleanRoot = TEXT("/Game/__HCI_Test/Organized/MatLinkClean");

static bool HCI_IsAllowedTestRoot(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game/__HCI_Test/"), ESearchCase::CaseSensitive)
		|| Path.StartsWith(TEXT("/Game/__HCI_Test"), ESearchCase::CaseSensitive)
		// Legacy root kept only to allow one-shot cleanup on older workspaces.
		|| Path.StartsWith(TEXT("/Game/_HCI_Test/"), ESearchCase::CaseSensitive)
		|| Path.StartsWith(TEXT("/Game/_HCI_Test"), ESearchCase::CaseSensitive);
}

static int32 HCI_HashStringToIndex(const FString& S, const int32 Mod)
{
	if (Mod <= 0)
	{
		return 0;
	}
	const uint32 H = GetTypeHash(S);
	return static_cast<int32>(H % static_cast<uint32>(Mod));
}

static FString HCI_SanitizeIdentifierLocal(FString InOut)
{
	InOut.TrimStartAndEndInline();
	InOut.ReplaceInline(TEXT(" "), TEXT("_"));
	InOut.ReplaceInline(TEXT("-"), TEXT("_"));
	InOut.ReplaceInline(TEXT("."), TEXT("_"));
	InOut.ReplaceInline(TEXT("/"), TEXT("_"));
	InOut.ReplaceInline(TEXT("\\"), TEXT("_"));
	InOut.ReplaceInline(TEXT(":"), TEXT("_"));
	InOut.ReplaceInline(TEXT(","), TEXT("_"));

	FString Out;
	Out.Reserve(InOut.Len());
	for (const TCHAR C : InOut)
	{
		if ((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_')
		{
			Out.AppendChar(C);
		}
	}
	while (Out.Contains(TEXT("__")))
	{
		Out.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	while (Out.StartsWith(TEXT("_")))
	{
		Out.RightChopInline(1, false);
	}
	while (Out.EndsWith(TEXT("_")))
	{
		Out.LeftChopInline(1, false);
	}
	if (!Out.IsEmpty() && Out.Len() > 64)
	{
		Out.LeftInline(64, false);
	}
	if (!Out.IsEmpty() && Out.Len() >= 1 && (Out[0] >= '0' && Out[0] <= '9'))
	{
		Out = FString::Printf(TEXT("A_%s"), *Out);
	}
	return Out;
}

static FString HCI_ToObjectPathMaybe(const FString& AssetPath)
{
	if (AssetPath.Contains(TEXT(".")))
	{
		return AssetPath;
	}
	int32 LastSlash = INDEX_NONE;
	if (!AssetPath.FindLastChar(TEXT('/'), LastSlash) || LastSlash <= 0 || LastSlash + 1 >= AssetPath.Len())
	{
		return AssetPath;
	}
	const FString Name = AssetPath.Mid(LastSlash + 1);
	return FString::Printf(TEXT("%s.%s"), *AssetPath, *Name);
}

static FString HCI_ReadableAssetClassPrefixFromAssetData(const FAssetData& AssetData)
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

static bool HCI_DeleteDirectoryWithConfirm(const FString& Directory, const FString& HumanLabel)
{
	if (!HCI_IsAllowedTestRoot(Directory))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] reject_delete not_test_root dir=%s"), *Directory);
		return false;
	}

	if (!UEditorAssetLibrary::DoesDirectoryExist(Directory))
	{
		return true;
	}

	const FString ConfirmText = FString::Printf(
		TEXT("将删除测试目录（仅限 __HCI_Test；同时支持清理 legacy /Game/_HCI_Test）：\n\n%s\n\n目录：%s\n\n继续？"),
		*HumanLabel,
		*Directory);
	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmText));
	if (Choice != EAppReturnType::Yes)
	{
		UE_LOG(LogHCIFixtures, Warning, TEXT("[HCI][Fixtures] delete_cancelled dir=%s"), *Directory);
		return false;
	}

	const bool bOk = UEditorAssetLibrary::DeleteDirectory(Directory);
	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] delete_dir ok=%s dir=%s"), bOk ? TEXT("true") : TEXT("false"), *Directory);
	return bOk;
}

static void HCI_WriteLatestFixtureReport(const TArray<FString>& CopiedAssets, const TArray<FString>& DirtyMoves, const TArray<FString>& DirtyRenames)
{
	const FString OutDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCI/TestFixtures/SeedChaos"));
	IFileManager::Get().MakeDirectory(*OutDir, true);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("seed_root"), HCI_SeedRoot);
	Root->SetStringField(TEXT("snapshot_root"), HCI_SnapshotRoot);
	Root->SetStringField(TEXT("incoming_root"), HCI_IncomingRoot);
	Root->SetStringField(TEXT("organized_root"), HCI_OrganizedRoot);
	Root->SetNumberField(TEXT("copied_count"), CopiedAssets.Num());

	auto ToJsonArray = [](const TArray<FString>& Rows)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Reserve(Rows.Num());
		for (const FString& Row : Rows)
		{
			Arr.Add(MakeShared<FJsonValueString>(Row));
		}
		return Arr;
	};

	Root->SetArrayField(TEXT("copied_assets"), ToJsonArray(CopiedAssets));
	Root->SetArrayField(TEXT("dirty_moves"), ToJsonArray(DirtyMoves));
	Root->SetArrayField(TEXT("dirty_renames"), ToJsonArray(DirtyRenames));

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	const FString OutPath = FPaths::Combine(OutDir, TEXT("latest.json"));
	FFileHelper::SaveStringToFile(JsonText, *OutPath);
	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] wrote_report path=%s bytes=%d"), *OutPath, JsonText.Len());
}

static void HCI_WriteLatestMatLinkFixtureReport(
	const FString& SelectedSeedAnchor,
	const FString& SelectedSeedMasterMaterial,
	const TMap<FString, FString>& SelectedSeedTexturesByRole,
	const FString& GroupHint,
	const FString& SnapshotMeshObjectPath,
	const FString& SnapshotMasterMaterialObjectPath,
	const TMap<FString, FString>& SnapshotTexturesByRole,
	const TArray<FString>& TextureParameterNames,
	const TArray<FString>& ChaosScenarioRoots)
{
	const FString OutDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCI/TestFixtures/MatLink"));
	IFileManager::Get().MakeDirectory(*OutDir, true);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("seed_root"), HCI_SeedRoot);
	Root->SetStringField(TEXT("snapshot_root"), HCI_MatLinkSnapshotRoot);
	Root->SetStringField(TEXT("masters_root"), HCI_MatLinkMastersRoot);
	Root->SetStringField(TEXT("chaos_root"), HCI_MatLinkChaosRoot);
	Root->SetStringField(TEXT("clean_root"), HCI_MatLinkCleanRoot);
	Root->SetStringField(TEXT("selected_seed_anchor"), SelectedSeedAnchor);
	Root->SetStringField(TEXT("selected_seed_master_material"), SelectedSeedMasterMaterial);
	Root->SetStringField(TEXT("seed_group_hint"), GroupHint);
	Root->SetStringField(TEXT("snapshot_mesh_object_path"), SnapshotMeshObjectPath);
	Root->SetStringField(TEXT("snapshot_master_material_object_path"), SnapshotMasterMaterialObjectPath);

	auto MapToJsonObject = [](const TMap<FString, FString>& M)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& It : M)
		{
			Obj->SetStringField(It.Key, It.Value);
		}
		return Obj;
	};

	Root->SetObjectField(TEXT("selected_seed_textures_by_role"), MapToJsonObject(SelectedSeedTexturesByRole));
	Root->SetObjectField(TEXT("snapshot_textures_by_role"), MapToJsonObject(SnapshotTexturesByRole));

	TArray<TSharedPtr<FJsonValue>> ParamArr;
	ParamArr.Reserve(TextureParameterNames.Num());
	for (const FString& Name : TextureParameterNames)
	{
		ParamArr.Add(MakeShared<FJsonValueString>(Name));
	}
	Root->SetArrayField(TEXT("texture_parameter_names"), ParamArr);

	TArray<TSharedPtr<FJsonValue>> ScenarioArr;
	ScenarioArr.Reserve(ChaosScenarioRoots.Num());
	for (const FString& R : ChaosScenarioRoots)
	{
		ScenarioArr.Add(MakeShared<FJsonValueString>(R));
	}
	Root->SetArrayField(TEXT("chaos_scenario_roots"), ScenarioArr);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	const FString OutPath = FPaths::Combine(OutDir, TEXT("latest.json"));
	FFileHelper::SaveStringToFile(JsonText, *OutPath);
	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] wrote_matlink_report path=%s bytes=%d"), *OutPath, JsonText.Len());
}


enum class EHCISeedChaosJobKind : uint8
{
	None = 0,
	BuildSnapshot,
	Reset
};

enum class EHCISeedChaosJobStage : uint8
{
	None = 0,
	Build_Duplicate,
	Build_Save,
	Reset_Duplicate,
	Reset_ListIncoming,
	Reset_DirtyRename,
	Reset_FixupRedirectors,
	Reset_WriteReport,
	Done
};

struct FHCISeedChaosJobState
{
	EHCISeedChaosJobKind Kind = EHCISeedChaosJobKind::None;
	EHCISeedChaosJobStage Stage = EHCISeedChaosJobStage::None;

	TArray<FName> Packages;
	int32 Index = 0;
	int32 SuccessCount = 0;

	TArray<FString> IncomingAssetPaths;
	int32 RenameIndex = 0;
	TArray<FString> CopiedAssets;
	TArray<FString> DirtyMoves;
	TArray<FString> DirtyRenames;

	FTSTicker::FDelegateHandle TickerHandle;
	TSharedPtr<SNotificationItem> Notification;
	bool bRequestedCancel = false;

	void Reset()
	{
		Kind = EHCISeedChaosJobKind::None;
		Stage = EHCISeedChaosJobStage::None;
		Packages.Reset();
		Index = 0;
		SuccessCount = 0;
		IncomingAssetPaths.Reset();
		RenameIndex = 0;
		CopiedAssets.Reset();
		DirtyMoves.Reset();
		DirtyRenames.Reset();
		bRequestedCancel = false;
		Notification.Reset();
		TickerHandle.Reset();
	}

	bool IsActive() const
	{
		return Kind != EHCISeedChaosJobKind::None;
	}
};

static FHCISeedChaosJobState GSeedChaosJob;

static void HCI_UpdateSeedChaosNotification(const FString& Text, const SNotificationItem::ECompletionState State, const bool bExpire)
{
	if (!GSeedChaosJob.Notification.IsValid())
	{
		return;
	}

	GSeedChaosJob.Notification->SetText(FText::FromString(Text));
	GSeedChaosJob.Notification->SetCompletionState(State);
	if (bExpire)
	{
		GSeedChaosJob.Notification->ExpireAndFadeout();
	}
}

static bool HCI_TickSeedChaosJob(float DeltaTime)
{
	constexpr int32 MaxAssets = 50;

	if (!GSeedChaosJob.IsActive())
	{
		return false;
	}

	if (GSeedChaosJob.bRequestedCancel)
	{
		HCI_UpdateSeedChaosNotification(TEXT("SeedChaos：已取消"), SNotificationItem::CS_Fail, true);
		GSeedChaosJob.Reset();
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	auto DuplicateOne = [&](const FString& DestRoot, const FString& NameSuffix) -> bool
	{
		if (GSeedChaosJob.Index >= GSeedChaosJob.Packages.Num())
		{
			return false;
		}

		const FName Pkg = GSeedChaosJob.Packages[GSeedChaosJob.Index];
		++GSeedChaosJob.Index;

		TArray<FAssetData> AssetsInPkg;
		AssetRegistry.GetAssetsByPackageName(Pkg, AssetsInPkg);
		if (AssetsInPkg.Num() <= 0)
		{
			return true;
		}

		UObject* SourceObj = AssetsInPkg[0].GetAsset();
		if (!SourceObj)
		{
			return true;
		}

		const FString Prefix = HCI_ReadableAssetClassPrefixFromAssetData(AssetsInPkg[0]);
		const FString UniqueName = HCI_SanitizeIdentifierLocal(FString::Printf(TEXT("%s_%08x_%s"), *Prefix, GetTypeHash(Pkg.ToString()), *NameSuffix));
		UObject* NewObj = AssetToolsModule.Get().DuplicateAsset(UniqueName, DestRoot, SourceObj);
		if (NewObj)
		{
			++GSeedChaosJob.SuccessCount;
			GSeedChaosJob.CopiedAssets.Add(Pkg.ToString());
		}
		return true;
	};

	switch (GSeedChaosJob.Stage)
	{
	case EHCISeedChaosJobStage::Build_Duplicate:
	{
		DuplicateOne(HCI_SnapshotRoot, TEXT("seed"));
		const FString Msg = FString::Printf(TEXT("SeedChaos：生成快照中 %d/%d（成功 %d）"), GSeedChaosJob.Index, GSeedChaosJob.Packages.Num(), GSeedChaosJob.SuccessCount);
		HCI_UpdateSeedChaosNotification(Msg, SNotificationItem::CS_Pending, false);
		if (GSeedChaosJob.Index >= GSeedChaosJob.Packages.Num())
		{
			GSeedChaosJob.Stage = EHCISeedChaosJobStage::Build_Save;
		}
		return true;
	}
	case EHCISeedChaosJobStage::Build_Save:
	{
		UEditorAssetLibrary::SaveDirectory(HCI_SnapshotRoot, true, true);
		HCI_UpdateSeedChaosNotification(TEXT("SeedChaos：快照生成完成"), SNotificationItem::CS_Success, true);
		GSeedChaosJob.Reset();
		return false;
	}
	case EHCISeedChaosJobStage::Reset_Duplicate:
	{
		DuplicateOne(HCI_IncomingRoot, TEXT("copy"));
		const FString Msg = FString::Printf(TEXT("SeedChaos：复制快照到 Incoming %d/%d（成功 %d）"), GSeedChaosJob.Index, GSeedChaosJob.Packages.Num(), GSeedChaosJob.SuccessCount);
		HCI_UpdateSeedChaosNotification(Msg, SNotificationItem::CS_Pending, false);
		if (GSeedChaosJob.Index >= GSeedChaosJob.Packages.Num())
		{
			GSeedChaosJob.Stage = EHCISeedChaosJobStage::Reset_ListIncoming;
		}
		return true;
	}
	case EHCISeedChaosJobStage::Reset_ListIncoming:
	{
		GSeedChaosJob.IncomingAssetPaths = UEditorAssetLibrary::ListAssets(HCI_IncomingRoot, true, false);
		GSeedChaosJob.RenameIndex = 0;
		GSeedChaosJob.Stage = EHCISeedChaosJobStage::Reset_DirtyRename;
		return true;
	}
	case EHCISeedChaosJobStage::Reset_DirtyRename:
	{
		if (GSeedChaosJob.RenameIndex >= GSeedChaosJob.IncomingAssetPaths.Num())
		{
			GSeedChaosJob.Stage = EHCISeedChaosJobStage::Reset_FixupRedirectors;
			return true;
		}

		const FString SrcAssetPath = GSeedChaosJob.IncomingAssetPaths[GSeedChaosJob.RenameIndex];
		++GSeedChaosJob.RenameIndex;

		const FAssetData Data = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(HCI_ToObjectPathMaybe(SrcAssetPath)));
		UObject* Loaded = Data.GetAsset();
		if (!Loaded)
		{
			return true;
		}

		const TArray<FString> Buckets = {TEXT("A"), TEXT("Misc"), TEXT("TexturesButMixed"), TEXT("Whatever")};
		const FString SrcPkg = Data.PackageName.ToString();
		const FString Prefix = HCI_ReadableAssetClassPrefixFromAssetData(Data);
		const int32 BucketIndex = HCI_HashStringToIndex(SrcPkg, Buckets.Num());
		const FString BucketDir = FString::Printf(TEXT("%s/%s"), HCI_IncomingRoot, *Buckets[BucketIndex]);
		UEditorAssetLibrary::MakeDirectory(BucketDir);

		const FString DirtyBase = HCI_SanitizeIdentifierLocal(FString::Printf(TEXT("%s_%08x_final_v2"), *Prefix, GetTypeHash(SrcPkg)));
		TArray<FAssetRenameData> RenameRequests;
		RenameRequests.Emplace(Loaded, BucketDir, DirtyBase);
		AssetToolsModule.Get().RenameAssets(RenameRequests);

		GSeedChaosJob.DirtyMoves.Add(FString::Printf(TEXT("%s -> %s/%s"), *SrcPkg, *BucketDir, *DirtyBase));
		GSeedChaosJob.DirtyRenames.Add(FString::Printf(TEXT("%s -> %s"), *Data.AssetName.ToString(), *DirtyBase));

		const FString Msg = FString::Printf(TEXT("SeedChaos：制造混乱中 %d/%d"), GSeedChaosJob.RenameIndex, GSeedChaosJob.IncomingAssetPaths.Num());
		HCI_UpdateSeedChaosNotification(Msg, SNotificationItem::CS_Pending, false);
		return true;
	}
	case EHCISeedChaosJobStage::Reset_FixupRedirectors:
	{
		const TArray<FString> Assets = UEditorAssetLibrary::ListAssets(HCI_IncomingRoot, true, false);
		TArray<UObjectRedirector*> Redirectors;
		for (const FString& AssetPath : Assets)
		{
			const FString ObjectPath = HCI_ToObjectPathMaybe(AssetPath);
			UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadObject<UObject>(nullptr, *ObjectPath));
			if (Redirector)
			{
				Redirectors.Add(Redirector);
			}
		}
		if (Redirectors.Num() > 0)
		{
			AssetToolsModule.Get().FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);
		}
		GSeedChaosJob.Stage = EHCISeedChaosJobStage::Reset_WriteReport;
		return true;
	}
	case EHCISeedChaosJobStage::Reset_WriteReport:
	{
		HCI_WriteLatestFixtureReport(GSeedChaosJob.CopiedAssets, GSeedChaosJob.DirtyMoves, GSeedChaosJob.DirtyRenames);
		HCI_UpdateSeedChaosNotification(TEXT("SeedChaos：Reset 完成（可开始 Agent 整理）"), SNotificationItem::CS_Success, true);
		GSeedChaosJob.Reset();
		return false;
	}
	default:
		break;
	}

	GSeedChaosJob.Reset();
	return false;
}

static void HCI_StartSeedChaosJob(const EHCISeedChaosJobKind Kind, const EHCISeedChaosJobStage Stage, TArray<FName>&& Packages)
{
	if (GSeedChaosJob.IsActive())
	{
		UE_LOG(LogHCIFixtures, Warning, TEXT("[HCI][Fixtures] job_busy kind=%d stage=%d"), static_cast<int32>(GSeedChaosJob.Kind), static_cast<int32>(GSeedChaosJob.Stage));
		return;
	}

	GSeedChaosJob.Reset();
	GSeedChaosJob.Kind = Kind;
	GSeedChaosJob.Stage = Stage;
	GSeedChaosJob.Packages = MoveTemp(Packages);
	GSeedChaosJob.Index = 0;
	GSeedChaosJob.SuccessCount = 0;

	FNotificationInfo Info(FText::FromString(TEXT("SeedChaos：开始...")));
	Info.bFireAndForget = false;
	Info.bUseLargeFont = false;
	Info.FadeInDuration = 0.1f;
	Info.FadeOutDuration = 0.2f;
	Info.ExpireDuration = 1.0f;
	Info.bUseThrobber = true;
	GSeedChaosJob.Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (GSeedChaosJob.Notification.IsValid())
	{
		GSeedChaosJob.Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}

	GSeedChaosJob.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&HCI_TickSeedChaosJob), 0.0f);
}
}

static void HCI_RunSeedChaosBuildSnapshotCommand(const TArray<FString>& Args)
{
	constexpr int32 MaxAssets = 50;

	if (!UEditorAssetLibrary::DoesDirectoryExist(HCI_SeedRoot))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] seed_missing dir=%s"), HCI_SeedRoot);
		return;
	}

	// Recreate snapshot directory (safe: under __HCI_Test only).
	if (UEditorAssetLibrary::DoesDirectoryExist(HCI_SnapshotRoot))
	{
		if (!HCI_DeleteDirectoryWithConfirm(HCI_SnapshotRoot, TEXT("重建 SeedChaosSnapshot（测试快照）")))
		{
			return;
		}
	}
	UEditorAssetLibrary::MakeDirectory(HCI_SnapshotRoot);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Prefer anchors (meshes). If none exist in /Game/Seed, fall back to any assets.
	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssetsByPath(FName(HCI_SeedRoot), AllAssets, true);
	if (AllAssets.Num() <= 0)
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] seed_empty dir=%s"), HCI_SeedRoot);
		return;
	}

	TArray<FAssetData> Anchors;
	for (const FAssetData& A : AllAssets)
	{
		const FString Prefix = HCI_ReadableAssetClassPrefixFromAssetData(A);
		if (Prefix == TEXT("SM") || Prefix == TEXT("SK"))
		{
			Anchors.Add(A);
		}
	}

	TArray<FName> SelectedPackages;
	SelectedPackages.Reserve(MaxAssets);
	TSet<FName> SelectedSet;

	auto AddPkg = [&SelectedPackages, &SelectedSet](const FName Pkg)
	{
		if (!Pkg.IsNone() && !SelectedSet.Contains(Pkg))
		{
			SelectedSet.Add(Pkg);
			SelectedPackages.Add(Pkg);
		}
	};

	// Collect packages from a few anchors + their hard dependencies within /Game/Seed.
	const int32 MaxAnchors = FMath::Max(1, FMath::Min(5, Anchors.Num()));
	for (int32 i = 0; i < MaxAnchors; ++i)
	{
		const FAssetData& Anchor = Anchors[i];
		AddPkg(Anchor.PackageName);

		TArray<FName> Deps;
		AssetRegistry.GetDependencies(
			Anchor.PackageName,
			Deps,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard));
		for (const FName& Dep : Deps)
		{
			if (SelectedPackages.Num() >= MaxAssets)
			{
				break;
			}
			// Only include dependencies under /Game/Seed to keep fixture self-contained.
			const FString DepStr = Dep.ToString();
			if (DepStr.StartsWith(HCI_SeedRoot))
			{
				AddPkg(Dep);
			}
		}
		if (SelectedPackages.Num() >= MaxAssets)
		{
			break;
		}
	}

	// If no anchors or selection too small, top up with arbitrary assets from /Game/Seed.
	for (const FAssetData& A : AllAssets)
	{
		if (SelectedPackages.Num() >= MaxAssets)
		{
			break;
		}
		AddPkg(A.PackageName);
	}

	TArray<FString> SrcAssets;
	SrcAssets.Reserve(SelectedPackages.Num());
	for (const FName& Pkg : SelectedPackages)
	{
		SrcAssets.Add(Pkg.ToString());
	}

	HCI_StartSeedChaosJob(EHCISeedChaosJobKind::BuildSnapshot, EHCISeedChaosJobStage::Build_Duplicate, MoveTemp(SelectedPackages));
	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] build_snapshot started selected=%d snapshot=%s"), GSeedChaosJob.Packages.Num(), HCI_SnapshotRoot);
}

static void HCI_RunSeedChaosResetCommand(const TArray<FString>& Args)
{
	constexpr int32 MaxAssets = 50;

	if (!UEditorAssetLibrary::DoesDirectoryExist(HCI_SnapshotRoot))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] snapshot_missing dir=%s (run HCI.SeedChaosBuildSnapshot first)"), HCI_SnapshotRoot);
		return;
	}

	// Delete only the two test dirs you approved.
	if (!HCI_DeleteDirectoryWithConfirm(HCI_IncomingRoot, TEXT("重置混乱输入目录（SeedChaos）")))
	{
		return;
	}
	if (!HCI_DeleteDirectoryWithConfirm(HCI_OrganizedRoot, TEXT("重置整理输出目录（SeedClean）")))
	{
		return;
	}

	UEditorAssetLibrary::MakeDirectory(HCI_IncomingRoot);
	UEditorAssetLibrary::MakeDirectory(HCI_OrganizedRoot);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Copy snapshot packages back to incoming root (async tick job).
	TArray<FAssetData> SnapshotAssets;
	AssetRegistry.GetAssetsByPath(FName(HCI_SnapshotRoot), SnapshotAssets, true);
	if (SnapshotAssets.Num() <= 0)
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] snapshot_empty dir=%s"), HCI_SnapshotRoot);
		return;
	}

	TArray<FName> SnapshotPkgs;
	for (const FAssetData& A : SnapshotAssets)
	{
		SnapshotPkgs.Add(A.PackageName);
		if (SnapshotPkgs.Num() >= MaxAssets)
		{
			break;
		}
	}

	HCI_StartSeedChaosJob(EHCISeedChaosJobKind::Reset, EHCISeedChaosJobStage::Reset_Duplicate, MoveTemp(SnapshotPkgs));
	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] reset started snapshot_pkgs=%d incoming=%s organized=%s"), GSeedChaosJob.Packages.Num(), HCI_IncomingRoot, HCI_OrganizedRoot);
}

enum class EHCIMatLinkRole : uint8
{
	Unknown = 0,
	BC,
	N,
	ORM
};

static FString HCI_MatLinkRoleToString(const EHCIMatLinkRole Role)
{
	switch (Role)
	{
	case EHCIMatLinkRole::BC:
		return TEXT("BC");
	case EHCIMatLinkRole::N:
		return TEXT("N");
	case EHCIMatLinkRole::ORM:
		return TEXT("ORM");
	default:
		return TEXT("Unknown");
	}
}

static EHCIMatLinkRole HCI_DetectTextureRoleFromName(const FString& InName)
{
	const FString Name = InName.ToLower();

	auto HasToken = [&Name](const TCHAR* Token)
	{
		return Name.Contains(Token);
	};

	// Prefer explicit suffix-like tokens.
	if (HasToken(TEXT("_orm")) || HasToken(TEXT("occlusionroughnessmetallic")) || HasToken(TEXT("_rma")) || HasToken(TEXT("_mrao")) || HasToken(TEXT("_aorm")))
	{
		return EHCIMatLinkRole::ORM;
	}
	if (HasToken(TEXT("_bc")) || HasToken(TEXT("basecolor")) || HasToken(TEXT("albedo")) || HasToken(TEXT("diffuse")) || HasToken(TEXT("_col")) || HasToken(TEXT("_color")))
	{
		return EHCIMatLinkRole::BC;
	}
	if (HasToken(TEXT("_n")) || HasToken(TEXT("normal")) || HasToken(TEXT("_nrm")) || HasToken(TEXT("_nor")))
	{
		return EHCIMatLinkRole::N;
	}
	return EHCIMatLinkRole::Unknown;
}

static int32 HCI_TextureRoleMatchQuality(const FString& InName, const EHCIMatLinkRole Role)
{
	const FString Name = InName.ToLower();
	switch (Role)
	{
	case EHCIMatLinkRole::BC:
		if (Name.EndsWith(TEXT("_bc")) || Name.Contains(TEXT("_bc_")))
		{
			return 3;
		}
		if (Name.Contains(TEXT("basecolor")) || Name.Contains(TEXT("albedo")))
		{
			return 2;
		}
		if (Name.Contains(TEXT("diffuse")) || Name.Contains(TEXT("color")))
		{
			return 1;
		}
		return 0;
	case EHCIMatLinkRole::N:
		if (Name.EndsWith(TEXT("_n")) || Name.Contains(TEXT("_n_")))
		{
			return 3;
		}
		if (Name.Contains(TEXT("normal")))
		{
			return 2;
		}
		return 0;
	case EHCIMatLinkRole::ORM:
		if (Name.EndsWith(TEXT("_orm")) || Name.Contains(TEXT("_orm_")))
		{
			return 3;
		}
		if (Name.Contains(TEXT("occlusionroughnessmetallic")) || Name.Contains(TEXT("_rma")) || Name.Contains(TEXT("_mrao")))
		{
			return 2;
		}
		return 0;
	default:
		return 0;
	}
}

static FString HCI_AssetDataToObjectPathString(const FAssetData& Data)
{
	// Prefer UE's object path string. (e.g. /Game/A/B/Asset.Asset)
	return Data.GetObjectPathString();
}

static FString HCI_DeriveGroupHintFromAnchorName(const FString& AnchorAssetName)
{
	FString Name = AnchorAssetName;
	Name.TrimStartAndEndInline();
	if (Name.StartsWith(TEXT("SM_"), ESearchCase::IgnoreCase))
	{
		Name.RightChopInline(3, false);
	}
	// Pick first token to keep it short for demo naming.
	int32 Underscore = INDEX_NONE;
	if (Name.FindChar(TEXT('_'), Underscore) && Underscore > 0)
	{
		Name = Name.Left(Underscore);
	}
	if (Name.IsEmpty())
	{
		Name = TEXT("Asset");
	}
	return HCI_SanitizeIdentifierLocal(Name);
}

static bool HCI_SelectBestSeedPbrGroup(
	IAssetRegistry& AssetRegistry,
	FAssetData& OutAnchorMesh,
	FAssetData& OutMasterMaterial,
	TMap<EHCIMatLinkRole, FAssetData>& OutRoleTextures,
	FString& OutGroupHint)
{
	auto IsMaterial = [](const FAssetData& A)
	{
		const FString ClassName = A.AssetClassPath.GetAssetName().ToString();
		return ClassName.Equals(TEXT("Material"), ESearchCase::IgnoreCase);
	};

	TArray<FAssetData> AllSeedAssets;
	AssetRegistry.GetAssetsByPath(FName(HCI_SeedRoot), AllSeedAssets, true);

	TArray<FAssetData> SeedMeshes;
	for (const FAssetData& A : AllSeedAssets)
	{
		if (HCI_ReadableAssetClassPrefixFromAssetData(A) == TEXT("SM"))
		{
			SeedMeshes.Add(A);
		}
	}
	if (SeedMeshes.Num() <= 0)
	{
		return false;
	}

	auto CollectHardClosure = [&AssetRegistry](const FName RootPkg, TSet<FName>& OutPkgs, const int32 MaxPkgs)
	{
		OutPkgs.Reset();
		TArray<FName> Queue;
		Queue.Reserve(MaxPkgs);
		Queue.Add(RootPkg);
		OutPkgs.Add(RootPkg);

		for (int32 i = 0; i < Queue.Num() && OutPkgs.Num() < MaxPkgs; ++i)
		{
			const FName Pkg = Queue[i];
			TArray<FName> Deps;
			AssetRegistry.GetDependencies(
				Pkg,
				Deps,
				UE::AssetRegistry::EDependencyCategory::Package,
				UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard));
			for (const FName& Dep : Deps)
			{
				if (OutPkgs.Num() >= MaxPkgs)
				{
					break;
				}
				// Skip engine content; fixture wants game-side content.
				const FString DepStr = Dep.ToString();
				if (!DepStr.StartsWith(TEXT("/Game/")))
				{
					continue;
				}
				if (!OutPkgs.Contains(Dep))
				{
					OutPkgs.Add(Dep);
					Queue.Add(Dep);
				}
			}
		}
	};

	int32 BestScore = -1;
	FAssetData BestMesh;
	FAssetData BestMaster;
	TMap<EHCIMatLinkRole, FAssetData> BestRoleTextures;

	for (const FAssetData& Mesh : SeedMeshes)
	{
		// In real projects, textures are usually multi-hop deps (Mesh -> MI -> Textures).
		// Use a bounded hard-dependency closure instead of only 1-hop deps.
		TSet<FName> ClosurePkgs;
		CollectHardClosure(Mesh.PackageName, ClosurePkgs, /*MaxPkgs*/ 256);

		TArray<FAssetData> Materials;
		TArray<FAssetData> MaterialInstances;
		TMap<EHCIMatLinkRole, FAssetData> RoleTextures;
		TMap<EHCIMatLinkRole, int32> RoleQualities;
		TArray<FAssetData> AnyTextures;

		for (const FName& Pkg : ClosurePkgs)
		{
			TArray<FAssetData> AssetsInPkg;
			AssetRegistry.GetAssetsByPackageName(Pkg, AssetsInPkg);
			for (const FAssetData& A : AssetsInPkg)
			{
				const FString Prefix = HCI_ReadableAssetClassPrefixFromAssetData(A);
				if (Prefix == TEXT("T"))
				{
					AnyTextures.Add(A);
					const EHCIMatLinkRole Role = HCI_DetectTextureRoleFromName(A.AssetName.ToString());
					if (Role != EHCIMatLinkRole::Unknown)
					{
						const int32 Q = HCI_TextureRoleMatchQuality(A.AssetName.ToString(), Role);
						const int32 Prev = RoleQualities.Contains(Role) ? RoleQualities[Role] : -1;
						if (Q > Prev)
						{
							RoleQualities.Add(Role, Q);
							RoleTextures.Add(Role, A);
						}
					}
				}
				else if (Prefix == TEXT("MI"))
				{
					MaterialInstances.Add(A);
				}
				else if (Prefix == TEXT("M") && IsMaterial(A))
				{
					Materials.Add(A);
				}
			}
		}

		// We'll resolve the master precisely after choosing the best mesh by loading it.
		// Here we just require there is at least one material-like asset in the closure.
		const bool bHasAnyMaterialLike = (MaterialInstances.Num() > 0) || (Materials.Num() > 0);

		// Fallback: if naming doesn't reveal roles but we have textures, pick a deterministic 2-3 textures.
		if (!RoleTextures.Contains(EHCIMatLinkRole::BC) || !RoleTextures.Contains(EHCIMatLinkRole::N))
		{
			AnyTextures.Sort([](const FAssetData& L, const FAssetData& R) { return L.AssetName.LexicalLess(R.AssetName); });
			if (AnyTextures.Num() >= 2)
			{
				if (!RoleTextures.Contains(EHCIMatLinkRole::BC))
				{
					RoleTextures.Add(EHCIMatLinkRole::BC, AnyTextures[0]);
				}
				if (!RoleTextures.Contains(EHCIMatLinkRole::N))
				{
					RoleTextures.Add(EHCIMatLinkRole::N, AnyTextures[1]);
				}
				if (!RoleTextures.Contains(EHCIMatLinkRole::ORM) && AnyTextures.Num() >= 3)
				{
					RoleTextures.Add(EHCIMatLinkRole::ORM, AnyTextures[2]);
				}
			}
		}

		// Score: require BC + N, master must exist.
		const bool bHasBC = RoleTextures.Contains(EHCIMatLinkRole::BC);
		const bool bHasN = RoleTextures.Contains(EHCIMatLinkRole::N);
		const bool bHasORM = RoleTextures.Contains(EHCIMatLinkRole::ORM);
		if (!bHasAnyMaterialLike || !bHasBC || !bHasN)
		{
			continue;
		}

		int32 Score = 0;
		Score += bHasBC ? 10 : 0;
		Score += bHasN ? 10 : 0;
		Score += bHasORM ? 5 : 0;
		Score += FMath::Min(5, RoleTextures.Num()); // small bias for richer sets
		Score += MaterialInstances.Num() > 0 ? 2 : 0; // prefer projects using MI (more realistic)

		if (Score > BestScore)
		{
			BestScore = Score;
			BestMesh = Mesh;
			BestRoleTextures = RoleTextures;
		}
	}

	if (BestScore < 0 || !BestMesh.IsValid())
	{
		UE_LOG(LogHCIFixtures, Warning, TEXT("[HCI][Fixtures] matlink_no_candidate meshes=%d (need at least: mesh with 2 textures + material-like deps)"), SeedMeshes.Num());
		return false;
	}

	// Resolve master material by loading the selected mesh and inspecting its static materials.
	UMaterialInterface* MasterInterface = nullptr;
	{
		UStaticMesh* LoadedMesh = Cast<UStaticMesh>(BestMesh.GetAsset());
		if (LoadedMesh)
		{
			for (const FStaticMaterial& SM : LoadedMesh->GetStaticMaterials())
			{
				if (SM.MaterialInterface)
				{
					MasterInterface = SM.MaterialInterface;
					break;
				}
			}
		}
	}

	UMaterial* MasterMaterialObj = nullptr;
	if (MasterInterface)
	{
		// For MI, GetMaterial() returns the parent UMaterial.
		MasterMaterialObj = MasterInterface->GetMaterial();
	}

	if (MasterMaterialObj)
	{
		const FAssetData MasterData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(MasterMaterialObj));
		if (MasterData.IsValid())
		{
			BestMaster = MasterData;
		}
	}

	// Fallback: pick any Material asset from the mesh closure if we couldn't resolve via runtime object.
	if (!BestMaster.IsValid())
	{
		TSet<FName> ClosurePkgs;
		CollectHardClosure(BestMesh.PackageName, ClosurePkgs, /*MaxPkgs*/ 256);
		for (const FName& Pkg : ClosurePkgs)
		{
			TArray<FAssetData> AssetsInPkg;
			AssetRegistry.GetAssetsByPackageName(Pkg, AssetsInPkg);
			for (const FAssetData& A : AssetsInPkg)
			{
				if (HCI_ReadableAssetClassPrefixFromAssetData(A) == TEXT("M") && IsMaterial(A))
				{
					BestMaster = A;
					break;
				}
			}
			if (BestMaster.IsValid())
			{
				break;
			}
		}
	}

	if (!BestMaster.IsValid())
	{
		UE_LOG(LogHCIFixtures, Warning, TEXT("[HCI][Fixtures] matlink_master_not_found mesh=%s (will fail; Seed may not include any Material assets referenced by the mesh)"), *BestMesh.GetObjectPathString());
		return false;
	}

	OutAnchorMesh = BestMesh;
	OutMasterMaterial = BestMaster;
	OutRoleTextures = BestRoleTextures;
	OutGroupHint = HCI_DeriveGroupHintFromAnchorName(BestMesh.AssetName.ToString());
	return true;
}

static void HCI_CollectTextureParameterNames(UMaterialInterface* Material, TArray<FString>& OutNames)
{
	OutNames.Reset();
	if (!Material)
	{
		return;
	}
	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	Material->GetAllTextureParameterInfo(Infos, Ids);
	for (const FMaterialParameterInfo& Info : Infos)
	{
		OutNames.Add(Info.Name.ToString());
	}
	OutNames.Sort();
}

static bool HCI_SetStaticMeshDefaultMaterialByObjectPath(const FString& MeshObjectPath)
{
	const FString ObjectPath = HCI_ToObjectPathMaybe(MeshObjectPath);
	UStaticMesh* Mesh = Cast<UStaticMesh>(LoadObject<UObject>(nullptr, *ObjectPath));
	if (!Mesh)
	{
		return false;
	}
	Mesh->Modify();
	UMaterialInterface* DefaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
	for (FStaticMaterial& SM : Mesh->GetStaticMaterials())
	{
		SM.MaterialInterface = DefaultMat;
	}
	Mesh->MarkPackageDirty();
	Mesh->PostEditChange();
	const FString AssetPath = FSoftObjectPath(Mesh).GetAssetPathString();
	UEditorAssetLibrary::SaveAsset(AssetPath, false);
	return true;
}

enum class EHCIMatLinkJobKind : uint8
{
	None = 0,
	BuildSnapshot,
	Reset
};

enum class EHCIMatLinkJobStage : uint8
{
	None = 0,
	Build_Duplicate,
	Build_Save,
	Build_WriteReport,
	Reset_Duplicate,
	Reset_BreakMeshMaterials,
	Reset_Save,
	Reset_WriteReport,
	Done
};

struct FHCIMatLinkDupTask
{
	FString SourceObjectPath;
	FString DestDir;
	FString DestName;
	FString DebugLabel;
};

struct FHCIMatLinkJobState
{
	EHCIMatLinkJobKind Kind = EHCIMatLinkJobKind::None;
	EHCIMatLinkJobStage Stage = EHCIMatLinkJobStage::None;

	FString SelectedSeedAnchor;
	FString SelectedSeedMasterMaterial;
	TMap<FString, FString> SelectedSeedTexturesByRole;

	FString SnapshotMeshObjectPath;
	FString SnapshotMasterMaterialObjectPath;
	TMap<FString, FString> SnapshotTexturesByRole;

	FString GroupHint;
	TArray<FString> TextureParameterNames;
	TArray<FString> ChaosScenarioRoots;

	TArray<FHCIMatLinkDupTask> Tasks;
	int32 TaskIndex = 0;

	TArray<FString> CreatedMeshObjectPaths;

	TSharedPtr<SNotificationItem> Notification;
	FTSTicker::FDelegateHandle TickerHandle;

	bool IsActive() const { return Kind != EHCIMatLinkJobKind::None && Stage != EHCIMatLinkJobStage::None; }
	void Reset()
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
		if (Notification.IsValid())
		{
			Notification->ExpireAndFadeout();
			Notification.Reset();
		}
		Kind = EHCIMatLinkJobKind::None;
		Stage = EHCIMatLinkJobStage::None;
		SelectedSeedAnchor.Reset();
		SelectedSeedMasterMaterial.Reset();
		SelectedSeedTexturesByRole.Reset();
		SnapshotMeshObjectPath.Reset();
		SnapshotMasterMaterialObjectPath.Reset();
		SnapshotTexturesByRole.Reset();
		GroupHint.Reset();
		TextureParameterNames.Reset();
		ChaosScenarioRoots.Reset();
		Tasks.Reset();
		TaskIndex = 0;
		CreatedMeshObjectPaths.Reset();
	}
};

static FHCIMatLinkJobState GMatLinkJob;

static bool HCI_TryLoadLatestMatLinkFixtureReport(FHCIMatLinkJobState& InOutJob)
{
	const FString InPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCI/TestFixtures/MatLink/latest.json"));
	FString JsonText;
	if (!FPaths::FileExists(InPath) || !FFileHelper::LoadFileToString(JsonText, *InPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	auto ReadStringSafe = [&Root](const TCHAR* Key) -> FString
	{
		FString Out;
		Root->TryGetStringField(Key, Out);
		return Out;
	};

	const FString SeedAnchor = ReadStringSafe(TEXT("selected_seed_anchor"));
	const FString SeedMaster = ReadStringSafe(TEXT("selected_seed_master_material"));
	const FString GroupHint = ReadStringSafe(TEXT("seed_group_hint"));
	if (!SeedAnchor.IsEmpty())
	{
		InOutJob.SelectedSeedAnchor = SeedAnchor;
	}
	if (!SeedMaster.IsEmpty())
	{
		InOutJob.SelectedSeedMasterMaterial = SeedMaster;
	}
	if (!GroupHint.IsEmpty())
	{
		InOutJob.GroupHint = GroupHint;
	}

	const TSharedPtr<FJsonObject>* TexturesObj = nullptr;
	if (Root->TryGetObjectField(TEXT("selected_seed_textures_by_role"), TexturesObj) && TexturesObj && TexturesObj->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*TexturesObj)->Values)
		{
			if (It.Value.IsValid() && It.Value->Type == EJson::String)
			{
				InOutJob.SelectedSeedTexturesByRole.Add(It.Key, It.Value->AsString());
			}
		}
	}
	return true;
}

static void HCI_UpdateMatLinkNotification(const FString& Msg, const SNotificationItem::ECompletionState State, const bool bExpire)
{
	if (!GMatLinkJob.Notification.IsValid())
	{
		return;
	}
	GMatLinkJob.Notification->SetText(FText::FromString(Msg));
	GMatLinkJob.Notification->SetCompletionState(State);
	if (bExpire)
	{
		GMatLinkJob.Notification->ExpireAndFadeout();
	}
}

static bool HCI_TickMatLinkJob(float)
{
	if (!GMatLinkJob.IsActive())
	{
		GMatLinkJob.Reset();
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	auto DuplicateOne = [&AssetToolsModule, &AssetRegistry](const FHCIMatLinkDupTask& Task, FString* OutNewObjectPath) -> bool
	{
		const FAssetData Src = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Task.SourceObjectPath));
		UObject* SourceObj = Src.GetAsset();
		if (!SourceObj)
		{
			return false;
		}
		UObject* NewObj = AssetToolsModule.Get().DuplicateAsset(Task.DestName, Task.DestDir, SourceObj);
		if (!NewObj)
		{
			return false;
		}
		if (OutNewObjectPath)
		{
			*OutNewObjectPath = NewObj->GetPathName();
		}
		return true;
	};

	switch (GMatLinkJob.Stage)
	{
	case EHCIMatLinkJobStage::Build_Duplicate:
	case EHCIMatLinkJobStage::Reset_Duplicate:
	{
		if (GMatLinkJob.TaskIndex >= GMatLinkJob.Tasks.Num())
		{
			GMatLinkJob.Stage = (GMatLinkJob.Kind == EHCIMatLinkJobKind::BuildSnapshot) ? EHCIMatLinkJobStage::Build_Save : EHCIMatLinkJobStage::Reset_BreakMeshMaterials;
			return true;
		}

		const FHCIMatLinkDupTask Task = GMatLinkJob.Tasks[GMatLinkJob.TaskIndex];
		++GMatLinkJob.TaskIndex;

		FString NewPath;
		const bool bOk = DuplicateOne(Task, &NewPath);
		const FString Msg = FString::Printf(TEXT("MatLink：复制中 %d/%d (%s)"), GMatLinkJob.TaskIndex, GMatLinkJob.Tasks.Num(), *Task.DebugLabel);
		HCI_UpdateMatLinkNotification(Msg, SNotificationItem::CS_Pending, false);

		if (bOk && !NewPath.IsEmpty())
		{
			// Record key outputs based on stable dest names.
			if (Task.DestName.StartsWith(TEXT("SM_"), ESearchCase::IgnoreCase))
			{
				GMatLinkJob.CreatedMeshObjectPaths.Add(NewPath);
			}
			if (GMatLinkJob.Kind == EHCIMatLinkJobKind::BuildSnapshot)
			{
				if (Task.DestName == TEXT("SM_MatLink_Base"))
				{
					GMatLinkJob.SnapshotMeshObjectPath = NewPath;
				}
				else if (Task.DestName == TEXT("M_MatLink_Master"))
				{
					GMatLinkJob.SnapshotMasterMaterialObjectPath = NewPath;
				}
				else if (Task.DestName == TEXT("T_MatLink_Base_BC"))
				{
					GMatLinkJob.SnapshotTexturesByRole.Add(TEXT("BC"), NewPath);
				}
				else if (Task.DestName == TEXT("T_MatLink_Base_N"))
				{
					GMatLinkJob.SnapshotTexturesByRole.Add(TEXT("N"), NewPath);
				}
				else if (Task.DestName == TEXT("T_MatLink_Base_ORM"))
				{
					GMatLinkJob.SnapshotTexturesByRole.Add(TEXT("ORM"), NewPath);
				}
			}
		}
		return true;
	}
	case EHCIMatLinkJobStage::Build_Save:
	{
		UEditorAssetLibrary::SaveDirectory(HCI_MatLinkSnapshotRoot, true, true);
		UEditorAssetLibrary::SaveDirectory(HCI_MatLinkMastersRoot, true, true);
		GMatLinkJob.Stage = EHCIMatLinkJobStage::Build_WriteReport;
		return true;
	}
	case EHCIMatLinkJobStage::Build_WriteReport:
	{
		UMaterialInterface* Master = Cast<UMaterialInterface>(LoadObject<UObject>(nullptr, *HCI_ToObjectPathMaybe(GMatLinkJob.SnapshotMasterMaterialObjectPath)));
		HCI_CollectTextureParameterNames(Master, GMatLinkJob.TextureParameterNames);
		HCI_WriteLatestMatLinkFixtureReport(
			GMatLinkJob.SelectedSeedAnchor,
			GMatLinkJob.SelectedSeedMasterMaterial,
			GMatLinkJob.SelectedSeedTexturesByRole,
			GMatLinkJob.GroupHint,
			GMatLinkJob.SnapshotMeshObjectPath,
			GMatLinkJob.SnapshotMasterMaterialObjectPath,
			GMatLinkJob.SnapshotTexturesByRole,
			GMatLinkJob.TextureParameterNames,
			GMatLinkJob.ChaosScenarioRoots);
		HCI_UpdateMatLinkNotification(TEXT("MatLink：BuildSnapshot 完成"), SNotificationItem::CS_Success, true);
		GMatLinkJob.Reset();
		return false;
	}
	case EHCIMatLinkJobStage::Reset_BreakMeshMaterials:
	{
		int32 OkCount = 0;
		for (const FString& MeshPath : GMatLinkJob.CreatedMeshObjectPaths)
		{
			OkCount += HCI_SetStaticMeshDefaultMaterialByObjectPath(MeshPath) ? 1 : 0;
		}
		const FString Msg = FString::Printf(TEXT("MatLink：断线（默认材质）%d/%d"), OkCount, GMatLinkJob.CreatedMeshObjectPaths.Num());
		HCI_UpdateMatLinkNotification(Msg, SNotificationItem::CS_Pending, false);
		GMatLinkJob.Stage = EHCIMatLinkJobStage::Reset_Save;
		return true;
	}
	case EHCIMatLinkJobStage::Reset_Save:
	{
		UEditorAssetLibrary::SaveDirectory(HCI_MatLinkChaosRoot, true, true);
		GMatLinkJob.Stage = EHCIMatLinkJobStage::Reset_WriteReport;
		return true;
	}
	case EHCIMatLinkJobStage::Reset_WriteReport:
	{
		UMaterialInterface* Master = Cast<UMaterialInterface>(LoadObject<UObject>(nullptr, *HCI_ToObjectPathMaybe(GMatLinkJob.SnapshotMasterMaterialObjectPath)));
		HCI_CollectTextureParameterNames(Master, GMatLinkJob.TextureParameterNames);
		HCI_WriteLatestMatLinkFixtureReport(
			GMatLinkJob.SelectedSeedAnchor,
			GMatLinkJob.SelectedSeedMasterMaterial,
			GMatLinkJob.SelectedSeedTexturesByRole,
			GMatLinkJob.GroupHint,
			GMatLinkJob.SnapshotMeshObjectPath,
			GMatLinkJob.SnapshotMasterMaterialObjectPath,
			GMatLinkJob.SnapshotTexturesByRole,
			GMatLinkJob.TextureParameterNames,
			GMatLinkJob.ChaosScenarioRoots);
		HCI_UpdateMatLinkNotification(TEXT("MatLink：Reset 完成（可开始后续连线测试）"), SNotificationItem::CS_Success, true);
		GMatLinkJob.Reset();
		return false;
	}
	default:
		break;
	}

	GMatLinkJob.Reset();
	return false;
}

static void HCI_StartMatLinkJob(const EHCIMatLinkJobKind Kind, const EHCIMatLinkJobStage Stage, TArray<FHCIMatLinkDupTask>&& Tasks)
{
	if (GMatLinkJob.IsActive())
	{
		UE_LOG(LogHCIFixtures, Warning, TEXT("[HCI][Fixtures] matlink_job_busy kind=%d stage=%d"), static_cast<int32>(GMatLinkJob.Kind), static_cast<int32>(GMatLinkJob.Stage));
		return;
	}

	GMatLinkJob.Reset();
	GMatLinkJob.Kind = Kind;
	GMatLinkJob.Stage = Stage;
	GMatLinkJob.Tasks = MoveTemp(Tasks);
	GMatLinkJob.TaskIndex = 0;

	FNotificationInfo Info(FText::FromString(TEXT("MatLink：开始...")));
	Info.bFireAndForget = false;
	Info.bUseLargeFont = false;
	Info.FadeInDuration = 0.1f;
	Info.FadeOutDuration = 0.2f;
	Info.ExpireDuration = 1.0f;
	Info.bUseThrobber = true;
	GMatLinkJob.Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (GMatLinkJob.Notification.IsValid())
	{
		GMatLinkJob.Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}

	GMatLinkJob.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&HCI_TickMatLinkJob), 0.0f);
}

static void HCI_RunMatLinkBuildSnapshotCommand(const TArray<FString>& Args)
{
	if (!UEditorAssetLibrary::DoesDirectoryExist(HCI_SeedRoot))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] seed_missing dir=%s"), HCI_SeedRoot);
		return;
	}

	// Recreate snapshot + masters directory (safe: under __HCI_Test only).
	if (UEditorAssetLibrary::DoesDirectoryExist(HCI_MatLinkSnapshotRoot))
	{
		if (!HCI_DeleteDirectoryWithConfirm(HCI_MatLinkSnapshotRoot, TEXT("重建 MatLinkSnapshot（材质连线夹具快照）")))
		{
			return;
		}
	}
	if (UEditorAssetLibrary::DoesDirectoryExist(HCI_MatLinkMastersRoot))
	{
		if (!HCI_DeleteDirectoryWithConfirm(HCI_MatLinkMastersRoot, TEXT("重建 MatLink Masters（测试 Master 材质）")))
		{
			return;
		}
	}

	UEditorAssetLibrary::MakeDirectory(HCI_MatLinkSnapshotRoot);
	UEditorAssetLibrary::MakeDirectory(HCI_MatLinkMastersRoot);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AnchorMesh;
	FAssetData MasterMaterial;
	TMap<EHCIMatLinkRole, FAssetData> RoleTextures;
	FString GroupHint;
	if (!HCI_SelectBestSeedPbrGroup(AssetRegistry, AnchorMesh, MasterMaterial, RoleTextures, GroupHint))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] matlink_select_failed seed=%s"), HCI_SeedRoot);
		return;
	}

	const FString SelectedSeedAnchor = HCI_AssetDataToObjectPathString(AnchorMesh);
	const FString SelectedSeedMaster = HCI_AssetDataToObjectPathString(MasterMaterial);

	TMap<FString, FString> SeedTexturesByRole;
	for (const TPair<EHCIMatLinkRole, FAssetData>& It : RoleTextures)
	{
		SeedTexturesByRole.Add(HCI_MatLinkRoleToString(It.Key), HCI_AssetDataToObjectPathString(It.Value));
	}

	TArray<FHCIMatLinkDupTask> Tasks;
	Tasks.Reserve(5);

	Tasks.Add({SelectedSeedAnchor, HCI_MatLinkSnapshotRoot, TEXT("SM_MatLink_Base"), TEXT("snapshot_mesh")});
	if (RoleTextures.Contains(EHCIMatLinkRole::BC))
	{
		Tasks.Add({HCI_AssetDataToObjectPathString(RoleTextures[EHCIMatLinkRole::BC]), HCI_MatLinkSnapshotRoot, TEXT("T_MatLink_Base_BC"), TEXT("snapshot_tex_bc")});
	}
	if (RoleTextures.Contains(EHCIMatLinkRole::N))
	{
		Tasks.Add({HCI_AssetDataToObjectPathString(RoleTextures[EHCIMatLinkRole::N]), HCI_MatLinkSnapshotRoot, TEXT("T_MatLink_Base_N"), TEXT("snapshot_tex_n")});
	}
	if (RoleTextures.Contains(EHCIMatLinkRole::ORM))
	{
		Tasks.Add({HCI_AssetDataToObjectPathString(RoleTextures[EHCIMatLinkRole::ORM]), HCI_MatLinkSnapshotRoot, TEXT("T_MatLink_Base_ORM"), TEXT("snapshot_tex_orm")});
	}
	Tasks.Add({SelectedSeedMaster, HCI_MatLinkMastersRoot, TEXT("M_MatLink_Master"), TEXT("snapshot_master")});

	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] matlink_build_snapshot select anchor=%s master=%s group_hint=%s roles=%d"),
		*SelectedSeedAnchor, *SelectedSeedMaster, *GroupHint, SeedTexturesByRole.Num());

	HCI_StartMatLinkJob(EHCIMatLinkJobKind::BuildSnapshot, EHCIMatLinkJobStage::Build_Duplicate, MoveTemp(Tasks));
	// StartMatLinkJob() resets state; assign seed selection after start.
	GMatLinkJob.SelectedSeedAnchor = SelectedSeedAnchor;
	GMatLinkJob.SelectedSeedMasterMaterial = SelectedSeedMaster;
	GMatLinkJob.SelectedSeedTexturesByRole = SeedTexturesByRole;
	GMatLinkJob.GroupHint = GroupHint;
}

enum class EHCIMatLinkResetPreset : uint8
{
	FailOrphansNonContract = 0,
	FuzzyOkNoOrphans = 1
};

static void HCI_RunMatLinkResetImpl(const TArray<FString>& Args, const EHCIMatLinkResetPreset Preset)
{
	if (!UEditorAssetLibrary::DoesDirectoryExist(HCI_MatLinkSnapshotRoot) || !UEditorAssetLibrary::DoesDirectoryExist(HCI_MatLinkMastersRoot))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] matlink_snapshot_missing (run HCI.MatLinkBuildSnapshot first) snapshot=%s masters=%s"),
			HCI_MatLinkSnapshotRoot, HCI_MatLinkMastersRoot);
		return;
	}

	// Delete MatLink test dirs, and also clean up legacy root from older workspaces.
	if (UEditorAssetLibrary::DoesDirectoryExist(TEXT("/Game/_HCI_Test")))
	{
		if (!HCI_DeleteDirectoryWithConfirm(TEXT("/Game/_HCI_Test"), TEXT("清理 legacy 测试根目录（/Game/_HCI_Test，已弃用）")))
		{
			return;
		}
	}

	if (!HCI_DeleteDirectoryWithConfirm(HCI_MatLinkChaosRoot, TEXT("重置 MatLink 混乱输入目录（MatLinkChaos）")))
	{
		return;
	}
	if (!HCI_DeleteDirectoryWithConfirm(HCI_MatLinkCleanRoot, TEXT("重置 MatLink 输出目录（MatLinkClean）")))
	{
		return;
	}

	UEditorAssetLibrary::MakeDirectory(HCI_MatLinkChaosRoot);
	UEditorAssetLibrary::MakeDirectory(HCI_MatLinkCleanRoot);

	// Recover seed selection + group hint from the last snapshot report (helps after editor restart).
	FHCIMatLinkJobState Loaded;
	HCI_TryLoadLatestMatLinkFixtureReport(Loaded);

	// Locate snapshot sources by stable names (asset paths first, then build object paths).
	const FString SnapshotMeshAssetPath = FString::Printf(TEXT("%s/SM_MatLink_Base"), HCI_MatLinkSnapshotRoot);
	const FString SnapshotBCAssetPath = FString::Printf(TEXT("%s/T_MatLink_Base_BC"), HCI_MatLinkSnapshotRoot);
	const FString SnapshotNAssetPath = FString::Printf(TEXT("%s/T_MatLink_Base_N"), HCI_MatLinkSnapshotRoot);
	const FString SnapshotORMAssetPath = FString::Printf(TEXT("%s/T_MatLink_Base_ORM"), HCI_MatLinkSnapshotRoot);
	const FString SnapshotMasterAssetPath = FString::Printf(TEXT("%s/M_MatLink_Master"), HCI_MatLinkMastersRoot);

	if (!UEditorAssetLibrary::DoesAssetExist(SnapshotMeshAssetPath) || !UEditorAssetLibrary::DoesAssetExist(SnapshotMasterAssetPath))
	{
		UE_LOG(LogHCIFixtures, Error, TEXT("[HCI][Fixtures] matlink_snapshot_assets_missing mesh=%s master=%s"), *SnapshotMeshAssetPath, *SnapshotMasterAssetPath);
		return;
	}

	const FString SnapshotMesh = HCI_ToObjectPathMaybe(SnapshotMeshAssetPath);
	const FString SnapshotBC = HCI_ToObjectPathMaybe(SnapshotBCAssetPath);
	const FString SnapshotN = HCI_ToObjectPathMaybe(SnapshotNAssetPath);
	const FString SnapshotORM = HCI_ToObjectPathMaybe(SnapshotORMAssetPath);
	const FString SnapshotMaster = HCI_ToObjectPathMaybe(SnapshotMasterAssetPath);

	TMap<FString, FString> SnapshotTexturesByRole;
	if (UEditorAssetLibrary::DoesAssetExist(SnapshotBCAssetPath))
	{
		SnapshotTexturesByRole.Add(TEXT("BC"), SnapshotBC);
	}
	if (UEditorAssetLibrary::DoesAssetExist(SnapshotNAssetPath))
	{
		SnapshotTexturesByRole.Add(TEXT("N"), SnapshotN);
	}
	if (UEditorAssetLibrary::DoesAssetExist(SnapshotORMAssetPath))
	{
		SnapshotTexturesByRole.Add(TEXT("ORM"), SnapshotORM);
	}

	// Scenario roots.
	const FString S1Root = FString::Printf(TEXT("%s/S1_PerfectContract"), HCI_MatLinkChaosRoot);
	const FString S2Root = FString::Printf(TEXT("%s/S2_Fuzzy"), HCI_MatLinkChaosRoot);
	const FString S3Root = FString::Printf(TEXT("%s/S3_None"), HCI_MatLinkChaosRoot);
	const TArray<FString> ScenarioRoots = {S1Root, S2Root, S3Root};

	UEditorAssetLibrary::MakeDirectory(S1Root);
	UEditorAssetLibrary::MakeDirectory(S2Root);
	UEditorAssetLibrary::MakeDirectory(S3Root);

	const FString GroupHint = Loaded.GroupHint.IsEmpty() ? TEXT("Barrel") : Loaded.GroupHint;

	TArray<FHCIMatLinkDupTask> Tasks;
	Tasks.Reserve(16);

	// S1: strict contract.
	UEditorAssetLibrary::MakeDirectory(S1Root + TEXT("/Textures"));
	Tasks.Add({SnapshotMesh, S1Root, TEXT("SM_A"), TEXT("S1_mesh")});
	if (SnapshotTexturesByRole.Contains(TEXT("BC")))
	{
		Tasks.Add({SnapshotBC, S1Root + TEXT("/Textures"), TEXT("T_A_BC"), TEXT("S1_bc")});
	}
	if (SnapshotTexturesByRole.Contains(TEXT("N")))
	{
		Tasks.Add({SnapshotN, S1Root + TEXT("/Textures"), TEXT("T_A_N"), TEXT("S1_n")});
	}
	if (SnapshotTexturesByRole.Contains(TEXT("ORM")))
	{
		Tasks.Add({SnapshotORM, S1Root + TEXT("/Textures"), TEXT("T_A_ORM"), TEXT("S1_orm")});
	}

	// S2: fuzzy, scattered & non-contract suffixes.
	UEditorAssetLibrary::MakeDirectory(S2Root + TEXT("/Whatever/Meshes"));
	UEditorAssetLibrary::MakeDirectory(S2Root + TEXT("/TexturesLoose"));
	UEditorAssetLibrary::MakeDirectory(S2Root + TEXT("/tmp"));
	if (Preset == EHCIMatLinkResetPreset::FuzzyOkNoOrphans)
	{
		// Still "fuzzy" in directory layout, but strictly contract-compliant so AutoMaterialSetupByNameContract can pass.
		const FString Id = FString::Printf(TEXT("Old_%s_Variant"), *GroupHint);
		Tasks.Add({SnapshotMesh, S2Root + TEXT("/Whatever/Meshes"), *FString::Printf(TEXT("SM_%s"), *Id), TEXT("S2_mesh_ok")});
		if (SnapshotTexturesByRole.Contains(TEXT("BC")))
		{
			Tasks.Add({SnapshotBC, S2Root + TEXT("/TexturesLoose"), *FString::Printf(TEXT("T_%s_BC"), *Id), TEXT("S2_bc_ok")});
		}
		if (SnapshotTexturesByRole.Contains(TEXT("N")))
		{
			Tasks.Add({SnapshotN, S2Root + TEXT("/tmp"), *FString::Printf(TEXT("T_%s_N"), *Id), TEXT("S2_n_ok")});
		}
		if (SnapshotTexturesByRole.Contains(TEXT("ORM")))
		{
			Tasks.Add({SnapshotORM, S2Root + TEXT("/TexturesLoose"), *FString::Printf(TEXT("T_%s_ORM"), *Id), TEXT("S2_orm_ok")});
		}
	}
	else
	{
		Tasks.Add({SnapshotMesh, S2Root + TEXT("/Whatever/Meshes"), *FString::Printf(TEXT("SM_Old_%s"), *GroupHint), TEXT("S2_mesh")});
		if (SnapshotTexturesByRole.Contains(TEXT("BC")))
		{
			Tasks.Add({SnapshotBC, S2Root + TEXT("/TexturesLoose"), *FString::Printf(TEXT("T_%s_New_Color"), *GroupHint), TEXT("S2_bc")});
			// Extra BC-like candidate to force fuzzy matching ambiguity (non-contract suffix).
			Tasks.Add({SnapshotBC, S2Root + TEXT("/tmp"), *FString::Printf(TEXT("T_%s_Color"), *GroupHint), TEXT("S2_bc_2")});
		}
		if (SnapshotTexturesByRole.Contains(TEXT("N")))
		{
			Tasks.Add({SnapshotN, S2Root + TEXT("/tmp"), *FString::Printf(TEXT("T_%s_NormalMap"), *GroupHint), TEXT("S2_n")});
			// Extra N-like candidate to force fuzzy matching ambiguity (non-contract suffix).
			Tasks.Add({SnapshotN, S2Root + TEXT("/TexturesLoose"), *FString::Printf(TEXT("T_%s_Normal"), *GroupHint), TEXT("S2_n_2")});
		}
		if (SnapshotTexturesByRole.Contains(TEXT("ORM")))
		{
			Tasks.Add({SnapshotORM, S2Root + TEXT("/tmp"), *FString::Printf(TEXT("T_%s_RMA"), *GroupHint), TEXT("S2_orm")});
			// Extra ORM-like candidate to force fuzzy matching ambiguity (non-contract suffix).
			Tasks.Add({SnapshotORM, S2Root + TEXT("/TexturesLoose"), *FString::Printf(TEXT("T_%s_ORMPacked"), *GroupHint), TEXT("S2_orm_2")});
		}
		// One obvious orphan (unrelated to any group hint) to test "orphan_assets" locate UX.
		if (SnapshotTexturesByRole.Contains(TEXT("BC")))
		{
			Tasks.Add({SnapshotBC, S2Root + TEXT("/tmp"), TEXT("T_Orphan_Trash"), TEXT("S2_orphan_trash")});
		}
	}

	// S3: no relation.
	UEditorAssetLibrary::MakeDirectory(S3Root + TEXT("/Unsorted"));
	Tasks.Add({SnapshotMesh, S3Root + TEXT("/Unsorted"), TEXT("SM_1"), TEXT("S3_mesh")});
	if (SnapshotTexturesByRole.Contains(TEXT("BC")))
	{
		Tasks.Add({SnapshotBC, S3Root + TEXT("/Unsorted"), TEXT("T_Unknown"), TEXT("S3_unknown")});
	}

	UE_LOG(LogHCIFixtures, Display, TEXT("[HCI][Fixtures] matlink_reset started chaos=%s clean=%s scenarios=3 group_hint=%s"),
		HCI_MatLinkChaosRoot, HCI_MatLinkCleanRoot, *GroupHint);

	HCI_StartMatLinkJob(EHCIMatLinkJobKind::Reset, EHCIMatLinkJobStage::Reset_Duplicate, MoveTemp(Tasks));
	// StartMatLinkJob() resets state; assign snapshot + scenario info after start.
	GMatLinkJob.SelectedSeedAnchor = Loaded.SelectedSeedAnchor;
	GMatLinkJob.SelectedSeedMasterMaterial = Loaded.SelectedSeedMasterMaterial;
	GMatLinkJob.SelectedSeedTexturesByRole = Loaded.SelectedSeedTexturesByRole;
	GMatLinkJob.GroupHint = GroupHint;
	GMatLinkJob.SnapshotMeshObjectPath = SnapshotMesh;
	GMatLinkJob.SnapshotMasterMaterialObjectPath = SnapshotMaster;
	GMatLinkJob.SnapshotTexturesByRole = SnapshotTexturesByRole;
	GMatLinkJob.ChaosScenarioRoots = ScenarioRoots;
}

static void HCI_RunMatLinkResetCommand(const TArray<FString>& Args)
{
	HCI_RunMatLinkResetImpl(Args, EHCIMatLinkResetPreset::FailOrphansNonContract);
}

static void HCI_RunMatLinkResetFuzzyOkCommand(const TArray<FString>& Args)
{
	HCI_RunMatLinkResetImpl(Args, EHCIMatLinkResetPreset::FuzzyOkNoOrphans);
}

void FHCIAgentDemoConsoleCommands::StartupFixtureCommands()
{
	if (!SeedChaosBuildSnapshotCommand.IsValid())
	{
		SeedChaosBuildSnapshotCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.SeedChaosBuildSnapshot"),
			TEXT("Stage N fixtures: build snapshot under /Game/__HCI_Test/Fixtures from /Game/Seed (read-only seed). Usage: HCI.SeedChaosBuildSnapshot"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunSeedChaosBuildSnapshotCommand));
	}

	if (!SeedChaosResetCommand.IsValid())
	{
		SeedChaosResetCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.SeedChaosReset"),
			TEXT("Stage N fixtures: reset incoming chaos + organized clean (deletes /Game/__HCI_Test/Incoming/SeedChaos and /Game/__HCI_Test/Organized/SeedClean). Usage: HCI.SeedChaosReset"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunSeedChaosResetCommand));
	}

	if (!MatLinkBuildSnapshotCommand.IsValid())
	{
		MatLinkBuildSnapshotCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.MatLinkBuildSnapshot"),
			TEXT("Stage O fixtures: build MatLink snapshot + master material under /Game/__HCI_Test from /Game/Seed (read-only seed). Usage: HCI.MatLinkBuildSnapshot"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunMatLinkBuildSnapshotCommand));
	}

	if (!MatLinkResetCommand.IsValid())
	{
		MatLinkResetCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.MatLinkReset"),
			TEXT("Stage O fixtures: reset MatLink chaos + clean (deletes /Game/__HCI_Test/Incoming/MatLinkChaos and /Game/__HCI_Test/Organized/MatLinkClean; also cleans legacy /Game/_HCI_Test if present). Usage: HCI.MatLinkReset"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunMatLinkResetCommand));
	}

	if (!MatLinkResetFuzzyOkCommand.IsValid())
	{
		MatLinkResetFuzzyOkCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCI.MatLinkResetFuzzyOk"),
			TEXT("Stage O fixtures: reset MatLink chaos + clean, with S2_Fuzzy being contract-compliant (no orphans) but still directory-scattered. Usage: HCI.MatLinkResetFuzzyOk"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunMatLinkResetFuzzyOkCommand));
	}
}

void FHCIAgentDemoConsoleCommands::ShutdownFixtureCommands()
{
	SeedChaosResetCommand.Reset();
	SeedChaosBuildSnapshotCommand.Reset();
	MatLinkResetCommand.Reset();
	MatLinkResetFuzzyOkCommand.Reset();
	MatLinkBuildSnapshotCommand.Reset();
}

