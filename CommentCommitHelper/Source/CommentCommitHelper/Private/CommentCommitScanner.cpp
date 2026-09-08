/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

#include "CommentCommitScanner.h"

#include "CommentCommitHelperSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SourceControlOperations.h"
#include "ScopedTransaction.h"
#include "UObject/FindObjectFlags.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace CommentCommitHelper
{
	static FString MakeAssetDisplayName(const FAssetData& AssetData)
	{
		return AssetData.AssetName.ToString();
	}

	static FString MakePackageFilename(const FString& PackageName)
	{
		return FPackageName::LongPackageNameToFilename(
			PackageName,
			FPackageName::GetAssetPackageExtension()
		);
	}

	static bool StartsWithAny(const FString& Value, const TArray<FString>& Prefixes)
	{
		for (const FString& Prefix : Prefixes)
		{
			FString CleanPrefix = Prefix;
			CleanPrefix.TrimStartAndEndInline();

			if (!CleanPrefix.IsEmpty() &&
				Value.StartsWith(CleanPrefix, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static bool ContainsAny(const FString& Value, const TArray<FString>& Needles)
	{
		for (const FString& Needle : Needles)
		{
			FString CleanNeedle = Needle;
			CleanNeedle.TrimStartAndEndInline();

			if (!CleanNeedle.IsEmpty() &&
				Value.Contains(CleanNeedle, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static bool EqualsAny(const FString& Value, const TArray<FString>& Values)
	{
		for (const FString& Candidate : Values)
		{
			FString CleanCandidate = Candidate;
			CleanCandidate.TrimStartAndEndInline();

			if (!CleanCandidate.IsEmpty() &&
				Value.Equals(CleanCandidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static bool MatchesPathPrefix(const FString& PackageName, const FString& RawPath)
	{
		FString Path = RawPath;
		Path.TrimStartAndEndInline();

		if (Path.IsEmpty())
		{
			return false;
		}

		Path.ReplaceInline(TEXT("\\"), TEXT("/"));

		// Treat /Game/Foo and /Game/Foo/ consistently.
		if (!Path.EndsWith(TEXT("/")))
		{
			Path += TEXT("/");
		}

		FString NormalizedPackage = PackageName;
		if (!NormalizedPackage.EndsWith(TEXT("/")))
		{
			NormalizedPackage += TEXT("/");
		}

		return NormalizedPackage.StartsWith(Path, ESearchCase::IgnoreCase);
	}
}

int32 FCommentCommitScanResult::GetCommentCount() const
{
	int32 Count = 0;

	for (const FCommentCommitAssetResult& Asset : Assets)
	{
		Count += Asset.NewOrChangedComments.Num();
	}

	return Count;
}

FString FCommentCommitScanResult::BuildCommitMessage() const
{
	TArray<FString> Lines;

	for (const FCommentCommitAssetResult& Asset : Assets)
	{
		if (Asset.NewOrChangedComments.IsEmpty())
		{
			continue;
		}

		if (!Lines.IsEmpty())
		{
			Lines.Add(TEXT(""));
		}

		Lines.Add(Asset.AssetName);

		for (const FString& Comment : Asset.NewOrChangedComments)
		{
			Lines.Add(FString::Printf(TEXT("- %s"), *Comment));
		}
	}

	if (!Warnings.IsEmpty())
	{
		if (!Lines.IsEmpty())
		{
			Lines.Add(TEXT(""));
		}

		Lines.Add(TEXT("Scan warnings:"));

		for (const FString& Warning : Warnings)
		{
			Lines.Add(FString::Printf(TEXT("- %s"), *Warning));
		}
	}

	if (Lines.IsEmpty())
	{
		return TEXT("No new or changed Blueprint comments were found.");
	}

	return FString::Join(Lines, TEXT("\n"));
}

FCommentCommitCleanupResult FCommentCommitScanner::RemoveCommitComments()
{
	FCommentCommitCleanupResult Result;
	const UCommentCommitHelperSettings* Settings =
		GetDefault<UCommentCommitHelperSettings>();

	if (!Settings)
	{
		Result.Warnings.Add(TEXT("Comment Commit Helper settings could not be loaded."));
		return Result;
	}

	FString CleanupPrefix = Settings->CommitCleanupPrefix;
	CleanupPrefix.TrimStartAndEndInline();

	if (CleanupPrefix.IsEmpty())
	{
		Result.Warnings.Add(TEXT("Commit Cleanup Prefix is empty. Nothing was removed."));
		return Result;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> BlueprintAssets;
	AssetRegistryModule.Get().GetAssets(Filter, BlueprintAssets);

	const FScopedTransaction Transaction(
		NSLOCTEXT(
			"CommentCommitHelper",
			"RemoveCommitCommentsTransaction",
			"Remove Commit Comments"
		)
	);

	for (const FAssetData& AssetData : BlueprintAssets)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		const FString AssetName = AssetData.AssetName.ToString();

		if (!ShouldScanAsset(PackageName, AssetName, Settings))
		{
			continue;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());

		if (!Blueprint)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("%s: Could not load Blueprint."),
				*AssetName
			));
			continue;
		}

		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);

		bool bBlueprintChanged = false;

		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			TArray<UEdGraphNode_Comment*> CommentBoxesToRemove;

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}

				const FString Normalized = NormalizeComment(Node->NodeComment);

				if (Normalized.IsEmpty() ||
					!Normalized.StartsWith(
						CleanupPrefix,
						ESearchCase::IgnoreCase
					))
				{
					continue;
				}

				if (UEdGraphNode_Comment* CommentNode =
					Cast<UEdGraphNode_Comment>(Node))
				{
					CommentBoxesToRemove.Add(CommentNode);
					continue;
				}

				Node->Modify();
				Node->NodeComment.Empty();
				Node->bCommentBubbleVisible = false;
				Result.ClearedNodeComments++;
				bBlueprintChanged = true;
			}

			if (!CommentBoxesToRemove.IsEmpty())
			{
				Graph->Modify();

				for (UEdGraphNode_Comment* CommentNode : CommentBoxesToRemove)
				{
					if (!CommentNode)
					{
						continue;
					}

					CommentNode->Modify();
					Graph->RemoveNode(CommentNode);
					Result.RemovedCommentBoxes++;
					bBlueprintChanged = true;
				}
			}
		}

		if (bBlueprintChanged)
		{
			Blueprint->Modify();
			Blueprint->MarkPackageDirty();
			Result.ModifiedBlueprints++;
		}
	}

	return Result;
}

FCommentCommitScanResult FCommentCommitScanner::Scan()
{
	FCommentCommitScanResult Result;
	const UCommentCommitHelperSettings* Settings =
		GetDefault<UCommentCommitHelperSettings>();

	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();

	if (!SourceControlModule.IsEnabled())
	{
		Result.Warnings.Add(TEXT("Source control is not enabled in the Unreal Editor."));
		return Result;
	}

	ISourceControlProvider& Provider = SourceControlModule.GetProvider();

	if (!Provider.IsAvailable())
	{
		Result.Warnings.Add(FString::Printf(
			TEXT("The active source-control provider '%s' is not available."),
			*Provider.GetName().ToString()
		));
		return Result;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> BlueprintAssets;
	AssetRegistryModule.Get().GetAssets(Filter, BlueprintAssets);

	for (const FAssetData& AssetData : BlueprintAssets)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		const FString AssetName = AssetData.AssetName.ToString();

		if (!ShouldScanAsset(PackageName, AssetName, Settings))
		{
			continue;
		}

		const FString Filename = CommentCommitHelper::MakePackageFilename(PackageName);

		if (!FPaths::FileExists(Filename))
		{
			continue;
		}

		FSourceControlStatePtr State =
			Provider.GetState(Filename, EStateCacheUsage::ForceUpdate);

		if (!State.IsValid())
		{
			continue;
		}

		const bool bAdded = State->IsAdded();
		const bool bModified = State->IsModified();

		if ((!bAdded && !bModified) ||
			(bAdded && !Settings->bIncludeAddedAssets) ||
			(bModified && !bAdded && !Settings->bIncludeModifiedAssets))
		{
			continue;
		}

		FCommentCommitAssetResult AssetResult;
		AssetResult.AssetName = CommentCommitHelper::MakeAssetDisplayName(AssetData);
		AssetResult.PackageName = PackageName;
		AssetResult.Filename = Filename;
		AssetResult.bAddedAsset = bAdded;

		UBlueprint* CurrentBlueprint = Cast<UBlueprint>(AssetData.GetAsset());

		if (!CurrentBlueprint)
		{
			AssetResult.Error = TEXT("Could not load the current Blueprint.");
			Result.Warnings.Add(FString::Printf(
				TEXT("%s: %s"),
				*AssetResult.AssetName,
				*AssetResult.Error
			));
			continue;
		}

		TArray<FString> CurrentComments;
		CollectBlueprintComments(CurrentBlueprint, Settings, CurrentComments);

		if (bAdded || !State->IsSourceControlled())
		{
			AssetResult.NewOrChangedComments = CurrentComments;
		}
		else
		{
			TArray<FString> HistoricalComments;
			FString HistoryError;

			if (!LoadHistoricalBlueprintComments(
				Filename,
				AssetResult.AssetName,
				Settings,
				HistoricalComments,
				HistoryError
			))
			{
				AssetResult.Error = HistoryError;
				Result.Warnings.Add(FString::Printf(
					TEXT("%s: %s"),
					*AssetResult.AssetName,
					*HistoryError
				));
				continue;
			}

			CompareComments(
				CurrentComments,
				HistoricalComments,
				AssetResult.NewOrChangedComments
			);
		}

		if (!AssetResult.NewOrChangedComments.IsEmpty())
		{
			Result.Assets.Add(MoveTemp(AssetResult));
		}
	}

	Result.Assets.Sort([](
		const FCommentCommitAssetResult& A,
		const FCommentCommitAssetResult& B
	)
	{
		return A.AssetName < B.AssetName;
	});

	return Result;
}

void FCommentCommitScanner::CollectBlueprintComments(
	UBlueprint* Blueprint,
	const UCommentCommitHelperSettings* Settings,
	TArray<FString>& OutComments
)
{
	OutComments.Reset();

	if (!Blueprint || !Settings)
	{
		return;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (Settings->bIncludeCommentBoxes && Cast<UEdGraphNode_Comment>(Node))
			{
				FString ProcessedComment;

				if (ProcessComment(Node->NodeComment, Settings, ProcessedComment))
				{
					OutComments.Add(ProcessedComment);
				}

				continue;
			}

			if (Settings->bIncludeNodeComments &&
				Node->bCommentBubbleVisible &&
				!Node->NodeComment.IsEmpty())
			{
				FString ProcessedComment;

				if (ProcessComment(Node->NodeComment, Settings, ProcessedComment))
				{
					OutComments.Add(ProcessedComment);
				}
			}
		}
	}
}

FString FCommentCommitScanner::NormalizeComment(const FString& InComment)
{
	FString Result = InComment;
	Result.TrimStartAndEndInline();

	Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Result.ReplaceInline(TEXT("\r"), TEXT("\n"));

	TArray<FString> RawLines;
	Result.ParseIntoArrayLines(RawLines, false);

	TArray<FString> CleanLines;

	for (FString Line : RawLines)
	{
		Line.TrimStartAndEndInline();

		if (!Line.IsEmpty())
		{
			CleanLines.Add(Line);
		}
	}

	Result = FString::Join(CleanLines, TEXT(" "));

	while (Result.Contains(TEXT("  ")))
	{
		Result.ReplaceInline(TEXT("  "), TEXT(" "));
	}

	return Result;
}

FString FCommentCommitScanner::StripLeadingAtTag(const FString& InComment)
{
	FString Result = InComment;
	Result.TrimStartAndEndInline();

	if (!Result.StartsWith(TEXT("@")))
	{
		return Result;
	}

	int32 Index = 1;

	while (Index < Result.Len())
	{
		const TCHAR Character = Result[Index];

		if (
			FChar::IsAlpha(Character) ||
			FChar::IsDigit(Character) ||
			Character == TEXT('_')
		)
		{
			Index++;
			continue;
		}

		break;
	}

	// A lone '@' is not treated as a tag.
	if (Index <= 1)
	{
		return Result;
	}

	// Strip whitespace after the tag as well.
	while (Index < Result.Len() && FChar::IsWhitespace(Result[Index]))
	{
		Index++;
	}

	Result.RightChopInline(Index, EAllowShrinking::No);
	Result.TrimStartAndEndInline();

	return Result;
}

bool FCommentCommitScanner::ProcessComment(
	const FString& InComment,
	const UCommentCommitHelperSettings* Settings,
	FString& OutProcessedComment
)
{
	OutProcessedComment = NormalizeComment(InComment);

	if (!Settings || OutProcessedComment.IsEmpty())
	{
		return false;
	}

	if (Settings->bIgnoreUnrealDisabledNodeMessages &&
		OutProcessedComment.Contains(
			TEXT("This node is disabled and will not be called"),
			ESearchCase::IgnoreCase
		))
	{
		return false;
	}

	if (CommentCommitHelper::EqualsAny(
			OutProcessedComment,
			Settings->IgnoredExactComments
		) ||
		CommentCommitHelper::StartsWithAny(
			OutProcessedComment,
			Settings->IgnoredCommentPrefixes
		) ||
		CommentCommitHelper::ContainsAny(
			OutProcessedComment,
			Settings->IgnoredCommentContains
		))
	{
		return false;
	}

	if (!Settings->RequiredCommentPrefixes.IsEmpty())
	{
		FString MatchedPrefix;

		for (const FString& Prefix : Settings->RequiredCommentPrefixes)
		{
			FString CleanPrefix = Prefix;
			CleanPrefix.TrimStartAndEndInline();

			if (!CleanPrefix.IsEmpty() &&
				OutProcessedComment.StartsWith(
					CleanPrefix,
					ESearchCase::IgnoreCase
				))
			{
				MatchedPrefix = CleanPrefix;
				break;
			}
		}

		if (MatchedPrefix.IsEmpty())
		{
			return false;
		}

		if (Settings->bStripRequiredPrefixFromOutput)
		{
			OutProcessedComment.RightChopInline(MatchedPrefix.Len(), EAllowShrinking::No);
			OutProcessedComment.TrimStartAndEndInline();
		}
	}

	if (Settings->bStripLeadingAtTagFromOutput)
	{
		OutProcessedComment = StripLeadingAtTag(OutProcessedComment);
	}

	return !OutProcessedComment.IsEmpty();
}

bool FCommentCommitScanner::ShouldScanAsset(
	const FString& PackageName,
	const FString& AssetName,
	const UCommentCommitHelperSettings* Settings
)
{
	if (!Settings)
	{
		return true;
	}

	if (!Settings->IncludedAssetPaths.IsEmpty())
	{
		bool bIncluded = false;

		for (const FString& Path : Settings->IncludedAssetPaths)
		{
			if (CommentCommitHelper::MatchesPathPrefix(PackageName, Path))
			{
				bIncluded = true;
				break;
			}
		}

		if (!bIncluded)
		{
			return false;
		}
	}

	for (const FString& Path : Settings->IgnoredAssetPaths)
	{
		if (CommentCommitHelper::MatchesPathPrefix(PackageName, Path))
		{
			return false;
		}
	}

	if (CommentCommitHelper::StartsWithAny(
			AssetName,
			Settings->IgnoredAssetNamePrefixes
		))
	{
		return false;
	}

	return true;
}

bool FCommentCommitScanner::LoadHistoricalBlueprintComments(
	const FString& LocalFilename,
	const FString& AssetName,
	const UCommentCommitHelperSettings* Settings,
	TArray<FString>& OutComments,
	FString& OutError
)
{
	OutComments.Reset();
	OutError.Reset();

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatus =
		ISourceControlOperation::Create<FUpdateStatus>();

	UpdateStatus->SetUpdateHistory(true);
	UpdateStatus->SetUpdateModifiedState(true);
	UpdateStatus->SetForceUpdate(true);

	const ECommandResult::Type UpdateResult = Provider.Execute(
		UpdateStatus,
		LocalFilename,
		EConcurrency::Synchronous
	);

	if (UpdateResult != ECommandResult::Succeeded)
	{
		OutError = TEXT("Could not refresh source-control history.");
		return false;
	}

	FSourceControlStatePtr State =
		Provider.GetState(LocalFilename, EStateCacheUsage::Use);

	if (!State.IsValid())
	{
		OutError = TEXT("Source-control state disappeared after history refresh.");
		return false;
	}

	TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision;

	if (State->GetHistorySize() > 0)
	{
		Revision = State->GetHistoryItem(0);
	}

	if (!Revision.IsValid())
	{
		Revision = State->GetCurrentRevision();
	}

	if (!Revision.IsValid())
	{
		OutError = TEXT("No submitted revision was available for comparison.");
		return false;
	}

	const FString RevisionFolder = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("CommentCommitHelper"),
		TEXT("Revisions")
	);

	IFileManager::Get().MakeDirectory(*RevisionFolder, true);

	const FString SafeRevision = Revision->GetRevision()
		.Replace(TEXT("/"), TEXT("_"))
		.Replace(TEXT("\\"), TEXT("_"))
		.Replace(TEXT(":"), TEXT("_"));

	FString RevisionFilename = FPaths::Combine(
		RevisionFolder,
		FString::Printf(
			TEXT("%s__SC_%s%s"),
			*AssetName,
			*SafeRevision,
			*FPackageName::GetAssetPackageExtension()
		)
	);

	if (!Revision->Get(RevisionFilename, EConcurrency::Synchronous))
	{
		OutError = FString::Printf(
			TEXT("Failed to download source-control revision '%s'."),
			*Revision->GetRevision()
		);
		return false;
	}

	if (!FPaths::FileExists(RevisionFilename))
	{
		OutError = TEXT(
			"The source-control provider reported success but no revision file was created."
		);
		return false;
	}

	UPackage* RevisionPackage = LoadPackage(
		nullptr,
		*RevisionFilename,
		LOAD_NoWarn | LOAD_Quiet
	);

	if (!RevisionPackage)
	{
		OutError = FString::Printf(
			TEXT(
				"Downloaded revision '%s', but Unreal could not load the historical package."
			),
			*Revision->GetRevision()
		);
		return false;
	}

	UBlueprint* RevisionBlueprint = FindObject<UBlueprint>(
		RevisionPackage,
		*AssetName
	);

	if (!RevisionBlueprint)
	{
		ForEachObjectWithOuter(
			RevisionPackage,
			[&RevisionBlueprint](UObject* Object)
			{
				if (!RevisionBlueprint)
				{
					RevisionBlueprint = Cast<UBlueprint>(Object);
				}
			},
			EGetObjectsFlags::IncludeNestedObjects
		);
	}

	if (!RevisionBlueprint)
	{
		OutError = TEXT(
			"Historical package loaded, but it did not contain a Blueprint export."
		);
		return false;
	}

	CollectBlueprintComments(RevisionBlueprint, Settings, OutComments);

	RevisionPackage->ClearFlags(RF_Standalone);
	RevisionPackage->SetDirtyFlag(false);

	return true;
}

void FCommentCommitScanner::CompareComments(
	const TArray<FString>& CurrentComments,
	const TArray<FString>& HistoricalComments,
	TArray<FString>& OutNewOrChanged
)
{
	OutNewOrChanged.Reset();

	TMap<FString, int32> HistoricalCounts;

	for (const FString& Comment : HistoricalComments)
	{
		HistoricalCounts.FindOrAdd(Comment)++;
	}

	TSet<FString> AlreadyEmitted;

	for (const FString& Comment : CurrentComments)
	{
		int32* HistoricalCount = HistoricalCounts.Find(Comment);

		if (HistoricalCount && *HistoricalCount > 0)
		{
			(*HistoricalCount)--;
			continue;
		}

		if (!AlreadyEmitted.Contains(Comment))
		{
			OutNewOrChanged.Add(Comment);
			AlreadyEmitted.Add(Comment);
		}
	}
}
