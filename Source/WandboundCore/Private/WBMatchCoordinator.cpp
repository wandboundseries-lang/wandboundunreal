#include "WBMatchCoordinator.h"

#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"
#include "WBResonanceOverflow.h"
#include "WBRules.h"

namespace
{
constexpr int32 OpeningHandSize = 6;
constexpr int32 BoardMidRow = 4;

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

FWBTile HeroSpawnForPlayer(const int32 PlayerId)
{
	return PlayerId == 0 ? FWBTile(4, 8) : FWBTile(4, 0);
}

bool IsFirstPlayerFirstTurn(const FWBGameStateData& State, const int32 FirstPlayerId)
{
	return State.TurnNumber == 1 && State.CurrentPlayer == FirstPlayerId;
}

bool EntersOpponentHalfOnFirstTurn(const FWBAction& Action, const int32 FirstPlayerId)
{
	if (Action.Type != EWBActionType::Move)
	{
		return false;
	}

	return (FirstPlayerId == 0 && Action.ToTile.Y < BoardMidRow)
		|| (FirstPlayerId == 1 && Action.ToTile.Y > BoardMidRow);
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
	const FWBCardDefinitionRepositoryValidationResult RepositoryValidation =
		WBCardDefinitionRepository::ValidateRepository(Request.Repository);
	if (!RepositoryValidation.bOk)
	{
		return MakeOperationFailure(RepositoryValidation.Reason);
	}

	if (!Request.bDeferMarkerSetup)
	{
		return MakeOperationFailure(TEXT("marker_setup_not_supported"));
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

	const int32 SelectedFirstPlayer = Request.FirstPlayerId == INDEX_NONE
		? static_cast<int32>((static_cast<uint32>(RollD6(WorkingRandomState)) - 1u) % 2u)
		: Request.FirstPlayerId;
	if (!FWBGameStateData::IsValidPlayerId(SelectedFirstPlayer))
	{
		return MakeOperationFailure(TEXT("invalid_first_player"));
	}

	FWBGameStateData WorkingState;
	WorkingState.CurrentPlayer = SelectedFirstPlayer;
	WorkingState.PriorityPlayer = SelectedFirstPlayer;
	WorkingState.TurnNumber = 1;
	WorkingState.Phase = EWBGamePhase::NormalTurn;
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
		Player.HeroUnitId = Setup.PlayerId;
		WorkingState.Players.Add(Player);

		FWBPlayerCardZoneState PlayerZones;
		PlayerZones.PlayerId = Setup.PlayerId;
		int32 DeckIndex = 0;
		for (int32 Index = 0; Index < Setup.OrderedDeck.Num(); ++Index)
		{
			if (Index == HeroDeckIndex)
			{
				continue;
			}

			FWBZoneCardEntry Entry;
			Entry.Card = Setup.OrderedDeck[Index];
			Entry.Card.OwnerPlayerId = Setup.PlayerId;
			Entry.Zone = EWBCardZone::Deck;
			Entry.ZoneIndex = DeckIndex++;
			PlayerZones.Deck.Add(Entry);
		}
		WorkingState.CardZoneState.PlayerZones.Add(PlayerZones);

		const FWBTile SpawnTile = HeroSpawnForPlayer(Setup.PlayerId);
		FWBUnitState Hero;
		Hero.UnitId = Setup.PlayerId;
		Hero.OwnerId = Setup.PlayerId;
		Hero.CardId = Setup.HeroCardId;
		Hero.X = SpawnTile.X;
		Hero.Y = SpawnTile.Y;
		Hero.HP = HeroLookup.Definition.CharacterStats.HP;
		Hero.MaxHP = Hero.HP;
		Hero.ATK = HeroLookup.Definition.CharacterStats.ATK;
		Hero.AR = HeroLookup.Definition.CharacterStats.AR;
		Hero.SetCanonicalRL(
			HeroLookup.Definition.CharacterStats.RL,
			HeroLookup.Definition.CharacterStats.RL,
			0);
		Hero.MaxAttacksPerTurn = 1;
		WorkingState.Units.Add(Hero);

		FWBTraceEvent HeroSpawned = MakeMatchTrace(
			FName(TEXT("hero_spawned")),
			Setup.PlayerId,
			1,
			PhaseToName(EWBMatchLoopPhase::Setup));
		HeroSpawned.SourceUnitId = Hero.UnitId;
		HeroSpawned.CardInstanceId = Setup.HeroInstanceId;
		HeroSpawned.CardId = Setup.HeroCardId;
		HeroSpawned.ToTile = SpawnTile;
		HeroSpawned.bHeroUnit = true;
		WorkingTraceEvents.Add(HeroSpawned);
	}

	WBCardZoneState::SortOrderedZonesDeterministically(WorkingState.CardZoneState);
	for (const FWBMatchPlayerSetup& Setup : PlayerSetups)
	{
		const FWBCardLifecycleResult DrawResult =
			WBCardLifecycle::ApplySetupDraw(WorkingState, Setup.PlayerId, OpeningHandSize);
		if (!DrawResult.bOk)
		{
			return MakeOperationFailure(DrawResult.Reason);
		}

		FWBTraceEvent OpeningDrawn = MakeMatchTrace(
			FName(TEXT("opening_cards_drawn")),
			Setup.PlayerId,
			1,
			PhaseToName(EWBMatchLoopPhase::Setup));
		OpeningDrawn.CardCount = OpeningHandSize;
		WorkingTraceEvents.Add(OpeningDrawn);
	}

	FWBTraceEvent MarkerBoundary = MakeMatchTrace(
		FName(TEXT("setup_markers_deferred")),
		SelectedFirstPlayer,
		1,
		PhaseToName(EWBMatchLoopPhase::Setup));
	MarkerBoundary.bDeferredBoundary = true;
	WorkingTraceEvents.Add(MarkerBoundary);

	const FWBApplyActionResult StartStatusResult =
		WBEffectRunner::ApplyStartOfTurnStatusTicks(WorkingState, SelectedFirstPlayer);
	if (!StartStatusResult.bOk)
	{
		return MakeOperationFailure(StartStatusResult.Reason);
	}
	WorkingTraceEvents.Append(StartStatusResult.TraceEvents);

	FWBTraceEvent DrawSkipped = MakeMatchTrace(
		FName(TEXT("turn_start_draw_skipped")),
		SelectedFirstPlayer,
		1,
		PhaseToName(EWBMatchLoopPhase::TurnStart));
	WorkingTraceEvents.Add(DrawSkipped);

	const int32 OpeningMPRoll = RollD6(WorkingRandomState);
	const FWBApplyActionResult ResourceResult =
		WBEffectRunner::ApplyTurnStartResourceSetup(WorkingState, SelectedFirstPlayer, OpeningMPRoll);
	if (!ResourceResult.bOk)
	{
		return MakeOperationFailure(ResourceResult.Reason);
	}
	WorkingTraceEvents.Append(ResourceResult.TraceEvents);
	WorkingTraceEvents.Add(MakeMatchTrace(
		FName(TEXT("turn_started")),
		SelectedFirstPlayer,
		1,
		PhaseToName(EWBMatchLoopPhase::Action)));

	WBMatchCoordinator Candidate;
	Candidate.bInitialized = true;
	Candidate.FirstPlayerId = SelectedFirstPlayer;
	Candidate.RandomState = WorkingRandomState;
	Candidate.MatchPhase = EWBMatchLoopPhase::Action;
	Candidate.State = WorkingState;
	Candidate.Repository = Request.Repository;
	Candidate.TraceLog = WorkingTraceEvents;

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
	return Result;
}

FWBMatchLegalActionGenerationResult WBMatchCoordinator::EnumerateLegalActions() const
{
	if (!bInitialized)
	{
		return MakeGenerationFailure(TEXT("match_not_initialized"));
	}
	return EnumerateLegalActionsForState(State, MatchPhase);
}

FWBMatchLegalActionGenerationResult WBMatchCoordinator::EnumerateLegalActionsForState(
	const FWBGameStateData& InState,
	const EWBMatchLoopPhase InPhase) const
{
	FWBMatchLegalActionGenerationResult Result;
	if (InState.bGameOver || InPhase == EWBMatchLoopPhase::GameOver)
	{
		Result.bOk = true;
		return Result;
	}

	if (InPhase != EWBMatchLoopPhase::Action && InPhase != EWBMatchLoopPhase::Response)
	{
		return MakeGenerationFailure(TEXT("match_not_accepting_actions"));
	}

	const int32 PlayerId = InState.PriorityPlayer;
	TArray<FWBAction> EndTurnActions;
	for (const FWBAction& CoreAction : WBRules::GenerateLegalActionsForPlayer(InState, PlayerId))
	{
		if (IsFirstPlayerFirstTurn(InState, FirstPlayerId)
			&& (CoreAction.Type == EWBActionType::Attack
				|| EntersOpponentHalfOnFirstTurn(CoreAction, FirstPlayerId)))
		{
			continue;
		}

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
	if (State.bGameOver || MatchPhase == EWBMatchLoopPhase::GameOver)
	{
		return MakeOperationFailure(TEXT("game_over"));
	}
	if (PlayerId != State.PriorityPlayer)
	{
		return MakeOperationFailure(TEXT("wrong_player"));
	}
	if (ActionId.IsEmpty())
	{
		return MakeOperationFailure(TEXT("action_id_missing"));
	}

	const FWBMatchLegalActionGenerationResult LegalResult = EnumerateLegalActions();
	if (!LegalResult.bOk)
	{
		return MakeOperationFailure(LegalResult.Reason);
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
		return MakeOperationFailure(TEXT("stale_or_illegal_action"));
	}

	FWBGameStateData WorkingState = State;
	uint32 WorkingRandomState = RandomState;
	EWBMatchLoopPhase WorkingPhase = MatchPhase;
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
			WorkingTraceEvents,
			FailureReason);
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
		return MakeOperationFailure(FailureReason.IsEmpty()
			? FString(TEXT("match_action_failed"))
			: FailureReason);
	}

