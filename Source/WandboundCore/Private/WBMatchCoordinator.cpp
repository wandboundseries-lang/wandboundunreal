#include "WBMatchCoordinator.h"

#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"
#include "WBHybridSummon.h"
#include "WBMarkerResolution.h"
#include "WBNPCPhaseResolution.h"
#include "WBResonanceOverflow.h"
#include "WBRules.h"

namespace
{
constexpr int32 OpeningHandSize = 6;

FName ReactionWindowKindToName(EWBReactionWindowKind Kind);

FString BuildPendingEffectCanonicalState(
	const TArray<FWBPendingEffectActivationFrame>& Frames)
{
	if (Frames.IsEmpty())
	{
		return FString();
	}

	FString Out = FString::Printf(TEXT("pending_effect.count=%d;"), Frames.Num());
	auto AppendString = [&Out](const TCHAR* Label, const FString& Value)
	{
		Out += FString::Printf(TEXT("%s=%d:%s;"), Label, Value.Len(), *Value);
	};
	for (const FWBPendingEffectActivationFrame& Frame : Frames)
	{
		AppendString(TEXT("frame"), Frame.FrameId);
		AppendString(TEXT("parent"), Frame.ParentFrameId);
		AppendString(TEXT("action"), Frame.ActivationActionId);
		AppendString(TEXT("card"), Frame.Command.Source.SourceCardId);
		AppendString(TEXT("instance"), Frame.Command.Source.SourceCardInstanceId);
		AppendString(TEXT("effect"), Frame.Command.Source.SourceEffectId);
		AppendString(TEXT("debug_activation"), Frame.Command.DebugActivationId);
		AppendString(TEXT("usage_key"), Frame.Command.UsageCommit.UsageKey);
		AppendString(TEXT("cost_kind"), Frame.Command.CostPaymentCommit.CostKind.ToString());
		AppendString(TEXT("parent_source_action"), Frame.ParentReactionWindow.SourceActionId);
		Out += FString::Printf(
			TEXT("player=%d;source_unit=%d;source_zone=%d;negated=%d;usage_mark=%d;usage_player=%d;cost_pay=%d;cost_player=%d;cost_source_unit=%d;cost_rr=%d;parent_kind=%d;parent_origin=%d;parent_passes=%d;parent_source_unit=%d;parent_target_unit=%d;parent_priority=%d;parent_game_phase=%d;parent_match_phase=%d;has_parent_reaction=%d;request_source_player=%d;request_source_unit=%d;request_target_unit=%d;request_target_tile=%d,%d;request_target_wall=%d,%d,%d,%d;"),
			Frame.ActivatingPlayerId,
			Frame.Command.Source.SourceUnitId,
			static_cast<int32>(Frame.Command.Source.SourceZone),
			Frame.bNegated ? 1 : 0,
			Frame.Command.UsageCommit.bMarkUsageOnSuccess ? 1 : 0,
			Frame.Command.UsageCommit.PlayerId,
			Frame.Command.CostPaymentCommit.bPayCostOnSuccess ? 1 : 0,
			Frame.Command.CostPaymentCommit.PlayerId,
			Frame.Command.CostPaymentCommit.SourceUnitId,
			Frame.Command.CostPaymentCommit.RequiredRR,
			static_cast<int32>(Frame.ParentReactionWindow.Kind),
			Frame.ParentReactionWindow.OriginatingPlayerId,
			Frame.ParentReactionWindow.ConsecutivePassCount,
			Frame.ParentReactionWindow.SourceUnitId,
			Frame.ParentReactionWindow.TargetUnitId,
			Frame.ParentPriorityPlayerId,
			static_cast<int32>(Frame.ParentGamePhase),
			static_cast<int32>(Frame.ParentMatchPhase),
			Frame.bHasParentReaction ? 1 : 0,
			Frame.Command.EffectRequest.Source.PlayerId,
			Frame.Command.EffectRequest.Source.SourceUnitId,
			Frame.Command.EffectRequest.Target.TargetUnitId,
			Frame.Command.EffectRequest.Target.TargetTile.X,
			Frame.Command.EffectRequest.Target.TargetTile.Y,
			Frame.Command.EffectRequest.Target.TargetWallEdge.A.X,
			Frame.Command.EffectRequest.Target.TargetWallEdge.A.Y,
			Frame.Command.EffectRequest.Target.TargetWallEdge.B.X,
			Frame.Command.EffectRequest.Target.TargetWallEdge.B.Y);
		AppendString(TEXT("request_source_card"), Frame.Command.EffectRequest.Source.SourceCardId);
		AppendString(TEXT("request_source_effect"), Frame.Command.EffectRequest.Source.SourceEffectId);
		for (const FWBGenericEffectPayload& Payload : Frame.Command.EffectRequest.Payloads)
		{
			Out += FString::Printf(
				TEXT("payload=%d;armor_op=%d;armor_target=%d;armor_amount=%d;status_op=%d;status_target=%d;status_duration=%d;damage_target=%d;damage_source_unit=%d;damage_source_player=%d;damage_amount=%d;damage_bypass=%d;heal_target=%d;heal_source_unit=%d;heal_source_player=%d;heal_amount=%d;"),
				static_cast<int32>(Payload.Operation),
				static_cast<int32>(Payload.ArmorEffect.Operation),
				Payload.ArmorEffect.TargetUnitId,
				Payload.ArmorEffect.Amount,
				static_cast<int32>(Payload.StatusEffect.Operation),
				Payload.StatusEffect.TargetUnitId,
				Payload.StatusEffect.Duration,
				Payload.DamageEffect.TargetUnitId,
				Payload.DamageEffect.SourceUnitId,
				Payload.DamageEffect.SourcePlayerId,
				Payload.DamageEffect.Amount,
				Payload.DamageEffect.bBypassArmor ? 1 : 0,
				Payload.HealEffect.TargetUnitId,
				Payload.HealEffect.SourceUnitId,
				Payload.HealEffect.SourcePlayerId,
				Payload.HealEffect.Amount);
			AppendString(TEXT("target_frame"), Payload.PendingEffectFrameId);
			if (!Payload.PendingAttackContinuationId.IsEmpty())
			{
				AppendString(TEXT("target_attack"), Payload.PendingAttackContinuationId);
			}
			AppendString(TEXT("armor_reason"), Payload.ArmorEffect.SourceReason.ToString());
			AppendString(TEXT("status_id"), Payload.StatusEffect.StatusId.ToString());
			AppendString(TEXT("status_reason"), Payload.StatusEffect.SourceReason.ToString());
			AppendString(TEXT("damage_cause"), Payload.DamageEffect.DamageCause.ToString());
			AppendString(TEXT("damage_reason"), Payload.DamageEffect.SourceReason.ToString());
			AppendString(TEXT("heal_reason"), Payload.HealEffect.SourceReason.ToString());
		}
	}
	return Out;
}

FWBMatchOperationResult MakeOperationFailure(const FString& Reason)
{
	FWBMatchOperationResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBMatchLegalActionGenerationResult MakeGenerationFailure(const FString& Reason)
{
	FWBMatchLegalActionGenerationResult Result;
	Result.Reason = Reason;
	return Result;
}

bool PlayerSetupLess(const FWBMatchPlayerSetup& A, const FWBMatchPlayerSetup& B)
{
	return A.PlayerId < B.PlayerId;
}

bool UnitIdPointerLess(const FWBUnitState& A, const FWBUnitState& B)
{
	return A.UnitId < B.UnitId;
}

bool ZoneEntryLess(const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
{
	if (A.ZoneIndex != B.ZoneIndex)
	{
		return A.ZoneIndex < B.ZoneIndex;
	}
	if (A.Card.InstanceId != B.Card.InstanceId)
	{
		return A.Card.InstanceId < B.Card.InstanceId;
	}
	return A.Card.CardId < B.Card.CardId;
}

uint32 NextMatchRandom(uint32& InOutRandomState)
{
	InOutRandomState =
		InOutRandomState * 1664525u + 1013904223u;
	return InOutRandomState;
}

void ShuffleDeckDeterministically(
	TArray<FWBCardInstanceRef>& Cards,
	uint32& InOutRandomState)
{
	for (int32 Index = Cards.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = static_cast<int32>(
			NextMatchRandom(InOutRandomState)
			% static_cast<uint32>(Index + 1));
		Cards.Swap(Index, SwapIndex);
	}
}

FWBTile HeroSpawnForPlayer(const int32 PlayerId)
{
	return PlayerId == 0 ? FWBTile(4, 8) : FWBTile(4, 0);
}

FWBTraceEvent MakeMatchTrace(
	const FName Kind,
	const int32 PlayerId,
	const int32 TurnNumber,
	const FName Phase)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = PlayerId;
	Event.TurnNumber = TurnNumber;
	Event.MatchPhase = Phase;
	Event.bOk = true;
	return Event;
}

EWBTerminalSource InferTerminalSource(
	const FWBMatchLegalAction& Action,
	const TArray<FWBTraceEvent>& Events)
{
	bool bHasNPCTrace = false;
	for (const FWBTraceEvent& Event : Events)
	{
		if (Event.DamageCause == FName(TEXT("Trap")))
		{
			return EWBTerminalSource::Trap;
		}
		if (Event.DamageCause == FName(TEXT("Burn"))
			|| Event.DamageCause == FName(TEXT("Poison")))
		{
			return EWBTerminalSource::Status;
		}
		bHasNPCTrace |= Event.Kind.ToString().StartsWith(TEXT("npc_"));
	}
	if (bHasNPCTrace)
	{
		return EWBTerminalSource::NPC;
	}
	if (Action.Family == EWBMatchActionFamily::Activation)
	{
		return EWBTerminalSource::Effect;
	}
	if (Action.Family == EWBMatchActionFamily::CoreAction
		&& Action.CoreAction.Type == EWBActionType::Attack)
	{
		return EWBTerminalSource::Attack;
	}
	return EWBTerminalSource::Unknown;
}

void CommitTerminalOutcome(
	FWBGameStateData& State,
	const FWBMatchLegalAction& Action,
	const int32 NextCoordinatorRevision,
	const int32 TraceBaseIndex,
	TArray<FWBTraceEvent>& Events)
{
	if (!State.bGameOver)
	{
		return;
	}
	if (State.HasOpenReactionWindow())
	{
		FWBTraceEvent Cleared = MakeMatchTrace(
			FName(TEXT("reaction_window_cleared_terminal")),
			State.ReactionWindow.OriginatingPlayerId,
			State.TurnNumber,
			FName(TEXT("game_over")));
		Cleared.ReactionWindowKind = ReactionWindowKindToName(
			State.ReactionWindow.Kind);
		Cleared.SourceUnitId = State.ReactionWindow.SourceUnitId;
		Cleared.TargetUnitId = State.ReactionWindow.TargetUnitId;
		Events.Add(Cleared);
		State.ClearReactionWindow();
	}
	State.NPCPhaseContinuation.Reset();
	FWBTerminalOutcome& Outcome = State.TerminalOutcome;
	Outcome.bTerminal = true;
	Outcome.WinnerPlayerId = State.WinnerPlayerId;
	Outcome.LoserPlayerId = Outcome.LoserPlayerId >= 0
		? Outcome.LoserPlayerId
		: (State.WinnerPlayerId == 0 ? 1 : 0);
	Outcome.Reason = EWBTerminalReason::HeroDefeatedWithoutReplacement;
	Outcome.Source = InferTerminalSource(Action, Events);
	Outcome.TurnNumber = State.TurnNumber;
	Outcome.CoordinatorRevision = NextCoordinatorRevision;
	Outcome.TraceIndex = TraceBaseIndex + Events.Num();

	FWBTraceEvent Committed = MakeMatchTrace(
		FName(TEXT("terminal_state_committed")),
		Outcome.LoserPlayerId,
		Outcome.TurnNumber,
		FName(TEXT("game_over")));
	Committed.WinningPlayerId = Outcome.WinnerPlayerId;
	Committed.Reason = WBTerminalOutcomeNames::ReasonToName(Outcome.Reason).ToString();
	Committed.DamageCause = WBTerminalOutcomeNames::SourceToName(Outcome.Source);
	Committed.bHeroUnit = true;
	Events.Add(Committed);

	FWBTraceEvent GameOver = MakeMatchTrace(
		FName(TEXT("game_over")),
		Outcome.LoserPlayerId,
		Outcome.TurnNumber,
		FName(TEXT("game_over")));
	GameOver.WinningPlayerId = Outcome.WinnerPlayerId;
	GameOver.Reason = Committed.Reason;
	GameOver.DamageCause = Committed.DamageCause;
	Events.Add(GameOver);
}

FWBTraceEvent MakeLegacyTurnTransitionTrace(
	const int32 EndingPlayerId,
	const int32 NextPlayerId,
	const int32 TurnNumberBefore,
	const int32 TurnNumberAfter,
	const int32 NextPlayerExplicitMPRoll)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("turn_transition"));
	Event.PlayerId = EndingPlayerId;
	Event.FromPlayer = EndingPlayerId;
	Event.ToPlayer = NextPlayerId;
	Event.NextPlayerId = NextPlayerId;
	Event.TurnNumberBefore = TurnNumberBefore;
	Event.TurnNumberAfter = TurnNumberAfter;
	Event.MPRoll = NextPlayerExplicitMPRoll;
	Event.bOk = true;
	return Event;
}

void AppendFixtureZoneEntry(
	FWBCardActivationFixtureZoneContext& Context,
	const FString& CardId,
	const FString& CardInstanceId,
	const int32 OwnerPlayerId,
	const EWBCardActivationSourceZone Zone,
	const int32 EquippedToUnitId = -1)
{
	FWBCardActivationFixtureZoneEntry Entry;
	Entry.CardId = CardId;
	Entry.CardInstanceId = CardInstanceId;
	Entry.OwnerPlayerId = OwnerPlayerId;
	Entry.Zone = Zone;
	Entry.EquippedToUnitId = EquippedToUnitId;
	Context.Entries.Add(Entry);
}

FWBCardActivationFixtureZoneContext BuildActivationZoneContext(const FWBGameStateData& State)
{
	FWBCardActivationFixtureZoneContext Context;
	for (const FWBPlayerCardZoneState& PlayerZones : State.GetCardZoneState().PlayerZones)
	{
		for (const FWBZoneCardEntry& Entry : PlayerZones.Deck)
		{
			AppendFixtureZoneEntry(Context, Entry.Card.CardId, Entry.Card.InstanceId, PlayerZones.PlayerId, EWBCardActivationSourceZone::Deck);
		}
		for (const FWBZoneCardEntry& Entry : PlayerZones.Hand)
		{
			AppendFixtureZoneEntry(Context, Entry.Card.CardId, Entry.Card.InstanceId, PlayerZones.PlayerId, EWBCardActivationSourceZone::Hand);
		}
		for (const FWBZoneCardEntry& Entry : PlayerZones.Discard)
		{
			AppendFixtureZoneEntry(Context, Entry.Card.CardId, Entry.Card.InstanceId, PlayerZones.PlayerId, EWBCardActivationSourceZone::Discard);
		}
	}

	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard())
		{
			AppendFixtureZoneEntry(Context, Unit.CardId, FString(), Unit.OwnerId, EWBCardActivationSourceZone::Board, Unit.UnitId);
		}
	}

	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		AppendFixtureZoneEntry(
			Context,
			Entry.Card.CardId,
			Entry.Card.InstanceId,
			Entry.Card.OwnerPlayerId,
			EWBCardActivationSourceZone::Equipped,
			Entry.EquippedToUnitId);
	}

	return Context;
}

TArray<FWBEffectTargetRef> BuildActivationTargets(const FWBGameStateData& State)
{
	TArray<FWBEffectTargetRef> Targets;
	Targets.Add(FWBEffectTargetRef());

	TArray<const FWBUnitState*> Units;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard())
		{
			Units.Add(&Unit);
		}
	}
	Units.Sort(UnitIdPointerLess);

	for (const FWBUnitState* Unit : Units)
	{
		FWBEffectTargetRef Target;
		Target.TargetUnitId = Unit->UnitId;
		Targets.Add(Target);
	}
	return Targets;
}

