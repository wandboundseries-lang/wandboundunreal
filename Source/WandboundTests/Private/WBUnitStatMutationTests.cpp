#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "WBArmorEffect.h"
#include "WBDeckSummon.h"
#include "WBProductionMatchReplay.h"
#include "WBPublicBoardSummary.h"
#include "WBUnitStatDelta.h"
#include "WBUnitStatMutation.h"
#include "WBUnitStatQuery.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using Stat = EWBStoredUnitStat;
using Op = EWBUnitStatMutationOp;

FWBGameStateData MakeStatMutationState()
{
	FWBGameStateData State;
	FWBUnitState Unit;
	Unit.UnitId = 10;
	Unit.SetOwnerAndControllerForRules(0, 0);
	Unit.CardId = TEXT("stat_subject");
	Unit.X = 2;
	Unit.Y = 2;
	Unit.ATK = 2;
	Unit.AR = 2;
	Unit.HP = 5;
	Unit.MaxHP = 10;
	Unit.SetArmorForTest(2, 4);
	Unit.SetCanonicalRL(3, 3, 1);
	State.AddUnitForTest(Unit);
	FWBPlayerStateData Player;
	Player.PlayerId = 0;
	State.Players.Add(Player);
	return State;
}

FWBUnitStatMutationRequest StatRequest(const TArray<FWBUnitStatMutationEntry>& Entries)
{
	FWBUnitStatMutationRequest Request;
	Request.TransactionId = TEXT("trigger:event7:source10:stat_growth");
	Request.TargetUnitId = 10;
	Request.Entries = Entries;
	return Request;
}

int32 StoredValue(const FWBGameStateData& State, Stat Which)
{
	const FWBUnitState& Unit = *State.GetUnitById(10);
	switch (Which)
	{
	case Stat::ATK: return Unit.ATK;
	case Stat::AR: return Unit.AR;
	case Stat::CurrentHP: return Unit.HP;
	case Stat::MaxHP: return Unit.MaxHP;
	case Stat::CurrentArmor: return Unit.CurrentArmor;
	case Stat::MaxArmor: return Unit.MaxArmor;
	default: return INDEX_NONE;
	}
}

FWBCardDefinition StatCharacter(const FString& Id, bool bAura = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = Id;
	Definition.PublicName = TEXT("Stat Subject");
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 10;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 2;
	Definition.CharacterStats.RL = 3;
	if (bAura)
	{
		FWBContinuousStatAuraDefinition Aura;
		Aura.AuraId = TEXT("enemy_ar");
		Aura.TargetRelation = EWBContinuousAuraTargetRelation::Enemy;
		Aura.TargetStat = EWBContinuousStat::AR;
		Aura.Operation = EWBContinuousStatOperation::Add;
		Aura.Amount = -1;
		Aura.RangeStat = EWBContinuousAuraRangeStat::AR;
		Aura.Geometry = EWBContinuousAuraGeometry::AttackLine;
		Aura.bBlockedByWalls = true;
		Aura.bBlockedByUnits = true;
		Aura.MinimumResult = 0;
		Definition.ContinuousStatAuras.Add(Aura);
	}
	return Definition;
}
}

#define WB_STAT_TEST(Class, Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, \
	"Wandbound.UnitStatMutation." Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_STAT_TEST(FWBStatScalarTest, "Scalar.AddSetAndBounds")
