#include "Subsystems/HCIAbilityKitAgentSubsystem.h"

#include "Subsystems/Session/AgentSessionController.h"

void UHCIAbilityKitAgentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!SessionController)
	{
		SessionController = NewObject<UHCIAbilityKitAgentSessionController>(this);
	}
	if (SessionController)
	{
		SessionController->Initialize(*this);
	}
}

void UHCIAbilityKitAgentSubsystem::Deinitialize()
{
	if (SessionController)
	{
		SessionController->Deinitialize();
		SessionController = nullptr;
	}

	Super::Deinitialize();
}

bool UHCIAbilityKitAgentSubsystem::SubmitChatInput(const FString& UserInput, const FString& SourceTag)
{
	return SessionController ? SessionController->SubmitChatInput(UserInput, SourceTag) : false;
}

bool UHCIAbilityKitAgentSubsystem::IsBusy() const
{
	return SessionController ? SessionController->IsBusy() : false;
}

bool UHCIAbilityKitAgentSubsystem::HasLastPlan() const
{
	return SessionController ? SessionController->HasLastPlan() : false;
}

EHCIAbilityKitAgentSessionState UHCIAbilityKitAgentSubsystem::GetCurrentState() const
{
	return SessionController ? SessionController->GetCurrentState() : EHCIAbilityKitAgentSessionState::Idle;
}

FString UHCIAbilityKitAgentSubsystem::GetCurrentStateLabel() const
{
	return SessionController ? SessionController->GetCurrentStateLabel() : TEXT("Idle");
}

bool UHCIAbilityKitAgentSubsystem::CanCommitLastPlanFromChat() const
{
	return SessionController ? SessionController->CanCommitLastPlanFromChat() : false;
}

bool UHCIAbilityKitAgentSubsystem::CanCancelPendingPlanFromChat() const
{
	return SessionController ? SessionController->CanCancelPendingPlanFromChat() : false;
}

bool UHCIAbilityKitAgentSubsystem::BuildLastPlanCardLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();
	return SessionController ? SessionController->BuildLastPlanCardLines(OutLines) : false;
}

bool UHCIAbilityKitAgentSubsystem::BuildLastPlanApprovalCard(FHCIAbilityKitAgentUiApprovalCard& OutCard) const
{
	OutCard = FHCIAbilityKitAgentUiApprovalCard();
	return SessionController ? SessionController->BuildLastPlanApprovalCard(OutCard) : false;
}

void UHCIAbilityKitAgentSubsystem::GetCurrentProgressState(FHCIAbilityKitAgentUiProgressState& OutState) const
{
	OutState = FHCIAbilityKitAgentUiProgressState();
	if (SessionController)
	{
		SessionController->GetCurrentProgressState(OutState);
	}
}

void UHCIAbilityKitAgentSubsystem::GetCurrentActivityHint(FString& OutHint) const
{
	OutHint.Reset();
	if (SessionController)
	{
		SessionController->GetCurrentActivityHint(OutHint);
	}
}

void UHCIAbilityKitAgentSubsystem::GetLastExecutionLocateTargets(TArray<FHCIAbilityKitAgentUiLocateTarget>& OutTargets) const
{
	OutTargets.Reset();
	if (SessionController)
	{
		SessionController->GetLastExecutionLocateTargets(OutTargets);
	}
}

bool UHCIAbilityKitAgentSubsystem::OpenLastPlanPreview()
{
	return SessionController ? SessionController->OpenLastPlanPreview() : false;
}

bool UHCIAbilityKitAgentSubsystem::CommitLastPlanFromChat()
{
	return SessionController ? SessionController->CommitLastPlanFromChat() : false;
}

bool UHCIAbilityKitAgentSubsystem::CancelPendingPlanFromChat()
{
	return SessionController ? SessionController->CancelPendingPlanFromChat() : false;
}

bool UHCIAbilityKitAgentSubsystem::TryLocateLastExecutionTargetByIndex(const int32 TargetIndex)
{
	return SessionController ? SessionController->TryLocateLastExecutionTargetByIndex(TargetIndex) : false;
}

void UHCIAbilityKitAgentSubsystem::ReloadQuickCommands()
{
	if (SessionController)
	{
		SessionController->ReloadQuickCommands();
	}
}

const TArray<FHCIAbilityKitAgentQuickCommand>& UHCIAbilityKitAgentSubsystem::GetQuickCommands() const
{
	static const TArray<FHCIAbilityKitAgentQuickCommand> Empty;
	return SessionController ? SessionController->GetQuickCommands() : Empty;
}

const FString& UHCIAbilityKitAgentSubsystem::GetQuickCommandsLoadError() const
{
	static const FString Empty;
	return SessionController ? SessionController->GetQuickCommandsLoadError() : Empty;
}

