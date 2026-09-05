#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBCharacterConstruction.h"
#include "WBCharacterSummon.h"
#include "WBDeathResolution.h"
#include "WBPostDestructionTrigger.h"
#include "WBProductionMatchReplay.h"
#include "WBReplayTrace.h"
#include "WBSummonDestructionComposition.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr int32 Hero0Id = 1;
constexpr int32 Hero1Id = 2;
constexpr int32 SourceId = 10;

FWBCardDefinition Character(
	const FString& CardId,
	const FString& Faction = TEXT("csn"),
	const int32 HP = 12,
	const int32 ATK = 3,
	const int32 AR = 2,
	const int32 RL = 4)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.PublicFactions.Add(Faction);
	Definition.CharacterStats.HP = HP;
	Definition.CharacterStats.ATK = ATK;
	Definition.CharacterStats.AR = AR;
	Definition.CharacterStats.RL = RL;
	return Definition;
}

FWBCardDefinition Observer()
{
	FWBCardDefinition Definition = Character(
		TEXT("observer"), TEXT("csn"), 10, 1, 3, 2);
	FWBAfterUnitDestroyedTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("observe_csn_destruction");
	Trigger.SourceScope =
		EWBAfterUnitDestroyedSourceScope::ControlledFactionUnitDestroyed;
	Trigger.Operation = EWBPostDestructionEffectOperation::
		ApplyPersistentStatDeltaToTriggerSource;
	Trigger.RequiredFaction = TEXT("csn");
	Trigger.Target = EWBPostDestructionTarget::TriggerSource;
	Trigger.StatDelta.ATKDelta = 1;
	Trigger.StatDelta.MaxHPDelta = 1;
	Trigger.StatDelta.CurrentHPDelta = 1;
	Definition.AfterUnitDestroyedTriggers.Add(Trigger);
	return Definition;
}

FWBCardDefinition Wand()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("wand");
	Definition.PublicName = TEXT("Wand");
	Definition.PublicCategory = TEXT("Wand");
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = 1;
	return Definition;
}

FWBCardDefinitionRepository Repository()
{
	TArray<FWBCardDefinition> Definitions = {
		Character(TEXT("hero0")),
		Character(TEXT("hero1"), TEXT("officer")),
		Character(TEXT("source")),
		Character(TEXT("candidate")),
		Character(TEXT("other")),
		Character(TEXT("overflow"), TEXT("csn"), 12, 3, 2, MAX_int32),
		Observer(),
		Wand()
	};
	FWBCardDefinitionRepository Result;
	const FWBCardDefinitionRepositoryValidationResult Built =
		WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
			TEXT("summon_destruction_composition"),
			TEXT("v1"),
			Definitions,
			Result);
	check(Built.bOk);
	return Result;
}

FWBUnitState Unit(
	const int32 UnitId,
	const int32 Owner,
	const int32 Controller,
	const FString& CardId,
	const FWBTile Tile,
	const int32 HP = 12,
	const int32 CurrentRL = 4)
{
	FWBUnitState Result;
	Result.UnitId = UnitId;
	Result.SetOwnerAndControllerForRules(Owner, Controller);
	Result.CardId = CardId;
	Result.X = Tile.X;
	Result.Y = Tile.Y;
	Result.HP = HP;
	Result.MaxHP = HP;
	Result.ATK = 3;
	Result.AR = 2;
	Result.SetCanonicalRL(4, CurrentRL, 1);
	Result.AttacksLeft = 1;
	Result.MaxAttacksPerTurn = 1;
	return Result;
}

FWBZoneCardEntry ZoneEntry(
	const int32 Owner,
	const FString& InstanceId,
	const FString& CardId,
	const EWBCardZone Zone,
	const int32 Index)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = Owner;
	Entry.Zone = Zone;
	Entry.ZoneIndex = Index;
	return Entry;
}

FWBGameStateData State()
{
	FWBGameStateData Result;
	Result.CurrentPlayer = 0;
	Result.PriorityPlayer = 0;
	Result.TurnNumber = 3;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = Hero0Id;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = Hero1Id;
	Result.Players = { Player0, Player1 };
	Result.AddUnitForTest(Unit(
		Hero0Id, 0, 0, TEXT("hero0"), FWBTile(0, 0), 20));
	Result.AddUnitForTest(Unit(
		Hero1Id, 1, 1, TEXT("hero1"), FWBTile(8, 8), 20));
	Result.AddUnitForTest(Unit(
		SourceId, 0, 0, TEXT("source"), FWBTile(2, 2), 7, 5));
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	Result.GetMutableCardZoneStateForTest().PlayerZones = {
		Zones0, Zones1
	};
	return Result;
}

void AddDeck(
	FWBGameStateData& State,
	const int32 Owner,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), Owner);
	check(Zones != nullptr);
	Zones->Deck.Add(ZoneEntry(
		Owner, InstanceId, CardId, EWBCardZone::Deck, Zones->Deck.Num()));
}

