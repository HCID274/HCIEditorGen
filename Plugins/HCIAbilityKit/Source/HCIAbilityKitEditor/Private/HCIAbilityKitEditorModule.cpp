#include "HCIAbilityKitEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "Dom/JsonObject.h"
#include "Audit/HCIAbilityKitAuditScanService.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HCIAbilityKitAsset.h"
#include "IPythonScriptPlugin.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Search/HCIAbilityKitSearchIndexService.h"
#include "Search/HCIAbilityKitSearchQueryService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Services/HCIAbilityKitParserService.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitSearchQuery, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAuditScan, Log, All);

static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitSearchCommand;
static TUniquePtr<FAutoConsoleCommand> GHCIAbilityKitAuditScanCommand;
static const TCHAR* GHCIAbilityKitPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py");

static FString HCI_ToPythonStringLiteral(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("'"), TEXT("\\'"));
	return FString::Printf(TEXT("'%s'"), *Escaped);
}

static FString HCI_GetJsonStringOrDefault(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& FieldName,
	const FString& DefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	FString Parsed;
	return JsonObject->TryGetStringField(FieldName, Parsed) ? Parsed : DefaultValue;
}

static void HCI_DeleteTempFile(const FString& Filename)
{
	if (!Filename.IsEmpty())
	{
		IFileManager::Get().Delete(*Filename, false, true, true);
	}
}

static void HCI_RunAbilityKitSearchCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(
			LogHCIAbilityKitSearchQuery,
			Warning,
			TEXT("[HCIAbilityKit][SearchQuery] usage: HCIAbilityKit.Search <query text> [topk]"));
		return;
	}

	int32 TopK = 5;
	int32 QueryTokenCount = Args.Num();
	if (Args.Num() >= 2)
	{
		int32 ParsedTopK = 0;
		if (LexTryParseString(ParsedTopK, *Args.Last()))
		{
			TopK = ParsedTopK;
			QueryTokenCount = Args.Num() - 1;
		}
	}

	TArray<FString> QueryTokens;
	QueryTokens.Reserve(QueryTokenCount);
	for (int32 Index = 0; Index < QueryTokenCount; ++Index)
	{
		QueryTokens.Add(Args[Index]);
	}
	const FString QueryText = FString::Join(QueryTokens, TEXT(" "));
	const FHCIAbilitySearchResult QueryResult = FHCIAbilityKitSearchQueryService::RunQuery(
		QueryText,
		TopK,
		FHCIAbilityKitSearchIndexService::Get().GetIndex());

	UE_LOG(
		LogHCIAbilityKitSearchQuery,
		Display,
		TEXT("[HCIAbilityKit][SearchQuery] raw=\"%s\" parsed=%s"),
		*QueryText,
		*QueryResult.ParsedQuery.ToSummaryString());

	UE_LOG(
		LogHCIAbilityKitSearchQuery,
		Display,
		TEXT("[HCIAbilityKit][SearchQuery] %s"),
		*QueryResult.ToSummaryString());

	if (QueryResult.Candidates.Num() > 0)
	{
		for (const FHCIAbilitySearchCandidate& Candidate : QueryResult.Candidates)
		{
			UE_LOG(
				LogHCIAbilityKitSearchQuery,
				Display,
				TEXT("[HCIAbilityKit][SearchQuery] hit id=%s score=%.2f"),
				*Candidate.Id,
				Candidate.Score);
		}
		return;
	}

	for (const FString& Suggestion : QueryResult.Suggestions)
	{
		UE_LOG(
			LogHCIAbilityKitSearchQuery,
			Warning,
			TEXT("[HCIAbilityKit][SearchQuery] suggestion=%s"),
			*Suggestion);
	}
}