bool DoesActivationConditionMatch(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 PlayerId,
	const FWBCardEffectDefinition& Effect,
	const FWBEffectTargetRef& Target)
{
	const FWBCardEffectActivationCondition& Condition =
		Effect.ActivationCondition;
	if (Condition.AttackDefender
		== EWBCardEffectAttackDefenderRequirement::OwnHeroCurrentDefender)
	{
		const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
		if (Player == nullptr
			|| !State.HasPendingAttack()
			|| State.PendingAttack.Stage != EWBAttackContinuationStage::PreHit
			|| State.PendingAttack.DefenderUnitId != Player->HeroUnitId)
		{
			return false;
		}
	}

	const bool bHasTargetCondition =
		Condition.TargetController
			!= EWBCardEffectTargetControllerRequirement::Any
		|| Condition.TargetRelation
			!= EWBCardEffectTargetRelationRequirement::Any
		|| !Condition.RequiredTargetFaction.IsEmpty();
	if (!bHasTargetCondition)
	{
		return true;
	}

	const FWBUnitState* TargetUnit = State.GetUnitById(Target.TargetUnitId);
	if (TargetUnit == nullptr
		|| TargetUnit->bDefeated
		|| !TargetUnit->IsUnitOnBoard())
	{
		return false;
	}
	if (Condition.TargetController
		== EWBCardEffectTargetControllerRequirement::Self
		&& TargetUnit->OwnerId != PlayerId)
	{
		return false;
	}

	const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
	const FWBUnitState* Hero = Player != nullptr
		? State.GetUnitById(Player->HeroUnitId)
		: nullptr;
	if (Condition.TargetRelation
		== EWBCardEffectTargetRelationRequirement::OrthogonallyAdjacentToOwnHero)
	{
		if (Hero == nullptr
			|| Hero->bDefeated
			|| !Hero->IsUnitOnBoard()
			|| TargetUnit->UnitId == Hero->UnitId
			|| FMath::Abs(TargetUnit->X - Hero->X)
				+ FMath::Abs(TargetUnit->Y - Hero->Y) != 1)
		{
			return false;
		}
	}
	else if (Condition.TargetRelation
		== EWBCardEffectTargetRelationRequirement::OtherThanOwnHero)
	{
		if (Hero == nullptr
			|| Hero->bDefeated
			|| !Hero->IsUnitOnBoard()
			|| TargetUnit->UnitId == Hero->UnitId)
		{
			return false;
		}
	}
	if (!Condition.RequiredTargetFaction.IsEmpty())
	{
		const FWBCardDefinitionRepositoryLookupResult TargetDefinition =
			WBCardDefinitionRepository::FindCardById(
				Repository, TargetUnit->CardId);
		if (!TargetDefinition.bFound
			|| !TargetDefinition.Definition.PublicFactions.Contains(
				Condition.RequiredTargetFaction))
		{
			return false;
		}
	}
	return true;
}

FWBCardActivationSourceGateContext BuildActivationGateContext(
	const FWBGameStateData& State,
	const FWBCardActivationFixtureZoneContext& ZoneContext,
	const FWBCardDefinition& Definition,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const FString& SourceCardInstanceId,
	const EWBCardActivationSourceZone SourceZone,
	const FWBCardEffectDefinition& Effect)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = PlayerId;
	Context.SourceUnitId = SourceUnitId;
	Context.CostPayerUnitId = SourceZone == EWBCardActivationSourceZone::Hand
		? (State.GetPlayerById(PlayerId) != nullptr
			? State.GetPlayerById(PlayerId)->HeroUnitId
			: -1)
		: SourceUnitId;
	Context.SourceCardId = Definition.CardId;
	Context.SourceCardInstanceId = SourceCardInstanceId;
	Context.SourceZone = SourceZone;
	Context.FixtureZoneContext = ZoneContext;
	Context.bHasExplicitSourceGateContext = true;
	Context.bCostsSatisfiedExternally = true;
	Context.CostContext.bHasExternalAffordability = true;
	Context.CostContext.SuppliedRequiredRR = Effect.SourceGate.CostGate.RequiredRR;
	Context.CostContext.CostKind = Effect.SourceGate.CostGate.CostKind;

	const FWBUnitState* CostPayer = State.GetUnitById(Context.CostPayerUnitId);
	Context.CostContext.SuppliedAvailableRL = CostPayer != nullptr
		? CostPayer->GetAvailableRLForRules()
		: 0;
	Context.CostContext.bExternallyAffordable =
		Effect.SourceGate.CostGate.RequiredRR <= Context.CostContext.SuppliedAvailableRL;
	Context.ActivationUsageKey = Effect.SourceGate.OncePerTurnKey;
	return Context;
}

void AddActivationSource(
	TArray<FWBCardActivationCandidateSource>& Sources,
	const FWBGameStateData& State,
	const FWBCardActivationFixtureZoneContext& ZoneContext,
	const FWBCardDefinition& Definition,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const FString& SourceCardInstanceId,
	const EWBCardActivationSourceZone SourceZone,
	const bool bResponseOnly = false)
{
	FWBCardDefinition EligibleDefinition = Definition;
	if (bResponseOnly)
	{
		EligibleDefinition.ActivatedEffects.RemoveAll(
			[](const FWBCardEffectDefinition& Effect)
			{
				return Effect.SourceGate.Timing
					!= EWBCardActivationTimingRequirement::ResponseWindow;
			});
	}
	if (EligibleDefinition.ActivatedEffects.IsEmpty())
	{
		return;
	}

	FWBCardActivationCandidateSource Source;
	Source.PlayerId = PlayerId;
	Source.SourceUnitId = SourceUnitId;
	Source.SourceCardInstanceId = SourceCardInstanceId;
	switch (SourceZone)
	{
	case EWBCardActivationSourceZone::Hand: Source.SourceZone = EWBCardZone::Hand; break;
	case EWBCardActivationSourceZone::Board: Source.SourceZone = EWBCardZone::Board; break;
	case EWBCardActivationSourceZone::Equipped: Source.SourceZone = EWBCardZone::Equipped; break;
	default: Source.SourceZone = EWBCardZone::Unknown; break;
	}
	Source.CardDefinition = EligibleDefinition;
	Source.CandidateTargets = BuildActivationTargets(State);
	for (const FWBCardEffectDefinition& Effect : EligibleDefinition.ActivatedEffects)
	{
		Source.EffectIdToSourceGateContext.Add(
			Effect.EffectId,
			BuildActivationGateContext(
				State,
				ZoneContext,
				EligibleDefinition,
				PlayerId,
				SourceUnitId,
				SourceCardInstanceId,
				SourceZone,
				Effect));
	}
	Source.SourceGateContext = Source.EffectIdToSourceGateContext.FindRef(
		EligibleDefinition.ActivatedEffects[0].EffectId);
	Sources.Add(MoveTemp(Source));
}

FWBMatchLegalActionGenerationResult GetActivationActions(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 PlayerId,
	const bool bResponseOnly,
	const FString& PendingEffectFrameId = FString(),
	const FString& PendingAttackContinuationId = FString())
{
	FWBMatchLegalActionGenerationResult Result;
	const FWBCardActivationFixtureZoneContext ZoneContext =
		BuildActivationZoneContext(State);
	TArray<FWBCardActivationCandidateSource> ActivationSources;
	const FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
	if (PlayerZones != nullptr)
	{
		TArray<FWBZoneCardEntry> Hand = PlayerZones->Hand;
		Hand.Sort(ZoneEntryLess);
		for (const FWBZoneCardEntry& Entry : Hand)
		{
			const FWBCardDefinitionRepositoryLookupResult Lookup =
				WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
			if (Lookup.bFound)
			{
				AddActivationSource(
					ActivationSources,
					State,
					ZoneContext,
					Lookup.Definition,
					PlayerId,
					-1,
					Entry.Card.InstanceId,
					EWBCardActivationSourceZone::Hand,
					bResponseOnly);
			}
		}
	}
	TArray<const FWBUnitState*> BoardUnits = State.GetUnitsForPlayer(PlayerId);
	BoardUnits.Sort(UnitIdPointerLess);
	for (const FWBUnitState* Unit : BoardUnits)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Unit->CardId);
		if (Lookup.bFound)
		{
			AddActivationSource(
				ActivationSources,
				State,
				ZoneContext,
				Lookup.Definition,
				PlayerId,
				Unit->UnitId,
				FString(),
				EWBCardActivationSourceZone::Board,
				bResponseOnly);
		}
	}

	TArray<FWBEquippedCardEntry> EquippedCards =
		State.GetCardZoneState().EquippedCards;
	EquippedCards.Sort(
		[](const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
		{
			if (A.EquippedToUnitId != B.EquippedToUnitId)
			{
				return A.EquippedToUnitId < B.EquippedToUnitId;
			}
			if (A.EquipOrder != B.EquipOrder)
			{
				return A.EquipOrder < B.EquipOrder;
			}
			return A.Card.InstanceId < B.Card.InstanceId;
		});
	for (const FWBEquippedCardEntry& Entry : EquippedCards)
	{
		if (Entry.Card.OwnerPlayerId != PlayerId)
		{
			continue;
		}
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
		if (Lookup.bFound)
		{
			AddActivationSource(
				ActivationSources,
				State,
				ZoneContext,
				Lookup.Definition,
				PlayerId,
				Entry.EquippedToUnitId,
				Entry.Card.InstanceId,
				EWBCardActivationSourceZone::Equipped,
				bResponseOnly);
		}
	}

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State,
			ActivationSources);
	if (!CandidateResult.bOk)
	{
		return MakeGenerationFailure(CandidateResult.Reason);
	}
	const FWBCardActivationLegalActionGenerationResult ActivationResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(
			CandidateResult.Candidates);
	if (!ActivationResult.bOk)
	{
		return MakeGenerationFailure(ActivationResult.Reason);
	}

	TArray<FWBCardActivationLegalAction> ActivationActions =
		ActivationResult.ActionSet.Actions;
	ActivationActions.Sort(
		[](const FWBCardActivationLegalAction& A,
			const FWBCardActivationLegalAction& B)
		{
			return A.ActivationActionId < B.ActivationActionId;
		});
	for (const FWBCardActivationLegalAction& ActivationAction : ActivationActions)
	{
		const FWBCardDefinitionRepositoryLookupResult SourceDefinition =
			WBCardDefinitionRepository::FindCardById(
				Repository,
				ActivationAction.Command.Source.SourceCardId);
		if (!SourceDefinition.bFound)
		{
			continue;
		}
		const FWBCardEffectDefinition* SourceEffect =
			SourceDefinition.Definition.ActivatedEffects.FindByPredicate(
				[&ActivationAction](const FWBCardEffectDefinition& Effect)
				{
					return Effect.EffectId
						== ActivationAction.Command.Source.SourceEffectId;
				});
		if (SourceEffect == nullptr
			|| !DoesActivationConditionMatch(
				State,
				Repository,
				PlayerId,
				*SourceEffect,
				ActivationAction.Command.EffectRequest.Target))
		{
			continue;
		}
		const bool bControlsPendingAttack =
			ActivationAction.Command.EffectRequest.Payloads.ContainsByPredicate(
				[](const FWBGenericEffectPayload& Payload)
				{
					return Payload.Operation
							== EWBGenericEffectOp::PreventPendingAttack
						|| Payload.Operation
							== EWBGenericEffectOp::RedirectPendingAttack
						|| Payload.Operation
							== EWBGenericEffectOp::RegisterPendingAttackHPDamageSubstitution;
				});
		if (bControlsPendingAttack
			&& (PendingAttackContinuationId.IsEmpty()
				|| !State.HasPendingAttack()
				|| State.PendingAttack.Stage
					!= EWBAttackContinuationStage::PreHit))
		{
			continue;
		}
		FWBMatchLegalAction Action;
		Action.Family = EWBMatchActionFamily::Activation;
		Action.ActionId = ActivationAction.ActivationActionId;
		Action.PlayerId = ActivationAction.PlayerId;
		Action.ActivationCommand = ActivationAction.Command;
		if (!PendingEffectFrameId.IsEmpty())
		{
			for (FWBGenericEffectPayload& Payload : Action.ActivationCommand.EffectRequest.Payloads)
			{
				if (Payload.Operation == EWBGenericEffectOp::NegatePendingEffect)
				{
					Payload.PendingEffectFrameId = PendingEffectFrameId;
				}
			}
		}
		if (!PendingAttackContinuationId.IsEmpty())
		{
			for (FWBGenericEffectPayload& Payload :
				Action.ActivationCommand.EffectRequest.Payloads)
			{
				if (Payload.Operation == EWBGenericEffectOp::PreventPendingAttack
					|| Payload.Operation == EWBGenericEffectOp::RedirectPendingAttack
					|| Payload.Operation
						== EWBGenericEffectOp::RegisterPendingAttackHPDamageSubstitution)
				{
					Payload.PendingAttackContinuationId =
						PendingAttackContinuationId;
				}
			}
		}
		if (bControlsPendingAttack)
		{
			const FWBActionQueryResult Query =
				WBRules::CanApplyCardActivationCommand(
					State, Action.ActivationCommand);
			if (!Query.bOk)
			{
				continue;
			}
		}
		Result.Actions.Add(MoveTemp(Action));
	}
	Result.bOk = true;
	return Result;
}

FName ReactionWindowKindToName(const EWBReactionWindowKind Kind)
{
	switch (Kind)
	{
	case EWBReactionWindowKind::PreHit: return FName(TEXT("pre_hit"));
	case EWBReactionWindowKind::PostHit: return FName(TEXT("post_hit"));
	case EWBReactionWindowKind::PostMove: return FName(TEXT("post_move"));
	case EWBReactionWindowKind::PostSummon: return FName(TEXT("post_summon"));
	case EWBReactionWindowKind::PostEffect: return FName(TEXT("post_effect"));
	case EWBReactionWindowKind::None:
	default: return NAME_None;
	}
}

void RebindReactionTargetFromPendingAttack(FWBGameStateData& State)
{
	if (!State.HasOpenReactionWindow() || !State.HasPendingAttack())
	{
		return;
	}
	if (State.ReactionWindow.Kind != EWBReactionWindowKind::PreHit
		&& State.ReactionWindow.Kind != EWBReactionWindowKind::PostHit)
	{
		return;
	}
	if (State.PendingAttack.Stage != EWBAttackContinuationStage::PreHit
		&& State.PendingAttack.Stage != EWBAttackContinuationStage::PostHit)
	{
		return;
	}
	State.ReactionWindow.SourceUnitId = State.PendingAttack.AttackerUnitId;
	State.ReactionWindow.TargetUnitId = State.PendingAttack.DefenderUnitId;
}

void AppendSummonTraceEvents(
	const FWBSummonExecutionResult& Result,
	TArray<FWBTraceEvent>& OutEvents)
{
	for (const FWBSummonExecutionTraceEvent& Source : Result.TraceEvents)
	{
		FWBTraceEvent Event;
		Event.Kind = FName(*Source.EventType);
		Event.PlayerId = Source.PlayerId;
		Event.SourceUnitId = Source.CreatedUnitId;
		Event.CardInstanceId = Source.SourceInstanceId;
		Event.CardId = Source.SourceCardId;
		Event.ToTile = Source.TargetTile;
		Event.bOk = true;
		OutEvents.Add(Event);
	}
}

void AppendEquipTraceEvents(
	const FWBEquipExecutionResult& Result,
	TArray<FWBTraceEvent>& OutEvents)
{
	for (const FWBEquipExecutionTraceEvent& Source : Result.TraceEvents)
	{
		FWBTraceEvent Event;
		Event.Kind = FName(*Source.EventType);
		Event.PlayerId = Source.PlayerId;
		Event.TargetUnitId = Source.EquippedToUnitId;
		Event.CardInstanceId = Source.SourceInstanceId;
		Event.CardId = Source.SourceCardId;
		Event.SlotId = Source.SlotId;
		Event.CostAmount = Source.RR;
		Event.PreviousRLUsed = Source.RLUsedBefore;
		Event.NewRLUsed = Source.RLUsedAfter;
		Event.bOk = true;
		OutEvents.Add(Event);
	}
}

void AppendOverflowTraceEvents(
	const FWBResonanceOverflowResult& Result,
	TArray<FWBTraceEvent>& OutEvents)
{
	for (const FWBResonanceOverflowTraceEvent& Source : Result.TraceEvents)
	{
		FWBTraceEvent Event;
		Event.Kind = FName(*Source.EventType);
		Event.PlayerId = Source.PlayerId;
		Event.SourceUnitId = Source.UnitId;
		Event.CardInstanceId = Source.SourceInstanceId;
		Event.CardId = Source.SourceCardId;
		Event.SlotId = Source.SlotId;
		Event.EquipOrder = Source.EquipOrder;
		Event.CostAmount = Source.RR;
		Event.PreviousRLUsed = Source.RLUsedBefore;
		Event.NewRLUsed = Source.RLUsedAfter;
		Event.bOk = true;
		OutEvents.Add(Event);
	}
}
}

