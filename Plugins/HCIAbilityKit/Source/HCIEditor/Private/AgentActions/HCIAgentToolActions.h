#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAgentToolAction.h"

namespace HCIAgentToolActions
{
HCIEDITOR_API void BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAgentToolAction>>& OutActions);
}


