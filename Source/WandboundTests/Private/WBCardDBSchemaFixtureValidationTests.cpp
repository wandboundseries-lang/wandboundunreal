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

FWBCardDBSchemaValidationResult ValidateFixture(const FString& FileName)
{
	return FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(FileName));
}

FWBCardDBSchemaValidationResult ValidateFixtureStrict(const FString& FileName)
{
	FWBCardDBSchemaValidationOptions Options;
	Options.bStrictUnknownFieldRejection = true;
	return FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(FileName), Options);
}

bool HasDiagnostic(
	const FWBCardDBSchemaValidationResult& Result,
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

bool ExpectFixtureFailsWith(
	FAutomationTestBase& Test,
	const FString& FileName,
	const EWBCardDBSchemaDiagnostic ExpectedCode)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixture(FileName);
	Test.TestFalse(*FString::Printf(TEXT("%s fails validation"), *FileName), Result.bOk);
	Test.TestTrue(
		*FString::Printf(
			TEXT("%s has %s"),
			*FileName,
			*FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(ExpectedCode)),
		HasDiagnostic(Result, ExpectedCode));
	return !Result.bOk && HasDiagnostic(Result, ExpectedCode);
}

bool ExpectStrictFixtureFailsWith(
	FAutomationTestBase& Test,
	const FString& FileName,
	const EWBCardDBSchemaDiagnostic ExpectedCode)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixtureStrict(FileName);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaAllValidFixturesTest, "Wandbound.Core.CardDBSchemaFixtureValidation.AllValidFixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaAllValidFixturesTest::RunTest(const FString& Parameters)
{
	const FString ValidFixtures[] = {
		TEXT("valid_damage_card.json"),
		TEXT("valid_heal_card.json"),
		TEXT("valid_status_burn_card.json"),
		TEXT("valid_armor_card.json"),
		TEXT("valid_cost_once_per_turn_card.json")
	};

	for (const FString& FixtureName : ValidFixtures)
	{
		const FWBCardDBSchemaValidationResult Result = ValidateFixture(FixtureName);
		TestTrue(*FString::Printf(TEXT("%s validates"), *FixtureName), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s diagnostics"), *FixtureName), Result.Diagnostics.Num(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictValidBasicDamageCardTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictValidBasicDamageCard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictValidBasicDamageCardTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixtureStrict(TEXT("strict_valid_basic_damage_card.json"));
	TestTrue(TEXT("Strict valid basic damage fixture validates"), Result.bOk);
	TestEqual(TEXT("Strict valid basic damage diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictValidMetadataFieldsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictValidMetadataFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictValidMetadataFieldsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixtureStrict(TEXT("strict_valid_metadata_fields.json"));
	TestTrue(TEXT("Strict valid metadata fixture validates"), Result.bOk);
	TestEqual(TEXT("Strict valid metadata diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownTopLevelFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownTopLevelFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownTopLevelFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_top_level_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownCardFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownCardFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownCardFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_card_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownCardField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownEffectFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownEffectFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownEffectFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_effect_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownEffectField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownSourceGateFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownSourceGateFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownSourceGateFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_source_gate_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownSourceGateField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownCostGateFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownCostGateFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownCostGateFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_cost_gate_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownCostGateField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownPayloadFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownPayloadFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownPayloadFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_payload_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownPayloadField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStrictUnknownMetadataFieldFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StrictUnknownMetadataFieldFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStrictUnknownMetadataFieldFailsTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("strict_invalid_unknown_metadata_field.json"),
		EWBCardDBSchemaDiagnostic::UnknownMetadataField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaNonStrictUnknownFieldAllowedTest, "Wandbound.Core.CardDBSchemaFixtureValidation.NonStrictUnknownFieldAllowed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaNonStrictUnknownFieldAllowedTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixture(TEXT("non_strict_unknown_field_allowed.json"));
	TestTrue(TEXT("Non-strict unknown field fixture validates"), Result.bOk);
	TestEqual(TEXT("Non-strict unknown field diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaNonStrictUnknownFieldFailsWhenStrictTest, "Wandbound.Core.CardDBSchemaFixtureValidation.NonStrictUnknownFieldFailsWhenStrict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaNonStrictUnknownFieldFailsWhenStrictTest::RunTest(const FString& Parameters)
{
	ExpectStrictFixtureFailsWith(
		*this,
		TEXT("non_strict_unknown_field_allowed.json"),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaDefaultOptionsRemainNonStrictTest, "Wandbound.Core.CardDBSchemaFixtureValidation.DefaultOptionsRemainNonStrict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaDefaultOptionsRemainNonStrictTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result =
		FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(CardDBSchemaFixturePath(TEXT("non_strict_unknown_field_allowed.json")));
	TestTrue(TEXT("Default options allow unknown fields"), Result.bOk);
	TestEqual(TEXT("Default options unknown-field diagnostics"), Result.Diagnostics.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaValidDamageMapsToCardDefinitionTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ValidDamageMapsToCardDefinition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaValidDamageMapsToCardDefinitionTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixture(TEXT("valid_damage_card.json"));
	TestTrue(TEXT("Valid damage fixture validates"), Result.bOk);
	TestEqual(TEXT("CardId"), Result.CardDefinition.CardId, FString(TEXT("valid_damage_card")));
	TestEqual(TEXT("Activated effects"), Result.CardDefinition.ActivatedEffects.Num(), 1);
	if (Result.CardDefinition.ActivatedEffects.Num() > 0)
	{
		const FWBCardEffectDefinition& Effect = Result.CardDefinition.ActivatedEffects[0];
		TestEqual(TEXT("EffectId"), Effect.EffectId, FString(TEXT("deal_2")));
		TestEqual(TEXT("Target requirement"), Effect.TargetRequirement, EWBCardEffectTargetRequirement::Unit);
		TestEqual(TEXT("Payloads"), Effect.Payloads.Num(), 1);
		if (Effect.Payloads.Num() > 0)
		{
			TestEqual(TEXT("Payload op"), Effect.Payloads[0].Operation, EWBGenericEffectOp::DamageEffect);
			TestEqual(TEXT("Damage amount"), Effect.Payloads[0].DamageEffect.Amount, 2);
			TestFalse(TEXT("Bypass armor"), Effect.Payloads[0].DamageEffect.bBypassArmor);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaValidStatusMapsStatusIdAndDurationTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ValidStatusMapsStatusIdAndDuration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaValidStatusMapsStatusIdAndDurationTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixture(TEXT("valid_status_burn_card.json"));
	TestTrue(TEXT("Valid status fixture validates"), Result.bOk);
	TestEqual(TEXT("Activated effects"), Result.CardDefinition.ActivatedEffects.Num(), 1);
	if (Result.CardDefinition.ActivatedEffects.Num() > 0 && Result.CardDefinition.ActivatedEffects[0].Payloads.Num() > 0)
	{
		const FWBGenericEffectPayload& Payload = Result.CardDefinition.ActivatedEffects[0].Payloads[0];
		TestEqual(TEXT("Payload op"), Payload.Operation, EWBGenericEffectOp::StatusEffect);
		TestEqual(TEXT("Status operation"), Payload.StatusEffect.Operation, EWBStatusEffectOp::ApplyStatus);
		TestEqual(TEXT("Status id"), Payload.StatusEffect.StatusId, FName(TEXT("Burn")));
		TestEqual(TEXT("Duration"), Payload.StatusEffect.Duration, 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaValidCostOncePerTurnMapsSourceGateTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ValidCostOncePerTurnMapsSourceGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaValidCostOncePerTurnMapsSourceGateTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixture(TEXT("valid_cost_once_per_turn_card.json"));
	TestTrue(TEXT("Valid cost/usage fixture validates"), Result.bOk);
	TestEqual(TEXT("Activated effects"), Result.CardDefinition.ActivatedEffects.Num(), 1);
	if (Result.CardDefinition.ActivatedEffects.Num() > 0)
	{
		const FWBCardActivationSourceGateDefinition& Gate = Result.CardDefinition.ActivatedEffects[0].SourceGate;
		TestTrue(TEXT("Explicit source gate"), Gate.bHasExplicitSourceGate);
		TestEqual(TEXT("Required zone"), Gate.RequiredZone, EWBCardActivationSourceZone::Board);
		TestEqual(TEXT("Timing"), Gate.Timing, EWBCardActivationTimingRequirement::NormalTurnPriority);
		TestTrue(TEXT("Once per turn"), Gate.bOncePerTurn);
		TestEqual(TEXT("Once key"), Gate.OncePerTurnKey, FString(TEXT("heavy_burst")));
		TestTrue(TEXT("External affordability"), Gate.CostGate.bRequiresExternalAffordability);
		TestEqual(TEXT("Required RR"), Gate.CostGate.RequiredRR, 2);
		TestEqual(TEXT("Cost kind"), Gate.CostGate.CostKind, FName(TEXT("RR")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaMissingSchemaVersionFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.MissingSchemaVersionFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaMissingSchemaVersionFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_missing_schema_version.json"), EWBCardDBSchemaDiagnostic::SchemaVersionMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaMissingCardIdFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.MissingCardIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaMissingCardIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_missing_card_id.json"), EWBCardDBSchemaDiagnostic::CardIdMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaMissingPublicNameFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.MissingPublicNameFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaMissingPublicNameFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_missing_public_name.json"), EWBCardDBSchemaDiagnostic::PublicNameMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaMissingEffectIdFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.MissingEffectIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaMissingEffectIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_missing_effect_id.json"), EWBCardDBSchemaDiagnostic::EffectIdMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaUnsupportedCardKindFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.UnsupportedCardKindFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaUnsupportedCardKindFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_unsupported_card_kind.json"), EWBCardDBSchemaDiagnostic::UnsupportedCardKind);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaDuplicateEffectIdFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.DuplicateEffectIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaDuplicateEffectIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_duplicate_effect_id.json"), EWBCardDBSchemaDiagnostic::EffectIdDuplicate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaUnsupportedPayloadTypeFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.UnsupportedPayloadTypeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaUnsupportedPayloadTypeFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_unsupported_payload_type.json"), EWBCardDBSchemaDiagnostic::UnsupportedPayloadType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaUnsupportedPayloadOperationFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.UnsupportedPayloadOperationFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaUnsupportedPayloadOperationFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_unsupported_payload_operation.json"), EWBCardDBSchemaDiagnostic::UnsupportedPayloadOperation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaUnsupportedTargetRequirementFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.UnsupportedTargetRequirementFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaUnsupportedTargetRequirementFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_unsupported_target_requirement.json"), EWBCardDBSchemaDiagnostic::UnsupportedTargetRequirement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaUnsupportedTimingFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.UnsupportedTimingFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaUnsupportedTimingFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_unsupported_timing.json"), EWBCardDBSchemaDiagnostic::UnsupportedTiming);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaUnsupportedSourceZoneFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.UnsupportedSourceZoneFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaUnsupportedSourceZoneFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_unsupported_source_zone.json"), EWBCardDBSchemaDiagnostic::UnsupportedSourceZone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaMalformedJsonFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.MalformedJsonFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaMalformedJsonFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_malformed_json.json"), EWBCardDBSchemaDiagnostic::JsonParseFailed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaPayloadsMissingFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.PayloadsMissingFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaPayloadsMissingFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_payloads_missing.json"), EWBCardDBSchemaDiagnostic::PayloadsMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaPayloadsMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.PayloadsMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaPayloadsMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_payloads_not_array.json"), EWBCardDBSchemaDiagnostic::PayloadsMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaPayloadsEmptyFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.PayloadsEmptyFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaPayloadsEmptyFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_payloads_empty.json"), EWBCardDBSchemaDiagnostic::PayloadsEmpty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaActivatedEffectsMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ActivatedEffectsMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaActivatedEffectsMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(
		*this,
		TEXT("invalid_activated_effects_not_array.json"),
		EWBCardDBSchemaDiagnostic::ActivatedEffectsMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaSourceGateMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.SourceGateMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaSourceGateMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_source_gate_not_object.json"), EWBCardDBSchemaDiagnostic::SourceGateMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaCostGateMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.CostGateMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaCostGateMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_cost_gate_not_object.json"), EWBCardDBSchemaDiagnostic::CostGateMalformed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaDamageAmountMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.DamageAmountMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaDamageAmountMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(
		*this,
		TEXT("invalid_damage_amount_not_number.json"),
		EWBCardDBSchemaDiagnostic::InvalidNumericField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaHealAmountNegativeFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.HealAmountNegativeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaHealAmountNegativeFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_heal_amount_negative.json"), EWBCardDBSchemaDiagnostic::InvalidNumericField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaStatusTurnsMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.StatusTurnsMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaStatusTurnsMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(
		*this,
		TEXT("invalid_status_turns_not_number.json"),
		EWBCardDBSchemaDiagnostic::InvalidNumericField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaArmorAmountMalformedFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ArmorAmountMalformedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaArmorAmountMalformedFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(
		*this,
		TEXT("invalid_armor_amount_not_number.json"),
		EWBCardDBSchemaDiagnostic::InvalidNumericField);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaInvalidRRCostFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.InvalidRRCostFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaInvalidRRCostFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_negative_rr_cost.json"), EWBCardDBSchemaDiagnostic::InvalidRRCost);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaInvalidStatusIdFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.InvalidStatusIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaInvalidStatusIdFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(*this, TEXT("invalid_bad_status_id.json"), EWBCardDBSchemaDiagnostic::InvalidStatusId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaInternalLabelFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.InternalLabelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaInternalLabelFailsTest::RunTest(const FString& Parameters)
{
	ExpectFixtureFailsWith(
		*this,
		TEXT("invalid_player_facing_label_internal_term.json"),
		EWBCardDBSchemaDiagnostic::PlayerFacingLabelContainsInternalTerm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaHiddenInfoFailsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.HiddenInfoFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaHiddenInfoFailsTest::RunTest(const FString& Parameters)
{
	const FWBCardDBSchemaValidationResult Result = ValidateFixture(TEXT("invalid_hidden_info_policy_violation.json"));
	TestFalse(TEXT("Hidden-info fixture fails"), Result.bOk);
	TestTrue(TEXT("Hidden-info diagnostic"), HasDiagnostic(Result, EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation));
	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		TestFalse(TEXT("Diagnostic message omits hidden token SECRET_"), Diagnostic.Message.Contains(TEXT("SECRET_")));
		TestFalse(TEXT("Diagnostic message omits hidden token opponent_hand"), Diagnostic.Message.Contains(TEXT("opponent_hand")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaDiagnosticCodeStringsStableTest, "Wandbound.Core.CardDBSchemaFixtureValidation.DiagnosticCodeStringsStable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaDiagnosticCodeStringsStableTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("UnsupportedPayloadType string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnsupportedPayloadType),
		FString(TEXT("unsupported_payload_type")));
	TestEqual(
		TEXT("UnsupportedPayloadOperation string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnsupportedPayloadOperation),
		FString(TEXT("unsupported_payload_operation")));
	TestEqual(
		TEXT("HiddenInfo string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation),
		FString(TEXT("hidden_info_policy_violation")));
	TestEqual(
		TEXT("JsonParseFailed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::JsonParseFailed),
		FString(TEXT("json_parse_failed")));
	TestEqual(
		TEXT("PayloadsMissing string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::PayloadsMissing),
		FString(TEXT("payloads_missing")));
	TestEqual(
		TEXT("PayloadsMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::PayloadsMalformed),
		FString(TEXT("payloads_malformed")));
	TestEqual(
		TEXT("PayloadsEmpty string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::PayloadsEmpty),
		FString(TEXT("payloads_empty")));
	TestEqual(
		TEXT("ActivatedEffectsMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::ActivatedEffectsMalformed),
		FString(TEXT("activated_effects_malformed")));
	TestEqual(
		TEXT("SourceGateMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::SourceGateMalformed),
		FString(TEXT("source_gate_malformed")));
	TestEqual(
		TEXT("CostGateMalformed string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::CostGateMalformed),
		FString(TEXT("cost_gate_malformed")));
	TestEqual(
		TEXT("InvalidNumericField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::InvalidNumericField),
		FString(TEXT("invalid_numeric_field")));
	TestEqual(
		TEXT("UnknownTopLevelField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownTopLevelField),
		FString(TEXT("unknown_top_level_field")));
	TestEqual(
		TEXT("UnknownCardField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownCardField),
		FString(TEXT("unknown_card_field")));
	TestEqual(
		TEXT("UnknownEffectField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownEffectField),
		FString(TEXT("unknown_effect_field")));
	TestEqual(
		TEXT("UnknownSourceGateField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownSourceGateField),
		FString(TEXT("unknown_source_gate_field")));
	TestEqual(
		TEXT("UnknownCostGateField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownCostGateField),
		FString(TEXT("unknown_cost_gate_field")));
	TestEqual(
		TEXT("UnknownPayloadField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownPayloadField),
		FString(TEXT("unknown_payload_field")));
	TestEqual(
		TEXT("UnknownMetadataField string"),
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(EWBCardDBSchemaDiagnostic::UnknownMetadataField),
		FString(TEXT("unknown_metadata_field")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaValidatorNotIncludedByCoreOrRuntimeTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ValidatorNotIncludedByCoreOrRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaValidatorNotIncludedByCoreOrRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	FindSourceFiles(TEXT("Source/WandboundCore"), Files);
	FindSourceFiles(TEXT("Source/WandboundRuntime"), Files);

	FString FoundFile;
	const bool bFound = AnyFileContains(Files, TEXT("WBCardDBSchemaFixtureValidator"), FoundFile);
	TestFalse(*FString::Printf(TEXT("Validator not referenced by core/runtime: %s"), *FoundFile), bFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDBSchemaValidatorNoRuntimeGenerationCallsTest, "Wandbound.Core.CardDBSchemaFixtureValidation.ValidatorNoRuntimeGenerationCalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDBSchemaValidatorNoRuntimeGenerationCallsTest::RunTest(const FString& Parameters)
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