static void HCI_RunAbilityKitAuditScanCommand(const TArray<FString>& Args)
{
	int32 LogTopN = 10;
	if (Args.Num() >= 1)
	{
		int32 ParsedTopN = 0;
		if (LexTryParseString(ParsedTopN, *Args[0]))
		{
			LogTopN = FMath::Max(0, ParsedTopN);
		}
	}

	const FHCIAbilityKitAuditScanSnapshot Snapshot = FHCIAbilityKitAuditScanService::Get().ScanFromAssetRegistry();
	UE_LOG(
		LogHCIAbilityKitAuditScan,
		Display,
		TEXT("[HCIAbilityKit][AuditScan] %s"),
		*Snapshot.Stats.ToSummaryString());

	if (Snapshot.Rows.Num() == 0)
	{
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Warning,
			TEXT("[HCIAbilityKit][AuditScan] suggestion=当前未发现 AbilityKit 资产，请先导入至少一个 .hciabilitykit 文件"));
		return;
	}

	const int32 CountToLog = FMath::Min(LogTopN, Snapshot.Rows.Num());
	for (int32 Index = 0; Index < CountToLog; ++Index)
	{
		const FHCIAbilityKitAuditAssetRow& Row = Snapshot.Rows[Index];
		UE_LOG(
			LogHCIAbilityKitAuditScan,
			Display,
			TEXT("[HCIAbilityKit][AuditScan] row=%d asset=%s id=%s display_name=%s damage=%.2f representing_mesh=%s"),
			Index,
			*Row.AssetPath,
			*Row.Id,
			*Row.DisplayName,
			Row.Damage,
			*Row.RepresentingMeshPath);
	}
}

static bool HCI_ParsePythonResponse(
	const FString& ResponseFilename,
	const FString& ScriptPath,
	const FString& SourceFilename,
	const FString& PythonCommandResult,
	FHCIAbilityKitParsedData& InOutParsed,
	FHCIAbilityKitParseError& OutError)
{
	IFileManager& FileManager = IFileManager::Get();
	constexpr int32 MaxWaitAttempts = 50;
	for (int32 Attempt = 0; Attempt < MaxWaitAttempts && !FileManager.FileExists(*ResponseFilename); ++Attempt)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	FString ResponseContent;
	if (!FFileHelper::LoadFileToString(ResponseContent, *ResponseFilename))
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response");
		OutError.Reason = TEXT("Python hook response file is missing");
		OutError.Hint = TEXT("Check Python script output and response path");
		OutError.Detail = PythonCommandResult.IsEmpty()
			? ResponseFilename
			: FString::Printf(TEXT("response_file=%s; command_result=%s"), *ResponseFilename, *PythonCommandResult);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response");
		OutError.Reason = TEXT("Python hook response is not valid JSON");
		OutError.Hint = TEXT("Return valid JSON with {ok, patch, error, audit}");
		OutError.Detail = ResponseContent;
		return false;
	}

	bool bPythonOk = false;
	if (!RootObject->TryGetBoolField(TEXT("ok"), bPythonOk))
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response.ok");
		OutError.Reason = TEXT("Python hook response missing required bool field: ok");
		OutError.Hint = TEXT("Set response.ok to true or false");
		return false;
	}

	if (!bPythonOk)
	{
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (RootObject->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
		{
			OutError.Code = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("code"), TEXT("E3001"));
			OutError.File = SourceFilename;
			OutError.Field = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("field"), TEXT("python_hook"));
			OutError.Reason = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("reason"), TEXT("Python hook returned failure"));
			OutError.Hint = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("hint"), TEXT("Inspect Python response error payload"));
			OutError.Detail = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("detail"), FString());
		}
		else
		{
			OutError.Code = TEXT("E3001");
			OutError.File = SourceFilename;
			OutError.Field = TEXT("python_hook");
			OutError.Reason = TEXT("Python hook returned failure without error payload");
			OutError.Hint = TEXT("Return response.error object when ok=false");
			OutError.Detail = ResponseContent;
		}
		return false;
	}

	const TSharedPtr<FJsonObject>* PatchObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("patch"), PatchObject) && PatchObject && PatchObject->IsValid())
	{
		FString DisplayNamePatch;
		if ((*PatchObject)->TryGetStringField(TEXT("display_name"), DisplayNamePatch))
		{
			InOutParsed.DisplayName = DisplayNamePatch;
		}

		FString RepresentingMeshPatch;
		if ((*PatchObject)->TryGetStringField(TEXT("representing_mesh"), RepresentingMeshPatch))
		{
			InOutParsed.RepresentingMeshPath = RepresentingMeshPatch;
		}
	}

	return true;
}

