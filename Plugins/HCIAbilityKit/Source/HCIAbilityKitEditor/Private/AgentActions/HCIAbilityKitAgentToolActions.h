#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"

namespace HCIAbilityKitAgentToolActions
{
HCIABILITYKITEDITOR_API void BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>>& OutActions);
}
