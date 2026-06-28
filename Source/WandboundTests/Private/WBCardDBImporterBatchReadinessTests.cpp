#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDBImporterBatchReadinessForTests.h"
#include "WBCardDBImporterDiagnosticSummaryForTests.h"
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

FString CardDBBundleSchemaFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("Bundles")), FileName);
}

FString CardDBExpectedExportFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("ExpectedExports")), FileName);
}

FWBCardDBSchemaValidationOptions SourceVersionCompatibilityOptions(
	const FString& TargetSourceVersion)
{
	FWBCardDBSchemaValidationOptions Options;
	Options.SourceVersionCompatibility.bEnableSourceVersionCompatibilityValidation = true;
	Options.SourceVersionCompatibility.TargetSourceVersion = TargetSourceVersion;
	return Options;
}

FWBCardDBSchemaValidationOptions SourceVersionTransitionOptions(
	const FString& TargetSourceVersion,
	const FString& FromSourceVersion,
	const FString& ToSourceVersion)
{
	FWBCardDBSchemaValidationOptions Options = SourceVersionCompatibilityOptions(TargetSourceVersion);
	FWBCardDBSourceVersionTransitionForTest Transition;
	Transition.FromSourceVersion = FromSourceVersion;
	Transition.ToSourceVersion = ToSourceVersion;
	Options.SourceVersionCompatibility.SupportedTransitions.Add(Transition);
	return Options;
}

FWBCardDBImporterBatchEntryForTest BatchEntry(
	const FString& Name,
	const FString& BundleFileName,
	const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions())
{
	FWBCardDBImporterBatchEntryForTest Entry;
	Entry.Name = Name;
	Entry.BundlePath = CardDBBundleSchemaFixturePath(BundleFileName);
	Entry.Options = Options;
	return Entry;
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

bool ExportBatch(
	FAutomationTestBase& Test,
	const TArray<FWBCardDBImporterBatchEntryForTest>& Entries,
	FWBCardDBImporterBatchReadinessResultForTest& OutResult,
	FString& OutJson)
{
	OutResult = FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(Entries);
	const bool bExported =
		FWBCardDBImporterBatchReadinessForTests::BatchReadinessResultToJsonStringForTest(OutResult, OutJson);
	Test.TestTrue(TEXT("Batch export succeeds"), bExported);
	return bExported;
}

bool ExpectBatchExportMatches(
	FAutomationTestBase& Test,
	const TArray<FWBCardDBImporterBatchEntryForTest>& Entries,
	const FString& ExpectedExportFileName,
	FWBCardDBImporterBatchReadinessResultForTest& OutResult)
{
	FString ActualJson;
	FString ExpectedJson;
	if (!ExportBatch(Test, Entries, OutResult, ActualJson)
		|| !LoadExpectedExportFixture(Test, ExpectedExportFileName, ExpectedJson))
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("Batch export matches %s"), *ExpectedExportFileName),
		ActualJson,
		ExpectedJson);
	return ActualJson == ExpectedJson;
}

TArray<FWBCardDBImporterBatchEntryForTest> MixedEntries()
{
	return {
		BatchEntry(TEXT("valid_current"), TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))),
		BatchEntry(TEXT("valid_dependency"), TEXT("valid_bundle_dependency_order_simple_chain.json")),
		BatchEntry(TEXT("unsupported_source"), TEXT("invalid_bundle_source_version_unsupported.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.2"))),
		BatchEntry(TEXT("missing_reference"), TEXT("invalid_bundle_dependency_missing_reference_skips_order.json"))
	};
}

TArray<FWBCardDBImporterBatchEntryForTest> AllReadyEntries()
{
	return {
		BatchEntry(TEXT("valid_current"), TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))),
		BatchEntry(TEXT("valid_transition"), TEXT("valid_bundle_source_version_supported_transition.json"), SourceVersionTransitionOptions(TEXT("wandbound_schema.2"), TEXT("legacy_schema.1"), TEXT("wandbound_schema.2"))),
		BatchEntry(TEXT("valid_dependency"), TEXT("valid_bundle_dependency_order_simple_chain.json"))
	};
}

TArray<FWBCardDBImporterBatchEntryForTest> AllNotReadyEntries()
{
	return {
		BatchEntry(TEXT("unsupported_source"), TEXT("invalid_bundle_source_version_unsupported.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.2"))),
		BatchEntry(TEXT("missing_reference"), TEXT("invalid_bundle_dependency_missing_reference_skips_order.json")),
		BatchEntry(TEXT("dependency_cycle"), TEXT("invalid_bundle_dependency_cycle_two_cards.json")),
		BatchEntry(TEXT("invalid_payload"), TEXT("invalid_bundle_contains_invalid_card.json"))
	};
}

