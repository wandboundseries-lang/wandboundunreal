#pragma once

#include "CoreMinimal.h"
#include "WBCardDBImporterDiagnosticSummaryForTests.h"

struct FWBCardDBImporterBatchEntryForTest
{
	FString Name;
	FString BundlePath;
	FWBCardDBSchemaValidationOptions Options;
};

struct FWBCardDBImporterBatchReadinessResultForTest
{
	bool bOk = false;
	FString Reason;

	int32 TotalBundles = 0;
	int32 ReadyBundles = 0;
	int32 NotReadyBundles = 0;

	TArray<FString> BundleNames;
	TArray<FWBCardDBImporterReadinessResultForTest> BundleResults;
	FWBCardDBImporterDiagnosticSummaryForTest DiagnosticSummary;
};

class FWBCardDBImporterBatchReadinessForTests
{
public:
	static FWBCardDBImporterBatchReadinessResultForTest EvaluateBatch(
		const TArray<FWBCardDBImporterBatchEntryForTest>& Entries);

	static bool BatchReadinessResultToJsonStringForTest(
		const FWBCardDBImporterBatchReadinessResultForTest& Result,
		FString& OutJson);
};
