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
