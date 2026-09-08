/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#include "CommentCommitHelperModule.h"

#include "SCommentCommitHelper.h"

#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FCommentCommitHelperModule"

const FName FCommentCommitHelperModule::TabName(TEXT("CommentCommitHelper"));

void FCommentCommitHelperModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateRaw(this, &FCommentCommitHelperModule::SpawnPluginTab)
	)
	.SetDisplayName(LOCTEXT("TabTitle", "Comment Commit Helper"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCommentCommitHelperModule::RegisterMenus)
	);
}

void FCommentCommitHelperModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FCommentCommitHelperModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("Programming");

	Section.AddMenuEntry(
		"OpenCommentCommitHelper",
		LOCTEXT("MenuLabel", "Comment Commit Helper"),
		LOCTEXT("MenuTooltip", "Generate a source-control summary from new or changed Blueprint comments."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FCommentCommitHelperModule::TabName);
		}))
	);
}

TSharedRef<SDockTab> FCommentCommitHelperModule::SpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SCommentCommitHelper)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCommentCommitHelperModule, CommentCommitHelper)
