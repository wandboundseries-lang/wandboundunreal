#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	const FString& TargetSourceVersion,
	const bool bRequireSourceVersion = false)
{
	FWBCardDBSchemaValidationOptions Options;
	Options.SourceVersionCompatibility.bEnableSourceVersionCompatibilityValidation = true;
	Options.SourceVersionCompatibility.bRequireSourceVersion = bRequireSourceVersion;
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

bool EvaluateReadinessAndExport(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	const FWBCardDBSchemaValidationOptions& Options,
	FWBCardDBImporterReadinessResultForTest& OutResult,
	FString& OutJson)
{
	OutResult = FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
		CardDBBundleSchemaFixturePath(BundleFileName),
		Options);
	const bool bExported =
		FWBCardDBImporterReadinessForTests::ImporterReadinessResultToJsonStringForTest(OutResult, OutJson);
	Test.TestTrue(*FString::Printf(TEXT("%s readiness export succeeds"), *BundleFileName), bExported);
	return bExported;
}

bool ExpectReadinessExportMatches(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	const FString& ExpectedExportFileName,
	const FWBCardDBSchemaValidationOptions& Options,
	FWBCardDBImporterReadinessResultForTest& OutResult)
{
	FString ActualJson;
	FString ExpectedJson;
	if (!EvaluateReadinessAndExport(Test, BundleFileName, Options, OutResult, ActualJson)
		|| !LoadExpectedExportFixture(Test, ExpectedExportFileName, ExpectedJson))
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s readiness export matches %s"), *BundleFileName, *ExpectedExportFileName),
		ActualJson,
		ExpectedJson);
	return ActualJson == ExpectedJson;
}

bool ExpectStringArrayEquals(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FString>& Actual,
	const TArray<FString>& Expected)
{
	bool bMatches = Test.TestEqual(*FString::Printf(TEXT("%s count"), *Label), Actual.Num(), Expected.Num());
	const int32 CompareCount = FMath::Min(Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < CompareCount; ++Index)
	{
		bMatches &= Test.TestEqual(*FString::Printf(TEXT("%s[%d]"), *Label, Index), Actual[Index], Expected[Index]);
	}

	return bMatches;
}

