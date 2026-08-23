#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "WBCardActivationCandidateGenerator.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBEffectRunner.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNCrashInSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBPostDestructionTrigger.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 AttackerId = 10;
constexpr int32 HeroId = 20;
constexpr int32 DefenderId = 30;

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const FString& Faction,
	const int32 HP,
	const int32 ATK,
	const int32 AR,
	const int32 RL)
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

FWBGenericEffectPayload MakeReplacementPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation =
		EWBGenericEffectOp::ReplacePendingAttackDefenderFromHand;
	Payload.PendingAttackContinuationId = TEXT("crash_in_continuation");
	Payload.RequiredSourceFaction = TEXT("csn");
	Payload.RequiredReplacementFaction = TEXT("csn");
	Payload.RequiredReplacementKind = EWBEffectReplacementCardKind::Character;
	Payload.InheritancePolicy =
		EWBEffectInheritancePolicy::TransferEquippedWandsAndAddSourceCurrentRL;
	return Payload;
}

FWBCardDefinition MakeCrashInDefinition(const FString& CardId, const bool bSemantic)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = TEXT("CSN Crash-In");
	Definition.PublicCategory = TEXT("Effect React");
	Definition.Kind = EWBCardDefinitionKind::Action;
	Definition.PublicFactions.Add(TEXT("csn"));
	if (!bSemantic)
	{
		return Definition;
	}

	FWBCardEffectDefinition Effect;
	Effect.EffectId = TEXT("replace_defender");
	Effect.PublicLabel = TEXT("Crash-In");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Hand;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = false;
	Effect.SourceGate.bRequiresCostsSatisfiedExternally = true;
	Effect.SourceGate.CostGate.bRequiresExternalAffordability = true;
	Effect.SourceGate.CostGate.RequiredRR = 2;
	Effect.SourceGate.CostGate.CostKind = FName(TEXT("RR"));
	Effect.ActivationCondition.AttackDefender =
		EWBCardEffectAttackDefenderRequirement::OwnCurrentDefender;
	Effect.ActivationCondition.TargetController =
		EWBCardEffectTargetControllerRequirement::Self;
	Effect.ActivationCondition.RequiredTargetFaction = TEXT("csn");
	Effect.Payloads.Add(MakeReplacementPayload());
	Definition.ActivatedEffects.Add(MoveTemp(Effect));
	return Definition;
}

FWBCardDefinition MakeDestructionObserverDefinition()
{
	FWBCardDefinition Definition = MakeCharacter(
		TEXT("sable_observer"), TEXT("csn"), 10, 1, 3, 2);
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
	Definition.AfterUnitDestroyedTriggers.Add(Trigger);
	return Definition;
}

FWBCardDefinitionRepository MakeRepository(
	const FString& CrashCardId = TEXT("effect_react_csn_crash_in"),
	const bool bCrashSemantic = true)
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("attacker"), TEXT("wandwright"), 14, 4, 8, 4));
	Definitions.Add(MakeCharacter(TEXT("hero_csn"), TEXT("csn"), 14, 1, 2, 4));
	Definitions.Add(MakeCharacter(TEXT("source_csn"), TEXT("csn"), 16, 3, 2, 3));
	Definitions.Add(MakeCharacter(TEXT("replacement_csn"), TEXT("csn"), 13, 2, 2, 2));
	Definitions.Add(MakeCharacter(TEXT("wrong_faction"), TEXT("officer"), 12, 2, 2, 2));
	Definitions.Add(MakeDestructionObserverDefinition());

	FWBCardDefinition Wand;
	Wand.CardId = TEXT("wand_instance_definition");
	Wand.PublicName = TEXT("Inherited Wand");
	Wand.PublicCategory = TEXT("Wand");
	Wand.Kind = EWBCardDefinitionKind::Wand;
	Wand.WandStats.RR = 1;
	Definitions.Add(Wand);
	Definitions.Add(MakeCrashInDefinition(CrashCardId, bCrashSemantic));

	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("csn_crash_in_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(
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

FWBZoneCardEntry MakeHandEntry(
	const int32 Owner,
	const FString& InstanceId,
	const FString& CardId,
	const int32 Index)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = Owner;
	Entry.Zone = EWBCardZone::Hand;
	Entry.ZoneIndex = Index;
	return Entry;
}

