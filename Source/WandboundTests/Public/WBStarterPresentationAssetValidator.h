#pragma once

#include "CoreMinimal.h"

class UClass;
class UWBRuntimePresentationAssetSet;

enum class EWBPresentationAssetValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

enum class EWBPresentationAssetDependencyStatus : uint8
{
	EngineProvided,
	TrackedProjectAsset,
	UntrackedProjectAsset,
	IgnoredProjectAsset,
	MissingAsset,
	EditorOnlyAsset,
	InvalidAssetClass
};

struct WANDBOUNDTESTS_API FWBPresentationAssetValidationDiagnostic
{
	EWBPresentationAssetValidationSeverity Severity =
		EWBPresentationAssetValidationSeverity::Error;
	FString Code;
	FString AssetSetPackage;
	int32 EntryIndex = INDEX_NONE;
	FString EntryKind;
	FString PublicEventOrCategory;
	FString ReferencedPackagePath;
	FString RecommendedCorrection;
};

struct WANDBOUNDTESTS_API FWBPresentationAssetValidationResult
{
	TArray<FWBPresentationAssetValidationDiagnostic> Diagnostics;

	bool IsValid() const;
	bool ContainsCode(const FString& Code) const;
	int32 ErrorCount() const;
};

struct WANDBOUNDTESTS_API WBStarterPresentationAssetValidator
{
	static FWBPresentationAssetValidationResult Validate(
		const UWBRuntimePresentationAssetSet* AssetSet,
		bool bValidateGitTracking = true);

	static EWBPresentationAssetDependencyStatus ClassifyProjectFile(
		const FString& AbsoluteFilename);

	static FString BuildNormalizedSignature(
		const UWBRuntimePresentationAssetSet* AssetSet);

	static FString DependencyStatusName(
		EWBPresentationAssetDependencyStatus Status);
};