bool FWBStatScalarTest::RunTest(const FString&)
{
	struct FCase { Stat Which; Op Operation; int32 Amount; int32 Expected; };
	const FCase Cases[] = {
		{ Stat::ATK, Op::Add, 3, 5 }, { Stat::ATK, Op::Set, 7, 7 },
		{ Stat::AR, Op::Add, 1, 3 }, { Stat::AR, Op::Set, 5, 5 },
		{ Stat::MaxHP, Op::Add, 3, 13 }, { Stat::MaxHP, Op::Set, 4, 4 },
		{ Stat::CurrentHP, Op::Add, 20, 10 }, { Stat::CurrentHP, Op::Set, 8, 8 },
		{ Stat::CurrentArmor, Op::Add, 1, 3 }, { Stat::CurrentArmor, Op::Add, 20, 4 },
		{ Stat::CurrentArmor, Op::Add, -20, 0 }, { Stat::CurrentArmor, Op::Set, 1, 1 },
		{ Stat::MaxArmor, Op::Add, 2, 6 }, { Stat::MaxArmor, Op::Add, -20, 0 },
		{ Stat::MaxArmor, Op::Set, 1, 1 } };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
	{
		const FCase& C = Cases[Index];
		FWBGameStateData State = MakeStatMutationState();
		const auto Result = WBUnitStatMutation::Apply(State, StatRequest({ { C.Which, C.Operation, C.Amount } }));
		TestTrue(FString::Printf(TEXT("case %d succeeds"), Index), Result.bOk);
		TestEqual(FString::Printf(TEXT("case %d stored result"), Index), StoredValue(State, C.Which), C.Expected);
		if (C.Which == Stat::MaxHP)
			TestEqual(TEXT("max alone never heals, lowering clamps"), State.GetUnitById(10)->HP, FMath::Min(5, C.Expected));
		if (C.Which == Stat::MaxArmor)
			TestEqual(TEXT("max alone never refills, lowering clamps"), State.GetUnitById(10)->CurrentArmor, FMath::Min(2, C.Expected));
	}
	return true;
}

WB_STAT_TEST(FWBStatAtomicTest, "Atomic.FinalPairsAndEntryOrder")
bool FWBStatAtomicTest::RunTest(const FString&)
{
	FWBGameStateData First = MakeStatMutationState();
	First.GetMutableUnitById(10)->SetArmorForTest(0, 0);
	FWBGameStateData Second = First;
	auto Request = StatRequest({ { Stat::CurrentArmor, Op::Add, 2 }, { Stat::MaxArmor, Op::Add, 2 },
		{ Stat::CurrentHP, Op::Add, 7 }, { Stat::MaxHP, Op::Add, 1 },
		{ Stat::ATK, Op::Add, 1 }, { Stat::AR, Op::Set, 3 } });
	const auto A = WBUnitStatMutation::Apply(First, Request);
	Algo::Reverse(Request.Entries);
	const auto B = WBUnitStatMutation::Apply(Second, Request);
	TestTrue(TEXT("both transactions succeed"), A.bOk && B.bOk);
	TestEqual(TEXT("pair final max armor"), First.GetUnitById(10)->MaxArmor, 2);
	TestEqual(TEXT("pair final current armor"), First.GetUnitById(10)->CurrentArmor, 2);
	TestEqual(TEXT("HP uses final max"), First.GetUnitById(10)->HP, 11);
	TestEqual(TEXT("maxHP uses same prestate"), First.GetUnitById(10)->MaxHP, 11);
	TestEqual(TEXT("ATK commits with pair"), First.GetUnitById(10)->ATK, 3);
	TestEqual(TEXT("AR commits with pair"), First.GetUnitById(10)->AR, 3);
	TestEqual(TEXT("state order independent"), WBProductionMatchReplay::BuildGameStateDigest(First), WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("trace order independent"), WBReplayTrace::SerializeEvents(A.TraceEvents), WBReplayTrace::SerializeEvents(B.TraceEvents));
	TestEqual(TEXT("six stat events"), A.TraceEvents.Num(), 6);
	for (int32 Index = 0; Index < A.TraceEvents.Num(); ++Index)
	{
		TestEqual(TEXT("typed order"), A.TraceEvents[Index].StatId, WBUnitStatMutation::GetStatName(static_cast<Stat>(Index)));
		TestEqual(TEXT("one transaction"), A.TraceEvents[Index].ActionId, Request.TransactionId);
		TestEqual(TEXT("not damage or heal"), A.TraceEvents[Index].Kind, FName(TEXT("unit_stat_mutated")));
	}
	return true;
}