FWBGameStateData MakeState(const bool bDuplicateReplacement = false)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 1;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::Response;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = AttackerId;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = HeroId;
	State.Players = { Player0, Player1 };

	State.AddUnitForTest(MakeUnit(
		AttackerId, 0, TEXT("attacker"), FWBTile(4, 4), 14, 4, 8, 4, 4, 0));
	State.AddUnitForTest(MakeUnit(
		HeroId, 1, TEXT("hero_csn"), FWBTile(5, 0), 14, 1, 2, 4, 4, 0));
	State.AddUnitForTest(MakeUnit(
		DefenderId, 1, TEXT("source_csn"), FWBTile(4, 1), 16, 3, 2, 3, 5, 1));

	FWBCardZoneState& Zones = State.GetMutableCardZoneStateForTest();
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	Zones1.Hand.Add(MakeHandEntry(
		1, TEXT("crash_in_instance"), TEXT("effect_react_csn_crash_in"), 0));
	Zones1.Hand.Add(MakeHandEntry(
		1, TEXT("replacement_instance_a"), TEXT("replacement_csn"), 1));
	if (bDuplicateReplacement)
	{
		Zones1.Hand.Add(MakeHandEntry(
			1, TEXT("replacement_instance_b"), TEXT("replacement_csn"), 2));
	}
	Zones.PlayerZones = { Zones0, Zones1 };
	FWBEquippedCardEntry Wand;
	Wand.Card.InstanceId = TEXT("inherited_wand_instance");
	Wand.Card.CardId = TEXT("wand_instance_definition");
	Wand.Card.OwnerPlayerId = 1;
	Wand.EquippedToUnitId = DefenderId;
	Wand.SlotId = TEXT("wand");
	Wand.EquipOrder = 4;
	Zones.EquippedCards.Add(Wand);

	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.Stage = EWBAttackContinuationStage::PreHit;
	Pending.AttackerUnitId = AttackerId;
	Pending.DefenderUnitId = DefenderId;
	Pending.OriginalAttackerUnitId = AttackerId;
	Pending.OriginalDefenderUnitId = DefenderId;
	Pending.AttackingPlayerId = 0;
	Pending.AttackerTile = FWBTile(4, 4);
	Pending.DefenderTile = FWBTile(4, 1);
	Pending.DeclarationActionId = TEXT("attack:p0:u10:t30");
	Pending.ContinuationId = TEXT("crash_in_continuation");
	State.SetPendingAttackForTest(Pending);
	State.ReactionWindow.Kind = EWBReactionWindowKind::PreHit;
	State.ReactionWindow.OriginatingPlayerId = 0;
	State.ReactionWindow.SourceActionId = Pending.DeclarationActionId;
	State.ReactionWindow.SourceUnitId = AttackerId;
	State.ReactionWindow.TargetUnitId = DefenderId;
	return State;
}

FWBEffectRequest MakeRequest(
	const FString& InstanceId = TEXT("replacement_instance_a"),
	const FString& CardId = TEXT("replacement_csn"))
{
	FWBEffectRequest Request;
	Request.Source.PlayerId = 1;
	Request.Source.SourceCardId = TEXT("effect_react_csn_crash_in");
	Request.Source.SourceEffectId = TEXT("replace_defender");
	Request.Target.TargetUnitId = DefenderId;
	Request.AuxiliaryCardSelection.Zone = EWBEffectAuxiliaryCardZone::Hand;
	Request.AuxiliaryCardSelection.CardInstanceId = InstanceId;
	Request.AuxiliaryCardSelection.CardId = CardId;
	Request.Payloads.Add(MakeReplacementPayload());
	return Request;
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

FWBCardActivationCandidateSource MakeCandidateSource(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const bool bWithSelections)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 1;
	Source.SourceUnitId = -1;
	Source.SourceCardInstanceId = TEXT("crash_in_instance");
	Source.SourceZone = EWBCardZone::Hand;
	Source.CardDefinition = Definition;
	FWBEffectTargetRef Target;
	Target.TargetUnitId = DefenderId;
	Source.CandidateTargets.Add(Target);

	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 1;
	Context.CostPayerUnitId = HeroId;
	Context.SourceCardId = Definition.CardId;
	Context.SourceCardInstanceId = TEXT("crash_in_instance");
	Context.SourceZone = EWBCardActivationSourceZone::Hand;
	Context.bHasExplicitSourceGateContext = true;
	Context.bCostsSatisfiedExternally = true;
	Context.CostContext.bHasExternalAffordability = true;
	Context.CostContext.bExternallyAffordable = true;
	Context.CostContext.SuppliedRequiredRR = 2;
	Context.CostContext.SuppliedAvailableRL = 4;
	Context.CostContext.CostKind = FName(TEXT("RR"));
	FWBCardActivationFixtureZoneEntry ZoneEntry;
	ZoneEntry.CardId = Definition.CardId;
	ZoneEntry.CardInstanceId = TEXT("crash_in_instance");
	ZoneEntry.OwnerPlayerId = 1;
	ZoneEntry.Zone = EWBCardActivationSourceZone::Hand;
	Context.FixtureZoneContext.Entries.Add(ZoneEntry);
	Source.EffectIdToSourceGateContext.Add(TEXT("replace_defender"), Context);
	Source.SourceGateContext = Context;

	if (bWithSelections)
	{
		const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
			State.GetCardZoneState(), 1);
		if (Zones != nullptr)
		{
			for (const FWBZoneCardEntry& Entry : Zones->Hand)
			{
				if (Entry.Card.CardId != TEXT("replacement_csn"))
				{
					continue;
				}
				FWBEffectAuxiliaryCardSelection Selection;
				Selection.Zone = EWBEffectAuxiliaryCardZone::Hand;
				Selection.CardInstanceId = Entry.Card.InstanceId;
				Selection.CardId = Entry.Card.CardId;
				Source.EffectIdToAuxiliaryCardSelections.FindOrAdd(
					TEXT("replace_defender")).Add(Selection);
			}
		}
	}
	return Source;
}
}

