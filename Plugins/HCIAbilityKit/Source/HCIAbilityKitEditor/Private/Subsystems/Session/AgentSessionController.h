#pragma once

#include "CoreMinimal.h"
#include "Subsystems/HCIAbilityKitAgentSubsystem.h"
#include "UObject/Object.h"

#include "AgentSessionController.generated.h"

/**
 * Agent 会话控制器（深模块）：
 * - 负责聊天会话状态机、命令执行编排、计划执行与结果归档。
 * - Subsystem 仅作为 Facade：生命周期 + 事件派发 + UI 入口。
 */
UCLASS(Transient)
class UHCIAbilityKitAgentSessionController : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHCIAbilityKitAgentSubsystem& InOwner);
	void Deinitialize();

	bool SubmitChatInput(const FString& UserInput, const FString& SourceTag);
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
	TWeakObjectPtr<UHCIAbilityKitAgentSubsystem> OwnerSubsystem;

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

