#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationExpansion.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeExpansionPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeExpansionUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 CurrentArmor = 0,
	const int32 MaxArmor = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(CurrentArmor, MaxArmor);
	return Unit;
}

FWBGameStateData MakeExpansionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeExpansionPlayer(0, 2));
	State.Players.Add(MakeExpansionPlayer(1, 0));
	State.AddUnitForTest(MakeExpansionUnit(1, 0, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeExpansionUnit(2, 1, FWBTile(5, 4), 5, 6, 0, 3));
	State.AddUnitForTest(MakeExpansionUnit(3, 1, FWBTile(6, 4), 3, 5));
	return State;
}

FWBGenericEffectPayload MakeExpansionArmorPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = EWBArmorEffectOp::AddCurrentArmor;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeExpansionStatusPayload(const FName StatusId, const int32 Duration)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeExpansionDamagePayload(const int32 Amount, const bool bBypassArmor = false)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = bBypassArmor;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeExpansionHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBCardDefinition MakeExpansionDefinition(
	const FString& CardId,
	const FString& EffectId,
	const EWBCardEffectTargetRequirement TargetRequirement,
	const TArray<FWBGenericEffectPayload>& Payloads)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Fixture;

	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = EffectId;
	Effect.TargetRequirement = TargetRequirement;
	Effect.Payloads = Payloads;
	Definition.ActivatedEffects.Add(Effect);
	return Definition;
}

FWBCardActivationExpansionRequest MakeExpansionRequest(
	const FWBCardDefinition& Definition,
	const FString& EffectId,
	const int32 TargetUnitId = 2)
{
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = 0;
	Request.SourceUnitId = 1;
	Request.CardDefinition = Definition;
	Request.EffectId = EffectId;
	Request.Target.TargetUnitId = TargetUnitId;
	Request.DebugActivationId = TEXT("fixture_debug_activation");
	return Request;
}

FWBAction MakeExpansionMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeExpansionAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeExpansionPlayerAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FString ExpansionFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

TSharedPtr<FJsonObject> ParseExpansionJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

bool LoadExpansionFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ExpansionFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseExpansionJsonObject(Json);
	return OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		&& Child->IsValid())
	{
		return *Child;
	}

	return nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Values))
	{
		return Values;
	}

	return nullptr;
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
}

bool ReadExpectedOk(const TSharedPtr<FJsonObject>& Fixture, bool& bOutOk)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	return Expected.IsValid() && Expected->TryGetBoolField(TEXT("ok"), bOutOk);
}

FString ReadExpectedReasonContains(const TSharedPtr<FJsonObject>& Fixture)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	FString ReasonContains;
	if (Expected.IsValid())
	{
		Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains);
	}
	return ReasonContains;
}

FString PayloadOperationToFixtureString(const EWBGenericEffectOp Operation)
{
	if (Operation == EWBGenericEffectOp::ArmorEffect)
	{
		return TEXT("armor_effect");
	}

	if (Operation == EWBGenericEffectOp::StatusEffect)
	{
		return TEXT("status_effect");
	}

	if (Operation == EWBGenericEffectOp::DamageEffect)
	{
		return TEXT("damage_effect");
	}

	if (Operation == EWBGenericEffectOp::HealEffect)
	{
		return TEXT("heal_effect");
	}

	return TEXT("unknown");
}

void CompareOptionalIntegerTraceField(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TCHAR* FieldName,
	const int32 ActualValue)
{
	int32 ExpectedValue = 0;
	if (TryReadIntegerField(Expected, FieldName, ExpectedValue))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s %s"), *Label, FieldName), ActualValue, ExpectedValue);
	}
}