static bool HCI_RunPythonHook(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
{
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GHCIAbilityKitPythonScriptPath);
	if (!FPaths::FileExists(ScriptPath))
	{
		OutError.Code = TEXT("E3002");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_hook");
		OutError.Reason = TEXT("Python hook script not found");
		OutError.Hint = TEXT("Restore script file in project directory");
		return false;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		OutError.Code = TEXT("E3003");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_plugin");
		OutError.Reason = TEXT("PythonScriptPlugin is unavailable");
		OutError.Hint = TEXT("Enable PythonScriptPlugin in project settings");
		return false;
	}

	const FString AbsoluteSourceFilename = FPaths::ConvertRelativePathToFull(SourceFilename);
	const FString ResponseDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("HCIAbilityKit"), TEXT("Python")));
	IFileManager::Get().MakeDirectory(*ResponseDir, true);
	const FString ResponseFilename = FPaths::Combine(
		ResponseDir,
		FString::Printf(TEXT("hook_response_%s.json"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

	const FString PythonStatement = FString::Printf(
		TEXT("import runpy, sys; sys.argv=[%s, %s, %s]; runpy.run_path(%s, run_name='__main__')"),
		*HCI_ToPythonStringLiteral(ScriptPath),
		*HCI_ToPythonStringLiteral(AbsoluteSourceFilename),
		*HCI_ToPythonStringLiteral(ResponseFilename),
		*HCI_ToPythonStringLiteral(ScriptPath));

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
	Command.Command = PythonStatement;

	const bool bExecOk = PythonPlugin->ExecPythonCommandEx(Command);
	if (!bExecOk)
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_execution");
		OutError.Reason = Command.CommandResult.IsEmpty() ? TEXT("Python hook execution failed") : Command.CommandResult;
		OutError.Hint = TEXT("Check Python script for syntax errors or runtime exceptions");
		HCI_DeleteTempFile(ResponseFilename);
		return false;
	}

	const bool bParseOk = HCI_ParsePythonResponse(
		ResponseFilename,
		ScriptPath,
		AbsoluteSourceFilename,
		Command.CommandResult,
		InOutParsed,
		OutError);
	HCI_DeleteTempFile(ResponseFilename);
	return bParseOk;
}

static bool HCI_CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return false;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		if (!Asset)
		{
			continue;
		}

		TArray<FString> SourceFiles;
		if (GHCIAbilityKitFactory->CanReimport(Asset, SourceFiles))
		{
			return true;
		}
	}

	return false;
}

static void HCI_ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		GHCIAbilityKitFactory->Reimport(Asset);
	}
}

void FHCIAbilityKitEditorModule::StartupModule()
{
	FHCIAbilityKitSearchIndexService::Get().RebuildFromAssetRegistry();

	FHCIAbilityKitParserService::SetPythonHook(
		[](const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
		{
			return HCI_RunPythonHook(SourceFilename, InOutParsed, OutError);
		});

	GHCIAbilityKitFactory = NewObject<UHCIAbilityKitFactory>(GetTransientPackage());
	GHCIAbilityKitFactory->AddToRoot();

	if (!GHCIAbilityKitSearchCommand.IsValid())
	{
		GHCIAbilityKitSearchCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.Search"),
			TEXT("Search AbilityKit assets by natural language. Usage: HCIAbilityKit.Search <query text> [topk]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitSearchCommand));
	}

	if (!GHCIAbilityKitAuditScanCommand.IsValid())
	{
		GHCIAbilityKitAuditScanCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("HCIAbilityKit.AuditScan"),
			TEXT("Enumerate AbilityKit metadata via AssetRegistry only. Usage: HCIAbilityKit.AuditScan [log_top_n]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&HCI_RunAbilityKitAuditScanCommand));
	}
}

void FHCIAbilityKitEditorModule::ShutdownModule()
{
	FHCIAbilityKitParserService::ClearPythonHook();

	if (GHCIAbilityKitFactory)
	{
		GHCIAbilityKitFactory->RemoveFromRoot();
		GHCIAbilityKitFactory = nullptr;
	}

	GHCIAbilityKitSearchCommand.Reset();
	GHCIAbilityKitAuditScanCommand.Reset();
}

IMPLEMENT_MODULE(FHCIAbilityKitEditorModule, HCIAbilityKitEditor)

static FDelayedAutoRegisterHelper HCIAbilityKitMenuRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAbilityKitAsset::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		Section.AddDynamicEntry("HCIAbilityKit_Reimport", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const TAttribute<FText> Label = NSLOCTEXT("HCIAbilityKit", "Reimport", "Reimport");
			const TAttribute<FText> ToolTip = NSLOCTEXT("HCIAbilityKit", "Reimport_Tooltip", "Reimport from source .hciabilitykit file.");
			const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset");

			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HCI_ReimportSelectedAbilityKits);
			UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&HCI_CanReimportSelectedAbilityKits);
			InSection.AddMenuEntry("HCIAbilityKit_Reimport", Label, ToolTip, Icon, UIAction);
		}));
	}));
});
