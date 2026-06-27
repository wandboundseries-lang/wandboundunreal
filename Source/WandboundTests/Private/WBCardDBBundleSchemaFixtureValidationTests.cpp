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
