#pragma once

#include "CoreMinimal.h"
#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"
#include "Agent/Executor/HCIAbilityKitAgentExecutor.h"
#include "Commands/HCIAbilityKitAgentCommandBase.h"
#include "EditorSubsystem.h"
#include "HCIAbilityKitAgentSubsystem.generated.h"

class UHCIAbilityKitAgentCommandBase;

struct FHCIAbilityKitAgentUiProgressState
{
	bool bVisible = false;
	bool bIndeterminate = false;
	float Percent01 = 0.0f;
	FString Label;
};

enum class EHCIAbilityKitAgentUiLocateTargetKind : uint8
{
	Asset,
	Actor
};

struct FHCIAbilityKitAgentUiLocateTarget
{
	EHCIAbilityKitAgentUiLocateTargetKind Kind = EHCIAbilityKitAgentUiLocateTargetKind::Asset;
	FString DisplayLabel;
	FString TargetPath;
	FString SourceToolName;
	FString SourceEvidenceKey;
};

struct FHCIAbilityKitAgentQuickCommand
{
	FString Label;
	FString Prompt;
};

UENUM()
enum class EHCIAbilityKitAgentSessionState : uint8
{
	Idle,
	Thinking,
	PlanReady,
	AutoExecuteReadOnly,
	AwaitUserConfirm,
	Executing,
	Summarizing,
	Completed,
	Failed,
	Cancelled
};

UENUM()
enum class EHCIAbilityKitAgentPlanExecutionBranch : uint8
{
	AutoExecuteReadOnly,
	AwaitUserConfirm
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentChatLineEvent, const FString& /*Line*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentStatusEvent, const FString& /*StatusText*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentSummaryEvent, const FString& /*SummaryText*/);
DECLARE_MULTICAST_DELEGATE(FHCIAbilityKitAgentPlanReadyEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentSessionStateEvent, EHCIAbilityKitAgentSessionState /*State*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentProgressStateEvent, const FHCIAbilityKitAgentUiProgressState& /*Progress*/);
DECLARE_MULTICAST_DELEGATE(FHCIAbilityKitAgentLocateTargetsChangedEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentActivityHintEvent, const FString& /*HintText*/);

struct FHCIAbilityKitAgentUiApprovalCard
{
	FString Title;
	FString KeyAction;
	FString ImpactHint;
	FString Warning;
};

/**
 * Agent 聊天编排入口：统一受理 UI 输入并分发命令。
 */
UCLASS()
class HCIABILITYKITEDITOR_API UHCIAbilityKitAgentSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool SubmitChatInput(const FString& UserInput, const FString& SourceTag = TEXT("AgentChatUI"));
	bool IsBusy() const;
	bool HasLastPlan() const;
	EHCIAbilityKitAgentSessionState GetCurrentState() const;
	FString GetCurrentStateLabel() const;
	bool CanCommitLastPlanFromChat() const;
	bool CanCancelPendingPlanFromChat() const;
	bool BuildLastPlanCardLines(TArray<FString>& OutLines) const;
	bool BuildLastPlanApprovalCard(FHCIAbilityKitAgentUiApprovalCard& OutCard) const;
	void GetCurrentProgressState(FHCIAbilityKitAgentUiProgressState& OutState) const;
	void GetCurrentActivityHint(FString& OutHint) const;
	void GetLastExecutionLocateTargets(TArray<FHCIAbilityKitAgentUiLocateTarget>& OutTargets) const;
	bool OpenLastPlanPreview();
	bool CommitLastPlanFromChat();
	bool CancelPendingPlanFromChat();
	bool TryLocateLastExecutionTargetByIndex(int32 TargetIndex);

	void ReloadQuickCommands();
	const TArray<FHCIAbilityKitAgentQuickCommand>& GetQuickCommands() const;
	const FString& GetQuickCommandsLoadError() const;

	FHCIAbilityKitAgentChatLineEvent OnChatLine;
	FHCIAbilityKitAgentStatusEvent OnStatusChanged;
	FHCIAbilityKitAgentSummaryEvent OnSummaryReceived;
	FHCIAbilityKitAgentPlanReadyEvent OnPlanReady;
	FHCIAbilityKitAgentSessionStateEvent OnSessionStateChanged;
	FHCIAbilityKitAgentProgressStateEvent OnProgressStateChanged;
	FHCIAbilityKitAgentLocateTargetsChangedEvent OnLocateTargetsChanged;
	FHCIAbilityKitAgentActivityHintEvent OnActivityHintChanged;

	static bool IsWriteLikePlan(const FHCIAbilityKitAgentPlan& Plan);
	static EHCIAbilityKitAgentPlanExecutionBranch ClassifyPlanExecutionBranch(const FHCIAbilityKitAgentPlan& Plan);
	static FString BuildStepDisplaySummaryForUi(const FHCIAbilityKitAgentPlanStep& Step);
	static FString BuildStepIntentReasonForUi(const FHCIAbilityKitAgentPlanStep& Step);
	static FString BuildStepRiskWarningForUi(const FHCIAbilityKitAgentPlanStep& Step);
	static FString BuildStepImpactHintForUi(const FHCIAbilityKitAgentPlanStep& Step);

private:
	void EmitUserLine(const FString& Text);
	void EmitAssistantLine(const FString& Text);
	void EmitStatus(const FString& Text);
	void SetCurrentState(EHCIAbilityKitAgentSessionState NewState);
	FString BuildStateStatusText(EHCIAbilityKitAgentSessionState State) const;
	void SetProgressState(const FHCIAbilityKitAgentUiProgressState& InState);
	void SetActivityHint(const FString& InHint);
	void ClearLocateTargets();
	void SetLocateTargetsFromExecutionReport(const struct FHCIAbilityKitAgentPlanExecutionReport& Report);
	bool ExecuteLastPlan(EHCIAbilityKitAgentPlanExecutionBranch Branch, const TCHAR* TriggerTag);

	void HandleCommandCompleted(const FHCIAbilityKitAgentCommandResult& Result);
	bool ExecuteRegisteredCommand(const FName& CommandName, const FHCIAbilityKitAgentCommandContext& Context);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHCIAbilityKitAgentCommandBase> ActiveCommand = nullptr;

	// Last chat request snapshot (used for auto-repair retry when ScanAssets returns empty due to dirty path input).
	FString LastChatTrimmedUserInput;
	FString LastChatEffectiveInput;
	FString LastChatSourceTag;
	FString LastChatExtraEnvContextText;
	int32 LastChatAutoRepairAttempts = 0;

	TMap<FName, TSubclassOf<UHCIAbilityKitAgentCommandBase>> CommandRegistry;
	TArray<FHCIAbilityKitAgentQuickCommand> QuickCommands;
	FString QuickCommandsLoadError;
	bool bHasLastPlan = false;
	FHCIAbilityKitAgentPlan LastPlan;
	FString LastRouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata LastPlannerMetadata;
	EHCIAbilityKitAgentSessionState CurrentState = EHCIAbilityKitAgentSessionState::Idle;
	FHCIAbilityKitAgentUiProgressState CurrentProgressState;
	FString CurrentActivityHint;
	TArray<FHCIAbilityKitAgentUiLocateTarget> LastExecutionLocateTargets;

	// Cached dry-run step results used to enrich the approval card with concrete routing proposals.
	bool bHasApprovalPreview = false;
	TArray<FHCIAbilityKitAgentExecutorStepResult> ApprovalPreviewStepResults;
};
