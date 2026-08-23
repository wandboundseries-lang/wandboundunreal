#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBPostDestructionTrigger.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNCrashInSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBUnitStatDelta.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 HeroId = 1;
constexpr int32 EnemyHeroId = 2;
constexpr int32 SableAId = 10;
constexpr int32 SableBId = 11;
constexpr int32 SubjectAId = 20;
constexpr int32 SubjectBId = 21;

FWBAfterUnitDestroyedTriggerDefinition MakeObserverTrigger()
{
	FWBAfterUnitDestroyedTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("grow_after_controlled_csn_destroyed");
	Trigger.SourceScope = EWBAfterUnitDestroyedSourceScope::ControlledFactionUnitDestroyed;
	Trigger.Operation = EWBPostDestructionEffectOperation::ApplyPersistentStatDeltaToTriggerSource;
	Trigger.RequiredFaction = TEXT("csn");
	Trigger.bMandatory = true;
	Trigger.Target = EWBPostDestructionTarget::TriggerSource;
	Trigger.StatDelta.ATKDelta = 1;
	Trigger.StatDelta.MaxHPDelta = 1;
	Trigger.StatDelta.CurrentHPDelta = 1;
	return Trigger;
}

FWBAfterUnitDestroyedTriggerDefinition MakeRookTrigger()
{
	FWBAfterUnitDestroyedTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("summon_csn_character_from_deck");
	Trigger.SourceScope = EWBAfterUnitDestroyedSourceScope::DestroyedSelf;
	Trigger.Operation = EWBPostDestructionEffectOperation::SummonCharacterFromDeckToDestroyedTile;
	Trigger.RequiredFaction = TEXT("csn");
	Trigger.SummonCount = 1;
	Trigger.bMandatory = true;
	Trigger.bIgnoreSummoningConditions = true;
	Trigger.bApplyCSNInheritance = true;
	return Trigger;
}

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const FString& Faction,
	const bool bObserver = false,
	const bool bRook = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.PublicFactions.Add(Faction);
	Definition.CharacterStats.HP = 10;
	Definition.CharacterStats.ATK = 1;
	Definition.CharacterStats.AR = 3;
	Definition.CharacterStats.RL = 2;
	if (bObserver) Definition.AfterUnitDestroyedTriggers.Add(MakeObserverTrigger());
	if (bRook) Definition.AfterUnitDestroyedTriggers.Add(MakeRookTrigger());
	return Definition;
}

FWBCardDefinitionRepository MakeRepository(
	const FString& ObserverCardId = TEXT("fixture_observer"),
	const bool bObserverHasTrigger = true)
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("hero"), TEXT("csn")));
	Definitions.Add(MakeCharacter(TEXT("enemy_hero"), TEXT("officer")));
	Definitions.Add(MakeCharacter(ObserverCardId, TEXT("csn"), bObserverHasTrigger));
	Definitions.Add(MakeCharacter(TEXT("friendly_csn_subject"), TEXT("csn")));
	Definitions.Add(MakeCharacter(TEXT("enemy_csn_subject"), TEXT("csn")));
	Definitions.Add(MakeCharacter(TEXT("friendly_officer_subject"), TEXT("officer")));
	Definitions.Add(MakeCharacter(TEXT("char_csn_sable_like"), TEXT("officer")));
	Definitions.Add(MakeCharacter(TEXT("fixture_rook"), TEXT("csn"), false, true));
	Definitions.Add(MakeCharacter(TEXT("rook_candidate"), TEXT("csn")));

	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("sable_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 HP = 10)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = 10;
	Unit.ATK = 1;
	Unit.AR = 3;
	Unit.SetCanonicalRL(2, 2, 0);
	return Unit;
}