void AddHand(
	FWBGameStateData& State,
	const int32 Owner,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), Owner);
	check(Zones != nullptr);
	Zones->Hand.Add(ZoneEntry(
		Owner, InstanceId, CardId, EWBCardZone::Hand, Zones->Hand.Num()));
}

void AddWand(FWBGameStateData& State, const int32 UnitId = SourceId)
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = TEXT("wand_instance");
	Entry.Card.CardId = TEXT("wand");
	Entry.Card.OwnerPlayerId = 0;
	Entry.EquippedToUnitId = UnitId;
	Entry.SlotId = TEXT("primary");
	Entry.EquipOrder = 0;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Entry);
}

int32 CountTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.FilterByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	}).Num();
}

FWBCharacterSummonRequest ExactSummon(
	const EWBCardZone Zone,
	const FString& InstanceId,
	const FString& CardId,
	const FWBTile Tile)
{
	FWBCharacterSummonRequest Request;
	Request.OwnerPlayerId = 0;
	Request.ControllerPlayerId = 0;
	Request.SourceZone = Zone;
	Request.CardInstanceId = InstanceId;
	Request.ExpectedCardId = CardId;
	Request.RequiredFaction = TEXT("csn");
	Request.TargetTile = Tile;
	Request.ConditionPolicy =
		EWBCharacterSummonConditionPolicy::IgnoreSummoningConditions;
	Request.TraceKind = FName(TEXT("effect_summon_completed"));
	Request.TransactionId = TEXT("fixture:composition");
	Request.SourceUnitId = SourceId;
	return Request;
}

FWBSummonDestructionCompositionRequest Composition(
	const FString& InstanceId = TEXT("replacement_instance"),
	const FString& CardId = TEXT("candidate"))
{
	FWBSummonDestructionCompositionRequest Request;
	Request.DestructionTargetUnitId = SourceId;
	Request.DestructionCause = EWBUnitDestructionCause::ReplacementEffect;
	Request.Summon = ExactSummon(
		EWBCardZone::Hand,
		InstanceId,
		CardId,
		FWBTile(2, 2));
	Request.Summon.TraceKind = FName(TEXT("effect_replacement_summon"));
	Request.TransactionId = TEXT("fixture:composition");
	return Request;
}

void SetPendingAttack(FWBGameStateData& State)
{
	FWBUnitState* Attacker = State.GetMutableUnitById(Hero1Id);
	check(Attacker != nullptr);
	Attacker->X = 2;
	Attacker->Y = 4;

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.Stage = EWBAttackContinuationStage::PreHit;
	Pending.AttackerUnitId = Hero1Id;
	Pending.DefenderUnitId = SourceId;
	Pending.OriginalAttackerUnitId = Hero1Id;
	Pending.OriginalDefenderUnitId = SourceId;
	Pending.AttackingPlayerId = 1;
	Pending.AttackerTile = FWBTile(2, 4);
	Pending.DefenderTile = FWBTile(2, 2);
	Pending.DeclarationActionId = TEXT("attack:p1:u2:t10");
	Pending.ContinuationId = TEXT("fixture:attack");
	State.SetPendingAttackForTest(Pending);
	State.ReactionWindow.Kind = EWBReactionWindowKind::PreHit;
	State.ReactionWindow.SourceUnitId = Hero1Id;
	State.ReactionWindow.TargetUnitId = SourceId;
}

bool ContainsInstance(
	const TArray<FWBZoneCardEntry>& Entries,
	const FString& InstanceId)
{
	return Entries.ContainsByPredicate([&InstanceId](
		const FWBZoneCardEntry& Entry)
	{
		return Entry.Card.InstanceId == InstanceId;
	});
}
}

#define WB_COMPOSITION_TEST(ClassName, Suffix) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, \
		"Wandbound.SummonDestructionComposition." Suffix, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_COMPOSITION_TEST(FWBCharacterConstructionFoundationTest,
	"CharacterConstruction.CanonicalFieldsAndAllocation")