TArray<FString> OrderedCardIdsFromDefinitions(const TArray<FWBCardDefinition>& CardDefinitions)
{
	TArray<FString> CardIds;
	for (const FWBCardDefinition& CardDefinition : CardDefinitions)
	{
		CardIds.Add(CardDefinition.CardId);
	}

	return CardIds;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessCurrentSourceReadyTest, "Wandbound.Core.CardDBImporterReadiness.CurrentSourceReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessCurrentSourceReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("valid_bundle_source_version_current.json"),
		TEXT("importer_ready_valid_current_source.export.json"),
		SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1")),
		Result);

	TestTrue(TEXT("Current source bundle is ready"), Result.bReady);
	TestEqual(TEXT("Current source ready reason"), Result.Reason, FString());
	TestEqual(TEXT("Current source ordered definitions"), Result.OrderedCardDefinitions.Num(), Result.DependencyOrderCardIds.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessTransitionReadyTest, "Wandbound.Core.CardDBImporterReadiness.TransitionReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessTransitionReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("valid_bundle_source_version_supported_transition.json"),
		TEXT("importer_ready_valid_transition.export.json"),
		SourceVersionTransitionOptions(TEXT("wandbound_schema.2"), TEXT("legacy_schema.1"), TEXT("wandbound_schema.2")),
		Result);

	TestTrue(TEXT("Transition bundle is ready"), Result.bReady);
	TestEqual(TEXT("Transition ordered definitions"), Result.OrderedCardDefinitions.Num(), Result.DependencyOrderCardIds.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessDependencyOrderReadyTest, "Wandbound.Core.CardDBImporterReadiness.DependencyOrderReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessDependencyOrderReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("valid_bundle_dependency_order_simple_chain.json"),
		TEXT("importer_ready_valid_dependency_order.export.json"),
		FWBCardDBSchemaValidationOptions(),
		Result);

	const TArray<FString> ExpectedOrder = {
		TEXT("base_card"),
		TEXT("middle_card"),
		TEXT("top_card")
	};

	TestTrue(TEXT("Dependency-order bundle is ready"), Result.bReady);
	ExpectStringArrayEquals(*this, TEXT("Dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectStringArrayEquals(
		*this,
		TEXT("Ordered card definitions"),
		OrderedCardIdsFromDefinitions(Result.OrderedCardDefinitions),
		ExpectedOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessUnsupportedSourceNotReadyTest, "Wandbound.Core.CardDBImporterReadiness.UnsupportedSourceNotReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessUnsupportedSourceNotReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("invalid_bundle_source_version_unsupported.json"),
		TEXT("importer_not_ready_unsupported_source.export.json"),
		SourceVersionCompatibilityOptions(TEXT("wandbound_schema.2")),
		Result);

	TestFalse(TEXT("Unsupported source bundle is not ready"), Result.bReady);
	TestEqual(TEXT("Unsupported source reason"), Result.Reason, FString(TEXT("bundle_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessUnsupportedTransitionNotReadyTest, "Wandbound.Core.CardDBImporterReadiness.UnsupportedTransitionNotReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessUnsupportedTransitionNotReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("invalid_bundle_source_version_transition_unsupported.json"),
		TEXT("importer_not_ready_unsupported_transition.export.json"),
		SourceVersionTransitionOptions(TEXT("wandbound_schema.3"), TEXT("legacy_schema.1"), TEXT("wandbound_schema.2")),
		Result);

	TestFalse(TEXT("Unsupported transition bundle is not ready"), Result.bReady);
	TestEqual(TEXT("Unsupported transition reason"), Result.Reason, FString(TEXT("bundle_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessMissingReferenceNotReadyTest, "Wandbound.Core.CardDBImporterReadiness.MissingReferenceNotReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessMissingReferenceNotReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_missing_reference_skips_order.json"),
		TEXT("importer_not_ready_missing_reference.export.json"),
		FWBCardDBSchemaValidationOptions(),
		Result);

	TestFalse(TEXT("Missing reference bundle is not ready"), Result.bReady);
	TestEqual(TEXT("Missing reference reason"), Result.Reason, FString(TEXT("bundle_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessDependencyCycleNotReadyTest, "Wandbound.Core.CardDBImporterReadiness.DependencyCycleNotReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessDependencyCycleNotReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_cycle_two_cards.json"),
		TEXT("importer_not_ready_dependency_cycle.export.json"),
		FWBCardDBSchemaValidationOptions(),
		Result);

	TestFalse(TEXT("Dependency cycle bundle is not ready"), Result.bReady);
	TestEqual(TEXT("Dependency cycle reason"), Result.Reason, FString(TEXT("bundle_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessDuplicateCardIdNotReadyTest, "Wandbound.Core.CardDBImporterReadiness.DuplicateCardIdNotReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessDuplicateCardIdNotReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("invalid_bundle_duplicate_card_id.json"),
		TEXT("importer_not_ready_duplicate_card_id.export.json"),
		FWBCardDBSchemaValidationOptions(),
		Result);

	TestFalse(TEXT("Duplicate card id bundle is not ready"), Result.bReady);
	TestEqual(TEXT("Duplicate card id reason"), Result.Reason, FString(TEXT("bundle_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessInvalidCardPayloadNotReadyTest, "Wandbound.Core.CardDBImporterReadiness.InvalidCardPayloadNotReady", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessInvalidCardPayloadNotReadyTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	ExpectReadinessExportMatches(
		*this,
		TEXT("invalid_bundle_contains_invalid_card.json"),
		TEXT("importer_not_ready_invalid_card_payload.export.json"),
		FWBCardDBSchemaValidationOptions(),
		Result);

	TestFalse(TEXT("Invalid payload bundle is not ready"), Result.bReady);
	TestEqual(TEXT("Invalid payload reason"), Result.Reason, FString(TEXT("bundle_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessHiddenTokenExportSafeTest, "Wandbound.Core.CardDBImporterReadiness.HiddenTokenExportSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessHiddenTokenExportSafeTest::RunTest(const FString& Parameters)
{
	FWBCardDBImporterReadinessResultForTest Result;
	FString ActualJson;
	FString ExpectedJson;
	EvaluateReadinessAndExport(
		*this,
		TEXT("invalid_bundle_source_version_hidden_token_safe.json"),
		SourceVersionCompatibilityOptions(TEXT("wandbound_schema.1")),
		Result,
		ActualJson);
	LoadExpectedExportFixture(*this, TEXT("importer_not_ready_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden-token bundle is not ready"), Result.bReady);
	TestFalse(TEXT("Hidden-token readiness export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden-token readiness export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessPublicApiAlwaysProducesOrderForValidBundlesTest, "Wandbound.Core.CardDBImporterReadiness.PublicApiAlwaysProducesOrderForValidBundles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessPublicApiAlwaysProducesOrderForValidBundlesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterReadinessResultForTest Result =
		FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_dependency_order_simple_chain.json")));

	TestTrue(TEXT("Valid bundle is ready through public API"), Result.bReady);
	TestTrue(TEXT("Valid non-empty bundle has dependency order through public API"), Result.DependencyOrderCardIds.Num() > 0);
	TestFalse(
		TEXT("Dependency-order-missing is not reachable for current valid fixtures"),
		Result.Reason == TEXT("dependency_order_missing"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessExportDeterministicTest, "Wandbound.Core.CardDBImporterReadiness.ExportDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessExportDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBImporterReadinessResultForTest Result =
		FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_dependency_order_simple_chain.json")));

	FString FirstJson;
	FString SecondJson;
	TestTrue(
		TEXT("First readiness export succeeds"),
		FWBCardDBImporterReadinessForTests::ImporterReadinessResultToJsonStringForTest(Result, FirstJson));
	TestTrue(
		TEXT("Second readiness export succeeds"),
		FWBCardDBImporterReadinessForTests::ImporterReadinessResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Readiness export helper deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessHelperNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBImporterReadiness.HelperNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessHelperNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBImporterReadinessForTests"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Readiness helper not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessHelperNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBImporterReadiness.HelperNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessHelperNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
{
	TArray<FString> HelperFiles;
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterReadinessForTests.h")));
	HelperFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBImporterReadinessForTests.cpp")));

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
		TestFalse(*FString::Printf(TEXT("Readiness helper omits %s"), *ForbiddenTerm), bFound);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessExistingBundleValidationStillPassesTest, "Wandbound.Core.CardDBImporterReadiness.ExistingBundleValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessExistingBundleValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(
			CardDBBundleSchemaFixturePath(TEXT("valid_bundle_source_version_current.json")));
	TestTrue(TEXT("Existing bundle validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing bundle validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBImporterReadinessExistingRootCardValidationStillPassesTest, "Wandbound.Core.CardDBImporterReadiness.ExistingRootCardValidationStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBImporterReadinessExistingRootCardValidationStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(TEXT("valid_damage_card.json")));
	TestTrue(TEXT("Existing root-card validation still passes"), Result.bOk);
	TestEqual(TEXT("Existing root-card validation diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}
