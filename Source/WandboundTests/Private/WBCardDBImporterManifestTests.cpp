#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDBImporterBatchReadinessForTests.h"
#include "WBCardDBImporterDiagnosticSummaryForTests.h"
#include "WBCardDBImporterManifestForTests.h"
#include "WBCardDBImporterReadinessForTests.h"
#include "WBCardDBSchemaFixtureValidator.h"

namespace
{
FString CardDBSchemaFixturePath(const FString& FileName)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Reference"),
		TEXT("GodotCanon"),
		TEXT("CardDBSchemaFixtures"),
		FileName);
}

FString CardDBManifestFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("Manifests")), FileName);
}

FString CardDBBundleSchemaFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("Bundles")), FileName);
}

FString CardDBExpectedExportFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("ExpectedExports")), FileName);
}

bool LoadExpectedExportFixture(
	FAutomationTestBase& Test,
	const FString& FileName,
	FString& OutJson)
{
	const bool bLoaded = FFileHelper::LoadFileToString(OutJson, *CardDBExpectedExportFixturePath(FileName));
	Test.TestTrue(*FString::Printf(TEXT("%s expected export loads"), *FileName), bLoaded);
	OutJson.TrimStartAndEndInline();
	return bLoaded;
}

bool ExportManifest(
	FAutomationTestBase& Test,
	const FString& ManifestFileName,
	FWBCardDBImporterManifestBatchEvaluationResultForTest& OutResult,
	FString& OutJson)
{
	OutResult = FWBCardDBImporterManifestForTests::EvaluateManifestFile(
		CardDBManifestFixturePath(ManifestFileName));
	const bool bExported =
		FWBCardDBImporterManifestForTests::ManifestEvaluationResultToJsonStringForTest(OutResult, OutJson);
	Test.TestTrue(*FString::Printf(TEXT("%s manifest export succeeds"), *ManifestFileName), bExported);
	return bExported;
}

bool ExpectManifestExportMatches(
	FAutomationTestBase& Test,
	const FString& ManifestFileName,
	const FString& ExpectedExportFileName,
	FWBCardDBImporterManifestBatchEvaluationResultForTest& OutResult)
{
	FString ActualJson;
	FString ExpectedJson;
	if (!ExportManifest(Test, ManifestFileName, OutResult, ActualJson)
		|| !LoadExpectedExportFixture(Test, ExpectedExportFileName, ExpectedJson))
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s export matches %s"), *ManifestFileName, *ExpectedExportFileName),
		ActualJson,
		ExpectedJson);
	return ActualJson == ExpectedJson;
}

FWBCardDBImporterManifestValidationResultForTest ValidateManifest(const FString& ManifestFileName)
{
	return FWBCardDBImporterManifestForTests::ValidateManifestFile(
		CardDBManifestFixturePath(ManifestFileName));
}

bool ExpectSingleDiagnostic(
	FAutomationTestBase& Test,
	const FString& ManifestFileName,
	EWBCardDBImporterManifestDiagnostic ExpectedDiagnostic)
{
	const FWBCardDBImporterManifestValidationResultForTest Result =
		ValidateManifest(ManifestFileName);
	Test.TestFalse(*FString::Printf(TEXT("%s validation fails"), *ManifestFileName), Result.bOk);
	Test.TestEqual(*FString::Printf(TEXT("%s diagnostic count"), *ManifestFileName), Result.Diagnostics.Num(), 1);
	if (Result.Diagnostics.Num() == 0)
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s diagnostic code"), *ManifestFileName),
		FWBCardDBImporterManifestForTests::ManifestDiagnosticCodeToString(Result.Diagnostics[0].Code),
		FWBCardDBImporterManifestForTests::ManifestDiagnosticCodeToString(ExpectedDiagnostic));
	return Result.Diagnostics[0].Code == ExpectedDiagnostic;
}