WB_STAT_TEST(FWBStatFailureTest, "Atomic.FailClosedAndOverflow")
bool FWBStatFailureTest::RunTest(const FString&)
{
	const TArray<FWBUnitStatMutationEntry> Invalid[] = {
		{ { Stat::ATK, Op::Set, -1 } }, { { Stat::AR, Op::Add, -3 } },
		{ { Stat::MaxHP, Op::Set, 0 } }, { { Stat::CurrentHP, Op::Add, -1 } },
		{ { Stat::CurrentHP, Op::Set, 0 } }, { { Stat::MaxArmor, Op::Set, -1 } },
		{ { Stat::CurrentArmor, Op::Set, -1 } }, { { Stat::ATK, Op::Add, MAX_int32 } },
		{ { Stat::AR, Op::Add, MIN_int32 }, { Stat::AR, Op::Add, -10 } },
		{ { Stat::ATK, Op::Set, 5 }, { Stat::ATK, Op::Add, 2 } },
		{ { Stat::ATK, Op::Set, 5 }, { Stat::ATK, Op::Set, 5 } },
		{ { Stat::Unsupported, Op::Add, 1 } },
		{ { static_cast<Stat>(255), Op::Set, 3 } },
		{ { Stat::ATK, static_cast<Op>(255), 3 } } };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Invalid); ++Index)
	{
		FWBGameStateData State = MakeStatMutationState();
		const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
		auto Request = StatRequest(Invalid[Index]);
		Request.Entries.Insert({ Stat::MaxHP, Op::Add, 2 }, 0);
		const auto Result = WBUnitStatMutation::Apply(State, Request);
		TestFalse(FString::Printf(TEXT("invalid case %d rejected"), Index), Result.bOk);
		TestEqual(TEXT("all stats, zones, resources unchanged"), WBProductionMatchReplay::BuildGameStateDigest(State), Before);
		TestTrue(TEXT("no partial success trace"), Result.TraceEvents.IsEmpty());
	}
	FWBGameStateData State = MakeStatMutationState();
	State.GetMutableUnitById(10)->ATK = MIN_int32;
	const auto Underflow = WBUnitStatMutation::Apply(State, StatRequest({ { Stat::ATK, Op::Add, -1 } }));
	TestEqual(TEXT("int32 underflow diagnosed before bounds"), Underflow.Reason, FString(TEXT("unit_stat_mutation_overflow")));
	State = MakeStatMutationState();
	const auto Adds = WBUnitStatMutation::Apply(State, StatRequest({ { Stat::ATK, Op::Add, MAX_int32 }, { Stat::ATK, Op::Add, -MAX_int32 }, { Stat::ATK, Op::Add, 3 } }));
	TestTrue(TEXT("compatible Adds aggregate before checking final int32"), Adds.bOk);
	TestEqual(TEXT("aggregate result"), State.GetUnitById(10)->ATK, 5);
	TestEqual(TEXT("aggregate single stat trace"), Adds.TraceEvents.Num(), 1);
	return true;
}

WB_STAT_TEST(FWBStatArmorAdapterTest, "Compatibility.SevenArmorOperations")
bool FWBStatArmorAdapterTest::RunTest(const FString&)
{
	struct FCase { EWBArmorEffectOp Operation; int32 Amount; int32 Current; int32 Max; };
	const FCase Cases[] = {
		{ EWBArmorEffectOp::AddCurrentArmor, 1, 3, 4 },
		{ EWBArmorEffectOp::ReduceCurrentArmor, 8, 0, 4 },
		{ EWBArmorEffectOp::SetCurrentArmor, 8, 4, 4 },
		{ EWBArmorEffectOp::AddMaxArmor, 2, 2, 6 },
		{ EWBArmorEffectOp::ReduceMaxArmor, 8, 0, 0 },
		{ EWBArmorEffectOp::SetMaxArmor, 1, 1, 1 },
		{ EWBArmorEffectOp::RestoreArmorToMax, 0, 4, 4 } };
	for (const FCase& C : Cases)
	{
		FWBGameStateData State = MakeStatMutationState();
		FWBArmorEffectRequest Request;
		Request.TargetUnitId = 10;
		Request.Operation = C.Operation;
		Request.Amount = C.Amount;
		const auto Result = WBArmorEffect::ApplyArmorEffect(State, Request);
		TestTrue(TEXT("legacy armor succeeds"), Result.bOk);
		TestEqual(TEXT("previous current"), Result.PreviousCurrentArmor, 2);
		TestEqual(TEXT("previous max"), Result.PreviousMaxArmor, 4);
		TestEqual(TEXT("current result"), Result.NewCurrentArmor, C.Current);
		TestEqual(TEXT("max result"), Result.NewMaxArmor, C.Max);
		TestEqual(TEXT("current committed"), State.GetUnitById(10)->CurrentArmor, C.Current);
		TestEqual(TEXT("max committed"), State.GetUnitById(10)->MaxArmor, C.Max);
	}
	return true;
}

