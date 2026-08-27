#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBCardDefinitionRepository.h"
#include "WBEffectRunner.h"
#include "WBPreDamageAttackTrigger.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionMarrowBlackcoinBouncerSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBReplayTrace.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 AttackerId = 10;
constexpr int32 BlackcoinId = 20;
constexpr int32 BodyDoubleId = 30;

FWBPreDamageAttackTriggerDefinition MakeBlackcoinTrigger()
{
	FWBPreDamageAttackTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("when_attacked_coin_reflect_or_bonus");
	Trigger.SourceRole = EWBPreDamageAttackTriggerSourceRole::CurrentDefender;
	Trigger.Timing =
		EWBPreDamageAttackTriggerTiming::AfterPreHitBeforeCalculateDamage;
	Trigger.RandomBranch = EWBDeterministicRandomBranchKind::CoinFlip;
	Trigger.Heads.Operation =
		EWBPendingBattleHitModifierOperation::ReflectToAttacker;
	Trigger.Tails.Operation =
		EWBPendingBattleHitModifierOperation::AddRawDamage;
	Trigger.Tails.Amount = 3;
	Trigger.bMandatory = true;
	Trigger.bOncePerTurn = true;
	return Trigger;
}

FWBCardDefinition MakeCharacterDefinition(
	const FString& CardId,
	const bool bHasBlackcoinTrigger = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 15;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 2;
	Definition.CharacterStats.RL = 3;
	if (bHasBlackcoinTrigger)
	{
		Definition.PreDamageAttackTriggers.Add(MakeBlackcoinTrigger());
	}
	return Definition;
}

FWBCardDefinitionRepository MakeRepository()
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("blackcoin_test_repository");
	Repository.SourceVersion = TEXT("1");
	Repository.Definitions = {
		MakeCharacterDefinition(TEXT("attacker")),
		MakeCharacterDefinition(TEXT("char_marrow_blackcoin_bouncer"), true),
		MakeCharacterDefinition(TEXT("alternate_blackcoin_identity"), true),
		MakeCharacterDefinition(TEXT("blackcoin_name_only")),
		MakeCharacterDefinition(TEXT("body_double"))
	};
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 HP,
	const int32 ATK,
	const int32 Armor = 0)
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
	Unit.AR = 8;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(Armor, Armor);
	return Unit;
}

FWBGameStateData MakeState(
	const FString& DefenderCardId = TEXT("char_marrow_blackcoin_bouncer"),
	const bool bCounter = false,
	const EWBAttackAuthorityKind Authority = EWBAttackAuthorityKind::Player)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 4;
	State.Phase = EWBGamePhase::Response;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = AttackerId;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = BlackcoinId;
	State.Players = { Player0, Player1 };

	State.AddUnitForTest(MakeUnit(
		AttackerId, 0, TEXT("attacker"), FWBTile(4, 4), 12, 4, 1));
	State.AddUnitForTest(MakeUnit(
		BlackcoinId, 1, DefenderCardId, FWBTile(4, 1), 15, 2, 2));
	State.AddUnitForTest(MakeUnit(
		BodyDoubleId, 1, TEXT("body_double"), FWBTile(3, 1), 9, 1, 6));

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.AuthorityKind = Authority;
	Pending.Stage = EWBAttackContinuationStage::AutomaticPreDamageModifiers;
	Pending.AttackerUnitId = AttackerId;
	Pending.DefenderUnitId = BlackcoinId;
	Pending.OriginalAttackerUnitId = AttackerId;
	Pending.OriginalDefenderUnitId = BlackcoinId;
	Pending.AttackingPlayerId = Authority == EWBAttackAuthorityKind::NeutralNPC
		? INDEX_NONE : 0;
	Pending.AttackerTile = FWBTile(4, 4);
	Pending.DefenderTile = FWBTile(4, 1);
	Pending.DeclarationActionId = TEXT("attack:p0:u10:t20");
	Pending.ContinuationId = bCounter
		? TEXT("blackcoin_counter") : TEXT("blackcoin_attack");
	Pending.bCounter = bCounter;
	State.SetPendingAttackForTest(Pending);
	return State;
}

