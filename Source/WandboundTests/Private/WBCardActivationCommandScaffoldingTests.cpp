#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCommand.h"
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

FWBPlayerStateData MakeActivationPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeActivationUnit(
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

FWBGameStateData MakeActivationState(
	const int32 TargetHP = 5,
	const int32 TargetMaxHP = 5,
	const int32 TargetCurrentArmor = 0,
	const int32 TargetMaxArmor = 0)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeActivationPlayer(0, 2));
	State.Players.Add(MakeActivationPlayer(1, 0));
	State.AddUnitForTest(MakeActivationUnit(1, 0, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeActivationUnit(2, 1, FWBTile(5, 4), TargetHP, TargetMaxHP, TargetCurrentArmor, TargetMaxArmor));
	return State;
}

FWBGenericEffectPayload MakeArmorPayload(const EWBArmorEffectOp Operation, const int32 Amount, const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = Operation;
	Payload.ArmorEffect.TargetUnitId = TargetUnitId;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeStatusPayload(
	const EWBStatusEffectOp Operation,
	const FName StatusId,
	const int32 Duration,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = Operation;
	Payload.StatusEffect.TargetUnitId = TargetUnitId;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeDamagePayload(const int32 Amount, const bool bBypassArmor = false, const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.TargetUnitId = TargetUnitId;
	Payload.DamageEffect.SourceUnitId = -1;
	Payload.DamageEffect.SourcePlayerId = -1;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = bBypassArmor;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeHealPayload(const int32 Amount, const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.TargetUnitId = TargetUnitId;
	Payload.HealEffect.SourceUnitId = -1;
	Payload.HealEffect.SourcePlayerId = -1;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBCardActivationCommand MakeActivationCommand(const TArray<FWBGenericEffectPayload>& Payloads, const int32 TargetUnitId = 2)
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = 1;
	Command.Source.SourceCardId = TEXT("fixture_activation_card");
	Command.Source.SourceEffectId = TEXT("fixture_activation_effect");
	Command.DebugActivationId = TEXT("fixture_activation_debug");
	Command.EffectRequest.Target.TargetUnitId = TargetUnitId;
	Command.EffectRequest.Payloads = Payloads;
	return Command;
}

FWBAction MakeMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeAttackAction()
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

FWBAction MakePlayerAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString ActivationFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadActivationFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ActivationFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
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

bool CompareActionIdArrays(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FString>& Actual,
	const TArray<FString>& Expected)
{
	Test.TestEqual(*FString::Printf(TEXT("%s count"), *Label), Actual.Num(), Expected.Num());
	const int32 NumToCompare = FMath::Min(Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s action %d"), *Label, Index), Actual[Index], Expected[Index]);
	}

	return Actual == Expected;
}

bool RuntimeJsonHasPublicUnitHP(FAutomationTestBase& Test, const FString& SerializedJson, const int32 UnitId, const int32 ExpectedHP)
{
	const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
	const TSharedPtr<FJsonObject> BoardSummary = GetObjectField(Root, TEXT("final_public_board_summary"));
	const TArray<TSharedPtr<FJsonValue>>* Units = GetArrayField(BoardSummary, TEXT("units"));
	if (Units == nullptr)
	{
		Test.AddError(TEXT("Missing public board units"));
		return false;
	}

	for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
	{
		const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
		int32 CandidateUnitId = -1;
		int32 CandidateHP = -1;
		if (TryReadIntegerField(UnitObject, TEXT("unit_id"), CandidateUnitId)
			&& CandidateUnitId == UnitId
			&& TryReadIntegerField(UnitObject, TEXT("hp"), CandidateHP))
		{
			Test.TestEqual(TEXT("Public unit HP"), CandidateHP, ExpectedHP);
			return CandidateHP == ExpectedHP;
		}
	}

	Test.AddError(FString::Printf(TEXT("Missing public unit %d"), UnitId));
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationValidationFillAndGlobalTest, "Wandbound.Core.CardActivationCommandScaffolding.ValidationFillsSourceAndAllowsGlobal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationValidationFillAndGlobalTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationState(5, 5);
	FWBCardActivationCommand Command = MakeActivationCommand({ MakeDamagePayload(2) });
	const FWBCardActivationCommandResult Result = WBEffectRunner::ApplyCardActivationCommand(State, Command);
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Target damaged"), State.GetUnitById(2)->HP, 3);
	TestEqual(TEXT("Effect source player filled"), Result.Command.EffectRequest.Source.PlayerId, 0);
	TestEqual(TEXT("Effect source unit filled"), Result.Command.EffectRequest.Source.SourceUnitId, 1);
	TestEqual(TEXT("Effect source card filled"), Result.Command.EffectRequest.Source.SourceCardId, FString(TEXT("fixture_activation_card")));
	TestEqual(TEXT("Effect source effect filled"), Result.Command.EffectRequest.Source.SourceEffectId, FString(TEXT("fixture_activation_effect")));

	FWBGameStateData GlobalState = MakeActivationState(5, 5);
	FWBCardActivationCommand GlobalCommand = MakeActivationCommand({ MakeHealPayload(0) });
	GlobalCommand.Source.SourceUnitId = -1;
	GlobalCommand.EffectRequest.Source.SourceUnitId = -1;
	const FWBCardActivationCommandResult GlobalResult = WBEffectRunner::ApplyCardActivationCommand(GlobalState, GlobalCommand);
	TestTrue(TEXT("Source-less/global activation succeeds"), GlobalResult.bOk);
	TestEqual(TEXT("Global parent source"), GlobalResult.TraceEvents[0].SourceUnitId, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceFailureTest, "Wandbound.Core.CardActivationCommandScaffolding.SourceFailuresAndMismatchNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData MissingSourceState = MakeActivationState(5, 5);
	FWBCardActivationCommand MissingSourceCommand = MakeActivationCommand({ MakeDamagePayload(2) });
	MissingSourceCommand.Source.SourceUnitId = 99;
	const FWBCardActivationCommandResult MissingSourceResult = WBEffectRunner::ApplyCardActivationCommand(MissingSourceState, MissingSourceCommand);
	TestFalse(TEXT("Missing source fails"), MissingSourceResult.bOk);
	TestEqual(TEXT("Missing source reason"), MissingSourceResult.Reason, FString(TEXT("missing_card_activation_source_unit")));
	TestEqual(TEXT("Missing source no mutation"), MissingSourceState.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Missing source no traces"), MissingSourceResult.TraceEvents.Num(), 0);

	FWBGameStateData RemovedSourceState = MakeActivationState(5, 5);
	RemovedSourceState.GetMutableUnitById(1)->MarkUnitDefeated();
	RemovedSourceState.GetMutableUnitById(1)->RemoveUnitFromBoard();
	const FWBCardActivationCommandResult RemovedSourceResult = WBEffectRunner::ApplyCardActivationCommand(RemovedSourceState, MakeActivationCommand({ MakeDamagePayload(2) }));
	TestFalse(TEXT("Removed source fails"), RemovedSourceResult.bOk);
	TestEqual(TEXT("Removed source reason"), RemovedSourceResult.Reason, FString(TEXT("card_activation_source_unit_removed")));
	TestEqual(TEXT("Removed source no mutation"), RemovedSourceState.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Removed source no traces"), RemovedSourceResult.TraceEvents.Num(), 0);

	FWBGameStateData MismatchState = MakeActivationState(5, 5);
	FWBCardActivationCommand MismatchCommand = MakeActivationCommand({ MakeDamagePayload(2) });
	MismatchCommand.EffectRequest.Source.SourceUnitId = 2;
	const FWBCardActivationCommandResult MismatchResult = WBEffectRunner::ApplyCardActivationCommand(MismatchState, MismatchCommand);
	TestFalse(TEXT("Source mismatch fails"), MismatchResult.bOk);
	TestEqual(TEXT("Source mismatch reason"), MismatchResult.Reason, FString(TEXT("card_activation_effect_source_unit_mismatch")));
	TestEqual(TEXT("Source mismatch no mutation"), MismatchState.GetUnitById(2)->HP, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationEffectFamiliesTest, "Wandbound.Core.CardActivationCommandScaffolding.EffectFamiliesSucceed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationEffectFamiliesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData ArmorState = MakeActivationState(5, 5, 1, 3);
	TestTrue(TEXT("Armor activation ok"), WBEffectRunner::ApplyCardActivationCommand(ArmorState, MakeActivationCommand({ MakeArmorPayload(EWBArmorEffectOp::AddCurrentArmor, 2) })).bOk);
	TestEqual(TEXT("Armor updated"), ArmorState.GetUnitById(2)->GetCurrentArmor(), 3);

	FWBGameStateData StatusState = MakeActivationState(5, 5);
	TestTrue(TEXT("Status activation ok"), WBEffectRunner::ApplyCardActivationCommand(StatusState, MakeActivationCommand({ MakeStatusPayload(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2) })).bOk);
	TestTrue(TEXT("Burn applied"), StatusState.GetUnitById(2)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Burn duration"), StatusState.GetUnitById(2)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);

	FWBGameStateData DamageState = MakeActivationState(5, 5);
	TestTrue(TEXT("Damage activation ok"), WBEffectRunner::ApplyCardActivationCommand(DamageState, MakeActivationCommand({ MakeDamagePayload(2) })).bOk);
	TestEqual(TEXT("Damage applied"), DamageState.GetUnitById(2)->HP, 3);

	FWBGameStateData HealState = MakeActivationState(2, 5);
	TestTrue(TEXT("Heal activation ok"), WBEffectRunner::ApplyCardActivationCommand(HealState, MakeActivationCommand({ MakeHealPayload(2) })).bOk);
	TestEqual(TEXT("Heal applied"), HealState.GetUnitById(2)->HP, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationMixedAtomicityTest, "Wandbound.Core.CardActivationCommandScaffolding.MixedPayloadAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationMixedAtomicityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData SuccessState = MakeActivationState(5, 5, 1, 3);
	const FWBCardActivationCommandResult SuccessResult = WBEffectRunner::ApplyCardActivationCommand(
		SuccessState,
		MakeActivationCommand({
			MakeArmorPayload(EWBArmorEffectOp::AddCurrentArmor, 1),
			MakeStatusPayload(EWBStatusEffectOp::ApplyStatus, FName(TEXT("Burn")), 2),
			MakeDamagePayload(3),
			MakeHealPayload(2)
		}));
	TestTrue(TEXT("Mixed success ok"), SuccessResult.bOk);
	TestEqual(TEXT("Mixed final hp"), SuccessState.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Mixed final armor"), SuccessState.GetUnitById(2)->GetCurrentArmor(), 0);
	TestTrue(TEXT("Mixed status applied"), SuccessState.GetUnitById(2)->HasStatus(FName(TEXT("Burn"))));

	FWBGameStateData FailureState = MakeActivationState(5, 5, 1, 3);
	const FWBCardActivationCommandResult FailureResult = WBEffectRunner::ApplyCardActivationCommand(
		FailureState,
		MakeActivationCommand({
			MakeArmorPayload(EWBArmorEffectOp::AddCurrentArmor, 1),
			MakeHealPayload(2, 99)
		}));
	TestFalse(TEXT("Mixed failure fails"), FailureResult.bOk);
	TestEqual(TEXT("Failure reason"), FailureResult.Reason, FString(TEXT("effect_payload_target_mismatch")));
	TestEqual(TEXT("Failure hp rolled back"), FailureState.GetUnitById(2)->HP, 5);
	TestEqual(TEXT("Failure armor rolled back"), FailureState.GetUnitById(2)->GetCurrentArmor(), 1);
	TestEqual(TEXT("Failure no traces"), FailureResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTraceOrderTest, "Wandbound.Core.CardActivationCommandScaffolding.ParentTraceBeforeEffectTraces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTraceOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationState(5, 5);
	const FWBCardActivationCommandResult Result = WBEffectRunner::ApplyCardActivationCommand(State, MakeActivationCommand({ MakeDamagePayload(2), MakeHealPayload(1) }));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 4);
	if (Result.TraceEvents.Num() == 4)
	{
		TestEqual(TEXT("Activation parent first"), Result.TraceEvents[0].Kind, FName(TEXT("card_activation_resolved")));
		TestEqual(TEXT("Effect request second"), Result.TraceEvents[1].Kind, FName(TEXT("effect_request_resolved")));
		TestEqual(TEXT("Damage third"), Result.TraceEvents[2].Kind, FName(TEXT("damage_effect_resolved")));
		TestEqual(TEXT("Heal fourth"), Result.TraceEvents[3].Kind, FName(TEXT("heal_effect_resolved")));
		TestEqual(TEXT("Parent player"), Result.TraceEvents[0].PlayerId, 0);
		TestEqual(TEXT("Parent source"), Result.TraceEvents[0].SourceUnitId, 1);
		TestEqual(TEXT("Parent target"), Result.TraceEvents[0].TargetUnitId, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationHiddenDataExclusionTest, "Wandbound.Core.CardActivationCommandScaffolding.HiddenDataExcludedFromTraceAndRuntimeSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationHiddenDataExclusionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationState(5, 5);
	State.Players[0].Deck.Add(TEXT("secret_activation_deck_token"));
	State.Players[0].Hand.Add(TEXT("secret_activation_hand_token"));
	State.Players[0].Discard.Add(TEXT("secret_activation_discard_token"));

	FWBCardActivationCommand Command = MakeActivationCommand({ MakeDamagePayload(2), MakeHealPayload(1) });
	Command.Source.SourceCardId = TEXT("secret_activation_source_card_token");
	Command.Source.SourceEffectId = TEXT("secret_activation_source_effect_token");
	Command.DebugActivationId = TEXT("secret_activation_debug_token");

	const FWBCardActivationCommandResult Result = WBEffectRunner::ApplyCardActivationCommand(State, Command);
	TestTrue(TEXT("Activation succeeds"), Result.bOk);

	const FString SerializedTraceEvents = WBReplayTrace::SerializeEvents(Result.TraceEvents);
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("card_activation_command_test"));
	Envelope.SelectedActionId = TEXT("card_activation_command_test");
	Envelope.ApplyResult.bOk = Result.bOk;
	Envelope.ApplyResult.Reason = Result.Reason;
	Envelope.ApplyResult.TraceEvents = Result.TraceEvents;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedRuntimeJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedRuntimeJson));
	TestTrue(TEXT("Public HP serialized"), RuntimeJsonHasPublicUnitHP(*this, SerializedRuntimeJson, 2, 4));

	const TArray<FString> ForbiddenSubstrings = {
		TEXT("secret_activation_source_card_token"),
		TEXT("secret_activation_source_effect_token"),
		TEXT("secret_activation_debug_token"),
		TEXT("secret_activation_deck_token"),
		TEXT("secret_activation_hand_token"),
		TEXT("secret_activation_discard_token"),
		TEXT("\"deck\""),
		TEXT("\"hand\""),
		TEXT("\"discard\""),
		TEXT("pending_attack")
	};
	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Forbidden), SerializedTraceEvents.Contains(Forbidden));
		TestFalse(*FString::Printf(TEXT("Runtime excludes %s"), *Forbidden), SerializedRuntimeJson.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalGenerationAndCodecTest, "Wandbound.Core.CardActivationCommandScaffolding.LegalGenerationAndActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalGenerationAndCodecTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeActivationState(5, 5);
	FWBGameStateData MutatedState = MakeActivationState(5, 5);
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));
	WBEffectRunner::ApplyCardActivationCommand(MutatedState, MakeActivationCommand({ MakeHealPayload(0) }));
	const TArray<FString> MutatedIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MutatedState, 0));
	CompareActionIdArrays(*this, TEXT("legal action ids"), MutatedIds, BaselineIds);

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakePlayerAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakePlayerAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakePlayerAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationFixtureScenariosTest, "Wandbound.Core.CardActivationCommandScaffolding.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_armor_effect_success.json"),
		TEXT("card_activation_status_effect_success.json"),
		TEXT("card_activation_damage_effect_success.json"),
		TEXT("card_activation_heal_effect_success.json"),
		TEXT("card_activation_mixed_payload_atomic_success.json"),
		TEXT("card_activation_mixed_payload_atomic_failure.json"),
		TEXT("card_activation_missing_source_unit_fails.json"),
		TEXT("card_activation_removed_source_unit_fails.json"),
		TEXT("card_activation_missing_target_fails.json"),
		TEXT("card_activation_source_metadata_not_public_trace.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadActivationFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

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
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::ApplyCardActivationCommand);

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
		TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s trace fields"), *FixtureName), ExpectTraceFieldsFromFixture(*this, FixtureName, Fixture, ApplyResult.TraceEvents));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationRuntimeSerializationFixtureTest, "Wandbound.Core.CardActivationCommandScaffolding.RuntimeSerializationFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationRuntimeSerializationFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("card_activation_source_metadata_not_public_trace.json");
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Runtime fixture loads"), LoadActivationFixture(FixtureName, Fixture));
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
	TestTrue(TEXT("Activation fixture applies"), ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
	TestTrue(TEXT("Activation succeeds"), ApplyResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("card_activation_command_test"));
	Envelope.SelectedActionId = TEXT("card_activation_command_test");
	Envelope.ApplyResult = ApplyResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));

	const TSharedPtr<FJsonObject> ExpectedJson = GetObjectField(Fixture, TEXT("expected_json"));
	int32 ExpectedUnitId = -1;
	int32 ExpectedHP = -1;
	TestTrue(TEXT("Expected unit id reads"), TryReadIntegerField(ExpectedJson, TEXT("unit_id"), ExpectedUnitId));
	TestTrue(TEXT("Expected hp reads"), TryReadIntegerField(ExpectedJson, TEXT("hp"), ExpectedHP));
	TestTrue(TEXT("Runtime public HP matches"), RuntimeJsonHasPublicUnitHP(*this, SerializedJson, ExpectedUnitId, ExpectedHP));

	const FString SerializedTraceEvents = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = GetArrayField(ExpectedJson, TEXT("forbidden_substrings"));
	if (ForbiddenSubstrings != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ForbiddenSubstrings)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				continue;
			}

			const FString Forbidden = Value->AsString();
			TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Forbidden), SerializedTraceEvents.Contains(Forbidden));
			TestFalse(*FString::Printf(TEXT("Runtime excludes %s"), *Forbidden), SerializedJson.Contains(Forbidden));
		}
	}

	return true;
}

#endif
