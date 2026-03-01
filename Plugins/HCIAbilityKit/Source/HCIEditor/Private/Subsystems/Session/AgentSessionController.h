#pragma once

#include "CoreMinimal.h"
#include "Subsystems/HCIAgentSubsystem.h"
#include "UObject/Object.h"

#include "AgentSessionController.generated.h"

/**
 * Agent 会话控制器（深模块）：
 * - 负责聊天会话状态机、命令执行编排、计划执行与结果归档。
 * - Subsystem 仅作为 Facade：生命周期 + 事件派发 + UI 入口。
 */
UCLASS(Transient)
class UHCIAgentSessionController : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHCIAgentSubsystem& InOwner);
	void Deinitialize();

	bool SubmitChatInput(const FString& UserInput, const FString& SourceTag);
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

private:
	void EmitUserLine(const FString& Text);
	void EmitAssistantLine(const FString& Text);
	void EmitStatus(const FString& Text);
	void SetCurrentState(EHCIAgentSessionState NewState);
	FString BuildStateStatusText(EHCIAgentSessionState State) const;
	void SetProgressState(const FHCIAgentUiProgressState& InState);
	void SetActivityHint(const FString& InHint);
	void ClearLocateTargets();
	void SetLocateTargetsFromExecutionReport(const struct FHCIAgentPlanExecutionReport& Report);
	bool ExecuteLastPlan(EHCIAgentPlanExecutionBranch Branch, const TCHAR* TriggerTag);

	void HandleCommandCompleted(const FHCIAgentCommandResult& Result);
	bool ExecuteRegisteredCommand(const FName& CommandName, const FHCIAgentCommandContext& Context);

private:
	TWeakObjectPtr<UHCIAgentSubsystem> OwnerSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UHCIAgentCommandBase> ActiveCommand = nullptr;

	// Last chat request snapshot (used for auto-repair retry when ScanAssets returns empty due to dirty path input).
	FString LastChatTrimmedUserInput;
	FString LastChatEffectiveInput;
	FString LastChatSourceTag;
	FString LastChatExtraEnvContextText;
	int32 LastChatAutoRepairAttempts = 0;

	TMap<FName, TSubclassOf<UHCIAgentCommandBase>> CommandRegistry;
	TArray<FHCIAgentQuickCommand> QuickCommands;
	FString QuickCommandsLoadError;
	bool bHasLastPlan = false;
	FHCIAgentPlan LastPlan;
	FString LastRouteReason;
	FHCIAgentPlannerResultMetadata LastPlannerMetadata;
	EHCIAgentSessionState CurrentState = EHCIAgentSessionState::Idle;
	FHCIAgentUiProgressState CurrentProgressState;
	FString CurrentActivityHint;
	TArray<FHCIAgentUiLocateTarget> LastExecutionLocateTargets;

	// Cached dry-run step results used to enrich the approval card with concrete routing proposals.
	bool bHasApprovalPreview = false;
	TArray<FHCIAgentExecutorStepResult> ApprovalPreviewStepResults;
};