void FindSourceFiles(const FString& RelativeDirectory, TArray<FString>& OutFiles)
{
	const FString AbsoluteDirectory = FPaths::Combine(FPaths::ProjectDir(), RelativeDirectory);
	IFileManager::Get().FindFilesRecursive(OutFiles, *AbsoluteDirectory, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(OutFiles, *AbsoluteDirectory, TEXT("*.cpp"), true, false);
}

bool AnyFileContains(const TArray<FString>& Files, const FString& Needle, FString& OutFile)
{
	for (const FString& File : Files)
	{
		FString Contents;
		if (FFileHelper::LoadFileToString(Contents, *File)
			&& Contents.Contains(Needle, ESearchCase::CaseSensitive))
		{
			OutFile = File;
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestValidMixedManifestTest, "Wandbound.Core.CardDBImporterManifest.ValidMixedManifest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestValidMixedManifestTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("valid_manifest_mixed_batches.json"),
		TEXT("importer_manifest_valid_mixed_batches.export.json"),
		Result);

	TestTrue(TEXT("Mixed manifest evaluates"), Result.bOk);
	TestEqual(TEXT("Mixed manifest batch count"), Result.BatchResults.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestValidAllReadyManifestTest, "Wandbound.Core.CardDBImporterManifest.ValidAllReadyManifest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestValidAllReadyManifestTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("valid_manifest_all_ready.json"),
		TEXT("importer_manifest_valid_all_ready.export.json"),
		Result);

	TestTrue(TEXT("All-ready manifest evaluates"), Result.bOk);
	TestEqual(TEXT("All-ready manifest has one batch"), Result.BatchResults.Num(), 1);
	if (Result.BatchResults.Num() < 1)
	{
		return true;
	}

	TestEqual(TEXT("All-ready manifest not-ready count"), Result.BatchResults[0].NotReadyBundles, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSourceVersionOverridesTest, "Wandbound.Core.CardDBImporterManifest.SourceVersionOverrides", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSourceVersionOverridesTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("valid_manifest_source_version_overrides.json"),
		TEXT("importer_manifest_valid_source_version_overrides.export.json"),
		Result);

	TestTrue(TEXT("Override manifest evaluates"), Result.bOk);
	TestEqual(TEXT("Override manifest batch count"), Result.BatchResults.Num(), 3);
	for (const FWBCardDBImporterBatchReadinessResultForTest& BatchResult : Result.BatchResults)
	{
		TestEqual(TEXT("Override batch all ready"), BatchResult.ReadyBundles, BatchResult.TotalBundles);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMalformedJsonFailsTest, "Wandbound.Core.CardDBImporterManifest.MalformedJsonFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMalformedJsonFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_malformed_json.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestJsonParseFailed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMissingSchemaVersionFailsTest, "Wandbound.Core.CardDBImporterManifest.MissingSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMissingSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("invalid_manifest_missing_schema_version.json"),
		TEXT("importer_manifest_invalid_missing_schema_version.export.json"),
		Result);

	TestFalse(TEXT("Missing schema version fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestUnsupportedSchemaVersionFailsTest, "Wandbound.Core.CardDBImporterManifest.UnsupportedSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestUnsupportedSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_unsupported_schema_version.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestSchemaVersionUnsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMissingManifestIdFailsTest, "Wandbound.Core.CardDBImporterManifest.MissingManifestIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMissingManifestIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_missing_id.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestIdMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMissingBatchesFailsTest, "Wandbound.Core.CardDBImporterManifest.MissingBatchesFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMissingBatchesFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_batches_missing.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestBatchesMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestBatchesNotArrayFailsTest, "Wandbound.Core.CardDBImporterManifest.BatchesNotArrayFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestBatchesNotArrayFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_batches_not_array.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestBatchesMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestDuplicateBatchNameFailsTest, "Wandbound.Core.CardDBImporterManifest.DuplicateBatchNameFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestDuplicateBatchNameFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("invalid_manifest_batch_name_duplicate.json"),
		TEXT("importer_manifest_invalid_duplicate_batch_name.export.json"),
		Result);

	TestFalse(TEXT("Duplicate batch name fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestDuplicateBundleNameFailsTest, "Wandbound.Core.CardDBImporterManifest.DuplicateBundleNameFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestDuplicateBundleNameFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("invalid_manifest_bundle_name_duplicate.json"),
		TEXT("importer_manifest_invalid_duplicate_bundle_name.export.json"),
		Result);

	TestFalse(TEXT("Duplicate bundle name fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMissingBundlePathFailsTest, "Wandbound.Core.CardDBImporterManifest.MissingBundlePathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMissingBundlePathFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_bundle_path_missing.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestBundlePathMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestPathTraversalFailsTest, "Wandbound.Core.CardDBImporterManifest.PathTraversalFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestPathTraversalFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("invalid_manifest_bundle_path_traversal.json"),
		TEXT("importer_manifest_invalid_path_traversal.export.json"),
		Result);

	TestFalse(TEXT("Path traversal fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestAbsolutePathFailsTest, "Wandbound.Core.CardDBImporterManifest.AbsolutePathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestAbsolutePathFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_bundle_path_absolute.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestBundlePathInvalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestNotFoundPathFailsTest, "Wandbound.Core.CardDBImporterManifest.NotFoundPathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestNotFoundPathFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("invalid_manifest_bundle_path_not_found.json"),
		TEXT("importer_manifest_invalid_path_not_found.export.json"),
		Result);

	TestFalse(TEXT("Not-found path fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMalformedCompatibilityFailsTest, "Wandbound.Core.CardDBImporterManifest.MalformedCompatibilityFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMalformedCompatibilityFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	ExpectManifestExportMatches(
		*this,
		TEXT("invalid_manifest_compatibility_malformed.json"),
		TEXT("importer_manifest_invalid_compatibility_malformed.export.json"),
		Result);

	TestFalse(TEXT("Malformed compatibility fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestMalformedMetadataFailsTest, "Wandbound.Core.CardDBImporterManifest.MalformedMetadataFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestMalformedMetadataFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_manifest_metadata_malformed.json"),
		EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestHiddenTokenSafeTest, "Wandbound.Core.CardDBImporterManifest.HiddenTokenSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportManifest(*this, TEXT("invalid_manifest_hidden_token_safe.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("importer_manifest_invalid_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden-token manifest fails"), Result.bOk);
	TestFalse(TEXT("Hidden-token manifest export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden-token manifest export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestPreservesBatchAndBundleOrderTest, "Wandbound.Core.CardDBImporterManifest.PreservesBatchAndBundleOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestPreservesBatchAndBundleOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestBatchEvaluationResultForTest Result =
		FWBCardDBImporterManifestForTests::EvaluateManifestFile(
			CardDBManifestFixturePath(TEXT("valid_manifest_mixed_batches.json")));

	TestTrue(TEXT("Order manifest evaluates"), Result.bOk);
	TestEqual(TEXT("Order manifest batch count"), Result.ManifestValidationResult.Batches.Num(), 2);
	TestEqual(TEXT("Order result batch count"), Result.BatchResults.Num(), 2);
	if (Result.ManifestValidationResult.Batches.Num() < 2 || Result.BatchResults.Num() < 2)
	{
		return true;
	}

	TestEqual(TEXT("Mixed batch bundle count"), Result.BatchResults[1].BundleNames.Num(), 3);
	if (Result.BatchResults[1].BundleNames.Num() < 3)
	{
		return true;
	}

	TestEqual(TEXT("First batch name"), Result.ManifestValidationResult.Batches[0].BatchName, FString(TEXT("core_ready")));
	TestEqual(TEXT("Second batch name"), Result.ManifestValidationResult.Batches[1].BatchName, FString(TEXT("mixed")));
	TestEqual(TEXT("First mixed bundle name"), Result.BatchResults[1].BundleNames[0], FString(TEXT("valid_current")));
	TestEqual(TEXT("Second mixed bundle name"), Result.BatchResults[1].BundleNames[1], FString(TEXT("unsupported_source")));
	TestEqual(TEXT("Third mixed bundle name"), Result.BatchResults[1].BundleNames[2], FString(TEXT("missing_reference")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestValidationFailureSkipsBatchEvaluationTest, "Wandbound.Core.CardDBImporterManifest.ValidationFailureSkipsBatchEvaluation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestValidationFailureSkipsBatchEvaluationTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestBatchEvaluationResultForTest Result =
		FWBCardDBImporterManifestForTests::EvaluateManifestFile(
			CardDBManifestFixturePath(TEXT("invalid_manifest_bundle_path_not_found.json")));

	TestFalse(TEXT("Invalid manifest evaluation fails"), Result.bOk);
	TestEqual(TEXT("Invalid manifest skips batch evaluation"), Result.BatchResults.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestNotReadyBundlesStillEvaluateTest, "Wandbound.Core.CardDBImporterManifest.NotReadyBundlesStillEvaluate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestNotReadyBundlesStillEvaluateTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestBatchEvaluationResultForTest Result =
		FWBCardDBImporterManifestForTests::EvaluateManifestFile(
			CardDBManifestFixturePath(TEXT("valid_manifest_mixed_batches.json")));

	TestTrue(TEXT("Manifest with not-ready bundles evaluates"), Result.bOk);
	TestEqual(TEXT("Manifest with not-ready has two batches"), Result.BatchResults.Num(), 2);
	if (Result.BatchResults.Num() < 2)
	{
		return true;
	}

	TestEqual(TEXT("Mixed batch not-ready count"), Result.BatchResults[1].NotReadyBundles, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestExportDeterministicTest, "Wandbound.Core.CardDBImporterManifest.ExportDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestExportDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestBatchEvaluationResultForTest Result =
		FWBCardDBImporterManifestForTests::EvaluateManifestFile(
			CardDBManifestFixturePath(TEXT("valid_manifest_mixed_batches.json")));

	FString FirstJson;
	FString SecondJson;
	TestTrue(TEXT("First manifest export"), FWBCardDBImporterManifestForTests::ManifestEvaluationResultToJsonStringForTest(Result, FirstJson));
	TestTrue(TEXT("Second manifest export"), FWBCardDBImporterManifestForTests::ManifestEvaluationResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Manifest export deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestHelperNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBImporterManifest.HelperNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestHelperNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBImporterManifestForTests"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Manifest helper not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestHelperNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBImporterManifest.HelperNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestHelperNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
{
	TArray<FString> HelperFiles;
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterManifestForTests.h")));
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterManifestForTests.cpp")));

	const FString ForbiddenTerms[] = {
		TEXT("WBEffectRunner"),
		TEXT("GenerateLegalActions"),
		TEXT("WBCardActivationCandidateGenerator"),
		TEXT("WBCardActivationLegalActionGenerator")
	};

	for (const FString& ForbiddenTerm : ForbiddenTerms)
	{
		FString FoundFile;
		const bool bFound = AnyFileContains(HelperFiles, ForbiddenTerm, FoundFile);
		TestFalse(*FString::Printf(TEXT("Manifest helper omits %s"), *ForbiddenTerm), bFound);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestExistingBatchReadinessStillPassesTest, "Wandbound.Core.CardDBImporterManifest.ExistingBatchReadinessStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestExistingBatchReadinessStillPassesTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchEntryForTest Entry;
	Entry.Name = TEXT("valid_current");
	Entry.BundlePath = CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json"));

	TArray<FWBCardDBImporterBatchEntryForTest> Entries = { Entry };
	const FWBCardDBImporterBatchReadinessResultForTest Result =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(Entries);

	TestTrue(TEXT("Existing batch readiness still passes"), Result.bOk);
	TestEqual(TEXT("Existing batch ready count"), Result.ReadyBundles, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestExistingDiagnosticSummaryStillPassesTest, "Wandbound.Core.CardDBImporterManifest.ExistingDiagnosticSummaryStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestExistingDiagnosticSummaryStillPassesTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchEntryForTest Entry;
	Entry.Name = TEXT("valid_current");
	Entry.BundlePath = CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json"));

	TArray<FWBCardDBImporterBatchEntryForTest> Entries = { Entry };
	const FWBCardDBImporterBatchReadinessResultForTest BatchResult =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(Entries);
	const FWBCardDBImporterDiagnosticSummaryForTest Summary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(BatchResult.BundleResults);

	TestEqual(TEXT("Existing summary total"), Summary.TotalBundles, 1);
	TestEqual(TEXT("Existing summary ready count"), Summary.ReadyBundles, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestExistingReadinessStillPassesTest, "Wandbound.Core.CardDBImporterManifest.ExistingReadinessStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestExistingReadinessStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterReadinessResultForTest Result =
		FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));

	TestTrue(TEXT("Existing readiness still passes"), Result.bReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestExistingBundleValidationStillPassesTest, "Wandbound.Core.CardDBImporterManifest.ExistingBundleValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestExistingBundleValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));

	TestTrue(TEXT("Existing bundle validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing bundle validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}
