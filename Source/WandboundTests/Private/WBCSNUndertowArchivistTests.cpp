#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "WBCardDefinitionFixtureLoader.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneObservation.h"
#include "WBCSNInheritanceTrigger.h"
#include "WBEffectRunner.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNCrashInSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBReplayTrace.h"
#include "WBSummonExecution.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 UndertowAttackerId = 10;
constexpr int32 UndertowHeroId = 20;
constexpr int32 UndertowSourceId = 30;

FWBCardDefinition MakeUndertowCharacter(
	const FString& CardId,
	const FString& Faction,
	const int32 HP,
	const int32 ATK,
	const int32 AR,
	const int32 RL,
	const bool bInheritanceDraw = false)
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
	if (bInheritanceDraw)
	{
		FWBAfterCSNInheritanceTriggerDefinition Trigger;
		Trigger.TriggerId = TEXT("draw_after_csn_inheritance");
		Trigger.DrawCount = 1;
		Trigger.bMandatory = true;
		Definition.AfterCSNInheritanceTriggers.Add(MoveTemp(Trigger));
	}
	return Definition;
}

FWBCardDefinitionRepository MakeUndertowRepository(
	const FString& ReplacementCardId = TEXT("fixture_inheritance_draw_character"),
	const bool bReplacementDraws = true)
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeUndertowCharacter(
		TEXT("undertow_attacker"), TEXT("wandwright"), 14, 4, 8, 4));
	Definitions.Add(MakeUndertowCharacter(
		TEXT("undertow_hero"), TEXT("csn"), 14, 1, 2, 4));
	Definitions.Add(MakeUndertowCharacter(
		TEXT("undertow_source"), TEXT("csn"), 16, 3, 2, 3));
	Definitions.Add(MakeUndertowCharacter(
		ReplacementCardId, TEXT("csn"), 11, 2, 3, 2,
		bReplacementDraws));

	for (int32 Index = 0; Index < 2; ++Index)
	{
		FWBCardDefinition Wand;
		Wand.CardId = FString::Printf(TEXT("undertow_wand_%d"), Index);
		Wand.PublicName = Wand.CardId;
		Wand.PublicCategory = TEXT("Wand");
		Wand.Kind = EWBCardDefinitionKind::Wand;
		Wand.WandStats.RR = 1;
		Definitions.Add(MoveTemp(Wand));
	}

	FWBCardDefinition PrivateDraw;
	PrivateDraw.CardId = TEXT("undertow_private_draw");
	PrivateDraw.PublicName = TEXT("Private Draw");
	PrivateDraw.PublicCategory = TEXT("Action");
	PrivateDraw.Kind = EWBCardDefinitionKind::Action;
	Definitions.Add(MoveTemp(PrivateDraw));

	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("undertow_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUndertowUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 HP,
	const int32 ATK,
	const int32 AR,
	const int32 BaseRL,
	const int32 CurrentRL,
	const int32 RLUsed)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = HP;
	Unit.ATK = ATK;
	Unit.AR = AR;
	Unit.SetCanonicalRL(BaseRL, CurrentRL, RLUsed);
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBZoneCardEntry MakeUndertowZoneEntry(
	const int32 OwnerId,
	const FString& InstanceId,
	const FString& CardId,
	const EWBCardZone Zone,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	return Entry;
}

