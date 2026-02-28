#include "Agent/Presentation/HCIAbilityKitAgentToolResultSummaryFormatter.h"

#include "Agent/Executor/HCIAbilityKitAgentExecutor.h"

namespace HCIAbilityKitAgentToolResultSummaryFormatter
{
static FString HCI_ShortAssetLabel(const FString& InPath)
{
	FString Text = InPath.TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return FString();
	}

	// Strip object suffix: /Game/A/B.Asset -> /Game/A/B
	const int32 DotIndex = Text.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (DotIndex != INDEX_NONE)
	{
		Text = Text.Left(DotIndex);
	}

	int32 LastSlash = INDEX_NONE;
	if (Text.FindLastChar(TEXT('/'), LastSlash) && LastSlash + 1 < Text.Len())
	{
		return Text.Mid(LastSlash + 1);
	}
	return Text;
}

static FString HCI_TruncateLine(const FString& InText, const int32 MaxChars)
{
	if (MaxChars <= 0 || InText.Len() <= MaxChars)
	{
		return InText;
	}
	return InText.Left(MaxChars) + TEXT("…");
}

static int32 HCI_ParseInt(const TMap<FString, FString>& Evidence, const TCHAR* Key, const int32 DefaultValue)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		int32 Parsed = DefaultValue;
		if (LexTryParseString(Parsed, **Value))
		{
			return Parsed;
		}
	}
	return DefaultValue;
}

static FString HCI_GetStr(const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static void HCI_SplitEvidenceList(const FString& InText, const int32 MaxItems, TArray<FString>& OutItems)
{
	OutItems.Reset();
	const FString Trimmed = InText.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("none"), ESearchCase::IgnoreCase) || Trimmed == TEXT("-"))
	{
		return;
	}

	TArray<FString> Tokens;
	Trimmed.ParseIntoArray(Tokens, TEXT("|"), true);
	for (FString& Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}
		OutItems.Add(MoveTemp(Token));
		if (MaxItems > 0 && OutItems.Num() >= MaxItems)
		{
			break;
		}
	}
}

static int32 HCI_CountEvidenceList(const FString& InText)
{
	const FString Trimmed = InText.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Equals(TEXT("none"), ESearchCase::IgnoreCase) || Trimmed == TEXT("-"))
	{
		return 0;
	}

	TArray<FString> Tokens;
	Trimmed.ParseIntoArray(Tokens, TEXT("|"), true);
	int32 Count = 0;
	for (FString& Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (!Token.IsEmpty())
		{
			++Count;
		}
	}
	return Count;
}