FWBGameStateData MakeState(
	const FString& ObserverCardId = TEXT("fixture_observer"),
	const FString& SubjectCardId = TEXT("friendly_csn_subject"),
	const int32 SubjectOwner = 0,
	const int32 SubjectHP = 0)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = HeroId;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = EnemyHeroId;
	State.Players = { Player0, Player1 };
	State.AddUnitForTest(MakeUnit(HeroId, 0, TEXT("hero"), FWBTile(0, 0), 20));
	State.GetMutableUnitById(HeroId)->MaxHP = 20;
	State.AddUnitForTest(MakeUnit(EnemyHeroId, 1, TEXT("enemy_hero"), FWBTile(8, 8), 20));
	State.GetMutableUnitById(EnemyHeroId)->MaxHP = 20;
	State.AddUnitForTest(MakeUnit(SableAId, 0, ObserverCardId, FWBTile(2, 2)));
	State.AddUnitForTest(MakeUnit(SubjectAId, SubjectOwner, SubjectCardId, FWBTile(3, 3), SubjectHP));
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	State.GetMutableCardZoneStateForTest().PlayerZones = { Zones0, Zones1 };
	return State;
}

FWBPostDestructionTriggerResult DestroyAndAdvance(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const EWBUnitDestructionCause Cause)
{
	const FWBApplyActionResult Destroyed = Cause == EWBUnitDestructionCause::ExplicitDestroy
		? WBDeathResolution::ApplyExplicitUnitDestruction(State, SubjectAId)
		: WBDeathResolution::ApplyZeroHPDeathResolution(State, Cause);
	if (!Destroyed.bOk)
	{
		FWBPostDestructionTriggerResult Failure;
		Failure.Reason = Destroyed.Reason;
		return Failure;
	}
	return WBPostDestructionTrigger::AdvanceToDecisionOrComplete(State, Repository, 0, 0);
}

int32 CountTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.FilterByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	}).Num();
}

bool SnapshotContainsSource(
	const FWBUnitDestructionSnapshot& Snapshot,
	const int32 SourceUnitId)
{
	return Snapshot.ObserverSources.ContainsByPredicate(
		[SourceUnitId](const FWBPostDestructionObserverSourceSnapshot& Source)
		{
			return Source.SourceUnitId == SourceUnitId;
		});
}

void AddDeckCandidate(FWBGameStateData& State)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), 0);
	check(Zones != nullptr);
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = TEXT("rook_candidate_instance");
	Entry.Card.CardId = TEXT("rook_candidate");
	Entry.Card.OwnerPlayerId = 0;
	Entry.Zone = EWBCardZone::Deck;
	Entry.ZoneIndex = 0;
	Zones->Deck.Add(Entry);
}
}

#define WB_SABLE_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_SABLE_TEST(FWBCSNSableProductionDefinitionTest,
	"Wandbound.CSNSable.CardDB.ProductionDefinition")
bool FWBCSNSableProductionDefinitionTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	TestTrue(TEXT("1 production suite loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid()) return false;
	AddInfo(TEXT("Production Sable bundle digest: ") + Loaded.Snapshot->ContentDigest);
	const FWBProductionCardRecord* Record = Loaded.Snapshot->FindRecord(TEXT("char_csn_sable"));
	TestNotNull(TEXT("2 Sable loads"), Record);
	if (Record == nullptr) return false;
	TestEqual(TEXT("3 HP"), Record->CoreDefinition.CharacterStats.HP, 10);
	TestEqual(TEXT("4 ATK"), Record->CoreDefinition.CharacterStats.ATK, 1);
	TestEqual(TEXT("5 AR"), Record->CoreDefinition.CharacterStats.AR, 3);
	TestEqual(TEXT("6 RL"), Record->CoreDefinition.CharacterStats.RL, 2);
	TestTrue(TEXT("7 CSN faction"), Record->CoreDefinition.PublicFactions.Contains(TEXT("csn")));
	TestEqual(TEXT("8 one observer trigger"), Record->CoreDefinition.AfterUnitDestroyedTriggers.Num(), 1);
	if (Record->CoreDefinition.AfterUnitDestroyedTriggers.IsEmpty()) return false;
	const FWBAfterUnitDestroyedTriggerDefinition& Trigger =
		Record->CoreDefinition.AfterUnitDestroyedTriggers[0];
	TestEqual(TEXT("9 generic observer scope"), Trigger.SourceScope,
		EWBAfterUnitDestroyedSourceScope::ControlledFactionUnitDestroyed);
	TestEqual(TEXT("10 generic stat operation"), Trigger.Operation,
		EWBPostDestructionEffectOperation::ApplyPersistentStatDeltaToTriggerSource);
	TestEqual(TEXT("11 source target"), Trigger.Target, EWBPostDestructionTarget::TriggerSource);
	TestEqual(TEXT("12 ATK delta"), Trigger.StatDelta.ATKDelta, 1);
	TestEqual(TEXT("13 MaxHP delta"), Trigger.StatDelta.MaxHPDelta, 1);
	TestEqual(TEXT("14 HP delta"), Trigger.StatDelta.CurrentHPDelta, 1);
	TestTrue(TEXT("15 full inheritance wording"),
		Record->CoreDefinition.PublicRulesText.StartsWith(TEXT("CSN Inheritance:")));
	TestTrue(TEXT("16 public growth wording"),
		Record->CoreDefinition.PublicRulesText.Contains(TEXT("+1 ATK, +1 HP, and +1 Max HP")));
	return true;
}

WB_SABLE_TEST(FWBCSNSableDestructionCausesTest,
	"Wandbound.CSNSable.Observer.CanonicalDestructionCauses")
bool FWBCSNSableDestructionCausesTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const EWBUnitDestructionCause Causes[] = {
		EWBUnitDestructionCause::BattleDamage,
		EWBUnitDestructionCause::EffectDamage,
		EWBUnitDestructionCause::StatusDamage,
		EWBUnitDestructionCause::ExplicitDestroy,
		EWBUnitDestructionCause::ReplacementEffect };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Causes); ++Index)
	{
		FWBGameStateData State = MakeState(TEXT("fixture_observer"),
			TEXT("friendly_csn_subject"), 0,
			Causes[Index] == EWBUnitDestructionCause::ExplicitDestroy ? 5 : 0);
		const FWBPostDestructionTriggerResult Result = DestroyAndAdvance(State, Repository, Causes[Index]);
		const FWBUnitState* Sable = State.GetUnitById(SableAId);
		TestTrue(FString::Printf(TEXT("%d cause resolves"), Index + 17), Result.bOk);
		TestNotNull(FString::Printf(TEXT("%d observer survives"), Index + 22), Sable);
		if (Sable != nullptr)
		{
			TestEqual(FString::Printf(TEXT("%d ATK grows"), Index + 27), Sable->ATK, 2);
			TestEqual(FString::Printf(TEXT("%d HP grows"), Index + 32), Sable->HP, 11);
			TestEqual(FString::Printf(TEXT("%d MaxHP grows"), Index + 37), Sable->MaxHP, 11);
		}
		TestEqual(FString::Printf(TEXT("%d one growth trace"), Index + 42),
			CountTrace(Result.TraceEvents, FName(TEXT("unit_stat_delta_applied"))), 1);
	}
	return true;
}

WB_SABLE_TEST(FWBCSNSableQualificationTest,
	"Wandbound.CSNSable.Observer.FactionControllerAndDefinitionDriven")
