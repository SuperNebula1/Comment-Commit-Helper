/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;
class STextBlock;

class SCommentCommitHelper : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCommentCommitHelper) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply HandleScanClicked();
	FReply HandleCopyClicked();
	FReply HandleSettingsClicked();
	FReply HandleRemoveCommitCommentsClicked();

	void SetStatus(const FText& InStatus);
	void SetOutput(const FString& InOutput);

	TSharedPtr<SMultiLineEditableTextBox> OutputTextBox;
	TSharedPtr<STextBlock> StatusText;
};