FWBGameStateData MakeUndertowReplacementState(
	const FString& ReplacementCardId,
	const int32 SourceCurrentRL,
	const int32 WandCount,
	const bool bSourceHero = false)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::Response;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = UndertowAttackerId;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = bSourceHero ? UndertowSourceId : UndertowHeroId;
	State.Players = { Player0, Player1 };

	State.AddUnitForTest(MakeUndertowUnit(
		UndertowAttackerId, 0, TEXT("undertow_attacker"),
		FWBTile(4, 4), 14, 4, 8, 4, 4, 0));
	if (!bSourceHero)
	{
		State.AddUnitForTest(MakeUndertowUnit(
			UndertowHeroId, 1, TEXT("undertow_hero"),
			FWBTile(5, 0), 14, 1, 2, 4, 4, 0));
	}
	State.AddUnitForTest(MakeUndertowUnit(
		UndertowSourceId, 1, TEXT("undertow_source"),
		FWBTile(4, 1), 16, 3, 2, 3, SourceCurrentRL, WandCount));

	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	Zones1.Hand.Add(MakeUndertowZoneEntry(
		1,
		TEXT("undertow_replacement_instance"),
		ReplacementCardId,
		EWBCardZone::Hand,
		0));
	Zones1.Deck.Add(MakeUndertowZoneEntry(
		1,
		TEXT("undertow_private_draw_instance"),
		TEXT("undertow_private_draw"),
		EWBCardZone::Deck,
		0));
	Zones1.Deck.Add(MakeUndertowZoneEntry(
		1,
		TEXT("undertow_second_deck_instance"),
		TEXT("undertow_private_draw"),
		EWBCardZone::Deck,
		1));
	State.GetMutableCardZoneStateForTest().PlayerZones = { Zones0, Zones1 };

	for (int32 Index = 0; Index < WandCount; ++Index)
	{
		FWBEquippedCardEntry Wand;
		Wand.Card.InstanceId = FString::Printf(
			TEXT("undertow_wand_instance_%d"), Index);
		Wand.Card.CardId = FString::Printf(TEXT("undertow_wand_%d"), Index);
		Wand.Card.OwnerPlayerId = 1;
		Wand.EquippedToUnitId = UndertowSourceId;
		Wand.SlotId = FString::Printf(TEXT("wand_%d"), Index);
		Wand.EquipOrder = Index;
		State.GetMutableCardZoneStateForTest().EquippedCards.Add(MoveTemp(Wand));
	}

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.Stage = EWBAttackContinuationStage::PreHit;
	Pending.AttackerUnitId = UndertowAttackerId;
	Pending.DefenderUnitId = UndertowSourceId;
	Pending.OriginalAttackerUnitId = UndertowAttackerId;
	Pending.OriginalDefenderUnitId = UndertowSourceId;
	Pending.AttackingPlayerId = 0;
	Pending.AttackerTile = FWBTile(4, 4);
	Pending.DefenderTile = FWBTile(4, 1);
	Pending.DeclarationActionId = TEXT("attack:p0:u10:t30");
	Pending.ContinuationId = TEXT("undertow_inheritance_transaction");
	State.SetPendingAttackForTest(Pending);
	State.ReactionWindow.Kind = EWBReactionWindowKind::PreHit;
	State.ReactionWindow.OriginatingPlayerId = 0;
	State.ReactionWindow.SourceActionId = Pending.DeclarationActionId;
	State.ReactionWindow.SourceUnitId = UndertowAttackerId;
	State.ReactionWindow.TargetUnitId = UndertowSourceId;
	return State;
}

FWBEffectRequest MakeUndertowReplacementRequest(const FString& ReplacementCardId)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation =
		EWBGenericEffectOp::ReplacePendingAttackDefenderFromHand;
	Payload.PendingAttackContinuationId =
		TEXT("undertow_inheritance_transaction");
	Payload.RequiredSourceFaction = TEXT("csn");
	Payload.RequiredReplacementFaction = TEXT("csn");
	Payload.RequiredReplacementKind = EWBEffectReplacementCardKind::Character;
	Payload.InheritancePolicy =
		EWBEffectInheritancePolicy::TransferEquippedWandsAndAddSourceCurrentRL;

	FWBEffectRequest Request;
	Request.Source.PlayerId = 1;
	Request.Source.SourceCardId = TEXT("fixture_csn_replacement_effect");
	Request.Source.SourceEffectId = TEXT("replace_defender");
	Request.Target.TargetUnitId = UndertowSourceId;
	Request.AuxiliaryCardSelection.Zone = EWBEffectAuxiliaryCardZone::Hand;
	Request.AuxiliaryCardSelection.CardInstanceId =
		TEXT("undertow_replacement_instance");
	Request.AuxiliaryCardSelection.CardId = ReplacementCardId;
	Request.Payloads.Add(MoveTemp(Payload));
	return Request;
}

const FWBUnitState* FindUndertowReplacement(
	const FWBGameStateData& State,
	const FString& CardId)
{
	return State.Units.FindByPredicate([&CardId](const FWBUnitState& Unit)
	{
		return Unit.CardId == CardId && Unit.IsUnitOnBoard();
	});
}

