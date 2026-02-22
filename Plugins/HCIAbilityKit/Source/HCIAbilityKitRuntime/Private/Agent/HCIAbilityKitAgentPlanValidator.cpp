#include "Agent/HCIAbilityKitAgentPlanValidator.h"

#include "Agent/HCIAbilityKitAgentExecutionGate.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Regex.h"

namespace
{
static EHCIAbilityKitAgentPlanRiskLevel HCI_ToExpectedPlanRiskLevel(const EHCIAbilityKitToolCapability Capability)
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

static int32 HCI_RiskRank(const EHCIAbilityKitAgentPlanRiskLevel RiskLevel)
{
	switch (RiskLevel)
	{
	case EHCIAbilityKitAgentPlanRiskLevel::ReadOnly:
		return 0;
	case EHCIAbilityKitAgentPlanRiskLevel::Write:
		return 1;
	case EHCIAbilityKitAgentPlanRiskLevel::Destructive:
		return 2;
	default:
		return 0;
	}
}

static void HCI_InitResultFromPlan(const FHCIAbilityKitAgentPlan& Plan, FHCIAbilityKitAgentPlanValidationResult& OutResult)
{
	OutResult = FHCIAbilityKitAgentPlanValidationResult();
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
	FHCIAbilityKitAgentPlanValidationResult& OutResult,
	const TCHAR* ErrorCode,
	const FString& Field,
	const FString& Reason,
	const int32 StepIndex,
	const FHCIAbilityKitAgentPlanStep* Step)
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

static const FHCIAbilityKitToolArgSchema* HCI_FindArgSchema(
	const FHCIAbilityKitToolDescriptor& Tool,
	const FString& ArgName)
{
	return Tool.ArgsSchema.FindByPredicate(
		[&ArgName](const FHCIAbilityKitToolArgSchema& Arg)
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

static bool HCI_ValidateStringValue(
	const FHCIAbilityKitToolArgSchema& Schema,
	const FString& Parsed,
	const FName ToolName,
	const int32 StepIndex,
	const FHCIAbilityKitAgentPlanStep& Step,
	FHCIAbilityKitAgentPlanValidationResult& OutResult)
{
	const FString FieldPath = HCI_MakeArgFieldPath(StepIndex, Schema.ArgName.ToString());

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

static bool HCI_ValidateArgsAgainstSchema(
	const FHCIAbilityKitToolDescriptor& Tool,
	const FHCIAbilityKitAgentPlanStep& Step,
	const int32 StepIndex,
	FHCIAbilityKitAgentPlanValidationResult& OutResult)
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

	for (const FHCIAbilityKitToolArgSchema& Schema : Tool.ArgsSchema)
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
		case EHCIAbilityKitToolArgValueType::String:
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

		case EHCIAbilityKitToolArgValueType::StringArray:
		{
			if (Value->Type != EJson::Array)
			{
				return HCI_Fail(
					OutResult,
					HCI_IsLevelRiskSpecialArg(Tool.ToolName, ArgKey) ? TEXT("E4011") : TEXT("E4003"),
					HCI_MakeArgFieldPath(StepIndex, ArgKey),
					TEXT("arg_type_mismatch_expected_string_array"),
					StepIndex,
					&Step);
			}

			const TArray<TSharedPtr<FJsonValue>>& ArrayValues = Value->AsArray();
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
			}
			break;
		}

		case EHCIAbilityKitToolArgValueType::Int:
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

static int32 HCI_CountModifyTargets(const FHCIAbilityKitAgentPlanStep& Step)
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

	const TSharedPtr<FJsonValue>* AssetPathValue = Step.Args->Values.Find(TEXT("asset_path"));
	if (AssetPathValue && AssetPathValue->IsValid() && (*AssetPathValue)->Type == EJson::String)
	{
		return 1;
	}

	return 0;
}

static bool HCI_CheckNamingMetadataSafetyMock(
	const FHCIAbilityKitAgentPlanValidationContext& Context,
	const FHCIAbilityKitAgentPlanStep& Step,
	const int32 StepIndex,
	FHCIAbilityKitAgentPlanValidationResult& OutResult)
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
}

bool FHCIAbilityKitAgentPlanValidator::ValidatePlan(
	const FHCIAbilityKitAgentPlan& Plan,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	FHCIAbilityKitAgentPlanValidationResult& OutResult)
{
	const FHCIAbilityKitAgentPlanValidationContext DefaultContext;
	return ValidatePlan(Plan, ToolRegistry, DefaultContext, OutResult);
}

bool FHCIAbilityKitAgentPlanValidator::ValidatePlan(
	const FHCIAbilityKitAgentPlan& Plan,
	const FHCIAbilityKitToolRegistry& ToolRegistry,
	const FHCIAbilityKitAgentPlanValidationContext& Context,
	FHCIAbilityKitAgentPlanValidationResult& OutResult)
{
	HCI_InitResultFromPlan(Plan, OutResult);

	FString MinimalContractError;
	if (!FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(Plan, MinimalContractError))
	{
		return HCI_Fail(OutResult, TEXT("E4001"), TEXT("plan"), MinimalContractError, INDEX_NONE, nullptr);
	}

	int32 HighestRiskRank = 0;

	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAbilityKitAgentPlanStep& Step = Plan.Steps[StepIndex];
		const FHCIAbilityKitToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);
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

		const EHCIAbilityKitAgentPlanRiskLevel ExpectedRiskLevel = HCI_ToExpectedPlanRiskLevel(Tool->Capability);
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

		const bool bExpectedRequiresConfirm = FHCIAbilityKitAgentExecutionGate::IsWriteLikeCapability(Tool->Capability);
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

		if (!HCI_ValidateArgsAgainstSchema(*Tool, Step, StepIndex, OutResult))
		{
			return false;
		}

		if (!HCI_CheckNamingMetadataSafetyMock(Context, Step, StepIndex, OutResult))
		{
			return false;
		}

		const bool bWriteLike = FHCIAbilityKitAgentExecutionGate::IsWriteLikeCapability(Tool->Capability);
		if (bWriteLike)
		{
			++OutResult.WriteLikeStepCount;

			const int32 StepModifyCount = HCI_CountModifyTargets(Step);
			OutResult.TotalTargetModifyCount += StepModifyCount;
			if (OutResult.TotalTargetModifyCount > FHCIAbilityKitAgentExecutionGate::MaxAssetModifyLimit)
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
		OutResult.MaxRiskLevel = FHCIAbilityKitAgentPlanContract::RiskLevelToString(
			HighestRiskRank >= 2 ? EHCIAbilityKitAgentPlanRiskLevel::Destructive
								: (HighestRiskRank == 1 ? EHCIAbilityKitAgentPlanRiskLevel::Write : EHCIAbilityKitAgentPlanRiskLevel::ReadOnly));

		++OutResult.ValidatedStepCount;
	}

	OutResult.bValid = true;
	OutResult.ErrorCode = TEXT("-");
	OutResult.Field = TEXT("-");
	OutResult.Reason = TEXT("ok");
	return true;
}

