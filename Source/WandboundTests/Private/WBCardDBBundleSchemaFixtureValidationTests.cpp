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
