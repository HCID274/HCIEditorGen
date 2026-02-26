#pragma once

#include "CoreMinimal.h"
#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"
#include "Commands/HCIAbilityKitAgentCommandBase.h"
#include "EditorSubsystem.h"
#include "HCIAbilityKitAgentSubsystem.generated.h"

class UHCIAbilityKitAgentCommandBase;

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
	bool OpenLastPlanPreview();
	bool CommitLastPlanFromChat();
	bool CancelPendingPlanFromChat();

	void ReloadQuickCommands();
	const TArray<FHCIAbilityKitAgentQuickCommand>& GetQuickCommands() const;
	const FString& GetQuickCommandsLoadError() const;

	FHCIAbilityKitAgentChatLineEvent OnChatLine;
	FHCIAbilityKitAgentStatusEvent OnStatusChanged;
	FHCIAbilityKitAgentSummaryEvent OnSummaryReceived;
	FHCIAbilityKitAgentPlanReadyEvent OnPlanReady;
	FHCIAbilityKitAgentSessionStateEvent OnSessionStateChanged;

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
	bool ExecuteLastPlan(EHCIAbilityKitAgentPlanExecutionBranch Branch, const TCHAR* TriggerTag);

	void HandleCommandCompleted(const FHCIAbilityKitAgentCommandResult& Result);
	bool ExecuteRegisteredCommand(const FName& CommandName, const FHCIAbilityKitAgentCommandContext& Context);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHCIAbilityKitAgentCommandBase> ActiveCommand = nullptr;

	TMap<FName, TSubclassOf<UHCIAbilityKitAgentCommandBase>> CommandRegistry;
	TArray<FHCIAbilityKitAgentQuickCommand> QuickCommands;
	FString QuickCommandsLoadError;
	bool bHasLastPlan = false;
	FHCIAbilityKitAgentPlan LastPlan;
	FString LastRouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata LastPlannerMetadata;
	EHCIAbilityKitAgentSessionState CurrentState = EHCIAbilityKitAgentSessionState::Idle;
};
