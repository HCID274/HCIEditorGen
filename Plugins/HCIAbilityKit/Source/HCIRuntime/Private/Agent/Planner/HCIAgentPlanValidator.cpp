#include "Agent/Planner/HCIAgentPlanValidator.h"

#include "Agent/Executor/HCIAgentExecutionGate.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAgentPlanValidator, Log, All);

namespace
{
static EHCIAgentPlanRiskLevel HCI_ToExpectedPlanRiskLevel(const EHCIToolCapability Capability)
{
	switch (Capability)
	{
	case EHCIToolCapability::ReadOnly:
		return EHCIAgentPlanRiskLevel::ReadOnly;
	case EHCIToolCapability::Write:
		return EHCIAgentPlanRiskLevel::Write;
	case EHCIToolCapability::Destructive:
		return EHCIAgentPlanRiskLevel::Destructive;
	default:
		return EHCIAgentPlanRiskLevel::ReadOnly;
	}
}

static int32 HCI_RiskRank(const EHCIAgentPlanRiskLevel RiskLevel)
{
	switch (RiskLevel)
	{
	case EHCIAgentPlanRiskLevel::ReadOnly:
		return 0;
	case EHCIAgentPlanRiskLevel::Write:
		return 1;
	case EHCIAgentPlanRiskLevel::Destructive:
		return 2;
	default:
		return 0;
	}
}

static void HCI_InitResultFromPlan(const FHCIAgentPlan& Plan, FHCIAgentPlanValidationResult& OutResult)
{
	OutResult = FHCIAgentPlanValidationResult();
	OutResult.RequestId = Plan.RequestId;
	OutResult.Intent = Plan.Intent;
	OutResult.PlanVersion = Plan.PlanVersion;
	OutResult.StepCount = Plan.Steps.Num();
	OutResult.ErrorCode = TEXT("-");
	OutResult.Field = TEXT("-");
	OutResult.Reason = TEXT("ok");
	OutResult.MaxRiskLevel = TEXT("read_only");
}

static bool HCI_Fail(
	FHCIAgentPlanValidationResult& OutResult,
	const TCHAR* ErrorCode,
	const FString& Field,
	const FString& Reason,
	const int32 StepIndex,
	const FHCIAgentPlanStep* Step)
{
	OutResult.bValid = false;
	OutResult.ErrorCode = ErrorCode;
	OutResult.Field = Field;
	OutResult.Reason = Reason;
	OutResult.FailedStepIndex = StepIndex;
	if (Step)
	{
		OutResult.FailedStepId = Step->StepId;
		OutResult.FailedToolName = Step->ToolName.ToString();
	}
	return false;
}

static bool HCI_TryGetIntExact(const TSharedPtr<FJsonValue>& Value, int32& OutInt)
{
	if (!Value.IsValid() || Value->Type != EJson::Number)
	{
		return false;
	}

	double Number = 0.0;
	if (!Value->TryGetNumber(Number))
	{
		return false;
	}

	const double Rounded = FMath::RoundToDouble(Number);
	if (!FMath::IsNearlyEqual(Number, Rounded))
	{
		return false;
	}

	OutInt = static_cast<int32>(Rounded);
	return true;
}

static bool HCI_IsVariableTemplateStringForValidation(const FString& Value)
{
	static const FRegexPattern Pattern(TEXT("^\\{\\{\\s*[A-Za-z0-9_]+\\.[A-Za-z0-9_]+(?:\\[\\d+\\])?\\s*\\}\\}$"));
	FRegexMatcher Matcher(Pattern, Value.TrimStartAndEnd());
	return Matcher.FindNext();
}

static const FHCIToolArgSchema* HCI_FindArgSchema(
	const FHCIToolDescriptor& Tool,
	const FString& ArgName)
{
	return Tool.ArgsSchema.FindByPredicate(
		[&ArgName](const FHCIToolArgSchema& Arg)
		{
			return Arg.ArgName.ToString().Equals(ArgName, ESearchCase::CaseSensitive);
		});
}

static bool HCI_IsLevelRiskSpecialArg(const FName ToolName, const FString& ArgName)
{
	return ToolName == TEXT("ScanLevelMeshRisks") &&
		   (ArgName.Equals(TEXT("scope"), ESearchCase::CaseSensitive) ||
			ArgName.Equals(TEXT("checks"), ESearchCase::CaseSensitive));
}

static const TCHAR* HCI_SelectParamErrorCode(const FName ToolName, const FString& ArgName)
{
	return HCI_IsLevelRiskSpecialArg(ToolName, ArgName) ? TEXT("E4011") : TEXT("E4009");
}

static FString HCI_MakeArgFieldPath(const int32 StepIndex, const FString& ArgName)
{
	return FString::Printf(TEXT("steps[%d].args.%s"), StepIndex, *ArgName);
}

static void HCI_AddEvidenceKeys(TSet<FString>& OutSet, std::initializer_list<const TCHAR*> Keys)
{
	for (const TCHAR* Key : Keys)
	{
		OutSet.Add(FString(Key));
	}
}

static bool HCI_GetAllowedExpectedEvidenceSet(
	const FName ToolName,
	TSet<FString>& OutAllowedSet)
{
	OutAllowedSet.Reset();

	if (ToolName == TEXT("SearchPath"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("keyword"),
			TEXT("keyword_expanded"),
			TEXT("keyword_expanded_count"),
			TEXT("matched_count"),
			TEXT("matched_directories"),
			TEXT("best_directory"),
			TEXT("semantic_fallback_used"),
			TEXT("semantic_fallback_directory"),
			TEXT("keyword_normalized"),
			TEXT("scanned_game_paths"),
			TEXT("result")});
		return true;
	}
	if (ToolName == TEXT("ScanAssets"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("scan_root"),
			TEXT("asset_count"),
			TEXT("result"),
			TEXT("asset_path"),
			TEXT("asset_paths")});
		return true;
	}
	if (ToolName == TEXT("ScanMeshTriangleCount"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("scan_root"),
			TEXT("scanned_count"),
			TEXT("mesh_count"),
			TEXT("max_triangle_count"),
			TEXT("max_triangle_asset"),
			TEXT("top_meshes"),
			TEXT("result")});
		return true;
	}
	if (ToolName == TEXT("SetTextureMaxSize"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("target_max_size"),
			TEXT("scanned_count"),
			TEXT("modified_count"),
			TEXT("failed_count"),
			TEXT("modified_assets"),
			TEXT("failed_assets"),
			TEXT("result")});
		return true;
	}
	if (ToolName == TEXT("SetMeshLODGroup"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("target_lod_group"),
			TEXT("scanned_count"),
			TEXT("modified_count"),
			TEXT("failed_count"),
			TEXT("modified_assets"),
			TEXT("failed_assets"),
			TEXT("result")});
		return true;
	}
	if (ToolName == TEXT("ScanLevelMeshRisks"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("actor_path"),
			TEXT("issue"),
			TEXT("evidence"),
			TEXT("scope"),
			TEXT("scanned_count"),
			TEXT("risky_count"),
			TEXT("risk_summary"),
			TEXT("risky_actors"),
			TEXT("missing_collision_actors"),
			TEXT("default_material_actors"),
			TEXT("result")});
		return true;
	}
	if (ToolName == TEXT("NormalizeAssetNamingByMetadata"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("result"),
			TEXT("proposed_renames"),
			TEXT("proposed_moves"),
			TEXT("affected_count")});
		return true;
	}
	if (ToolName == TEXT("RenameAsset") || ToolName == TEXT("MoveAsset"))
	{
		HCI_AddEvidenceKeys(OutAllowedSet, {
			TEXT("asset_path"),
			TEXT("before"),
			TEXT("after"),
			TEXT("result"),
			TEXT("redirector_fixup"),
			TEXT("redirector_count")});
		return true;
	}

	return false;
}