	WorkingPhase = WorkingState.bGameOver
		? EWBMatchLoopPhase::GameOver
		: (WorkingState.IsResponsePhase() ? EWBMatchLoopPhase::Response : EWBMatchLoopPhase::Action);

	FWBTraceEvent Resolved = MakeMatchTrace(
		FName(TEXT("action_resolved")),
		PlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	Resolved.ActionId = ActionId;
	WorkingTraceEvents.Add(Resolved);

	const FWBMatchLegalActionGenerationResult NextLegalResult =
		EnumerateLegalActionsForState(WorkingState, WorkingPhase);
	if (!NextLegalResult.bOk)
	{
		return MakeOperationFailure(NextLegalResult.Reason);
	}

	State = WorkingState;
	RandomState = WorkingRandomState;
	MatchPhase = WorkingPhase;
	TraceLog.Append(WorkingTraceEvents);

	FWBMatchOperationResult Result;
	Result.bOk = true;
	Result.SubmittedActionId = ActionId;
	Result.TraceEvents = MoveTemp(WorkingTraceEvents);
	Result.NextLegalActions = NextLegalResult.Actions;
	Result.bGameOver = State.bGameOver;
	Result.WinnerPlayerId = State.WinnerPlayerId;
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
			if (Unit.IsUnitOnBoard())
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
	FWBTraceEvent NPCBoundary = MakeMatchTrace(
		FName(TEXT("npc_phase_deferred")),
		EndingPlayerId,
		EndingTurnNumber,
		PhaseToName(WorkingPhase));
	NPCBoundary.bDeferredBoundary = true;
	OutTraceEvents.Add(NPCBoundary);

	const int32 NextPlayerId = WorkingState.CurrentPlayer;
	WorkingPhase = EWBMatchLoopPhase::TurnStart;
	const FWBApplyActionResult StartStatusResult =
		WBEffectRunner::ApplyStartOfTurnStatusTicks(WorkingState, NextPlayerId);
	if (!StartStatusResult.bOk)
	{
		OutReason = StartStatusResult.Reason;
		return false;
	}
	OutTraceEvents.Append(StartStatusResult.TraceEvents);
	if (WorkingState.bGameOver)
	{
		WorkingPhase = EWBMatchLoopPhase::GameOver;
		FWBTraceEvent GameOver = MakeMatchTrace(
			FName(TEXT("game_over")),
			NextPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase));
		GameOver.WinningPlayerId = WorkingState.WinnerPlayerId;
		OutTraceEvents.Add(GameOver);
		OutTraceEvents.Add(MakeMatchTrace(
			FName(TEXT("automatic_resolution")),
			NextPlayerId,
			WorkingState.TurnNumber,
			PhaseToName(WorkingPhase)));
		OutReason.Reset();
		return true;
	}