bool FWBCSNSableQualificationTest::RunTest(const FString&)
{
	struct FCase { FString ObserverId; FString SubjectId; int32 Owner; bool bExpected; };
	const FCase Cases[] = {
		{ TEXT("alternate_observer_id"), TEXT("friendly_csn_subject"), 0, true },
		{ TEXT("alternate_observer_id"), TEXT("enemy_csn_subject"), 1, false },
		{ TEXT("alternate_observer_id"), TEXT("friendly_officer_subject"), 0, false },
		{ TEXT("alternate_observer_id"), TEXT("char_csn_sable_like"), 0, false }
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
	{
		const FWBCardDefinitionRepository Repository = MakeRepository(Cases[Index].ObserverId, true);
		FWBGameStateData State = MakeState(Cases[Index].ObserverId, Cases[Index].SubjectId, Cases[Index].Owner, 0);
		const FWBPostDestructionTriggerResult Result = DestroyAndAdvance(
			State, Repository, EWBUnitDestructionCause::BattleDamage);
		TestTrue(FString::Printf(TEXT("%d case resolves"), Index + 48), Result.bOk);
		TestEqual(FString::Printf(TEXT("%d semantic trigger result"), Index + 52),
			State.GetUnitById(SableAId)->ATK, Cases[Index].bExpected ? 2 : 1);
	}

	const FWBCardDefinitionRepository NoTriggerRepository =
		MakeRepository(TEXT("char_csn_sable"), false);
	FWBGameStateData NoTrigger = MakeState(TEXT("char_csn_sable"));
	const FWBPostDestructionTriggerResult NoTriggerResult = DestroyAndAdvance(
		NoTrigger, NoTriggerRepository, EWBUnitDestructionCause::BattleDamage);
	TestTrue(TEXT("56 Sable-like id without definition resolves"), NoTriggerResult.bOk);
	TestEqual(TEXT("57 Sable-like id has no authority"), NoTrigger.GetUnitById(SableAId)->ATK, 1);
	return true;
}

WB_SABLE_TEST(FWBCSNSableCaptureAndSuppressionTest,
	"Wandbound.CSNSable.Observer.CaptureSuppressionAndSourceRevalidation")
bool FWBCSNSableCaptureAndSuppressionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const FName Statuses[] = { FName(TEXT("Negated")), FName(TEXT("Stunned")), FName(TEXT("Frozen")) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Statuses); ++Index)
	{
		FWBGameStateData State = MakeState();
		State.GetMutableUnitById(SableAId)->AddStatus(Statuses[Index]);
		const FWBApplyActionResult Death = WBDeathResolution::ApplyZeroHPDeathResolution(
			State, EWBUnitDestructionCause::BattleDamage);
		TestTrue(FString::Printf(TEXT("%d suppressed death commits"), Index + 58), Death.bOk);
		TestTrue(FString::Printf(TEXT("%d event remains queued"), Index + 61),
			!State.PendingUnitDestructionEvents.IsEmpty());
		if (!State.PendingUnitDestructionEvents.IsEmpty())
		{
			TestTrue(FString::Printf(TEXT("%d suppressed source not captured"), Index + 64),
				!SnapshotContainsSource(State.PendingUnitDestructionEvents[0], SableAId));
		}
		const FWBPostDestructionTriggerResult Result =
			WBPostDestructionTrigger::AdvanceToDecisionOrComplete(State, Repository, 0, 0);
		TestTrue(FString::Printf(TEXT("%d suppressed event resolves"), Index + 67), Result.bOk);
		TestEqual(FString::Printf(TEXT("%d no suppressed growth"), Index + 70),
			State.GetUnitById(SableAId)->ATK, 1);
	}

	FWBGameStateData LaterStatus = MakeState();
	TestTrue(TEXT("73 death commits"), WBDeathResolution::ApplyZeroHPDeathResolution(
		LaterStatus, EWBUnitDestructionCause::BattleDamage).bOk);
	TestTrue(TEXT("74 eligible source captured"),
		SnapshotContainsSource(LaterStatus.PendingUnitDestructionEvents[0], SableAId));
	LaterStatus.GetMutableUnitById(SableAId)->AddStatus(FName(TEXT("Stunned")));
	const FWBPostDestructionTriggerResult LaterResult =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(LaterStatus, Repository, 0, 0);
	TestTrue(TEXT("75 queued trigger resolves after later status"), LaterResult.bOk);
	TestEqual(TEXT("76 event-time eligibility is authoritative"), LaterStatus.GetUnitById(SableAId)->ATK, 2);

	FWBGameStateData Retroactive = MakeState(TEXT("fixture_observer"), TEXT("friendly_csn_subject"), 0, 0);
	Retroactive.GetMutableUnitById(SableAId)->RemoveUnitFromBoard();
	TestTrue(TEXT("77 death without observer commits"), WBDeathResolution::ApplyZeroHPDeathResolution(
		Retroactive, EWBUnitDestructionCause::ReplacementEffect).bOk);
	TestTrue(TEXT("78 historical event lacks later observer"),
		!SnapshotContainsSource(Retroactive.PendingUnitDestructionEvents[0], SableBId));
	FWBUnitState NewlySummoned = MakeUnit(SableBId, 0, TEXT("fixture_observer"), FWBTile(4, 4));
	Retroactive.AddUnitForTest(NewlySummoned);
	TestTrue(TEXT("79 historical event resolves"), WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
		Retroactive, Repository, 0, 0).bOk);
	TestEqual(TEXT("80 later summon does not grow"), Retroactive.GetUnitById(SableBId)->ATK, 1);

	FWBGameStateData RemovedSource = MakeState();
	TestTrue(TEXT("81 source captured"), WBDeathResolution::ApplyZeroHPDeathResolution(
		RemovedSource, EWBUnitDestructionCause::EffectDamage).bOk);
	RemovedSource.GetMutableUnitById(SableAId)->RemoveUnitFromBoard();
	const FWBPostDestructionTriggerResult RemovedResult =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(RemovedSource, Repository, 0, 0);
	TestTrue(TEXT("82 unavailable source fails closed"), RemovedResult.bOk);
	TestEqual(TEXT("83 skipped trace"), CountTrace(RemovedResult.TraceEvents,
		FName(TEXT("post_destruction_observer_skipped"))), 1);
	TestEqual(TEXT("84 removed source unchanged"), RemovedSource.GetUnitById(SableAId)->ATK, 1);
	return true;
}