static void HCI_AppendIfSpace(TArray<FString>& OutLines, const FString& Line, const FHCIAbilityKitToolResultSummaryOptions& Options)
{
	if (Line.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	if (Options.MaxLines > 0 && OutLines.Num() >= Options.MaxLines)
	{
		return;
	}
	OutLines.Add(HCI_TruncateLine(Line, Options.MaxCharsPerLine));
}

static void HCI_FormatScanMeshTriangleCount(
	const FHCIAbilityKitAgentExecutorStepResult& Step,
	const FHCIAbilityKitToolResultSummaryOptions& Options,
	TArray<FString>& OutLines)
{
	const int32 MeshCount = HCI_ParseInt(Step.Evidence, TEXT("mesh_count"), Step.TargetCountEstimate);
	const int32 MaxTriangles = HCI_ParseInt(Step.Evidence, TEXT("max_triangle_count"), 0);
	const FString MaxAsset = HCI_GetStr(Step.Evidence, TEXT("max_triangle_asset"));
	const FString TopMeshes = HCI_GetStr(Step.Evidence, TEXT("top_meshes"));

	HCI_AppendIfSpace(
		OutLines,
		FString::Printf(TEXT("面数：最大 %s（%s），共 %d 个模型"),
			*FString::FormatAsNumber(FMath::Max(0, MaxTriangles)),
			MaxAsset.IsEmpty() ? TEXT("-") : *HCI_ShortAssetLabel(MaxAsset),
			FMath::Max(0, MeshCount)),
		Options);

	TArray<FString> Items;
	HCI_SplitEvidenceList(TopMeshes, Options.MaxItemsPerList, Items);
	if (Items.Num() > 1)
	{
		for (FString& Item : Items)
		{
			// Keep list compact: /Game/A/B:123 -> B 123
			const int32 ColonIndex = Item.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (ColonIndex != INDEX_NONE)
			{
				const FString Path = Item.Left(ColonIndex);
				const FString Num = Item.Mid(ColonIndex + 1).TrimStartAndEnd();
				Item = FString::Printf(TEXT("%s %s"), *HCI_ShortAssetLabel(Path), *Num);
			}
		}
		HCI_AppendIfSpace(OutLines, FString::Printf(TEXT("Top：%s"), *FString::Join(Items, TEXT(" | "))), Options);
	}
}

static void HCI_FormatScanLevelMeshRisks(
	const FHCIAbilityKitAgentExecutorStepResult& Step,
	const FHCIAbilityKitToolResultSummaryOptions& Options,
	TArray<FString>& OutLines)
{
	const FString Scope = HCI_GetStr(Step.Evidence, TEXT("scope"));
	const int32 Scanned = HCI_ParseInt(Step.Evidence, TEXT("scanned_count"), Step.TargetCountEstimate);
	const int32 Risky = HCI_ParseInt(Step.Evidence, TEXT("risky_count"), 0);
	const int32 Candidate = HCI_ParseInt(Step.Evidence, TEXT("candidate_actor_count"), 0);
	const FString MissingCollisionActors = HCI_GetStr(Step.Evidence, TEXT("missing_collision_actors"));
	const FString DefaultMaterialActors = HCI_GetStr(Step.Evidence, TEXT("default_material_actors"));
	const int32 MissingCollisionCount = HCI_CountEvidenceList(MissingCollisionActors);
	const int32 DefaultMaterialCount = HCI_CountEvidenceList(DefaultMaterialActors);

	FString ScopeLabel = TEXT("-");
	if (Scope.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		ScopeLabel = TEXT("全场景");
	}
	else if (Scope.Equals(TEXT("selected"), ESearchCase::IgnoreCase))
	{
		ScopeLabel = TEXT("选中");
	}

	HCI_AppendIfSpace(
		OutLines,
		FString::Printf(TEXT("关卡风险（%s）：扫描 Actor %d%s，风险 %d（缺碰撞 %d，默认材质 %d）"),
			*ScopeLabel,
			FMath::Max(0, Scanned),
			(Candidate > 0 ? *FString::Printf(TEXT("/%d"), FMath::Max(0, Candidate)) : TEXT("")),
			FMath::Max(0, Risky),
			FMath::Max(0, MissingCollisionCount),
			FMath::Max(0, DefaultMaterialCount)),
		Options);

	if (false && Risky > 0)
	{
		TArray<FString> Items;
		const FString RiskyActors = HCI_GetStr(Step.Evidence, TEXT("risky_actors"));
		HCI_SplitEvidenceList(RiskyActors, 1, Items);
		if (Items.Num() > 0)
		{
			HCI_AppendIfSpace(OutLines, FString::Printf(TEXT("风险Actor示例（仅展示1项，非文件）：%s"), *Items[0]), Options);
		}
	}
}

static void HCI_FormatScanAssets(
	const FHCIAbilityKitAgentExecutorStepResult& Step,
	const FHCIAbilityKitToolResultSummaryOptions& Options,
	TArray<FString>& OutLines)
{
	const FString Root = HCI_GetStr(Step.Evidence, TEXT("scan_root"));
	const int32 AssetCount = HCI_ParseInt(Step.Evidence, TEXT("asset_count"), Step.TargetCountEstimate);
	HCI_AppendIfSpace(
		OutLines,
		FString::Printf(TEXT("资产：扫描 %s，共 %d 个"),
			Root.IsEmpty() ? TEXT("-") : *Root,
			FMath::Max(0, AssetCount)),
		Options);
}

static void HCI_FormatNormalizeAssetNamingByMetadata(
	const FHCIAbilityKitAgentExecutorStepResult& Step,
	const FHCIAbilityKitToolResultSummaryOptions& Options,
	TArray<FString>& OutLines)
{
	const int32 Affected = HCI_ParseInt(Step.Evidence, TEXT("affected_count"), Step.TargetCountEstimate);
	const FString Renames = HCI_GetStr(Step.Evidence, TEXT("proposed_renames"));
	const FString Moves = HCI_GetStr(Step.Evidence, TEXT("proposed_moves"));
	const FString Failed = HCI_GetStr(Step.Evidence, TEXT("failed_assets"));

	const int32 RenameCount = HCI_CountEvidenceList(Renames);
	const int32 MoveCount = HCI_CountEvidenceList(Moves);
	const int32 FailedCount = HCI_CountEvidenceList(Failed);

	HCI_AppendIfSpace(
		OutLines,
		FString::Printf(TEXT("归档预览：影响 %d（重命名 %d，移动 %d，失败 %d）"),
			FMath::Max(0, Affected),
			FMath::Max(0, RenameCount),
			FMath::Max(0, MoveCount),
			FMath::Max(0, FailedCount)),
		Options);

	TArray<FString> Items;
	HCI_SplitEvidenceList(Renames, Options.MaxItemsPerList, Items);
	if (Items.Num() > 0)
	{
		HCI_AppendIfSpace(OutLines, FString::Printf(TEXT("例：重命名 %s"), *FString::Join(Items, TEXT(" | "))), Options);
	}

	HCI_SplitEvidenceList(Moves, Options.MaxItemsPerList, Items);
	if (Items.Num() > 0)
	{
		HCI_AppendIfSpace(OutLines, FString::Printf(TEXT("例：移动 %s"), *FString::Join(Items, TEXT(" | "))), Options);
	}

	HCI_SplitEvidenceList(Failed, Options.MaxItemsPerList, Items);
	if (Items.Num() > 0)
	{
		HCI_AppendIfSpace(OutLines, FString::Printf(TEXT("例：失败 %s"), *FString::Join(Items, TEXT(" | "))), Options);
	}
}

static void HCI_FormatGenericStep(
	const FHCIAbilityKitAgentExecutorStepResult& Step,
	const FHCIAbilityKitToolResultSummaryOptions& Options,
	TArray<FString>& OutLines)
{
	const FString Status = Step.bSucceeded ? TEXT("ok") : TEXT("fail");
	const FString Code = Step.ErrorCode.IsEmpty() ? TEXT("-") : Step.ErrorCode;
	const FString Reason = Step.Reason.IsEmpty() ? TEXT("-") : Step.Reason;
	HCI_AppendIfSpace(
		OutLines,
		FString::Printf(TEXT("结果：%s tool=%s status=%s code=%s reason=%s"),
			Step.StepId.IsEmpty() ? TEXT("-") : *Step.StepId,
			*Step.ToolName,
			*Status,
			*Code,
			*Reason),
		Options);
}

void BuildSummaryLines(
	const TArray<FHCIAbilityKitAgentExecutorStepResult>& StepResults,
	const FHCIAbilityKitToolResultSummaryOptions& Options,
	TArray<FString>& OutLines)
{
	OutLines.Reset();
	if (StepResults.Num() <= 0)
	{
		return;
	}

	for (const FHCIAbilityKitAgentExecutorStepResult& Step : StepResults)
	{
		if (Options.MaxLines > 0 && OutLines.Num() >= Options.MaxLines)
		{
			break;
		}

		if (Step.ToolName == TEXT("ScanMeshTriangleCount"))
		{
			HCI_FormatScanMeshTriangleCount(Step, Options, OutLines);
		}
		else if (Step.ToolName == TEXT("ScanLevelMeshRisks"))
		{
			HCI_FormatScanLevelMeshRisks(Step, Options, OutLines);
		}
		else if (Step.ToolName == TEXT("ScanAssets"))
		{
			HCI_FormatScanAssets(Step, Options, OutLines);
		}
		else if (Step.ToolName == TEXT("NormalizeAssetNamingByMetadata"))
		{
			HCI_FormatNormalizeAssetNamingByMetadata(Step, Options, OutLines);
		}
		else
		{
			HCI_FormatGenericStep(Step, Options, OutLines);
		}
	}
}
}