	const FWBCardLifecycleResult DrawResult = WBCardLifecycle::ApplyTurnStartDraw(
		WorkingState,
		NextPlayerId,
		WorkingState.TurnNumber,
		FirstPlayerId);
	if (!DrawResult.bOk)
	{
		OutReason = DrawResult.Reason;
		return false;
	}
	FWBTraceEvent DrawTrace = MakeMatchTrace(
		DrawResult.Code == EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped
			? FName(TEXT("turn_start_draw_skipped"))
			: FName(TEXT("turn_start_card_drawn")),
		NextPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(WorkingPhase));
	DrawTrace.CardInstanceId = DrawResult.CardInstanceId;
	DrawTrace.CardId = DrawResult.CardId;
	DrawTrace.CardCount = DrawResult.Code == EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped ? 0 : 1;
	OutTraceEvents.Add(DrawTrace);

	const int32 MPRoll = RollD6(WorkingRandomState);
	const FWBApplyActionResult ResourceResult =
		WBEffectRunner::ApplyTurnStartResourceSetup(WorkingState, NextPlayerId, MPRoll);
	if (!ResourceResult.bOk)
	{
		OutReason = ResourceResult.Reason;
		return false;
	}
	OutTraceEvents.Append(ResourceResult.TraceEvents);

	FWBTraceEvent Advanced = MakeMatchTrace(
		FName(TEXT("player_advanced")),
		NextPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(EWBMatchLoopPhase::Action));
	Advanced.FromPlayer = EndingPlayerId;
	Advanced.ToPlayer = NextPlayerId;
	OutTraceEvents.Add(Advanced);
	OutTraceEvents.Add(MakeMatchTrace(
		FName(TEXT("turn_started")),
		NextPlayerId,
		WorkingState.TurnNumber,
		PhaseToName(EWBMatchLoopPhase::Action)));

	if (!ApplyAutomaticResolution(WorkingState, OutTraceEvents, OutReason))
	{
		return false;
	}

	WorkingPhase = WorkingState.bGameOver
		? EWBMatchLoopPhase::GameOver
		: EWBMatchLoopPhase::Action;
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

FWBGameStateData& WBMatchCoordinator::GetMutableStateForTest()
{
	return State;
}

int32 WBMatchCoordinator::RollD6(uint32& InOutRandomState)
{
	InOutRandomState = InOutRandomState * 1664525u + 1013904223u;
	return static_cast<int32>((InOutRandomState % 6u) + 1u);
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
