#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDBImporterBatchReadinessForTests.h"
#include "WBCardDBImporterDiagnosticSummaryForTests.h"
#include "WBCardDBImporterManifestForTests.h"
#include "WBCardDBImporterManifestSuiteForTests.h"
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

FString CardDBSuiteFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("Suites")), FileName);
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

bool ExportSuite(
	FAutomationTestBase& Test,
	const FString& SuiteFileName,
	FWBCardDBImporterManifestSuiteEvaluationResultForTest& OutResult,
	FString& OutJson)
{
	OutResult = FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
		CardDBSuiteFixturePath(SuiteFileName));
	const bool bExported =
		FWBCardDBImporterManifestSuiteForTests::SuiteEvaluationResultToJsonStringForTest(OutResult, OutJson);
	Test.TestTrue(*FString::Printf(TEXT("%s suite export succeeds"), *SuiteFileName), bExported);
	return bExported;
}

bool ExpectSuiteExportMatches(
	FAutomationTestBase& Test,
	const FString& SuiteFileName,
	const FString& ExpectedExportFileName,
	FWBCardDBImporterManifestSuiteEvaluationResultForTest& OutResult)
{
	FString ActualJson;
	FString ExpectedJson;
	if (!ExportSuite(Test, SuiteFileName, OutResult, ActualJson)
		|| !LoadExpectedExportFixture(Test, ExpectedExportFileName, ExpectedJson))
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s export matches %s"), *SuiteFileName, *ExpectedExportFileName),
		ActualJson,
		ExpectedJson);
	return ActualJson == ExpectedJson;
}

FWBCardDBImporterManifestSuiteValidationResultForTest ValidateSuite(const FString& SuiteFileName)
{
	return FWBCardDBImporterManifestSuiteForTests::ValidateSuiteFile(
		CardDBSuiteFixturePath(SuiteFileName));
}