const FWBTraceEvent* FindTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.FindByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

bool ResolveCalculatedDamage(
	FWBGameStateData& State,
	TArray<FWBTraceEvent>& OutTrace)
{
	const FWBApplyActionResult Calculate =
		WBEffectRunner::CalculatePendingAttackDamage(State);
	OutTrace.Append(Calculate.TraceEvents);
	if (!Calculate.bOk)
	{
		return false;
	}
	const FWBApplyActionResult Substitute =
		WBEffectRunner::ResolvePendingAttackDamageSubstitution(State);
	OutTrace.Append(Substitute.TraceEvents);
	if (!Substitute.bOk)
	{
		return false;
	}
	const FWBApplyActionResult Apply =
		WBEffectRunner::ApplyCalculatedPendingAttackDamage(State, true);
	OutTrace.Append(Apply.TraceEvents);
	return Apply.bOk;
}
}

#define WB_BLACKCOIN_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_BLACKCOIN_TEST(FWBBlackcoinProductionDefinitionTest,
	"Wandbound.MarrowBlackcoinBouncer.CardDB.ProductionDefinition")
bool FWBBlackcoinProductionDefinitionTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json")));
	TestTrue(TEXT("Production suite loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid()) return false;
	AddInfo(TEXT("Production Blackcoin bundle digest: ")
		+ Loaded.Snapshot->ContentDigest);
	const FWBProductionCardRecord* Record = Loaded.Snapshot->FindRecord(
		TEXT("char_marrow_blackcoin_bouncer"));
	TestNotNull(TEXT("Blackcoin loads"), Record);
	if (Record == nullptr) return false;
	const FWBCardDefinition& Definition = Record->CoreDefinition;
	TestEqual(TEXT("Public name"), Definition.PublicName,
		FString(TEXT("Marrow Blackcoin Bouncer")));
	TestEqual(TEXT("HP"), Definition.CharacterStats.HP, 15);
	TestEqual(TEXT("ATK"), Definition.CharacterStats.ATK, 2);
	TestEqual(TEXT("AR"), Definition.CharacterStats.AR, 2);
	TestEqual(TEXT("RL"), Definition.CharacterStats.RL, 3);
	TestTrue(TEXT("Marrow faction"),
		Definition.PublicFactions.Contains(TEXT("marrow_syndicate")));
	TestEqual(TEXT("One generic pre-damage trigger"),
		Definition.PreDamageAttackTriggers.Num(), 1);
	if (Definition.PreDamageAttackTriggers.Num() != 1) return false;
	const FWBPreDamageAttackTriggerDefinition& Trigger =
		Definition.PreDamageAttackTriggers[0];
	TestTrue(TEXT("Mandatory"), Trigger.bMandatory);
	TestTrue(TEXT("Once per turn"), Trigger.bOncePerTurn);
	TestEqual(TEXT("Current defender source"),
		static_cast<int32>(Trigger.SourceRole),
		static_cast<int32>(EWBPreDamageAttackTriggerSourceRole::CurrentDefender));
	TestEqual(TEXT("Typed checkpoint"),
		static_cast<int32>(Trigger.Timing),
		static_cast<int32>(
			EWBPreDamageAttackTriggerTiming::AfterPreHitBeforeCalculateDamage));
	TestEqual(TEXT("Heads reflects"),
		static_cast<int32>(Trigger.Heads.Operation),
		static_cast<int32>(EWBPendingBattleHitModifierOperation::ReflectToAttacker));
	TestEqual(TEXT("Tails adds damage"),
		static_cast<int32>(Trigger.Tails.Operation),
		static_cast<int32>(EWBPendingBattleHitModifierOperation::AddRawDamage));
	TestEqual(TEXT("Tails adds three"), Trigger.Tails.Amount, 3);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinHeadsTest,
	"Wandbound.MarrowBlackcoinBouncer.Damage.HeadsReflectsBattleHit")
bool FWBBlackcoinHeadsTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	uint32 RandomState = 1;
	const FWBPreDamageAttackTriggerResult Trigger =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestTrue(TEXT("Trigger resolves"), Trigger.bOk);
	TestEqual(TEXT("One trigger resolves"), Trigger.ResolvedTriggerCount, 1);
	TestEqual(TEXT("Heads targets attacker"),
		State.PendingAttack.DefenderUnitId, AttackerId);
	TestEqual(TEXT("Original defender remains Blackcoin"),
		State.PendingAttack.OriginalDefenderUnitId, BlackcoinId);
	TestTrue(TEXT("Reflection is recorded"),
		State.PendingAttack.bPendingBattleHitReflectedToAttacker);
	TestTrue(TEXT("Counter is suppressed"),
		State.PendingAttack.bCounterSuppressedByPendingHitTransform);
	const FWBTraceEvent* RandomTrace = FindTrace(
		Trigger.TraceEvents, FName(TEXT("random_branch_resolved")));
	TestNotNull(TEXT("Random trace emitted"), RandomTrace);
	if (RandomTrace != nullptr)
	{
		TestEqual(TEXT("Heads is public outcome"), RandomTrace->RandomOutcome,
			FName(TEXT("heads")));
		TestEqual(TEXT("Stable source identity"),
			RandomTrace->SourceUnitId, BlackcoinId);
	}
	const int32 AttackerHPBefore = State.GetUnitById(AttackerId)->HP;
	const int32 BlackcoinHPBefore = State.GetUnitById(BlackcoinId)->HP;
	TArray<FWBTraceEvent> DamageTrace;
	TestTrue(TEXT("Damage pipeline resolves"),
		ResolveCalculatedDamage(State, DamageTrace));
	TestEqual(TEXT("Attacker armor is consumed"),
		State.GetUnitById(AttackerId)->GetCurrentArmor(), 0);
	TestEqual(TEXT("Attacker takes remaining battle damage"),
		State.GetUnitById(AttackerId)->HP, AttackerHPBefore - 3);
	TestEqual(TEXT("Blackcoin is untouched"),
		State.GetUnitById(BlackcoinId)->HP, BlackcoinHPBefore);
	TestEqual(TEXT("Calculation hit is attacker"),
		State.PendingAttack.DamageCalculation.HitUnitId, AttackerId);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinTailsTest,
	"Wandbound.MarrowBlackcoinBouncer.Damage.TailsAddsThreeBeforeArmor")
bool FWBBlackcoinTailsTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	uint32 RandomState = 0;
	const FWBPreDamageAttackTriggerResult Trigger =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestTrue(TEXT("Trigger resolves"), Trigger.bOk);
	TestEqual(TEXT("Tails keeps Blackcoin as hit unit"),
		State.PendingAttack.DefenderUnitId, BlackcoinId);
	TestEqual(TEXT("Raw damage modifier is three"),
		State.PendingAttack.RawDamageModifier, 3);
	TestFalse(TEXT("No reflection"),
		State.PendingAttack.bPendingBattleHitReflectedToAttacker);
	const int32 HPBefore = State.GetUnitById(BlackcoinId)->HP;
	TArray<FWBTraceEvent> DamageTrace;
	TestTrue(TEXT("Damage pipeline resolves"),
		ResolveCalculatedDamage(State, DamageTrace));
	TestEqual(TEXT("Raw battle damage includes bonus"),
		State.PendingAttack.DamageCalculation.RawAttackDamage, 7);
	TestEqual(TEXT("Armor absorbs two before HP"),
		State.GetUnitById(BlackcoinId)->GetCurrentArmor(), 0);
	TestEqual(TEXT("Blackcoin takes five HP damage"),
		State.GetUnitById(BlackcoinId)->HP, HPBefore - 5);
	const FWBTraceEvent* RandomTrace = FindTrace(
		Trigger.TraceEvents, FName(TEXT("random_branch_resolved")));
	TestNotNull(TEXT("Random trace emitted"), RandomTrace);
	if (RandomTrace != nullptr)
	{
		TestEqual(TEXT("Tails outcome"), RandomTrace->RandomOutcome,
			FName(TEXT("tails")));
		TestEqual(TEXT("Trace reports modifier"), RandomTrace->DamageAmount, 3);
	}
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinEligibilityTest,
	"Wandbound.MarrowBlackcoinBouncer.Eligibility.TimingStatusAndRemovalFailClosed")
