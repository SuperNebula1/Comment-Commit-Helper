/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#include "SCommentCommitHelper.h"

#include "CommentCommitScanner.h"

#include "HAL/PlatformApplicationMisc.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "CommentCommitHelperSettings.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCommentCommitHelper"

void SCommentCommitHelper::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Comment Commit Helper"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT(
					"Description",
					"Scans modified Blueprint assets, downloads their last submitted source-control revision, "
					"and lists comments that are new or changed locally."
				))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.Text(LOCTEXT("ScanButton", "Scan Source Control"))
						.OnClicked(this, &SCommentCommitHelper::HandleScanClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f)
				[
					SNew(SButton)
						.Text(LOCTEXT("CopyButton", "Copy Commit Message"))
						.OnClicked(this, &SCommentCommitHelper::HandleCopyClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.Text(LOCTEXT("SettingsButton", "Open Settings"))
						.OnClicked(this, &SCommentCommitHelper::HandleSettingsClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
						.Text(LOCTEXT("RemoveCommitCommentsButton", "Remove Commit Comments"))
						.ToolTipText(LOCTEXT(
							"RemoveCommitCommentsTooltip",
							"Deletes comment boxes and clears node comments that begin with the configured commit cleanup prefix."
						))
						.OnClicked(
							this,
							&SCommentCommitHelper::HandleRemoveCommitCommentsClicked
						)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 6.0f)
			[
				SAssignNew(StatusText, STextBlock)
					.Text(LOCTEXT("Ready", "Ready."))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(OutputTextBox, SMultiLineEditableTextBox)
					.AutoWrapText(false)
					.HScrollBar(nullptr)
					.Text(LOCTEXT("InitialOutput", "Click \"Scan Source Control\" to generate a commit summary."))
			]
		]
	];
}

FReply SCommentCommitHelper::HandleScanClicked()
{
	SetStatus(LOCTEXT("Scanning", "Scanning Blueprint assets and source-control history..."));

	const FCommentCommitScanResult Result = FCommentCommitScanner::Scan();
	const FString CommitMessage = Result.BuildCommitMessage();

	SetOutput(CommitMessage);

	const int32 AssetCount = Result.Assets.Num();
	const int32 CommentCount = Result.GetCommentCount();

	if (Result.Warnings.Num() > 0)
	{
		SetStatus(FText::Format(
			LOCTEXT("CompleteWarnings", "Complete: {0} asset(s), {1} comment(s), {2} warning(s)."),
			FText::AsNumber(AssetCount),
			FText::AsNumber(CommentCount),
			FText::AsNumber(Result.Warnings.Num())
		));
	}
	else
	{
		SetStatus(FText::Format(
			LOCTEXT("Complete", "Complete: {0} asset(s), {1} new/changed comment(s)."),
			FText::AsNumber(AssetCount),
			FText::AsNumber(CommentCount)
		));
	}

	return FReply::Handled();
}

FReply SCommentCommitHelper::HandleCopyClicked()
{
	if (OutputTextBox.IsValid())
	{
		const FString Text = OutputTextBox->GetText().ToString();
		FPlatformApplicationMisc::ClipboardCopy(*Text);
		SetStatus(LOCTEXT("Copied", "Commit message copied to the clipboard."));
	}

	return FReply::Handled();
}

FReply SCommentCommitHelper::HandleSettingsClicked()
{
	if (ISettingsModule* SettingsModule =
		FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer(
			TEXT("Project"),
			TEXT("Plugins"),
			TEXT("Comment Commit Helper")
		);

		SetStatus(LOCTEXT(
			"SettingsOpened",
			"Opened Project Settings -> Plugins -> Comment Commit Helper."
		));
	}
	else
	{
		SetStatus(LOCTEXT(
			"SettingsUnavailable",
			"Could not open Project Settings."
		));
	}

	return FReply::Handled();
}

FReply SCommentCommitHelper::HandleRemoveCommitCommentsClicked()
{
	const UCommentCommitHelperSettings* Settings =
		GetDefault<UCommentCommitHelperSettings>();

	FString Prefix = Settings
		? Settings->CommitCleanupPrefix
		: TEXT("@commit");

	Prefix.TrimStartAndEndInline();

	const FText ConfirmationText = FText::Format(
		LOCTEXT(
			"RemoveCommitCommentsConfirm",
			"Remove every Blueprint comment box and node comment that begins with \"{0}\"?\n\n"
			"This changes the Blueprint assets, but the operation can be undone with Ctrl+Z."
		),
		FText::FromString(Prefix)
	);

	if (FMessageDialog::Open(
		EAppMsgType::YesNo,
		ConfirmationText,
		LOCTEXT("RemoveCommitCommentsTitle", "Remove Commit Comments")
	) != EAppReturnType::Yes)
	{
		SetStatus(LOCTEXT("CleanupCancelled", "Commit comment cleanup cancelled."));
		return FReply::Handled();
	}

	const FCommentCommitCleanupResult Result =
		FCommentCommitScanner::RemoveCommitComments();

	if (Result.GetTotalRemoved() <= 0)
	{
		if (!Result.Warnings.IsEmpty())
		{
			SetStatus(FText::Format(
				LOCTEXT(
					"CleanupWarning",
					"No commit comments were removed. {0} warning(s) occurred."
				),
				FText::AsNumber(Result.Warnings.Num())
			));
		}
		else
		{
			SetStatus(FText::Format(
				LOCTEXT(
					"CleanupNone",
					"No comments beginning with \"{0}\" were found."
				),
				FText::FromString(Prefix)
			));
		}

		return FReply::Handled();
	}

	SetStatus(FText::Format(
		LOCTEXT(
			"CleanupComplete",
			"Removed {0} commit comment(s) from {1} Blueprint(s). Save the modified assets when ready."
		),
		FText::AsNumber(Result.GetTotalRemoved()),
		FText::AsNumber(Result.ModifiedBlueprints)
	));

	return FReply::Handled();
}

void SCommentCommitHelper::SetStatus(const FText& InStatus)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(InStatus);
	}
}

void SCommentCommitHelper::SetOutput(const FString& InOutput)
{
	if (OutputTextBox.IsValid())
	{
		OutputTextBox->SetText(FText::FromString(InOutput));
	}
}

#undef LOCTEXT_NAMESPACE