int32 CountUndertowTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.FilterByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	}).Num();
}

const FWBPlayerCardZoneState* UndertowPlayerZones(
	const FWBGameStateData& State)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), 1);
}

FString UndertowProductionRoot()
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
}
}

#define WB_UNDERTOW_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_UNDERTOW_TEST(FWBCSNUndertowProductionDefinitionTest,
	"Wandbound.CSNUndertowArchivist.CardDB.ProductionDefinitionLoads")
bool FWBCSNUndertowProductionDefinitionTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(UndertowProductionRoot());
	TestTrue(TEXT("Production bundle loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid())
	{
		return false;
	}
	const FWBProductionCardRecord* Record = Loaded.Snapshot->FindRecord(
		TEXT("char_csn_undertow_archivist"));
	TestNotNull(TEXT("Undertow exists"), Record);
	if (Record == nullptr)
	{
		return false;
	}
	const FWBCardDefinition& Card = Record->CoreDefinition;
	TestEqual(TEXT("Public name"), Card.PublicName,
		FString(TEXT("CSN Undertow Archivist")));
	TestEqual(TEXT("Kind"), Card.Kind, EWBCardDefinitionKind::Character);
	TestTrue(TEXT("CSN faction"), Card.PublicFactions.Contains(TEXT("csn")));
	TestEqual(TEXT("HP 11"), Card.CharacterStats.HP, 11);
	TestEqual(TEXT("ATK 2"), Card.CharacterStats.ATK, 2);
	TestEqual(TEXT("AR 3"), Card.CharacterStats.AR, 3);
	TestEqual(TEXT("RL 2"), Card.CharacterStats.RL, 2);
	TestEqual(TEXT("One inheritance trigger"),
		Card.AfterCSNInheritanceTriggers.Num(), 1);
	if (Card.AfterCSNInheritanceTriggers.Num() == 1)
	{
		TestEqual(TEXT("Draw exactly one"),
			Card.AfterCSNInheritanceTriggers[0].DrawCount, 1);
		TestTrue(TEXT("Mandatory"),
			Card.AfterCSNInheritanceTriggers[0].bMandatory);
	}
	AddInfo(FString::Printf(
		TEXT("CSN_UNDERTOW_BUNDLE_DIGEST=%s"),
		*Loaded.Snapshot->ContentDigest));
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowDefinitionDrivenFixtureTest,
	"Wandbound.CSNUndertowArchivist.CardDB.DefinitionDrivenAlternateId")
bool FWBCSNUndertowDefinitionDrivenFixtureTest::RunTest(const FString&)
{
	const FWBCardDefinitionFixtureLoadResult Loaded =
		WBCardDefinitionFixtureLoader::LoadRepositoryFromFile(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Data/Replay/CSNUndertowArchivistFixture/definitions.json")));
	TestTrue(TEXT("Fixture loads"), Loaded.bOk);
	const FWBCardDefinitionRepositoryLookupResult Equivalent =
		WBCardDefinitionRepository::FindCardById(
			Loaded.Repository, TEXT("fixture_inheritance_draw_character"));
	const FWBCardDefinitionRepositoryLookupResult NameOnly =
		WBCardDefinitionRepository::FindCardById(
			Loaded.Repository,
			TEXT("char_csn_undertow_archivist_name_only"));
	TestTrue(TEXT("Alternate id exists"), Equivalent.bFound);
	TestTrue(TEXT("Name-only id exists"), NameOnly.bFound);
	TestEqual(TEXT("Alternate id carries behavior"),
		Equivalent.Definition.AfterCSNInheritanceTriggers.Num(), 1);
	TestEqual(TEXT("Name alone carries no behavior"),
		NameOnly.Definition.AfterCSNInheritanceTriggers.Num(), 0);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowNormalSummonTest,
	"Wandbound.CSNUndertowArchivist.Summon.NormalDoesNotDraw")
bool FWBCSNUndertowNormalSummonTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	const FWBCardDefinitionRepository Repository = MakeUndertowRepository(CardId);
	FWBGameStateData State;
	State.CurrentPlayer = 1;
	State.PriorityPlayer = 1;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = UndertowHeroId;
	State.Players = { Player0, Player1 };
	State.AddUnitForTest(MakeUndertowUnit(
		UndertowHeroId, 1, TEXT("undertow_hero"), FWBTile(4, 8),
		14, 1, 2, 4, 4, 0));
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	Zones1.Hand.Add(MakeUndertowZoneEntry(
		1, TEXT("normal_summon_instance"), CardId, EWBCardZone::Hand, 0));
	Zones1.Deck.Add(MakeUndertowZoneEntry(
		1, TEXT("normal_summon_deck_top"), TEXT("undertow_private_draw"),
		EWBCardZone::Deck, 0));
	State.GetMutableCardZoneStateForTest().PlayerZones = { Zones0, Zones1 };

	FWBSummonExecutionRequest Request;
	Request.PlayerId = 1;
	Request.SourceInstanceId = TEXT("normal_summon_instance");
	Request.SourceCardId = CardId;
	Request.TargetTile = FWBTile(4, 7);
	const FWBSummonExecutionResult Summoned =
		WBSummonExecution::ExecuteCharacterSummonFromHand(
			State, Repository, Request);
	TestTrue(TEXT("Normal summon succeeds"), Summoned.bOk);
	const FWBUnitState* Unit = State.GetUnitById(Summoned.CreatedUnitId);
	TestNotNull(TEXT("Unit created"), Unit);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("HP"), Unit->HP, 11);
		TestEqual(TEXT("ATK"), Unit->ATK, 2);
		TestEqual(TEXT("AR"), Unit->AR, 3);
		TestEqual(TEXT("Base RL"), Unit->BaseRL, 2);
	}
	const FWBPlayerCardZoneState* Zones = UndertowPlayerZones(State);
	TestEqual(TEXT("Deck unchanged"), Zones != nullptr ? Zones->Deck.Num() : -1, 1);
	TestFalse(TEXT("No inheritance draw"), Zones != nullptr
		&& Zones->Hand.ContainsByPredicate([](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == TEXT("normal_summon_deck_top");
		}));
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowZeroChangeInheritanceTest,
	"Wandbound.CSNUndertowArchivist.Inheritance.ZeroRLZeroWandsStillDraws")