WB_SABLE_TEST(FWBCSNSableStackingTest,
	"Wandbound.CSNSable.Observer.MultipleSourcesDeathsAndSelfDestruction")
bool FWBCSNSableStackingTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(SableBId, 0, TEXT("fixture_observer"), FWBTile(2, 3)));
	State.AddUnitForTest(MakeUnit(SubjectBId, 0, TEXT("friendly_csn_subject"), FWBTile(3, 4), 0));
	TestTrue(TEXT("85 two deaths commit"), WBDeathResolution::ApplyZeroHPDeathResolution(
		State, EWBUnitDestructionCause::StatusDamage).bOk);
	TestEqual(TEXT("86 two events queued"), State.PendingUnitDestructionEvents.Num(), 2);
	const FWBPostDestructionTriggerResult Result =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(State, Repository, 0, 0);
	TestTrue(TEXT("87 all observers resolve"), Result.bOk);
	TestEqual(TEXT("88 Sable A ATK twice"), State.GetUnitById(SableAId)->ATK, 3);
	TestEqual(TEXT("89 Sable A HP twice"), State.GetUnitById(SableAId)->HP, 12);
	TestEqual(TEXT("90 Sable A MaxHP twice"), State.GetUnitById(SableAId)->MaxHP, 12);
	TestEqual(TEXT("91 Sable B ATK twice"), State.GetUnitById(SableBId)->ATK, 3);
	TestEqual(TEXT("92 Sable B HP twice"), State.GetUnitById(SableBId)->HP, 12);
	TestEqual(TEXT("93 Sable B MaxHP twice"), State.GetUnitById(SableBId)->MaxHP, 12);
	TestEqual(TEXT("94 four independent applications"), CountTrace(Result.TraceEvents,
		FName(TEXT("unit_stat_delta_applied"))), 4);

	FWBGameStateData SelfDeath = MakeState(TEXT("fixture_observer"), TEXT("fixture_observer"), 0, 0);
	SelfDeath.AddUnitForTest(MakeUnit(SableBId, 0, TEXT("fixture_observer"), FWBTile(2, 3)));
	const FWBPostDestructionTriggerResult SelfResult = DestroyAndAdvance(
		SelfDeath, Repository, EWBUnitDestructionCause::BattleDamage);
	TestTrue(TEXT("95 Sable destruction resolves"), SelfResult.bOk);
	TestTrue(TEXT("96 destroyed Sable is off board"), !SelfDeath.GetUnitById(SubjectAId)->IsUnitOnBoard());
	TestEqual(TEXT("97 original Sable observes destroyed Sable"), SelfDeath.GetUnitById(SableAId)->ATK, 2);
	TestEqual(TEXT("98 second surviving Sable observes"), SelfDeath.GetUnitById(SableBId)->ATK, 2);
	TestEqual(TEXT("99 no self application to removed source"), CountTrace(SelfResult.TraceEvents,
		FName(TEXT("unit_stat_delta_applied"))), 2);
	return true;
}

WB_SABLE_TEST(FWBCSNSableRookCompositionTest,
	"Wandbound.CSNSable.Composition.RookChoicePausesThenObserverResumes")
