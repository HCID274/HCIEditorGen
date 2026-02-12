#pragma once

#include "Modules/ModuleManager.h"

class FHCIAbilityKitRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