static bool HCI_ValidateExpectedEvidence(
	const FHCIAgentPlanStep& Step,
	const int32 StepIndex,
	FHCIAgentPlanValidationResult& OutResult)
{
	if (Step.ExpectedEvidence.Num() <= 0)
	{
		return HCI_Fail(
			OutResult,
			TEXT("E4001"),
			FString::Printf(TEXT("steps[%d].expected_evidence"), StepIndex),
			TEXT("expected_evidence_missing"),
			StepIndex,
			&Step);
	}

	TSet<FString> AllowedEvidence;
	if (!HCI_GetAllowedExpectedEvidenceSet(Step.ToolName, AllowedEvidence))
	{
		return true;
	}

	TSet<FString> SeenEvidence;
	for (int32 EvidenceIndex = 0; EvidenceIndex < Step.ExpectedEvidence.Num(); ++EvidenceIndex)
	{
		const FString EvidenceKey = Step.ExpectedEvidence[EvidenceIndex].TrimStartAndEnd();
		if (EvidenceKey.IsEmpty())
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4009"),
				FString::Printf(TEXT("steps[%d].expected_evidence[%d]"), StepIndex, EvidenceIndex),
				TEXT("expected_evidence_empty"),
				StepIndex,
				&Step);
		}
		if (SeenEvidence.Contains(EvidenceKey))
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4009"),
				FString::Printf(TEXT("steps[%d].expected_evidence[%d]"), StepIndex, EvidenceIndex),
				TEXT("expected_evidence_duplicate"),
				StepIndex,
				&Step);
		}
		SeenEvidence.Add(EvidenceKey);

		if (!AllowedEvidence.Contains(EvidenceKey))
		{
			const FString IllegalEvidenceDetail = FString::Printf(
				TEXT("Illegal evidence key \"%s\" for tool \"%s\"."),
				*EvidenceKey,
				*Step.ToolName.ToString());
			UE_LOG(
				LogHCIAgentPlanValidator,
				Warning,
				TEXT("[HCI][AgentPlanValidator] error_code=E4009 field=steps[%d].expected_evidence[%d] reason=expected_evidence_not_allowed_for_tool detail=%s"),
				StepIndex,
				EvidenceIndex,
				*IllegalEvidenceDetail);
			return HCI_Fail(
				OutResult,
				TEXT("E4009"),
				FString::Printf(TEXT("steps[%d].expected_evidence[%d]"), StepIndex, EvidenceIndex),
				IllegalEvidenceDetail,
				StepIndex,
				&Step);
		}
	}

	return true;
}