bool FWBCSNSableRookCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("fixture_observer"), TEXT("fixture_rook"), 0, 0);
	AddDeckCandidate(State);
	const FWBApplyActionResult Death = WBDeathResolution::ApplyZeroHPDeathResolution(
		State, EWBUnitDestructionCause::BattleDamage);
	TestTrue(TEXT("100 Rook destruction commits"), Death.bOk);
	TestTrue(TEXT("101 observer captured with Rook event"),
		SnapshotContainsSource(State.PendingUnitDestructionEvents[0], SableAId));
	const FWBPostDestructionTriggerResult Pending =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(State, Repository, 0, 0);
	TestTrue(TEXT("102 self trigger advances"), Pending.bOk);
	TestTrue(TEXT("103 Rook choice pending"), Pending.bPendingChoice);
	TestTrue(TEXT("104 mandatory choice state active"), State.HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("105 observer has not run before choice"), State.GetUnitById(SableAId)->ATK, 1);
	TestEqual(TEXT("106 event retained while choice pending"), State.PendingUnitDestructionEvents.Num(), 1);
	const TArray<FString> Actions = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(State, Repository);
	TestEqual(TEXT("107 one private exact choice"), Actions.Num(), 1);
	if (Actions.IsEmpty()) return false;
	const FWBPostDestructionTriggerResult Submitted =
		WBPostDestructionTrigger::SubmitChoice(State, Repository, Actions[0]);
	TestTrue(TEXT("108 explicit choice resolves"), Submitted.bOk);
	TestTrue(TEXT("109 Rook summon occurs"), Submitted.bSummoned);
	TestEqual(TEXT("110 observer grows after summon"), State.GetUnitById(SableAId)->ATK, 2);
	TestEqual(TEXT("111 observer growth exactly once"), CountTrace(Submitted.TraceEvents,
		FName(TEXT("unit_stat_delta_applied"))), 1);
	TestEqual(TEXT("112 event completes"), State.PendingUnitDestructionEvents.Num(), 0);
	TestTrue(TEXT("113 choice clears"), !State.HasPendingMandatoryDeckChoice());
	return true;
}

WB_SABLE_TEST(FWBCSNSableStatTransactionTest,
	"Wandbound.CSNSable.StatGrowth.TransactionalDirectIncreaseNotHealing")
bool FWBCSNSableStatTransactionTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	FWBUnitState* Source = State.GetMutableUnitById(SableAId);
	Source->ATK = 3;
	Source->HP = 4;
	Source->MaxHP = 12;
	FWBUnitStatDeltaRequest Request;
	Request.SourceUnitId = SableAId;
	Request.TargetUnitId = SableAId;
	Request.ATKDelta = 1;
	Request.MaxHPDelta = 1;
	Request.CurrentHPDelta = 1;
	Request.TransactionId = TEXT("stat_test");
	const FWBUnitStatDeltaResult Applied = WBUnitStatDelta::ApplyPersistentDelta(State, Request);
	TestTrue(TEXT("114 direct stat transaction succeeds"), Applied.bOk);
	TestEqual(TEXT("115 damaged ATK increments"), State.GetUnitById(SableAId)->ATK, 4);
	TestEqual(TEXT("116 damaged HP increments exactly one"), State.GetUnitById(SableAId)->HP, 5);
	TestEqual(TEXT("117 MaxHP increments first/result valid"), State.GetUnitById(SableAId)->MaxHP, 13);
	TestEqual(TEXT("118 one semantic trace"), Applied.TraceEvents.Num(), 1);
	if (!Applied.TraceEvents.IsEmpty())
	{
		TestEqual(TEXT("119 not a heal trace"), Applied.TraceEvents[0].Kind,
			FName(TEXT("unit_stat_delta_applied")));
		TestEqual(TEXT("120 previous ATK traced"), Applied.TraceEvents[0].PreviousATK, 3);
		TestEqual(TEXT("121 resulting ATK traced"), Applied.TraceEvents[0].NewATK, 4);
	}

	FWBGameStateData Failed = State;
	Failed.GetMutableUnitById(SableAId)->ATK = MAX_int32;
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(Failed);
	const FWBUnitStatDeltaResult Failure = WBUnitStatDelta::ApplyPersistentDelta(Failed, Request);
	TestTrue(TEXT("122 overflow fails"), !Failure.bOk);
	TestEqual(TEXT("123 failure applies no partial mutation"),
		WBProductionMatchReplay::BuildGameStateDigest(Failed), Before);
	TestTrue(TEXT("124 failure emits no partial trace"), Failure.TraceEvents.IsEmpty());
	return true;
}

