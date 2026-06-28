#pragma once

#include "CoreMinimal.h"
#include "WBCardDBImporterBatchReadinessForTests.h"

enum class EWBCardDBImporterManifestDiagnostic : uint8
{
	None,
	ManifestJsonParseFailed,
	ManifestSchemaVersionMissing,
	ManifestSchemaVersionUnsupported,
	ManifestIdMissing,
	ManifestBatchesMissing,
	ManifestBatchesMalformed,
	ManifestBatchNameMissing,
	ManifestBatchNameDuplicate,
	ManifestBundlesMissing,
	ManifestBundlesMalformed,
	ManifestBundleNameMissing,
	ManifestBundleNameDuplicate,
	ManifestBundlePathMissing,
	ManifestBundlePathInvalid,
	ManifestBundlePathNotFound,
	ManifestCompatibilityMalformed,
	ManifestMetadataMalformed,
	ManifestHiddenInfoPolicyViolation
};

struct FWBCardDBImporterManifestDiagnosticForTest
{
	EWBCardDBImporterManifestDiagnostic Code = EWBCardDBImporterManifestDiagnostic::None;
	FString ManifestId;
	FString BatchName;
	FString BundleName;
	FString JsonPath;
};

struct FWBCardDBImporterManifestBundleEntryForTest
{
	FString Name;
	FString RelativePath;
	FWBCardDBSchemaValidationOptions Options;
};

struct FWBCardDBImporterManifestBatchForTest
{
	FString BatchName;
	TArray<FWBCardDBImporterManifestBundleEntryForTest> Bundles;
};

struct FWBCardDBImporterManifestValidationResultForTest
{
	bool bOk = false;
	FString Reason;
	FString SourcePath;
	FString ManifestId;

	TArray<FWBCardDBImporterManifestDiagnosticForTest> Diagnostics;
	TArray<FWBCardDBImporterManifestBatchForTest> Batches;
};

struct FWBCardDBImporterManifestBatchEvaluationResultForTest
{
	bool bOk = false;
	FString Reason;

	FWBCardDBImporterManifestValidationResultForTest ManifestValidationResult;
	TArray<FWBCardDBImporterBatchReadinessResultForTest> BatchResults;
};

class FWBCardDBImporterManifestForTests
{
public:
	static FWBCardDBImporterManifestValidationResultForTest ValidateManifestFile(
		const FString& AbsolutePathToManifest);

	static FWBCardDBImporterManifestValidationResultForTest ValidateManifestJsonString(
		const FString& Json,
		const FString& SourcePathForDiagnostics,
		const FString& FixtureRootAbsolutePath);

	static FWBCardDBImporterManifestBatchEvaluationResultForTest EvaluateManifestFile(
		const FString& AbsolutePathToManifest);

	static bool ManifestEvaluationResultToJsonStringForTest(
		const FWBCardDBImporterManifestBatchEvaluationResultForTest& Result,
		FString& OutJson);

	static FString ManifestDiagnosticCodeToString(EWBCardDBImporterManifestDiagnostic Code);
};
