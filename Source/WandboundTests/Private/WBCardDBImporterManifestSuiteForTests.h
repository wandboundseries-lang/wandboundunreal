#pragma once

#include "CoreMinimal.h"
#include "WBCardDBImporterDiagnosticSummaryForTests.h"
#include "WBCardDBImporterManifestForTests.h"

enum class EWBCardDBImporterManifestSuiteDiagnostic : uint8
{
	None,
	SuiteJsonParseFailed,
	SuiteSchemaVersionMissing,
	SuiteSchemaVersionUnsupported,
	SuiteIdMissing,
	SuiteManifestsMissing,
	SuiteManifestsMalformed,
	SuiteManifestNameMissing,
	SuiteManifestNameDuplicate,
	SuiteManifestPathMissing,
	SuiteManifestPathInvalid,
	SuiteManifestPathNotFound,
	SuiteMetadataMalformed,
	SuiteHiddenInfoPolicyViolation
};

struct FWBCardDBImporterManifestSuiteDiagnosticForTest
{
	EWBCardDBImporterManifestSuiteDiagnostic Code = EWBCardDBImporterManifestSuiteDiagnostic::None;
	FString SuiteId;
	FString ManifestName;
	FString JsonPath;
};

struct FWBCardDBImporterManifestSuiteEntryForTest
{
	FString Name;
	FString RelativePath;
};

struct FWBCardDBImporterManifestSuiteValidationResultForTest
{
	bool bOk = false;
	FString Reason;
	FString SourcePath;
	FString SuiteId;

	TArray<FWBCardDBImporterManifestSuiteDiagnosticForTest> Diagnostics;
	TArray<FWBCardDBImporterManifestSuiteEntryForTest> ManifestEntries;
};

struct FWBCardDBImporterManifestSuiteEvaluationResultForTest
{
	bool bOk = false;
	FString Reason;

	FWBCardDBImporterManifestSuiteValidationResultForTest SuiteValidationResult;
	TArray<FWBCardDBImporterManifestBatchEvaluationResultForTest> ManifestResults;

	int32 TotalManifests = 0;
	int32 ValidManifests = 0;
	int32 InvalidManifests = 0;
	int32 TotalBatches = 0;
	int32 TotalBundles = 0;
	int32 ReadyBundles = 0;
	int32 NotReadyBundles = 0;

	FWBCardDBImporterDiagnosticSummaryForTest AggregateDiagnosticSummary;
};

class FWBCardDBImporterManifestSuiteForTests
{
public:
	static FWBCardDBImporterManifestSuiteValidationResultForTest ValidateSuiteFile(
		const FString& AbsolutePathToSuite);

	static FWBCardDBImporterManifestSuiteValidationResultForTest ValidateSuiteJsonString(
		const FString& Json,
		const FString& SourcePathForDiagnostics,
		const FString& FixtureRootAbsolutePath);

	static FWBCardDBImporterManifestSuiteEvaluationResultForTest EvaluateSuiteFile(
		const FString& AbsolutePathToSuite);

	static bool SuiteEvaluationResultToJsonStringForTest(
		const FWBCardDBImporterManifestSuiteEvaluationResultForTest& Result,
		FString& OutJson);

	static FString SuiteDiagnosticCodeToString(EWBCardDBImporterManifestSuiteDiagnostic Code);
};
