#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

FWBCardDBSchemaValidationOptions StrictOptions()
{
	FWBCardDBSchemaValidationOptions Options;
	Options.bStrictUnknownFieldRejection = true;
	return Options;
}

FWBCardDBBundleSchemaValidationResult ValidateBundleFixture(const FString& FileName)
{
	return FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(CardDBBundleSchemaFixturePath(FileName));
}

FWBCardDBBundleSchemaValidationResult ValidateBundleFixtureStrict(const FString& FileName)
{
	return FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(CardDBBundleSchemaFixturePath(FileName), StrictOptions());
}

bool HasDiagnostic(
	const FWBCardDBBundleSchemaValidationResult& Result,
	const EWBCardDBSchemaDiagnostic ExpectedCode)
{
	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Code == ExpectedCode)
		{
			return true;
		}
	}

	return false;
}

bool ExpectBundleFailsWith(
	FAutomationTestBase& Test,
	const FString& FileName,
	const EWBCardDBSchemaDiagnostic ExpectedCode)
{
	const FWBCardDBBundleSchemaValidationResult Result = ValidateBundleFixture(FileName);
	Test.TestFalse(*FString::Printf(TEXT("%s fails validation"), *FileName), Result.bOk);
	Test.TestTrue(
		*FString::Printf(
			TEXT("%s has %s"),
			*FileName,
			*FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(ExpectedCode)),
		HasDiagnostic(Result, ExpectedCode));
	return !Result.bOk && HasDiagnostic(Result, ExpectedCode);
}

