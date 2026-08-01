#include "WBMatchCoordinator.h"

#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"
#include "WBMarkerResolution.h"
#include "WBNPCPhaseResolution.h"
#include "WBResonanceOverflow.h"
#include "WBRules.h"

namespace
{
constexpr int32 OpeningHandSize = 6;

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
	const int32 OwnerPlayerId,
	const EWBCardActivationSourceZone Zone,
	const int32 EquippedToUnitId = -1)
{
	FWBCardActivationFixtureZoneEntry Entry;
	Entry.CardId = CardId;
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
			AppendFixtureZoneEntry(Context, Entry.Card.CardId, PlayerZones.PlayerId, EWBCardActivationSourceZone::Deck);
		}
		for (const FWBZoneCardEntry& Entry : PlayerZones.Hand)
		{
			AppendFixtureZoneEntry(Context, Entry.Card.CardId, PlayerZones.PlayerId, EWBCardActivationSourceZone::Hand);
		}
		for (const FWBZoneCardEntry& Entry : PlayerZones.Discard)
		{
			AppendFixtureZoneEntry(Context, Entry.Card.CardId, PlayerZones.PlayerId, EWBCardActivationSourceZone::Discard);
		}
	}

	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard())
		{
			AppendFixtureZoneEntry(Context, Unit.CardId, Unit.OwnerId, EWBCardActivationSourceZone::Board, Unit.UnitId);
		}
	}

	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		AppendFixtureZoneEntry(
			Context,
			Entry.Card.CardId,
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

FWBCardActivationSourceGateContext BuildActivationGateContext(
	const FWBGameStateData& State,
	const FWBCardActivationFixtureZoneContext& ZoneContext,
	const FWBCardDefinition& Definition,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const EWBCardActivationSourceZone SourceZone,
	const FWBCardEffectDefinition& Effect)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = PlayerId;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceCardId = Definition.CardId;
	Context.SourceZone = SourceZone;
	Context.FixtureZoneContext = ZoneContext;
	Context.bHasExplicitSourceGateContext = true;
	Context.bCostsSatisfiedExternally = true;
	Context.CostContext.bHasExternalAffordability = true;
	Context.CostContext.SuppliedRequiredRR = Effect.SourceGate.CostGate.RequiredRR;
	Context.CostContext.CostKind = Effect.SourceGate.CostGate.CostKind;

	const FWBUnitState* SourceUnit = State.GetUnitById(SourceUnitId);
	Context.CostContext.SuppliedAvailableRL = SourceUnit != nullptr
		? SourceUnit->GetAvailableRLForRules()
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
	const EWBCardActivationSourceZone SourceZone)
{
	if (Definition.ActivatedEffects.IsEmpty())
	{
		return;
	}

	FWBCardActivationCandidateSource Source;
	Source.PlayerId = PlayerId;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = BuildActivationTargets(State);
	for (const FWBCardEffectDefinition& Effect : Definition.ActivatedEffects)
	{
		Source.EffectIdToSourceGateContext.Add(
			Effect.EffectId,
			BuildActivationGateContext(
				State,
				ZoneContext,
				Definition,
				PlayerId,
				SourceUnitId,
				SourceZone,
				Effect));
	}
	Source.SourceGateContext = Source.EffectIdToSourceGateContext.FindRef(Definition.ActivatedEffects[0].EffectId);
	Sources.Add(MoveTemp(Source));
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
		MatchPhase == EWBMatchLoopPhase::TurnStart
		&& !TurnStartSequence.bCompleted;
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
		&TurnStartSequence);
}