static bool HCI_ValidateUiPresentation(
	const FHCIAgentPlanStep& Step,
	const int32 StepIndex,
	FHCIAgentPlanValidationResult& OutResult)
{
	const FHCIAgentPlanStep::FUiPresentation& Ui = Step.UiPresentation;
	if (!Ui.HasAnyField())
	{
		return true;
	}

	auto ValidateField = [&](const bool bHasValue, const FString& Value, const TCHAR* FieldName, const int32 MaxLen) -> bool
	{
		if (!bHasValue)
		{
			return true;
		}

		const FString Trimmed = Value.TrimStartAndEnd();
		const FString FieldPath = FString::Printf(TEXT("steps[%d].ui_presentation.%s"), StepIndex, FieldName);
		if (Trimmed.IsEmpty())
		{
			return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("ui_presentation_empty"), StepIndex, &Step);
		}
		if (Trimmed.Len() > MaxLen)
		{
			return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("ui_presentation_too_long"), StepIndex, &Step);
		}
		return true;
	};

	if (!ValidateField(Ui.bHasStepSummary, Ui.StepSummary, TEXT("step_summary"), 120))
	{
		return false;
	}
	if (!ValidateField(Ui.bHasIntentReason, Ui.IntentReason, TEXT("intent_reason"), 160))
	{
		return false;
	}
	if (!ValidateField(Ui.bHasRiskWarning, Ui.RiskWarning, TEXT("risk_warning"), 200))
	{
		return false;
	}
	return true;
}

static bool HCI_ValidateStringValue(
	const FHCIToolArgSchema& Schema,
	const FString& Parsed,
	const FName ToolName,
	const int32 StepIndex,
	const FHCIAgentPlanStep& Step,
	FHCIAgentPlanValidationResult& OutResult)
{
	const FString FieldPath = HCI_MakeArgFieldPath(StepIndex, Schema.ArgName.ToString());
	const bool bIsVariableTemplate = HCI_IsVariableTemplateStringForValidation(Parsed);
	if (bIsVariableTemplate)
	{
		// Variable piping placeholders are only allowed on /Game path-like args.
		if (!Schema.bMustStartWithGamePath)
		{
			return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("variable_template_not_allowed_for_arg"), StepIndex, &Step);
		}
		return true;
	}

	if (Schema.MinStringLength != INDEX_NONE && Parsed.Len() < Schema.MinStringLength)
	{
		return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("string_too_short"), StepIndex, &Step);
	}
	if (Schema.MaxStringLength != INDEX_NONE && Parsed.Len() > Schema.MaxStringLength)
	{
		return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("string_too_long"), StepIndex, &Step);
	}
	if (!Schema.RegexPattern.IsEmpty())
	{
		const FRegexPattern Pattern(Schema.RegexPattern);
		FRegexMatcher Matcher(Pattern, Parsed);
		const bool bMatchFound = Matcher.FindNext() && Matcher.GetMatchBeginning() == 0 && Matcher.GetMatchEnding() == Parsed.Len();
		if (!bMatchFound)
		{
			return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("regex_mismatch"), StepIndex, &Step);
		}
	}
	if (Schema.bMustStartWithGamePath && !Parsed.StartsWith(TEXT("/Game/")))
	{
		return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("must_start_with_game_path"), StepIndex, &Step);
	}
	if (Schema.AllowedStringValues.Num() > 0 && !Schema.AllowedStringValues.Contains(Parsed))
	{
		return HCI_Fail(
			OutResult,
			HCI_SelectParamErrorCode(ToolName, Schema.ArgName.ToString()),
			FieldPath,
			TEXT("enum_value_not_allowed"),
			StepIndex,
			&Step);
	}
	return true;
}

// Forward decl (used by args/schema validation and by optional field-level pipeline variables).
static bool HCI_TryParseVariableTemplateReference(
	const FString& InText,
	FString& OutStepId,
	FString& OutEvidenceKey);

