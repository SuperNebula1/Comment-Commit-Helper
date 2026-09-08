/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#include "CommentCommitHelperSettings.h"

UCommentCommitHelperSettings::UCommentCommitHelperSettings()
{
	// Keep the most common Unreal-generated Blueprint message out by default.
	IgnoredExactComments.Add(
		TEXT("This node is disabled and will not be called. Drag off pins to build functionality.")
	);

	// Useful starter defaults. These can be removed/changed in Project Settings.
	IgnoredCommentPrefixes.Add(TEXT("TODO:"));
	IgnoredCommentPrefixes.Add(TEXT("DEV:"));
	IgnoredCommentPrefixes.Add(TEXT("DEBUG:"));
}

FName UCommentCommitHelperSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UCommentCommitHelperSettings::GetSectionName() const
{
	return TEXT("Comment Commit Helper");
}