#define WB_CRASH_IN_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_CRASH_IN_TEST(FWBCSNCrashInProductionDefinitionTest,
	"Wandbound.CSNCrashIn.CardDB.ActualProductionDefinitionLoads")
bool FWBCSNCrashInProductionDefinitionTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	for (const FWBProductionCardDBDiagnostic& Diagnostic : Loaded.Diagnostics)
	{
		if (Diagnostic.Severity == EWBProductionCardDBDiagnosticSeverity::Error)
		{
			AddError(FString::Printf(TEXT("%s | %s | %s"),
				*Diagnostic.Code, *Diagnostic.FieldPath, *Diagnostic.Message));
		}
	}
	TestTrue(TEXT("Production suite loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid())
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("CSN_CRASH_IN_BUNDLE_DIGEST=%s"),
		*Loaded.Snapshot->ContentDigest));
	const FWBProductionCardRecord* Record = Loaded.Snapshot->FindRecord(
		TEXT("effect_react_csn_crash_in"));
	TestNotNull(TEXT("Actual card exists"), Record);
	if (Record == nullptr || Record->CoreDefinition.ActivatedEffects.IsEmpty())
	{
		return false;
	}
	const FWBCardEffectDefinition& Effect =
		Record->CoreDefinition.ActivatedEffects[0];
	TestEqual(TEXT("Public name"), Record->CoreDefinition.PublicName,
		FString(TEXT("CSN Crash-In")));
	TestEqual(TEXT("RR two"), Effect.SourceGate.CostGate.RequiredRR, 2);
	TestEqual(TEXT("Response timing"), Effect.SourceGate.Timing,
		EWBCardActivationTimingRequirement::ResponseWindow);
	TestEqual(TEXT("Current own defender"),
		Effect.ActivationCondition.AttackDefender,
		EWBCardEffectAttackDefenderRequirement::OwnCurrentDefender);
	TestEqual(TEXT("CSN defender metadata"),
		Effect.ActivationCondition.RequiredTargetFaction,
		FString(TEXT("csn")));
	TestEqual(TEXT("Generic replacement operation"),
		Effect.Payloads[0].Operation,
		EWBGenericEffectOp::ReplacePendingAttackDefenderFromHand);
	TestEqual(TEXT("Character only"),
		Effect.Payloads[0].RequiredReplacementKind,
		EWBEffectReplacementCardKind::Character);
	TestEqual(TEXT("Generic inheritance policy"),
		Effect.Payloads[0].InheritancePolicy,
		EWBEffectInheritancePolicy::TransferEquippedWandsAndAddSourceCurrentRL);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInCandidateChoiceTest,
	"Wandbound.CSNCrashIn.Candidates.InstanceDistinctDeterministicChoices")