FWBMatchOperationResult WBMatchCoordinator::InitializeMatch(const FWBMatchInitializationRequest& Request)
{
	const int32 NextCoordinatorGeneration = CoordinatorGeneration + 1;
	const FWBCardDefinitionRepositoryValidationResult RepositoryValidation =
		WBCardDefinitionRepository::ValidateRepository(Request.Repository);
	if (!RepositoryValidation.bOk)
	{
		return MakeOperationFailure(RepositoryValidation.Reason);
	}

	if (Request.Players.Num() != 2)
	{
		return MakeOperationFailure(TEXT("expected_two_players"));
	}

	TArray<FWBMatchPlayerSetup> PlayerSetups = Request.Players;
	PlayerSetups.Sort(PlayerSetupLess);
	if (PlayerSetups[0].PlayerId != 0 || PlayerSetups[1].PlayerId != 1)
	{
		return MakeOperationFailure(TEXT("invalid_player_setup"));
	}

	uint32 WorkingRandomState = static_cast<uint32>(Request.Seed);
	if (WorkingRandomState == 0)
	{
		WorkingRandomState = 0x6d2b79f5u;
	}

	int32 SelectedFirstPlayer = Request.FirstPlayerId;
	if (Request.bDeriveFirstPlayerFromSeed
		|| SelectedFirstPlayer == INDEX_NONE)
	{
		SelectedFirstPlayer = static_cast<int32>(
			NextMatchRandom(WorkingRandomState) % 2u);
	}
	if (!FWBGameStateData::IsValidPlayerId(SelectedFirstPlayer))
	{
		return MakeOperationFailure(TEXT("invalid_first_player"));
	}
	if (Request.bDeriveFirstPlayerFromSeed
		&& Request.ExpectedFirstPlayerId != INDEX_NONE
		&& Request.ExpectedFirstPlayerId != SelectedFirstPlayer)
	{
		return MakeOperationFailure(TEXT("expected_first_player_mismatch"));
	}

	FWBGameStateData WorkingState;
	WorkingState.CurrentPlayer = SelectedFirstPlayer;
	WorkingState.PriorityPlayer = SelectedFirstPlayer;
	WorkingState.FirstPlayerId = SelectedFirstPlayer;
	WorkingState.TurnNumber = 1;
	WorkingState.Phase = EWBGamePhase::NormalTurn;
	WorkingState.bInitialSetupInProgress = true;
	WorkingState.bSuppressManualReactsDuringInitialHeroSetup = true;
	WorkingState.bGameOver = false;
	WorkingState.WinnerPlayerId = -1;

	TSet<FString> SeenInstanceIds;
	TArray<FWBTraceEvent> WorkingTraceEvents;
	FWBTraceEvent MatchInitialized = MakeMatchTrace(
		FName(TEXT("match_initialized")),
		SelectedFirstPlayer,
		1,
		PhaseToName(EWBMatchLoopPhase::Setup));
	MatchInitialized.RandomSeed = Request.Seed;
	WorkingTraceEvents.Add(MatchInitialized);

	FWBTraceEvent FirstPlayerSelected = MakeMatchTrace(
		FName(TEXT("first_player_selected")),
		SelectedFirstPlayer,
		1,
		PhaseToName(EWBMatchLoopPhase::Setup));
	WorkingTraceEvents.Add(FirstPlayerSelected);

	TArray<FWBInitialHeroPlacement> HeroPlacements;
	for (const FWBMatchPlayerSetup& Setup : PlayerSetups)
	{
		if (Setup.HeroInstanceId.IsEmpty() || Setup.HeroCardId.IsEmpty())
		{
			return MakeOperationFailure(TEXT("hero_selection_missing"));
		}

		const FWBCardDefinitionRepositoryLookupResult HeroLookup =
			WBCardDefinitionRepository::FindCardById(Request.Repository, Setup.HeroCardId);
		if (!HeroLookup.bFound || HeroLookup.Definition.Kind != EWBCardDefinitionKind::Character)
		{
			return MakeOperationFailure(TEXT("hero_definition_invalid"));
		}

		int32 HeroDeckIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Setup.OrderedDeck.Num(); ++Index)
		{
			const FWBCardInstanceRef& Card = Setup.OrderedDeck[Index];
			if (Card.InstanceId.IsEmpty() || Card.CardId.IsEmpty())
			{
				return MakeOperationFailure(TEXT("deck_card_invalid"));
			}
			if (SeenInstanceIds.Contains(Card.InstanceId))
			{
				return MakeOperationFailure(TEXT("duplicate_instance_id"));
			}
			SeenInstanceIds.Add(Card.InstanceId);
			if (!WBCardDefinitionRepository::ContainsCardId(Request.Repository, Card.CardId))
			{
				return MakeOperationFailure(TEXT("card_definition_not_found"));
			}
			if (Card.InstanceId == Setup.HeroInstanceId && Card.CardId == Setup.HeroCardId)
			{
				if (HeroDeckIndex != INDEX_NONE)
				{
					return MakeOperationFailure(TEXT("hero_instance_duplicated"));
				}
				HeroDeckIndex = Index;
			}
		}

		if (HeroDeckIndex == INDEX_NONE)
		{
			return MakeOperationFailure(TEXT("hero_not_in_deck"));
		}
		if (Setup.OrderedDeck.Num() - 1 < OpeningHandSize)
		{
			return MakeOperationFailure(TEXT("opening_deck_too_small"));
		}

		FWBPlayerStateData Player;
		Player.PlayerId = Setup.PlayerId;
		Player.HeroUnitId = -1;
		WorkingState.Players.Add(Player);

		TArray<FWBCardInstanceRef> RemainingDeck;
		for (int32 Index = 0; Index < Setup.OrderedDeck.Num(); ++Index)
		{
			if (Index != HeroDeckIndex)
			{
				RemainingDeck.Add(Setup.OrderedDeck[Index]);
			}
		}
		if (Request.bShuffleDecksAtMatchStart)
		{
			ShuffleDeckDeterministically(
				RemainingDeck,
				WorkingRandomState);
		}

		FWBPlayerCardZoneState PlayerZones;
		PlayerZones.PlayerId = Setup.PlayerId;
		for (int32 Index = 0; Index < RemainingDeck.Num(); ++Index)
		{
			FWBZoneCardEntry Entry;
			Entry.Card = RemainingDeck[Index];
			Entry.Card.OwnerPlayerId = Setup.PlayerId;
			Entry.Zone = EWBCardZone::Deck;
			Entry.ZoneIndex = Index;
			PlayerZones.Deck.Add(Entry);
		}
		WorkingState.CardZoneState.PlayerZones.Add(PlayerZones);

		FWBInitialHeroPlacement Placement;
		Placement.PlayerId = Setup.PlayerId;
		Placement.HeroInstanceId = Setup.HeroInstanceId;
		Placement.HeroCardId = Setup.HeroCardId;
		Placement.SpawnTile = WBRules::IsTileInBounds(
			Setup.HeroSpawnTile)
			? Setup.HeroSpawnTile
			: HeroSpawnForPlayer(Setup.PlayerId);
		if (Placement.SpawnTile
			!= HeroSpawnForPlayer(Setup.PlayerId))
		{
			return MakeOperationFailure(TEXT("hero_spawn_tile_invalid"));
		}
		HeroPlacements.Add(MoveTemp(Placement));
	}

	WBCardZoneState::SortOrderedZonesDeterministically(WorkingState.CardZoneState);

	const FWBMarkerResolutionResult MarkerSetupResult =
		WBMarkerResolution::ApplyCanonicalSetup(
			WorkingState,
			Request.Repository,
			Request.MarkerPlacements);
	if (!MarkerSetupResult.bOk)
	{
		return MakeOperationFailure(MarkerSetupResult.Reason);
	}
	WorkingTraceEvents.Append(MarkerSetupResult.TraceEvents);

	FWBInitialHeroSetupRequest HeroSetupRequest;
	HeroSetupRequest.FirstPlayerId = SelectedFirstPlayer;
	HeroSetupRequest.Placements = MoveTemp(HeroPlacements);
	HeroSetupRequest.TriggerOrderChoices =
		Request.SetupTriggerOrderChoices;
	const FWBInitialHeroSetupResult HeroSetupResult =
		WBInitialHeroSetup::Apply(
			WorkingState,
			Request.Repository,
			HeroSetupRequest);
	if (!HeroSetupResult.bOk)
	{
		return MakeOperationFailure(HeroSetupResult.Reason);
	}
	WorkingTraceEvents.Append(HeroSetupResult.TraceEvents);

	const int32 OpeningDrawPlayers[] = {
		SelectedFirstPlayer,
		1 - SelectedFirstPlayer
	};
	for (const int32 PlayerId : OpeningDrawPlayers)
	{
		const FWBCardLifecycleResult DrawResult =
			WBCardLifecycle::ApplySetupDraw(
				WorkingState,
				PlayerId,
				OpeningHandSize);
		if (!DrawResult.bOk)
		{
			return MakeOperationFailure(DrawResult.Reason);
		}

		FWBTraceEvent OpeningDrawn = MakeMatchTrace(
			FName(TEXT("opening_hand_draw")),
			PlayerId,
			1,
			PhaseToName(EWBMatchLoopPhase::Setup));
		OpeningDrawn.CardCount = OpeningHandSize;
		WorkingTraceEvents.Add(OpeningDrawn);
	}

	WorkingState.bSuppressManualReactsDuringInitialHeroSetup = false;
	WorkingState.bInitialSetupInProgress = false;

	const int32 OpeningMPRoll = RollD6(WorkingRandomState);
	FWBTurnStartSequenceState WorkingTurnStartSequence;
	const FWBTurnStartSequenceResult TurnStartResult =
		WBTurnStartSequence::Begin(
			WorkingState,
			Request.Repository,
			SelectedFirstPlayer,
			OpeningMPRoll,
			WorkingTurnStartSequence);
	if (!TurnStartResult.bOk)
	{
		return MakeOperationFailure(TurnStartResult.Reason);
	}
	WorkingTraceEvents.Append(TurnStartResult.TraceEvents);
	const EWBMatchLoopPhase InitialMatchPhase =
		TurnStartResult.bTerminal
			? EWBMatchLoopPhase::GameOver
			: (TurnStartResult.bCompleted
				? EWBMatchLoopPhase::Action
				: EWBMatchLoopPhase::TurnStart);
	if (TurnStartResult.bCompleted)
	{
		WorkingTraceEvents.Add(MakeMatchTrace(
			FName(TEXT("turn_started")),
			SelectedFirstPlayer,
			1,
			PhaseToName(EWBMatchLoopPhase::Action)));
	}

	WBMatchCoordinator Candidate;
	Candidate.bInitialized = true;
	Candidate.FirstPlayerId = SelectedFirstPlayer;
	Candidate.RandomState = WorkingRandomState;
	Candidate.MatchPhase = InitialMatchPhase;
	Candidate.State = WorkingState;
	Candidate.Repository = Request.Repository;
	Candidate.TraceLog = WorkingTraceEvents;
	Candidate.bHeroSpawnBatchCommitted =
		HeroSetupResult.bSpawnBatchCommitted;
	Candidate.bHeroSetupTriggersResolved =
		HeroSetupResult.bTriggersResolved;
	Candidate.bOpeningHandsDrawn = true;
	Candidate.TurnStartSequence =
		MoveTemp(WorkingTurnStartSequence);
	Candidate.CoordinatorGeneration = NextCoordinatorGeneration;
	Candidate.CoordinatorRevision = 1;
	Candidate.InitialStateDigest =
		WBProductionMatchReplay::BuildCoordinatorStateDigest(
			Candidate.State,
			static_cast<int32>(Candidate.MatchPhase),
			Candidate.RandomState,
			Candidate.TurnStartSequence);
	Candidate.InitialTraceDigest =
		WBProductionMatchReplay::BuildTraceDigest(
			Candidate.TraceLog);

	const FWBMatchLegalActionGenerationResult LegalResult = Candidate.EnumerateLegalActions();
	if (!LegalResult.bOk)
	{
		return MakeOperationFailure(LegalResult.Reason);
	}

	*this = MoveTemp(Candidate);

	FWBMatchOperationResult Result;
	Result.bOk = true;
	Result.TraceEvents = WorkingTraceEvents;
	Result.NextLegalActions = LegalResult.Actions;
	Result.bPendingDecision =
		(MatchPhase == EWBMatchLoopPhase::TurnStart
			&& !TurnStartSequence.bCompleted)
		|| (MatchPhase == EWBMatchLoopPhase::Response
			&& State.HasOpenReactionWindow());
	Result.bCompleted = !Result.bPendingDecision;
	Result.bTerminal = State.bGameOver;
	Result.PendingPlayerId =
		Result.bPendingDecision ? State.PriorityPlayer : -1;
	Result.ActivePlayerId = State.CurrentPlayer;
	Result.TurnNumber = State.TurnNumber;
	Result.TraceBeginIndex = 0;
	Result.TraceEndIndex = TraceLog.Num();
	Result.bGameOver = State.bGameOver;
	Result.WinnerPlayerId = State.WinnerPlayerId;
	Result.CoordinatorGeneration = CoordinatorGeneration;
	Result.CoordinatorRevision = CoordinatorRevision;
	return Result;
}

FWBMatchLegalActionGenerationResult WBMatchCoordinator::EnumerateLegalActions() const
{
	if (!bInitialized)
	{
		return MakeGenerationFailure(TEXT("match_not_initialized"));
	}
	return EnumerateLegalActionsForState(
		State,
		MatchPhase,
		&TurnStartSequence,
		&PendingEffectActivations);
}