static bool HCI_ValidateArgsAgainstSchema(
	const FHCIToolDescriptor& Tool,
	const FHCIAgentPlanStep& Step,
	const int32 StepIndex,
	FHCIAgentPlanValidationResult& OutResult)
{
	if (!Step.Args.IsValid())
	{
		return HCI_Fail(OutResult, TEXT("E4001"), FString::Printf(TEXT("steps[%d].args"), StepIndex), TEXT("args_missing"), StepIndex, &Step);
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step.Args->Values)
	{
		if (!HCI_FindArgSchema(Tool, Pair.Key))
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4009"),
				HCI_MakeArgFieldPath(StepIndex, Pair.Key),
				TEXT("arg_not_declared_in_schema"),
				StepIndex,
				&Step);
		}
	}

	for (const FHCIToolArgSchema& Schema : Tool.ArgsSchema)
	{
		const FString ArgKey = Schema.ArgName.ToString();
		const TSharedPtr<FJsonValue>* ValuePtr = Step.Args->Values.Find(ArgKey);
		if (!ValuePtr || !ValuePtr->IsValid())
		{
			if (Schema.bRequired)
			{
				return HCI_Fail(
					OutResult,
					TEXT("E4001"),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("required_arg_missing"),
					StepIndex,
					&Step);
			}
			continue;
		}

		const TSharedPtr<FJsonValue>& Value = *ValuePtr;
		switch (Schema.ValueType)
		{
		case EHCIToolArgValueType::String:
		{
			if (Value->Type != EJson::String)
			{
				return HCI_Fail(
					OutResult,
					HCI_IsLevelRiskSpecialArg(Tool.ToolName, ArgKey) ? TEXT("E4011") : TEXT("E4003"),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("arg_type_mismatch_expected_string"),
					StepIndex,
					&Step);
			}
			FString Parsed;
			if (!Value->TryGetString(Parsed))
			{
				return HCI_Fail(OutResult, TEXT("E4003"), HCI_MakeArgFieldPath(StepIndex, ArgKey), TEXT("arg_string_parse_failed"), StepIndex, &Step);
			}
			if (!HCI_ValidateStringValue(Schema, Parsed, Tool.ToolName, StepIndex, Step, OutResult))
			{
				return false;
			}
			break;
		}

		case EHCIToolArgValueType::StringArray:
		{
			if (Value->Type != EJson::Array)
			{
				// Stage N4: allow field-level pipeline variable for NormalizeAssetNamingByMetadata.asset_paths, e.g.
				// "asset_paths": "{{step_1_scan.asset_paths}}"
				if (Tool.ToolName == TEXT("NormalizeAssetNamingByMetadata") &&
					ArgKey.Equals(TEXT("asset_paths"), ESearchCase::CaseSensitive) &&
					Value->Type == EJson::String)
				{
					FString Parsed;
					if (Value->TryGetString(Parsed))
					{
						FString SourceStepId;
						FString SourceEvidenceKey;
						if (HCI_TryParseVariableTemplateReference(Parsed, SourceStepId, SourceEvidenceKey))
						{
							// The executor will resolve it to a concrete array before tool execution.
							break;
						}
					}
				}
				return HCI_Fail(
					OutResult,
					HCI_IsLevelRiskSpecialArg(Tool.ToolName, ArgKey) ? TEXT("E4011") : TEXT("E4003"),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("arg_type_mismatch_expected_string_array"),
					StepIndex,
					&Step);
			}

			const TArray<TSharedPtr<FJsonValue>>& ArrayValues = Value->AsArray();
			TSet<FString> SeenStringValues;
			if (Schema.MinArrayLength != INDEX_NONE && ArrayValues.Num() < Schema.MinArrayLength)
			{
				return HCI_Fail(
					OutResult,
					HCI_SelectParamErrorCode(Tool.ToolName, ArgKey),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("array_too_short"),
					StepIndex,
					&Step);
			}
			if (Schema.MaxArrayLength != INDEX_NONE && ArrayValues.Num() > Schema.MaxArrayLength)
			{
				return HCI_Fail(
					OutResult,
					HCI_SelectParamErrorCode(Tool.ToolName, ArgKey),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("array_too_long"),
					StepIndex,
					&Step);
			}

			for (int32 ArrayIndex = 0; ArrayIndex < ArrayValues.Num(); ++ArrayIndex)
			{
				const TSharedPtr<FJsonValue>& Element = ArrayValues[ArrayIndex];
				if (!Element.IsValid() || Element->Type != EJson::String)
				{
					return HCI_Fail(
						OutResult,
						HCI_IsLevelRiskSpecialArg(Tool.ToolName, ArgKey) ? TEXT("E4011") : TEXT("E4003"),
						FString::Printf(TEXT("%s[%d]"), *HCI_MakeArgFieldPath(StepIndex, ArgKey), ArrayIndex),
						TEXT("array_element_type_mismatch_expected_string"),
						StepIndex,
						&Step);
				}

				FString Parsed;
				if (!Element->TryGetString(Parsed))
				{
					return HCI_Fail(
						OutResult,
						TEXT("E4003"),
						FString::Printf(TEXT("%s[%d]"), *HCI_MakeArgFieldPath(StepIndex, ArgKey), ArrayIndex),
						TEXT("array_element_string_parse_failed"),
						StepIndex,
						&Step);
				}

				if (Schema.bMustStartWithGamePath && !Parsed.StartsWith(TEXT("/Game/")))
				{
					return HCI_Fail(
						OutResult,
						TEXT("E4009"),
						FString::Printf(TEXT("%s[%d]"), *HCI_MakeArgFieldPath(StepIndex, ArgKey), ArrayIndex),
						TEXT("must_start_with_game_path"),
						StepIndex,
						&Step);
				}

				if (Schema.AllowedStringValues.Num() > 0 && !Schema.AllowedStringValues.Contains(Parsed))
				{
					return HCI_Fail(
						OutResult,
						HCI_SelectParamErrorCode(Tool.ToolName, ArgKey),
						FString::Printf(TEXT("%s[%d]"), *HCI_MakeArgFieldPath(StepIndex, ArgKey), ArrayIndex),
						TEXT("enum_value_not_allowed"),
						StepIndex,
						&Step);
				}

				if (Schema.bStringArrayAllowsSubsetOfEnum)
				{
					if (SeenStringValues.Contains(Parsed))
					{
						return HCI_Fail(
							OutResult,
							HCI_SelectParamErrorCode(Tool.ToolName, ArgKey),
							FString::Printf(TEXT("%s[%d]"), *HCI_MakeArgFieldPath(StepIndex, ArgKey), ArrayIndex),
							TEXT("duplicate_value_not_allowed_for_subset_enum"),
							StepIndex,
							&Step);
					}
					SeenStringValues.Add(Parsed);
				}
			}
			break;
		}

		case EHCIToolArgValueType::Int:
		{
			int32 ParsedInt = 0;
			if (!HCI_TryGetIntExact(Value, ParsedInt))
			{
				return HCI_Fail(
					OutResult,
					TEXT("E4003"),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("arg_type_mismatch_expected_int"),
					StepIndex,
					&Step);
			}

			if (Schema.AllowedIntValues.Num() > 0 && !Schema.AllowedIntValues.Contains(ParsedInt))
			{
				return HCI_Fail(OutResult, TEXT("E4009"), HCI_MakeArgFieldPath(StepIndex, ArgKey), TEXT("enum_value_not_allowed"), StepIndex, &Step);
			}
			if (Schema.MinIntValue != INDEX_NONE && ParsedInt < Schema.MinIntValue)
			{
				return HCI_Fail(OutResult, TEXT("E4009"), HCI_MakeArgFieldPath(StepIndex, ArgKey), TEXT("int_below_min"), StepIndex, &Step);
			}
			if (Schema.MaxIntValue != INDEX_NONE && ParsedInt > Schema.MaxIntValue)
			{
				return HCI_Fail(OutResult, TEXT("E4009"), HCI_MakeArgFieldPath(StepIndex, ArgKey), TEXT("int_above_max"), StepIndex, &Step);
			}
			break;
		}

		default:
			return HCI_Fail(OutResult, TEXT("E4003"), HCI_MakeArgFieldPath(StepIndex, ArgKey), TEXT("unsupported_arg_value_type"), StepIndex, &Step);
		}
	}

	return true;
}