bool FWBCharacterConstructionFoundationTest::RunTest(const FString&)
{
	const FWBCardDefinition Definition = Character(TEXT("candidate"));
	FWBCharacterConstructionRequest Request;
	Request.UnitId = 44;
	Request.OwnerPlayerId = 1;
	Request.ControllerPlayerId = 0;
	Request.CardId = TEXT("candidate");
	Request.Tile = FWBTile(3, 4);
	const FWBCharacterConstructionResult Built =
		WBCharacterConstruction::BuildUnit(Definition, Request);
	TestTrue(TEXT("1 construction succeeds"), Built.bOk);
	TestEqual(TEXT("2 UnitId"), Built.Unit.UnitId, 44);
	TestEqual(TEXT("3 Owner"), Built.Unit.GetOwnerPlayerIdForRules(), 1);
	TestEqual(TEXT("4 Controller"), Built.Unit.GetControllerPlayerIdForRules(), 0);
	TestEqual(TEXT("5 CardId"), Built.Unit.CardId, FString(TEXT("candidate")));
	TestEqual(TEXT("6 tile X"), Built.Unit.X, 3);
	TestEqual(TEXT("7 tile Y"), Built.Unit.Y, 4);
	TestEqual(TEXT("8 HP"), Built.Unit.HP, 12);
	TestEqual(TEXT("9 MaxHP"), Built.Unit.MaxHP, 12);
	TestEqual(TEXT("10 ATK"), Built.Unit.ATK, 3);
	TestEqual(TEXT("11 AR"), Built.Unit.AR, 2);
	TestEqual(TEXT("12 BaseRL"), Built.Unit.GetBaseRLForRules(), 4);
	TestEqual(TEXT("13 CurrentRL"), Built.Unit.GetCurrentRLForRules(), 4);
	TestEqual(TEXT("14 RL mirror"), Built.Unit.RLTotal, 4);
	TestEqual(TEXT("15 RLUsed"), Built.Unit.RLUsed, 0);
	TestEqual(TEXT("16 attacks"), Built.Unit.AttacksLeft, 0);
	TestEqual(TEXT("17 max attacks"), Built.Unit.MaxAttacksPerTurn, 1);
	TestEqual(TEXT("18 MP"), Built.Unit.MPRemaining, 0);

	FWBGameStateData Allocation = State();
	TestEqual(TEXT("19 deterministic next ID"),
		WBCharacterConstruction::AllocateNextUnitId(Allocation), 11);
	Allocation.AddUnitForTest(Unit(
		MAX_int32, 0, 0, TEXT("source"), FWBTile(7, 7)));
	TestEqual(TEXT("20 overflow fails closed"),
		WBCharacterConstruction::AllocateNextUnitId(Allocation), INDEX_NONE);
	FWBCardDefinition Invalid = Definition;
	Invalid.CharacterStats.HP = 0;
	TestFalse(TEXT("21 invalid stats rejected"),
		WBCharacterConstruction::BuildUnit(Invalid, Request).bOk);
	FWBCardDefinition NonCharacter = Wand();
	Request.CardId = NonCharacter.CardId;
	TestFalse(TEXT("22 non-Character rejected"),
		WBCharacterConstruction::BuildUnit(NonCharacter, Request).bOk);
	TestEqual(TEXT("23 immutable definition HP"),
		Definition.CharacterStats.HP, 12);
	TestEqual(TEXT("24 immutable definition RL"),
		Definition.CharacterStats.RL, 4);
	return true;
}

WB_COMPOSITION_TEST(FWBExactCharacterSummonFoundationTest,
	"ExactSummon.DeckHandOrderConditionsAndOwnership")
bool FWBExactCharacterSummonFoundationTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repo = Repository();
	FWBGameStateData DeckState = State();
	AddDeck(DeckState, 0, TEXT("copy_a"), TEXT("candidate"));
	AddDeck(DeckState, 0, TEXT("middle"), TEXT("other"));
	AddDeck(DeckState, 0, TEXT("copy_b"), TEXT("candidate"));
	const FWBCharacterSummonResult DeckSummon =
		WBCharacterSummon::SummonExactCharacter(
			DeckState,
			Repo,
			ExactSummon(
				EWBCardZone::Deck,
				TEXT("copy_b"),
				TEXT("candidate"),
				FWBTile(4, 4)));
	TestTrue(TEXT("25 exact Deck summon succeeds"), DeckSummon.bOk);
	TestEqual(TEXT("26 exact CardId"), DeckSummon.CardId,
		FString(TEXT("candidate")));
	TestEqual(TEXT("27 exact instance result"), DeckSummon.CardInstanceId,
		FString(TEXT("copy_b")));
	TestEqual(TEXT("28 deterministic ID"), DeckSummon.CreatedUnitId, 11);
	const FWBPlayerCardZoneState* DeckZones =
		WBCardZoneState::FindPlayerZones(DeckState.GetCardZoneState(), 0);
	TestNotNull(TEXT("29 Deck zones"), DeckZones);
	if (DeckZones == nullptr) return false;
	TestEqual(TEXT("30 Deck count"), DeckZones->Deck.Num(), 2);
	TestEqual(TEXT("31 first duplicate remains"),
		DeckZones->Deck[0].Card.InstanceId, FString(TEXT("copy_a")));
	TestEqual(TEXT("32 relative middle order remains"),
		DeckZones->Deck[1].Card.InstanceId, FString(TEXT("middle")));
	TestFalse(TEXT("33 selected copy removed"),
		ContainsInstance(DeckZones->Deck, TEXT("copy_b")));
	TestTrue(TEXT("34 no shuffle"), DeckZones->Deck[0].ZoneIndex == 0
		&& DeckZones->Deck[1].ZoneIndex == 1);
	TestEqual(TEXT("35 one summon trace"),
		CountTrace(DeckSummon.TraceEvents,
			FName(TEXT("effect_summon_completed"))), 1);
	TestTrue(TEXT("36 generic trace hides exact instance"),
		DeckSummon.TraceEvents[0].CardInstanceId.IsEmpty());

	FWBGameStateData HandState = State();
	AddHand(HandState, 1, TEXT("controlled_copy"), TEXT("candidate"));
	FWBCharacterSummonRequest HandRequest = ExactSummon(
		EWBCardZone::Hand,
		TEXT("controlled_copy"),
		TEXT("candidate"),
		FWBTile(5, 5));
	HandRequest.OwnerPlayerId = 1;
	HandRequest.ControllerPlayerId = 0;
	const FWBCharacterSummonResult HandSummon =
		WBCharacterSummon::SummonExactCharacter(
			HandState, Repo, HandRequest);
	TestTrue(TEXT("37 exact Hand summon succeeds"), HandSummon.bOk);
	const FWBUnitState* Controlled =
		HandState.GetUnitById(HandSummon.CreatedUnitId);
	TestNotNull(TEXT("38 controlled unit exists"), Controlled);
	TestEqual(TEXT("39 explicit Owner"),
		Controlled != nullptr ? Controlled->GetOwnerPlayerIdForRules() : -1, 1);
	TestEqual(TEXT("40 explicit Controller"),
		Controlled != nullptr
			? Controlled->GetControllerPlayerIdForRules() : -1, 0);
	const FWBPlayerCardZoneState* HandZones =
		WBCardZoneState::FindPlayerZones(HandState.GetCardZoneState(), 1);
	TestTrue(TEXT("41 exact Hand instance consumed"), HandZones != nullptr
		&& !ContainsInstance(HandZones->Hand, TEXT("controlled_copy")));
	FString ZoneReason;
	TestTrue(TEXT("42 final zone state valid"),
		WBCardZoneState::ValidateZoneStateForTest(
			HandState.GetCardZoneState(), ZoneReason));

	FWBGameStateData NormalState = State();
	AddHand(NormalState, 0, TEXT("normal_copy"), TEXT("candidate"));
	FWBCharacterSummonRequest Normal = ExactSummon(
		EWBCardZone::Hand,
		TEXT("normal_copy"),
		TEXT("candidate"),
		FWBTile(1, 0));
	Normal.ConditionPolicy = EWBCharacterSummonConditionPolicy::Normal;
	TestTrue(TEXT("43 normal adjacent summon succeeds"),
		WBCharacterSummon::SummonExactCharacter(
			NormalState, Repo, Normal).bOk);
	return true;
}