FWBMatchLegalActionGenerationResult WBMatchCoordinator::EnumerateLegalActionsForState(
	const FWBGameStateData& InState,
	const EWBMatchLoopPhase InPhase,
	const FWBTurnStartSequenceState* InTurnStartSequence,
	const TArray<FWBPendingEffectActivationFrame>* InPendingEffects) const
{
	const TArray<FWBPendingEffectActivationFrame>& EffectivePendingEffects =
		InPendingEffects != nullptr ? *InPendingEffects : PendingEffectActivations;
	const FString PendingEffectFrameId = EffectivePendingEffects.IsEmpty()
		? FString()
		: EffectivePendingEffects.Last().FrameId;
	const FString PendingAttackContinuationId = InState.HasPendingAttack()
		? InState.PendingAttack.ContinuationId
		: FString();
	FWBMatchLegalActionGenerationResult Result;
	if (InState.bGameOver || InPhase == EWBMatchLoopPhase::GameOver)
	{
		Result.bOk = true;
		return Result;
	}

	if (InPhase == EWBMatchLoopPhase::TurnStart)
	{
		if (InTurnStartSequence == nullptr
			|| InTurnStartSequence->bCompleted)
		{
			return MakeGenerationFailure(
				TEXT("turn_start_sequence_not_complete"));
		}
		for (const FString& ActionId :
			WBTurnStartSequence::
				EnumerateLegalChoiceActionIds(
					InState,
					*InTurnStartSequence))
		{
			FWBMatchLegalAction Action;
			Action.Family =
				EWBMatchActionFamily::TurnStartTrigger;
			Action.ActionId = ActionId;
			Action.PlayerId =
				InTurnStartSequence->ActivePlayerId;
			Result.Actions.Add(MoveTemp(Action));
		}
		if (Result.Actions.IsEmpty())
		{
			return MakeGenerationFailure(
				TEXT("turn_start_trigger_order_required"));
		}
		Result.bOk = true;
		return Result;
	}

	if (InPhase != EWBMatchLoopPhase::Action
		&& InPhase != EWBMatchLoopPhase::Response)
	{
		return MakeGenerationFailure(TEXT("match_not_accepting_actions"));
	}

	const int32 PlayerId = InState.PriorityPlayer;
	TArray<FWBAction> EndTurnActions;
	for (const FWBAction& CoreAction : WBRules::GenerateLegalActionsForPlayer(InState, PlayerId))
	{
		FWBMatchLegalAction MatchAction;
		MatchAction.Family = EWBMatchActionFamily::CoreAction;
		MatchAction.ActionId = WBActionCodec::MakeActionId(CoreAction);
		MatchAction.PlayerId = CoreAction.PlayerId;
		MatchAction.CoreAction = CoreAction;
		if (CoreAction.Type == EWBActionType::EndTurn)
		{
			EndTurnActions.Add(CoreAction);
		}
		else
		{
			Result.Actions.Add(MatchAction);
		}
	}

	if (InPhase == EWBMatchLoopPhase::Action)
	{
		const FWBPlayerCardZoneState* PlayerZones =
			WBCardZoneState::FindPlayerZones(InState.GetCardZoneState(), PlayerId);
		if (PlayerZones == nullptr)
		{
			return MakeGenerationFailure(TEXT("player_zones_missing"));
		}

		TArray<FWBZoneCardEntry> Hand = PlayerZones->Hand;
		Hand.Sort(ZoneEntryLess);
		for (const FWBZoneCardEntry& Entry : Hand)
		{
			const FWBCardDefinitionRepositoryLookupResult Lookup =
				WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
			if (!Lookup.bFound)
			{
				return MakeGenerationFailure(TEXT("card_definition_not_found"));
			}

			if (Lookup.Definition.Kind == EWBCardDefinitionKind::Hybrid)
			{
				const FWBHybridSummonPlanResult Plans =
					WBHybridSummon::BuildSummonPlans(
						InState,
						Repository,
						PlayerId,
						Entry.Card.InstanceId,
						CoordinatorGeneration,
						CoordinatorRevision);
				if (Plans.bOk)
				{
					for (const FWBHybridSummonPlan& Plan : Plans.Plans)
					{
						FWBMatchLegalAction Action;
						Action.Family = EWBMatchActionFamily::Summon;
						Action.PlayerId = PlayerId;
						Action.bHybridSummon = true;
						Action.bHybridHeroReplacement =
							Plan.bBecomesReplacementHero;
						Action.HybridSummonPlan = Plan;
						Action.ActionId = WBHybridSummon::BuildStableActionId(Plan);
						Result.Actions.Add(MoveTemp(Action));
					}
				}
			}
			else if (Lookup.Definition.Kind == EWBCardDefinitionKind::Character)
			{
				const FWBPlayerStateData* Player = InState.GetPlayerById(PlayerId);
				const FWBUnitState* Hero = Player != nullptr ? InState.GetUnitById(Player->HeroUnitId) : nullptr;
				if (Hero != nullptr && Hero->IsUnitOnBoard())
				{
					TArray<FWBTile> Tiles = {
						FWBTile(Hero->X + 1, Hero->Y),
						FWBTile(Hero->X - 1, Hero->Y),
						FWBTile(Hero->X, Hero->Y + 1),
						FWBTile(Hero->X, Hero->Y - 1)
					};
					Tiles.Sort([](const FWBTile& A, const FWBTile& B)
					{
						return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
					});
					for (const FWBTile& Tile : Tiles)
					{
						FWBSummonExecutionRequest Request;
						Request.PlayerId = PlayerId;
						Request.SourceInstanceId = Entry.Card.InstanceId;
						Request.SourceCardId = Entry.Card.CardId;
						Request.TargetTile = Tile;
						FWBGameStateData ProbeState = InState;
						if (!WBSummonExecution::ExecuteCharacterSummonFromHand(ProbeState, Repository, Request).bOk)
						{
							continue;
						}

						FWBMatchLegalAction Action;
						Action.Family = EWBMatchActionFamily::Summon;
						Action.PlayerId = PlayerId;
						Action.SummonRequest = Request;
						Action.ActionId = FString::Printf(
							TEXT("summon:p%d:i%s:x%d:y%d"),
							PlayerId,
							*Entry.Card.InstanceId,
							Tile.X,
							Tile.Y);
						Result.Actions.Add(Action);
					}
				}
			}
			else if (Lookup.Definition.Kind == EWBCardDefinitionKind::Wand)
			{
				TArray<const FWBUnitState*> Units = InState.GetUnitsForPlayer(PlayerId);
				Units.Sort(UnitIdPointerLess);
				for (const FWBUnitState* Unit : Units)
				{
					FWBEquipExecutionRequest Request;
					Request.PlayerId = PlayerId;
					Request.SourceInstanceId = Entry.Card.InstanceId;
					Request.SourceCardId = Entry.Card.CardId;
					Request.TargetUnitId = Unit->UnitId;
					FWBGameStateData ProbeState = InState;
					if (!WBEquipExecution::ExecuteWandEquipFromHand(ProbeState, Repository, Request).bOk)
					{
						continue;
					}

					FWBMatchLegalAction Action;
					Action.Family = EWBMatchActionFamily::Equip;
					Action.PlayerId = PlayerId;
					Action.EquipRequest = Request;
					Action.ActionId = FString::Printf(
						TEXT("equip:p%d:i%s:u%d"),
						PlayerId,
						*Entry.Card.InstanceId,
						Unit->UnitId);
					Result.Actions.Add(Action);
				}
			}

			FWBMatchLegalAction DiscardAction;
			DiscardAction.Family = EWBMatchActionFamily::Discard;
			DiscardAction.PlayerId = PlayerId;
			DiscardAction.DiscardCardInstanceId = Entry.Card.InstanceId;
			DiscardAction.ActionId = FString::Printf(
				TEXT("discard:p%d:i%s"),
				PlayerId,
				*Entry.Card.InstanceId);
			Result.Actions.Add(DiscardAction);
		}

		const FWBMatchLegalActionGenerationResult ActivationResult =
			GetActivationActions(
				InState, Repository, PlayerId, false, PendingEffectFrameId,
				PendingAttackContinuationId);
		if (!ActivationResult.bOk)
		{
			return ActivationResult;
		}
		Result.Actions.Append(ActivationResult.Actions);
	}
	else
	{
		const FWBMatchLegalActionGenerationResult ActivationResult =
			GetActivationActions(
				InState, Repository, PlayerId, true, PendingEffectFrameId,
				PendingAttackContinuationId);
		if (!ActivationResult.bOk)
		{
			return ActivationResult;
		}
		Result.Actions.Append(ActivationResult.Actions);
	}

	for (const FWBAction& EndTurnAction : EndTurnActions)
	{
		FWBMatchLegalAction MatchAction;
		MatchAction.Family = EWBMatchActionFamily::CoreAction;
		MatchAction.ActionId = WBActionCodec::MakeActionId(EndTurnAction);
		MatchAction.PlayerId = EndTurnAction.PlayerId;
		MatchAction.CoreAction = EndTurnAction;
		Result.Actions.Add(MatchAction);
	}

	Result.bOk = true;
	return Result;
}