static int32 HCI_CountModifyTargets(const FHCIAgentPlanStep& Step)
{
	if (!Step.Args.IsValid())
	{
		return 0;
	}

	const TSharedPtr<FJsonValue>* AssetPathsValue = Step.Args->Values.Find(TEXT("asset_paths"));
	if (AssetPathsValue && AssetPathsValue->IsValid() && (*AssetPathsValue)->Type == EJson::Array)
	{
		return (*AssetPathsValue)->AsArray().Num();
	}
	if (AssetPathsValue && AssetPathsValue->IsValid() && (*AssetPathsValue)->Type == EJson::String)
	{
		FString Raw;
		if ((*AssetPathsValue)->TryGetString(Raw))
		{
			Raw.TrimStartAndEndInline();
			// Conservative: a pipeline variable may expand up to the hard modify limit.
			if (Raw.StartsWith(TEXT("{{")) && Raw.Contains(TEXT(".")) && Raw.EndsWith(TEXT("}}")))
			{
				return FHCIAgentExecutionGate::MaxAssetModifyLimit;
			}
		}
	}

	const TSharedPtr<FJsonValue>* AssetPathValue = Step.Args->Values.Find(TEXT("asset_path"));
	if (AssetPathValue && AssetPathValue->IsValid() && (*AssetPathValue)->Type == EJson::String)
	{
		return 1;
	}

	return 0;
}

static bool HCI_CheckNamingMetadataSafetyMock(
	const FHCIAgentPlanValidationContext& Context,
	const FHCIAgentPlanStep& Step,
	const int32 StepIndex,
	FHCIAgentPlanValidationResult& OutResult)
{
	if (Step.ToolName != TEXT("NormalizeAssetNamingByMetadata") || !Step.Args.IsValid())
	{
		return true;
	}

	if (Context.MockMetadataUnavailableAssetPaths.Num() == 0)
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (!Step.Args->TryGetArrayField(TEXT("asset_paths"), AssetPaths) || AssetPaths == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Value : *AssetPaths)
	{
		FString AssetPath;
		if (Value.IsValid() && Value->TryGetString(AssetPath) && Context.MockMetadataUnavailableAssetPaths.Contains(AssetPath))
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4012"),
				HCI_MakeArgFieldPath(StepIndex, TEXT("asset_paths")),
				TEXT("naming_metadata_insufficient_no_safe_proposal"),
				StepIndex,
				&Step);
		}
	}

	return true;
}

struct FHCIVariableTemplateRef
{
	FString SourceStepId;
	FString SourceEvidenceKey;
	FString FieldPath;
};

static bool HCI_TryParseVariableTemplateReference(
	const FString& InText,
	FString& OutSourceStepId,
	FString& OutSourceEvidenceKey)
{
	OutSourceStepId.Reset();
	OutSourceEvidenceKey.Reset();
	const FString Trimmed = InText.TrimStartAndEnd();
	static const FRegexPattern Pattern(TEXT("^\\{\\{\\s*([A-Za-z0-9_]+)\\.([A-Za-z0-9_]+)(?:\\[\\d+\\])?\\s*\\}\\}$"));
	FRegexMatcher Matcher(Pattern, Trimmed);
	if (!Matcher.FindNext())
	{
		return false;
	}

	OutSourceStepId = Matcher.GetCaptureGroup(1);
	OutSourceEvidenceKey = Matcher.GetCaptureGroup(2);
	return !OutSourceStepId.IsEmpty() && !OutSourceEvidenceKey.IsEmpty();
}

