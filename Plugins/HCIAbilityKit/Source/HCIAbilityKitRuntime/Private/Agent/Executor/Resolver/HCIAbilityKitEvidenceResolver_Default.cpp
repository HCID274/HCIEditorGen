#include "Agent/Executor/Resolver/HCIAbilityKitEvidenceResolver_Default.h"

#include "Agent/Executor/Interfaces/IHCIAbilityKitEvidenceContextView.h"
#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Regex.h"

namespace
{
static bool HCI_ParseVariableTemplate(
	const FString& InText,
	FString& OutStepId,
	FString& OutEvidenceKey,
	bool& bOutHasIndex,
	int32& OutIndex)
{
	OutStepId.Reset();
	OutEvidenceKey.Reset();
	bOutHasIndex = false;
	OutIndex = INDEX_NONE;

	const FString Trimmed = InText.TrimStartAndEnd();
	static const FRegexPattern Pattern(TEXT("^\\{\\{\\s*([A-Za-z0-9_]+)\\.([A-Za-z0-9_]+)(?:\\[(\\d+)\\])?\\s*\\}\\}$"));
	FRegexMatcher Matcher(Pattern, Trimmed);
	if (!Matcher.FindNext())
	{
		return false;
	}

	OutStepId = Matcher.GetCaptureGroup(1);
	OutEvidenceKey = Matcher.GetCaptureGroup(2);
	const FString IndexString = Matcher.GetCaptureGroup(3);
	if (!IndexString.IsEmpty())
	{
		int32 ParsedIndex = INDEX_NONE;
		if (!LexTryParseString(ParsedIndex, *IndexString) || ParsedIndex < 0)
		{
			return false;
		}
		bOutHasIndex = true;
		OutIndex = ParsedIndex;
	}

	return !OutStepId.IsEmpty() && !OutEvidenceKey.IsEmpty();
}

static bool HCI_StringMayContainVariableTemplate(const FString& InText)
{
	return InText.Contains(TEXT("{{")) && InText.Contains(TEXT("}}"));
}

static bool HCI_JsonValueContainsVariableTemplate(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		return HCI_StringMayContainVariableTemplate(Value->AsString());
	case EJson::Array:
	{
		for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
		{
			if (HCI_JsonValueContainsVariableTemplate(Item))
			{
				return true;
			}
		}
		return false;
	}
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
		{
			if (HCI_JsonValueContainsVariableTemplate(Pair.Value))
			{
				return true;
			}
		}
		return false;
	}
	default:
		return false;
	}
}

static bool HCI_JsonObjectContainsVariableTemplate(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid())
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
	{
		if (HCI_JsonValueContainsVariableTemplate(Pair.Value))
		{
			return true;
		}
	}
	return false;
}

static bool HCI_TryResolveEvidenceReference(
	const IHCIAbilityKitEvidenceContextView& EvidenceContext,
	const FString& StepId,
	const FString& EvidenceKey,
	const bool bHasIndex,
	const int32 Index,
	FString& OutValue)
{
	OutValue.Reset();

	if (bHasIndex)
	{
		const FString IndexedKey = FString::Printf(TEXT("%s[%d]"), *EvidenceKey, Index);
		if (EvidenceContext.TryGetEvidenceValue(StepId, IndexedKey, OutValue))
		{
			return !OutValue.IsEmpty();
		}

		FString BaseValue;
		if (EvidenceContext.TryGetEvidenceValue(StepId, EvidenceKey, BaseValue))
		{
			TArray<FString> Tokens;
			BaseValue.ParseIntoArray(Tokens, TEXT("|"), true);
			if (Tokens.IsValidIndex(Index))
			{
				OutValue = Tokens[Index];
				return !OutValue.IsEmpty();
			}
		}

		return false;
	}

	return EvidenceContext.TryGetEvidenceValue(StepId, EvidenceKey, OutValue) && !OutValue.IsEmpty();
}

static void HCI_ParsePipeDelimitedEvidenceList(const FString& InText, TArray<FString>& OutValues)
{
	OutValues.Reset();
	if (!InText.Contains(TEXT("|")))
	{
		return;
	}

	TArray<FString> Tokens;
	InText.ParseIntoArray(Tokens, TEXT("|"), true);
	for (FString& Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (!Token.IsEmpty())
		{
			OutValues.Add(Token);
		}
	}
}

