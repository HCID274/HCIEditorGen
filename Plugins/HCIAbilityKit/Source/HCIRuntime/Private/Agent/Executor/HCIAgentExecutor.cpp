#include "Agent/Executor/HCIAgentExecutor.h"

#include "Agent/Executor/HCIAgentExecutionGate.h"
#include "Agent/Executor/Resolver/HCIEvidenceContext_Default.h"
#include "Agent/Executor/Resolver/HCIEvidenceResolver_Default.h"
#include "Common/HCITimeFormat.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAgentExecutor, Log, All);

namespace
{
static bool HCI_TextMayContainVariableTemplate(const FString& InText)
{
	return InText.Contains(TEXT("{{")) && InText.Contains(TEXT("}}"));
}

static bool HCI_TryParseVariableTemplateReference(
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

static bool HCI_PlanContainsVariableTemplate(const FHCIAgentPlan& Plan, const IHCIEvidenceResolver& EvidenceResolver)
{
	for (const FHCIAgentPlanStep& Step : Plan.Steps)
	{
		if (EvidenceResolver.StepArgsMayContainTemplates(Step.Args))
		{
			return true;
		}
	}
	return false;
}

static FString HCI_GetCapabilityString(const FHCIToolDescriptor* Tool)
{
	return Tool ? FHCIToolRegistry::CapabilityToString(Tool->Capability) : FString(TEXT("-"));
}

static bool HCI_IsWriteLike(const FHCIToolDescriptor* Tool)
{
	return Tool && FHCIAgentExecutionGate::IsWriteLikeCapability(Tool->Capability);
}

static FString HCI_TerminationPolicyToString(const EHCIAgentExecutorTerminationPolicy Policy)
{
	switch (Policy)
	{
	case EHCIAgentExecutorTerminationPolicy::ContinueOnFailure:
		return TEXT("continue_on_failure");
	case EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure:
	default:
		return TEXT("stop_on_first_failure");
	}
}

static int32 HCI_GetTargetCountEstimate(const FHCIAgentPlanStep& Step)
{
	if (!Step.Args.IsValid())
	{
		return 0;
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (Step.Args->TryGetArrayField(TEXT("asset_paths"), AssetPaths) && AssetPaths != nullptr)
	{
		return AssetPaths->Num();
	}

	double MaxActorCount = 0.0;
	if (Step.Args->TryGetNumberField(TEXT("max_actor_count"), MaxActorCount))
	{
		return FMath::Max(0, FMath::RoundToInt(MaxActorCount));
	}

	return 0;
}

static FString HCI_TryGetFirstAssetPath(const TSharedPtr<FJsonObject>& Args)
{
	if (!Args.IsValid())
	{
		return FString();
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (!Args->TryGetArrayField(TEXT("asset_paths"), AssetPaths) || AssetPaths == nullptr || AssetPaths->Num() == 0)
	{
		return FString();
	}

	const FString Value = (*AssetPaths)[0].IsValid() ? (*AssetPaths)[0]->AsString() : FString();
	return Value;
}

static FString HCI_BuildEvidenceValue(const FString& EvidenceKey, const FHCIAgentPlanStep& Step)
{
	if (EvidenceKey == TEXT("asset_path"))
	{
		const FString FirstAssetPath = HCI_TryGetFirstAssetPath(Step.Args);
		return FirstAssetPath.IsEmpty() ? TEXT("-") : FirstAssetPath;
	}

	if (EvidenceKey == TEXT("actor_path"))
	{
		return TEXT("/Temp/EditorSelection/DemoActor");
	}

	if (EvidenceKey == TEXT("before"))
	{
		return TEXT("simulated_before");
	}

	if (EvidenceKey == TEXT("after"))
	{
		return TEXT("simulated_after");
	}

	if (EvidenceKey == TEXT("issue"))
	{
		return TEXT("simulated_issue");
	}

	if (EvidenceKey == TEXT("evidence"))
	{
		return TEXT("simulated_evidence");
	}

	if (EvidenceKey == TEXT("result"))
	{
		return TEXT("simulated_success");
	}

	return TEXT("simulated");
}

static void HCI_InitRunResultBase(const FHCIAgentPlan& Plan, FHCIAgentExecutorRunResult& OutResult)
{
	OutResult = FHCIAgentExecutorRunResult();
	OutResult.RequestId = Plan.RequestId;
	OutResult.Intent = Plan.Intent;
	OutResult.PlanVersion = Plan.PlanVersion;
	OutResult.TotalSteps = Plan.Steps.Num();
}

static FHCIAgentExecutorStepResult& HCI_AddStepResultSkeleton(
	const FHCIAgentPlanStep& Step,
	const int32 StepIndex,
	const FHCIToolRegistry& ToolRegistry,
	FHCIAgentExecutorRunResult& OutResult)
{
	const FHCIToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);
	FHCIAgentExecutorStepResult& StepResult = OutResult.StepResults.AddDefaulted_GetRef();
	StepResult.StepIndex = StepIndex;
	StepResult.StepId = Step.StepId;
	StepResult.ToolName = Step.ToolName.ToString();
	StepResult.Capability = HCI_GetCapabilityString(Tool);
	StepResult.RiskLevel = FHCIAgentPlanContract::RiskLevelToString(Step.RiskLevel);
	StepResult.bWriteLike = HCI_IsWriteLike(Tool);
	StepResult.TargetCountEstimate = HCI_GetTargetCountEstimate(Step);
	StepResult.EvidenceKeys = Step.ExpectedEvidence;
	StepResult.FailurePhase = TEXT("-");
	StepResult.PreflightGate = TEXT("-");
	return StepResult;
}

struct FHCIAgentExecutorPreflightDecision
{
	bool bAllowed = true;
	FString FailedGate;
	FString ErrorCode;
	FString Reason;
};

static FHCIAgentExecutorPreflightDecision HCI_EvaluateExecutorPreflight(
	const FHCIAgentPlan& Plan,
	const FHCIAgentPlanStep& Step,
	const FHCIAgentExecutorStepResult& StepResult,
	const FHCIToolRegistry& ToolRegistry,
	const FHCIAgentExecutorOptions& Options)
{
	FHCIAgentExecutorPreflightDecision Preflight;
	if (!Options.bEnablePreflightGates)
	{
		return Preflight;
	}

	const FHCIAgentExecutionDecision ConfirmDecision = FHCIAgentExecutionGate::EvaluateConfirmGate(
		{Step.StepId, Step.ToolName, Step.bRequiresConfirm, Options.bUserConfirmedWriteSteps},
		ToolRegistry);
	if (!ConfirmDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("confirm_gate");
		Preflight.ErrorCode = ConfirmDecision.ErrorCode;
		Preflight.Reason = ConfirmDecision.Reason;
		return Preflight;
	}

	const FHCIAgentBlastRadiusDecision BlastDecision = FHCIAgentExecutionGate::EvaluateBlastRadius(
		{Plan.RequestId, Step.ToolName, StepResult.TargetCountEstimate},
		ToolRegistry);
	if (!BlastDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("blast_radius");
		Preflight.ErrorCode = BlastDecision.ErrorCode;
		Preflight.Reason = BlastDecision.Reason;
		return Preflight;
	}

	const FHCIAgentMockRbacDecision RbacDecision = FHCIAgentExecutionGate::EvaluateMockRbac(
		{
			Plan.RequestId,
			Options.MockUserName,
			Options.MockResolvedRole,
			Options.bMockUserMatchedWhitelist,
			Step.ToolName,
			StepResult.TargetCountEstimate,
			Options.MockAllowedCapabilities},
		ToolRegistry);
	if (!RbacDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("rbac");
		Preflight.ErrorCode = RbacDecision.ErrorCode;
		Preflight.Reason = RbacDecision.Reason;
		return Preflight;
	}

	const FHCIAgentSourceControlDecision SourceControlDecision = FHCIAgentExecutionGate::EvaluateSourceControlFailFast(
		{Plan.RequestId, Step.ToolName, Options.bSourceControlEnabled, Options.bSourceControlCheckoutSucceeded},
		ToolRegistry);
	if (!SourceControlDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("source_control");
		Preflight.ErrorCode = SourceControlDecision.ErrorCode;
		Preflight.Reason = SourceControlDecision.Reason;
		return Preflight;
	}

	const FHCIAgentLodToolSafetyDecision LodSafetyDecision = FHCIAgentExecutionGate::EvaluateLodToolSafety(
		{Plan.RequestId, Step.ToolName, Options.SimulatedLodTargetObjectClass, Options.bSimulatedLodTargetNaniteEnabled},
		ToolRegistry);
	if (!LodSafetyDecision.bAllowed)
	{
		Preflight.bAllowed = false;
		Preflight.FailedGate = TEXT("lod_safety");
		Preflight.ErrorCode = LodSafetyDecision.ErrorCode;
		Preflight.Reason = LodSafetyDecision.Reason;
		return Preflight;
	}

	return Preflight;
}

static bool HCI_TryRunToolAction(
	const FHCIAgentPlan& Plan,
	const FHCIAgentPlanStep& Step,
	const FHCIAgentExecutorOptions& Options,
	FHCIAgentExecutorStepResult& OutStepResult)
{
	const TSharedPtr<IHCIAgentToolAction>* FoundAction = Options.ToolActions.Find(Step.ToolName);
	if (FoundAction == nullptr || !FoundAction->IsValid())
	{
		return false;
	}

	FHCIAgentToolActionRequest ActionRequest;
	ActionRequest.RequestId = Plan.RequestId;
	ActionRequest.StepId = Step.StepId;
	ActionRequest.ToolName = Step.ToolName;
	ActionRequest.Args = Step.Args;

	FHCIAgentToolActionResult ActionResult;
	const bool bCallOk = Options.bDryRun
		? (*FoundAction)->DryRun(ActionRequest, ActionResult)
		: (*FoundAction)->Execute(ActionRequest, ActionResult);

	OutStepResult.TargetCountEstimate = FMath::Max(OutStepResult.TargetCountEstimate, ActionResult.EstimatedAffectedCount);
	OutStepResult.Evidence = MoveTemp(ActionResult.Evidence);
	OutStepResult.bSucceeded = bCallOk && ActionResult.bSucceeded;
	OutStepResult.Status = OutStepResult.bSucceeded ? TEXT("succeeded") : TEXT("failed");
	OutStepResult.ErrorCode = ActionResult.ErrorCode;
	OutStepResult.Reason = ActionResult.Reason;
	OutStepResult.FailurePhase = OutStepResult.bSucceeded ? TEXT("-") : TEXT("execute");
	return true;
}

static bool HCI_TryValidateStepWithResolvedPrefix(
	const FHCIAgentPlan& Plan,
	const TArray<FHCIAgentPlanStep>& ResolvedPrefixSteps,
	const FHCIAgentPlanStep& CurrentResolvedStep,
	const FHCIToolRegistry& ToolRegistry,
	const FHCIAgentPlanValidationContext& ValidationContext,
	FHCIAgentPlanValidationResult& OutValidation)
{
	FHCIAgentPlan PrefixPlan;
	PrefixPlan.PlanVersion = Plan.PlanVersion;
	PrefixPlan.RequestId = Plan.RequestId;
	PrefixPlan.Intent = Plan.Intent;
	PrefixPlan.Steps = ResolvedPrefixSteps;
	PrefixPlan.Steps.Add(CurrentResolvedStep);
	return FHCIAgentPlanValidator::ValidatePlan(PrefixPlan, ToolRegistry, ValidationContext, OutValidation);
}

static bool HCI_IsDirectoryLikeArgName(const FString& ArgName)
{
	return ArgName.Equals(TEXT("directory"), ESearchCase::CaseSensitive) ||
		   ArgName.Equals(TEXT("target_root"), ESearchCase::CaseSensitive) ||
		   ArgName.Equals(TEXT("target_path"), ESearchCase::CaseSensitive);
}

static bool HCI_SearchStepHasConsumableDirectoryEvidence(const TMap<FString, FString>& Evidence)
{
	const FString* BestDirectory = Evidence.Find(TEXT("best_directory"));
	if (BestDirectory != nullptr)
	{
		const FString BestTrimmed = BestDirectory->TrimStartAndEnd();
		if (!BestTrimmed.IsEmpty() && !BestTrimmed.Equals(TEXT("-"), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	const FString* MatchedDirectories = Evidence.Find(TEXT("matched_directories"));
	if (MatchedDirectories != nullptr && !MatchedDirectories->TrimStartAndEnd().IsEmpty())
	{
		return true;
	}

	for (const TPair<FString, FString>& Pair : Evidence)
	{
		if (!Pair.Key.StartsWith(TEXT("matched_directories["), ESearchCase::CaseSensitive))
		{
			continue;
		}
		if (!Pair.Value.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}
	}

	return false;
}

static bool HCI_TryDetectPipelineBypassWarning(
	const FHCIAgentPlan& Plan,
	const int32 StepIndex,
	const FHCIAgentPlanStep& OriginalStep,
	const FHCIAgentPlanStep& ResolvedStep,
	const TMap<FString, TMap<FString, FString>>& StepEvidenceContext,
	FString& OutDetail)
{
	OutDetail.Reset();
	if (StepIndex <= 0)
	{
		return false;
	}
	if (!OriginalStep.Args.IsValid() || !ResolvedStep.Args.IsValid())
	{
		return false;
	}

	const FHCIAgentPlanStep& PrevStep = Plan.Steps[StepIndex - 1];
	if (PrevStep.ToolName != TEXT("SearchPath") || PrevStep.StepId.IsEmpty())
	{
		return false;
	}

	const TMap<FString, FString>* PrevEvidence = StepEvidenceContext.Find(PrevStep.StepId);
	if (PrevEvidence == nullptr)
	{
		return false;
	}

	if (!HCI_SearchStepHasConsumableDirectoryEvidence(*PrevEvidence))
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& ResolvedArg : ResolvedStep.Args->Values)
	{
		if (!HCI_IsDirectoryLikeArgName(ResolvedArg.Key))
		{
			continue;
		}
		if (!ResolvedArg.Value.IsValid() || ResolvedArg.Value->Type != EJson::String)
		{
			continue;
		}

		const TSharedPtr<FJsonValue>* RawArgValue = OriginalStep.Args->Values.Find(ResolvedArg.Key);
		const FString RawText = (RawArgValue != nullptr && RawArgValue->IsValid() && (*RawArgValue)->Type == EJson::String)
			? (*RawArgValue)->AsString().TrimStartAndEnd()
			: FString();
		if (RawText.IsEmpty())
		{
			continue;
		}

		const bool bUsesPipelineTemplate = HCI_TextMayContainVariableTemplate(RawText);
		if (bUsesPipelineTemplate)
		{
			FString SourceStepId;
			FString SourceEvidenceKey;
			bool bHasIndex = false;
			int32 SourceIndex = INDEX_NONE;
			if (HCI_TryParseVariableTemplateReference(RawText, SourceStepId, SourceEvidenceKey, bHasIndex, SourceIndex) &&
				SourceStepId.Equals(PrevStep.StepId, ESearchCase::CaseSensitive) &&
				(SourceEvidenceKey.Equals(TEXT("matched_directories"), ESearchCase::CaseSensitive) ||
				 SourceEvidenceKey.Equals(TEXT("best_directory"), ESearchCase::CaseSensitive)))
			{
				continue;
			}
		}

		const FString ExpectedTemplate = FString::Printf(TEXT("{{%s.matched_directories[0]}}"), *PrevStep.StepId);
		const int32 PrevStepNumber = StepIndex; // current StepIndex is 0-based, previous step number is StepIndex (1-based).
		OutDetail = FString::Printf(
			TEXT("Pipe variable from Step %d (SearchPath) is defined but not consumed by subsequent steps. arg=%s expected_template=%s raw_value=%s"),
			PrevStepNumber,
			*ResolvedArg.Key,
			*ExpectedTemplate,
			*RawText);
		return true;
	}

	return false;
}
}

bool FHCIAgentExecutor::ExecutePlan(
	const FHCIAgentPlan& Plan,
	const FHCIToolRegistry& ToolRegistry,
	FHCIAgentExecutorRunResult& OutResult)
{
	const FHCIAgentPlanValidationContext DefaultValidationContext;
	const FHCIAgentExecutorOptions DefaultOptions;
	return ExecutePlan(Plan, ToolRegistry, DefaultValidationContext, DefaultOptions, OutResult);
}

bool FHCIAgentExecutor::ExecutePlan(
	const FHCIAgentPlan& Plan,
	const FHCIToolRegistry& ToolRegistry,
	const FHCIAgentPlanValidationContext& ValidationContext,
	const FHCIAgentExecutorOptions& Options,
	FHCIAgentExecutorRunResult& OutResult)
{
	HCI_InitRunResultBase(Plan, OutResult);
	OutResult.ExecutionMode = Options.bDryRun ? TEXT("simulate_dry_run") : TEXT("execute_apply");
	OutResult.TerminationPolicy = HCI_TerminationPolicyToString(Options.TerminationPolicy);
	OutResult.bPreflightEnabled = Options.bEnablePreflightGates;
	OutResult.StartedAtUtc = FHCITimeFormat::FormatNowBeijingIso8601();

	const FHCIEvidenceResolver_Default EvidenceResolver;
	const bool bContainsVariableTemplate = HCI_PlanContainsVariableTemplate(Plan, EvidenceResolver);
	const bool bUsePerStepValidation = Options.bValidatePlanBeforeExecute && bContainsVariableTemplate;

	if (Options.bValidatePlanBeforeExecute && !bUsePerStepValidation)
	{
		FHCIAgentPlanValidationResult ValidationResult;
		if (!FHCIAgentPlanValidator::ValidatePlan(Plan, ToolRegistry, ValidationContext, ValidationResult))
		{
			OutResult.bAccepted = false;
			OutResult.bCompleted = false;
			OutResult.ErrorCode = ValidationResult.ErrorCode;
			OutResult.Reason = ValidationResult.Reason;
			OutResult.TerminalStatus = TEXT("rejected_precheck");
			OutResult.TerminalReason = TEXT("validator_rejected_plan");
			OutResult.FailedStepIndex = ValidationResult.FailedStepIndex;
			OutResult.FailedStepId = ValidationResult.FailedStepId;
			OutResult.FailedToolName = ValidationResult.FailedToolName;
			OutResult.FinishedAtUtc = FHCITimeFormat::FormatNowBeijingIso8601();
			return false;
		}
	}

	OutResult.bAccepted = true;
	OutResult.StepResults.Reserve(Plan.Steps.Num());
	bool bSawFailure = false;
	bool bAnySimulatedStep = false;
	bool bAnyToolActionExecuted = false;
	TMap<FString, TMap<FString, FString>> StepEvidenceContext;
	TArray<FHCIAgentPlanStep> ResolvedValidatedPrefixSteps;
	ResolvedValidatedPrefixSteps.Reserve(Plan.Steps.Num());

	for (int32 StepIndex = 0; StepIndex < Plan.Steps.Num(); ++StepIndex)
	{
		const FHCIAgentPlanStep& Step = Plan.Steps[StepIndex];
		if (Options.OnStepBegin)
		{
			Options.OnStepBegin(StepIndex, Step);
		}
		FHCIAgentPlanStep ResolvedStep;
		const FHCIEvidenceContext_Default EvidenceContext(StepEvidenceContext);
		FHCIEvidenceResolveError ResolveError;
		const bool bResolveOk = EvidenceResolver.ResolveStepArgs(Step, EvidenceContext, ResolvedStep, ResolveError);

		const FHCIToolDescriptor* Tool = ToolRegistry.FindTool(Step.ToolName);
		FHCIAgentExecutorStepResult& StepResult = HCI_AddStepResultSkeleton(ResolvedStep, StepIndex, ToolRegistry, OutResult);
		StepResult.bAttempted = true;
		bool bHasPipelineBypassWarning = false;
		FString PipelineBypassWarningDetail;
		if (bResolveOk)
		{
			bHasPipelineBypassWarning = HCI_TryDetectPipelineBypassWarning(
				Plan,
				StepIndex,
				Step,
				ResolvedStep,
				StepEvidenceContext,
				PipelineBypassWarningDetail);
		}

		if (!bResolveOk)
		{
			StepResult.bSucceeded = false;
			StepResult.Status = TEXT("failed");
			StepResult.ErrorCode = ResolveError.ErrorCode.IsEmpty() ? TEXT("E4311") : ResolveError.ErrorCode;
			StepResult.Reason = ResolveError.Reason.IsEmpty() ? TEXT("resolve_arguments_failed") : ResolveError.Reason;
			StepResult.FailurePhase = TEXT("precheck");
		}
		else if (bUsePerStepValidation)
		{
			FHCIAgentPlanValidationResult ValidationResult;
			if (!HCI_TryValidateStepWithResolvedPrefix(
					Plan,
					ResolvedValidatedPrefixSteps,
					ResolvedStep,
					ToolRegistry,
					ValidationContext,
					ValidationResult))
			{
				StepResult.bSucceeded = false;
				StepResult.Status = TEXT("failed");
				StepResult.ErrorCode = ValidationResult.ErrorCode;
				StepResult.Reason = ValidationResult.Reason;
				StepResult.FailurePhase = TEXT("precheck");
			}
			else
			{
				ResolvedValidatedPrefixSteps.Add(ResolvedStep);
			}
		}

		if (StepResult.Status == TEXT("failed"))
		{
			// precheck failed; continue to unified failure convergence branch below
		}
		else if (Tool == nullptr)
		{
			StepResult.bSucceeded = false;
			StepResult.Status = TEXT("failed");
			StepResult.ErrorCode = TEXT("E4002");
			StepResult.Reason = TEXT("tool_not_whitelisted");
			StepResult.FailurePhase = TEXT("execute");
		}
			else
			{
				const FHCIAgentExecutorPreflightDecision PreflightDecision =
					HCI_EvaluateExecutorPreflight(Plan, ResolvedStep, StepResult, ToolRegistry, Options);
			if (!PreflightDecision.bAllowed)
			{
				StepResult.bSucceeded = false;
				StepResult.Status = TEXT("failed");
				StepResult.ErrorCode = PreflightDecision.ErrorCode;
				StepResult.Reason = PreflightDecision.Reason;
				StepResult.FailurePhase = TEXT("preflight");
				StepResult.PreflightGate = PreflightDecision.FailedGate.IsEmpty() ? TEXT("-") : PreflightDecision.FailedGate;
				OutResult.PreflightBlockedSteps += 1;
			}
			else if (Options.SimulatedFailureStepIndex == StepIndex)
			{
				StepResult.bSucceeded = false;
				StepResult.Status = TEXT("failed");
				StepResult.ErrorCode = Options.SimulatedFailureErrorCode.IsEmpty() ? TEXT("E4101") : Options.SimulatedFailureErrorCode;
				StepResult.Reason = Options.SimulatedFailureReason.IsEmpty() ? TEXT("simulated_tool_execution_failed") : Options.SimulatedFailureReason;
				StepResult.FailurePhase = TEXT("execute");
				StepResult.PreflightGate = Options.bEnablePreflightGates ? TEXT("passed") : TEXT("-");
			}
			else
			{
					const bool bHandledByAction = HCI_TryRunToolAction(Plan, ResolvedStep, Options, StepResult);
					if (bHandledByAction && !Options.bDryRun)
					{
						bAnyToolActionExecuted = true;
					}
					if (bHandledByAction && !StepResult.bSucceeded)
					{
						// Action returned a failed dry-run/execute result.
				}
				else if (bHandledByAction)
				{
					if (StepResult.Reason.IsEmpty())
					{
						StepResult.Reason = Options.bDryRun ? TEXT("tool_action_dry_run_success") : TEXT("tool_action_execute_success");
					}
					StepResult.PreflightGate = Options.bEnablePreflightGates ? TEXT("passed") : TEXT("-");
				}
				else
				{
					for (const FString& EvidenceKey : Step.ExpectedEvidence)
					{
						StepResult.Evidence.Add(EvidenceKey, HCI_BuildEvidenceValue(EvidenceKey, ResolvedStep));
					}

						StepResult.bSucceeded = true;
						StepResult.Status = TEXT("succeeded");
						StepResult.Reason = Options.bDryRun ? TEXT("simulated_dry_run_success") : TEXT("simulated_apply_success");
						StepResult.PreflightGate = Options.bEnablePreflightGates ? TEXT("passed") : TEXT("-");
						bAnySimulatedStep = true;
					}
					}
				}

			if (bHasPipelineBypassWarning)
			{
				StepResult.Evidence.Add(TEXT("planning_warning_code"), TEXT("W5101"));
				StepResult.Evidence.Add(TEXT("planning_warning_reason"), TEXT("planner_pipeline_variable_not_used_after_search"));
				StepResult.Evidence.Add(
					TEXT("planning_warning_detail"),
					PipelineBypassWarningDetail.IsEmpty()
						? TEXT("Pipe variable from previous SearchPath step is defined but not consumed.")
						: PipelineBypassWarningDetail);
				if (StepResult.bSucceeded)
				{
					StepResult.bSucceeded = false;
					StepResult.Status = TEXT("failed");
					StepResult.ErrorCode = TEXT("E4009");
					StepResult.Reason = TEXT("planner_pipeline_variable_not_used_after_search");
					StepResult.FailurePhase = TEXT("precheck");
				}
				UE_LOG(
					LogHCIAgentExecutor,
					Warning,
					TEXT("[HCI][AgentExecutor] error_code=E4009 warning_code=W5101 step_id=%s tool=%s reason=planner_pipeline_variable_not_used_after_search detail=%s"),
					*StepResult.StepId,
					*StepResult.ToolName,
					PipelineBypassWarningDetail.IsEmpty()
						? TEXT("Pipe variable from previous SearchPath step is defined but not consumed.")
						: *PipelineBypassWarningDetail);
			}

			if (StepResult.bSucceeded)
			{
			if (!StepResult.StepId.IsEmpty())
			{
				StepEvidenceContext.Add(StepResult.StepId, StepResult.Evidence);
			}
			OutResult.SucceededSteps += 1;
			continue;
		}

		bSawFailure = true;
		OutResult.FailedSteps += 1;
		if (OutResult.FailedStepIndex == INDEX_NONE)
		{
			OutResult.FailedStepIndex = StepIndex;
			OutResult.FailedStepId = Step.StepId;
			OutResult.FailedToolName = Step.ToolName.ToString();
			OutResult.FailedGate = StepResult.FailurePhase == TEXT("preflight") ? StepResult.PreflightGate : FString(TEXT("-"));
			OutResult.ErrorCode = StepResult.ErrorCode;
			OutResult.Reason = StepResult.Reason;
		}

		if (Options.TerminationPolicy == EHCIAgentExecutorTerminationPolicy::StopOnFirstFailure)
		{
			OutResult.ExecutedSteps = StepIndex + 1;
			OutResult.SkippedSteps = FMath::Max(0, Plan.Steps.Num() - OutResult.ExecutedSteps);

			for (int32 RemainingIndex = StepIndex + 1; RemainingIndex < Plan.Steps.Num(); ++RemainingIndex)
			{
				const FHCIAgentPlanStep& RemainingStep = Plan.Steps[RemainingIndex];
				FHCIAgentExecutorStepResult& SkippedRow =
					HCI_AddStepResultSkeleton(RemainingStep, RemainingIndex, ToolRegistry, OutResult);
				SkippedRow.bAttempted = false;
				SkippedRow.bSucceeded = false;
				SkippedRow.Status = TEXT("skipped");
				SkippedRow.Reason = TEXT("terminated_by_stop_on_first_failure");
			}

			OutResult.bCompleted = false;
			OutResult.TerminalStatus = TEXT("failed");
			OutResult.TerminalReason =
				(StepResult.FailurePhase == TEXT("preflight"))
					? TEXT("executor_preflight_gate_failed_stop_on_first_failure")
					: TEXT("executor_step_failed_stop_on_first_failure");
			OutResult.FinishedAtUtc = FHCITimeFormat::FormatNowBeijingIso8601();
			return false;
		}
	}

	OutResult.ExecutedSteps = OutResult.TotalSteps;
	OutResult.SkippedSteps = 0;
	if (bSawFailure)
	{
		OutResult.bCompleted = false;
		OutResult.TerminalStatus = TEXT("completed_with_failures");
		OutResult.TerminalReason = (OutResult.PreflightBlockedSteps > 0)
									 ? TEXT("executor_preflight_gate_failed_continue_on_failure")
									 : TEXT("executor_step_failed_continue_on_failure");
		OutResult.FinishedAtUtc = FHCITimeFormat::FormatNowBeijingIso8601();
		return false;
	}

	OutResult.bCompleted = true;
	OutResult.TerminalStatus = TEXT("completed");
	if (Options.bDryRun)
	{
		OutResult.TerminalReason = TEXT("executor_simulated_dry_run_completed");
	}
	else if (bAnySimulatedStep || !bAnyToolActionExecuted)
	{
		OutResult.TerminalReason = TEXT("executor_execute_completed_with_simulated_steps");
	}
	else
	{
		OutResult.TerminalReason = TEXT("executor_execute_completed");
	}
	OutResult.FinishedAtUtc = FHCITimeFormat::FormatNowBeijingIso8601();
	return true;
}