static void HCI_CollectVariableTemplateRefsFromJsonValue(
	const TSharedPtr<FJsonValue>& InValue,
	const FString& InFieldPath,
	TArray<FHCIVariableTemplateRef>& OutRefs)
{
	if (!InValue.IsValid())
	{
		return;
	}

	switch (InValue->Type)
	{
	case EJson::String:
	{
		FString SourceStepId;
		FString SourceEvidenceKey;
		if (HCI_TryParseVariableTemplateReference(InValue->AsString(), SourceStepId, SourceEvidenceKey))
		{
			FHCIVariableTemplateRef& Ref = OutRefs.AddDefaulted_GetRef();
			Ref.SourceStepId = SourceStepId;
			Ref.SourceEvidenceKey = SourceEvidenceKey;
			Ref.FieldPath = InFieldPath;
		}
		break;
	}
	case EJson::Array:
	{
		const TArray<TSharedPtr<FJsonValue>>& Items = InValue->AsArray();
		for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
		{
			HCI_CollectVariableTemplateRefsFromJsonValue(
				Items[ItemIndex],
				FString::Printf(TEXT("%s[%d]"), *InFieldPath, ItemIndex),
				OutRefs);
		}
		break;
	}
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject> Obj = InValue->AsObject();
		if (!Obj.IsValid())
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
		{
			HCI_CollectVariableTemplateRefsFromJsonValue(
				Pair.Value,
				InFieldPath + TEXT(".") + Pair.Key,
				OutRefs);
		}
		break;
	}
	default:
		break;
	}
}

static bool HCI_ValidateVariableTemplateReferences(
	const int32 StepIndex,
	const FHCIAgentPlanStep& Step,
	const TMap<FString, int32>& StepIndexById,
	FHCIAgentPlanValidationResult& OutResult)
{
	if (!Step.Args.IsValid())
	{
		return true;
	}

	TArray<FHCIVariableTemplateRef> TemplateRefs;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step.Args->Values)
	{
		HCI_CollectVariableTemplateRefsFromJsonValue(
			Pair.Value,
			HCI_MakeArgFieldPath(StepIndex, Pair.Key),
			TemplateRefs);
	}

	for (const FHCIVariableTemplateRef& Ref : TemplateRefs)
	{
		const int32* SourceStepIndexPtr = StepIndexById.Find(Ref.SourceStepId);
		if (SourceStepIndexPtr == nullptr)
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4009"),
				Ref.FieldPath,
				TEXT("variable_source_step_missing"),
				StepIndex,
				&Step);
		}
		if (*SourceStepIndexPtr >= StepIndex)
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4009"),
				Ref.FieldPath,
				TEXT("variable_source_step_must_precede_consumer"),
				StepIndex,
				&Step);
		}
	}

	return true;
}

static bool HCI_IsModifyStyleIntent(const FString& InIntent)
{
	const FString IntentLower = InIntent.TrimStartAndEnd().ToLower();
	return IntentLower == TEXT("batch_fix_asset_compliance") ||
		IntentLower == TEXT("normalize_temp_assets_by_metadata") ||
		IntentLower.Contains(TEXT("batch_fix")) ||
		IntentLower.Contains(TEXT("normalize")) ||
		IntentLower.Contains(TEXT("rename")) ||
		IntentLower.Contains(TEXT("move"));
}

static bool HCI_IsWriteLikeStep(const FHCIAgentPlanStep& Step)
{
	return Step.bRequiresConfirm || Step.RiskLevel != EHCIAgentPlanRiskLevel::ReadOnly;
}

static bool HCI_ValidateModifyIntentHasWriteStep(
	const FHCIAgentPlan& Plan,
	const FHCIAgentPlanValidationContext& Context,
	FHCIAgentPlanValidationResult& OutResult)
{
	if (!Context.bRequireWriteStepForModifyIntent || !HCI_IsModifyStyleIntent(Plan.Intent))
	{
		return true;
	}

	for (const FHCIAgentPlanStep& Step : Plan.Steps)
	{
		if (HCI_IsWriteLikeStep(Step))
		{
			return true;
		}
	}

	return HCI_Fail(
		OutResult,
		TEXT("E4009"),
		TEXT("steps"),
		TEXT("modify_intent_requires_write_step"),
		INDEX_NONE,
		nullptr);
}