bool FWBBlackcoinEligibilityTest::RunTest(const FString&)
{
	for (const FName Status : {
		FName(TEXT("Stunned")), FName(TEXT("Frozen")), FName(TEXT("Negated")) })
	{
		FWBGameStateData State = MakeState();
		State.GetMutableUnitById(BlackcoinId)->AddStatus(Status, 1);
		uint32 RandomState = 7;
		const FWBPreDamageAttackTriggerResult Result =
			WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
		TestTrue(*FString::Printf(TEXT("%s skips successfully"), *Status.ToString()),
			Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s does not consume RNG"), *Status.ToString()),
			static_cast<int64>(RandomState), static_cast<int64>(7));
		TestEqual(*FString::Printf(TEXT("%s does not resolve trigger"), *Status.ToString()),
			Result.ResolvedTriggerCount, 0);
	}

	FWBGameStateData Prevented = MakeState();
	Prevented.PendingAttack.bPrevented = true;
	uint32 PreventedRandom = 9;
	const FWBPreDamageAttackTriggerResult PreventedResult =
		WBPreDamageAttackTrigger::Resolve(
			Prevented, MakeRepository(), PreventedRandom);
	TestTrue(TEXT("Prevented hit skips successfully"), PreventedResult.bOk);
	TestEqual(TEXT("Prevented hit does not consume RNG"),
		static_cast<int64>(PreventedRandom), static_cast<int64>(9));

	FWBGameStateData WrongStage = MakeState();
	WrongStage.PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	uint32 WrongStageRandom = 11;
	const FWBPreDamageAttackTriggerResult WrongStageResult =
		WBPreDamageAttackTrigger::Resolve(
			WrongStage, MakeRepository(), WrongStageRandom);
	TestFalse(TEXT("Wrong stage fails closed"), WrongStageResult.bOk);
	TestEqual(TEXT("Wrong stage does not consume RNG"),
		static_cast<int64>(WrongStageRandom), static_cast<int64>(11));

	FWBGameStateData Removed = MakeState();
	Removed.GetMutableUnitById(BlackcoinId)->RemoveUnitFromBoard();
	uint32 RemovedRandom = 13;
	const FWBPreDamageAttackTriggerResult RemovedResult =
		WBPreDamageAttackTrigger::Resolve(Removed, MakeRepository(), RemovedRandom);
	TestFalse(TEXT("Removed current defender fails closed"), RemovedResult.bOk);
	TestEqual(TEXT("Removed defender does not consume RNG"),
		static_cast<int64>(RemovedRandom), static_cast<int64>(13));

	FWBGameStateData RestoredEligibility = MakeState();
	RestoredEligibility.GetMutableUnitById(BlackcoinId)->AddStatus(
		FName(TEXT("Stunned")), 1);
	RestoredEligibility.GetMutableUnitById(BlackcoinId)->RemoveStatus(
		FName(TEXT("Stunned")));
	uint32 RestoredRandom = 1;
	const FWBPreDamageAttackTriggerResult RestoredResult =
		WBPreDamageAttackTrigger::Resolve(
			RestoredEligibility, MakeRepository(), RestoredRandom);
	TestEqual(TEXT("Removed suppression allows an unused later trigger"),
		RestoredResult.ResolvedTriggerCount, 1);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinDefinitionDrivenTest,
	"Wandbound.MarrowBlackcoinBouncer.DefinitionDriven.AlternateIdentityAndNameOnly")