bool ExpectTraceFieldsFromFixture(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TArray<TSharedPtr<FJsonValue>>* TraceFields = GetArrayField(Expected, TEXT("trace_fields"));
	if (TraceFields == nullptr)
	{
		return true;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s trace field count"), *Label), TraceEvents.Num(), TraceFields->Num());
	const int32 NumToCompare = FMath::Min(TraceEvents.Num(), TraceFields->Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		const TSharedPtr<FJsonObject> ExpectedTrace = (*TraceFields)[Index].IsValid() ? (*TraceFields)[Index]->AsObject() : nullptr;
		if (!ExpectedTrace.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s trace %d malformed expected trace fields"), *Label, Index));
			return false;
		}

		const FWBTraceEvent& ActualTrace = TraceEvents[Index];
		const FString TraceLabel = FString::Printf(TEXT("%s trace %d"), *Label, Index);
		FString ExpectedKind;
		if (ExpectedTrace->TryGetStringField(TEXT("kind"), ExpectedKind))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s kind"), *TraceLabel), ActualTrace.Kind.ToString(), ExpectedKind);
		}

		FString ExpectedStatusId;
		if (ExpectedTrace->TryGetStringField(TEXT("status_id"), ExpectedStatusId))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s status id"), *TraceLabel), ActualTrace.StatusId.ToString(), ExpectedStatusId);
		}

		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("player_id"), ActualTrace.PlayerId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("source_unit_id"), ActualTrace.SourceUnitId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("target_unit_id"), ActualTrace.TargetUnitId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("damage_amount"), ActualTrace.DamageAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_hp"), ActualTrace.PreviousHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_hp"), ActualTrace.NewHP);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_armor"), ActualTrace.PreviousArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_armor"), ActualTrace.NewArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("effective_heal_amount"), ActualTrace.EffectiveHealAmount);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_status_turns"), ActualTrace.NewStatusTurns);
	}

	return TraceEvents.Num() == TraceFields->Num();
}

bool CompareBuiltCommandFromFixture(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationCommand& Command)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TSharedPtr<FJsonObject> BuiltCommand = GetObjectField(Expected, TEXT("built_command"));
	if (!BuiltCommand.IsValid())
	{
		return true;
	}

	int32 ExpectedInteger = -1;
	if (TryReadIntegerField(BuiltCommand, TEXT("player_id"), ExpectedInteger))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s player"), *Label), Command.Source.PlayerId, ExpectedInteger);
	}

	if (TryReadIntegerField(BuiltCommand, TEXT("source_unit_id"), ExpectedInteger))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s source unit"), *Label), Command.Source.SourceUnitId, ExpectedInteger);
	}

	if (TryReadIntegerField(BuiltCommand, TEXT("target_unit_id"), ExpectedInteger))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s target unit"), *Label), Command.EffectRequest.Target.TargetUnitId, ExpectedInteger);
	}

	FString ExpectedString;
	if (BuiltCommand->TryGetStringField(TEXT("source_card_id"), ExpectedString))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s source card"), *Label), Command.Source.SourceCardId, ExpectedString);
		Test.TestEqual(*FString::Printf(TEXT("%s request source card"), *Label), Command.EffectRequest.Source.SourceCardId, ExpectedString);
	}

	if (BuiltCommand->TryGetStringField(TEXT("source_effect_id"), ExpectedString))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s source effect"), *Label), Command.Source.SourceEffectId, ExpectedString);
		Test.TestEqual(*FString::Printf(TEXT("%s request source effect"), *Label), Command.EffectRequest.Source.SourceEffectId, ExpectedString);
	}

	if (BuiltCommand->TryGetStringField(TEXT("debug_activation_id"), ExpectedString))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s debug id"), *Label), Command.DebugActivationId, ExpectedString);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedOperations = GetArrayField(BuiltCommand, TEXT("payload_operations"));
	if (ExpectedOperations != nullptr)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s payload op count"), *Label), Command.EffectRequest.Payloads.Num(), ExpectedOperations->Num());
		const int32 NumToCompare = FMath::Min(Command.EffectRequest.Payloads.Num(), ExpectedOperations->Num());
		for (int32 Index = 0; Index < NumToCompare; ++Index)
		{
			const FString ActualOperation = PayloadOperationToFixtureString(Command.EffectRequest.Payloads[Index].Operation);
			const FString ExpectedOperation = (*ExpectedOperations)[Index].IsValid() ? (*ExpectedOperations)[Index]->AsString() : FString();
			Test.TestEqual(*FString::Printf(TEXT("%s payload op %d"), *Label, Index), ActualOperation, ExpectedOperation);
		}
	}

	return true;
}