WB_COMPOSITION_TEST(FWBExactCharacterSummonFailureTest,
	"ExactSummon.FailClosedAndAtomic")
bool FWBExactCharacterSummonFailureTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repo = Repository();
	auto ExpectFailure = [this, &Repo](
		const TCHAR* Label,
		FWBGameStateData State,
		const FWBCharacterSummonRequest& Request,
		const FString& ExpectedReason)
	{
		const FString Before =
			WBProductionMatchReplay::BuildGameStateDigest(State);
		const FWBCharacterSummonResult Result =
			WBCharacterSummon::SummonExactCharacter(State, Repo, Request);
		TestFalse(Label, Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s reason"), Label),
			Result.Reason, ExpectedReason);
		TestEqual(*FString::Printf(TEXT("%s atomic"), Label),
			WBProductionMatchReplay::BuildGameStateDigest(State), Before);
	};
	FWBGameStateData Missing = State();
	ExpectFailure(TEXT("44 missing exact Deck instance"), Missing,
		ExactSummon(EWBCardZone::Deck, TEXT("missing"),
			TEXT("candidate"), FWBTile(4, 4)),
		TEXT("source_card_missing"));
	FWBGameStateData WrongZone = State();
	AddHand(WrongZone, 0, TEXT("wrong_zone"), TEXT("candidate"));
	ExpectFailure(TEXT("47 wrong source zone"), WrongZone,
		ExactSummon(EWBCardZone::Deck, TEXT("wrong_zone"),
			TEXT("candidate"), FWBTile(4, 4)),
		TEXT("source_card_not_in_deck"));
	FWBGameStateData WrongId = State();
	AddDeck(WrongId, 0, TEXT("wrong_id"), TEXT("other"));
	ExpectFailure(TEXT("50 wrong CardId"), WrongId,
		ExactSummon(EWBCardZone::Deck, TEXT("wrong_id"),
			TEXT("candidate"), FWBTile(4, 4)),
		TEXT("source_card_id_mismatch"));
	FWBGameStateData Bounds = State();
	AddDeck(Bounds, 0, TEXT("bounds"), TEXT("candidate"));
	ExpectFailure(TEXT("53 bounds"), Bounds,
		ExactSummon(EWBCardZone::Deck, TEXT("bounds"),
			TEXT("candidate"), FWBTile(9, 0)),
		TEXT("target_tile_out_of_bounds"));
	FWBGameStateData Occupied = State();
	AddDeck(Occupied, 0, TEXT("occupied"), TEXT("candidate"));
	ExpectFailure(TEXT("56 occupied"), Occupied,
		ExactSummon(EWBCardZone::Deck, TEXT("occupied"),
			TEXT("candidate"), FWBTile(2, 2)),
		TEXT("target_tile_occupied"));
	FWBGameStateData Cap = State();
	Cap.AddUnitForTest(Unit(
		20, 0, 0, TEXT("other"), FWBTile(3, 3)));
	Cap.AddUnitForTest(Unit(
		21, 0, 0, TEXT("other"), FWBTile(4, 3)));
	AddDeck(Cap, 0, TEXT("cap"), TEXT("candidate"));
	ExpectFailure(TEXT("59 cap"), Cap,
		ExactSummon(EWBCardZone::Deck, TEXT("cap"),
			TEXT("candidate"), FWBTile(5, 5)),
		TEXT("unit_cap_reached"));
	return true;
}

