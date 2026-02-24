#pragma once

#include "CoreMinimal.h"

#include "Agent/HCIAbilityKitAgentToolAction.h"

namespace HCIAbilityKitAgentToolActions
{
void BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>>& OutActions);
}

