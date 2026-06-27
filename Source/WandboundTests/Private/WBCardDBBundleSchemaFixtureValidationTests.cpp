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

FString CardDBExpectedExportFixturePath(const FString& FileName)
{
	return FPaths::Combine(CardDBSchemaFixturePath(TEXT("ExpectedExports")), FileName);
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

const FWBCardDBSchemaValidationDiagnostic* FindDiagnostic(
	const FWBCardDBBundleSchemaValidationResult& Result,
	const EWBCardDBSchemaDiagnostic ExpectedCode)
{
	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Code == ExpectedCode)
		{
			return &Diagnostic;
		}
	}

	return nullptr;
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

bool ExportBundleFixtureJson(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	FWBCardDBBundleSchemaValidationResult& OutResult,
	FString& OutJson)
{
	OutResult = FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(CardDBBundleSchemaFixturePath(BundleFileName));
	const bool bExported = FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(OutResult, OutJson);
	Test.TestTrue(*FString::Printf(TEXT("%s export succeeds"), *BundleFileName), bExported);
	return bExported;
}

bool ExportBundleFixtureJsonWithOptions(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	const FWBCardDBSchemaValidationOptions& Options,
	FWBCardDBBundleSchemaValidationResult& OutResult,
	FString& OutJson)
{
	OutResult = FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(CardDBBundleSchemaFixturePath(BundleFileName), Options);
	const bool bExported = FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(OutResult, OutJson);
	Test.TestTrue(*FString::Printf(TEXT("%s export succeeds"), *BundleFileName), bExported);
	return bExported;
}

bool ExpectExportMatchesWithOptions(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	const FString& ExpectedExportFileName,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	FString ExpectedJson;
	if (!ExportBundleFixtureJsonWithOptions(Test, BundleFileName, Options, Result, ActualJson)
		|| !LoadExpectedExportFixture(Test, ExpectedExportFileName, ExpectedJson))
	{
		return false;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s export matches %s"), *BundleFileName, *ExpectedExportFileName),
		ActualJson,
		ExpectedJson);
	return ActualJson == ExpectedJson;
}

bool ExpectExportMatches(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	const FString& ExpectedExportFileName)
{
	return ExpectExportMatchesWithOptions(
		Test,
		BundleFileName,
		ExpectedExportFileName,
		FWBCardDBSchemaValidationOptions());
}