bool FWBCSNUndertowZeroChangeInheritanceTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	FWBGameStateData State = MakeUndertowReplacementState(CardId, 0, 0);
	const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
		State, MakeUndertowReplacementRequest(CardId),
		MakeUndertowRepository(CardId));
	TestTrue(TEXT("Replacement succeeds"), Applied.bOk);
	const FWBUnitState* Unit = FindUndertowReplacement(State, CardId);
	TestNotNull(TEXT("Replacement exists"), Unit);
	TestEqual(TEXT("Base RL remains printed"), Unit != nullptr ? Unit->BaseRL : -1, 2);
	const FWBPlayerCardZoneState* Zones = UndertowPlayerZones(State);
	TestEqual(TEXT("One exact card drawn"), Zones != nullptr ? Zones->Hand.Num() : -1, 1);
	TestTrue(TEXT("Top instance moved to Hand"), Zones != nullptr
		&& Zones->Hand.ContainsByPredicate([](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == TEXT("undertow_private_draw_instance")
				&& Entry.Zone == EWBCardZone::Hand;
		}));
	TestEqual(TEXT("Inheritance once"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("csn_inheritance"))), 1);
	TestEqual(TEXT("Trigger once"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("csn_inheritance_triggered"))), 1);
	TestEqual(TEXT("Draw once"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("csn_inheritance_card_drawn"))), 1);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowMultipleWandInheritanceTest,
	"Wandbound.CSNUndertowArchivist.Inheritance.MultipleWandsSingleDraw")
