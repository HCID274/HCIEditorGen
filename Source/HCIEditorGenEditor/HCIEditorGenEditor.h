#pragma once
#include "Modules/ModuleManager.h"

class FHCIEditorGenEditorModule : public IModuleInterface{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};