WB_STAT_TEST(FWBStatSableAdapterTest, "Compatibility.SableTraceAndCommonPrestate")
bool FWBStatSableAdapterTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeStatMutationState();
	FWBUnitStatDeltaRequest Legacy;
	Legacy.SourceUnitId = Legacy.TargetUnitId = 10;
	Legacy.TransactionId = TEXT("observer:event7:source10");
	Legacy.ATKDelta = Legacy.MaxHPDelta = Legacy.CurrentHPDelta = 1;
	const auto Applied = WBUnitStatDelta::ApplyPersistentDelta(State, Legacy);
	TestTrue(TEXT("legacy adapter succeeds"), Applied.bOk);
	TestEqual(TEXT("Sable ATK"), State.GetUnitById(10)->ATK, 3);
	TestEqual(TEXT("Sable HP"), State.GetUnitById(10)->HP, 6);
	TestEqual(TEXT("Sable maxHP"), State.GetUnitById(10)->MaxHP, 11);
	FWBTraceEvent Expected;
	Expected.Kind = TEXT("unit_stat_delta_applied");
	Expected.ActionId = Legacy.TransactionId;
	Expected.PlayerId = 0;
	Expected.SourceUnitId = Expected.TargetUnitId = 10;
	Expected.PreviousATK = 2; Expected.NewATK = 3;
	Expected.PreviousHP = 5; Expected.NewHP = 6;
	Expected.PreviousMaxHP = 10; Expected.NewMaxHP = 11;
	Expected.bOk = true;
	TestEqual(TEXT("exact legacy serialized trace"), WBReplayTrace::SerializeEvents(Applied.TraceEvents), WBReplayTrace::SerializeEvents({ Expected }));
	TestEqual(TEXT("replay schema"), WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_STAT_TEST(FWBStatEffectiveTest, "AR.StoredHighgroundAndAuraComposition")
bool FWBStatEffectiveTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeStatMutationState();
	FWBCardDefinitionRepository Repository;
	TestTrue(TEXT("definitions build"), WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("stat_mutation"), TEXT("1"), { StatCharacter(TEXT("stat_subject"), true), StatCharacter(TEXT("enemy_aura"), true) }, Repository).bOk);
	FWBUnitState Enemy = *State.GetUnitById(10);
	Enemy.UnitId = 20; Enemy.CardId = TEXT("enemy_aura"); Enemy.Y = 4;
	Enemy.SetOwnerAndControllerForRules(1, 1);
	State.AddUnitForTest(Enemy);
	State.SetTerrainForTest(FWBTile(2, 2), TEXT("Highground"));
	TestTrue(TEXT("persistent AR mutation"), WBUnitStatMutation::Apply(State, StatRequest({ { Stat::AR, Op::Add, 1 } })).bOk);
	const auto Query = WBUnitStatQuery::GetEffectiveAR(State, Repository, 10);
	TestEqual(TEXT("stored AR"), Query.StoredValue, 3);
	TestEqual(TEXT("Highground adds to stored"), WBUnitStatQuery::GetIntrinsicAR(State, 10), 4);
	TestEqual(TEXT("Vex style aura applies on intrinsic"), Query.EffectiveValue, 3);
	TestEqual(TEXT("neither modifier baked in"), State.GetUnitById(10)->AR, 3);
	TestEqual(TEXT("source aura range excludes opposing aura"), WBUnitStatQuery::GetAuraRangeAR(State, 20), 2);
	TestEqual(TEXT("opposing source is affected without recursion"), WBUnitStatQuery::GetEffectiveAR(State, Repository, 20).EffectiveValue, 1);
	TestEqual(TEXT("public intrinsic summary without repository"), WBPublicBoardSummary::Build(State).Units[0].AR, 4);
	TestEqual(TEXT("public effective summary"), WBPublicBoardSummary::Build(State, Repository).Units[0].AR, 3);
	State.GetMutableUnitById(20)->RemoveUnitFromBoard();
	TestEqual(TEXT("removal removes only live aura"), WBUnitStatQuery::GetEffectiveAR(State, Repository, 10).EffectiveValue, 4);
	State.ClearTerrainForTest(FWBTile(2, 2));
	TestEqual(TEXT("terrain removal reveals persistent AR"), WBUnitStatQuery::GetEffectiveAR(State, Repository, 10).EffectiveValue, 3);
	TestTrue(TEXT("maximum stored AR accepted"), WBUnitStatMutation::Apply(State, StatRequest({ { Stat::AR, Op::Set, MAX_int32 } })).bOk);
	State.SetTerrainForTest(FWBTile(2, 2), TEXT("Highground"));
	TestEqual(TEXT("Highground never wraps maximum stored AR"), WBUnitStatQuery::GetEffectiveAR(State, Repository, 10).EffectiveValue, MAX_int32);
	return true;
}

