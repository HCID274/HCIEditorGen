#include "Commands/HCIAbilityKitAgentDemoConsoleCommands.h"

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
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectRedirector.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitFixtures, Log, All);

namespace
{
static const TCHAR* HCI_SeedRoot = TEXT("/Game/Seed");
static const TCHAR* HCI_SnapshotRoot = TEXT("/Game/__HCI_Test/Fixtures/SeedChaosSnapshot");
static const TCHAR* HCI_IncomingRoot = TEXT("/Game/__HCI_Test/Incoming/SeedChaos");
static const TCHAR* HCI_OrganizedRoot = TEXT("/Game/__HCI_Test/Organized/SeedClean");

static bool HCI_IsAllowedTestRoot(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game/__HCI_Test/"), ESearchCase::CaseSensitive)
		|| Path.StartsWith(TEXT("/Game/__HCI_Test"), ESearchCase::CaseSensitive);
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
		UE_LOG(LogHCIAbilityKitFixtures, Error, TEXT("[HCIAbilityKit][Fixtures] reject_delete not_test_root dir=%s"), *Directory);
		return false;
	}

	if (!UEditorAssetLibrary::DoesDirectoryExist(Directory))
	{
		return true;
	}

	const FString ConfirmText = FString::Printf(
		TEXT("将删除测试目录（仅限 __HCI_Test）：\n\n%s\n\n目录：%s\n\n继续？"),
		*HumanLabel,
		*Directory);
	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmText));
	if (Choice != EAppReturnType::Yes)
	{
		UE_LOG(LogHCIAbilityKitFixtures, Warning, TEXT("[HCIAbilityKit][Fixtures] delete_cancelled dir=%s"), *Directory);
		return false;
	}

	const bool bOk = UEditorAssetLibrary::DeleteDirectory(Directory);
	UE_LOG(LogHCIAbilityKitFixtures, Display, TEXT("[HCIAbilityKit][Fixtures] delete_dir ok=%s dir=%s"), bOk ? TEXT("true") : TEXT("false"), *Directory);
	return bOk;
}

static void HCI_WriteLatestFixtureReport(const TArray<FString>& CopiedAssets, const TArray<FString>& DirtyMoves, const TArray<FString>& DirtyRenames)
{
	const FString OutDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit/TestFixtures/SeedChaos"));
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
	UE_LOG(LogHCIAbilityKitFixtures, Display, TEXT("[HCIAbilityKit][Fixtures] wrote_report path=%s bytes=%d"), *OutPath, JsonText.Len());
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
		UE_LOG(LogHCIAbilityKitFixtures, Warning, TEXT("[HCIAbilityKit][Fixtures] job_busy kind=%d stage=%d"), static_cast<int32>(GSeedChaosJob.Kind), static_cast<int32>(GSeedChaosJob.Stage));
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
		UE_LOG(LogHCIAbilityKitFixtures, Error, TEXT("[HCIAbilityKit][Fixtures] seed_missing dir=%s"), HCI_SeedRoot);
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
		UE_LOG(LogHCIAbilityKitFixtures, Error, TEXT("[HCIAbilityKit][Fixtures] seed_empty dir=%s"), HCI_SeedRoot);
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
	UE_LOG(LogHCIAbilityKitFixtures, Display, TEXT("[HCIAbilityKit][Fixtures] build_snapshot started selected=%d snapshot=%s"), GSeedChaosJob.Packages.Num(), HCI_SnapshotRoot);
}

static void HCI_RunSeedChaosResetCommand(const TArray<FString>& Args)
{
	constexpr int32 MaxAssets = 50;

	if (!UEditorAssetLibrary::DoesDirectoryExist(HCI_SnapshotRoot))
	{
		UE_LOG(LogHCIAbilityKitFixtures, Error, TEXT("[HCIAbilityKit][Fixtures] snapshot_missing dir=%s (run HCIAbilityKit.SeedChaosBuildSnapshot first)"), HCI_SnapshotRoot);
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
		UE_LOG(LogHCIAbilityKitFixtures, Error, TEXT("[HCIAbilityKit][Fixtures] snapshot_empty dir=%s"), HCI_SnapshotRoot);
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
	UE_LOG(LogHCIAbilityKitFixtures, Display, TEXT("[HCIAbilityKit][Fixtures] reset started snapshot_pkgs=%d incoming=%s organized=%s"), GSeedChaosJob.Packages.Num(), HCI_IncomingRoot, HCI_OrganizedRoot);
}

void FHCIAbilityKitAgentDemoConsoleCommands::StartupFixtureCommands()
{
	if (!SeedChaosBuildSnapshotCommand.IsValid())
	{
		SeedChaosBuildSnapshotCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.SeedChaosBuildSnapshot"),
			TEXT("Stage N fixtures: build snapshot under /Game/__HCI_Test/Fixtures from /Game/Seed (read-only seed). Usage: HCIAbilityKit.SeedChaosBuildSnapshot"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunSeedChaosBuildSnapshotCommand));
	}

	if (!SeedChaosResetCommand.IsValid())
	{
		SeedChaosResetCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.SeedChaosReset"),
			TEXT("Stage N fixtures: reset incoming chaos + organized clean (deletes /Game/__HCI_Test/Incoming/SeedChaos and /Game/__HCI_Test/Organized/SeedClean). Usage: HCIAbilityKit.SeedChaosReset"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunSeedChaosResetCommand));
	}
}

void FHCIAbilityKitAgentDemoConsoleCommands::ShutdownFixtureCommands()
{
	SeedChaosResetCommand.Reset();
	SeedChaosBuildSnapshotCommand.Reset();
}
