#pragma once

#include "CoreMinimal.h"
#include "WBCardDBSchemaFixtureValidator.h"
#include "WBCardDefinition.h"

struct FWBCardDBImporterReadinessResultForTest
{
	bool bReady = false;
	FString Reason;

	FString SourcePath;
	FString CardDBVersion;
	FString SourceVersion;

	FWBCardDBBundleSchemaValidationResult BundleValidationResult;

	TArray<FString> DependencyOrderCardIds;
	TArray<FWBCardDefinition> OrderedCardDefinitions;
};

class FWBCardDBImporterReadinessForTests
{
public:
	static FWBCardDBImporterReadinessResultForTest EvaluateBundleFileForImportReadiness(
		const FString& AbsolutePath,
		const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions());

	static FWBCardDBImporterReadinessResultForTest EvaluateBundleJsonForImportReadiness(
		const FString& Json,
		const FString& SourcePathForDiagnostics,
		const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions());

	static bool ImporterReadinessResultToJsonStringForTest(
		const FWBCardDBImporterReadinessResultForTest& Result,
		FString& OutJson);
};