bool FWBCSNCrashInCandidateChoiceTest::RunTest(const FString&)
{
	const FWBGameStateData State = MakeState(true);
	const FWBCardDefinition Definition = MakeCrashInDefinition(
		TEXT("fixture_equivalent_replacement"), true);
	const FWBCardActivationCandidateGenerationResult Generated =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State, { MakeCandidateSource(State, Definition, true) });
	TestTrue(TEXT("Candidates generate"), Generated.bOk);
	TestEqual(TEXT("Two exact instances produce two candidates"),
		Generated.Candidates.Num(), 2);
	if (Generated.Candidates.Num() == 2)
	{
		TestNotEqual(TEXT("Stable ids differ by instance"),
			Generated.Candidates[0].ActivationCandidateId,
			Generated.Candidates[1].ActivationCandidateId);
		TestTrue(TEXT("First id carries first immutable instance"),
			Generated.Candidates[0].ActivationCandidateId.Contains(
				TEXT("replacement_instance_a")));
		TestTrue(TEXT("Second id carries second immutable instance"),
			Generated.Candidates[1].ActivationCandidateId.Contains(
				TEXT("replacement_instance_b")));
		TestEqual(TEXT("Command captures first exact instance"),
			Generated.Candidates[0].Command.EffectRequest.AuxiliaryCardSelection.CardInstanceId,
			FString(TEXT("replacement_instance_a")));
	}
	const FWBCardActivationCandidateGenerationResult Missing =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State, { MakeCandidateSource(State, Definition, false) });
	TestTrue(TEXT("Missing replacement is a valid empty set"), Missing.bOk);
	TestEqual(TEXT("No choice means no candidate"), Missing.Candidates.Num(), 0);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInLegalityAndAtomicEdgesTest,
	"Wandbound.CSNCrashIn.Rules.LegalityAndAtomicEdges")
bool FWBCSNCrashInLegalityAndAtomicEdgesTest::RunTest(const FString&)
{
	const FWBCardDefinition Definition = MakeCrashInDefinition(
		TEXT("fixture_equivalent_replacement"), true);
	FWBGameStateData AffordableState = MakeState();
	FWBCardActivationCandidateSource Unaffordable = MakeCandidateSource(
		AffordableState, Definition, true);
	FWBCardActivationSourceGateContext* CostContext =
		Unaffordable.EffectIdToSourceGateContext.Find(TEXT("replace_defender"));
	if (CostContext != nullptr)
	{
		CostContext->CostContext.bExternallyAffordable = false;
		CostContext->CostContext.SuppliedAvailableRL = 1;
		Unaffordable.SourceGateContext = *CostContext;
	}
	const FWBCardActivationCandidateGenerationResult UnaffordableResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			AffordableState, { Unaffordable });
	TestTrue(TEXT("Unaffordable generation remains valid"),
		UnaffordableResult.bOk);
	TestEqual(TEXT("Insufficient RR produces no candidate"),
		UnaffordableResult.Candidates.Num(), 0);

	auto ExpectAtomicFailure = [this](
		const TCHAR* Label,
		FWBGameStateData State,
		const FWBEffectRequest& Request,
		const FWBCardDefinitionRepository& Repository)
	{
		const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
		const FWBEffectRequestResult Applied = WBEffectRunner::ApplyEffectRequest(
			State, Request, Repository);
		TestFalse(Label, Applied.bOk);
		TestEqual(
			*FString::Printf(TEXT("%s leaves state unchanged"), Label),
			WBProductionMatchReplay::BuildGameStateDigest(State),
			Before);
	};

	FWBGameStateData WrongTiming = MakeState();
	WrongTiming.PendingAttack.Stage = EWBAttackContinuationStage::PostHit;
	ExpectAtomicFailure(
		TEXT("Wrong attack timing fails"),
		WrongTiming,
		MakeRequest(),
		MakeRepository());

	FWBGameStateData WrongOwner = MakeState();
	WrongOwner.GetMutableUnitById(DefenderId)->OwnerId = 0;
	ExpectAtomicFailure(
		TEXT("Opponent defender fails"),
		WrongOwner,
		MakeRequest(),
		MakeRepository());

	FWBGameStateData WrongSourceFaction = MakeState();
	WrongSourceFaction.GetMutableUnitById(DefenderId)->CardId =
		TEXT("wrong_faction");
	ExpectAtomicFailure(
		TEXT("Non-CSN defender fails"),
		WrongSourceFaction,
		MakeRequest(),
		MakeRepository());

	FWBGameStateData NonCharacter = MakeState();
	NonCharacter.GetMutableCardZoneStateForTest().PlayerZones[1]
		.Hand[1].Card.CardId = TEXT("wand_instance_definition");
	ExpectAtomicFailure(
		TEXT("Non-Character replacement fails"),
		NonCharacter,
		MakeRequest(
			TEXT("replacement_instance_a"),
			TEXT("wand_instance_definition")),
		MakeRepository());

	FWBGameStateData InvalidSource = MakeState();
	InvalidSource.GetMutableUnitById(DefenderId)->MarkUnitDefeated();
	ExpectAtomicFailure(
		TEXT("Invalid source defender fails"),
		InvalidSource,
		MakeRequest(),
		MakeRepository());

	FWBGameStateData InvalidZones = MakeState();
	InvalidZones.GetMutableCardZoneStateForTest().PlayerZones[1].Hand.Add(
		MakeHandEntry(
			1,
			TEXT("inherited_wand_instance"),
			TEXT("replacement_csn"),
			2));
	ExpectAtomicFailure(
		TEXT("Duplicate zone instance fails"),
		InvalidZones,
		MakeRequest(),
		MakeRepository());

	ExpectAtomicFailure(
		TEXT("Missing replacement definition fails"),
		MakeState(),
		MakeRequest(),
		FWBCardDefinitionRepository());

	FWBGameStateData AtCap = MakeState();
	AtCap.AddUnitForTest(MakeUnit(
		31, 1, TEXT("source_csn"), FWBTile(0, 0), 16, 3, 2, 3, 3, 0));
	AtCap.AddUnitForTest(MakeUnit(
		32, 1, TEXT("source_csn"), FWBTile(1, 0), 16, 3, 2, 3, 3, 0));
	TestEqual(TEXT("Player begins at normal cap"),
		AtCap.GetUnitsForPlayer(1).Num(), 4);
	const FWBEffectRequestResult AtCapResult = WBEffectRunner::ApplyEffectRequest(
		AtCap, MakeRequest(), MakeRepository());
	TestTrue(TEXT("One-for-one replacement at cap succeeds"), AtCapResult.bOk);
	TestEqual(TEXT("Replacement result stays at cap"),
		AtCap.GetUnitsForPlayer(1).Num(), 4);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInSuccessfulResolutionTest,
	"Wandbound.CSNCrashIn.Resolution.TransactionalInheritanceAndRedirect")