static bool HCI_ValidatePipelineReferenceTargetsScanAssets(
	const FHCIAgentPlan& Plan,
	const int32 StepIndex,
	const FHCIAgentPlanStep& Step,
	const FHCIVariableTemplateRef& Ref,
	const TMap<FString, int32>& StepIndexById,
	FHCIAgentPlanValidationResult& OutResult)
{
	const int32* SourceStepIndexPtr = StepIndexById.Find(Ref.SourceStepId);
	if (SourceStepIndexPtr == nullptr)
	{
		return HCI_Fail(
			OutResult,
			TEXT("E4009"),
			Ref.FieldPath,
			TEXT("pipeline_source_step_missing"),
			StepIndex,
			&Step);
	}
	if (*SourceStepIndexPtr >= StepIndex)
	{
		return HCI_Fail(
			OutResult,
			TEXT("E4009"),
			Ref.FieldPath,
			TEXT("pipeline_source_step_must_precede_consumer"),
			StepIndex,
			&Step);
	}

	const FHCIAgentPlanStep& SourceStep = Plan.Steps[*SourceStepIndexPtr];
	if (SourceStep.ToolName != TEXT("ScanAssets"))
	{
		return HCI_Fail(
			OutResult,
			TEXT("E4009"),
			Ref.FieldPath,
			TEXT("pipeline_source_must_be_scan_assets"),
			StepIndex,
			&Step);
	}
	if (!SourceStep.ExpectedEvidence.Contains(TEXT("asset_paths")))
	{
		return HCI_Fail(
			OutResult,
			TEXT("E4009"),
			Ref.FieldPath,
			TEXT("pipeline_source_missing_asset_paths_evidence"),
			StepIndex,
			&Step);
	}
	if (!Ref.SourceEvidenceKey.Equals(TEXT("asset_paths"), ESearchCase::CaseSensitive))
	{
		return HCI_Fail(
			OutResult,
			TEXT("E4009"),
			Ref.FieldPath,
			TEXT("pipeline_evidence_key_must_be_asset_paths"),
			StepIndex,
			&Step);
	}

	return true;
}

static bool HCI_ValidatePipelineRequiredArgs(
	const FHCIAgentPlan& Plan,
	const FHCIToolDescriptor& Tool,
	const int32 StepIndex,
	const FHCIAgentPlanStep& Step,
	const TMap<FString, int32>& StepIndexById,
	const FHCIAgentPlanValidationContext& Context,
	FHCIAgentPlanValidationResult& OutResult)
{
	if (!Context.bRequirePipelineInputs || !Step.Args.IsValid())
	{
		return true;
	}

	for (const FHCIToolArgSchema& Schema : Tool.ArgsSchema)
	{
		if (!Schema.bRequiresPipelineInput)
		{
			continue;
		}

		const FString ArgKey = Schema.ArgName.ToString();
		const TSharedPtr<FJsonValue>* ValuePtr = Step.Args->Values.Find(ArgKey);
		if (!ValuePtr || !ValuePtr->IsValid())
		{
			continue;
		}

		const FString FieldPath = HCI_MakeArgFieldPath(StepIndex, ArgKey);
		const TSharedPtr<FJsonValue>& Value = *ValuePtr;

		if (Schema.ValueType == EHCIToolArgValueType::String)
		{
			if (Value->Type != EJson::String)
			{
				return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("pipeline_input_required_for_arg"), StepIndex, &Step);
			}

			FString SourceStepId;
			FString SourceEvidenceKey;
			if (!HCI_TryParseVariableTemplateReference(Value->AsString(), SourceStepId, SourceEvidenceKey))
			{
				return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("pipeline_input_required_for_arg"), StepIndex, &Step);
			}

			FHCIVariableTemplateRef Ref;
			Ref.SourceStepId = SourceStepId;
			Ref.SourceEvidenceKey = SourceEvidenceKey;
			Ref.FieldPath = FieldPath;
			if (!HCI_ValidatePipelineReferenceTargetsScanAssets(Plan, StepIndex, Step, Ref, StepIndexById, OutResult))
			{
				return false;
			}
			continue;
		}

		if (Schema.ValueType == EHCIToolArgValueType::StringArray)
		{
			if (Value->Type != EJson::Array)
			{
				return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("pipeline_input_required_for_arg"), StepIndex, &Step);
			}

			const TArray<TSharedPtr<FJsonValue>>& ArrayValues = Value->AsArray();
			if (ArrayValues.Num() <= 0)
			{
				return HCI_Fail(OutResult, TEXT("E4009"), FieldPath, TEXT("pipeline_input_required_for_arg"), StepIndex, &Step);
			}

			for (int32 ArrayIndex = 0; ArrayIndex < ArrayValues.Num(); ++ArrayIndex)
			{
				const TSharedPtr<FJsonValue>& Element = ArrayValues[ArrayIndex];
				const FString ElementFieldPath = FString::Printf(TEXT("%s[%d]"), *FieldPath, ArrayIndex);
				if (!Element.IsValid() || Element->Type != EJson::String)
				{
					return HCI_Fail(OutResult, TEXT("E4009"), ElementFieldPath, TEXT("pipeline_input_required_for_arg"), StepIndex, &Step);
				}

				FString SourceStepId;
				FString SourceEvidenceKey;
				if (!HCI_TryParseVariableTemplateReference(Element->AsString(), SourceStepId, SourceEvidenceKey))
				{
					return HCI_Fail(OutResult, TEXT("E4009"), ElementFieldPath, TEXT("pipeline_input_required_for_arg"), StepIndex, &Step);
				}

				FHCIVariableTemplateRef Ref;
				Ref.SourceStepId = SourceStepId;
				Ref.SourceEvidenceKey = SourceEvidenceKey;
				Ref.FieldPath = ElementFieldPath;
				if (!HCI_ValidatePipelineReferenceTargetsScanAssets(Plan, StepIndex, Step, Ref, StepIndexById, OutResult))
				{
					return false;
				}
			}
			continue;
		}
	}

	return true;
}
}