bool ExpectStrictBundleFailsWith(
	FAutomationTestBase& Test,
	const FString& FileName,
	const EWBCardDBSchemaDiagnostic ExpectedCode)
{
	const FWBCardDBBundleSchemaValidationResult Result = ValidateBundleFixtureStrict(FileName);
	Test.TestFalse(*FString::Printf(TEXT("%s fails strict validation"), *FileName), Result.bOk);
	Test.TestTrue(
		*FString::Printf(
			TEXT("%s has %s"),
			*FileName,
			*FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(ExpectedCode)),
		HasDiagnostic(Result, ExpectedCode));
	return !Result.bOk && HasDiagnostic(Result, ExpectedCode);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidSupportedCardsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidSupportedCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidSupportedCardsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result = ValidateBundleFixture(TEXT("valid_bundle_supported_cards.json"));
	TestTrue(TEXT("Supported-cards bundle validates"), Result.bOk);
	TestEqual(TEXT("Supported-cards bundle card definitions"), Result.CardDefinitions.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidCostUsageCardsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidCostUsageCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidCostUsageCardsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result = ValidateBundleFixture(TEXT("valid_bundle_cost_usage_cards.json"));
	TestTrue(TEXT("Cost/usage bundle validates"), Result.bOk);
	TestEqual(TEXT("Cost/usage bundle card definitions"), Result.CardDefinitions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictValidBundleMetadataTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictValidBundleMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictValidBundleMetadataTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result = ValidateBundleFixtureStrict(TEXT("strict_valid_bundle_metadata.json"));
	TestTrue(TEXT("Strict bundle metadata validates"), Result.bOk);
	TestEqual(TEXT("Strict bundle metadata diagnostics"), Result.Diagnostics.Num(), 0);
	TestEqual(TEXT("Strict bundle metadata card definitions"), Result.CardDefinitions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingSchemaVersionFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_missing_schema_version.json"),
		EWBCardDBSchemaDiagnostic::BundleSchemaVersionMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaUnsupportedSchemaVersionFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.UnsupportedSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaUnsupportedSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_unsupported_schema_version.json"),
		EWBCardDBSchemaDiagnostic::BundleSchemaVersionUnsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingCardDBVersionFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingCardDBVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingCardDBVersionFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_missing_carddb_version.json"),
		EWBCardDBSchemaDiagnostic::CardDBVersionMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingCardsFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingCardsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingCardsFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(*this, TEXT("invalid_bundle_missing_cards.json"), EWBCardDBSchemaDiagnostic::CardsMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaCardsNotArrayFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.CardsNotArrayFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaCardsNotArrayFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(*this, TEXT("invalid_bundle_cards_not_array.json"), EWBCardDBSchemaDiagnostic::CardsMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaCardsEmptyFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.CardsEmptyFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaCardsEmptyFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(*this, TEXT("invalid_bundle_cards_empty.json"), EWBCardDBSchemaDiagnostic::CardsEmpty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDuplicateCardIdFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DuplicateCardIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDuplicateCardIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(*this, TEXT("invalid_bundle_duplicate_card_id.json"), EWBCardDBSchemaDiagnostic::CardIdDuplicate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidCardDiagnosticBubblesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidCardDiagnosticBubbles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidCardDiagnosticBubblesTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_contains_invalid_card.json"),
		EWBCardDBSchemaDiagnostic::UnsupportedPayloadType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictUnknownTopLevelFieldFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictUnknownTopLevelFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictUnknownTopLevelFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictBundleFailsWith(
		*this,
		TEXT("strict_invalid_bundle_unknown_top_level_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictUnknownMetadataFieldFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictUnknownMetadataFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictUnknownMetadataFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictBundleFailsWith(
		*this,
		TEXT("strict_invalid_bundle_unknown_metadata_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownMetadataField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaNonStrictUnknownFieldAllowedTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.NonStrictUnknownFieldAllowed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaNonStrictUnknownFieldAllowedTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("non_strict_bundle_unknown_field_allowed.json"));
	TestTrue(TEXT("Non-strict bundle unknown field validates"), Result.bOk);
	TestEqual(TEXT("Non-strict bundle unknown diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaNonStrictUnknownFieldFailsWhenStrictTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.NonStrictUnknownFieldFailsWhenStrict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaNonStrictUnknownFieldFailsWhenStrictTest::RunTest(const FString& Parameters)
{
	ExpectStrictBundleFailsWith(
		*this,
		TEXT("non_strict_bundle_unknown_field_allowed.json"),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictModePropagatesToCardsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictModePropagatesToCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictModePropagatesToCardsTest::RunTest(const FString& Parameters)
{
	ExpectStrictBundleFailsWith(
		*this,
		TEXT("strict_invalid_bundle_card_unknown_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaRootCardFixtureStillPassesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.RootCardFixtureStillPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaRootCardFixtureStillPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(TEXT("valid_damage_card.json")));
	TestTrue(TEXT("Existing root-card fixture still validates"), Result.bOk);
	TestEqual(TEXT("Existing root-card fixture diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDiagnosticCodeStringsStableTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DiagnosticCodeStringsStable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDiagnosticCodeStringsStableTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("CardDBVersionMissing string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::CardDBVersionMissing),
		FString(TEXT("carddb_version_missing")));
	TestEqual(
		TEXT("CardsMissing string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::CardsMissing),
		FString(TEXT("cards_missing")));
	TestEqual(
		TEXT("CardsMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::CardsMalformed),
		FString(TEXT("cards_malformed")));
	TestEqual(
		TEXT("CardsEmpty string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::CardsEmpty),
		FString(TEXT("cards_empty")));
	TestEqual(
		TEXT("CardIdDuplicate string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::CardIdDuplicate),
		FString(TEXT("card_id_duplicate")));
	TestEqual(
		TEXT("BundleSchemaVersionMissing string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::BundleSchemaVersionMissing),
		FString(TEXT("bundle_schema_version_missing")));
	TestEqual(
		TEXT("BundleSchemaVersionUnsupported string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::BundleSchemaVersionUnsupported),
		FString(TEXT("bundle_schema_version_unsupported")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidatorNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidatorNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidatorNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBSchemaFixtureValidator"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Validator not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidatorNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidatorNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidatorNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
{
	TArray<FString> ValidatorFiles;
	ValidatorFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBSchemaFixtureValidator.h")));
	ValidatorFiles.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBCardDBSchemaFixtureValidator.cpp")));

	const FString ForbiddenTerms[] = {
		TEXT("WBEffectRunner"),
		TEXT("GenerateLegalActions"),
		TEXT("WBCardActivationCandidateGenerator"),
		TEXT("WBCardActivationLegalActionGenerator")
	};

	for (const FString& ForbiddenTerm : ForbiddenTerms)
	{
		FString FoundFile;
		const bool bFound = AnyFileContains(ValidatorFiles, ForbiddenTerm, FoundFile);
		TestFalse(*FString::Printf(TEXT("Validator omits %s"), *ForbiddenTerm), bFound);
	}

	return true;
}