bool FWBCSNCrashInSuccessfulResolutionTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const int32 AttacksBefore = State.GetUnitById(AttackerId)->AttacksLeft;
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(
		State, MakeRequest(), Repository);
	TestTrue(TEXT("Replacement resolves"), Result.bOk);
	const FWBUnitState* Source = State.GetUnitById(DefenderId);
	TestNotNull(TEXT("Destroyed source retained for replay identity"), Source);
	TestTrue(TEXT("Source defeated"), Source != nullptr && Source->bDefeated);
	TestTrue(TEXT("Source removed"), Source != nullptr && Source->bRemovedFromBoard);
	const FWBUnitState* Replacement = State.Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("replacement_csn") && Unit.IsUnitOnBoard();
		});
	TestNotNull(TEXT("Replacement exists"), Replacement);
	if (Replacement == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Exact tile X"), Replacement->X, 4);
	TestEqual(TEXT("Exact tile Y"), Replacement->Y, 1);
	TestEqual(TEXT("Printed HP"), Replacement->HP, 13);
	TestEqual(TEXT("Printed ATK"), Replacement->ATK, 2);
	TestEqual(TEXT("Printed AR"), Replacement->AR, 2);
	TestEqual(TEXT("Base RL is printed two plus source CurrentRL five"),
		Replacement->BaseRL, 7);
	TestEqual(TEXT("Current RL recalculated"), Replacement->CurrentRL, 7);
	TestEqual(TEXT("RLUsed from transferred Wand"), Replacement->RLUsed, 1);
	TestEqual(TEXT("Same attack continuation"),
		State.PendingAttack.ContinuationId,
		FString(TEXT("crash_in_continuation")));
	TestEqual(TEXT("Attack redirected"),
		State.PendingAttack.DefenderUnitId, Replacement->UnitId);
	TestEqual(TEXT("No second attack spend"),
		State.GetUnitById(AttackerId)->AttacksLeft, AttacksBefore);
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), 1);
	TestTrue(TEXT("Selected instance removed once"), Zones != nullptr
		&& !Zones->Hand.ContainsByPredicate([](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == TEXT("replacement_instance_a");
		}));
	const FWBEquippedCardEntry* Wand =
		State.GetCardZoneState().EquippedCards.FindByPredicate(
			[](const FWBEquippedCardEntry& Entry)
			{
				return Entry.Card.InstanceId == TEXT("inherited_wand_instance");
			});
	TestNotNull(TEXT("Exact Wand instance remains equipped"), Wand);
	TestEqual(TEXT("Wand transferred to replacement"),
		Wand != nullptr ? Wand->EquippedToUnitId : -1, Replacement->UnitId);
	TestEqual(TEXT("Wand owner preserved"),
		Wand != nullptr ? Wand->Card.OwnerPlayerId : -1, 1);
	TestEqual(TEXT("Wand equip order preserved"),
		Wand != nullptr ? Wand->EquipOrder : -1, 4);
	TestNotNull(TEXT("Inheritance trace"), FindTrace(
		Result.TraceEvents, FName(TEXT("csn_inheritance"))));
	TestNotNull(TEXT("Redirect trace"), FindTrace(
		Result.TraceEvents, FName(TEXT("pending_attack_redirected"))));
	TestNotNull(TEXT("Replacement summon trace"), FindTrace(
		Result.TraceEvents, FName(TEXT("effect_replacement_summon"))));
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInFailureAtomicityTest,
	"Wandbound.CSNCrashIn.Resolution.FailuresAreAtomic")
bool FWBCSNCrashInFailureAtomicityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData MissingCard = MakeState();
	const FString MissingBefore = WBProductionMatchReplay::BuildGameStateDigest(
		MissingCard);
	const FWBEffectRequestResult MissingResult = WBEffectRunner::ApplyEffectRequest(
		MissingCard, MakeRequest(TEXT("removed_by_nested_response")), Repository);
	TestFalse(TEXT("Missing selected instance fails"), MissingResult.bOk);
	TestEqual(TEXT("Missing selection leaves state byte-canonical"),
		WBProductionMatchReplay::BuildGameStateDigest(MissingCard), MissingBefore);

	FWBGameStateData Stale = MakeState();
	FWBEffectRequest StaleRequest = MakeRequest();
	StaleRequest.Payloads[0].PendingAttackContinuationId = TEXT("stale");
	const FString StaleBefore = WBProductionMatchReplay::BuildGameStateDigest(Stale);
	const FWBEffectRequestResult StaleResult = WBEffectRunner::ApplyEffectRequest(
		Stale, StaleRequest, Repository);
	TestFalse(TEXT("Stale continuation fails"), StaleResult.bOk);
	TestEqual(TEXT("Stale continuation leaves state unchanged"),
		WBProductionMatchReplay::BuildGameStateDigest(Stale), StaleBefore);

	FWBGameStateData WrongDefinition = MakeState();
	FWBEffectRequest WrongRequest = MakeRequest(
		TEXT("replacement_instance_a"), TEXT("wrong_faction"));
	WrongDefinition.GetMutableCardZoneStateForTest().PlayerZones[1].Hand[1].Card.CardId =
		TEXT("wrong_faction");
	const FString WrongBefore = WBProductionMatchReplay::BuildGameStateDigest(
		WrongDefinition);
	const FWBEffectRequestResult WrongResult = WBEffectRunner::ApplyEffectRequest(
		WrongDefinition, WrongRequest, Repository);
	TestFalse(TEXT("Wrong faction fails"), WrongResult.bOk);
	TestEqual(TEXT("Wrong faction leaves state unchanged"),
		WBProductionMatchReplay::BuildGameStateDigest(WrongDefinition), WrongBefore);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInSableObserverBoundaryTest,
	"Wandbound.CSNCrashIn.Regression.SableObserverUsesDestructionBoundary")