WB_COMPOSITION_TEST(FWBGenuineDestructionFoundationTest,
	"GenuineDestruction.PositiveHPNoDamageSnapshotAndCleanup")
bool FWBGenuineDestructionFoundationTest::RunTest(const FString&)
{
	FWBGameStateData TestState = State();
	AddWand(TestState);
	SetPendingAttack(TestState);
	FWBUnitDestructionRequest Request;
	Request.TargetUnitId = SourceId;
	Request.Cause = EWBUnitDestructionCause::ExplicitDestroy;
	Request.TerminalSource = EWBTerminalSource::Effect;
	const FWBUnitDestructionResult Destroyed =
		WBDeathResolution::ApplyGenuineUnitDestruction(TestState, Request);
	TestTrue(TEXT("62 positive HP destruction succeeds"), Destroyed.bOk);
	TestTrue(TEXT("63 genuine destruction committed"), Destroyed.bDestroyed);
	TestFalse(TEXT("64 not prevented"), Destroyed.bPrevented);
	const FWBUnitState* Removed = TestState.GetUnitById(SourceId);
	TestNotNull(TEXT("65 historical unit retained"), Removed);
	TestEqual(TEXT("66 internal HP zero"),
		Removed != nullptr ? Removed->HP : -1, 0);
	TestTrue(TEXT("67 defeated"), Removed != nullptr && Removed->bDefeated);
	TestTrue(TEXT("68 removed"), Removed != nullptr
		&& Removed->bRemovedFromBoard);
	TestEqual(TEXT("69 Owner snapshot"),
		Destroyed.Snapshot.OwnerPlayerId, 0);
	TestEqual(TEXT("70 Controller snapshot"),
		Destroyed.Snapshot.ControllerPlayerId, 0);
	TestEqual(TEXT("71 former tile"),
		Destroyed.Snapshot.LastTile, FWBTile(2, 2));
	TestEqual(TEXT("72 BaseRL snapshot"),
		Destroyed.Snapshot.BaseRLSnapshot, 4);
	TestEqual(TEXT("73 CurrentRL snapshot"),
		Destroyed.Snapshot.CurrentRLSnapshot, 5);
	TestEqual(TEXT("74 RLUsed snapshot"),
		Destroyed.Snapshot.RLUsedSnapshot, 1);
	TestEqual(TEXT("75 Wand snapshot count"),
		Destroyed.Snapshot.EquippedWands.Num(), 1);
	TestEqual(TEXT("76 exactly one queued event"),
		TestState.PendingUnitDestructionEvents.Num(), 1);
	TestEqual(TEXT("77 exact event identity"),
		TestState.PendingUnitDestructionEvents[0].EventId,
		Destroyed.Snapshot.EventId);
	TestEqual(TEXT("78 one defeated trace"),
		CountTrace(Destroyed.TraceEvents, FName(TEXT("unit_defeated"))), 1);
	TestEqual(TEXT("79 one removed trace"),
		CountTrace(Destroyed.TraceEvents,
			FName(TEXT("unit_removed_from_board"))), 1);
	TestEqual(TEXT("80 one equipment cleanup trace"),
		CountTrace(Destroyed.TraceEvents,
			FName(TEXT("equipped_card_discarded_on_death"))), 1);
	TestEqual(TEXT("81 no fake damage trace"),
		CountTrace(Destroyed.TraceEvents,
			FName(TEXT("damage_effect_resolved"))), 0);
	TestEqual(TEXT("82 no fake attack damage trace"),
		CountTrace(Destroyed.TraceEvents,
			FName(TEXT("attack_damage_applied"))), 0);
	TestFalse(TEXT("83 pending attack cleared"), TestState.HasPendingAttack());
	const FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindPlayerZones(TestState.GetCardZoneState(), 0);
	TestTrue(TEXT("84 equipped Wand discarded"), Zones != nullptr
		&& ContainsInstance(Zones->Discard, TEXT("wand_instance")));
	FWBDeathResolutionCandidate Candidate;
	Candidate.UnitId = SourceId;
	Candidate.OwnerId = 0;
	TestFalse(TEXT("85 current prevention hook allows"),
		WBDeathResolution::EvaluateDeathPrevention(State(), Candidate).bPrevented);
	return true;
}

WB_COMPOSITION_TEST(FWBDestructionObserverSacrificeTest,
	"DestructionVsSacrifice.ObserverBoundary")
bool FWBDestructionObserverSacrificeTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repo = Repository();
	FWBGameStateData DestroyedState = State();
	DestroyedState.AddUnitForTest(Unit(
		30, 0, 0, TEXT("observer"), FWBTile(3, 2), 10));
	FWBUnitDestructionRequest Request;
	Request.TargetUnitId = SourceId;
	Request.Cause = EWBUnitDestructionCause::ExplicitDestroy;
	const FWBUnitDestructionResult Destroyed =
		WBDeathResolution::ApplyGenuineUnitDestruction(
			DestroyedState, Request);
	TestTrue(TEXT("86 direct destruction succeeds"), Destroyed.bOk);
	TestTrue(TEXT("87 observer captured"),
		Destroyed.Snapshot.ObserverSources.ContainsByPredicate(
			[](const FWBPostDestructionObserverSourceSnapshot& Source)
			{
				return Source.SourceUnitId == 30;
			}));
	const FWBPostDestructionTriggerResult Triggered =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			DestroyedState, Repo, 0, 0);
	TestTrue(TEXT("88 observer resolution succeeds"), Triggered.bOk);
	TestEqual(TEXT("89 Sable-style ATK grows"),
		DestroyedState.GetUnitById(30)->ATK, 4);
	TestEqual(TEXT("90 Sable-style HP grows"),
		DestroyedState.GetUnitById(30)->HP, 11);
	TestEqual(TEXT("91 Sable-style MaxHP grows"),
		DestroyedState.GetUnitById(30)->MaxHP, 11);
	TestEqual(TEXT("92 exactly one observer mutation"),
		CountTrace(Triggered.TraceEvents,
			FName(TEXT("unit_stat_delta_applied"))), 1);

	FWBGameStateData SacrificedState = State();
	SacrificedState.AddUnitForTest(Unit(
		30, 0, 0, TEXT("observer"), FWBTile(3, 2), 10));
	FWBUnitState* Sacrificed =
		SacrificedState.GetMutableUnitById(SourceId);
	Sacrificed->RemoveUnitFromBoard();
	TestEqual(TEXT("93 sacrifice queues no destruction"),
		SacrificedState.PendingUnitDestructionEvents.Num(), 0);
	TestEqual(TEXT("94 sacrifice leaves observer ATK"),
		SacrificedState.GetUnitById(30)->ATK, 3);
	TestEqual(TEXT("95 sacrifice leaves observer HP"),
		SacrificedState.GetUnitById(30)->HP, 10);
	TestEqual(TEXT("96 sacrifice leaves observer MaxHP"),
		SacrificedState.GetUnitById(30)->MaxHP, 10);
	return true;
}

WB_COMPOSITION_TEST(FWBDestroySummonCompositionTest,
	"Composition.FinalStateAtomicityAndExactInstance")
bool FWBDestroySummonCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repo = Repository();
	FWBGameStateData TestState = State();
	TestState.AddUnitForTest(Unit(
		20, 0, 0, TEXT("other"), FWBTile(3, 3)));
	TestState.AddUnitForTest(Unit(
		21, 0, 0, TEXT("other"), FWBTile(4, 3)));
	AddHand(TestState, 0, TEXT("other_copy"), TEXT("candidate"));
	AddHand(TestState, 0, TEXT("replacement_instance"), TEXT("candidate"));
	AddWand(TestState);
	TestEqual(TEXT("97 starts at cap"),
		TestState.GetUnitsControlledByPlayer(0).Num(), 4);
	const FWBSummonDestructionCompositionResult Applied =
		WBSummonDestructionComposition::Apply(
			TestState, Repo, Composition());
	TestTrue(TEXT("98 composition succeeds"), Applied.bOk);
	TestEqual(TEXT("99 exact destroyed unit"),
		Applied.DestroyedUnitId, SourceId);
	TestEqual(TEXT("100 deterministic created unit"),
		Applied.CreatedUnitId, 22);
	TestEqual(TEXT("101 final controlled count"),
		TestState.GetUnitsControlledByPlayer(0).Num(), 4);
	const FWBUnitState* Replacement =
		TestState.GetUnitById(Applied.CreatedUnitId);
	TestNotNull(TEXT("102 replacement exists"), Replacement);
	TestEqual(TEXT("103 replacement tile"),
		Replacement != nullptr
			? FWBTile(Replacement->X, Replacement->Y)
			: FWBTile(),
		FWBTile(2, 2));
	TestEqual(TEXT("104 replacement CardId"),
		Replacement != nullptr ? Replacement->CardId : FString(),
		FString(TEXT("candidate")));
	const FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindPlayerZones(TestState.GetCardZoneState(), 0);
	TestTrue(TEXT("105 selected exact instance consumed"), Zones != nullptr
		&& !ContainsInstance(Zones->Hand, TEXT("replacement_instance")));
	TestTrue(TEXT("106 duplicate copy remains"), Zones != nullptr
		&& ContainsInstance(Zones->Hand, TEXT("other_copy")));
	TestTrue(TEXT("107 normal destruction discarded equipment"),
		Zones != nullptr
		&& ContainsInstance(Zones->Discard, TEXT("wand_instance")));
	TestEqual(TEXT("108 one destruction event"),
		TestState.PendingUnitDestructionEvents.Num(), 1);
	TestEqual(TEXT("109 one defeated trace"),
		CountTrace(Applied.TraceEvents, FName(TEXT("unit_defeated"))), 1);
	TestEqual(TEXT("110 one removed trace"),
		CountTrace(Applied.TraceEvents,
			FName(TEXT("unit_removed_from_board"))), 1);
	TestEqual(TEXT("111 one summon trace"),
		CountTrace(Applied.TraceEvents,
			FName(TEXT("effect_replacement_summon"))), 1);
	FString ZoneReason;
	TestTrue(TEXT("112 final zones valid"),
		WBCardZoneState::ValidateZoneStateForTest(
			TestState.GetCardZoneState(), ZoneReason));

	FWBGameStateData Missing = State();
	const FString MissingBefore =
		WBProductionMatchReplay::BuildGameStateDigest(Missing);
	TestFalse(TEXT("113 failed summon validation fails composition"),
		WBSummonDestructionComposition::Apply(
			Missing, Repo, Composition()).bOk);
	TestEqual(TEXT("114 failed summon leaves source"),
		WBProductionMatchReplay::BuildGameStateDigest(Missing),
		MissingBefore);

	FWBGameStateData Downstream = State();
	AddHand(Downstream, 0, TEXT("overflow_instance"), TEXT("overflow"));
	const FString DownstreamBefore =
		WBProductionMatchReplay::BuildGameStateDigest(Downstream);
	FWBSummonDestructionCompositionRequest Overflow =
		Composition(TEXT("overflow_instance"), TEXT("overflow"));
	Overflow.InheritancePolicy =
		EWBDestructionSummonInheritancePolicy::ApplyCSNInheritance;
	TestFalse(TEXT("115 downstream overflow fails"),
		WBSummonDestructionComposition::Apply(
			Downstream, Repo, Overflow).bOk);
	TestEqual(TEXT("116 downstream failure rolls back"),
		WBProductionMatchReplay::BuildGameStateDigest(Downstream),
		DownstreamBefore);
	return true;
}