FWBMatchOperationResult WBMatchCoordinator::SubmitActionId(
	const int32 PlayerId,
	const FString& ActionId)
{
	if (!bInitialized)
	{
		return MakeOperationFailure(TEXT("match_not_initialized"));
	}
	const auto MakeCurrentFailure =
		[this](const FString& Reason)
		{
			FWBMatchOperationResult Result =
				MakeOperationFailure(Reason);
			Result.bTerminal = State.bGameOver;
			Result.bPendingDecision =
				IsTurnTransitionInProgress()
				|| (MatchPhase == EWBMatchLoopPhase::Response
					&& State.HasOpenReactionWindow());
			Result.PendingPlayerId =
				IsTurnTransitionInProgress()
					? GetPendingTurnStartDecisionPlayerId()
					: (Result.bPendingDecision ? State.PriorityPlayer : -1);
			Result.ActivePlayerId = State.CurrentPlayer;
			Result.TurnNumber = State.TurnNumber;
			Result.TraceBeginIndex = TraceLog.Num();
			Result.TraceEndIndex = TraceLog.Num();
			Result.bGameOver = State.bGameOver;
			Result.WinnerPlayerId = State.WinnerPlayerId;
			Result.LoserPlayerId = State.TerminalOutcome.LoserPlayerId;
			Result.TerminalReason =
				WBTerminalOutcomeNames::ReasonToName(State.TerminalOutcome.Reason);
			Result.TerminalSource =
				WBTerminalOutcomeNames::SourceToName(State.TerminalOutcome.Source);
			Result.TerminalTurnNumber = State.TerminalOutcome.TurnNumber;
			Result.TerminalRevision = State.TerminalOutcome.CoordinatorRevision;
			Result.TerminalTraceIndex = State.TerminalOutcome.TraceIndex;
			Result.CoordinatorGeneration = CoordinatorGeneration;
			Result.CoordinatorRevision = CoordinatorRevision;
			return Result;
		};
	if (State.bGameOver || MatchPhase == EWBMatchLoopPhase::GameOver)
	{
		return MakeCurrentFailure(TEXT("game_over"));
	}
	if (PlayerId != State.PriorityPlayer)
	{
		return MakeCurrentFailure(TEXT("wrong_player"));
	}
	if (ActionId.IsEmpty())
	{
		return MakeCurrentFailure(TEXT("action_id_missing"));
	}
	if (MatchPhase == EWBMatchLoopPhase::TurnStart
		&& !TurnStartSequence.bCompleted
		&& ActionId.StartsWith(TEXT("end_turn:")))
	{
		return MakeCurrentFailure(
			TEXT("turn_transition_pending_decision"));
	}

	const FWBMatchLegalActionGenerationResult LegalResult = EnumerateLegalActions();
	if (!LegalResult.bOk)
	{
		return MakeCurrentFailure(LegalResult.Reason);
	}

	const FWBMatchLegalAction* SelectedAction = nullptr;
	for (const FWBMatchLegalAction& Candidate : LegalResult.Actions)
	{
		if (Candidate.ActionId == ActionId)
		{
			SelectedAction = &Candidate;
			break;
		}
	}
	if (SelectedAction == nullptr)
	{
		return MakeCurrentFailure(TEXT("stale_or_illegal_action"));
	}

	FString ReplayActionFamily;
	if (!ClassifyReplayActionFamily(*SelectedAction, ReplayActionFamily))
	{
		return MakeCurrentFailure(
			TEXT("unsupported_replay_action_family"));
	}
	TArray<FString> CanonicalLegalActions;
	CanonicalLegalActions.Reserve(LegalResult.Actions.Num());
	for (const FWBMatchLegalAction& LegalAction : LegalResult.Actions)
	{
		FString Family;
		if (!ClassifyReplayActionFamily(LegalAction, Family))
		{
			return MakeCurrentFailure(
				TEXT("unsupported_replay_action_family"));
		}
		CanonicalLegalActions.Add(FString::Printf(
			TEXT("%s|p%d|%s"),
			*Family,
			LegalAction.PlayerId,
			*LegalAction.ActionId));
	}
	const FString LegalActionSetDigest =
		WBProductionMatchReplay::BuildLegalActionSetDigest(
			CanonicalLegalActions);
	const int32 BeforeGeneration = CoordinatorGeneration;
	const int32 BeforeRevision = CoordinatorRevision;
	const FString BeforeStateDigest = GetCurrentStateDigest();
	const FString ExpectedDecisionId =
		WBProductionMatchReplay::BuildDecisionId(
			BeforeGeneration,
			BeforeRevision,
			PlayerId,
			static_cast<int32>(MatchPhase),
			LegalActionSetDigest);

	const int32 TraceBeginIndex = TraceLog.Num();
	FWBGameStateData WorkingState = State;
	TArray<FWBPendingEffectActivationFrame> WorkingPendingEffects =
		PendingEffectActivations;
	int32 WorkingNextPendingEffectSequence = NextPendingEffectSequence;
	uint32 WorkingRandomState = RandomState;
	EWBMatchLoopPhase WorkingPhase = MatchPhase;
	FWBTurnStartSequenceState WorkingTurnStartSequence =
		TurnStartSequence;
	TArray<FWBTraceEvent> WorkingTraceEvents;
	FWBTraceEvent Submitted = MakeMatchTrace(
		FName(TEXT("action_submitted")),
		PlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Submitted.ActionId = ActionId;
	WorkingTraceEvents.Add(Submitted);

	FString FailureReason;
	bool bActionApplied = false;
	bool bWasReactionPass = false;
	EWBReactionWindowKind PendingReactionKind =
		EWBReactionWindowKind::None;
	int32 PendingReactionSourceUnitId = -1;
	int32 PendingReactionTargetUnitId = -1;
	if (SelectedAction->Family == EWBMatchActionFamily::CoreAction
		&& SelectedAction->CoreAction.Type == EWBActionType::EndTurn)
	{
		WorkingPhase = EWBMatchLoopPhase::TurnEnd;
		bActionApplied = ApplyTurnTransition(
			WorkingState,
			WorkingRandomState,
			WorkingPhase,
			WorkingTurnStartSequence,
			WorkingPendingEffects,
			WorkingTraceEvents,
			FailureReason);
	}
	else if (SelectedAction->Family
		== EWBMatchActionFamily::TurnStartTrigger)
	{
		const FWBTurnStartSequenceResult TurnStartResult =
			WBTurnStartSequence::SubmitChoice(
				WorkingState,
				Repository,
				ActionId,
				WorkingTurnStartSequence);
		bActionApplied = TurnStartResult.bOk;
		FailureReason = TurnStartResult.Reason;
		WorkingTraceEvents.Append(
			TurnStartResult.TraceEvents);
		if (bActionApplied
			&& TurnStartResult.bCompleted
			&& !WorkingState.bGameOver)
		{
			bActionApplied = ApplyAutomaticResolution(
				WorkingState,
				WorkingTraceEvents,
				FailureReason);
			if (bActionApplied)
			{
				WorkingTraceEvents.Add(MakeMatchTrace(
					FName(TEXT("turn_started")),
					WorkingState.CurrentPlayer,
					WorkingState.TurnNumber,
					PhaseToName(
						EWBMatchLoopPhase::Action)));
			}
		}
		WorkingPhase = WorkingState.bGameOver
			? EWBMatchLoopPhase::GameOver
			: (TurnStartResult.bCompleted
				? EWBMatchLoopPhase::Action
				: EWBMatchLoopPhase::TurnStart);
	}
	else
	{
		switch (SelectedAction->Family)
		{
		case EWBMatchActionFamily::CoreAction:
		{
			if (SelectedAction->CoreAction.Type == EWBActionType::PassResponse
				&& WorkingState.HasOpenReactionWindow())
			{
				bWasReactionPass = true;
				bActionApplied = ApplyReactionPass(
					WorkingState,
					WorkingPhase,
					WorkingPendingEffects,
					PlayerId,
					false,
					WorkingTraceEvents,
					FailureReason);
			}
			else
			{
				const FWBApplyActionResult ApplyResult =
					WBEffectRunner::ApplyAction(
						WorkingState,
						SelectedAction->CoreAction);
				bActionApplied = ApplyResult.bOk;
				FailureReason = ApplyResult.Reason;
				WorkingTraceEvents.Append(ApplyResult.TraceEvents);
			}
			if (bActionApplied && SelectedAction->CoreAction.Type == EWBActionType::Move)
			{
				const FWBMarkerResolutionResult MarkerResult =
					WBMarkerResolution::ResolveMarkerAtUnitTile(
						WorkingState,
						Repository,
						SelectedAction->CoreAction.SourceUnitId);
				bActionApplied = MarkerResult.bOk;
				FailureReason = MarkerResult.Reason;
				WorkingTraceEvents.Append(MarkerResult.TraceEvents);
				if (bActionApplied)
				{
					PendingReactionKind = EWBReactionWindowKind::PostMove;
					PendingReactionSourceUnitId =
						SelectedAction->CoreAction.SourceUnitId;
				}
			}
		if (bActionApplied
				&& SelectedAction->CoreAction.Type == EWBActionType::Attack
				&& WorkingState.HasPendingAttack())
			{
				FWBTraceEvent Started = MakeMatchTrace(
					FName(TEXT("attack_continuation_started")),
					PlayerId,
					WorkingState.TurnNumber,
					PhaseToName(WorkingPhase));
				Started.ActionId = ActionId;
				Started.SourceUnitId =
					WorkingState.PendingAttack.AttackerUnitId;
				Started.TargetUnitId =
					WorkingState.PendingAttack.DefenderUnitId;
				Started.AttackContinuationId =
					WorkingState.PendingAttack.ContinuationId;
				Started.AttackContinuationStage = FName(TEXT("pre_hit"));
				WorkingTraceEvents.Add(MoveTemp(Started));
				PendingReactionKind = EWBReactionWindowKind::PreHit;
				PendingReactionSourceUnitId =
					WorkingState.PendingAttack.AttackerUnitId;
				PendingReactionTargetUnitId =
					WorkingState.PendingAttack.DefenderUnitId;
			}
			break;
		}
		case EWBMatchActionFamily::Summon:
		{
			int32 CreatedUnitId = -1;
			if (SelectedAction->bHybridSummon)
			{
				const FWBHybridSummonResult ApplyResult =
					WBHybridSummon::ExecuteSummon(
						WorkingState,
						Repository,
						SelectedAction->HybridSummonPlan,
						CoordinatorGeneration,
						CoordinatorRevision);
				bActionApplied = ApplyResult.bOk;
				FailureReason = ApplyResult.Reason;
				WorkingTraceEvents.Append(ApplyResult.TraceEvents);
				CreatedUnitId = ApplyResult.NewHybridUnitId;
			}
			else
			{
				const FWBSummonExecutionResult ApplyResult =
					WBSummonExecution::ExecuteCharacterSummonFromHand(
						WorkingState,
						Repository,
						SelectedAction->SummonRequest);
				bActionApplied = ApplyResult.bOk;
				FailureReason = ApplyResult.Reason;
				AppendSummonTraceEvents(ApplyResult, WorkingTraceEvents);
				CreatedUnitId = ApplyResult.CreatedUnitId;
			}
			if (bActionApplied)
			{
				const FWBMarkerResolutionResult MarkerResult =
					WBMarkerResolution::ResolveMarkerAtUnitTile(
						WorkingState,
						Repository,
						CreatedUnitId);
				bActionApplied = MarkerResult.bOk;
				FailureReason = MarkerResult.Reason;
				WorkingTraceEvents.Append(MarkerResult.TraceEvents);
				if (bActionApplied)
				{
					PendingReactionKind =
						EWBReactionWindowKind::PostSummon;
					PendingReactionTargetUnitId = CreatedUnitId;
					const FWBPlayerStateData* PlayerState =
						WorkingState.GetPlayerById(PlayerId);
					PendingReactionSourceUnitId = PlayerState != nullptr
						? PlayerState->HeroUnitId
						: -1;
				}
			}
			break;
		}
		case EWBMatchActionFamily::Equip:
		{
			const FWBEquipExecutionResult ApplyResult =
				WBEquipExecution::ExecuteWandEquipFromHand(
					WorkingState,
					Repository,
					SelectedAction->EquipRequest);
			bActionApplied = ApplyResult.bOk;
			FailureReason = ApplyResult.Reason;
			AppendEquipTraceEvents(ApplyResult, WorkingTraceEvents);
			break;
		}
		case EWBMatchActionFamily::Activation:
		{
			bActionApplied = BeginPendingEffectActivation(
				WorkingState,
				WorkingPhase,
				*SelectedAction,
				WorkingPendingEffects,
				WorkingNextPendingEffectSequence,
				WorkingTraceEvents,
				FailureReason);
			break;
		}
		case EWBMatchActionFamily::Discard:
		{
			const FWBCardLifecycleResult ApplyResult =
				WBCardLifecycle::MoveHandCardToDiscard(
					WorkingState,
					PlayerId,
					SelectedAction->DiscardCardInstanceId);
			bActionApplied = ApplyResult.bOk;
			FailureReason = ApplyResult.Reason;
			if (ApplyResult.bOk)
			{
				FWBTraceEvent Discarded = MakeMatchTrace(
					FName(TEXT("card_discarded")),
					PlayerId,
					WorkingState.TurnNumber,
					PhaseToName(WorkingPhase));
				Discarded.CardInstanceId = ApplyResult.CardInstanceId;
				Discarded.CardId = ApplyResult.CardId;
				WorkingTraceEvents.Add(Discarded);
			}
			break;
		}
		default:
			FailureReason = TEXT("unsupported_match_action_family");
			break;
		}

		if (bActionApplied
			&& SelectedAction->Family != EWBMatchActionFamily::Activation)
		{
			bActionApplied = ApplyAutomaticResolution(
				WorkingState,
				WorkingTraceEvents,
				FailureReason);
		}
		if (bActionApplied && !WorkingState.bGameOver)
		{
			if (bWasReactionPass
				&& WorkingState.HasOpenReactionWindow())
			{
				bActionApplied = ApplyForcedReactionPasses(
					WorkingState,
					WorkingPhase,
					WorkingPendingEffects,
					WorkingTraceEvents,
					FailureReason);
			}
			else if (PendingReactionKind
				!= EWBReactionWindowKind::None)
			{
				bActionApplied = OpenReactionWindowIfApplicable(
					WorkingState,
					WorkingPhase,
					WorkingPendingEffects,
					PendingReactionKind,
					PlayerId,
					ActionId,
					PendingReactionSourceUnitId,
					PendingReactionTargetUnitId,
					WorkingTraceEvents,
					FailureReason);
				if (bActionApplied
					&& PendingReactionKind == EWBReactionWindowKind::PreHit
					&& !WorkingState.HasOpenReactionWindow())
				{
					bActionApplied = AdvanceAttackContinuation(
						WorkingState,
						WorkingPhase,
						WorkingPendingEffects,
						WorkingTraceEvents,
						FailureReason);
				}
			}
		}
	}

	if (bActionApplied
		&& WorkingState.NPCPhaseContinuation.bActive
		&& !WorkingState.HasPendingAttack()
		&& !WorkingState.HasOpenReactionWindow()
		&& !WorkingState.bGameOver)
	{
		bActionApplied = ResumeNPCPhaseAndTurnTransition(
			WorkingState,
			WorkingRandomState,
			WorkingPhase,
			WorkingTurnStartSequence,
			WorkingPendingEffects,
			WorkingTraceEvents,
			FailureReason);
	}

	if (!bActionApplied)
	{
		return MakeCurrentFailure(FailureReason.IsEmpty()
			? FString(TEXT("match_action_failed"))
			: FailureReason);
	}

	CommitTerminalOutcome(
		WorkingState,
		*SelectedAction,
		CoordinatorRevision + 1,
		TraceBeginIndex,
		WorkingTraceEvents);

	if (WorkingPhase != EWBMatchLoopPhase::TurnStart)
	{
		WorkingPhase = WorkingState.bGameOver
			? EWBMatchLoopPhase::GameOver
			: (WorkingState.IsResponsePhase()
				? EWBMatchLoopPhase::Response
				: EWBMatchLoopPhase::Action);
	}

	FWBTraceEvent Resolved = MakeMatchTrace(
		FName(TEXT("action_resolved")),
		PlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Resolved.ActionId = ActionId;
	WorkingTraceEvents.Add(Resolved);

	const FWBMatchLegalActionGenerationResult NextLegalResult =
		EnumerateLegalActionsForState(
			WorkingState,
			WorkingPhase,
			&WorkingTurnStartSequence,
			&WorkingPendingEffects);
	if (!NextLegalResult.bOk)
	{
		return MakeCurrentFailure(NextLegalResult.Reason);
	}

	State = WorkingState;
	RandomState = WorkingRandomState;
	MatchPhase = WorkingPhase;
	PendingEffectActivations = MoveTemp(WorkingPendingEffects);
	NextPendingEffectSequence = WorkingNextPendingEffectSequence;
	TurnStartSequence =
		MoveTemp(WorkingTurnStartSequence);
	TraceLog.Append(WorkingTraceEvents);
	++CoordinatorRevision;

	FWBMatchCommittedActionRecord CommittedRecord;
	CommittedRecord.RecordIndex =
		CommittedActionRecords.Num();
	CommittedRecord.ActingPlayer = PlayerId;
	CommittedRecord.ActionFamily = ReplayActionFamily;
	CommittedRecord.ChosenActionId = ActionId;
	CommittedRecord.ExpectedDecisionId = ExpectedDecisionId;
	CommittedRecord.BeforeGeneration = BeforeGeneration;
	CommittedRecord.BeforeRevision = BeforeRevision;
	CommittedRecord.BeforeStateDigest = BeforeStateDigest;
	CommittedRecord.LegalActionSetDigest = LegalActionSetDigest;
	CommittedRecord.AfterGeneration = CoordinatorGeneration;
	CommittedRecord.AfterRevision = CoordinatorRevision;
	CommittedRecord.bCompleted =
		!((MatchPhase == EWBMatchLoopPhase::TurnStart
				&& !TurnStartSequence.bCompleted)
			|| (MatchPhase == EWBMatchLoopPhase::Response
				&& State.HasOpenReactionWindow()));
	CommittedRecord.bPendingDecision =
		!CommittedRecord.bCompleted;
	CommittedRecord.PendingPlayer =
		CommittedRecord.bPendingDecision
			? State.PriorityPlayer
			: -1;
	CommittedRecord.bTerminal = State.bGameOver;
	CommittedRecord.WinnerPlayer = State.TerminalOutcome.WinnerPlayerId;
	CommittedRecord.LoserPlayer = State.TerminalOutcome.LoserPlayerId;
	CommittedRecord.TerminalReason =
		WBTerminalOutcomeNames::ReasonToName(State.TerminalOutcome.Reason);
	CommittedRecord.TerminalSource =
		WBTerminalOutcomeNames::SourceToName(State.TerminalOutcome.Source);
	CommittedRecord.TerminalTurn = State.TerminalOutcome.TurnNumber;
	CommittedRecord.TerminalRevision =
		State.TerminalOutcome.CoordinatorRevision;
	CommittedRecord.TerminalTraceIndex = State.TerminalOutcome.TraceIndex;
	CommittedRecord.TraceStart = TraceBeginIndex;
	CommittedRecord.TraceEnd = TraceLog.Num();
	CommittedRecord.TraceDigest =
		WBProductionMatchReplay::BuildTraceDigest(
			WorkingTraceEvents);
	CommittedRecord.AfterStateDigest = GetCurrentStateDigest();
	CommittedActionRecords.Add(MoveTemp(CommittedRecord));

	FWBMatchOperationResult Result;
	Result.bOk = true;
	Result.SubmittedActionId = ActionId;
	Result.TraceEvents = MoveTemp(WorkingTraceEvents);
	Result.NextLegalActions = NextLegalResult.Actions;
	Result.bPendingDecision =
		(MatchPhase == EWBMatchLoopPhase::TurnStart
			&& !TurnStartSequence.bCompleted)
		|| (MatchPhase == EWBMatchLoopPhase::Response
			&& State.HasOpenReactionWindow());
	Result.bCompleted = !Result.bPendingDecision;
	Result.bTerminal = State.bGameOver;
	Result.PendingPlayerId =
		Result.bPendingDecision ? State.PriorityPlayer : -1;
	Result.ActivePlayerId = State.CurrentPlayer;
	Result.TurnNumber = State.TurnNumber;
	Result.TraceBeginIndex = TraceBeginIndex;
	Result.TraceEndIndex = TraceLog.Num();
	Result.bGameOver = State.bGameOver;
	Result.WinnerPlayerId = State.WinnerPlayerId;
	Result.LoserPlayerId = State.TerminalOutcome.LoserPlayerId;
	Result.TerminalReason =
		WBTerminalOutcomeNames::ReasonToName(State.TerminalOutcome.Reason);
	Result.TerminalSource =
		WBTerminalOutcomeNames::SourceToName(State.TerminalOutcome.Source);
	Result.TerminalTurnNumber = State.TerminalOutcome.TurnNumber;
	Result.TerminalRevision = State.TerminalOutcome.CoordinatorRevision;
	Result.TerminalTraceIndex = State.TerminalOutcome.TraceIndex;
	Result.CoordinatorGeneration = CoordinatorGeneration;
	Result.CoordinatorRevision = CoordinatorRevision;
	return Result;
}

bool WBMatchCoordinator::ApplyAutomaticResolution(
	FWBGameStateData& WorkingState,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	const FWBActionQueryResult DeathQuery = WBRules::CanApplyZeroHPDeathRemoval(WorkingState);
	if (DeathQuery.bOk)
	{
		const FWBApplyActionResult DeathResult =
			WBDeathResolution::ApplyZeroHPDeathResolution(WorkingState);
		if (!DeathResult.bOk)
		{
			OutReason = DeathResult.Reason;
			return false;
		}
		OutTraceEvents.Append(DeathResult.TraceEvents);
	}

	if (!WorkingState.bGameOver)
	{
		TArray<const FWBUnitState*> Units;
		for (const FWBUnitState& Unit : WorkingState.Units)
		{
			if (Unit.IsUnitOnBoard() && FWBGameStateData::IsValidPlayerId(Unit.OwnerId))
			{
				Units.Add(&Unit);
			}
		}
		Units.Sort(UnitIdPointerLess);
		TArray<int32> UnitIds;
		for (const FWBUnitState* Unit : Units)
		{
			UnitIds.Add(Unit->UnitId);
		}

		for (const int32 UnitId : UnitIds)
		{
			const FWBResonanceOverflowResult OverflowResult =
				WBResonanceOverflow::ResolveOverflowForUnit(WorkingState, Repository, UnitId);
			if (!OverflowResult.bOk)
			{
				OutReason = OverflowResult.Reason;
				return false;
			}
			AppendOverflowTraceEvents(OverflowResult, OutTraceEvents);
		}
	}

	OutTraceEvents.Add(MakeMatchTrace(
		FName(TEXT("automatic_resolution")),
		WorkingState.CurrentPlayer,
		WorkingState.TurnNumber,
		PhaseToName(WorkingState.bGameOver
			? EWBMatchLoopPhase::GameOver
			: EWBMatchLoopPhase::Action)));
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::HasLegalReactForPriority(
	const FWBGameStateData& InState,
	const TArray<FWBPendingEffectActivationFrame>& InPendingEffects,
	FString& OutReason) const
{
	if (!InState.HasOpenReactionWindow()
		|| !InState.IsResponsePhase()
		|| !FWBGameStateData::IsValidPlayerId(InState.PriorityPlayer))
	{
		OutReason = TEXT("reaction_window_not_open");
		return false;
	}

	const FWBMatchLegalActionGenerationResult Result =
		GetActivationActions(
			InState,
			Repository,
			InState.PriorityPlayer,
			true,
			InPendingEffects.IsEmpty()
				? FString()
				: InPendingEffects.Last().FrameId,
			InState.HasPendingAttack()
				? InState.PendingAttack.ContinuationId
				: FString());
	if (!Result.bOk)
	{
		OutReason = Result.Reason;
		return false;
	}
	OutReason.Reset();
	return !Result.Actions.IsEmpty();
}

bool WBMatchCoordinator::OpenReactionWindowIfApplicable(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	const EWBReactionWindowKind Kind,
	const int32 OriginatingPlayerId,
	const FString& SourceActionId,
	const int32 SourceUnitId,
	const int32 TargetUnitId,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason,
	const int32 ExplicitFirstPriorityPlayerId) const
{
	if (Kind == EWBReactionWindowKind::None
		|| WorkingState.bGameOver
		|| WorkingState.bSuppressManualReactsDuringInitialHeroSetup)
	{
		OutReason.Reset();
		return true;
	}
	if (!FWBGameStateData::IsValidPlayerId(OriginatingPlayerId)
		&& !FWBGameStateData::IsValidPlayerId(ExplicitFirstPriorityPlayerId))
	{
		OutReason = TEXT("reaction_originating_player_invalid");
		return false;
	}
	if (WorkingState.HasOpenReactionWindow())
	{
		OutReason = TEXT("nested_reaction_window_not_supported");
		return false;
	}

	FWBGameStateData ProbeState = WorkingState;
	ProbeState.ReactionWindow.Kind = Kind;
	ProbeState.ReactionWindow.OriginatingPlayerId = OriginatingPlayerId;
	ProbeState.ReactionWindow.SourceActionId = SourceActionId;
	ProbeState.ReactionWindow.SourceUnitId = SourceUnitId;
	ProbeState.ReactionWindow.TargetUnitId = TargetUnitId;
	ProbeState.Phase = EWBGamePhase::Response;
	const int32 FirstPriorityPlayerId =
		FWBGameStateData::IsValidPlayerId(ExplicitFirstPriorityPlayerId)
			? ExplicitFirstPriorityPlayerId
			: 1 - OriginatingPlayerId;
	ProbeState.PriorityPlayer = FirstPriorityPlayerId;

	FString ProbeReason;
	const bool bFirstPlayerHasReact =
		HasLegalReactForPriority(
			ProbeState, WorkingPendingEffects, ProbeReason);
	if (!ProbeReason.IsEmpty())
	{
		OutReason = ProbeReason;
		return false;
	}
	const int32 OtherPlayerId = 1 - FirstPriorityPlayerId;
	ProbeState.PriorityPlayer = OtherPlayerId;
	const bool bOtherPlayerHasReact =
		HasLegalReactForPriority(
			ProbeState, WorkingPendingEffects, ProbeReason);
	if (!ProbeReason.IsEmpty())
	{
		OutReason = ProbeReason;
		return false;
	}
	if (!bFirstPlayerHasReact && !bOtherPlayerHasReact)
	{
		OutReason.Reset();
		return true;
	}

	WorkingState.ReactionWindow = ProbeState.ReactionWindow;
	WorkingState.PriorityPlayer = FirstPriorityPlayerId;
	WorkingState.Phase = EWBGamePhase::Response;
	WorkingPhase = EWBMatchLoopPhase::Response;
	FWBTraceEvent Opened = MakeMatchTrace(
		FName(TEXT("reaction_window_opened")),
		OriginatingPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Opened.ActionId = SourceActionId;
	Opened.FromPlayer = OriginatingPlayerId;
	Opened.ToPlayer = FirstPriorityPlayerId;
	Opened.SourceUnitId = SourceUnitId;
	Opened.TargetUnitId = TargetUnitId;
	Opened.ReactionWindowKind = ReactionWindowKindToName(Kind);
	Opened.ReactionPassCount = 0;
	OutTraceEvents.Add(MoveTemp(Opened));
	return ApplyForcedReactionPasses(
		WorkingState,
		WorkingPhase,
		WorkingPendingEffects,
		OutTraceEvents,
		OutReason);
}

bool WBMatchCoordinator::ApplyReactionPass(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	const int32 PassingPlayerId,
	const bool bAutomatic,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	if (!WorkingState.HasOpenReactionWindow()
		|| !WorkingState.IsResponsePhase()
		|| WorkingPhase != EWBMatchLoopPhase::Response)
	{
		OutReason = TEXT("reaction_window_not_open");
		return false;
	}
	if (PassingPlayerId != WorkingState.PriorityPlayer)
	{
		OutReason = TEXT("wrong_player");
		return false;
	}

	const EWBReactionWindowKind Kind = WorkingState.ReactionWindow.Kind;
	const int32 NextPlayerId = 1 - PassingPlayerId;
	++WorkingState.ReactionWindow.ConsecutivePassCount;
	FWBTraceEvent Passed = MakeMatchTrace(
		bAutomatic
			? FName(TEXT("reaction_auto_passed"))
			: FName(TEXT("pass_response")),
		PassingPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Passed.FromPlayer = PassingPlayerId;
	Passed.ToPlayer = NextPlayerId;
	Passed.ReactionWindowKind = ReactionWindowKindToName(Kind);
	Passed.ReactionPassCount =
		WorkingState.ReactionWindow.ConsecutivePassCount;
	OutTraceEvents.Add(MoveTemp(Passed));

	if (WorkingState.ReactionWindow.ConsecutivePassCount >= 2)
	{
		return CloseReactionWindow(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			OutTraceEvents,
			OutReason);
	}
	else
	{
		WorkingState.PriorityPlayer = NextPlayerId;
	}
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::BeginPendingEffectActivation(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	const FWBMatchLegalAction& Action,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	int32& WorkingNextPendingEffectSequence,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	if (Action.Family != EWBMatchActionFamily::Activation)
	{
		OutReason = TEXT("pending_effect_requires_activation");
		return false;
	}
	const FWBActionQueryResult Query =
		WBRules::CanApplyCardActivationCommand(
			WorkingState, Action.ActivationCommand);
	if (!Query.bOk)
	{
		OutReason = Query.Reason;
		return false;
	}

	FWBPendingEffectActivationFrame Frame;
	Frame.FrameId = FString::Printf(
		TEXT("pending_effect:g%d:r%d:f%d"),
		CoordinatorGeneration,
		CoordinatorRevision + 1,
		WorkingNextPendingEffectSequence++);
	Frame.ParentFrameId = WorkingPendingEffects.IsEmpty()
		? FString()
		: WorkingPendingEffects.Last().FrameId;
	Frame.ActivationActionId = Action.ActionId;
	Frame.ActivatingPlayerId = Action.PlayerId;
	Frame.Command = Action.ActivationCommand;

	if (Frame.Command.UsageCommit.bMarkUsageOnSuccess)
	{
		if (WorkingState.HasActivationUsageKeyThisTurn(
			Frame.Command.UsageCommit.PlayerId,
			Frame.Command.UsageCommit.UsageKey))
		{
			OutReason = TEXT("once_per_turn_already_used");
			return false;
		}
		WorkingState.MarkActivationUsageKeyForTest(
			Frame.Command.UsageCommit.PlayerId,
			Frame.Command.UsageCommit.UsageKey);
		Frame.Command.UsageCommit.bMarkUsageOnSuccess = false;
		FWBTraceEvent Reserved = MakeMatchTrace(
			FName(TEXT("card_activation_usage_reserved")),
			Action.PlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		Reserved.ActionId = Action.ActionId;
		Reserved.PendingEffectFrameId = Frame.FrameId;
		OutTraceEvents.Add(MoveTemp(Reserved));
	}

	if (Frame.Command.Source.SourceZone == EWBCardZone::Hand)
	{
		const FWBCardLifecycleResult DiscardResult =
			WBCardLifecycle::MoveHandCardToDiscard(
				WorkingState,
				Action.PlayerId,
				Frame.Command.Source.SourceCardInstanceId);
		if (!DiscardResult.bOk)
		{
			OutReason = TEXT("pending_effect_hand_source_unavailable");
			return false;
		}
		FWBTraceEvent Discarded = MakeMatchTrace(
			FName(TEXT("card_discarded_for_pending_effect")),
			Action.PlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		Discarded.ActionId = Action.ActionId;
		Discarded.CardInstanceId = DiscardResult.CardInstanceId;
		Discarded.CardId = DiscardResult.CardId;
		Discarded.PendingEffectFrameId = Frame.FrameId;
		OutTraceEvents.Add(MoveTemp(Discarded));
	}

	if (WorkingState.HasOpenReactionWindow())
	{
		Frame.bHasParentReaction = true;
		Frame.ParentReactionWindow = WorkingState.ReactionWindow;
		Frame.ParentReactionWindow.ConsecutivePassCount = 0;
		Frame.ParentPriorityPlayerId = 1 - Action.PlayerId;
		Frame.ParentGamePhase = WorkingState.Phase;
		Frame.ParentMatchPhase = WorkingPhase;

		FWBTraceEvent Reacted = MakeMatchTrace(
			FName(TEXT("reaction_resolved")),
			Action.PlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		Reacted.ActionId = Action.ActionId;
		Reacted.FromPlayer = Action.PlayerId;
		Reacted.ToPlayer = Frame.ParentPriorityPlayerId;
		Reacted.ReactionWindowKind = ReactionWindowKindToName(
			WorkingState.ReactionWindow.Kind);
		Reacted.ReactionPassCount = 0;
		Reacted.PendingEffectFrameId = Frame.FrameId;
		Reacted.ParentPendingEffectFrameId = Frame.ParentFrameId;
		OutTraceEvents.Add(MoveTemp(Reacted));
	}

	WorkingPendingEffects.Add(Frame);
	WorkingState.ReactionWindow.Reset();
	WorkingState.ReactionWindow.Kind = EWBReactionWindowKind::PostEffect;
	WorkingState.ReactionWindow.OriginatingPlayerId = Action.PlayerId;
	WorkingState.ReactionWindow.SourceActionId = Action.ActionId;
	WorkingState.ReactionWindow.SourceUnitId =
		Action.ActivationCommand.Source.SourceUnitId;
	WorkingState.ReactionWindow.TargetUnitId =
		Action.ActivationCommand.EffectRequest.Target.TargetUnitId;
	WorkingState.PriorityPlayer = 1 - Action.PlayerId;
	WorkingState.Phase = EWBGamePhase::Response;
	WorkingPhase = EWBMatchLoopPhase::Response;

	FWBTraceEvent Declared = MakeMatchTrace(
		FName(TEXT("pending_effect_activation_declared")),
		Action.PlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Declared.ActionId = Action.ActionId;
	Declared.SourceUnitId = Action.ActivationCommand.Source.SourceUnitId;
	Declared.TargetUnitId =
		Action.ActivationCommand.EffectRequest.Target.TargetUnitId;
	Declared.CardInstanceId =
		Action.ActivationCommand.Source.SourceCardInstanceId;
	Declared.CardId = Action.ActivationCommand.Source.SourceCardId;
	Declared.PendingEffectFrameId = Frame.FrameId;
	Declared.ParentPendingEffectFrameId = Frame.ParentFrameId;
	Declared.PendingEffectStackDepth = WorkingPendingEffects.Num();
	OutTraceEvents.Add(MoveTemp(Declared));

	FWBTraceEvent Opened = MakeMatchTrace(
		FName(TEXT("reaction_window_opened")),
		Action.PlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Opened.ActionId = Action.ActionId;
	Opened.FromPlayer = Action.PlayerId;
	Opened.ToPlayer = WorkingState.PriorityPlayer;
	Opened.SourceUnitId = Action.ActivationCommand.Source.SourceUnitId;
	Opened.TargetUnitId =
		Action.ActivationCommand.EffectRequest.Target.TargetUnitId;
	Opened.ReactionWindowKind = FName(TEXT("post_effect"));
	Opened.ReactionPassCount = 0;
	Opened.PendingEffectFrameId = Frame.FrameId;
	Opened.ParentPendingEffectFrameId = Frame.ParentFrameId;
	Opened.PendingEffectStackDepth = WorkingPendingEffects.Num();
	OutTraceEvents.Add(MoveTemp(Opened));

	return ApplyForcedReactionPasses(
		WorkingState,
		WorkingPhase,
		WorkingPendingEffects,
		OutTraceEvents,
		OutReason);
}

bool WBMatchCoordinator::ResolveTopPendingEffectActivation(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	if (WorkingPendingEffects.IsEmpty())
	{
		OutReason = TEXT("pending_effect_stack_empty");
		return false;
	}

	const FWBPendingEffectActivationFrame Frame = WorkingPendingEffects.Last();
	FWBTraceEvent Started = MakeMatchTrace(
		FName(TEXT("pending_effect_resolution_started")),
		Frame.ActivatingPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Started.ActionId = Frame.ActivationActionId;
	Started.PendingEffectFrameId = Frame.FrameId;
	Started.ParentPendingEffectFrameId = Frame.ParentFrameId;
	Started.PendingEffectStackDepth = WorkingPendingEffects.Num();
	Started.bPendingEffectNegated = Frame.bNegated;
	OutTraceEvents.Add(MoveTemp(Started));

	bool bResolutionSucceeded = true;
	FString ResolutionFailure;
	if (!Frame.bNegated)
	{
		for (const FWBGenericEffectPayload& Payload :
			Frame.Command.EffectRequest.Payloads)
		{
			if (Payload.Operation != EWBGenericEffectOp::NegatePendingEffect)
			{
				continue;
			}
			const int32 TargetIndex = WorkingPendingEffects.IndexOfByPredicate(
				[&Payload, &Frame](const FWBPendingEffectActivationFrame& Candidate)
				{
					return Candidate.FrameId == Payload.PendingEffectFrameId
						&& Candidate.FrameId != Frame.FrameId;
				});
			if (TargetIndex == INDEX_NONE)
			{
				bResolutionSucceeded = false;
				ResolutionFailure = TEXT("pending_effect_negation_target_missing");
				break;
			}
		}

		if (bResolutionSucceeded)
		{
			const FWBCardActivationCommandResult ApplyResult =
				WBEffectRunner::ApplyCardActivationCommand(
					WorkingState, Frame.Command);
			bResolutionSucceeded = ApplyResult.bOk;
			ResolutionFailure = ApplyResult.Reason;
			OutTraceEvents.Append(ApplyResult.TraceEvents);
		}

		if (bResolutionSucceeded)
		{
			for (const FWBGenericEffectPayload& Payload :
				Frame.Command.EffectRequest.Payloads)
			{
				if (Payload.Operation != EWBGenericEffectOp::NegatePendingEffect)
				{
					continue;
				}
				FWBPendingEffectActivationFrame* Target =
					WorkingPendingEffects.FindByPredicate(
						[&Payload, &Frame](FWBPendingEffectActivationFrame& Candidate)
						{
							return Candidate.FrameId == Payload.PendingEffectFrameId
								&& Candidate.FrameId != Frame.FrameId;
						});
				if (Target != nullptr)
				{
					Target->bNegated = true;
					FWBTraceEvent Negated = MakeMatchTrace(
						FName(TEXT("pending_effect_activation_negated")),
						Frame.ActivatingPlayerId,
						WorkingState.TurnNumber,
						PhaseToName(WorkingPhase));
					Negated.ActionId = Frame.ActivationActionId;
					Negated.PendingEffectFrameId = Target->FrameId;
					Negated.ParentPendingEffectFrameId = Frame.FrameId;
					Negated.PendingEffectStackDepth = WorkingPendingEffects.Num();
					Negated.bPendingEffectNegated = true;
					OutTraceEvents.Add(MoveTemp(Negated));
				}
			}
		}

		if (bResolutionSucceeded)
		{
			for (const FWBGenericEffectPayload& Payload :
				Frame.Command.EffectRequest.Payloads)
			{
				if (Payload.Operation != EWBGenericEffectOp::PreventPendingAttack)
				{
					continue;
				}
				if (!WorkingState.HasPendingAttack()
					|| Payload.PendingAttackContinuationId.IsEmpty()
					|| Payload.PendingAttackContinuationId
						!= WorkingState.PendingAttack.ContinuationId)
				{
					bResolutionSucceeded = false;
					ResolutionFailure = TEXT("pending_attack_target_mismatch");
					break;
				}
				WorkingState.PendingAttack.bPrevented = true;
				FWBTraceEvent Prevented = MakeMatchTrace(
					FName(TEXT("attack_prevented")),
					Frame.ActivatingPlayerId,
					WorkingState.TurnNumber,
					PhaseToName(WorkingPhase));
				Prevented.ActionId = Frame.ActivationActionId;
				Prevented.SourceUnitId = WorkingState.PendingAttack.AttackerUnitId;
				Prevented.TargetUnitId = WorkingState.PendingAttack.DefenderUnitId;
				Prevented.AttackContinuationId =
					WorkingState.PendingAttack.ContinuationId;
				Prevented.AttackContinuationStage = FName(TEXT("pre_hit"));
				Prevented.bAttackPrevented = true;
				OutTraceEvents.Add(MoveTemp(Prevented));
			}
		}
	}

	WorkingPendingEffects.Pop(EAllowShrinking::No);
	FWBTraceEvent Finished = MakeMatchTrace(
		Frame.bNegated
			? FName(TEXT("pending_effect_activation_skipped"))
			: (bResolutionSucceeded
				? FName(TEXT("pending_effect_activation_resolved"))
				: FName(TEXT("pending_effect_activation_failed"))),
		Frame.ActivatingPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Finished.ActionId = Frame.ActivationActionId;
	Finished.PendingEffectFrameId = Frame.FrameId;
	Finished.ParentPendingEffectFrameId = Frame.ParentFrameId;
	Finished.PendingEffectStackDepth = WorkingPendingEffects.Num();
	Finished.bPendingEffectNegated = Frame.bNegated;
	Finished.Reason = ResolutionFailure;
	OutTraceEvents.Add(MoveTemp(Finished));

	if (!Frame.bNegated && bResolutionSucceeded)
	{
		if (!ApplyAutomaticResolution(
			WorkingState, OutTraceEvents, OutReason))
		{
			return false;
		}
	}
	if (WorkingState.bGameOver)
	{
		WorkingPendingEffects.Reset();
		WorkingState.ClearReactionWindow();
		WorkingState.Phase = EWBGamePhase::NormalTurn;
		WorkingPhase = EWBMatchLoopPhase::GameOver;
		OutReason.Reset();
		return true;
	}

	if (Frame.bHasParentReaction)
	{
		WorkingState.ReactionWindow = Frame.ParentReactionWindow;
		RebindReactionTargetFromPendingAttack(WorkingState);
		WorkingState.PriorityPlayer = Frame.ParentPriorityPlayerId;
		WorkingState.Phase = Frame.ParentGamePhase;
		WorkingPhase = Frame.ParentMatchPhase;
		FWBTraceEvent Restored = MakeMatchTrace(
			FName(TEXT("pending_effect_parent_context_restored")),
			Frame.ActivatingPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		Restored.ActionId = Frame.ParentReactionWindow.SourceActionId;
		Restored.PendingEffectFrameId = Frame.ParentFrameId;
		Restored.ParentPendingEffectFrameId = WorkingPendingEffects.Num() > 1
			? WorkingPendingEffects[WorkingPendingEffects.Num() - 2].FrameId
			: FString();
		Restored.PendingEffectStackDepth = WorkingPendingEffects.Num();
		if (WorkingState.HasPendingAttack()
			&& WorkingState.PendingAttack.DefenderUnitId
				!= WorkingState.PendingAttack.OriginalDefenderUnitId)
		{
			Restored.SourceUnitId = WorkingState.PendingAttack.AttackerUnitId;
			Restored.TargetUnitId = WorkingState.PendingAttack.DefenderUnitId;
			Restored.AttackContinuationId =
				WorkingState.PendingAttack.ContinuationId;
			Restored.AttackContinuationStage = FName(TEXT("pre_hit"));
		}
		OutTraceEvents.Add(MoveTemp(Restored));
		return ApplyForcedReactionPasses(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			OutTraceEvents,
			OutReason);
	}

	WorkingState.PriorityPlayer = WorkingState.CurrentPlayer;
	WorkingState.Phase = EWBGamePhase::NormalTurn;
	WorkingPhase = EWBMatchLoopPhase::Action;
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::AdvanceReactionAfterReact(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	const int32 ReactingPlayerId,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	if (!WorkingState.HasOpenReactionWindow()
		|| WorkingPhase != EWBMatchLoopPhase::Response
		|| WorkingState.PriorityPlayer != ReactingPlayerId)
	{
		OutReason = TEXT("reaction_window_not_open");
		return false;
	}

	WorkingState.ReactionWindow.ConsecutivePassCount = 0;
	WorkingState.PriorityPlayer = 1 - ReactingPlayerId;
	FWBTraceEvent Resolved = MakeMatchTrace(
		FName(TEXT("reaction_resolved")),
		ReactingPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Resolved.FromPlayer = ReactingPlayerId;
	Resolved.ToPlayer = WorkingState.PriorityPlayer;
	Resolved.ReactionWindowKind = ReactionWindowKindToName(
		WorkingState.ReactionWindow.Kind);
	Resolved.ReactionPassCount = 0;
	OutTraceEvents.Add(MoveTemp(Resolved));
	return ApplyForcedReactionPasses(
		WorkingState,
		WorkingPhase,
		WorkingPendingEffects,
		OutTraceEvents,
		OutReason);
}

bool WBMatchCoordinator::ApplyForcedReactionPasses(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	int32 Guard = 0;
	while (WorkingState.HasOpenReactionWindow())
	{
		if (++Guard > 64)
		{
			OutReason = TEXT("reaction_auto_pass_guard_exceeded");
			return false;
		}
		FString LegalReason;
		if (HasLegalReactForPriority(
			WorkingState, WorkingPendingEffects, LegalReason))
		{
			OutReason.Reset();
			return true;
		}
		if (!LegalReason.IsEmpty())
		{
			OutReason = LegalReason;
			return false;
		}
		if (!ApplyReactionPass(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			WorkingState.PriorityPlayer,
			true,
			OutTraceEvents,
			OutReason))
		{
			return false;
		}
	}
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::CloseReactionWindow(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	FWBTraceEvent Closed = MakeMatchTrace(
		FName(TEXT("reaction_window_closed")),
		WorkingState.ReactionWindow.OriginatingPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(EWBMatchLoopPhase::Action));
	Closed.ActionId = WorkingState.ReactionWindow.SourceActionId;
	Closed.SourceUnitId = WorkingState.ReactionWindow.SourceUnitId;
	Closed.TargetUnitId = WorkingState.ReactionWindow.TargetUnitId;
	Closed.ReactionWindowKind = ReactionWindowKindToName(
		WorkingState.ReactionWindow.Kind);
	Closed.ReactionPassCount =
		WorkingState.ReactionWindow.ConsecutivePassCount;
	OutTraceEvents.Add(MoveTemp(Closed));
	const bool bClosesPendingEffect =
		WorkingState.ReactionWindow.Kind == EWBReactionWindowKind::PostEffect
		&& !WorkingPendingEffects.IsEmpty();
	const bool bClosesAttackWindow =
		WorkingState.ReactionWindow.Kind == EWBReactionWindowKind::PreHit
		|| WorkingState.ReactionWindow.Kind == EWBReactionWindowKind::PostHit;
	WorkingState.ClearReactionWindow();
	if (bClosesPendingEffect)
	{
		return ResolveTopPendingEffectActivation(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			OutTraceEvents,
			OutReason);
	}
	if (bClosesAttackWindow && WorkingState.HasPendingAttack())
	{
		return AdvanceAttackContinuation(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			OutTraceEvents,
			OutReason);
	}
	WorkingState.PriorityPlayer = WorkingState.CurrentPlayer;
	WorkingState.Phase = EWBGamePhase::NormalTurn;
	WorkingPhase = EWBMatchLoopPhase::Action;
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::AdvanceAttackContinuation(
	FWBGameStateData& WorkingState,
	EWBMatchLoopPhase& WorkingPhase,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	const auto ResolveFirstReactionPlayer = [&WorkingState]()
	{
		const FWBUnitState* Defender =
			WorkingState.GetUnitById(WorkingState.PendingAttack.DefenderUnitId);
		if (Defender != nullptr
			&& FWBGameStateData::IsValidPlayerId(Defender->OwnerId))
		{
			return Defender->OwnerId;
		}
		return 1 - WorkingState.CurrentPlayer;
	};
	auto AddStageTraceForAttack = [&](const FWBPendingAttackState& Attack, const FName Kind, const FName Stage)
	{
		FWBTraceEvent Event = MakeMatchTrace(
			Kind,
			Attack.AttackingPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		Event.ActionId = Attack.DeclarationActionId;
		Event.SourceUnitId = Attack.AttackerUnitId;
		Event.TargetUnitId = Attack.DefenderUnitId;
		Event.AttackContinuationId = Attack.ContinuationId;
		Event.AttackContinuationStage = Stage;
		Event.bAttackPrevented = Attack.bPrevented;
		Event.bCounterAttack = Attack.bCounter;
		OutTraceEvents.Add(MoveTemp(Event));
	};
	auto AddStageTrace = [&](const FName Kind, const FName Stage)
	{
		AddStageTraceForAttack(WorkingState.PendingAttack, Kind, Stage);
	};
	auto Complete = [&]()
	{
		AddStageTrace(
			FName(TEXT("attack_continuation_completed")),
			FName(TEXT("complete")));
		WorkingState.ClearPendingAttack();
		WorkingState.ClearReactionWindow();
		WorkingState.PriorityPlayer = WorkingState.CurrentPlayer;
		WorkingState.Phase = EWBGamePhase::NormalTurn;
		WorkingPhase = WorkingState.bGameOver
			? EWBMatchLoopPhase::GameOver
			: EWBMatchLoopPhase::Action;
		OutReason.Reset();
		return true;
	};

	int32 Guard = 0;
	while (WorkingState.HasPendingAttack() && !WorkingState.HasOpenReactionWindow())
	{
		if (++Guard > 24)
		{
			OutReason = TEXT("attack_continuation_guard_exceeded");
			return false;
		}
		if (WorkingState.bGameOver)
		{
			WorkingPendingEffects.Reset();
			return Complete();
		}

		const FName CurrentStage = [&WorkingState]()
		{
			switch (WorkingState.PendingAttack.Stage)
			{
			case EWBAttackContinuationStage::CalculateDamage:
				return FName(TEXT("calculate_damage"));
			case EWBAttackContinuationStage::SubstituteDamage:
				return FName(TEXT("substitute_damage"));
			case EWBAttackContinuationStage::ApplyDamage:
				return FName(TEXT("apply_damage"));
			case EWBAttackContinuationStage::CounterEligibility:
				return FName(TEXT("counter_eligibility"));
			default:
				return FName(NAME_None);
			}
		}();
		if (!CurrentStage.IsNone())
		{
			AddStageTrace(FName(TEXT("attack_stage_entered")), CurrentStage);
		}

		switch (WorkingState.PendingAttack.Stage)
		{
		case EWBAttackContinuationStage::PreHit:
		{
			AddStageTrace(FName(TEXT("attack_pre_hit_closed")), FName(TEXT("pre_hit")));
			const bool bPreventedBeforeDamage = WorkingState.PendingAttack.bPrevented;
			if (WBRules::CanResolvePendingAttackDamage(WorkingState).bOk == false)
			{
				AddStageTrace(FName(TEXT("attack_continuation_cancelled")), FName(TEXT("pre_hit")));
				return Complete();
			}
			WorkingState.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
			if (!bPreventedBeforeDamage)
			{
				AddStageTrace(
					FName(TEXT("attack_damage_started")),
					FName(TEXT("calculate_damage")));
			}
			break;
		}

		case EWBAttackContinuationStage::CalculateDamage:
		{
			const FWBApplyActionResult Calculated =
				WBEffectRunner::CalculatePendingAttackDamage(WorkingState);
			if (!Calculated.bOk)
			{
				OutReason = Calculated.Reason;
				return false;
			}
			OutTraceEvents.Append(Calculated.TraceEvents);
			break;
		}

		case EWBAttackContinuationStage::SubstituteDamage:
		{
			const FWBApplyActionResult Substituted =
				WBEffectRunner::ResolvePendingAttackDamageSubstitution(WorkingState);
			if (!Substituted.bOk)
			{
				OutReason = Substituted.Reason;
				return false;
			}
			OutTraceEvents.Append(Substituted.TraceEvents);
			break;
		}

		case EWBAttackContinuationStage::ApplyDamage:
		{
			const FWBPendingAttackState AttackBeforeDamage = WorkingState.PendingAttack;
			{
				const FWBApplyActionResult Damage =
					WBEffectRunner::ApplyCalculatedPendingAttackDamage(
						WorkingState, true);
				if (!Damage.bOk)
				{
					OutReason = Damage.Reason;
					return false;
				}
				OutTraceEvents.Append(Damage.TraceEvents);
				if (AttackBeforeDamage.AuthorityKind
					== EWBAttackAuthorityKind::NeutralNPC)
				{
					FWBTraceEvent NPCResolved = MakeMatchTrace(
						FName(TEXT("npc_attack_damage_resolved")),
						-1,
						WorkingState.TurnNumber,
						FName(TEXT("NPCPhase")));
					NPCResolved.SourceUnitId = AttackBeforeDamage.AttackerUnitId;
					NPCResolved.TargetUnitId = AttackBeforeDamage.DefenderUnitId;
					NPCResolved.ActionSequence =
						WorkingState.NPCPhaseContinuation.CurrentActionSequence;
					if (const FWBUnitState* NPC =
						WorkingState.GetUnitById(AttackBeforeDamage.AttackerUnitId))
					{
						NPCResolved.SpawnOrder = NPC->NPCSpawnOrder;
						NPCResolved.CardId = NPC->CardId;
					}
					if (const FWBTraceEvent* CoreDamage = Damage.TraceEvents.FindByPredicate(
						[](const FWBTraceEvent& Event)
						{
							return Event.Kind == FName(TEXT("attack_damage_resolved"));
						}))
					{
						NPCResolved.DamageAmount = CoreDamage->DamageAmount;
						NPCResolved.FinalDamageAmount = CoreDamage->FinalDamageAmount;
						NPCResolved.ArmorAbsorbedAmount = CoreDamage->ArmorAbsorbedAmount;
						NPCResolved.HPDamageAmount = CoreDamage->HPDamageAmount;
						NPCResolved.PreviousHP = CoreDamage->PreviousHP;
						NPCResolved.NewHP = CoreDamage->NewHP;
					}
					OutTraceEvents.Add(MoveTemp(NPCResolved));
				}
			}
			if (WorkingState.bGameOver || !WorkingState.HasPendingAttack())
			{
				AddStageTraceForAttack(
					AttackBeforeDamage,
					FName(TEXT("attack_continuation_completed")),
					FName(TEXT("complete")));
				WorkingPendingEffects.Reset();
				WorkingState.ClearReactionWindow();
				WorkingPhase = WorkingState.bGameOver
					? EWBMatchLoopPhase::GameOver
					: EWBMatchLoopPhase::Action;
				OutReason.Reset();
				return true;
			}
			AddStageTrace(
				FName(TEXT("attack_damage_resolved_stage")),
				FName(TEXT("apply_damage")));
			if (AttackBeforeDamage.bPrevented)
			{
				return Complete();
			}
			if (WorkingState.GetUnitById(WorkingState.PendingAttack.DefenderUnitId) == nullptr)
			{
				return Complete();
			}
			if (!OpenReactionWindowIfApplicable(
				WorkingState,
				WorkingPhase,
				WorkingPendingEffects,
				EWBReactionWindowKind::PostHit,
				WorkingState.PendingAttack.AttackingPlayerId,
				WorkingState.PendingAttack.DeclarationActionId,
				WorkingState.PendingAttack.AttackerUnitId,
				WorkingState.PendingAttack.DefenderUnitId,
				OutTraceEvents,
				OutReason,
				ResolveFirstReactionPlayer()))
			{
				return false;
			}
			if (WorkingState.HasOpenReactionWindow())
			{
				return true;
			}
			break;
		}

		case EWBAttackContinuationStage::PostHit:
		{
			WorkingState.PendingAttack.bPostHitCompleted = true;
			AddStageTrace(FName(TEXT("attack_post_hit_closed")), FName(TEXT("post_hit")));
			WorkingState.PendingAttack.Stage =
				EWBAttackContinuationStage::CounterEligibility;
			break;
		}

		case EWBAttackContinuationStage::CounterEligibility:
		{
			AddStageTrace(
				FName(TEXT("attack_counter_eligibility_evaluated")),
				FName(TEXT("counter_eligibility")));
			if (WorkingState.PendingAttack.bCounter
				|| WorkingState.PendingAttack.bFrozenBroken)
			{
				return Complete();
			}
			WorkingState.PendingAttack.Stage = EWBAttackContinuationStage::Counter;
			if (!WBRules::CanResolveCounterattack(WorkingState, Repository).bOk)
			{
				return Complete();
			}
			{
				const int32 OriginalAttacker = WorkingState.PendingAttack.AttackerUnitId;
				const int32 OriginalDefender = WorkingState.PendingAttack.DefenderUnitId;
				const FWBUnitState* CounterAttacker = WorkingState.GetUnitById(OriginalDefender);
				const FWBUnitState* CounterDefender = WorkingState.GetUnitById(OriginalAttacker);
				if (CounterAttacker == nullptr || CounterDefender == nullptr)
				{
					return Complete();
				}
				WorkingState.PendingAttack.AttackerUnitId = OriginalDefender;
				WorkingState.PendingAttack.DefenderUnitId = OriginalAttacker;
				WorkingState.PendingAttack.AttackingPlayerId = CounterAttacker->OwnerId;
				WorkingState.PendingAttack.AuthorityKind =
					CounterAttacker->OwnerId == -1
						? EWBAttackAuthorityKind::NeutralNPC
						: EWBAttackAuthorityKind::Player;
				WorkingState.PendingAttack.AttackerTile = FWBTile(CounterAttacker->X, CounterAttacker->Y);
				WorkingState.PendingAttack.DefenderTile = FWBTile(CounterDefender->X, CounterDefender->Y);
				WorkingState.PendingAttack.bCounter = true;
				WorkingState.PendingAttack.bDamageResolved = false;
				WorkingState.PendingAttack.bPostHitCompleted = false;
				WorkingState.PendingAttack.bFrozenBroken = false;
				WorkingState.PendingAttack.DamageCalculation = {};
				WorkingState.PendingAttack.DamageSubstitution = {};
				WorkingState.PendingAttack.FinalDamageRecipientUnitId = INDEX_NONE;
				WorkingState.PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
				AddStageTrace(FName(TEXT("counter_started")), FName(TEXT("counter")));
			}
			const FWBUnitState* CounterTarget =
				WorkingState.GetUnitById(WorkingState.PendingAttack.DefenderUnitId);
			const bool bNeutralCounterTarget =
				CounterTarget != nullptr && CounterTarget->OwnerId == -1;
			if (bNeutralCounterTarget)
			{
				break;
			}
			if (!OpenReactionWindowIfApplicable(
				WorkingState,
				WorkingPhase,
				WorkingPendingEffects,
				EWBReactionWindowKind::PreHit,
				WorkingState.PendingAttack.AttackingPlayerId,
				WorkingState.PendingAttack.DeclarationActionId,
				WorkingState.PendingAttack.AttackerUnitId,
				WorkingState.PendingAttack.DefenderUnitId,
				OutTraceEvents,
				OutReason,
				ResolveFirstReactionPlayer()))
			{
				return false;
			}
			if (WorkingState.HasOpenReactionWindow())
			{
				return true;
			}
			break;
		}

		case EWBAttackContinuationStage::Damage:
		case EWBAttackContinuationStage::Counter:
		case EWBAttackContinuationStage::Complete:
		case EWBAttackContinuationStage::None:
		default:
			return Complete();
		}
	}
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::ApplyTurnTransition(
	FWBGameStateData& WorkingState,
	uint32& WorkingRandomState,
	EWBMatchLoopPhase& WorkingPhase,
	FWBTurnStartSequenceState& WorkingTurnStartSequence,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	const int32 EndingPlayerId = WorkingState.CurrentPlayer;
	const int32 EndingTurnNumber = WorkingState.TurnNumber;
	const FWBApplyActionResult EndStatusResult =
		WBEffectRunner::ApplyEndOfTurnStatusTicks(WorkingState, EndingPlayerId);
	if (!EndStatusResult.bOk)
	{
		OutReason = EndStatusResult.Reason;
		return false;
	}
	OutTraceEvents.Append(EndStatusResult.TraceEvents);
	if (WorkingState.bGameOver)
	{
		WorkingPhase = EWBMatchLoopPhase::GameOver;
		OutTraceEvents.Add(MakeMatchTrace(
			FName(TEXT("automatic_resolution")),
			EndingPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase)));
		OutReason.Reset();
		return true;
	}

	FWBAction EndTurnAction;
	EndTurnAction.Type = EWBActionType::EndTurn;
	EndTurnAction.PlayerId = EndingPlayerId;
	const FWBApplyActionResult EndTurnResult =
		WBEffectRunner::ApplyEndTurn(WorkingState, EndTurnAction);
	if (!EndTurnResult.bOk)
	{
		OutReason = EndTurnResult.Reason;
		return false;
	}
	OutTraceEvents.Append(EndTurnResult.TraceEvents);
	OutTraceEvents.Add(MakeMatchTrace(
		FName(TEXT("turn_ended")),
		EndingPlayerId,
		EndingTurnNumber,
		PhaseToName(EWBMatchLoopPhase::TurnEnd)));

	WorkingPhase = EWBMatchLoopPhase::NPCPhase;
	const FWBNPCPhaseResolutionResult NPCPhaseResult =
		WBNPCPhaseResolution::BeginPhase(
			WorkingState,
			Repository,
			EndingPlayerId);
	if (!NPCPhaseResult.bOk)
	{
		OutReason = NPCPhaseResult.Reason;
		return false;
	}
	OutTraceEvents.Append(NPCPhaseResult.TraceEvents);
	return ResumeNPCPhaseAndTurnTransition(
		WorkingState,
		WorkingRandomState,
		WorkingPhase,
		WorkingTurnStartSequence,
		WorkingPendingEffects,
		OutTraceEvents,
		OutReason);
}

bool WBMatchCoordinator::BeginTurnStartAfterNPCPhase(
	FWBGameStateData& WorkingState,
	uint32& WorkingRandomState,
	EWBMatchLoopPhase& WorkingPhase,
	FWBTurnStartSequenceState& WorkingTurnStartSequence,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	const int32 NextPlayerId = WorkingState.CurrentPlayer;
	const int32 EndingPlayerId = 1 - NextPlayerId;
	WorkingPhase = EWBMatchLoopPhase::TurnStart;
	const int32 MPRoll = RollD6(WorkingRandomState);
	WorkingTurnStartSequence =
		FWBTurnStartSequenceState();
	const FWBTurnStartSequenceResult TurnStartResult =
		WBTurnStartSequence::Begin(
			WorkingState,
			Repository,
			NextPlayerId,
			MPRoll,
			WorkingTurnStartSequence);
	if (!TurnStartResult.bOk)
	{
		OutReason = TurnStartResult.Reason;
		return false;
	}
	OutTraceEvents.Append(TurnStartResult.TraceEvents);

	FWBTraceEvent Advanced = MakeMatchTrace(
		FName(TEXT("player_advanced")),
		NextPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(TurnStartResult.bCompleted
			? EWBMatchLoopPhase::Action
			: EWBMatchLoopPhase::TurnStart));
	Advanced.FromPlayer = EndingPlayerId;
	Advanced.ToPlayer = NextPlayerId;
	OutTraceEvents.Add(Advanced);
	if (TurnStartResult.bCompleted)
	{
		OutTraceEvents.Add(MakeMatchTrace(
			FName(TEXT("turn_started")),
			NextPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(EWBMatchLoopPhase::Action)));
		if (!ApplyAutomaticResolution(
			WorkingState,
			OutTraceEvents,
			OutReason))
		{
			return false;
		}
	}

	WorkingPhase = WorkingState.bGameOver
		? EWBMatchLoopPhase::GameOver
		: (TurnStartResult.bCompleted
			? EWBMatchLoopPhase::Action
			: EWBMatchLoopPhase::TurnStart);
	OutReason.Reset();
	return true;
}

bool WBMatchCoordinator::ResumeNPCPhaseAndTurnTransition(
	FWBGameStateData& WorkingState,
	uint32& WorkingRandomState,
	EWBMatchLoopPhase& WorkingPhase,
	FWBTurnStartSequenceState& WorkingTurnStartSequence,
	TArray<FWBPendingEffectActivationFrame>& WorkingPendingEffects,
	TArray<FWBTraceEvent>& OutTraceEvents,
	FString& OutReason) const
{
	WorkingPhase = EWBMatchLoopPhase::NPCPhase;
	int32 Guard = 0;
	while (WorkingState.NPCPhaseContinuation.bActive
		&& !WorkingState.bGameOver)
	{
		if (++Guard > 128)
		{
			OutReason = TEXT("npc_phase_continuation_guard_exceeded");
			return false;
		}
		if (WorkingState.NPCPhaseContinuation.bWaitingForAttackContinuation)
		{
			if (WorkingState.HasPendingAttack()
				|| WorkingState.HasOpenReactionWindow())
			{
				OutReason = TEXT("npc_phase_attack_still_pending");
				return false;
			}
			if (!ApplyAutomaticResolution(
				WorkingState,
				OutTraceEvents,
				OutReason))
			{
				return false;
			}
			WorkingState.NPCPhaseContinuation.bWaitingForAttackContinuation = false;
			if (WorkingState.bGameOver)
			{
				break;
			}
		}
		const FWBNPCPhaseResolutionResult NPCResult =
			WBNPCPhaseResolution::AdvanceUntilAttackOrComplete(
				WorkingState,
				Repository,
				WorkingRandomState);
		if (!NPCResult.bOk)
		{
			OutReason = NPCResult.Reason;
			return false;
		}
		OutTraceEvents.Append(NPCResult.TraceEvents);
		if (!NPCResult.bPausedForAttack)
		{
			break;
		}

		FWBTraceEvent Started = MakeMatchTrace(
			FName(TEXT("attack_continuation_started")),
			-1,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		Started.ActionId = WorkingState.PendingAttack.DeclarationActionId;
		Started.SourceUnitId = WorkingState.PendingAttack.AttackerUnitId;
		Started.TargetUnitId = WorkingState.PendingAttack.DefenderUnitId;
		Started.AttackContinuationId = WorkingState.PendingAttack.ContinuationId;
		Started.AttackContinuationStage = FName(TEXT("pre_hit"));
		OutTraceEvents.Add(MoveTemp(Started));

		const FWBUnitState* Defender =
			WorkingState.GetUnitById(WorkingState.PendingAttack.DefenderUnitId);
		if (Defender == nullptr
			|| !FWBGameStateData::IsValidPlayerId(Defender->OwnerId))
		{
			OutReason = TEXT("npc_attack_response_owner_invalid");
			return false;
		}
		if (!OpenReactionWindowIfApplicable(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			EWBReactionWindowKind::PreHit,
			-1,
			WorkingState.PendingAttack.DeclarationActionId,
			WorkingState.PendingAttack.AttackerUnitId,
			WorkingState.PendingAttack.DefenderUnitId,
			OutTraceEvents,
			OutReason,
			Defender->OwnerId))
		{
			return false;
		}
		if (WorkingState.HasOpenReactionWindow())
		{
			return true;
		}
		if (!AdvanceAttackContinuation(
			WorkingState,
			WorkingPhase,
			WorkingPendingEffects,
			OutTraceEvents,
			OutReason))
		{
			return false;
		}
		if (WorkingState.HasOpenReactionWindow())
		{
			return true;
		}
		if (!ApplyAutomaticResolution(
			WorkingState,
			OutTraceEvents,
			OutReason))
		{
			return false;
		}
		WorkingState.NPCPhaseContinuation.bWaitingForAttackContinuation = false;
	}

	if (WorkingState.bGameOver)
	{
		WorkingState.NPCPhaseContinuation.Reset();
		WorkingPhase = EWBMatchLoopPhase::GameOver;
		OutTraceEvents.Add(MakeMatchTrace(
			FName(TEXT("automatic_resolution")),
			WorkingState.CurrentPlayer,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase)));
		OutReason.Reset();
		return true;
	}
	return BeginTurnStartAfterNPCPhase(
		WorkingState,
		WorkingRandomState,
		WorkingPhase,
		WorkingTurnStartSequence,
		OutTraceEvents,
		OutReason);
}

FWBMatchObservation WBMatchCoordinator::BuildObservation(const int32 ViewerPlayerId) const
{
	FWBMatchObservation Observation;
	Observation.ViewerPlayerId = ViewerPlayerId;
	Observation.MatchPhase = MatchPhase;
	if (!bInitialized)
	{
		return Observation;
	}

	Observation.PublicTurn = WBPublicTurnSummary::Build(State);
	Observation.PublicBoard = WBPublicBoardSummary::Build(State);
	Observation.CardZones = WBCardZoneObservation::BuildObservationForPlayer(State, ViewerPlayerId);
	if (ViewerPlayerId == State.PriorityPlayer)
	{
		const FWBMatchLegalActionGenerationResult LegalResult = EnumerateLegalActions();
		if (LegalResult.bOk)
		{
			Observation.LegalActions = LegalResult.Actions;
		}
	}
	return Observation;
}

bool WBMatchCoordinator::IsInitialized() const
{
	return bInitialized;
}

EWBMatchLoopPhase WBMatchCoordinator::GetMatchPhase() const
{
	return MatchPhase;
}

FName WBMatchCoordinator::GetMatchPhaseName() const
{
	return PhaseToName(MatchPhase);
}

int32 WBMatchCoordinator::GetFirstPlayerId() const
{
	return FirstPlayerId;
}

bool WBMatchCoordinator::WasHeroSpawnBatchCommitted() const
{
	return bHeroSpawnBatchCommitted;
}

bool WBMatchCoordinator::WereHeroSetupTriggersResolved() const
{
	return bHeroSetupTriggersResolved;
}

bool WBMatchCoordinator::WereOpeningHandsDrawn() const
{
	return bOpeningHandsDrawn;
}

bool WBMatchCoordinator::WasTurnStartCompleted() const
{
	return TurnStartSequence.bCompleted;
}

bool WBMatchCoordinator::IsTurnTransitionInProgress() const
{
	return bInitialized
		&& MatchPhase == EWBMatchLoopPhase::TurnStart
		&& !TurnStartSequence.bCompleted;
}

bool WBMatchCoordinator::HasPendingTurnStartDecision() const
{
	return IsTurnTransitionInProgress();
}

int32 WBMatchCoordinator::GetPendingTurnStartDecisionPlayerId() const
{
	return HasPendingTurnStartDecision()
		? State.PriorityPlayer
		: -1;
}

const FWBTurnStartSequenceState&
WBMatchCoordinator::GetTurnStartSequenceState() const
{
	return TurnStartSequence;
}

const FWBGameStateData& WBMatchCoordinator::GetState() const
{
	return State;
}

const FWBCardDefinitionRepository& WBMatchCoordinator::GetRepository() const
{
	return Repository;
}

const TArray<FWBTraceEvent>& WBMatchCoordinator::GetTraceLog() const
{
	return TraceLog;
}

int32 WBMatchCoordinator::GetCoordinatorGeneration() const
{
	return CoordinatorGeneration;
}

int32 WBMatchCoordinator::GetCoordinatorRevision() const
{
	return CoordinatorRevision;
}

const FString& WBMatchCoordinator::GetInitialStateDigest() const
{
	return InitialStateDigest;
}

const FString& WBMatchCoordinator::GetInitialTraceDigest() const
{
	return InitialTraceDigest;
}

FString WBMatchCoordinator::GetCurrentStateDigest() const
{
	return WBProductionMatchReplay::BuildCoordinatorStateDigest(
		State,
		static_cast<int32>(MatchPhase),
		RandomState,
		TurnStartSequence,
		BuildPendingEffectCanonicalState(PendingEffectActivations));
}

FString WBMatchCoordinator::GetCurrentTraceDigest() const
{
	return WBProductionMatchReplay::BuildTraceDigest(TraceLog);
}

const TArray<FWBMatchCommittedActionRecord>&
WBMatchCoordinator::GetCommittedActionRecords() const
{
	return CommittedActionRecords;
}

const TArray<FWBPendingEffectActivationFrame>&
WBMatchCoordinator::GetPendingEffectActivationStack() const
{
	return PendingEffectActivations;
}

bool WBMatchCoordinator::ClassifyReplayActionFamily(
	const FWBMatchLegalAction& Action,
	FString& OutFamily)
{
	OutFamily.Reset();
	switch (Action.Family)
	{
	case EWBMatchActionFamily::CoreAction:
		switch (Action.CoreAction.Type)
		{
		case EWBActionType::Move:
			OutFamily = TEXT("move");
			return true;
		case EWBActionType::Attack:
			OutFamily = TEXT("attack");
			return true;
		case EWBActionType::Pass:
			OutFamily = TEXT("pass");
			return true;
		case EWBActionType::EndTurn:
			OutFamily = TEXT("end_turn");
			return true;
		case EWBActionType::PassResponse:
			OutFamily = TEXT("pass_react");
			return true;
		case EWBActionType::None:
		default:
			return false;
		}
	case EWBMatchActionFamily::Summon:
		OutFamily = TEXT("summon");
		return true;
	case EWBMatchActionFamily::Equip:
		OutFamily = TEXT("equip");
		return true;
	case EWBMatchActionFamily::Activation:
		OutFamily = TEXT("activate");
		return true;
	case EWBMatchActionFamily::Discard:
		OutFamily = TEXT("discard");
		return true;
	case EWBMatchActionFamily::TurnStartTrigger:
		OutFamily = TEXT("turn_start_trigger_order");
		return true;
	case EWBMatchActionFamily::Count:
	default:
		return false;
	}
}

FWBGameStateData& WBMatchCoordinator::GetMutableStateForTest()
{
	return State;
}

FWBApplyActionResult WBMatchCoordinator::ApplyLegacyCompatibilityTurnTransition(
	FWBGameStateData& InOutState,
	const int32 EndingPlayerId,
	const int32 NextPlayerExplicitMPRoll)
{
	FWBApplyActionResult Result;

	FString Reason;
	if (!WBRules::CanApplyDeterministicTurnTransition(
		InOutState,
		EndingPlayerId,
		NextPlayerExplicitMPRoll,
		Reason))
	{
		Result.Reason = Reason;
		return Result;
	}

	FWBGameStateData WorkingState = InOutState;
	const int32 TurnNumberBefore = WorkingState.TurnNumber;

	const FWBApplyActionResult EndStatusResult =
		WBEffectRunner::ApplyEndOfTurnStatusTicks(
			WorkingState,
			EndingPlayerId);
	if (!EndStatusResult.bOk)
	{
		Result.Reason = EndStatusResult.Reason;
		return Result;
	}

	FWBAction EndTurnAction;
	EndTurnAction.Type = EWBActionType::EndTurn;
	EndTurnAction.PlayerId = EndingPlayerId;
	const FWBApplyActionResult EndTurnResult =
		WBEffectRunner::ApplyEndTurn(
			WorkingState,
			EndTurnAction);
	if (!EndTurnResult.bOk)
	{
		Result.Reason = EndTurnResult.Reason;
		return Result;
	}

	const int32 NextPlayerId = WorkingState.CurrentPlayer;
	const FWBApplyActionResult ResourceSetupResult =
		WBEffectRunner::ApplyTurnStartResourceSetup(
			WorkingState,
			NextPlayerId,
			NextPlayerExplicitMPRoll);
	if (!ResourceSetupResult.bOk)
	{
		Result.Reason = ResourceSetupResult.Reason;
		return Result;
	}

	const FWBApplyActionResult StartStatusResult =
		WBEffectRunner::ApplyStartOfTurnStatusTicks(
			WorkingState,
			NextPlayerId);
	if (!StartStatusResult.bOk)
	{
		Result.Reason = StartStatusResult.Reason;
		return Result;
	}

	Result.bOk = true;
	Result.TraceEvents.Add(MakeLegacyTurnTransitionTrace(
		EndingPlayerId,
		NextPlayerId,
		TurnNumberBefore,
		WorkingState.TurnNumber,
		NextPlayerExplicitMPRoll));
	Result.TraceEvents.Append(EndStatusResult.TraceEvents);
	Result.TraceEvents.Append(EndTurnResult.TraceEvents);
	Result.TraceEvents.Append(ResourceSetupResult.TraceEvents);
	Result.TraceEvents.Append(StartStatusResult.TraceEvents);

	InOutState = MoveTemp(WorkingState);
	return Result;
}

int32 WBMatchCoordinator::RollD6(uint32& InOutRandomState)
{
	return WBNPCPhaseResolution::RollD6(InOutRandomState);
}

FName WBMatchCoordinator::PhaseToName(const EWBMatchLoopPhase Phase)
{
	switch (Phase)
	{
	case EWBMatchLoopPhase::Setup:
		return FName(TEXT("setup"));
	case EWBMatchLoopPhase::TurnStart:
		return FName(TEXT("turn_start"));
	case EWBMatchLoopPhase::Action:
		return FName(TEXT("action"));
	case EWBMatchLoopPhase::Response:
		return FName(TEXT("response"));
	case EWBMatchLoopPhase::TurnEnd:
		return FName(TEXT("turn_end"));
	case EWBMatchLoopPhase::NPCPhase:
		return FName(TEXT("npc_phase"));
	case EWBMatchLoopPhase::GameOver:
		return FName(TEXT("game_over"));
	case EWBMatchLoopPhase::Uninitialized:
	default:
		return FName(TEXT("uninitialized"));
	}
}