bool FWBBlackcoinDefinitionDrivenTest::RunTest(const FString&)
{
	FWBGameStateData Alternate = MakeState(TEXT("alternate_blackcoin_identity"));
	uint32 AlternateRandom = 1;
	const FWBPreDamageAttackTriggerResult AlternateResult =
		WBPreDamageAttackTrigger::Resolve(
			Alternate, MakeRepository(), AlternateRandom);
	TestTrue(TEXT("Alternate metadata identity resolves"), AlternateResult.bOk);
	TestEqual(TEXT("Alternate metadata triggers"),
		AlternateResult.ResolvedTriggerCount, 1);

	FWBGameStateData NameOnly = MakeState(TEXT("blackcoin_name_only"));
	uint32 NameOnlyRandom = 1;
	const FWBPreDamageAttackTriggerResult NameOnlyResult =
		WBPreDamageAttackTrigger::Resolve(NameOnly, MakeRepository(), NameOnlyRandom);
	TestTrue(TEXT("No metadata skips successfully"), NameOnlyResult.bOk);
	TestEqual(TEXT("Name alone does not trigger"),
		NameOnlyResult.ResolvedTriggerCount, 0);
	TestEqual(TEXT("Name alone does not consume RNG"),
		static_cast<int64>(NameOnlyRandom), static_cast<int64>(1));
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinCurrentDefenderTest,
	"Wandbound.MarrowBlackcoinBouncer.Timing.CurrentDefenderAfterPreHitIsAuthoritative")
bool FWBBlackcoinCurrentDefenderTest::RunTest(const FString&)
{
	FWBGameStateData ReplacedOut = MakeState(TEXT("blackcoin_name_only"));
	uint32 ReplacedOutRandom = 1;
	const FWBPreDamageAttackTriggerResult ReplacedOutResult =
		WBPreDamageAttackTrigger::Resolve(
			ReplacedOut, MakeRepository(), ReplacedOutRandom);
	TestTrue(TEXT("Replacement-out continuation succeeds"), ReplacedOutResult.bOk);
	TestEqual(TEXT("Replaced Blackcoin does not flip"),
		ReplacedOutResult.ResolvedTriggerCount, 0);
	TestEqual(TEXT("Replacement-out does not consume RNG"),
		static_cast<int64>(ReplacedOutRandom), static_cast<int64>(1));
	const FString ReplacedOutUsage = WBPreDamageAttackTrigger::BuildUsageKey(
		BlackcoinId, TEXT("when_attacked_coin_reflect_or_bonus"),
		ReplacedOut.TurnNumber);
	TestFalse(TEXT("Replacement-out does not spend usage"),
		ReplacedOut.HasActivationUsageKeyThisTurn(1, ReplacedOutUsage));

	FWBGameStateData ReplacedIn = MakeState(TEXT("alternate_blackcoin_identity"));
	ReplacedIn.PendingAttack.OriginalDefenderUnitId = BodyDoubleId;
	uint32 ReplacedInRandom = 1;
	const FWBPreDamageAttackTriggerResult ReplacedInResult =
		WBPreDamageAttackTrigger::Resolve(
			ReplacedIn, MakeRepository(), ReplacedInRandom);
	TestTrue(TEXT("Replacement-in continuation succeeds"), ReplacedInResult.bOk);
	TestEqual(TEXT("Current eligible Blackcoin flips"),
		ReplacedInResult.ResolvedTriggerCount, 1);
	TestEqual(TEXT("Immutable original defender is preserved"),
		ReplacedIn.PendingAttack.OriginalDefenderUnitId, BodyDoubleId);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinMultipleSourceTest,
	"Wandbound.MarrowBlackcoinBouncer.Usage.MultipleExactSourcesRemainIndependent")
bool FWBBlackcoinMultipleSourceTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	const int32 SecondBlackcoinId = 21;
	State.AddUnitForTest(MakeUnit(
		SecondBlackcoinId,
		1,
		TEXT("char_marrow_blackcoin_bouncer"),
		FWBTile(2, 1),
		15,
		2));
	uint32 RandomState = 0;
	const FWBPreDamageAttackTriggerResult First =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestEqual(TEXT("First exact source flips"), First.ResolvedTriggerCount, 1);

	State.PendingAttack = MakeState().PendingAttack;
	State.PendingAttack.DefenderUnitId = SecondBlackcoinId;
	State.PendingAttack.OriginalDefenderUnitId = SecondBlackcoinId;
	State.PendingAttack.DefenderTile = FWBTile(2, 1);
	const FWBPreDamageAttackTriggerResult Second =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestEqual(TEXT("Second exact source also flips"), Second.ResolvedTriggerCount, 1);
	const FString FirstKey = WBPreDamageAttackTrigger::BuildUsageKey(
		BlackcoinId, TEXT("when_attacked_coin_reflect_or_bonus"), State.TurnNumber);
	const FString SecondKey = WBPreDamageAttackTrigger::BuildUsageKey(
		SecondBlackcoinId, TEXT("when_attacked_coin_reflect_or_bonus"),
		State.TurnNumber);
	TestTrue(TEXT("First exact source key is marked"),
		State.HasActivationUsageKeyThisTurn(1, FirstKey));
	TestTrue(TEXT("Second exact source key is marked"),
		State.HasActivationUsageKeyThisTurn(1, SecondKey));
	TestNotEqual(TEXT("Source keys do not collapse by CardId"), FirstKey, SecondKey);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinLethalDamageTest,
	"Wandbound.MarrowBlackcoinBouncer.Death.HeadsAndTailsUseCanonicalHeroAuthority")
bool FWBBlackcoinLethalDamageTest::RunTest(const FString&)
{
	FWBGameStateData Heads = MakeState();
	FWBUnitState* HeadsAttacker = Heads.GetMutableUnitById(AttackerId);
	HeadsAttacker->HP = 3;
	HeadsAttacker->MaxHP = 3;
	HeadsAttacker->SetArmorForTest(0, 0);
	uint32 HeadsRandom = 1;
	TestTrue(TEXT("Lethal Heads trigger resolves"),
		WBPreDamageAttackTrigger::Resolve(
			Heads, MakeRepository(), HeadsRandom).bOk);
	TArray<FWBTraceEvent> HeadsTrace;
	TestTrue(TEXT("Lethal Heads damage resolves"),
		ResolveCalculatedDamage(Heads, HeadsTrace));
	TestTrue(TEXT("Attacking Hero is defeated"),
		Heads.GetUnitById(AttackerId)->bDefeated);
	TestTrue(TEXT("Heads commits terminal Hero loss"),
		Heads.TerminalOutcome.bTerminal);
	TestEqual(TEXT("Defender owner wins Heads lethal"),
		Heads.TerminalOutcome.WinnerPlayerId, 1);

	FWBGameStateData Tails = MakeState();
	FWBUnitState* TailsBlackcoin = Tails.GetMutableUnitById(BlackcoinId);
	TailsBlackcoin->HP = 5;
	TailsBlackcoin->MaxHP = 5;
	TailsBlackcoin->SetArmorForTest(0, 0);
	uint32 TailsRandom = 0;
	TestTrue(TEXT("Lethal Tails trigger resolves"),
		WBPreDamageAttackTrigger::Resolve(
			Tails, MakeRepository(), TailsRandom).bOk);
	TArray<FWBTraceEvent> TailsTrace;
	TestTrue(TEXT("Lethal Tails damage resolves"),
		ResolveCalculatedDamage(Tails, TailsTrace));
	TestTrue(TEXT("Blackcoin Hero is defeated"),
		Tails.GetUnitById(BlackcoinId)->bDefeated);
	TestTrue(TEXT("Tails commits terminal Hero loss"),
		Tails.TerminalOutcome.bTerminal);
	TestEqual(TEXT("Attacker owner wins Tails lethal"),
		Tails.TerminalOutcome.WinnerPlayerId, 0);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinOncePerTurnTest,
	"Wandbound.MarrowBlackcoinBouncer.Usage.OncePerTurnPerExactSource")
bool FWBBlackcoinOncePerTurnTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	uint32 RandomState = 0;
	const FWBPreDamageAttackTriggerResult First =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestEqual(TEXT("First attack resolves trigger"), First.ResolvedTriggerCount, 1);
	const uint32 AfterFirst = RandomState;
	const FWBPreDamageAttackTriggerResult Reentry =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestFalse(TEXT("Same continuation cannot process twice"), Reentry.bOk);
	TestEqual(TEXT("Reentry does not consume RNG"),
		static_cast<int64>(RandomState), static_cast<int64>(AfterFirst));
	const FString UsageKey = WBPreDamageAttackTrigger::BuildUsageKey(
		BlackcoinId, TEXT("when_attacked_coin_reflect_or_bonus"), State.TurnNumber);
	TestTrue(TEXT("Exact source usage is marked"),
		State.HasActivationUsageKeyThisTurn(1, UsageKey));

	State.PendingAttack = MakeState().PendingAttack;
	const FWBPreDamageAttackTriggerResult Second =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestTrue(TEXT("Second attack continues"), Second.bOk);
	TestEqual(TEXT("Second attack does not trigger"), Second.ResolvedTriggerCount, 0);
	TestEqual(TEXT("Second attack does not consume RNG"),
		static_cast<int64>(RandomState), static_cast<int64>(AfterFirst));

	++State.TurnNumber;
	State.PendingAttack = MakeState().PendingAttack;
	const FWBPreDamageAttackTriggerResult NextTurn =
		WBPreDamageAttackTrigger::Resolve(State, MakeRepository(), RandomState);
	TestEqual(TEXT("Next turn triggers again"), NextTurn.ResolvedTriggerCount, 1);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinBodyDoubleTest,
	"Wandbound.MarrowBlackcoinBouncer.Interaction.BodyDoubleHeadsBypassesTailsTransfers")
bool FWBBlackcoinBodyDoubleTest::RunTest(const FString&)
{
	FWBGameStateData Heads = MakeState();
	Heads.PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	TestTrue(TEXT("Heads substitution registers"),
		WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
			Heads, Heads.PendingAttack.ContinuationId, BodyDoubleId).bOk);
	Heads.PendingAttack.Stage =
		EWBAttackContinuationStage::AutomaticPreDamageModifiers;
	uint32 HeadsRandom = 1;
	TestTrue(TEXT("Heads trigger resolves"),
		WBPreDamageAttackTrigger::Resolve(
			Heads, MakeRepository(), HeadsRandom).bOk);
	TArray<FWBTraceEvent> HeadsTrace;
	TestTrue(TEXT("Heads damage resolves"),
		ResolveCalculatedDamage(Heads, HeadsTrace));
	TestEqual(TEXT("Heads final recipient is attacker"),
		Heads.PendingAttack.FinalDamageRecipientUnitId, AttackerId);
	TestEqual(TEXT("Body Double is untouched on Heads"),
		Heads.GetUnitById(BodyDoubleId)->HP, 9);

	FWBGameStateData Tails = MakeState();
	Tails.PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	TestTrue(TEXT("Tails substitution registers"),
		WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
			Tails, Tails.PendingAttack.ContinuationId, BodyDoubleId).bOk);
	Tails.PendingAttack.Stage =
		EWBAttackContinuationStage::AutomaticPreDamageModifiers;
	uint32 TailsRandom = 0;
	TestTrue(TEXT("Tails trigger resolves"),
		WBPreDamageAttackTrigger::Resolve(
			Tails, MakeRepository(), TailsRandom).bOk);
	TArray<FWBTraceEvent> TailsTrace;
	TestTrue(TEXT("Tails damage resolves"),
		ResolveCalculatedDamage(Tails, TailsTrace));
	TestEqual(TEXT("Tails final recipient is Body Double"),
		Tails.PendingAttack.FinalDamageRecipientUnitId, BodyDoubleId);
	TestEqual(TEXT("Transferred HP damage uses Blackcoin armor"),
		Tails.GetUnitById(BodyDoubleId)->HP, 4);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinCounterAndNPCTest,
	"Wandbound.MarrowBlackcoinBouncer.Authority.CounterAndNPCUseSamePipeline")