WB_COMPOSITION_TEST(FWBReplacementContinuationCompositionTest,
	"Composition.PendingAttackInheritanceAndHeroTerminal")
bool FWBReplacementContinuationCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repo = Repository();
	FWBGameStateData AttackState = State();
	AddHand(AttackState, 0, TEXT("replacement_instance"), TEXT("candidate"));
	AddWand(AttackState);
	SetPendingAttack(AttackState);
	FWBSummonDestructionCompositionRequest Attack = Composition();
	Attack.InheritancePolicy =
		EWBDestructionSummonInheritancePolicy::ApplyCSNInheritance;
	Attack.PendingAttackPolicy =
		EWBDestructionSummonPendingAttackPolicy::PreserveAndRedirect;
	Attack.PendingAttackContinuationId = TEXT("fixture:attack");
	Attack.Summon.bIncludeSelectedInstanceInTrace = true;
	const FWBSummonDestructionCompositionResult Replaced =
		WBSummonDestructionComposition::Apply(
			AttackState, Repo, Attack);
	TestTrue(TEXT("117 attack replacement succeeds"), Replaced.bOk);
	TestTrue(TEXT("118 pending attack preserved"), AttackState.HasPendingAttack());
	TestEqual(TEXT("119 defender redirected"),
		AttackState.PendingAttack.DefenderUnitId, Replaced.CreatedUnitId);
	TestEqual(TEXT("120 same continuation"),
		AttackState.PendingAttack.ContinuationId,
		FString(TEXT("fixture:attack")));
	TestEqual(TEXT("121 stage remains PreHit"),
		AttackState.PendingAttack.Stage,
		EWBAttackContinuationStage::PreHit);
	TestEqual(TEXT("122 no new declaration"),
		CountTrace(Replaced.TraceEvents, FName(TEXT("attack_declared"))), 0);
	TestEqual(TEXT("123 exactly one redirect"),
		CountTrace(Replaced.TraceEvents,
			FName(TEXT("pending_attack_redirected"))), 1);
	TestEqual(TEXT("124 exactly one destruction event"),
		AttackState.PendingUnitDestructionEvents.Num(), 1);
	const FWBEquippedCardEntry* WandEntry =
		AttackState.GetCardZoneState().EquippedCards.FindByPredicate(
			[](const FWBEquippedCardEntry& Entry)
			{
				return Entry.Card.InstanceId == TEXT("wand_instance");
			});
	TestNotNull(TEXT("125 inherited Wand exists"), WandEntry);
	TestEqual(TEXT("126 Wand follows replacement"),
		WandEntry != nullptr ? WandEntry->EquippedToUnitId : -1,
		Replaced.CreatedUnitId);
	const FWBUnitState* AttackReplacement =
		AttackState.GetUnitById(Replaced.CreatedUnitId);
	TestEqual(TEXT("127 inherited BaseRL"),
		AttackReplacement != nullptr
			? AttackReplacement->GetBaseRLForRules() : -1,
		9);
	TestEqual(TEXT("128 inheritance trace once"),
		CountTrace(Replaced.TraceEvents, FName(TEXT("csn_inheritance"))), 1);

	FWBGameStateData HeroState = State();
	HeroState.GetMutablePlayerById(0)->HeroUnitId = SourceId;
	AddHand(HeroState, 0, TEXT("replacement_instance"), TEXT("candidate"));
	FWBSummonDestructionCompositionRequest Hero = Composition();
	Hero.HeroPolicy =
		EWBDestructionSummonHeroPolicy::SummonThenCommitTerminal;
	const FWBSummonDestructionCompositionResult HeroResult =
		WBSummonDestructionComposition::Apply(HeroState, Repo, Hero);
	TestTrue(TEXT("129 Hero composition succeeds"), HeroResult.bOk);
	TestTrue(TEXT("130 Hero destruction terminal"), HeroState.bGameOver);
	TestEqual(TEXT("131 terminal loser"), HeroState.TerminalOutcome.LoserPlayerId, 0);
	TestEqual(TEXT("132 Hero identity remains destroyed source"),
		HeroState.GetPlayerById(0)->HeroUnitId, SourceId);
	TestTrue(TEXT("133 old Hero removed"),
		HeroState.GetUnitById(SourceId)->bRemovedFromBoard);
	TestTrue(TEXT("134 replacement enters before terminal"),
		HeroState.GetUnitById(HeroResult.CreatedUnitId)->IsUnitOnBoard());
	TestTrue(TEXT("135 replacement not promoted"),
		HeroResult.CreatedUnitId != HeroState.GetPlayerById(0)->HeroUnitId);
	TestEqual(TEXT("136 one Hero trace"),
		CountTrace(HeroResult.TraceEvents, FName(TEXT("hero_defeated"))), 1);
	return true;
}