bool FHCIAgentPlanValidator::ValidatePlan(
	const FHCIAgentPlan& Plan,
	const FHCIToolRegistry& ToolRegistry,
	FHCIAgentPlanValidationResult& OutResult)
{
	const FHCIAgentPlanValidationContext DefaultContext;
	return ValidatePlan(Plan, ToolRegistry, DefaultContext, OutResult);
}

bool FHCIAgentPlanValidator::ValidatePlan(
	const FHCIAgentPlan& Plan,
	const FHCIToolRegistry& ToolRegistry,
	const FHCIAgentPlanValidationContext& Context,
	FHCIAgentPlanValidationResult& OutResult)
{
	HCI_InitResultFromPlan(Plan, OutResult);

	FString MinimalContractError;
	if (!FHCIAgentPlanContract::ValidateMinimalContract(Plan, MinimalContractError))
	{
		return HCI_Fail(OutResult, TEXT("E4001"), TEXT("plan"), MinimalContractError, INDEX_NONE, nullptr);
	}
	if (!HCI_ValidateModifyIntentHasWriteStep(Plan, Context, OutResult))
	{
		return false;
	}

	int32 HighestRiskRank = 0;
	TMap<FString, int32> StepIndexById;
	StepIndexById.Reserve(Plan.Steps.Num());
	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAgentPlanStep& Step = Plan.Steps[StepIndex];
		if (Step.StepId.IsEmpty())
		{
			continue;
		}
		if (StepIndexById.Contains(Step.StepId))
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4001"),
				FString::Printf(TEXT("steps[%d].step_id"), StepIndex),
				TEXT("duplicate_step_id"),
				StepIndex,
				&Step);
		}
		StepIndexById.Add(Step.StepId, StepIndex);
	}

	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAgentPlanStep& Step = Plan.Steps[StepIndex];
		const FHCIToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);
		if (Tool == nullptr)
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4002"),
				FString::Printf(TEXT("steps[%d].tool_name"), StepIndex),
				TEXT("tool_not_whitelisted"),
				StepIndex,
				&Step);
		}

		const EHCIAgentPlanRiskLevel ExpectedRiskLevel = HCI_ToExpectedPlanRiskLevel(Tool->Capability);
		if (Step.RiskLevel != ExpectedRiskLevel)
		{
			return HCI_Fail(
				OutResult,
				TEXT("E4003"),
				FString::Printf(TEXT("steps[%d].risk_level"), StepIndex),
				TEXT("risk_level_mismatch_with_tool_capability"),
				StepIndex,
				&Step);
		}

		const bool bExpectedRequiresConfirm = FHCIAgentExecutionGate::IsWriteLikeCapability(Tool->Capability);
			if (Step.bRequiresConfirm != bExpectedRequiresConfirm)
			{
				return HCI_Fail(
					OutResult,
					TEXT("E4003"),
				FString::Printf(TEXT("steps[%d].requires_confirm"), StepIndex),
				TEXT("requires_confirm_mismatch_with_tool_capability"),
					StepIndex,
					&Step);
			}

			if (!HCI_ValidateExpectedEvidence(Step, StepIndex, OutResult))
			{
				return false;
			}
			if (!HCI_ValidateUiPresentation(Step, StepIndex, OutResult))
			{
				return false;
			}
			if (!HCI_ValidateArgsAgainstSchema(*Tool, Step, StepIndex, OutResult))
			{
				return false;
			}
			if (!HCI_ValidateVariableTemplateReferences(StepIndex, Step, StepIndexById, OutResult))
			{
				return false;
			}
			if (!HCI_ValidatePipelineRequiredArgs(Plan, *Tool, StepIndex, Step, StepIndexById, Context, OutResult))
			{
				return false;
			}

		if (!HCI_CheckNamingMetadataSafetyMock(Context, Step, StepIndex, OutResult))
		{
			return false;
		}

		const bool bWriteLike = FHCIAgentExecutionGate::IsWriteLikeCapability(Tool->Capability);
		if (bWriteLike)
		{
			++OutResult.WriteLikeStepCount;

			const int32 StepModifyCount = HCI_CountModifyTargets(Step);
			OutResult.TotalTargetModifyCount += StepModifyCount;
			if (OutResult.TotalTargetModifyCount > FHCIAgentExecutionGate::MaxAssetModifyLimit)
			{
				return HCI_Fail(
					OutResult,
					TEXT("E4004"),
					FString::Printf(TEXT("steps[%d].args.asset_paths"), StepIndex),
					TEXT("modify_limit_exceeded"),
					StepIndex,
					&Step);
			}
		}

		HighestRiskRank = FMath::Max(HighestRiskRank, HCI_RiskRank(Step.RiskLevel));
		OutResult.MaxRiskLevel = FHCIAgentPlanContract::RiskLevelToString(
			HighestRiskRank >= 2 ? EHCIAgentPlanRiskLevel::Destructive
								: (HighestRiskRank == 1 ? EHCIAgentPlanRiskLevel::Write : EHCIAgentPlanRiskLevel::ReadOnly));

		++OutResult.ValidatedStepCount;
	}

	OutResult.bValid = true;
	OutResult.ErrorCode = TEXT("-");
	OutResult.Field = TEXT("-");
	OutResult.Reason = TEXT("ok");
	return true;
}