bool FWBCSNCrashInSableObserverBoundaryTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData NewlySummoned = MakeState();
	FWBPlayerCardZoneState* NewZones = WBCardZoneState::FindMutablePlayerZones(
		NewlySummoned.GetMutableCardZoneStateForTest(), 1);
	check(NewZones != nullptr);
	NewZones->Hand[1].Card.CardId = TEXT("sable_observer");
	const FWBEffectRequestResult NewResult = WBEffectRunner::ApplyEffectRequest(
		NewlySummoned,
		MakeRequest(TEXT("replacement_instance_a"), TEXT("sable_observer")),
		Repository);
	TestTrue(TEXT("Crash-In can summon observer definition"), NewResult.bOk);
	const FWBUnitState* NewSable = NewlySummoned.Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("sable_observer") && Unit.IsUnitOnBoard();
		});
	TestNotNull(TEXT("Replacement Sable exists"), NewSable);
	TestTrue(TEXT("Historical destruction remains queued"),
		!NewlySummoned.PendingUnitDestructionEvents.IsEmpty());
	if (NewSable == nullptr || NewlySummoned.PendingUnitDestructionEvents.IsEmpty()) return false;
	TestFalse(TEXT("New Sable was not captured retroactively"),
		NewlySummoned.PendingUnitDestructionEvents[0].ObserverSources.ContainsByPredicate(
			[NewSable](const FWBPostDestructionObserverSourceSnapshot& Source)
			{
				return Source.SourceUnitId == NewSable->UnitId;
			}));
	const FWBPostDestructionTriggerResult NewTriggers =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			NewlySummoned, Repository, 1, 0);
	TestTrue(TEXT("Historical event completes"), NewTriggers.bOk);
	TestEqual(TEXT("New Sable does not grow from historical death"),
		NewlySummoned.GetUnitById(NewSable->UnitId)->ATK, 1);

	FWBGameStateData ExistingObserver = MakeState();
	ExistingObserver.AddUnitForTest(MakeUnit(
		40, 1, TEXT("sable_observer"), FWBTile(6, 0), 10, 1, 3, 2, 2, 0));
	const FWBEffectRequestResult ExistingResult = WBEffectRunner::ApplyEffectRequest(
		ExistingObserver, MakeRequest(), Repository);
	TestTrue(TEXT("Crash-In with existing observer resolves"), ExistingResult.bOk);
	TestTrue(TEXT("Existing Sable captured at destruction"),
		ExistingObserver.PendingUnitDestructionEvents[0].ObserverSources.ContainsByPredicate(
			[](const FWBPostDestructionObserverSourceSnapshot& Source)
			{
				return Source.SourceUnitId == 40;
			}));
	const int32 AttackDefenderAfterReplacement =
		ExistingObserver.PendingAttack.DefenderUnitId;
	const int32 AttacksLeftAfterReplacement =
		ExistingObserver.GetUnitById(AttackerId)->AttacksLeft;
	const FWBPostDestructionTriggerResult ExistingTriggers =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			ExistingObserver, Repository, 1, 0);
	TestTrue(TEXT("Existing observer trigger resolves"), ExistingTriggers.bOk);
	TestEqual(TEXT("Existing Sable ATK grows once"), ExistingObserver.GetUnitById(40)->ATK, 2);
	TestEqual(TEXT("Existing Sable HP grows once"), ExistingObserver.GetUnitById(40)->HP, 11);
	TestEqual(TEXT("Existing Sable MaxHP grows once"), ExistingObserver.GetUnitById(40)->MaxHP, 11);
	TestEqual(TEXT("Attack remains redirected"),
		ExistingObserver.PendingAttack.DefenderUnitId, AttackDefenderAfterReplacement);
	TestEqual(TEXT("No second attack declaration"),
		ExistingObserver.GetUnitById(AttackerId)->AttacksLeft,
		AttacksLeftAfterReplacement);
	TestEqual(TEXT("Exactly one observer application"),
		ExistingTriggers.TraceEvents.FilterByPredicate([](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("unit_stat_delta_applied"));
		}).Num(), 1);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInHeroTerminalTest,
	"Wandbound.CSNCrashIn.Hero.DestroyedHeroRemainsTerminal")
bool FWBCSNCrashInHeroTerminalTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(DefenderId)->X = 5;
	State.GetMutableUnitById(DefenderId)->Y = 1;
	FWBUnitState* Hero = State.GetMutableUnitById(HeroId);
	Hero->X = 4;
	Hero->Y = 1;
	State.PendingAttack.DefenderUnitId = HeroId;
	State.PendingAttack.OriginalDefenderUnitId = HeroId;
	State.PendingAttack.DefenderTile = FWBTile(4, 1);
	State.ReactionWindow.TargetUnitId = HeroId;
	State.GetMutableCardZoneStateForTest().EquippedCards[0].EquippedToUnitId = HeroId;
	FWBEffectRequest Request = MakeRequest();
	Request.Target.TargetUnitId = HeroId;
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(
		State, Request, MakeRepository());
	TestTrue(TEXT("Hero-targeted effect resolves"), Result.bOk);
	TestTrue(TEXT("Hero loss commits terminal"), State.bGameOver);
	TestEqual(TEXT("Hero-loss reason"), State.TerminalOutcome.Reason,
		EWBTerminalReason::HeroDefeatedWithoutReplacement);
	TestEqual(TEXT("Effect terminal source"), State.TerminalOutcome.Source,
		EWBTerminalSource::Effect);
	TestEqual(TEXT("Hero identity is not silently replaced"),
		State.GetPlayerById(1)->HeroUnitId, HeroId);
	TestTrue(TEXT("Original Hero is removed"),
		State.GetUnitById(HeroId)->bRemovedFromBoard);
	const FWBUnitState* Replacement = State.Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("replacement_csn") && Unit.IsUnitOnBoard();
		});
	TestNotNull(TEXT("Character still enters before terminal boundary"), Replacement);
	TestTrue(TEXT("Replacement is not Hero"), Replacement != nullptr
		&& Replacement->UnitId != State.GetPlayerById(1)->HeroUnitId);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInPrivacyAndAuthorityTest,
	"Wandbound.CSNCrashIn.PrivacyAndAuthority.NoIdBranchOrPreRevealLeak")