bool ExpectSingleDiagnostic(
	FAutomationTestBase& Test,
	const FString& SuiteFileName,
	EWBCardDBImporterManifestSuiteDiagnostic ExpectedDiagnostic)
{
	const FWBCardDBImporterManifestSuiteValidationResultForTest Result =
		ValidateSuite(SuiteFileName);
	Test.TestFalse(*FString::Printf(TEXT("%s validation fails"), *SuiteFileName), Result.bOk);
	Test.TestEqual(*FString::Printf(TEXT("%s diagnostic count"), *SuiteFileName), Result.Diagnostics.Num(), 1);
	if (Result.Diagnostics.Num() == 0)
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s diagnostic code"), *SuiteFileName),
		FWBCardDBImporterManifestSuiteForTests::SuiteDiagnosticCodeToString(Result.Diagnostics[0].Code),
		FWBCardDBImporterManifestSuiteForTests::SuiteDiagnosticCodeToString(ExpectedDiagnostic));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteValidMixedSuiteTest, "Wandbound.Core.CardDBImporterManifestSuite.ValidMixedSuite", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteValidMixedSuiteTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("valid_suite_mixed_manifests.json"),
		TEXT("importer_suite_valid_mixed_manifests.export.json"),
		Result);

	TestTrue(TEXT("Mixed suite evaluates"), Result.bOk);
	TestEqual(TEXT("Mixed suite manifest count"), Result.TotalManifests, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteValidAllReadySuiteTest, "Wandbound.Core.CardDBImporterManifestSuite.ValidAllReadySuite", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteValidAllReadySuiteTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("valid_suite_all_ready.json"),
		TEXT("importer_suite_valid_all_ready.export.json"),
		Result);

	TestTrue(TEXT("All-ready suite evaluates"), Result.bOk);
	TestEqual(TEXT("All-ready suite not-ready count"), Result.NotReadyBundles, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteValidWithNotReadyBundlesTest, "Wandbound.Core.CardDBImporterManifestSuite.ValidWithNotReadyBundles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteValidWithNotReadyBundlesTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("valid_suite_with_not_ready_bundles.json"),
		TEXT("importer_suite_valid_with_not_ready_bundles.export.json"),
		Result);

	TestTrue(TEXT("Suite with not-ready bundles evaluates"), Result.bOk);
	TestEqual(TEXT("Suite with not-ready bundle count"), Result.NotReadyBundles, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteMalformedJsonFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.MalformedJsonFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteMalformedJsonFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_malformed_json.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteJsonParseFailed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteMissingSchemaVersionFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.MissingSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteMissingSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("invalid_suite_missing_schema_version.json"),
		TEXT("importer_suite_invalid_missing_schema_version.export.json"),
		Result);

	TestFalse(TEXT("Missing schema version fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteUnsupportedSchemaVersionFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.UnsupportedSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteUnsupportedSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_unsupported_schema_version.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteSchemaVersionUnsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteMissingSuiteIdFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.MissingSuiteIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteMissingSuiteIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_missing_id.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteIdMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteMissingManifestsFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.MissingManifestsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteMissingManifestsFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_manifests_missing.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteManifestsNotArrayFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.ManifestsNotArrayFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteManifestsNotArrayFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_manifests_not_array.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteDuplicateManifestNameFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.DuplicateManifestNameFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteDuplicateManifestNameFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("invalid_suite_manifest_name_duplicate.json"),
		TEXT("importer_suite_invalid_duplicate_manifest_name.export.json"),
		Result);

	TestFalse(TEXT("Duplicate manifest name fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteMissingManifestPathFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.MissingManifestPathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteMissingManifestPathFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_manifest_path_missing.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuitePathTraversalFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.PathTraversalFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuitePathTraversalFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("invalid_suite_manifest_path_traversal.json"),
		TEXT("importer_suite_invalid_path_traversal.export.json"),
		Result);

	TestFalse(TEXT("Path traversal fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteAbsolutePathFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.AbsolutePathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteAbsolutePathFailsTest::RunTest(const FString& Parameters)
{
	ExpectSingleDiagnostic(
		*this,
		TEXT("invalid_suite_manifest_path_absolute.json"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathInvalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteNotFoundPathFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.NotFoundPathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteNotFoundPathFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("invalid_suite_manifest_path_not_found.json"),
		TEXT("importer_suite_invalid_path_not_found.export.json"),
		Result);

	TestFalse(TEXT("Not-found manifest path fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteMalformedMetadataFailsTest, "Wandbound.Core.CardDBImporterManifestSuite.MalformedMetadataFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteMalformedMetadataFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	ExpectSuiteExportMatches(
		*this,
		TEXT("invalid_suite_metadata_malformed.json"),
		TEXT("importer_suite_invalid_metadata_malformed.export.json"),
		Result);

	TestFalse(TEXT("Malformed metadata fails"), Result.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteHiddenTokenSafeTest, "Wandbound.Core.CardDBImporterManifestSuite.HiddenTokenSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportSuite(*this, TEXT("invalid_suite_hidden_token_safe.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("importer_suite_invalid_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden-token suite fails"), Result.bOk);
	TestFalse(TEXT("Hidden-token suite export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden-token suite export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuitePreservesManifestOrderTest, "Wandbound.Core.CardDBImporterManifestSuite.PreservesManifestOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuitePreservesManifestOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestSuiteEvaluationResultForTest Result =
		FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
			CardDBSuiteFixturePath(TEXT("valid_suite_mixed_manifests.json")));

	TestTrue(TEXT("Order suite evaluates"), Result.bOk);
	TestEqual(TEXT("Order suite entries count"), Result.SuiteValidationResult.ManifestEntries.Num(), 2);
	if (Result.SuiteValidationResult.ManifestEntries.Num() < 2)
	{
		return true;
	}

	TestEqual(TEXT("First manifest alias"), Result.SuiteValidationResult.ManifestEntries[0].Name, FString(TEXT("mixed_manifest")));
	TestEqual(TEXT("Second manifest alias"), Result.SuiteValidationResult.ManifestEntries[1].Name, FString(TEXT("all_ready_manifest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteValidationFailureSkipsManifestEvaluationTest, "Wandbound.Core.CardDBImporterManifestSuite.ValidationFailureSkipsManifestEvaluation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteValidationFailureSkipsManifestEvaluationTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestSuiteEvaluationResultForTest Result =
		FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
			CardDBSuiteFixturePath(TEXT("invalid_suite_manifest_path_not_found.json")));

	TestFalse(TEXT("Invalid suite evaluation fails"), Result.bOk);
	TestEqual(TEXT("Invalid suite skips manifest evaluation"), Result.ManifestResults.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteAggregatesCountsTest, "Wandbound.Core.CardDBImporterManifestSuite.AggregatesCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteAggregatesCountsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestSuiteEvaluationResultForTest Result =
		FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
			CardDBSuiteFixturePath(TEXT("valid_suite_mixed_manifests.json")));

	TestTrue(TEXT("Aggregate suite evaluates"), Result.bOk);
	TestEqual(TEXT("Aggregate total manifests"), Result.TotalManifests, 2);
	TestEqual(TEXT("Aggregate valid manifests"), Result.ValidManifests, 2);
	TestEqual(TEXT("Aggregate invalid manifests"), Result.InvalidManifests, 0);
	TestEqual(TEXT("Aggregate total batches"), Result.TotalBatches, 3);
	TestEqual(TEXT("Aggregate total bundles"), Result.TotalBundles, 8);
	TestEqual(TEXT("Aggregate ready bundles"), Result.ReadyBundles, 6);
	TestEqual(TEXT("Aggregate not-ready bundles"), Result.NotReadyBundles, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteAggregateSummaryMatchesExpectedTest, "Wandbound.Core.CardDBImporterManifestSuite.AggregateSummaryMatchesExpected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteAggregateSummaryMatchesExpectedTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestSuiteEvaluationResultForTest Result =
		FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
			CardDBSuiteFixturePath(TEXT("valid_suite_mixed_manifests.json")));

	TestTrue(TEXT("Summary suite evaluates"), Result.bOk);
	TestEqual(TEXT("Aggregate reason summary count"), Result.AggregateDiagnosticSummary.ReasonSummaries.Num(), 1);
	TestEqual(TEXT("Aggregate diagnostic summary count"), Result.AggregateDiagnosticSummary.DiagnosticSummaries.Num(), 2);
	if (Result.AggregateDiagnosticSummary.ReasonSummaries.Num() > 0)
	{
		TestEqual(TEXT("Aggregate reason"), Result.AggregateDiagnosticSummary.ReasonSummaries[0].Reason, FString(TEXT("bundle_validation_failed")));
		TestEqual(TEXT("Aggregate reason count"), Result.AggregateDiagnosticSummary.ReasonSummaries[0].Count, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteExportDeterministicTest, "Wandbound.Core.CardDBImporterManifestSuite.ExportDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteExportDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestSuiteEvaluationResultForTest Result =
		FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
			CardDBSuiteFixturePath(TEXT("valid_suite_mixed_manifests.json")));

	FString FirstJson;
	FString SecondJson;
	TestTrue(TEXT("First suite export"), FWBCardDBImporterManifestSuiteForTests::SuiteEvaluationResultToJsonStringForTest(Result, FirstJson));
	TestTrue(TEXT("Second suite export"), FWBCardDBImporterManifestSuiteForTests::SuiteEvaluationResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Suite export deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteHelperNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBImporterManifestSuite.HelperNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteHelperNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBImporterManifestSuiteForTests"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Suite helper not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteHelperNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBImporterManifestSuite.HelperNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteHelperNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
{
	TArray<FString> HelperFiles;
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterManifestSuiteForTests.h")));
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterManifestSuiteForTests.cpp")));

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
		TestFalse(*FString::Printf(TEXT("Suite helper omits %s"), *ForbiddenTerm), bFound);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteExistingManifestStillPassesTest, "Wandbound.Core.CardDBImporterManifestSuite.ExistingManifestStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteExistingManifestStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterManifestBatchEvaluationResultForTest Result =
		FWBCardDBImporterManifestForTests::EvaluateManifestFile(
			CardDBManifestFixturePath(TEXT("valid_manifest_all_ready.json")));

	TestTrue(TEXT("Existing manifest still passes"), Result.bOk);
	TestEqual(TEXT("Existing manifest batch count"), Result.BatchResults.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteExistingBatchReadinessStillPassesTest, "Wandbound.Core.CardDBImporterManifestSuite.ExistingBatchReadinessStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteExistingBatchReadinessStillPassesTest::RunTest(const FString& Parameters)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteExistingDiagnosticSummaryStillPassesTest, "Wandbound.Core.CardDBImporterManifestSuite.ExistingDiagnosticSummaryStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteExistingDiagnosticSummaryStillPassesTest::RunTest(const FString& Parameters)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteExistingReadinessStillPassesTest, "Wandbound.Core.CardDBImporterManifestSuite.ExistingReadinessStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteExistingReadinessStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterReadinessResultForTest Result =
		FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));

	TestTrue(TEXT("Existing readiness still passes"), Result.bReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterManifestSuiteExistingBundleValidationStillPassesTest, "Wandbound.Core.CardDBImporterManifestSuite.ExistingBundleValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterManifestSuiteExistingBundleValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));

	TestTrue(TEXT("Existing bundle validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing bundle validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}