static bool HCI_ResolveJsonValueInternal(
	const TSharedPtr<FJsonValue>& InValue,
	const IHCIAbilityKitEvidenceContextView& EvidenceContext,
	TSharedPtr<FJsonValue>& OutValue,
	FHCIAbilityKitEvidenceResolveError& OutError)
{
	if (!InValue.IsValid())
	{
		OutValue = InValue;
		return true;
	}

	switch (InValue->Type)
	{
	case EJson::String:
	{
		FString Raw = InValue->AsString();
		if (!HCI_StringMayContainVariableTemplate(Raw))
		{
			OutValue = MakeShared<FJsonValueString>(Raw);
			return true;
		}

		FString StepId;
		FString EvidenceKey;
		bool bHasIndex = false;
		int32 Index = INDEX_NONE;
		if (!HCI_ParseVariableTemplate(Raw, StepId, EvidenceKey, bHasIndex, Index))
		{
			OutError.ErrorCode = TEXT("E4311");
			OutError.Reason = TEXT("variable_template_invalid");
			OutError.Detail = Raw;
			return false;
		}

		if (!EvidenceContext.HasStep(StepId))
		{
			OutError.ErrorCode = TEXT("E4311");
			OutError.Reason = TEXT("variable_source_step_missing");
			OutError.Detail = StepId;
			return false;
		}

		FString ResolvedValue;
		if (!HCI_TryResolveEvidenceReference(EvidenceContext, StepId, EvidenceKey, bHasIndex, Index, ResolvedValue))
		{
			OutError.ErrorCode = TEXT("E4311");
			OutError.Reason = TEXT("variable_reference_unresolved");
			OutError.Detail = FString::Printf(TEXT("%s.%s"), *StepId, *EvidenceKey);
			return false;
		}

		if (!bHasIndex)
		{
			TArray<FString> ExpandedValues;
			HCI_ParsePipeDelimitedEvidenceList(ResolvedValue, ExpandedValues);
			if (ExpandedValues.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> ExpandedJsonArray;
				ExpandedJsonArray.Reserve(ExpandedValues.Num());
				for (const FString& ExpandedValue : ExpandedValues)
				{
					ExpandedJsonArray.Add(MakeShared<FJsonValueString>(ExpandedValue));
				}
				OutValue = MakeShared<FJsonValueArray>(ExpandedJsonArray);
				return true;
			}
		}

		OutValue = MakeShared<FJsonValueString>(ResolvedValue);
		return true;
	}

	case EJson::Array:
	{
		TArray<TSharedPtr<FJsonValue>> ResolvedArray;
		const TArray<TSharedPtr<FJsonValue>>& InArray = InValue->AsArray();
		ResolvedArray.Reserve(InArray.Num());
		for (const TSharedPtr<FJsonValue>& Item : InArray)
		{
			TSharedPtr<FJsonValue> ResolvedItem;
			if (!HCI_ResolveJsonValueInternal(Item, EvidenceContext, ResolvedItem, OutError))
			{
				return false;
			}
			if (!ResolvedItem.IsValid())
			{
				ResolvedItem = MakeShared<FJsonValueNull>();
			}
			if (ResolvedItem->Type == EJson::Array)
			{
				for (const TSharedPtr<FJsonValue>& ExpandedItem : ResolvedItem->AsArray())
				{
					ResolvedArray.Add(ExpandedItem.IsValid() ? ExpandedItem : MakeShared<FJsonValueNull>());
				}
			}
			else
			{
				ResolvedArray.Add(ResolvedItem);
			}
		}
		OutValue = MakeShared<FJsonValueArray>(ResolvedArray);
		return true;
	}

	case EJson::Object:
	{
		const TSharedPtr<FJsonObject> InObj = InValue->AsObject();
		if (!InObj.IsValid())
		{
			OutValue = InValue;
			return true;
		}

		TSharedPtr<FJsonObject> ResolvedObj = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : InObj->Values)
		{
			TSharedPtr<FJsonValue> ResolvedChild;
			if (!HCI_ResolveJsonValueInternal(Pair.Value, EvidenceContext, ResolvedChild, OutError))
			{
				return false;
			}
			if (!ResolvedChild.IsValid())
			{
				ResolvedChild = MakeShared<FJsonValueNull>();
			}
			ResolvedObj->SetField(Pair.Key, ResolvedChild);
		}

		OutValue = MakeShared<FJsonValueObject>(ResolvedObj);
		return true;
	}

	default:
		OutValue = InValue;
		return true;
	}
}
}

bool FHCIAbilityKitEvidenceResolver_Default::StepArgsMayContainTemplates(const TSharedPtr<FJsonObject>& Args) const
{
	return HCI_JsonObjectContainsVariableTemplate(Args);
}

bool FHCIAbilityKitEvidenceResolver_Default::ResolveStepArgs(
	const FHCIAbilityKitAgentPlanStep& InStep,
	const IHCIAbilityKitEvidenceContextView& EvidenceContext,
	FHCIAbilityKitAgentPlanStep& OutResolvedStep,
	FHCIAbilityKitEvidenceResolveError& OutError) const
{
	OutResolvedStep = InStep;
	OutError.Reset();

	if (!InStep.Args.IsValid())
	{
		return true;
	}

	TSharedPtr<FJsonObject> ResolvedArgs = MakeShared<FJsonObject>();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : InStep.Args->Values)
	{
		TSharedPtr<FJsonValue> ResolvedValue;
		if (!HCI_ResolveJsonValueInternal(Pair.Value, EvidenceContext, ResolvedValue, OutError))
		{
			return false;
		}
		if (!ResolvedValue.IsValid())
		{
			ResolvedValue = MakeShared<FJsonValueNull>();
		}
		ResolvedArgs->SetField(Pair.Key, ResolvedValue);
	}

	OutResolvedStep.Args = MoveTemp(ResolvedArgs);
	return true;
}