bool FWBCSNCrashInPrivacyAndAuthorityTest::RunTest(const FString&)
{
	const FWBGameStateData State = MakeState();
	const FWBCardZonePlayerObservation Opponent =
		WBCardZoneObservation::BuildObservationForPlayer(State, 0);
	TestFalse(TEXT("Opponent observation hides selected definition"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Opponent, TEXT("replacement_csn")));
	TestFalse(TEXT("Opponent observation hides selected instance"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Opponent, TEXT("replacement_instance_a")));

	FString CoreSource;
	FFileHelper::LoadFileToString(CoreSource, *FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source/WandboundCore/Private/WBUnitReplacementEffect.cpp")));
	FString CoordinatorSource;
	FFileHelper::LoadFileToString(CoordinatorSource, *FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp")));
	TestFalse(TEXT("Replacement authority has no Crash-In id branch"),
		CoreSource.Contains(TEXT("effect_react_csn_crash_in")));
	TestFalse(TEXT("Coordinator has no Crash-In id branch"),
		CoordinatorSource.Contains(TEXT("effect_react_csn_crash_in")));

	const FWBCardDefinition Equivalent = MakeCrashInDefinition(
		TEXT("fixture_other_definition_id"), true);
	const FWBCardActivationCandidateGenerationResult EquivalentCandidates =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State, { MakeCandidateSource(State, Equivalent, true) });
	TestTrue(TEXT("Equivalent semantic definition works"),
		EquivalentCandidates.bOk && EquivalentCandidates.Candidates.Num() == 1);
	const FWBCardDefinition NameOnly = MakeCrashInDefinition(
		TEXT("effect_react_csn_crash_in"), false);
	FWBCardActivationCandidateSource NameOnlySource;
	NameOnlySource.PlayerId = 1;
	NameOnlySource.CardDefinition = NameOnly;
	FWBEffectTargetRef Target;
	Target.TargetUnitId = DefenderId;
	NameOnlySource.CandidateTargets.Add(Target);
	const FWBCardActivationCandidateGenerationResult NameOnlyCandidates =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State, { NameOnlySource });
	TestTrue(TEXT("Name-only definition is harmless"), NameOnlyCandidates.bOk);
	TestEqual(TEXT("Name-only definition gains no behavior"),
		NameOnlyCandidates.Candidates.Num(), 0);
	return true;
}

WB_CRASH_IN_TEST(FWBCSNCrashInProductionSmokeTest,
	"Wandbound.CSNCrashIn.Fixture.ProductionSmokeAndFreshReplay")
bool FWBCSNCrashInProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNCrashInFixture/match_spec.json"));

	const FWBProductionCSNCrashInSmokeResult First =
		WBProductionCSNCrashInSmoke::Run(Request);
	const FWBProductionCSNCrashInSmokeResult Second =
		WBProductionCSNCrashInSmoke::Run(Request);
	if (!First.bOk)
	{
		AddError(FString::Printf(
			TEXT("First production Crash-In smoke failed: %s"),
			*First.Reason));
	}
	if (!Second.bOk)
	{
		AddError(FString::Printf(
			TEXT("Second production Crash-In smoke failed: %s"),
			*Second.Reason));
	}
	TestTrue(TEXT("First production smoke succeeds"), First.bOk);
	TestTrue(TEXT("Second production smoke succeeds"), Second.bOk);
	TestEqual(TEXT("Replay record count deterministic"),
		First.RecordsVerified, Second.RecordsVerified);
	TestEqual(TEXT("Generation deterministic"),
		First.FinalGeneration, Second.FinalGeneration);
	TestEqual(TEXT("Revision deterministic"),
		First.FinalRevision, Second.FinalRevision);
	TestEqual(TEXT("State digest deterministic"),
		First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("Trace digest deterministic"),
		First.FinalTraceDigest, Second.FinalTraceDigest);
	TestEqual(TEXT("Archive bytes deterministic"),
		First.SerializedArchive, Second.SerializedArchive);
	TestEqual(TEXT("Receipt bytes deterministic"),
		First.SerializedReceipt, Second.SerializedReceipt);
	TestTrue(TEXT("Replay verifies committed actions"),
		First.RecordsVerified > 0);
	return true;
}

#undef WB_CRASH_IN_TEST

#endif
