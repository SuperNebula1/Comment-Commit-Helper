/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

class FCommentCommitHelperModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

	static const FName TabName;
};
