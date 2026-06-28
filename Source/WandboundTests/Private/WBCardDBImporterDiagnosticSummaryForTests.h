#pragma once

#include "CoreMinimal.h"
#include "WBCardDBImporterReadinessForTests.h"

struct FWBCardDBImporterReasonSummaryForTest
{
	FString Reason;
	int32 Count = 0;
};

struct FWBCardDBImporterDiagnosticCodeSummaryForTest
{
	FString DiagnosticCode;
	int32 Count = 0;
	int32 AffectedBundleCount = 0;
	int32 AffectedCardCount = 0;
};

struct FWBCardDBImporterDiagnosticSummaryForTest
{
	int32 TotalBundles = 0;
	int32 ReadyBundles = 0;
	int32 NotReadyBundles = 0;

	TArray<FWBCardDBImporterReasonSummaryForTest> ReasonSummaries;
	TArray<FWBCardDBImporterDiagnosticCodeSummaryForTest> DiagnosticSummaries;

	TArray<FString> ReadyBundleSourcePaths;
	TArray<FString> NotReadyBundleSourcePaths;
};

class FWBCardDBImporterDiagnosticSummaryForTests
{
public:
	static FWBCardDBImporterDiagnosticSummaryForTest BuildSummary(
		const TArray<FWBCardDBImporterReadinessResultForTest>& Results);

	static bool SummaryToJsonStringForTest(
		const FWBCardDBImporterDiagnosticSummaryForTest& Summary,
		FString& OutJson);
};
