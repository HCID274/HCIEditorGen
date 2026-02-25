#pragma once

#include "CoreMinimal.h"
#include "Commands/HCIAbilityKitAgentCommandBase.h"
#include "EditorSubsystem.h"
#include "HCIAbilityKitAgentSubsystem.generated.h"

class UHCIAbilityKitAgentCommandBase;

struct FHCIAbilityKitAgentQuickCommand
{
	FString Label;
	FString Prompt;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentChatLineEvent, const FString& /*Line*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentStatusEvent, const FString& /*StatusText*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHCIAbilityKitAgentSummaryEvent, const FString& /*SummaryText*/);

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

	void ReloadQuickCommands();
	const TArray<FHCIAbilityKitAgentQuickCommand>& GetQuickCommands() const;
	const FString& GetQuickCommandsLoadError() const;

	FHCIAbilityKitAgentChatLineEvent OnChatLine;
	FHCIAbilityKitAgentStatusEvent OnStatusChanged;
	FHCIAbilityKitAgentSummaryEvent OnSummaryReceived;

private:
	void EmitUserLine(const FString& Text);
	void EmitAssistantLine(const FString& Text);
	void EmitStatus(const FString& Text);

	void HandleCommandCompleted(const FHCIAbilityKitAgentCommandResult& Result);
	bool ExecuteRegisteredCommand(const FName& CommandName, const FHCIAbilityKitAgentCommandContext& Context);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHCIAbilityKitAgentCommandBase> ActiveCommand = nullptr;

	TMap<FName, TSubclassOf<UHCIAbilityKitAgentCommandBase>> CommandRegistry;
	TArray<FHCIAbilityKitAgentQuickCommand> QuickCommands;
	FString QuickCommandsLoadError;
};