bool SerializedJsonExcludesExpectedForbiddenSubstrings(
	FAutomationTestBase& Test,
	const TSharedPtr<FJsonObject>& Fixture,
	const FString& TraceJson,
	const FString& RuntimeJson)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = GetArrayField(Expected, TEXT("forbidden_substrings"));
	if (ForbiddenSubstrings == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Value : *ForbiddenSubstrings)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			continue;
		}

		const FString Forbidden = Value->AsString();
		Test.TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Forbidden), TraceJson.Contains(Forbidden));
		Test.TestFalse(*FString::Printf(TEXT("Runtime excludes %s"), *Forbidden), RuntimeJson.Contains(Forbidden));
	}

	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionBuildFamiliesTest, "Wandbound.Core.CardDefinitionActivationExpansion.BuildsEffectFamilies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionBuildFamiliesTest::RunTest(const FString& Parameters)
{
	const TArray<FWBCardDefinition> Definitions = {
		MakeExpansionDefinition(TEXT("fixture_armor_card"), TEXT("armor_2"), EWBCardEffectTargetRequirement::Unit, { MakeExpansionArmorPayload(2) }),
		MakeExpansionDefinition(TEXT("fixture_status_card"), TEXT("burn_2"), EWBCardEffectTargetRequirement::Unit, { MakeExpansionStatusPayload(FName(TEXT("Burn")), 2) }),
		MakeExpansionDefinition(TEXT("fixture_damage_card"), TEXT("deal_2"), EWBCardEffectTargetRequirement::Unit, { MakeExpansionDamagePayload(2) }),
		MakeExpansionDefinition(TEXT("fixture_heal_card"), TEXT("heal_2"), EWBCardEffectTargetRequirement::Unit, { MakeExpansionHealPayload(2) })
	};

	const TArray<FString> EffectIds = { TEXT("armor_2"), TEXT("burn_2"), TEXT("deal_2"), TEXT("heal_2") };
	const TArray<EWBGenericEffectOp> Operations = {
		EWBGenericEffectOp::ArmorEffect,
		EWBGenericEffectOp::StatusEffect,
		EWBGenericEffectOp::DamageEffect,
		EWBGenericEffectOp::HealEffect
	};

	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const FWBCardActivationExpansionResult Result = WBCardActivationExpansion::BuildActivationCommand(
			MakeExpansionRequest(Definitions[Index], EffectIds[Index]));
		TestTrue(*FString::Printf(TEXT("Build %d ok"), Index), Result.bOk);
		TestEqual(*FString::Printf(TEXT("Build %d source card"), Index), Result.Command.Source.SourceCardId, Definitions[Index].CardId);
		TestEqual(*FString::Printf(TEXT("Build %d source effect"), Index), Result.Command.Source.SourceEffectId, EffectIds[Index]);
		TestEqual(*FString::Printf(TEXT("Build %d operation"), Index), Result.Command.EffectRequest.Payloads[0].Operation, Operations[Index]);
		TestEqual(*FString::Printf(TEXT("Build %d target"), Index), Result.Command.EffectRequest.Target.TargetUnitId, 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionApplyBuiltCommandsTest, "Wandbound.Core.CardDefinitionActivationExpansion.BuildAndApplyCommands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionApplyBuiltCommandsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData DamageState = MakeExpansionState();
	const FWBCardDefinition DamageDefinition = MakeExpansionDefinition(
		TEXT("fixture_damage_card"),
		TEXT("deal_2"),
		EWBCardEffectTargetRequirement::Unit,
		{ MakeExpansionDamagePayload(2) });
	const FWBCardActivationExpansionResult DamageExpansion = WBCardActivationExpansion::BuildActivationCommand(
		MakeExpansionRequest(DamageDefinition, TEXT("deal_2")));
	TestTrue(TEXT("Damage expansion ok"), DamageExpansion.bOk);
	const FWBCardActivationCommandResult DamageResult = WBEffectRunner::ApplyCardActivationCommand(DamageState, DamageExpansion.Command);
	TestTrue(TEXT("Damage apply ok"), DamageResult.bOk);
	TestEqual(TEXT("Damage target hp"), DamageState.GetUnitById(2)->HP, 3);

	FWBGameStateData MixedState = MakeExpansionState();
	MixedState.GetMutableUnitById(2)->HP = 4;
	const FWBCardDefinition MixedDefinition = MakeExpansionDefinition(
		TEXT("fixture_mixed_card"),
		TEXT("mixed_unit_effect"),
		EWBCardEffectTargetRequirement::Unit,
		{
			MakeExpansionArmorPayload(2),
			MakeExpansionStatusPayload(FName(TEXT("Burn")), 2),
			MakeExpansionDamagePayload(1, true),
			MakeExpansionHealPayload(2)
		});
	const FWBCardActivationExpansionResult MixedExpansion = WBCardActivationExpansion::BuildActivationCommand(
		MakeExpansionRequest(MixedDefinition, TEXT("mixed_unit_effect")));
	TestTrue(TEXT("Mixed expansion ok"), MixedExpansion.bOk);
	const FWBCardActivationCommandResult MixedResult = WBEffectRunner::ApplyCardActivationCommand(MixedState, MixedExpansion.Command);
	TestTrue(TEXT("Mixed apply ok"), MixedResult.bOk);
	const FWBUnitState* Target = MixedState.GetUnitById(2);
	TestEqual(TEXT("Mixed hp"), Target->HP, 5);
	TestEqual(TEXT("Mixed armor"), Target->GetCurrentArmor(), 2);
	TestTrue(TEXT("Mixed status"), Target->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Mixed status duration"), Target->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);
	TestEqual(TEXT("Mixed trace count"), MixedResult.TraceEvents.Num(), 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionFailureReasonsTest, "Wandbound.Core.CardDefinitionActivationExpansion.FailureReasons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionFailureReasonsTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinition DamageDefinition = MakeExpansionDefinition(
		TEXT("fixture_damage_card"),
		TEXT("deal_2"),
		EWBCardEffectTargetRequirement::Unit,
		{ MakeExpansionDamagePayload(2) });

	FWBCardActivationExpansionRequest MissingEffectRequest = MakeExpansionRequest(DamageDefinition, TEXT("missing_effect"));
	TestEqual(
		TEXT("Missing effect reason"),
		WBCardActivationExpansion::BuildActivationCommand(MissingEffectRequest).Reason,
		FString(TEXT("effect_id_not_found")));

	FWBCardDefinition AmbiguousDefinition = DamageDefinition;
	const FWBCardEffectDefinition DuplicateEffect = AmbiguousDefinition.ActivatedEffects[0];
	AmbiguousDefinition.ActivatedEffects.Add(DuplicateEffect);
	TestEqual(
		TEXT("Ambiguous effect reason"),
		WBCardActivationExpansion::BuildActivationCommand(MakeExpansionRequest(AmbiguousDefinition, TEXT("deal_2"))).Reason,
		FString(TEXT("effect_id_ambiguous")));

	FWBCardActivationExpansionRequest MissingTargetRequest = MakeExpansionRequest(DamageDefinition, TEXT("deal_2"));
	MissingTargetRequest.Target.TargetUnitId = -1;
	TestEqual(
		TEXT("Missing target reason"),
		WBCardActivationExpansion::BuildActivationCommand(MissingTargetRequest).Reason,
		FString(TEXT("missing_unit_target")));

	FWBCardDefinition EmptyPayloadDefinition = MakeExpansionDefinition(
		TEXT("fixture_empty_card"),
		TEXT("empty"),
		EWBCardEffectTargetRequirement::Unit,
		{});
	TestEqual(
		TEXT("Empty payload reason"),
		WBCardActivationExpansion::BuildActivationCommand(MakeExpansionRequest(EmptyPayloadDefinition, TEXT("empty"))).Reason,
		FString(TEXT("empty_effect_payloads")));

	FWBGenericEffectPayload UnknownPayload;
	UnknownPayload.Operation = EWBGenericEffectOp::Unknown;
	FWBCardDefinition UnknownPayloadDefinition = MakeExpansionDefinition(
		TEXT("fixture_unknown_payload_card"),
		TEXT("unknown_payload"),
		EWBCardEffectTargetRequirement::Unit,
		{ UnknownPayload });
	TestEqual(
		TEXT("Unknown payload reason"),
		WBCardActivationExpansion::BuildActivationCommand(MakeExpansionRequest(UnknownPayloadDefinition, TEXT("unknown_payload"))).Reason,
		FString(TEXT("unknown_effect_payload_operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionNoTargetRequirementBuildOnlyTest, "Wandbound.Core.CardDefinitionActivationExpansion.NoTargetRequirementBuildOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionNoTargetRequirementBuildOnlyTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinition Definition = MakeExpansionDefinition(
		TEXT("fixture_no_target_card"),
		TEXT("no_target_effect"),
		EWBCardEffectTargetRequirement::None,
		{ MakeExpansionArmorPayload(1) });
	FWBCardActivationExpansionRequest Request = MakeExpansionRequest(Definition, TEXT("no_target_effect"));
	Request.Target = FWBEffectTargetRef();

	const FWBCardActivationExpansionResult Result = WBCardActivationExpansion::BuildActivationCommand(Request);
	TestTrue(TEXT("No target requirement builds"), Result.bOk);
	TestEqual(TEXT("No unit target carried"), Result.Command.EffectRequest.Target.TargetUnitId, -1);
	TestEqual(TEXT("No target source card"), Result.Command.Source.SourceCardId, FString(TEXT("fixture_no_target_card")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionCandidateOrderingTest, "Wandbound.Core.CardDefinitionActivationExpansion.CandidateOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionCandidateOrderingTest::RunTest(const FString& Parameters)
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("fixture_order_card");
	Definition.PublicName = TEXT("Fixture Order");
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeExpansionDefinition(TEXT("fixture_order_card"), TEXT("deal_1"), EWBCardEffectTargetRequirement::Unit, { MakeExpansionDamagePayload(1) }).ActivatedEffects[0]);
	Definition.ActivatedEffects.Add(MakeExpansionDefinition(TEXT("fixture_order_card"), TEXT("heal_1"), EWBCardEffectTargetRequirement::Unit, { MakeExpansionHealPayload(1) }).ActivatedEffects[0]);

	FWBEffectTargetRef Target2;
	Target2.TargetUnitId = 2;
	FWBEffectTargetRef Target3;
	Target3.TargetUnitId = 3;

	TArray<FWBCardActivationCommand> Commands;
	WBCardActivationExpansion::BuildActivationCommandCandidates(Definition, 0, 1, { Target2, Target3 }, Commands);

	TestEqual(TEXT("Candidate count"), Commands.Num(), 4);
	TestEqual(TEXT("Candidate 0 effect"), Commands[0].Source.SourceEffectId, FString(TEXT("deal_1")));
	TestEqual(TEXT("Candidate 0 target"), Commands[0].EffectRequest.Target.TargetUnitId, 2);
	TestEqual(TEXT("Candidate 1 effect"), Commands[1].Source.SourceEffectId, FString(TEXT("deal_1")));
	TestEqual(TEXT("Candidate 1 target"), Commands[1].EffectRequest.Target.TargetUnitId, 3);
	TestEqual(TEXT("Candidate 2 effect"), Commands[2].Source.SourceEffectId, FString(TEXT("heal_1")));
	TestEqual(TEXT("Candidate 2 target"), Commands[2].EffectRequest.Target.TargetUnitId, 2);
	TestEqual(TEXT("Candidate 3 effect"), Commands[3].Source.SourceEffectId, FString(TEXT("heal_1")));
	TestEqual(TEXT("Candidate 3 target"), Commands[3].EffectRequest.Target.TargetUnitId, 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionCandidateSkipsInvalidTargetsTest, "Wandbound.Core.CardDefinitionActivationExpansion.CandidateSkipsInvalidTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionCandidateSkipsInvalidTargetsTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinition Definition = MakeExpansionDefinition(
		TEXT("fixture_skip_invalid_card"),
		TEXT("deal_1"),
		EWBCardEffectTargetRequirement::Unit,
		{ MakeExpansionDamagePayload(1) });

	FWBEffectTargetRef InvalidTarget;
	InvalidTarget.TargetUnitId = -1;
	FWBEffectTargetRef ValidTarget;
	ValidTarget.TargetUnitId = 2;

	TArray<FWBCardActivationCommand> Commands;
	WBCardActivationExpansion::BuildActivationCommandCandidates(Definition, 0, 1, { InvalidTarget, ValidTarget }, Commands);

	TestEqual(TEXT("Only valid target builds"), Commands.Num(), 1);
	TestEqual(TEXT("Valid target id"), Commands[0].EffectRequest.Target.TargetUnitId, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionBuildDoesNotMutateStateTest, "Wandbound.Core.CardDefinitionActivationExpansion.BuildDoesNotMutateState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionBuildDoesNotMutateStateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeExpansionState();
	const int32 InitialHP = State.GetUnitById(2)->HP;
	const int32 InitialArmor = State.GetUnitById(2)->GetCurrentArmor();

	const FWBCardDefinition Definition = MakeExpansionDefinition(
		TEXT("fixture_no_mutation_card"),
		TEXT("deal_2"),
		EWBCardEffectTargetRequirement::Unit,
		{ MakeExpansionDamagePayload(2) });

	const FWBCardActivationExpansionResult Result = WBCardActivationExpansion::BuildActivationCommand(
		MakeExpansionRequest(Definition, TEXT("deal_2")));
	TestTrue(TEXT("Build ok"), Result.bOk);
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(2)->HP, InitialHP);
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), InitialArmor);
	TestEqual(TEXT("No statuses added"), State.GetUnitById(2)->Statuses.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionSourceGuardTest, "Wandbound.Core.CardDefinitionActivationExpansion.SourceGuardNoRulesRunnerOrGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationExpansion.cpp"));
	FString Source;
	TestTrue(TEXT("Expansion source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));

	TestFalse(TEXT("No WBRules dependency"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No WBEffectRunner dependency"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No ApplyCardActivationCommand call"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	TestFalse(TEXT("No legal action generation"), Source.Contains(TEXT("GenerateLegalActions")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionLegalGenerationAndCodecUnchangedTest, "Wandbound.Core.CardDefinitionActivationExpansion.LegalGenerationAndActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionLegalGenerationAndCodecUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeExpansionState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));

	const FWBCardDefinition Definition = MakeExpansionDefinition(
		TEXT("fixture_generation_guard_card"),
		TEXT("heal_1"),
		EWBCardEffectTargetRequirement::Unit,
		{ MakeExpansionHealPayload(1) });
	WBCardActivationExpansion::BuildActivationCommand(MakeExpansionRequest(Definition, TEXT("heal_1")));

	const TArray<FString> AfterBuildIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));
	TestEqual(TEXT("Legal generation unchanged count"), AfterBuildIds.Num(), BaselineIds.Num());
	TestTrue(TEXT("Legal generation unchanged ids"), AfterBuildIds == BaselineIds);

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeExpansionMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeExpansionAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeExpansionPlayerAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeExpansionPlayerAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeExpansionPlayerAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionFixtureScenariosTest, "Wandbound.Core.CardDefinitionActivationExpansion.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_definition_build_damage_activation_command.json"),
		TEXT("card_definition_build_heal_activation_command.json"),
		TEXT("card_definition_build_status_activation_command.json"),
		TEXT("card_definition_build_armor_activation_command.json"),
		TEXT("card_definition_build_and_apply_damage_activation.json"),
		TEXT("card_definition_build_and_apply_mixed_activation.json"),
		TEXT("card_definition_missing_effect_fails.json"),
		TEXT("card_definition_ambiguous_effect_fails.json"),
		TEXT("card_definition_missing_target_fails.json"),
		TEXT("card_definition_source_metadata_not_public_trace.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadExpansionFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBCardActivationExpansionRequest Request;
		FString ParseReason;
		TestTrue(
			*FString::Printf(TEXT("%s parses activation expansion request: %s"), *FixtureName, *ParseReason),
			ParseCardActivationExpansionRequestFromFixture(Fixture, Request, ParseReason));

		const FWBCardActivationExpansionResult ExpansionResult = WBCardActivationExpansion::BuildActivationCommand(Request);
		CompareBuiltCommandFromFixture(*this, FixtureName, Fixture, ExpansionResult.Command);

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
		if (!StateReason.IsEmpty())
		{
			return false;
		}

		FWBApplyActionResult ApplyResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString ApplyReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *ApplyReason),
			ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));

		FString OperationName;
		Fixture->TryGetStringField(TEXT("operation"), OperationName);
		if (OperationName == TEXT("build_card_activation_command"))
		{
			TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::BuildCardActivationCommand);
		}
		else if (OperationName == TEXT("build_and_apply_card_activation_command"))
		{
			TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::BuildAndApplyCardActivationCommand);
		}

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ReadExpectedOk(Fixture, bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), ApplyResult.bOk, bExpectedOk);

		const FString ExpectedReasonContains = ReadExpectedReasonContains(Fixture);
		if (!ExpectedReasonContains.IsEmpty())
		{
			TestTrue(
				*FString::Printf(TEXT("%s reason contains %s"), *FixtureName, *ExpectedReasonContains),
				ApplyResult.Reason.Contains(ExpectedReasonContains));
		}

		FString ExpectReason;
		const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
		if (GetArrayField(Expected, TEXT("expected_trace_order")) != nullptr)
		{
			TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		}

		if (GetArrayField(Expected, TEXT("final_units")) != nullptr)
		{
			TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
		}

		TestTrue(*FString::Printf(TEXT("%s trace fields"), *FixtureName), ExpectTraceFieldsFromFixture(*this, FixtureName, Fixture, ApplyResult.TraceEvents));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionExpansionHiddenMetadataExclusionFixtureTest, "Wandbound.Core.CardDefinitionActivationExpansion.HiddenMetadataExcludedFromTraceAndRuntimeSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionExpansionHiddenMetadataExclusionFixtureTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Hidden fixture loads"), LoadExpansionFixture(TEXT("card_definition_source_metadata_not_public_trace.json"), Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(TEXT("State builds"), BuildGameStateFromFixture(Fixture, State, StateReason));
	if (!StateReason.IsEmpty())
	{
		return false;
	}

	FWBApplyActionResult ApplyResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString ApplyReason;
	TestTrue(TEXT("Fixture applies"), ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
	TestTrue(TEXT("Fixture succeeds"), ApplyResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("card_definition_activation_test"));
	Envelope.SelectedActionId = TEXT("card_definition_activation_test");
	Envelope.ApplyResult = ApplyResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));

	const FString SerializedTraceEvents = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	SerializedJsonExcludesExpectedForbiddenSubstrings(*this, Fixture, SerializedTraceEvents, SerializedJson);

	return true;
}

#endif