bool ExpectStrictExportMatches(
	FAutomationTestBase& Test,
	const FString& BundleFileName,
	const FString& ExpectedExportFileName)
{
	return ExpectExportMatchesWithOptions(Test, BundleFileName, ExpectedExportFileName, StrictOptions());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidVersionMetadataFullTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidVersionMetadataFull", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidVersionMetadataFullTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_version_metadata_full.json"));
	TestTrue(TEXT("Full version metadata bundle validates"), Result.bOk);
	TestEqual(TEXT("Full version metadata diagnostics"), Result.Diagnostics.Num(), 0);
	TestEqual(TEXT("Full version metadata card definitions"), Result.CardDefinitions.Num(), 1);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_version_metadata_full.json"),
		TEXT("valid_bundle_version_metadata_full.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidVersionMetadataMinimalTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidVersionMetadataMinimal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidVersionMetadataMinimalTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_version_metadata_minimal.json"));
	TestTrue(TEXT("Minimal version metadata bundle validates"), Result.bOk);
	TestEqual(TEXT("Minimal version metadata diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictValidVersionMetadataTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictValidVersionMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictValidVersionMetadataTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixtureStrict(TEXT("strict_valid_bundle_version_metadata.json"));
	TestTrue(TEXT("Strict version metadata bundle validates"), Result.bOk);
	TestEqual(TEXT("Strict version metadata diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidCardDBVersionEmptyTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidCardDBVersionEmpty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidCardDBVersionEmptyTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_carddb_version_empty.json"),
		EWBCardDBSchemaDiagnostic::InvalidCardDBVersion);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_carddb_version_empty.json"),
		TEXT("invalid_bundle_carddb_version_empty.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidCardDBVersionBadCharsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidCardDBVersionBadChars", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidCardDBVersionBadCharsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_carddb_version_bad_chars.json"),
		EWBCardDBSchemaDiagnostic::InvalidCardDBVersion);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_carddb_version_bad_chars.json"),
		TEXT("invalid_bundle_carddb_version_bad_chars.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidCardDBVersionTooLongTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidCardDBVersionTooLong", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidCardDBVersionTooLongTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_carddb_version_too_long.json"),
		EWBCardDBSchemaDiagnostic::InvalidCardDBVersion);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_carddb_version_too_long.json"),
		TEXT("invalid_bundle_carddb_version_too_long.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidSourceVersionNotStringTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidSourceVersionNotString", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidSourceVersionNotStringTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_source_version_not_string.json"),
		EWBCardDBSchemaDiagnostic::InvalidSourceVersion);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_source_version_not_string.json"),
		TEXT("invalid_bundle_source_version_not_string.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidSourceVersionBadCharsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidSourceVersionBadChars", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidSourceVersionBadCharsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_source_version_bad_chars.json"),
		EWBCardDBSchemaDiagnostic::InvalidSourceVersion);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_source_version_bad_chars.json"),
		TEXT("invalid_bundle_source_version_bad_chars.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaInvalidSourceVersionTooLongTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.InvalidSourceVersionTooLong", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaInvalidSourceVersionTooLongTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_source_version_too_long.json"),
		EWBCardDBSchemaDiagnostic::InvalidSourceVersion);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_source_version_too_long.json"),
		TEXT("invalid_bundle_source_version_too_long.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMigrationNotesNotStringTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MigrationNotesNotString", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMigrationNotesNotStringTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_migration_notes_not_string.json"),
		EWBCardDBSchemaDiagnostic::MigrationNotesMalformed);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_migration_notes_not_string.json"),
		TEXT("invalid_bundle_migration_notes_not_string.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMigrationNotesTooLongTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MigrationNotesTooLong", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMigrationNotesTooLongTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_migration_notes_too_long.json"),
		EWBCardDBSchemaDiagnostic::MigrationNotesMalformed);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_migration_notes_too_long.json"),
		TEXT("invalid_bundle_migration_notes_too_long.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMigrationNotesHiddenTokenTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MigrationNotesHiddenToken", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMigrationNotesHiddenTokenTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_migration_notes_hidden_token.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("invalid_bundle_migration_notes_hidden_token.export.json"), ExpectedJson);

	TestFalse(TEXT("Migration notes hidden-token bundle fails"), Result.bOk);
	TestTrue(TEXT("Migration notes hidden-token diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation));
	TestFalse(TEXT("Migration notes hidden-token export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Migration notes hidden-token export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMetadataNotObjectTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MetadataNotObject", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMetadataNotObjectTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_metadata_not_object.json"),
		EWBCardDBSchemaDiagnostic::MetadataMalformed);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_metadata_not_object.json"),
		TEXT("invalid_bundle_metadata_not_object.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMetadataAuthorNotStringTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MetadataAuthorNotString", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMetadataAuthorNotStringTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_metadata_author_not_string.json"),
		EWBCardDBSchemaDiagnostic::MetadataMalformed);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_metadata_author_not_string.json"),
		TEXT("invalid_bundle_metadata_author_not_string.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMetadataNotesTooLongTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MetadataNotesTooLong", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMetadataNotesTooLongTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_metadata_notes_too_long.json"),
		EWBCardDBSchemaDiagnostic::MetadataMalformed);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_metadata_notes_too_long.json"),
		TEXT("invalid_bundle_metadata_notes_too_long.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMetadataTestOnlyNotBoolTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MetadataTestOnlyNotBool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMetadataTestOnlyNotBoolTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_metadata_test_only_not_bool.json"),
		EWBCardDBSchemaDiagnostic::MetadataMalformed);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_metadata_test_only_not_bool.json"),
		TEXT("invalid_bundle_metadata_test_only_not_bool.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMetadataHiddenTokenTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MetadataHiddenToken", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMetadataHiddenTokenTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_metadata_hidden_token.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("invalid_bundle_metadata_hidden_token.export.json"), ExpectedJson);

	TestFalse(TEXT("Metadata hidden-token bundle fails"), Result.bOk);
	TestTrue(TEXT("Metadata hidden-token diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation));
	TestFalse(TEXT("Metadata hidden-token export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Metadata hidden-token export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictMetadataUnknownFieldTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictMetadataUnknownField", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictMetadataUnknownFieldTest::RunTest(const FString& Parameters)
{
	ExpectStrictBundleFailsWith(
		*this,
		TEXT("strict_invalid_bundle_metadata_unknown_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownMetadataField);
	ExpectStrictExportMatches(
		*this,
		TEXT("strict_invalid_bundle_metadata_unknown_field.json"),
		TEXT("strict_invalid_bundle_metadata_unknown_field.export.json"));
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
	TestEqual(
		TEXT("MissingCardReference string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::MissingCardReference),
		FString(TEXT("missing_card_reference")));
	TestEqual(
		TEXT("MissingEffectReference string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::MissingEffectReference),
		FString(TEXT("missing_effect_reference")));
	TestEqual(
		TEXT("ReferenceMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::ReferenceMalformed),
		FString(TEXT("reference_malformed")));
	TestEqual(
		TEXT("UnknownReferenceField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownReferenceField),
		FString(TEXT("unknown_reference_field")));
	TestEqual(
		TEXT("DependencyCycleDetected string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::DependencyCycleDetected),
		FString(TEXT("dependency_cycle_detected")));
	TestEqual(
		TEXT("DependencySelfReference string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::DependencySelfReference),
		FString(TEXT("dependency_self_reference")));
	TestEqual(
		TEXT("InvalidCardDBVersion string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::InvalidCardDBVersion),
		FString(TEXT("invalid_carddb_version")));
	TestEqual(
		TEXT("InvalidSourceVersion string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::InvalidSourceVersion),
		FString(TEXT("invalid_source_version")));
	TestEqual(
		TEXT("MigrationNotesMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::MigrationNotesMalformed),
		FString(TEXT("migration_notes_malformed")));
	TestEqual(
		TEXT("MetadataMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::MetadataMalformed),
		FString(TEXT("metadata_malformed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMultipleInvalidCardsReportContextTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MultipleInvalidCardsReportContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMultipleInvalidCardsReportContextTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_multiple_invalid_cards.json"));
	TestFalse(TEXT("Multiple invalid cards bundle fails"), Result.bOk);
	TestTrue(
		TEXT("Missing public name has card context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::PublicNameMissing,
			0,
			TEXT("bundle_missing_public_name"),
			FString(),
			TEXT("$.cards[0].public_name")));
	TestTrue(
		TEXT("Unsupported payload has card/effect/payload context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::UnsupportedPayloadType,
			1,
			TEXT("bundle_unsupported_payload"),
			TEXT("draw_2"),
			TEXT("$.cards[1].activated_effects[0].payloads[0]")));
	TestTrue(
		TEXT("Invalid status has card/effect/payload context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::InvalidStatusId,
			2,
			TEXT("bundle_invalid_status"),
			TEXT("bad_status"),
			TEXT("$.cards[2].activated_effects[0].payloads[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaEffectContextPreservedTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.EffectContextPreserved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaEffectContextPreservedTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_card_effect_context.json"));
	TestFalse(TEXT("Effect context bundle fails"), Result.bOk);
	TestTrue(
		TEXT("Effect context includes card index, card id, effect id, and payload path"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::UnsupportedPayloadType,
			0,
			TEXT("bundle_effect_context_card"),
			TEXT("bad_effect"),
			TEXT("$.cards[0].activated_effects[0].payloads[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDuplicateCardIdContextTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DuplicateCardIdContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDuplicateCardIdContextTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_duplicate_card_context.json"));
	TestFalse(TEXT("Duplicate card context bundle fails"), Result.bOk);
	TestTrue(
		TEXT("Duplicate card id reports the second duplicate index"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::CardIdDuplicate,
			2,
			TEXT("duplicate_context_card"),
			FString(),
			TEXT("$.cards")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaBundleDiagnosticsPrecedeCardDiagnosticsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.BundleDiagnosticsPrecedeCardDiagnostics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaBundleDiagnosticsPrecedeCardDiagnosticsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixtureStrict(TEXT("invalid_bundle_mixed_bundle_and_card_errors.json"));
	TestFalse(TEXT("Mixed bundle/card errors fail"), Result.bOk);
	TestTrue(TEXT("Mixed diagnostics exist"), Result.Diagnostics.Num() >= 2);
	if (Result.Diagnostics.Num() >= 2)
	{
		TestEqual(TEXT("Bundle-level diagnostic appears first"), Result.Diagnostics[0].Code, EWBCardDBSchemaDiagnostic::UnknownTopLevelField);
		TestEqual(TEXT("Bundle-level diagnostic path"), Result.Diagnostics[0].JsonPath, FString(TEXT("$")));
	}

	TestTrue(
		TEXT("Card-level missing effect id keeps context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::EffectIdMissing,
			0,
			TEXT("bundle_missing_effect_id"),
			FString(),
			TEXT("$.cards[0].activated_effects[0].effect_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaHiddenTokenNotLeakedTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.HiddenTokenNotLeaked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaHiddenTokenNotLeakedTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_hidden_token_not_in_diagnostics.json"));
	TestFalse(TEXT("Hidden-token bundle fails"), Result.bOk);
	TestTrue(TEXT("Hidden-token diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation));

	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		TestFalse(TEXT("Message omits SECRET"), Diagnostic.Message.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("JsonPath omits SECRET"), Diagnostic.JsonPath.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("BundleCardId omits SECRET"), Diagnostic.BundleCardId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("CardId omits SECRET"), Diagnostic.CardId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("EffectId omits SECRET"), Diagnostic.EffectId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidBundleHasNoDiagnosticsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidBundleHasNoDiagnostics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidBundleHasNoDiagnosticsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_context_fields_absent.json"));
	TestTrue(TEXT("Valid context bundle validates"), Result.bOk);
	TestEqual(TEXT("Valid context bundle diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaRootCardDiagnosticContextAbsentTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.RootCardDiagnosticContextAbsent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaRootCardDiagnosticContextAbsentTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(TEXT("invalid_missing_public_name.json")));
	TestFalse(TEXT("Root invalid card fails"), Result.bOk);
	TestTrue(TEXT("Root invalid card has diagnostics"), Result.Diagnostics.Num() > 0);
	if (Result.Diagnostics.Num() > 0)
	{
		TestEqual(TEXT("Root diagnostic card index stays absent"), Result.Diagnostics[0].CardIndex, -1);
		TestEqual(TEXT("Root diagnostic bundle card id stays absent"), Result.Diagnostics[0].BundleCardId, FString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDiagnosticContextStringStableTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DiagnosticContextStringStable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDiagnosticContextStringStableTest::RunTest(const FString& Parameters)
{
	FWBCardDBSchemaValidationDiagnostic Diagnostic;
	Diagnostic.Code = EWBCardDBSchemaDiagnostic::UnsupportedPayloadType;
	Diagnostic.CardIndex = 3;
	Diagnostic.BundleCardId = TEXT("safe_card");
	Diagnostic.CardId = TEXT("safe_card");
	Diagnostic.EffectId = TEXT("safe_effect");
	Diagnostic.JsonPath = TEXT("$.cards[3].activated_effects[0].payloads[0]");

	TestEqual(
		TEXT("Diagnostic context string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticContextToStringForTest(Diagnostic),
		FString(TEXT("code=unsupported_payload_type;card_index=3;bundle_card_id=safe_card;card_id=safe_card;effect_id=safe_effect;json_path=$.cards[3].activated_effects[0].payloads[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaContainsDiagnosticWithContextWorksTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ContainsDiagnosticWithContextWorks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaContainsDiagnosticWithContextWorksTest::RunTest(const FString& Parameters)
{
	FWBCardDBSchemaValidationDiagnostic Diagnostic;
	Diagnostic.Code = EWBCardDBSchemaDiagnostic::InvalidStatusId;
	Diagnostic.CardIndex = 2;
	Diagnostic.BundleCardId = TEXT("status_card");
	Diagnostic.EffectId = TEXT("bad_status");
	Diagnostic.JsonPath = TEXT("$.cards[2].activated_effects[0].payloads[0]");

	TArray<FWBCardDBSchemaValidationDiagnostic> Diagnostics;
	Diagnostics.Add(Diagnostic);
	TestTrue(
		TEXT("Helper finds exact context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Diagnostics,
			EWBCardDBSchemaDiagnostic::InvalidStatusId,
			2,
			TEXT("status_card"),
			TEXT("bad_status"),
			TEXT("$.cards[2].activated_effects[0].payloads[0]")));
	TestFalse(
		TEXT("Helper rejects wrong index"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Diagnostics,
			EWBCardDBSchemaDiagnostic::InvalidStatusId,
			1,
			TEXT("status_card"),
			TEXT("bad_status"),
			TEXT("$.cards[2].activated_effects[0].payloads[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidCardLevelReferencePassesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidCardLevelReferencePasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidCardLevelReferencePassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_card_level_reference.json"));
	TestTrue(TEXT("Valid card-level reference bundle validates"), Result.bOk);
	TestEqual(TEXT("Valid card-level reference diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidEffectReferencePassesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidEffectReferencePasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidEffectReferencePassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_effect_reference.json"));
	TestTrue(TEXT("Valid effect reference bundle validates"), Result.bOk);
	TestEqual(TEXT("Valid effect reference diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaValidPayloadReferencePassesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ValidPayloadReferencePasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaValidPayloadReferencePassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_payload_reference.json"));
	TestTrue(TEXT("Valid payload reference bundle validates"), Result.bOk);
	TestEqual(TEXT("Valid payload reference diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictValidReferencesMetadataPassesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictValidReferencesMetadataPasses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictValidReferencesMetadataPassesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixtureStrict(TEXT("strict_valid_bundle_references_metadata.json"));
	TestTrue(TEXT("Strict valid references metadata validates"), Result.bOk);
	TestEqual(TEXT("Strict valid references metadata diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingCardReferenceCardLevelTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingCardReferenceCardLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingCardReferenceCardLevelTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_missing_card_reference_card_level.json"));
	TestFalse(TEXT("Missing card reference at card level fails"), Result.bOk);
	TestTrue(
		TEXT("Missing card reference has card-level context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::MissingCardReference,
			0,
			TEXT("missing_card_reference_source"),
			FString(),
			TEXT("$.cards[0].references.card_ids[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingCardReferenceEffectLevelTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingCardReferenceEffectLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingCardReferenceEffectLevelTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_missing_card_reference_effect_level.json"));
	TestFalse(TEXT("Missing card reference at effect level fails"), Result.bOk);
	TestTrue(
		TEXT("Missing card reference has effect-level context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::MissingCardReference,
			0,
			TEXT("effect_missing_card_source"),
			TEXT("effect_referrer"),
			TEXT("$.cards[0].activated_effects[0].references.card_ids[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingEffectReferenceTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingEffectReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingEffectReferenceTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_missing_effect_reference.json"));
	TestFalse(TEXT("Missing effect reference fails"), Result.bOk);
	TestTrue(
		TEXT("Missing effect reference has effect-ref context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::MissingEffectReference,
			0,
			TEXT("missing_effect_reference_source"),
			TEXT("effect_referrer"),
			TEXT("$.cards[0].activated_effects[0].references.effect_refs[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingCardInEffectReferenceTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingCardInEffectReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingCardInEffectReferenceTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_missing_card_in_effect_reference.json"));
	TestFalse(TEXT("Missing card in effect reference fails"), Result.bOk);
	TestTrue(
		TEXT("Missing card in effect reference has context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::MissingCardReference,
			0,
			TEXT("missing_card_effect_reference_source"),
			TEXT("effect_referrer"),
			TEXT("$.cards[0].activated_effects[0].references.effect_refs[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingCardReferencePayloadLevelTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingCardReferencePayloadLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingCardReferencePayloadLevelTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_missing_card_reference_payload_level.json"));
	TestFalse(TEXT("Missing card reference at payload level fails"), Result.bOk);
	TestTrue(
		TEXT("Missing card reference has payload-level context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::MissingCardReference,
			0,
			TEXT("payload_missing_card_source"),
			TEXT("payload_referrer"),
			TEXT("$.cards[0].activated_effects[0].payloads[0].references.card_ids[0]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaReferencesNotObjectFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ReferencesNotObjectFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaReferencesNotObjectFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(*this, TEXT("invalid_bundle_references_not_object.json"), EWBCardDBSchemaDiagnostic::ReferenceMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaReferenceCardIdsNotArrayFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ReferenceCardIdsNotArrayFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaReferenceCardIdsNotArrayFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_reference_card_ids_not_array.json"),
		EWBCardDBSchemaDiagnostic::ReferenceMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaReferenceCardIdNotStringFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ReferenceCardIdNotStringFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaReferenceCardIdNotStringFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_reference_card_id_not_string.json"),
		EWBCardDBSchemaDiagnostic::ReferenceMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaEffectRefsNotArrayFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.EffectRefsNotArrayFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaEffectRefsNotArrayFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_effect_refs_not_array.json"),
		EWBCardDBSchemaDiagnostic::ReferenceMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaEffectRefNotObjectFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.EffectRefNotObjectFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaEffectRefNotObjectFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_effect_ref_not_object.json"),
		EWBCardDBSchemaDiagnostic::ReferenceMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaEffectRefMissingFieldsFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.EffectRefMissingFieldsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaEffectRefMissingFieldsFailsTest::RunTest(const FString& Parameters)
{
	ExpectBundleFailsWith(
		*this,
		TEXT("invalid_bundle_effect_ref_missing_fields.json"),
		EWBCardDBSchemaDiagnostic::ReferenceMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaStrictUnknownReferenceFieldFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.StrictUnknownReferenceFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaStrictUnknownReferenceFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictBundleFailsWith(
		*this,
		TEXT("strict_invalid_bundle_unknown_reference_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownReferenceField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaReferenceHiddenTokenSafeTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ReferenceHiddenTokenSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaReferenceHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_reference_hidden_token_safe.json"));
	TestFalse(TEXT("Hidden reference token bundle fails"), Result.bOk);
	TestTrue(TEXT("Missing card reference diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::MissingCardReference));

	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		TestFalse(TEXT("Message omits SECRET"), Diagnostic.Message.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("JsonPath omits SECRET"), Diagnostic.JsonPath.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("BundleCardId omits SECRET"), Diagnostic.BundleCardId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("CardId omits SECRET"), Diagnostic.CardId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("EffectId omits SECRET"), Diagnostic.EffectId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyOrderSimpleChainTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyOrderSimpleChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyOrderSimpleChainTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_order_simple_chain.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("base_card"),
		TEXT("middle_card"),
		TEXT("top_card")
	};

	TestTrue(TEXT("Simple dependency chain validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Simple dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyOrderReverseInputTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyOrderReverseInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyOrderReverseInputTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_order_reverse_input.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("base_card"),
		TEXT("middle_card"),
		TEXT("top_card")
	};

	TestTrue(TEXT("Reverse-input dependency chain validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Reverse-input dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyOrderDiamondTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyOrderDiamond", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyOrderDiamondTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_order_diamond.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("base_card"),
		TEXT("left_card"),
		TEXT("right_card"),
		TEXT("top_card")
	};

	TestTrue(TEXT("Diamond dependency bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Diamond dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyOrderIndependentTiebreakTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyOrderIndependentTiebreak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyOrderIndependentTiebreakTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_order_independent_tiebreak.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("independent_a"),
		TEXT("independent_b"),
		TEXT("independent_c")
	};

	TestTrue(TEXT("Independent dependency bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Independent dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencySelfReferenceTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencySelfReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencySelfReferenceTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_self_reference.json"));
	TestFalse(TEXT("Self-reference dependency bundle fails"), Result.bOk);
	TestTrue(
		TEXT("Self-reference diagnostic has context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::DependencySelfReference,
			0,
			TEXT("self_reference_card"),
			FString(),
			TEXT("$.cards[0].references.card_ids[0]")));
	TestEqual(TEXT("Self-reference dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyCycleTwoCardsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyCycleTwoCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyCycleTwoCardsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_cycle_two_cards.json"));
	TestFalse(TEXT("Two-card dependency cycle fails"), Result.bOk);
	TestTrue(TEXT("Two-card cycle diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::DependencyCycleDetected));
	TestEqual(TEXT("Two-card cycle dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyCycleThreeCardsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyCycleThreeCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyCycleThreeCardsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_cycle_three_cards.json"));
	TestFalse(TEXT("Three-card dependency cycle fails"), Result.bOk);
	TestTrue(TEXT("Three-card cycle diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::DependencyCycleDetected));
	TestEqual(TEXT("Three-card cycle dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyCycleEffectRefTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyCycleEffectRef", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyCycleEffectRefTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_cycle_effect_ref.json"));
	TestFalse(TEXT("Effect-ref dependency cycle fails"), Result.bOk);
	TestTrue(
		TEXT("Effect-ref cycle diagnostic has effect context"),
		FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
			Result.Diagnostics,
			EWBCardDBSchemaDiagnostic::DependencyCycleDetected,
			1,
			TEXT("effect_cycle_b"),
			TEXT("return_ref"),
			TEXT("$.cards[1].activated_effects[0].references.effect_refs[0]")));
	TestEqual(TEXT("Effect-ref cycle dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyCyclePayloadRefTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyCyclePayloadRef", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyCyclePayloadRefTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_cycle_payload_ref.json"));
	TestFalse(TEXT("Payload-ref dependency cycle fails"), Result.bOk);
	TestTrue(TEXT("Payload-ref cycle diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::DependencyCycleDetected));

	const FWBCardDBSchemaValidationDiagnostic* Diagnostic =
		FindDiagnostic(Result, EWBCardDBSchemaDiagnostic::DependencyCycleDetected);
	TestTrue(TEXT("Payload-ref cycle diagnostic exists"), Diagnostic != nullptr);
	if (Diagnostic != nullptr)
	{
		TestTrue(TEXT("Payload-ref cycle path includes payloads"), Diagnostic->JsonPath.Contains(TEXT(".payloads[0].references")));
	}

	TestEqual(TEXT("Payload-ref cycle dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyHiddenTokenSafeTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyHiddenTokenSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_hidden_token_safe.json"));
	TestFalse(TEXT("Hidden dependency token bundle fails"), Result.bOk);
	TestTrue(TEXT("Hidden dependency missing reference diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::MissingCardReference));
	TestEqual(TEXT("Hidden dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);

	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		TestFalse(TEXT("Message omits SECRET"), Diagnostic.Message.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("JsonPath omits SECRET"), Diagnostic.JsonPath.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("BundleCardId omits SECRET"), Diagnostic.BundleCardId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("CardId omits SECRET"), Diagnostic.CardId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
		TestFalse(TEXT("EffectId omits SECRET"), Diagnostic.EffectId.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaMissingReferenceSkipsDependencyOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.MissingReferenceSkipsDependencyOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaMissingReferenceSkipsDependencyOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_missing_card_reference_card_level.json"));
	TestFalse(TEXT("Missing-reference bundle fails"), Result.bOk);
	TestTrue(TEXT("Missing-reference diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::MissingCardReference));
	TestEqual(TEXT("Missing-reference dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDuplicateCardIdSkipsDependencyOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DuplicateCardIdSkipsDependencyOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDuplicateCardIdSkipsDependencyOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_duplicate_card_id.json"));
	TestFalse(TEXT("Duplicate card id bundle fails"), Result.bOk);
	TestTrue(TEXT("Duplicate card id diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::CardIdDuplicate));
	TestEqual(TEXT("Duplicate card id dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyDuplicateReferencesTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyDuplicateReferences", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyDuplicateReferencesTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_duplicate_references.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("duplicate_ref_dependency"),
		TEXT("duplicate_ref_dependent")
	};

	TestTrue(TEXT("Duplicate-reference bundle validates"), Result.bOk);
	TestEqual(TEXT("Duplicate-reference diagnostics empty"), Result.Diagnostics.Num(), 0);
	ExpectStringArrayEquals(*this, TEXT("Duplicate-reference dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_duplicate_references.json"),
		TEXT("valid_bundle_dependency_duplicate_references.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyMixedReferenceLevelsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyMixedReferenceLevels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyMixedReferenceLevelsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_mixed_reference_levels.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("mixed_ref_card_dependency"),
		TEXT("mixed_ref_effect_dependency"),
		TEXT("mixed_ref_payload_dependency"),
		TEXT("mixed_ref_dependent")
	};

	TestTrue(TEXT("Mixed-reference-level bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Mixed-reference-level dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_mixed_reference_levels.json"),
		TEXT("valid_bundle_dependency_mixed_reference_levels.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyRepeatedPathsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyRepeatedPaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyRepeatedPathsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_repeated_paths.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("repeated_path_b"),
		TEXT("repeated_path_c"),
		TEXT("repeated_path_a")
	};

	TestTrue(TEXT("Repeated-path bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Repeated-path dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_repeated_paths.json"),
		TEXT("valid_bundle_dependency_repeated_paths.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyDisconnectedComponentsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyDisconnectedComponents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyDisconnectedComponentsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_disconnected_components.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("component_d"),
		TEXT("component_c"),
		TEXT("component_b"),
		TEXT("component_a"),
		TEXT("component_e")
	};

	TestTrue(TEXT("Disconnected-components bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Disconnected-components dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_disconnected_components.json"),
		TEXT("valid_bundle_dependency_disconnected_components.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyIndependentCardsInputOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyIndependentCardsInputOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyIndependentCardsInputOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_independent_cards_input_order.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("independent_gamma"),
		TEXT("independent_alpha"),
		TEXT("independent_beta")
	};

	TestTrue(TEXT("Independent-cards bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Independent-cards dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_independent_cards_input_order.json"),
		TEXT("valid_bundle_dependency_independent_cards_input_order.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyEffectAndPayloadRefsSameCardTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyEffectAndPayloadRefsSameCard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyEffectAndPayloadRefsSameCardTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_effect_and_payload_refs_same_card.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("same_ref_dependency"),
		TEXT("same_ref_dependent")
	};

	TestTrue(TEXT("Effect/payload same-ref bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Effect/payload same-ref dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_effect_and_payload_refs_same_card.json"),
		TEXT("valid_bundle_dependency_effect_and_payload_refs_same_card.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyMissingReferenceEdgeCaseSkipsOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyMissingReferenceEdgeCaseSkipsOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyMissingReferenceEdgeCaseSkipsOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_missing_reference_skips_order.json"));

	TestFalse(TEXT("Missing-reference edge-case bundle fails"), Result.bOk);
	TestTrue(TEXT("Missing-reference edge-case diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::MissingCardReference));
	TestEqual(TEXT("Missing-reference edge-case dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_missing_reference_skips_order.json"),
		TEXT("invalid_bundle_dependency_missing_reference_skips_order.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyDuplicateCardEdgeCaseSkipsOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyDuplicateCardEdgeCaseSkipsOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyDuplicateCardEdgeCaseSkipsOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_duplicate_card_skips_order.json"));

	TestFalse(TEXT("Duplicate-card edge-case bundle fails"), Result.bOk);
	TestTrue(TEXT("Duplicate-card edge-case diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::CardIdDuplicate));
	TestEqual(TEXT("Duplicate-card edge-case dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_duplicate_card_skips_order.json"),
		TEXT("invalid_bundle_dependency_duplicate_card_skips_order.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyInvalidCardSkipsOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyInvalidCardSkipsOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyInvalidCardSkipsOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("invalid_bundle_dependency_invalid_card_skips_order.json"));

	TestFalse(TEXT("Invalid-card dependency bundle fails"), Result.bOk);
	TestTrue(TEXT("Invalid-card dependency diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::UnsupportedPayloadType));
	TestEqual(TEXT("Invalid-card dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_invalid_card_skips_order.json"),
		TEXT("invalid_bundle_dependency_invalid_card_skips_order.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyStrictUnknownFieldSkipsOrderTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyStrictUnknownFieldSkipsOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyStrictUnknownFieldSkipsOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixtureStrict(TEXT("strict_invalid_bundle_dependency_unknown_field_skips_order.json"));

	TestFalse(TEXT("Strict unknown-field dependency bundle fails"), Result.bOk);
	TestTrue(TEXT("Strict unknown-field dependency diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::UnknownTopLevelField));
	TestEqual(TEXT("Strict unknown-field dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	ExpectStrictExportMatches(
		*this,
		TEXT("strict_invalid_bundle_dependency_unknown_field_skips_order.json"),
		TEXT("strict_invalid_bundle_dependency_unknown_field_skips_order.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyNonStrictUnknownFieldOrdersTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyNonStrictUnknownFieldOrders", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyNonStrictUnknownFieldOrdersTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("non_strict_bundle_dependency_unknown_field_ordered.json"));
	const TArray<FString> ExpectedOrder = {
		TEXT("non_strict_unknown_dependency"),
		TEXT("non_strict_unknown_dependent")
	};

	TestTrue(TEXT("Non-strict unknown-field dependency bundle validates"), Result.bOk);
	ExpectStringArrayEquals(*this, TEXT("Non-strict unknown-field dependency order"), Result.DependencyOrderCardIds, ExpectedOrder);
	ExpectExportMatches(
		*this,
		TEXT("non_strict_bundle_dependency_unknown_field_ordered.json"),
		TEXT("non_strict_bundle_dependency_unknown_field_ordered.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyHiddenReferenceSafeTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyHiddenReferenceSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyHiddenReferenceSafeTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_dependency_hidden_reference_safe.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("invalid_bundle_dependency_hidden_reference_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden-reference dependency bundle fails"), Result.bOk);
	TestTrue(TEXT("Hidden-reference missing-reference diagnostic present"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::MissingCardReference));
	TestEqual(TEXT("Hidden-reference dependency order empty"), Result.DependencyOrderCardIds.Num(), 0);
	TestFalse(TEXT("Hidden-reference export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden-reference export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaDependencyEdgeCaseExportDeterministicTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.DependencyEdgeCaseExportDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaDependencyEdgeCaseExportDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_disconnected_components.json"));

	FString FirstJson;
	FString SecondJson;
	TestTrue(
		TEXT("First edge-case export succeeds"),
		FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(Result, FirstJson));
	TestTrue(
		TEXT("Second edge-case export succeeds"),
		FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Edge-case export helper deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportValidSimpleChainMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportValidSimpleChainMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportValidSimpleChainMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_order_simple_chain.json"),
		TEXT("valid_bundle_dependency_order_simple_chain.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportValidReverseInputMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportValidReverseInputMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportValidReverseInputMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_order_reverse_input.json"),
		TEXT("valid_bundle_dependency_order_reverse_input.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportValidDiamondMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportValidDiamondMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportValidDiamondMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("valid_bundle_dependency_order_diamond.json"),
		TEXT("valid_bundle_dependency_order_diamond.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportSelfReferenceMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportSelfReferenceMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportSelfReferenceMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_self_reference.json"),
		TEXT("invalid_bundle_dependency_self_reference.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportTwoCardCycleMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportTwoCardCycleMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportTwoCardCycleMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_cycle_two_cards.json"),
		TEXT("invalid_bundle_dependency_cycle_two_cards.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportEffectRefCycleMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportEffectRefCycleMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportEffectRefCycleMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_dependency_cycle_effect_ref.json"),
		TEXT("invalid_bundle_dependency_cycle_effect_ref.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportHiddenTokenSafeMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportHiddenTokenSafeMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportHiddenTokenSafeMatchesFixtureTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_dependency_hidden_token_safe.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("invalid_bundle_dependency_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden dependency export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden dependency export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportMultipleInvalidCardsMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportMultipleInvalidCardsMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportMultipleInvalidCardsMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_multiple_invalid_cards.json"),
		TEXT("invalid_bundle_multiple_invalid_cards.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportMissingSchemaVersionMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportMissingSchemaVersionMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportMissingSchemaVersionMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_missing_schema_version.json"),
		TEXT("invalid_bundle_missing_schema_version.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportUnsupportedSchemaVersionMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportUnsupportedSchemaVersionMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportUnsupportedSchemaVersionMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_unsupported_schema_version.json"),
		TEXT("invalid_bundle_unsupported_schema_version.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportMissingCardDBVersionMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportMissingCardDBVersionMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportMissingCardDBVersionMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_missing_carddb_version.json"),
		TEXT("invalid_bundle_missing_carddb_version.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportMissingCardsMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportMissingCardsMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportMissingCardsMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_missing_cards.json"),
		TEXT("invalid_bundle_missing_cards.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportCardsNotArrayMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportCardsNotArrayMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportCardsNotArrayMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_cards_not_array.json"),
		TEXT("invalid_bundle_cards_not_array.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportCardsEmptyMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportCardsEmptyMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportCardsEmptyMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_cards_empty.json"),
		TEXT("invalid_bundle_cards_empty.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportDuplicateCardIdMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportDuplicateCardIdMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportDuplicateCardIdMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_duplicate_card_id.json"),
		TEXT("invalid_bundle_duplicate_card_id.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportContainsInvalidCardMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportContainsInvalidCardMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportContainsInvalidCardMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("invalid_bundle_contains_invalid_card.json"),
		TEXT("invalid_bundle_contains_invalid_card.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportStrictUnknownTopLevelMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportStrictUnknownTopLevelMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportStrictUnknownTopLevelMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectStrictExportMatches(
		*this,
		TEXT("strict_invalid_bundle_unknown_top_level_field.json"),
		TEXT("strict_invalid_bundle_unknown_top_level_field.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportStrictUnknownMetadataMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportStrictUnknownMetadataMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportStrictUnknownMetadataMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectStrictExportMatches(
		*this,
		TEXT("strict_invalid_bundle_unknown_metadata_field.json"),
		TEXT("strict_invalid_bundle_unknown_metadata_field.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportNonStrictUnknownAllowedMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportNonStrictUnknownAllowedMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportNonStrictUnknownAllowedMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectExportMatches(
		*this,
		TEXT("non_strict_bundle_unknown_field_allowed.json"),
		TEXT("non_strict_bundle_unknown_field_allowed.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportStrictUnknownAllowedFixtureFailsTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportStrictUnknownAllowedFixtureFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportStrictUnknownAllowedFixtureFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictExportMatches(
		*this,
		TEXT("non_strict_bundle_unknown_field_allowed.json"),
		TEXT("strict_non_strict_bundle_unknown_field_allowed.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportMixedBundleAndCardErrorsMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportMixedBundleAndCardErrorsMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportMixedBundleAndCardErrorsMatchesFixtureTest::RunTest(const FString& Parameters)
{
	ExpectStrictExportMatches(
		*this,
		TEXT("invalid_bundle_mixed_bundle_and_card_errors.json"),
		TEXT("invalid_bundle_mixed_bundle_and_card_errors.export.json"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportReferenceHiddenTokenSafeMatchesFixtureTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportReferenceHiddenTokenSafeMatchesFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportReferenceHiddenTokenSafeMatchesFixtureTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	FString ExpectedJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_reference_hidden_token_safe.json"), Result, ActualJson);
	LoadExpectedExportFixture(*this, TEXT("invalid_bundle_reference_hidden_token_safe.export.json"), ExpectedJson);

	TestFalse(TEXT("Hidden reference export omits SECRET"), ActualJson.Contains(TEXT("SECRET"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Hidden reference export matches expected"), ActualJson, ExpectedJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportBundleLevelHelperDeterministicTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportBundleLevelHelperDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportBundleLevelHelperDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixtureStrict(TEXT("invalid_bundle_mixed_bundle_and_card_errors.json"));

	FString FirstJson;
	FString SecondJson;
	TestTrue(
		TEXT("First bundle-level export succeeds"),
		FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(Result, FirstJson));
	TestTrue(
		TEXT("Second bundle-level export succeeds"),
		FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Bundle-level export helper deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportHelperDeterministicTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportHelperDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportHelperDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDBBundleSchemaValidationResult Result =
		ValidateBundleFixture(TEXT("valid_bundle_dependency_order_diamond.json"));

	FString FirstJson;
	FString SecondJson;
	TestTrue(
		TEXT("First export succeeds"),
		FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(Result, FirstJson));
	TestTrue(
		TEXT("Second export succeeds"),
		FWBCardDBSchemaFixtureValidator::BundleValidationResultToJsonStringForTest(Result, SecondJson));
	TestEqual(TEXT("Export helper deterministic"), FirstJson, SecondJson);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportOmitsSourceJsonTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportOmitsSourceJson", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportOmitsSourceJsonTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_dependency_cycle_effect_ref.json"), Result, ActualJson);

	TestFalse(TEXT("Export omits public labels"), ActualJson.Contains(TEXT("public_label")));
	TestFalse(TEXT("Export omits payload types"), ActualJson.Contains(TEXT("damage_effect")));
	TestFalse(TEXT("Export omits payload amount field"), ActualJson.Contains(TEXT("amount")));
	TestFalse(TEXT("Export omits source schema version"), ActualJson.Contains(TEXT("bundle_schema_version")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBBundleSchemaExportOmitsDiagnosticMessageTest, "Wandbound.Core.CardDBBundleSchemaFixtureValidation.ExportOmitsDiagnosticMessage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBBundleSchemaExportOmitsDiagnosticMessageTest::RunTest(const FString& Parameters)
{
	FWBCardDBBundleSchemaValidationResult Result;
	FString ActualJson;
	ExportBundleFixtureJson(*this, TEXT("invalid_bundle_dependency_cycle_two_cards.json"), Result, ActualJson);

	TestFalse(TEXT("Export omits message field"), ActualJson.Contains(TEXT("\"message\"")));
	TestFalse(TEXT("Export omits diagnostic message text"), ActualJson.Contains(TEXT("dependency cycle detected")));
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