bool FWBCSNUndertowMultipleWandInheritanceTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	FWBGameStateData State = MakeUndertowReplacementState(CardId, 5, 2);
	const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
		State, MakeUndertowReplacementRequest(CardId),
		MakeUndertowRepository(CardId));
	TestTrue(TEXT("Replacement succeeds"), Applied.bOk);
	const FWBUnitState* Unit = FindUndertowReplacement(State, CardId);
	TestNotNull(TEXT("Replacement exists"), Unit);
	if (Unit == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Base RL inherits five"), Unit->BaseRL, 7);
	TestEqual(TEXT("Current RL reconciles"), Unit->CurrentRL, 7);
	TestEqual(TEXT("RLUsed carries two Wands"), Unit->RLUsed, 2);
	TestEqual(TEXT("Both exact Wands remain equipped"),
		State.GetCardZoneState().EquippedCards.FilterByPredicate(
			[Unit](const FWBEquippedCardEntry& Entry)
			{
				return Entry.EquippedToUnitId == Unit->UnitId;
			}).Num(),
		2);
	TestEqual(TEXT("Two transfer traces"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("inherited_wand_transferred"))), 2);
	TestEqual(TEXT("Still one trigger"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("csn_inheritance_triggered"))), 1);
	TestEqual(TEXT("Still one drawn card"), UndertowPlayerZones(State)->Hand.Num(), 1);
	TestEqual(TEXT("Same pending attack"),
		State.PendingAttack.ContinuationId,
		FString(TEXT("undertow_inheritance_transaction")));
	TestEqual(TEXT("Attack redirected"),
		State.PendingAttack.DefenderUnitId, Unit->UnitId);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowPassiveSuppressionTest,
	"Wandbound.CSNUndertowArchivist.Passive.StunFrozenNegatedSuppressDrawOnly")
bool FWBCSNUndertowPassiveSuppressionTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	for (const FName Status :
		{ FName(TEXT("Stunned")), FName(TEXT("Frozen")), FName(TEXT("Negated")) })
	{
		FWBGameStateData State;
		FWBPlayerStateData Player0;
		Player0.PlayerId = 0;
		FWBPlayerStateData Player1;
		Player1.PlayerId = 1;
		State.Players = { Player0, Player1 };
		FWBUnitState Unit = MakeUndertowUnit(
			42, 1, CardId, FWBTile(4, 4), 11, 2, 3, 7, 7, 2);
		Unit.AddStatus(Status, 1);
		State.AddUnitForTest(Unit);
		FWBPlayerCardZoneState Zones0;
		Zones0.PlayerId = 0;
		FWBPlayerCardZoneState Zones1;
		Zones1.PlayerId = 1;
		Zones1.Deck.Add(MakeUndertowZoneEntry(
			1, TEXT("suppressed_private_draw"), TEXT("undertow_private_draw"),
			EWBCardZone::Deck, 0));
		State.GetMutableCardZoneStateForTest().PlayerZones = { Zones0, Zones1 };

		FWBCSNInheritanceEventContext Context;
		Context.InheritingUnitId = 42;
		Context.InheritingPlayerId = 1;
		Context.SourceUnitId = 30;
		Context.SourceCurrentRL = 5;
		Context.InheritedWandCount = 2;
		Context.TransactionId = TEXT("suppressed_inheritance");
		const FWBCSNInheritanceTriggerResult Result =
			WBCSNInheritanceTrigger::ResolveAfterSuccessfulInheritance(
				State, MakeUndertowRepository(CardId), Context);
		TestTrue(*FString::Printf(TEXT("%s is a valid suppression"),
			*Status.ToString()), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s draws nothing"),
			*Status.ToString()), UndertowPlayerZones(State)->Hand.Num(), 0);
		TestEqual(*FString::Printf(TEXT("%s leaves inheritance applied"),
			*Status.ToString()), State.GetUnitById(42)->BaseRL, 7);
		TestEqual(*FString::Printf(TEXT("%s emits no trigger"),
			*Status.ToString()), Result.TraceEvents.Num(), 0);
	}
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowAtomicFailureTest,
	"Wandbound.CSNUndertowArchivist.Atomicity.FailureAndStaleContinuationNoDraw")
bool FWBCSNUndertowAtomicFailureTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	const FWBCardDefinitionRepository Repository = MakeUndertowRepository(CardId);
	auto ExpectFailure = [this, &Repository, &CardId](
		const TCHAR* Label,
		FWBGameStateData State,
		FWBEffectRequest Request)
	{
		const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
		const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(
			State, Request, Repository);
		TestFalse(Label, Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s is atomic"), Label),
			WBProductionMatchReplay::BuildGameStateDigest(State), Before);
		TestEqual(*FString::Printf(TEXT("%s emits no draw"), Label),
			CountUndertowTrace(
				Result.TraceEvents, FName(TEXT("csn_inheritance_card_drawn"))),
			0);
	};

	FWBEffectRequest Missing = MakeUndertowReplacementRequest(CardId);
	Missing.AuxiliaryCardSelection.CardInstanceId = TEXT("missing");
	ExpectFailure(TEXT("Missing replacement fails"),
		MakeUndertowReplacementState(CardId, 5, 1), Missing);

	FWBEffectRequest Stale = MakeUndertowReplacementRequest(CardId);
	Stale.Payloads[0].PendingAttackContinuationId = TEXT("stale");
	ExpectFailure(TEXT("Stale continuation fails"),
		MakeUndertowReplacementState(CardId, 5, 1), Stale);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowEmptyDeckAtomicityTest,
	"Wandbound.CSNUndertowArchivist.Atomicity.EmptyDeckUsesExistingFailure")