WB_SABLE_TEST(FWBCSNSableDeterminismAndTerminalTest,
	"Wandbound.CSNSable.Replay.DeterminismTerminalAndSchema")
bool FWBCSNSableDeterminismAndTerminalTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData First = MakeState();
	FWBGameStateData Second = First;
	const FWBPostDestructionTriggerResult FirstResult = DestroyAndAdvance(
		First, Repository, EWBUnitDestructionCause::BattleDamage);
	const FWBPostDestructionTriggerResult SecondResult = DestroyAndAdvance(
		Second, Repository, EWBUnitDestructionCause::BattleDamage);
	TestTrue(TEXT("125 first deterministic run"), FirstResult.bOk);
	TestTrue(TEXT("126 second deterministic run"), SecondResult.bOk);
	TestEqual(TEXT("127 state digest deterministic"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("128 serialized trace deterministic"),
		WBReplayTrace::SerializeEvents(FirstResult.TraceEvents),
		WBReplayTrace::SerializeEvents(SecondResult.TraceEvents));
	TestEqual(TEXT("129 trace digest deterministic"),
		WBProductionMatchReplay::BuildTraceDigest(FirstResult.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(SecondResult.TraceEvents));
	TestEqual(TEXT("130 replay schema unchanged"), WBProductionMatchReplay::SchemaVersion, 1);

	FWBGameStateData Terminal = MakeState();
	Terminal.GetMutableUnitById(SubjectAId)->HP = 5;
	Terminal.GetMutableUnitById(HeroId)->HP = 0;
	const FWBApplyActionResult HeroDeath = WBDeathResolution::ApplyZeroHPDeathResolution(
		Terminal, EWBUnitDestructionCause::EffectDamage);
	TestTrue(TEXT("131 Hero destruction commits"), HeroDeath.bOk);
	TestTrue(TEXT("132 terminal wins"), Terminal.bGameOver);
	const FWBPostDestructionTriggerResult TerminalResult =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(Terminal, Repository, 0, 0);
	TestTrue(TEXT("133 terminal cleanup succeeds"), TerminalResult.bOk);
	TestEqual(TEXT("134 no post-terminal growth"), Terminal.GetUnitById(SableAId)->ATK, 1);
	TestTrue(TEXT("135 terminal clears queued events"), Terminal.PendingUnitDestructionEvents.IsEmpty());
	return true;
}

WB_SABLE_TEST(FWBCSNSableProductionSmokeTest,
	"Wandbound.CSNSable.Fixture.ProductionSmokeAndFreshReplay")
bool FWBCSNSableProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNSableFixture/match_spec.json"));
	const FWBProductionCSNCrashInSmokeResult First =
		WBProductionCSNCrashInSmoke::RunSable(Request);
	const FWBProductionCSNCrashInSmokeResult Second =
		WBProductionCSNCrashInSmoke::RunSable(Request);
	if (!First.bOk) AddError(TEXT("First production Sable smoke failed: ") + First.Reason);
	if (!Second.bOk) AddError(TEXT("Second production Sable smoke failed: ") + Second.Reason);
	TestTrue(TEXT("136 first production smoke succeeds"), First.bOk);
	TestTrue(TEXT("137 second production smoke succeeds"), Second.bOk);
	TestTrue(TEXT("138 fresh replay verifies actions"), First.RecordsVerified > 0);
	TestEqual(TEXT("139 archive deterministic"), First.SerializedArchive, Second.SerializedArchive);
	TestEqual(TEXT("140 receipt deterministic"), First.SerializedReceipt, Second.SerializedReceipt);
	TestEqual(TEXT("141 state digest deterministic"), First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("142 trace digest deterministic"), First.FinalTraceDigest, Second.FinalTraceDigest);
	return true;
}

#endif