TArray<FWBCardDBImporterBatchEntryForTest> SourceVersionMixEntries()
{
	return {
		BatchEntry(TEXT("valid_current"), TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))),
		BatchEntry(TEXT("valid_transition"), TEXT("valid_bundle_source_version_supported_transition.json"), SourceVersionTransitionOptions(TEXT("wandbound_schema.2"), TEXT("legacy_schema.1"), TEXT("wandbound_schema.2"))),
		BatchEntry(TEXT("unsupported_source"), TEXT("invalid_bundle_source_version_unsupported.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.2"))),
		BatchEntry(TEXT("unsupported_transition"), TEXT("invalid_bundle_source_version_transition_unsupported.json"), SourceVersionTransitionOptions(TEXT("wandbound_schema.3"), TEXT("legacy_schema.1"), TEXT("wandbound_schema.2")))
	};
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessEmptyBatchTest, "Wandbound.Core.CardDBImporterBatchReadiness.EmptyBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessEmptyBatchTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterBatchEntryForTest> Entries;
	FWBCardDBImporterBatchReadinessResultForTest Result;
	ExpectBatchExportMatches(*this, Entries, TEXT("importer_batch_readiness_empty.export.json"), Result);

	TestTrue(TEXT("Empty batch evaluates"), Result.bOk);
	TestEqual(TEXT("Empty batch total"), Result.TotalBundles, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessMixedBatchTest, "Wandbound.Core.CardDBImporterBatchReadiness.MixedBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessMixedBatchTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchReadinessResultForTest Result;
	ExpectBatchExportMatches(*this, MixedEntries(), TEXT("importer_batch_readiness_mixed.export.json"), Result);

	TestTrue(TEXT("Mixed batch evaluates"), Result.bOk);
	TestEqual(TEXT("Mixed ready count"), Result.ReadyBundles, 2);
	TestEqual(TEXT("Mixed not-ready count"), Result.NotReadyBundles, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessAllReadyBatchTest, "Wandbound.Core.CardDBImporterBatchReadiness.AllReadyBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessAllReadyBatchTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchReadinessResultForTest Result;
	ExpectBatchExportMatches(*this, AllReadyEntries(), TEXT("importer_batch_readiness_all_ready.export.json"), Result);

	TestTrue(TEXT("All-ready batch evaluates"), Result.bOk);
	TestEqual(TEXT("All-ready count"), Result.ReadyBundles, Result.TotalBundles);
	TestEqual(TEXT("All-ready not-ready count"), Result.NotReadyBundles, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessAllNotReadyBatchTest, "Wandbound.Core.CardDBImporterBatchReadiness.AllNotReadyBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessAllNotReadyBatchTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchReadinessResultForTest Result;
	ExpectBatchExportMatches(*this, AllNotReadyEntries(), TEXT("importer_batch_readiness_all_not_ready.export.json"), Result);

	TestTrue(TEXT("All-not-ready batch evaluates"), Result.bOk);
	TestEqual(TEXT("All-not-ready ready count"), Result.ReadyBundles, 0);
	TestEqual(TEXT("All-not-ready not-ready count"), Result.NotReadyBundles, Result.TotalBundles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessSourceVersionMixBatchTest, "Wandbound.Core.CardDBImporterBatchReadiness.SourceVersionMixBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessSourceVersionMixBatchTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchReadinessResultForTest Result;
	ExpectBatchExportMatches(*this, SourceVersionMixEntries(), TEXT("importer_batch_readiness_source_version_mix.export.json"), Result);

	TestTrue(TEXT("Source-version mix batch evaluates"), Result.bOk);
	TestEqual(TEXT("Source-version mix ready count"), Result.ReadyBundles, 2);
	TestEqual(TEXT("Source-version mix not-ready count"), Result.NotReadyBundles, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessHiddenTokenSafeTest, "Wandbound.Core.CardDBImporterBatchReadiness.HiddenTokenSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterBatchEntryForTest> Entries = {
		BatchEntry(TEXT("hidden_token"), TEXT("invalid_bundle_source_version_hidden_token_safe.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1")))
	};

	FWBCardDBImporterBatchReadinessResultForTest Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportBatch(*this, Entries, Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("importer_batch_readiness_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden-token batch export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden-token batch export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessPreservesInputOrderTest, "Wandbound.Core.CardDBImporterBatchReadiness.PreservesInputOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessPreservesInputOrderTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterBatchEntryForTest> Entries = {
		BatchEntry(TEXT("z_first"), TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))),
		BatchEntry(TEXT("a_second"), TEXT("valid_bundle_dependency_order_simple_chain.json"))
	};

	const FWBCardDBImporterBatchReadinessResultForTest Result =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(Entries);
	TestTrue(TEXT("Order batch evaluates"), Result.bOk);
	TestEqual(TEXT("Order batch preserves two names"), Result.BundleNames.Num(), 2);
	if (Result.BundleNames.Num() < 2)
	{
		return true;
	}

	TestEqual(TEXT("First bundle name preserves input order"), Result.BundleNames[0], FString(TEXT("z_first")));
	TestEqual(TEXT("Second bundle name preserves input order"), Result.BundleNames[1], FString(TEXT("a_second")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessSummaryMatchesHelperTest, "Wandbound.Core.CardDBImporterBatchReadiness.SummaryMatchesHelper", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessSummaryMatchesHelperTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterBatchReadinessResultForTest Result =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(AllNotReadyEntries());
	const FWBCardDBImporterDiagnosticSummaryForTest ExpectedSummary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Result.BundleResults);

	TestEqual(TEXT("Summary total matches helper"), Result.DiagnosticSummary.TotalBundles, ExpectedSummary.TotalBundles);
	TestEqual(TEXT("Summary ready matches helper"), Result.DiagnosticSummary.ReadyBundles, ExpectedSummary.ReadyBundles);
	TestEqual(TEXT("Summary not-ready matches helper"), Result.DiagnosticSummary.NotReadyBundles, ExpectedSummary.NotReadyBundles);
	TestEqual(TEXT("Summary diagnostic count matches helper"), Result.DiagnosticSummary.DiagnosticSummaries.Num(), ExpectedSummary.DiagnosticSummaries.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessMissingPathFailsTest, "Wandbound.Core.CardDBImporterBatchReadiness.MissingPathFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessMissingPathFailsTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterBatchEntryForTest Entry;
	Entry.Name = TEXT("missing");
	Entry.BundlePath = CardDBBundleSchemaFixturePath(TEXT("missing_bundle_file.json"));

	TArray<FWBCardDBImporterBatchEntryForTest> Entries = { Entry };
	const FWBCardDBImporterBatchReadinessResultForTest Result =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(Entries);

	TestFalse(TEXT("Missing path batch fails"), Result.bOk);
	TestEqual(TEXT("Missing path reason"), Result.Reason, FString(TEXT("bundle_path_missing")));
	TestEqual(TEXT("Missing path total count"), Result.TotalBundles, 1);
	TestEqual(TEXT("Missing path has no bundle results"), Result.BundleResults.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessExportDeterministicTest, "Wandbound.Core.CardDBImporterBatchReadiness.ExportDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessExportDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterBatchReadinessResultForTest Result =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(MixedEntries());

	FString FirstJson;
	FString SecondJson;
	TestTrue(TEXT("First batch export"), FWBCardDBImporterBatchReadinessForTests::BatchReadinessResultToJsonStringForTest(Result, FirstJson));
	TestTrue(TEXT("Second batch export"), FWBCardDBImporterBatchReadinessForTests::BatchReadinessResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Batch export deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessHelperNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBImporterBatchReadiness.HelperNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessHelperNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBImporterBatchReadinessForTests"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Batch helper not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessHelperNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBImporterBatchReadiness.HelperNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessHelperNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
{
	TArray<FString> HelperFiles;
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterBatchReadinessForTests.h")));
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterBatchReadinessForTests.cpp")));

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
		TestFalse(*FString::Printf(TEXT("Batch helper omits %s"), *ForbiddenTerm), bFound);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessExistingReadinessStillPassesTest, "Wandbound.Core.CardDBImporterBatchReadiness.ExistingReadinessStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessExistingReadinessStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterReadinessResultForTest Result =
		FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")),
			SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1")));
	TestTrue(TEXT("Existing readiness still passes"), Result.bReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessExistingSummaryStillPassesTest, "Wandbound.Core.CardDBImporterBatchReadiness.ExistingSummaryStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessExistingSummaryStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterBatchReadinessResultForTest Result =
		FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(MixedEntries());
	TestEqual(TEXT("Existing diagnostic summary total"), Result.DiagnosticSummary.TotalBundles, Result.TotalBundles);
	TestEqual(TEXT("Existing diagnostic summary not-ready"), Result.DiagnosticSummary.NotReadyBundles, Result.NotReadyBundles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessExistingBundleValidationStillPassesTest, "Wandbound.Core.CardDBImporterBatchReadiness.ExistingBundleValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessExistingBundleValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));
	TestTrue(TEXT("Existing bundle validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing bundle validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterBatchReadinessExistingRootCardValidationStillPassesTest, "Wandbound.Core.CardDBImporterBatchReadiness.ExistingRootCardValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterBatchReadinessExistingRootCardValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(TEXT("valid_damage_card.json")));
	TestTrue(TEXT("Existing root-card validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing root-card validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}
