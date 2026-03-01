#include "Subsystems/HCIAgentSubsystem.h"

#include "Subsystems/Session/AgentSessionController.h"

void UHCIAgentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!SessionController)
	{
		SessionController = NewObject<UHCIAgentSessionController>(this);
	}
	if (SessionController)
	{
		SessionController->Initialize(*this);
	}
}

void UHCIAgentSubsystem::Deinitialize()
{
	if (SessionController)
	{
		SessionController->Deinitialize();
		SessionController = nullptr;
	}

	Super::Deinitialize();
}

bool UHCIAgentSubsystem::SubmitChatInput(const FString& UserInput, const FString& SourceTag)
{
	return SessionController ? SessionController->SubmitChatInput(UserInput, SourceTag) : false;
}

bool UHCIAgentSubsystem::IsBusy() const
{
	return SessionController ? SessionController->IsBusy() : false;
}

bool UHCIAgentSubsystem::HasLastPlan() const
{
	return SessionController ? SessionController->HasLastPlan() : false;
}

EHCIAgentSessionState UHCIAgentSubsystem::GetCurrentState() const
{
	return SessionController ? SessionController->GetCurrentState() : EHCIAgentSessionState::Idle;
}

FString UHCIAgentSubsystem::GetCurrentStateLabel() const
{
	return SessionController ? SessionController->GetCurrentStateLabel() : TEXT("Idle");
}

bool UHCIAgentSubsystem::CanCommitLastPlanFromChat() const
{
	return SessionController ? SessionController->CanCommitLastPlanFromChat() : false;
}

bool UHCIAgentSubsystem::CanCancelPendingPlanFromChat() const
{
	return SessionController ? SessionController->CanCancelPendingPlanFromChat() : false;
}

bool UHCIAgentSubsystem::BuildLastPlanCardLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();
	return SessionController ? SessionController->BuildLastPlanCardLines(OutLines) : false;
}

bool UHCIAgentSubsystem::BuildLastPlanApprovalCard(FHCIAgentUiApprovalCard& OutCard) const
{
	OutCard = FHCIAgentUiApprovalCard();
	return SessionController ? SessionController->BuildLastPlanApprovalCard(OutCard) : false;
}

void UHCIAgentSubsystem::GetCurrentProgressState(FHCIAgentUiProgressState& OutState) const
{
	OutState = FHCIAgentUiProgressState();
	if (SessionController)
	{
		SessionController->GetCurrentProgressState(OutState);
	}
}

void UHCIAgentSubsystem::GetCurrentActivityHint(FString& OutHint) const
{
	OutHint.Reset();
	if (SessionController)
	{
		SessionController->GetCurrentActivityHint(OutHint);
	}
}

void UHCIAgentSubsystem::GetLastExecutionLocateTargets(TArray<FHCIAgentUiLocateTarget>& OutTargets) const
{
	OutTargets.Reset();
	if (SessionController)
	{
		SessionController->GetLastExecutionLocateTargets(OutTargets);
	}
}

bool UHCIAgentSubsystem::OpenLastPlanPreview()
{
	return SessionController ? SessionController->OpenLastPlanPreview() : false;
}

bool UHCIAgentSubsystem::CommitLastPlanFromChat()
{
	return SessionController ? SessionController->CommitLastPlanFromChat() : false;
}

bool UHCIAgentSubsystem::CancelPendingPlanFromChat()
{
	return SessionController ? SessionController->CancelPendingPlanFromChat() : false;
}

bool UHCIAgentSubsystem::TryLocateLastExecutionTargetByIndex(const int32 TargetIndex)
{
	return SessionController ? SessionController->TryLocateLastExecutionTargetByIndex(TargetIndex) : false;
}

void UHCIAgentSubsystem::ReloadQuickCommands()
{
	if (SessionController)
	{
		SessionController->ReloadQuickCommands();
	}
}

const TArray<FHCIAgentQuickCommand>& UHCIAgentSubsystem::GetQuickCommands() const
{
	static const TArray<FHCIAgentQuickCommand> Empty;
	return SessionController ? SessionController->GetQuickCommands() : Empty;
}

const FString& UHCIAgentSubsystem::GetQuickCommandsLoadError() const
{
	static const FString Empty;
	return SessionController ? SessionController->GetQuickCommandsLoadError() : Empty;
}


