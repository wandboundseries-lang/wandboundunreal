#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

FWBCardDBImporterReadinessResultForTest EvaluateReadiness(
	const FString& BundleFileName,
	const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions())
{
	return FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
		CardDBBundleSchemaFixturePath(BundleFileName),
		Options);
}

FWBCardDBImporterReadinessResultForTest SyntheticNotReadyResult(
	const FString& SourcePath,
	const FString& Reason)
{
	FWBCardDBImporterReadinessResultForTest Result;
	Result.bReady = false;
	Result.Reason = Reason;
	Result.SourcePath = SourcePath;
	return Result;
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

bool ExportSummary(
	FAutomationTestBase& Test,
	const TArray<FWBCardDBImporterReadinessResultForTest>& Results,
	FWBCardDBImporterDiagnosticSummaryForTest& OutSummary,
	FString& OutJson)
{
	OutSummary = FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Results);
	const bool bExported =
		FWBCardDBImporterDiagnosticSummaryForTests::SummaryToJsonStringForTest(OutSummary, OutJson);
	Test.TestTrue(TEXT("Summary export succeeds"), bExported);
	return bExported;
}

bool ExpectSummaryExportMatches(
	FAutomationTestBase& Test,
	const TArray<FWBCardDBImporterReadinessResultForTest>& Results,
	const FString& ExpectedExportFileName,
	FWBCardDBImporterDiagnosticSummaryForTest& OutSummary)
{
	FString ActualJson;
	FString ExpectedJson;
	if (!ExportSummary(Test, Results, OutSummary, ActualJson)
		|| !LoadExpectedExportFixture(Test, ExpectedExportFileName, ExpectedJson))
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("Summary export matches %s"), *ExpectedExportFileName),
		ActualJson,
		ExpectedJson);
	return ActualJson == ExpectedJson;
}

bool IsSortedAscending(const TArray<FString>& Values)
{
	for (int32 Index = 1; Index < Values.Num(); ++Index)
	{
		if (Values[Index - 1] > Values[Index])
		{
			return false;
		}
	}

	return true;
}

TArray<FString> ReasonNames(const TArray<FWBCardDBImporterReasonSummaryForTest>& ReasonSummaries)
{
	TArray<FString> Names;
	for (const FWBCardDBImporterReasonSummaryForTest& Summary : ReasonSummaries)
	{
		Names.Add(Summary.Reason);
	}
	return Names;
}

TArray<FString> DiagnosticNames(const TArray<FWBCardDBImporterDiagnosticCodeSummaryForTest>& DiagnosticSummaries)
{
	TArray<FString> Names;
	for (const FWBCardDBImporterDiagnosticCodeSummaryForTest& Summary : DiagnosticSummaries)
	{
		Names.Add(Summary.DiagnosticCode);
	}
	return Names;
}