WB_STAT_TEST(FWBStatContextTest, "Validation.ContextBoundsAndDuplicateDiagnostics")
bool FWBStatContextTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeStatMutationState();
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
	auto Request = StatRequest({ { Stat::ATK, Op::Add, 1 } });
	Request.TransactionId.Reset();
	TestEqual(TEXT("identity required"), WBUnitStatMutation::Apply(State, Request).Reason,
		FString(TEXT("unit_stat_mutation_context_invalid")));
	Request = StatRequest({});
	TestFalse(TEXT("empty transaction rejected"), WBUnitStatMutation::Apply(State, Request).bOk);
	Request = StatRequest({ { Stat::ATK, Op::Add, 1 } });
	Request.TargetUnitId = 999;
	TestEqual(TEXT("missing target"), WBUnitStatMutation::Apply(State, Request).Reason,
		FString(TEXT("unit_stat_mutation_target_unavailable")));
	const TArray<FWBUnitStatMutationEntry> Bounds[] = {
		{ { Stat::ATK, Op::Add, -3 } }, { { Stat::AR, Op::Set, -1 } },
		{ { Stat::MaxHP, Op::Add, -10 } }, { { Stat::MaxArmor, Op::Set, -1 } },
		{ { Stat::CurrentArmor, Op::Set, -1 } } };
	for (const auto& Entries : Bounds)
	{
		TestEqual(TEXT("bounds rejection"), WBUnitStatMutation::Apply(State, StatRequest(Entries)).Reason,
			FString(TEXT("unit_stat_mutation_result_invalid")));
	}
	Request = StatRequest({ { Stat::ATK, Op::Set, 5 }, { Stat::ATK, Op::Add, 2 } });
	const auto First = WBUnitStatMutation::Apply(State, Request);
	Algo::Reverse(Request.Entries);
	const auto Second = WBUnitStatMutation::Apply(State, Request);
	TestEqual(TEXT("mixed entries diagnostic"), First.Reason, FString(TEXT("unit_stat_mutation_ambiguous_entries")));
	TestEqual(TEXT("mixed entries reverse diagnostic"), First.Reason, Second.Reason);
	TestEqual(TEXT("all rejections unchanged"), WBProductionMatchReplay::BuildGameStateDigest(State), Before);
	FWBArmorEffectRequest Armor;
	Armor.TargetUnitId = 10; Armor.Operation = EWBArmorEffectOp::AddMaxArmor; Armor.Amount = MAX_int32;
	TestFalse(TEXT("legacy overflow now safely rejects"), WBArmorEffect::ApplyArmorEffect(State, Armor).bOk);
	TestEqual(TEXT("legacy overflow is atomic"), WBProductionMatchReplay::BuildGameStateDigest(State), Before);
	return true;
}