bool FWBCSNUndertowEmptyDeckAtomicityTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	FWBGameStateData State = MakeUndertowReplacementState(CardId, 0, 0);
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), 1);
	if (Zones == nullptr)
	{
		AddError(TEXT("Player zones missing"));
		return false;
	}
	Zones->Deck.Reset();
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
	const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
		State, MakeUndertowReplacementRequest(CardId),
		MakeUndertowRepository(CardId));
	TestFalse(TEXT("Existing empty-deck behavior fails trigger"), Applied.bOk);
	TestEqual(TEXT("Empty deck reason"), Applied.Reason,
		FString(TEXT("deck_empty")));
	TestEqual(TEXT("Whole replacement remains atomic"),
		WBProductionMatchReplay::BuildGameStateDigest(State), Before);
	TestEqual(TEXT("No speculative draw trace"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("csn_inheritance_card_drawn"))), 0);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowMalformedDefinitionTest,
	"Wandbound.CSNUndertowArchivist.CardDB.MalformedTriggerFailsClosed")
bool FWBCSNUndertowMalformedDefinitionTest::RunTest(const FString&)
{
	const FString Json = TEXT(R"json({
		"repository_id":"bad_inheritance_trigger",
		"source_version":"fixture_v1",
		"cards":[{
			"card_id":"bad_inheritance_character",
			"public_name":"Bad Inheritance Character",
			"kind":"character",
			"character_stats":{"hp":11,"atk":2,"ar":3,"rl":2},
			"after_csn_inheritance_triggers":[{
				"trigger_id":"bad_trigger",
				"draw_count":0,
				"mandatory":false
			}]
		}]
	})json");
	const FWBCardDefinitionFixtureLoadResult Loaded =
		WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(
			Json, TEXT("bad_inheritance_trigger.json"));
	TestFalse(TEXT("Malformed trigger fails closed"), Loaded.bOk);
	TestTrue(TEXT("Draw count diagnostic present"),
		Loaded.Diagnostics.ContainsByPredicate([](
			const FWBCardDefinitionFixtureLoadDiagnostic& Diagnostic)
		{
			return Diagnostic.Code
				== TEXT("csn_inheritance_trigger_draw_count_invalid");
		}));
	TestTrue(TEXT("Optional trigger diagnostic present"),
		Loaded.Diagnostics.ContainsByPredicate([](
			const FWBCardDefinitionFixtureLoadDiagnostic& Diagnostic)
		{
			return Diagnostic.Code
				== TEXT("optional_csn_inheritance_trigger_unsupported");
		}));
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowTerminalBoundaryTest,
	"Wandbound.CSNUndertowArchivist.Terminal.HeroBoundarySuppressesDraw")
bool FWBCSNUndertowTerminalBoundaryTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	FWBGameStateData State = MakeUndertowReplacementState(CardId, 5, 1, true);
	const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
		State, MakeUndertowReplacementRequest(CardId),
		MakeUndertowRepository(CardId));
	TestTrue(TEXT("Existing Hero Crash-In path resolves"), Applied.bOk);
	TestTrue(TEXT("Hero boundary remains terminal"), State.bGameOver);
	TestEqual(TEXT("No post-terminal draw"), UndertowPlayerZones(State)->Hand.Num(), 0);
	TestEqual(TEXT("No post-terminal trigger"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("csn_inheritance_triggered"))), 0);
	TestNotNull(TEXT("Replacement still enters before terminal"),
		FindUndertowReplacement(State, CardId));
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowPrivacyTest,
	"Wandbound.CSNUndertowArchivist.Privacy.DrawIdentityRemainsPrivate")
bool FWBCSNUndertowPrivacyTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	FWBGameStateData State = MakeUndertowReplacementState(CardId, 0, 0);
	const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
		State, MakeUndertowReplacementRequest(CardId),
		MakeUndertowRepository(CardId));
	TestTrue(TEXT("Inheritance resolves"), Applied.bOk);
	const FWBCardZonePlayerObservation Opponent =
		WBCardZoneObservation::BuildObservationForPlayer(State, 0);
	TestFalse(TEXT("Opponent cannot observe drawn definition"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Opponent, TEXT("undertow_private_draw")));
	TestFalse(TEXT("Opponent cannot observe drawn instance"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Opponent, TEXT("undertow_private_draw_instance")));
	const FString TraceJson = WBReplayTrace::SerializeEvents(Applied.TraceEvents);
	TestFalse(TEXT("Trace omits drawn definition"),
		TraceJson.Contains(TEXT("undertow_private_draw")));
	TestFalse(TEXT("Trace omits drawn instance"),
		TraceJson.Contains(TEXT("undertow_private_draw_instance")));
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowReplayDeterminismTest,
	"Wandbound.CSNUndertowArchivist.Replay.StateAndTraceDeterministic")
bool FWBCSNUndertowReplayDeterminismTest::RunTest(const FString&)
{
	const FString CardId = TEXT("fixture_inheritance_draw_character");
	const FWBCardDefinitionRepository Repository = MakeUndertowRepository(CardId);
	FWBGameStateData FirstState = MakeUndertowReplacementState(CardId, 5, 2);
	FWBGameStateData SecondState = FirstState;
	const FWBEffectRequest Request = MakeUndertowReplacementRequest(CardId);
	const FWBEffectRequestResult First = WBEffectRunner::ApplyEffectRequest(
		FirstState, Request, Repository);
	const FWBEffectRequestResult Second = WBEffectRunner::ApplyEffectRequest(
		SecondState, Request, Repository);
	TestTrue(TEXT("First resolves"), First.bOk);
	TestTrue(TEXT("Second resolves"), Second.bOk);
	TestEqual(TEXT("State digest deterministic"),
		WBProductionMatchReplay::BuildGameStateDigest(FirstState),
		WBProductionMatchReplay::BuildGameStateDigest(SecondState));
	TestEqual(TEXT("Trace digest deterministic"),
		WBProductionMatchReplay::BuildTraceDigest(First.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(Second.TraceEvents));
	TestEqual(TEXT("Trace bytes deterministic"),
		WBReplayTrace::SerializeEvents(First.TraceEvents),
		WBReplayTrace::SerializeEvents(Second.TraceEvents));
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowProductionCrashInIntegrationTest,
	"Wandbound.CSNUndertowArchivist.CrashIn.RealDefinitionsResolve")
bool FWBCSNUndertowProductionCrashInIntegrationTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(UndertowProductionRoot());
	TestTrue(TEXT("Production bundle loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid())
	{
		return false;
	}
	const FString CardId = TEXT("char_csn_undertow_archivist");
	FWBGameStateData State = MakeUndertowReplacementState(CardId, 5, 0);
	State.GetMutableUnitById(UndertowAttackerId)->CardId =
		TEXT("crash_in_smoke_hero_attacker");
	State.GetMutableUnitById(UndertowHeroId)->CardId =
		TEXT("crash_in_smoke_hero_csn");
	State.GetMutableUnitById(UndertowSourceId)->CardId = TEXT("char_csn_rook");
	const FWBProductionCardRecord* CrashIn = Loaded.Snapshot->FindRecord(
		TEXT("effect_react_csn_crash_in"));
	TestNotNull(TEXT("Real Crash-In exists"), CrashIn);
	if (CrashIn == nullptr || CrashIn->CoreDefinition.ActivatedEffects.IsEmpty())
	{
		return false;
	}
	FWBEffectRequest Request = MakeUndertowReplacementRequest(CardId);
	Request.Source.SourceCardId = CrashIn->CoreDefinition.CardId;
	Request.Source.SourceEffectId =
		CrashIn->CoreDefinition.ActivatedEffects[0].EffectId;
	Request.Payloads = CrashIn->CoreDefinition.ActivatedEffects[0].Payloads;
	Request.Payloads[0].PendingAttackContinuationId =
		TEXT("undertow_inheritance_transaction");
	const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
		State, Request, Loaded.Snapshot->CoreRepository);
	TestTrue(TEXT("Real definitions resolve"), Applied.bOk);
	const FWBUnitState* Unit = FindUndertowReplacement(State, CardId);
	TestNotNull(TEXT("Real Undertow replaces defender"), Unit);
	TestEqual(TEXT("Real Undertow draws one"),
		UndertowPlayerZones(State)->Hand.Num(), 1);
	TestEqual(TEXT("Attack redirects once"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("pending_attack_redirected"))), 1);
	TestEqual(TEXT("No second attack declaration"), CountUndertowTrace(
		Applied.TraceEvents, FName(TEXT("attack_declared"))), 0);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowProductionSmokeTest,
	"Wandbound.CSNUndertowArchivist.Fixture.ProductionSmokeAndFreshReplay")