FWBMatchLegalActionGenerationResult WBMatchCoordinator::EnumerateLegalActionsForState(
	const FWBGameStateData& InState,
	const EWBMatchLoopPhase InPhase,
	const FWBTurnStartSequenceState* InTurnStartSequence) const
{
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

			if (Lookup.Definition.Kind == EWBCardDefinitionKind::Character)
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

		const FWBCardActivationFixtureZoneContext ZoneContext = BuildActivationZoneContext(InState);
		TArray<FWBCardActivationCandidateSource> ActivationSources;
		TArray<const FWBUnitState*> BoardUnits = InState.GetUnitsForPlayer(PlayerId);
		BoardUnits.Sort(UnitIdPointerLess);
		for (const FWBUnitState* Unit : BoardUnits)
		{
			const FWBCardDefinitionRepositoryLookupResult Lookup =
				WBCardDefinitionRepository::FindCardById(Repository, Unit->CardId);
			if (Lookup.bFound)
			{
				AddActivationSource(
					ActivationSources,
					InState,
					ZoneContext,
					Lookup.Definition,
					PlayerId,
					Unit->UnitId,
					EWBCardActivationSourceZone::Board);
			}
		}

		TArray<FWBEquippedCardEntry> EquippedCards = InState.GetCardZoneState().EquippedCards;
		EquippedCards.Sort([](const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
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
					InState,
					ZoneContext,
					Lookup.Definition,
					PlayerId,
					Entry.EquippedToUnitId,
					EWBCardActivationSourceZone::Equipped);
			}
		}

		const FWBCardActivationCandidateGenerationResult CandidateResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(InState, ActivationSources);
		if (!CandidateResult.bOk)
		{
			return MakeGenerationFailure(CandidateResult.Reason);
		}
		const FWBCardActivationLegalActionGenerationResult ActivationResult =
			WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
		if (!ActivationResult.bOk)
		{
			return MakeGenerationFailure(ActivationResult.Reason);
		}

		TArray<FWBCardActivationLegalAction> ActivationActions = ActivationResult.ActionSet.Actions;
		ActivationActions.Sort([](const FWBCardActivationLegalAction& A, const FWBCardActivationLegalAction& B)
		{
			return A.ActivationActionId < B.ActivationActionId;
		});
		for (const FWBCardActivationLegalAction& ActivationAction : ActivationActions)
		{
			FWBMatchLegalAction Action;
			Action.Family = EWBMatchActionFamily::Activation;
			Action.ActionId = ActivationAction.ActivationActionId;
			Action.PlayerId = ActivationAction.PlayerId;
			Action.ActivationCommand = ActivationAction.Command;
			Result.Actions.Add(Action);
		}
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
				IsTurnTransitionInProgress();
			Result.PendingPlayerId =
				GetPendingTurnStartDecisionPlayerId();
			Result.ActivePlayerId = State.CurrentPlayer;
			Result.TurnNumber = State.TurnNumber;
			Result.TraceBeginIndex = TraceLog.Num();
			Result.TraceEndIndex = TraceLog.Num();
			Result.bGameOver = State.bGameOver;
			Result.WinnerPlayerId = State.WinnerPlayerId;
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
	if (SelectedAction->Family == EWBMatchActionFamily::CoreAction
		&& SelectedAction->CoreAction.Type == EWBActionType::EndTurn)
	{
		WorkingPhase = EWBMatchLoopPhase::TurnEnd;
		bActionApplied = ApplyTurnTransition(
			WorkingState,
			WorkingRandomState,
			WorkingPhase,
			WorkingTurnStartSequence,
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
			const FWBApplyActionResult ApplyResult =
				WBEffectRunner::ApplyAction(WorkingState, SelectedAction->CoreAction);
			bActionApplied = ApplyResult.bOk;
			FailureReason = ApplyResult.Reason;
			WorkingTraceEvents.Append(ApplyResult.TraceEvents);
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
			}
			if (bActionApplied
				&& SelectedAction->CoreAction.Type == EWBActionType::Attack
				&& WorkingState.HasPendingAttack())
			{
				const FWBApplyActionResult DamageResult =
					WBEffectRunner::ApplyPendingAttackDamage(WorkingState);
				bActionApplied = DamageResult.bOk;
				FailureReason = DamageResult.Reason;
				WorkingTraceEvents.Append(DamageResult.TraceEvents);
			}
			break;
		}
		case EWBMatchActionFamily::Summon:
		{
			const FWBSummonExecutionResult ApplyResult =
				WBSummonExecution::ExecuteCharacterSummonFromHand(
					WorkingState,
					Repository,
					SelectedAction->SummonRequest);
			bActionApplied = ApplyResult.bOk;
			FailureReason = ApplyResult.Reason;
			AppendSummonTraceEvents(ApplyResult, WorkingTraceEvents);
			if (bActionApplied)
			{
				const FWBMarkerResolutionResult MarkerResult =
					WBMarkerResolution::ResolveMarkerAtUnitTile(
						WorkingState,
						Repository,
						ApplyResult.CreatedUnitId);
				bActionApplied = MarkerResult.bOk;
				FailureReason = MarkerResult.Reason;
				WorkingTraceEvents.Append(MarkerResult.TraceEvents);
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
			const FWBCardActivationCommandResult ApplyResult =
				WBEffectRunner::ApplyCardActivationCommand(
					WorkingState,
					SelectedAction->ActivationCommand);
			bActionApplied = ApplyResult.bOk;
			FailureReason = ApplyResult.Reason;
			WorkingTraceEvents.Append(ApplyResult.TraceEvents);
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

		if (bActionApplied)
		{
			bActionApplied = ApplyAutomaticResolution(
				WorkingState,
				WorkingTraceEvents,
				FailureReason);
		}
	}

	if (!bActionApplied)
	{
		return MakeCurrentFailure(FailureReason.IsEmpty()
			? FString(TEXT("match_action_failed"))
			: FailureReason);
	}

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
			&WorkingTurnStartSequence);
	if (!NextLegalResult.bOk)
	{
		return MakeCurrentFailure(NextLegalResult.Reason);
	}

	State = WorkingState;
	RandomState = WorkingRandomState;
	MatchPhase = WorkingPhase;
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
		!(MatchPhase == EWBMatchLoopPhase::TurnStart
			&& !TurnStartSequence.bCompleted);
	CommittedRecord.bPendingDecision =
		!CommittedRecord.bCompleted;
	CommittedRecord.PendingPlayer =
		CommittedRecord.bPendingDecision
			? State.PriorityPlayer
			: -1;
	CommittedRecord.bTerminal = State.bGameOver;
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
		MatchPhase == EWBMatchLoopPhase::TurnStart
		&& !TurnStartSequence.bCompleted;
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

	if (WorkingState.bGameOver)
	{
		FWBTraceEvent GameOver = MakeMatchTrace(
			FName(TEXT("game_over")),
			WorkingState.CurrentPlayer,
			WorkingState.TurnNumber,
			PhaseToName(EWBMatchLoopPhase::GameOver));
		GameOver.WinningPlayerId = WorkingState.WinnerPlayerId;
		OutTraceEvents.Add(GameOver);
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

bool WBMatchCoordinator::ApplyTurnTransition(
	FWBGameStateData& WorkingState,
	uint32& WorkingRandomState,
	EWBMatchLoopPhase& WorkingPhase,
	FWBTurnStartSequenceState& WorkingTurnStartSequence,
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
		FWBTraceEvent GameOver = MakeMatchTrace(
			FName(TEXT("game_over")),
			EndingPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		GameOver.WinningPlayerId = WorkingState.WinnerPlayerId;
		OutTraceEvents.Add(GameOver);
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
		WBNPCPhaseResolution::ResolvePhase(
			WorkingState,
			Repository,
			WorkingRandomState,
			EndingPlayerId);
	if (!NPCPhaseResult.bOk)
	{
		OutReason = NPCPhaseResult.Reason;
		return false;
	}
	OutTraceEvents.Append(NPCPhaseResult.TraceEvents);
	if (WorkingState.bGameOver)
	{
		WorkingPhase = EWBMatchLoopPhase::GameOver;
		FWBTraceEvent GameOver = MakeMatchTrace(
			FName(TEXT("game_over")),
			EndingPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		GameOver.WinningPlayerId = WorkingState.WinnerPlayerId;
		OutTraceEvents.Add(GameOver);
		OutTraceEvents.Add(MakeMatchTrace(
			FName(TEXT("automatic_resolution")),
			EndingPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase)));
		OutReason.Reset();
		return true;
	}

	const int32 NextPlayerId = WorkingState.CurrentPlayer;
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
		TurnStartSequence);
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