const FWBCardDBImporterDiagnosticCodeSummaryForTest* FindDiagnosticSummary(
	const FWBCardDBImporterDiagnosticSummaryForTest& Summary,
	const FString& DiagnosticCode)
{
	for (const FWBCardDBImporterDiagnosticCodeSummaryForTest& DiagnosticSummary : Summary.DiagnosticSummaries)
	{
		if (DiagnosticSummary.DiagnosticCode == DiagnosticCode)
		{
			return &DiagnosticSummary;
		}
	}

	return nullptr;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryMixedReadinessTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.MixedReadiness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryMixedReadinessTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))));
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_dependency_order_simple_chain.json")));
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_source_version_unsupported.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.2"))));
	Results.Add(SyntheticNotReadyResult(TEXT("synthetic_dependency_order_missing.json"), TEXT("dependency_order_missing")));
	Results.Add(SyntheticNotReadyResult(TEXT("synthetic_ordered_card_missing.json"), TEXT("ordered_card_missing")));

	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	ExpectSummaryExportMatches(
		*this,
		Results,
		TEXT("importer_diagnostic_summary_mixed_readiness.export.json"),
		Summary);

	TestEqual(TEXT("Mixed total"), Summary.TotalBundles, 5);
	TestEqual(TEXT("Mixed ready"), Summary.ReadyBundles, 2);
	TestEqual(TEXT("Mixed not ready"), Summary.NotReadyBundles, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummarySchemaFailuresTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.SchemaFailures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummarySchemaFailuresTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_source_version_unsupported.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.2"))));
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_dependency_missing_reference_skips_order.json")));
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_dependency_cycle_two_cards.json")));
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_contains_invalid_card.json")));

	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	ExpectSummaryExportMatches(
		*this,
		Results,
		TEXT("importer_diagnostic_summary_schema_failures.export.json"),
		Summary);

	TestEqual(TEXT("Schema failure diagnostic groups"), Summary.DiagnosticSummaries.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryCardContextCountsTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.CardContextCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryCardContextCountsTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_multiple_invalid_cards.json")));

	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	ExpectSummaryExportMatches(
		*this,
		Results,
		TEXT("importer_diagnostic_summary_card_context_counts.export.json"),
		Summary);

	const FWBCardDBImporterDiagnosticCodeSummaryForTest* UnsupportedPayload =
		FindDiagnosticSummary(Summary, TEXT("unsupported_payload_type"));
	TestTrue(TEXT("Unsupported payload summary exists"), UnsupportedPayload != nullptr);
	if (UnsupportedPayload != nullptr)
	{
		TestEqual(TEXT("Unsupported payload affected cards"), UnsupportedPayload->AffectedCardCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryAllReadyTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.AllReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryAllReadyTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))));
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_source_version_supported_transition.json"), SourceVersionTransitionOptions(TEXT("wandbound_schema.2"), TEXT("legacy_schema.1"), TEXT("wandbound_schema.2"))));
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_dependency_order_simple_chain.json")));

	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	ExpectSummaryExportMatches(
		*this,
		Results,
		TEXT("importer_diagnostic_summary_all_ready.export.json"),
		Summary);

	TestEqual(TEXT("All-ready reason summaries empty"), Summary.ReasonSummaries.Num(), 0);
	TestEqual(TEXT("All-ready diagnostic summaries empty"), Summary.DiagnosticSummaries.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryHiddenTokenSafeTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.HiddenTokenSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_source_version_hidden_token_safe.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))));

	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	FString ActualJson;
	FString ExpectedJson;
	ExportSummary(*this, Results, Summary, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("importer_diagnostic_summary_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden summary omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden summary export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryOutputDeterministicTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.OutputDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryOutputDeterministicTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_dependency_cycle_two_cards.json")));
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_dependency_order_simple_chain.json")));

	const FWBCardDBImporterDiagnosticSummaryForTest Summary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Results);

	FString FirstJson;
	FString SecondJson;
	TestTrue(TEXT("First summary export"), FWBCardDBImporterDiagnosticSummaryForTests::SummaryToJsonStringForTest(Summary, FirstJson));
	TestTrue(TEXT("Second summary export"), FWBCardDBImporterDiagnosticSummaryForTests::SummaryToJsonStringForTest(Summary, SecondJson));
	TestEqual(TEXT("Summary export deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryReasonSortedTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.ReasonSorted", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryReasonSortedTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(SyntheticNotReadyResult(TEXT("zeta.json"), TEXT("ordered_card_missing")));
	Results.Add(SyntheticNotReadyResult(TEXT("alpha.json"), TEXT("bundle_validation_failed")));
	Results.Add(SyntheticNotReadyResult(TEXT("middle.json"), TEXT("dependency_order_missing")));

	const FWBCardDBImporterDiagnosticSummaryForTest Summary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Results);
	TestTrue(TEXT("Reason summaries sorted"), IsSortedAscending(ReasonNames(Summary.ReasonSummaries)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryDiagnosticSortedTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.DiagnosticSorted", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryDiagnosticSortedTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("invalid_bundle_multiple_invalid_cards.json")));

	const FWBCardDBImporterDiagnosticSummaryForTest Summary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Results);
	TestTrue(TEXT("Diagnostic summaries sorted"), IsSortedAscending(DiagnosticNames(Summary.DiagnosticSummaries)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummarySourcePathsSortedAndSanitizedTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.SourcePathsSortedAndSanitized", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummarySourcePathsSortedAndSanitizedTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1"))));
	Results.Add(EvaluateReadiness(TEXT("valid_bundle_dependency_order_simple_chain.json")));
	Results.Add(SyntheticNotReadyResult(TEXT("zeta_SECRET_bundle.json"), TEXT("bundle_validation_failed")));
	Results.Add(SyntheticNotReadyResult(TEXT("alpha_invalid_bundle.json"), TEXT("bundle_validation_failed")));

	const FWBCardDBImporterDiagnosticSummaryForTest Summary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Results);
	TestTrue(TEXT("Ready paths sorted"), IsSortedAscending(Summary.ReadyBundleSourcePaths));
	TestTrue(TEXT("Not-ready paths sorted"), IsSortedAscending(Summary.NotReadyBundleSourcePaths));
	TestFalse(TEXT("Not-ready paths omit SECRET"), Summary.NotReadyBundleSourcePaths.Contains(TEXT("zeta_SECRET_bundle.json")));
	TestTrue(TEXT("Hidden source path redacted"), Summary.NotReadyBundleSourcePaths.Contains(TEXT("redacted_source")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryEmptyInputTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.EmptyInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryEmptyInputTest::RunTest(const FString& Parameters)
{
	TArray<FWBCardDBImporterReadinessResultForTest> Results;
	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	ExpectSummaryExportMatches(
		*this,
		Results,
		TEXT("importer_diagnostic_summary_empty.export.json"),
		Summary);

	TestEqual(TEXT("Empty total"), Summary.TotalBundles, 0);
	TestEqual(TEXT("Empty ready"), Summary.ReadyBundles, 0);
	TestEqual(TEXT("Empty not ready"), Summary.NotReadyBundles, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryHelperNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.HelperNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryHelperNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBImporterDiagnosticSummaryForTests"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Summary helper not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryHelperNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.HelperNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryHelperNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
{
	TArray<FString> HelperFiles;
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterDiagnosticSummaryForTests.h")));
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterDiagnosticSummaryForTests.cpp")));

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
		TestFalse(*FString::Printf(TEXT("Summary helper omits %s"), *ForbiddenTerm), bFound);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryExistingReadinessStillPassesTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.ExistingReadinessStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryExistingReadinessStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterReadinessResultForTest Result =
		EvaluateReadiness(TEXT("valid_bundle_source_version_current.json"), SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1")));
	TestTrue(TEXT("Existing readiness still passes"), Result.bReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryExistingBundleValidationStillPassesTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.ExistingBundleValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryExistingBundleValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));
	TestTrue(TEXT("Existing bundle validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing bundle validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterDiagnosticSummaryExistingRootCardValidationStillPassesTest, "Wandbound.Core.CardDBImporterDiagnosticSummary.ExistingRootCardValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterDiagnosticSummaryExistingRootCardValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(TEXT("valid_damage_card.json")));
	TestTrue(TEXT("Existing root-card validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing root-card validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}