bool FWBCSNUndertowProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = UndertowProductionRoot();
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNUndertowArchivistFixture/match_spec.json"));
	const FWBProductionCSNCrashInSmokeResult First =
		WBProductionCSNCrashInSmoke::RunUndertow(Request);
	const FWBProductionCSNCrashInSmokeResult Second =
		WBProductionCSNCrashInSmoke::RunUndertow(Request);
	if (!First.bOk)
	{
		AddError(FString::Printf(
			TEXT("First Undertow smoke failed: %s"), *First.Reason));
	}
	if (!Second.bOk)
	{
		AddError(FString::Printf(
			TEXT("Second Undertow smoke failed: %s"), *Second.Reason));
	}
	TestTrue(TEXT("First smoke succeeds"), First.bOk);
	TestTrue(TEXT("Second smoke succeeds"), Second.bOk);
	TestEqual(TEXT("Archive bytes deterministic"),
		First.SerializedArchive, Second.SerializedArchive);
	TestEqual(TEXT("Receipt bytes deterministic"),
		First.SerializedReceipt, Second.SerializedReceipt);
	TestEqual(TEXT("State digest deterministic"),
		First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("Trace digest deterministic"),
		First.FinalTraceDigest, Second.FinalTraceDigest);
	TestEqual(TEXT("Generation deterministic"),
		First.FinalGeneration, Second.FinalGeneration);
	TestEqual(TEXT("Revision deterministic"),
		First.FinalRevision, Second.FinalRevision);
	TestEqual(TEXT("Replay count deterministic"),
		First.RecordsVerified, Second.RecordsVerified);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowNegatedCrashInTest,
	"Wandbound.CSNUndertowArchivist.CrashIn.NegatedDoesNotDraw")
bool FWBCSNUndertowNegatedCrashInTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = UndertowProductionRoot();
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNUndertowArchivistFixture/match_spec.json"));
	const FWBProductionCSNCrashInSmokeResult Result =
		WBProductionCSNCrashInSmoke::RunUndertowNegatedForTest(Request);
	if (!Result.bOk)
	{
		AddError(FString::Printf(
			TEXT("Negated Crash-In scenario failed: %s"), *Result.Reason));
	}
	TestTrue(TEXT("Real Crash-In negation leaves Undertow undrawn"), Result.bOk);
	return true;
}

WB_UNDERTOW_TEST(FWBCSNUndertowAuthorityGuardTest,
	"Wandbound.CSNUndertowArchivist.Authority.NoCardIdBranchCodecOrSchemaChange")
bool FWBCSNUndertowAuthorityGuardTest::RunTest(const FString&)
{
	FString TriggerSource;
	FFileHelper::LoadFileToString(TriggerSource, *FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp")));
	FString ReplacementSource;
	FFileHelper::LoadFileToString(ReplacementSource, *FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source/WandboundCore/Private/WBUnitReplacementEffect.cpp")));
	TestFalse(TEXT("Trigger authority has no Undertow id branch"),
		TriggerSource.Contains(TEXT("char_csn_undertow_archivist")));
	TestFalse(TEXT("Inheritance authority has no Undertow id branch"),
		ReplacementSource.Contains(TEXT("char_csn_undertow_archivist")));
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

#undef WB_UNDERTOW_TEST

#endif
