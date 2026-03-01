#pragma once

#include "CoreMinimal.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "Agent/Executor/HCIAgentExecutor.h"
#include "Commands/HCIAgentCommandBase.h"
#include "EditorSubsystem.h"
#include "HCIAgentSubsystem.generated.h"

class UHCIAgentCommandBase;
class UHCIAgentSessionController;

struct FHCIAgentUiProgressState
{
	bool bVisible = false;
	bool bIndeterminate = false;
	float Percent01 = 0.0f;
	FString Label;
};

enum class EHCIAgentUiLocateTargetKind : uint8
{
	Asset,
	Actor
};

struct FHCIAgentUiLocateTarget
{
	EHCIAgentUiLocateTargetKind Kind = EHCIAgentUiLocateTargetKind::Asset;
	FString DisplayLabel;
	FString TargetPath;
	FString Detail;
	FString SourceToolName;
	FString SourceEvidenceKey;
};

struct FHCIAgentQuickCommand
{
	FString Label;
	FString Prompt;
};

UENUM()
enum class EHCIAgentSessionState : uint8
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
enum class EHCIAgentPlanExecutionBranch : uint8
{
	AutoExecuteReadOnly,
	AwaitUserConfirm
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAgentChatLineEvent, const FString& /*Line*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAgentStatusEvent, const FString& /*StatusText*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAgentSummaryEvent, const FString& /*SummaryText*/);
DECLARE_MULTICAST_DELEGATE(FHCIAgentPlanReadyEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAgentSessionStateEvent, EHCIAgentSessionState /*State*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAgentProgressStateEvent, const FHCIAgentUiProgressState& /*Progress*/);
DECLARE_MULTICAST_DELEGATE(FHCIAgentLocateTargetsChangedEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAgentActivityHintEvent, const FString& /*HintText*/);

struct FHCIAgentUiApprovalCard
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
class HCIEDITOR_API UHCIAgentSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool SubmitChatInput(const FString& UserInput, const FString& SourceTag = TEXT("AgentChatUI"));
	bool IsBusy() const;
	bool HasLastPlan() const;
	EHCIAgentSessionState GetCurrentState() const;
	FString GetCurrentStateLabel() const;
	bool CanCommitLastPlanFromChat() const;
	bool CanCancelPendingPlanFromChat() const;
	bool BuildLastPlanCardLines(TArray<FString>& OutLines) const;
	bool BuildLastPlanApprovalCard(FHCIAgentUiApprovalCard& OutCard) const;
	void GetCurrentProgressState(FHCIAgentUiProgressState& OutState) const;
	void GetCurrentActivityHint(FString& OutHint) const;
	void GetLastExecutionLocateTargets(TArray<FHCIAgentUiLocateTarget>& OutTargets) const;
	bool OpenLastPlanPreview();
	bool CommitLastPlanFromChat();
	bool CancelPendingPlanFromChat();
	bool TryLocateLastExecutionTargetByIndex(int32 TargetIndex);

	void ReloadQuickCommands();
	const TArray<FHCIAgentQuickCommand>& GetQuickCommands() const;
	const FString& GetQuickCommandsLoadError() const;

	FHCIAgentChatLineEvent OnChatLine;
	FHCIAgentStatusEvent OnStatusChanged;
	FHCIAgentSummaryEvent OnSummaryReceived;
	FHCIAgentPlanReadyEvent OnPlanReady;
	FHCIAgentSessionStateEvent OnSessionStateChanged;
	FHCIAgentProgressStateEvent OnProgressStateChanged;
	FHCIAgentLocateTargetsChangedEvent OnLocateTargetsChanged;
	FHCIAgentActivityHintEvent OnActivityHintChanged;

	static bool IsWriteLikePlan(const FHCIAgentPlan& Plan);
	static EHCIAgentPlanExecutionBranch ClassifyPlanExecutionBranch(const FHCIAgentPlan& Plan);
	static FString BuildStepDisplaySummaryForUi(const FHCIAgentPlanStep& Step);
	static FString BuildStepIntentReasonForUi(const FHCIAgentPlanStep& Step);
	static FString BuildStepRiskWarningForUi(const FHCIAgentPlanStep& Step);
	static FString BuildStepImpactHintForUi(const FHCIAgentPlanStep& Step);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHCIAgentSessionController> SessionController = nullptr;
};