bool FWBBlackcoinCounterAndNPCTest::RunTest(const FString&)
{
	FWBGameStateData Counter = MakeState(TEXT("char_marrow_blackcoin_bouncer"), true);
	uint32 CounterRandom = 1;
	const FWBPreDamageAttackTriggerResult CounterResult =
		WBPreDamageAttackTrigger::Resolve(Counter, MakeRepository(), CounterRandom);
	TestTrue(TEXT("Counter trigger resolves"), CounterResult.bOk);
	TestEqual(TEXT("Counter trigger count"), CounterResult.ResolvedTriggerCount, 1);
	TestTrue(TEXT("Counter identity remains traceable"),
		CounterResult.TraceEvents[0].bCounterAttack);

	FWBGameStateData NPC = MakeState(
		TEXT("char_marrow_blackcoin_bouncer"), false,
		EWBAttackAuthorityKind::NeutralNPC);
	uint32 NPCRandom = 0;
	const FWBPreDamageAttackTriggerResult NPCResult =
		WBPreDamageAttackTrigger::Resolve(NPC, MakeRepository(), NPCRandom);
	TestTrue(TEXT("NPC trigger resolves"), NPCResult.bOk);
	TestEqual(TEXT("NPC trigger count"), NPCResult.ResolvedTriggerCount, 1);
	TestEqual(TEXT("NPC Tails adds three"), NPC.PendingAttack.RawDamageModifier, 3);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinDeterminismAndPrivacyTest,
	"Wandbound.MarrowBlackcoinBouncer.Replay.DeterministicAndPrivacySafe")
bool FWBBlackcoinDeterminismAndPrivacyTest::RunTest(const FString&)
{
	FWBGameStateData First = MakeState();
	FWBGameStateData Second = MakeState();
	uint32 FirstRandom = 37;
	uint32 SecondRandom = 37;
	const FWBPreDamageAttackTriggerResult FirstResult =
		WBPreDamageAttackTrigger::Resolve(First, MakeRepository(), FirstRandom);
	const FWBPreDamageAttackTriggerResult SecondResult =
		WBPreDamageAttackTrigger::Resolve(Second, MakeRepository(), SecondRandom);
	TestTrue(TEXT("First resolves"), FirstResult.bOk);
	TestTrue(TEXT("Second resolves"), SecondResult.bOk);
	TestEqual(TEXT("RNG state matches"),
		static_cast<int64>(FirstRandom), static_cast<int64>(SecondRandom));
	TestEqual(TEXT("State digest matches"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	const FString FirstTrace =
		WBReplayTrace::SerializeEvents(FirstResult.TraceEvents);
	TestEqual(TEXT("Trace matches"), FirstTrace,
		WBReplayTrace::SerializeEvents(SecondResult.TraceEvents));
	TestTrue(TEXT("Public coin outcome is present"),
		FirstTrace.Contains(TEXT("random_outcome")));
	TestFalse(TEXT("No opponent hand leak"),
		FirstTrace.Contains(TEXT("opponent_hand"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("No private candidate leak"),
		FirstTrace.Contains(TEXT("candidate"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("No protected digest leak"),
		FirstTrace.Contains(TEXT("state_digest"), ESearchCase::IgnoreCase));
	TestEqual(TEXT("Replay schema unchanged"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_BLACKCOIN_TEST(FWBBlackcoinProductionSmokeTest,
	"Wandbound.MarrowBlackcoinBouncer.Runtime.ProductionHeadsTailsAndFreshReplay")
bool FWBBlackcoinProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/MarrowBlackcoinBouncerFixture/match_spec.json"));
	const FWBProductionMarrowBlackcoinBouncerSmokeResult Result =
		WBProductionMarrowBlackcoinBouncerSmoke::Run(Request);
	TestTrue(*FString::Printf(TEXT("Production smoke succeeds: %s"),
		*Result.Reason), Result.bOk);
	TestTrue(TEXT("Replay verifies records"), Result.RecordsVerified > 0);
	TestFalse(TEXT("State digest is present"), Result.FinalStateDigest.IsEmpty());
	TestFalse(TEXT("Trace digest is present"), Result.FinalTraceDigest.IsEmpty());
	TestTrue(TEXT("Archive is serialized"),
		Result.SerializedArchive.Contains(TEXT("WandboundProductionMatchReplay")));
	TestTrue(TEXT("Receipt is serialized"),
		Result.SerializedReceipt.Contains(TEXT("schema_version")));
	return Result.bOk;
}

#undef WB_BLACKCOIN_TEST

#endif