WB_STAT_TEST(FWBStatProvenanceTest, "Provenance.PrivacyAndDeferredRL")
bool FWBStatProvenanceTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeStatMutationState();
	auto Request = StatRequest({ { Stat::ATK, Op::Add, 1 } });
	FWBUnitState Source = *State.GetUnitById(10);
	Source.UnitId = 20;
	Request.Source = WBEventSnapshot::CaptureUnitSource(State, Source);
	Request.Source.SourceCardInstanceId = TEXT("private_instance_never_publish");
	Source.RemoveUnitFromBoard();
	const auto Result = WBUnitStatMutation::Apply(State, Request);
	TestTrue(TEXT("snapshot provenance does not require source lookup"), Result.bOk);
	TestEqual(TEXT("captured source preserved"), Result.TraceEvents[0].SourceUnitId, 20);
	TestEqual(TEXT("trigger is not caster"), Request.Source.GetCasterUnitId(), INDEX_NONE);
	TestFalse(TEXT("no declared activation fabricated"), Result.TraceEvents[0].bDeclaredActivation);
	TestFalse(TEXT("no declared target fabricated"), Result.TraceEvents[0].bDeclaredTarget);
	TestFalse(TEXT("no private instance in trace"), WBReplayTrace::SerializeEvents(Result.TraceEvents).Contains(TEXT("private_instance")));
	TestTrue(TEXT("source-less rule request allowed"), WBUnitStatMutation::Apply(State, StatRequest({ { Stat::ATK, Op::Add, 1 } })).bOk);
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
	for (const uint8 Unsupported : { uint8(6), uint8(7) })
	{
		const auto Rejected = WBUnitStatMutation::Apply(State, StatRequest({ { static_cast<Stat>(Unsupported), Op::Add, 2 } }));
		TestEqual(TEXT("RL identifiers not part of supported stat domain"), Rejected.Reason, FString(TEXT("unit_stat_mutation_unsupported_stat")));
		TestEqual(TEXT("RL and all state untouched"), WBProductionMatchReplay::BuildGameStateDigest(State), Before);
	}
	State.GetMutableUnitById(10)->RemoveUnitFromBoard();
	TestFalse(TEXT("removed target cannot mutate"), WBUnitStatMutation::Apply(State, Request).bOk);
	State = MakeStatMutationState();
	State.GetMutableUnitById(10)->bDefeated = true;
	TestFalse(TEXT("defeated target cannot mutate"), WBUnitStatMutation::Apply(State, Request).bOk);
	return true;
}

WB_STAT_TEST(FWBStatLifetimeTest, "Lifetime.DefinitionAndFreshSummon")
bool FWBStatLifetimeTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeStatMutationState();
	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(TEXT("stat_lifetime"), TEXT("1"),
		{ StatCharacter(TEXT("stat_subject")) }, Repository);
	TestTrue(TEXT("live mutation succeeds"), WBUnitStatMutation::Apply(State, StatRequest({ { Stat::ATK, Op::Add, 1 } })).bOk);
	TestEqual(TEXT("live unit changed"), State.GetUnitById(10)->ATK, 3);
	TestEqual(TEXT("definition unchanged"), WBCardDefinitionRepository::FindCardById(Repository, TEXT("stat_subject")).Definition.CharacterStats.ATK, 2);
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = 0;
	FWBZoneCardEntry Card;
	Card.Card.InstanceId = TEXT("new_copy"); Card.Card.CardId = TEXT("stat_subject"); Card.Card.OwnerPlayerId = 0;
	Card.Zone = EWBCardZone::Deck; Card.ZoneIndex = 0;
	Zones.Deck.Add(Card);
	State.GetMutableCardZoneStateForTest().PlayerZones.Add(Zones);
	FWBDeckSummonRequest Summon;
	Summon.PlayerId = 0;
	Summon.SelectedCardInstanceId = TEXT("new_copy");
	Summon.TargetTile = FWBTile(3, 2);
	Summon.TransactionId = TEXT("lifetime_summon");
	Summon.InheritanceSource.SourceUnitId = 10;
	Summon.InheritanceSource.SourceSnapshot = WBEventSnapshot::CaptureUnitParticipant(State, *State.GetUnitById(10));
	const auto Result = WBDeckSummon::SummonExactCharacterToTile(State, Repository, Summon);
	TestTrue(TEXT("production Deck summon succeeds: ") + Result.Reason, Result.bOk);
	if (Result.bOk)
	{
		TestEqual(TEXT("new instance starts definition ATK"), State.GetUnitById(Result.CreatedUnitId)->ATK, 2);
		TestEqual(TEXT("new instance starts definition HP"), State.GetUnitById(Result.CreatedUnitId)->HP, 10);
		TestEqual(TEXT("original live mutation retained only on original"), State.GetUnitById(10)->ATK, 3);
	}
	return true;
}
#undef WB_STAT_TEST
#endif
