/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UCommentCommitHelperSettings;

struct FCommentCommitAssetResult
{
	FString AssetName;
	FString PackageName;
	FString Filename;
	bool bAddedAsset = false;
	TArray<FString> NewOrChangedComments;
	FString Error;
};

struct FCommentCommitCleanupResult
{
	int32 RemovedCommentBoxes = 0;
	int32 ClearedNodeComments = 0;
	int32 ModifiedBlueprints = 0;
	TArray<FString> Warnings;

	int32 GetTotalRemoved() const
	{
		return RemovedCommentBoxes + ClearedNodeComments;
	}
};

struct FCommentCommitScanResult
{
	TArray<FCommentCommitAssetResult> Assets;
	TArray<FString> Warnings;

	int32 GetCommentCount() const;
	FString BuildCommitMessage() const;
};

class FCommentCommitScanner
{
public:
	static FCommentCommitScanResult Scan();
	static FCommentCommitCleanupResult RemoveCommitComments();

private:
	static void CollectBlueprintComments(
		UBlueprint* Blueprint,
		const UCommentCommitHelperSettings* Settings,
		TArray<FString>& OutComments
	);

	static FString NormalizeComment(const FString& InComment);
	static FString StripLeadingAtTag(const FString& InComment);

	static bool ProcessComment(
		const FString& InComment,
		const UCommentCommitHelperSettings* Settings,
		FString& OutProcessedComment
	);

	static bool ShouldScanAsset(
		const FString& PackageName,
		const FString& AssetName,
		const UCommentCommitHelperSettings* Settings
	);

	static bool LoadHistoricalBlueprintComments(
		const FString& LocalFilename,
		const FString& AssetName,
		const UCommentCommitHelperSettings* Settings,
		TArray<FString>& OutComments,
		FString& OutError
	);

	static void CompareComments(
		const TArray<FString>& CurrentComments,
		const TArray<FString>& HistoricalComments,
		TArray<FString>& OutNewOrChanged
	);
};