WB_COMPOSITION_TEST(FWBCompositionDeterminismPrivacyTest,
	"Replay.DeterminismPrivacySchemaAndReceipt")
bool FWBCompositionDeterminismPrivacyTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repo = Repository();
	FWBGameStateData First = State();
	FWBGameStateData Second = State();
	AddHand(First, 0, TEXT("replacement_instance"), TEXT("candidate"));
	AddHand(Second, 0, TEXT("replacement_instance"), TEXT("candidate"));
	const FWBSummonDestructionCompositionResult A =
		WBSummonDestructionComposition::Apply(First, Repo, Composition());
	const FWBSummonDestructionCompositionResult B =
		WBSummonDestructionComposition::Apply(Second, Repo, Composition());
	TestTrue(TEXT("137 first deterministic run"), A.bOk);
	TestTrue(TEXT("138 second deterministic run"), B.bOk);
	TestEqual(TEXT("139 deterministic unit ID"),
		A.CreatedUnitId, B.CreatedUnitId);
	TestEqual(TEXT("140 deterministic destruction EventId"),
		A.DestructionSnapshot.EventId, B.DestructionSnapshot.EventId);
	TestEqual(TEXT("141 deterministic state digest"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("142 deterministic trace digest"),
		WBProductionMatchReplay::BuildTraceDigest(A.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(B.TraceEvents));
	TestEqual(TEXT("143 deterministic serialized trace"),
		WBReplayTrace::SerializeEvents(A.TraceEvents),
		WBReplayTrace::SerializeEvents(B.TraceEvents));
	const FString TraceJson = WBReplayTrace::SerializeEvents(A.TraceEvents);
	TestFalse(TEXT("144 no hidden candidate alternatives"),
		TraceJson.Contains(TEXT("alternate_instance")));
	TestFalse(TEXT("145 selected exact instance hidden by default"),
		TraceJson.Contains(TEXT("replacement_instance")));
	TestEqual(TEXT("146 replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);

	FWBProductionMatchReplayReceipt Receipt;
	const FString ReceiptJson = WBProductionMatchReplay::SerializeReceipt(Receipt);
	TSharedPtr<FJsonObject> ReceiptObject;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(ReceiptJson);
	TestTrue(TEXT("147 receipt parses"),
		FJsonSerializer::Deserialize(Reader, ReceiptObject)
		&& ReceiptObject.IsValid());
	TestEqual(TEXT("148 receipt remains eight fields"),
		ReceiptObject.IsValid() ? ReceiptObject->Values.Num() : -1, 8);

	FWBGameStateData OverflowState = State();
	AddHand(OverflowState, 0, TEXT("replacement_instance"), TEXT("candidate"));
	OverflowState.AddUnitForTest(Unit(
		MAX_int32, 1, 1, TEXT("other"), FWBTile(7, 7)));
	const FString OverflowBefore =
		WBProductionMatchReplay::BuildGameStateDigest(OverflowState);
	TestFalse(TEXT("149 allocator overflow fails composition"),
		WBSummonDestructionComposition::Apply(
			OverflowState, Repo, Composition()).bOk);
	TestEqual(TEXT("150 allocator overflow is atomic"),
		WBProductionMatchReplay::BuildGameStateDigest(OverflowState),
		OverflowBefore);
	return true;
}

#undef WB_COMPOSITION_TEST
#endif
