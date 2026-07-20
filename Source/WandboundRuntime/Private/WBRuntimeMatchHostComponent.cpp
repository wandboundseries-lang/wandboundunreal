#include "WBRuntimeMatchHostComponent.h"

#include "WBBoardViewActor.h"

namespace
{
FWBCardDefinition MakeDevelopmentCharacter(
	const FString& CardId,
	const EWBCardDefinitionKind Kind = EWBCardDefinitionKind::Character)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	Definition.CharacterStats.HP = Kind == EWBCardDefinitionKind::NPC ? 5 : 8;
	Definition.CharacterStats.ATK = Kind == EWBCardDefinitionKind::NPC ? 2 : 3;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	return Definition;
}

FWBCardInstanceRef MakeDevelopmentCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerId;
	return Card;
}

FWBMatchPlayerSetup MakeDevelopmentPlayer(const int32 PlayerId)
{
	const FString Prefix = FString::Printf(TEXT("p%d"), PlayerId);
	FWBMatchPlayerSetup Player;
	Player.PlayerId = PlayerId;
	Player.HeroInstanceId = Prefix + TEXT("_hero_instance");
	Player.HeroCardId = PlayerId == 0 ? TEXT("hero_alpha") : TEXT("hero_beta");
	Player.OrderedDeck.Add(MakeDevelopmentCard(Player.HeroInstanceId, Player.HeroCardId, PlayerId));
	Player.OrderedDeck.Add(MakeDevelopmentCard(Prefix + TEXT("_student"), TEXT("student"), PlayerId));
	Player.OrderedDeck.Add(MakeDevelopmentCard(Prefix + TEXT("_wand"), TEXT("test_wand"), PlayerId));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Player.OrderedDeck.Add(MakeDevelopmentCard(
			FString::Printf(TEXT("%s_private_%d"), *Prefix, Index),
			TEXT("filler"),
			PlayerId));
	}
	return Player;
}

FWBSetupMarkerPlacement MakeDevelopmentMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile& Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = PlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap ? TEXT("basic_trap") : TEXT("basic_npc");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBMatchInitializationRequest BuildDeterministicDevelopmentSetup(const bool bFragileFirstHero)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 91234;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("runtime_development_match");
	Request.Repository.SourceVersion = TEXT("1");

	FWBCardDefinition Wand;
	Wand.CardId = TEXT("test_wand");
	Wand.PublicName = TEXT("Test Wand");
	Wand.Kind = EWBCardDefinitionKind::Wand;
	Wand.WandStats.RR = 1;
	FWBCardDefinition Filler;
	Filler.CardId = TEXT("filler");
	Filler.PublicName = TEXT("Filler");
	Filler.Kind = EWBCardDefinitionKind::Action;
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("basic_trap");
	Trap.PublicName = TEXT("Basic Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	FWBCardDefinition HeroAlpha = MakeDevelopmentCharacter(TEXT("hero_alpha"));
	for (const TPair<FString, FString>& EffectIdentity : {
		TPair<FString, FString>(TEXT("arc_bolt"), TEXT("Arc Bolt")),
		TPair<FString, FString>(TEXT("ember_bolt"), TEXT("Ember Bolt")) })
	{
		FWBCardEffectDefinition Effect;
		Effect.EffectId = EffectIdentity.Key;
		Effect.PublicLabel = EffectIdentity.Value;
		Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
		FWBGenericEffectPayload Payload;
		Payload.Operation = EWBGenericEffectOp::DamageEffect;
		Payload.DamageEffect.Amount = 1;
		Payload.DamageEffect.bBypassArmor = true;
		Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
		Payload.DamageEffect.SourceReason = FName(TEXT("development_match"));
		Effect.Payloads.Add(Payload);
		Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
		Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
		Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
		Effect.SourceGate.bRequiresSourceUnit = true;
		Effect.SourceGate.bRequiresSourceUnitOwnership = true;
		Effect.SourceGate.bHasExplicitSourceGate = true;
		HeroAlpha.ActivatedEffects.Add(Effect);
	}

	Request.Repository.Definitions = {
		HeroAlpha,
		MakeDevelopmentCharacter(TEXT("hero_beta")),
		MakeDevelopmentCharacter(TEXT("student")),
		Wand,
		Filler,
		Trap,
		MakeDevelopmentCharacter(TEXT("basic_npc"), EWBCardDefinitionKind::NPC)
	};
	if (bFragileFirstHero) Request.Repository.Definitions[0].CharacterStats.HP = 1;
	Request.Players = { MakeDevelopmentPlayer(0), MakeDevelopmentPlayer(1) };
	Request.MarkerPlacements = {
		MakeDevelopmentMarker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeDevelopmentMarker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeDevelopmentMarker(0, EWBMarkerType::NPC, FWBTile(2, 8), 2),
		MakeDevelopmentMarker(0, EWBMarkerType::NPC, FWBTile(3, 7), 3),
		MakeDevelopmentMarker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeDevelopmentMarker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeDevelopmentMarker(1, EWBMarkerType::NPC, FWBTile(2, 0), 6),
		MakeDevelopmentMarker(1, EWBMarkerType::NPC, FWBTile(3, 1), 7)
	};
	return Request;
}

FName CoreActionName(const EWBActionType Type)
{
	switch (Type)
	{
	case EWBActionType::Move: return FName(TEXT("move"));
	case EWBActionType::Attack: return FName(TEXT("attack"));
	case EWBActionType::EndTurn: return FName(TEXT("end_turn"));
	case EWBActionType::Pass: return FName(TEXT("pass"));
	case EWBActionType::PassResponse: return FName(TEXT("pass_response"));
	default: return FName(TEXT("unknown"));
	}
}

EWBRuntimeMatchActionFamily RuntimeFamily(const FWBMatchLegalAction& Action)
{
	if (Action.Family == EWBMatchActionFamily::CoreAction)
	{
		switch (Action.CoreAction.Type)
		{
		case EWBActionType::Move: return EWBRuntimeMatchActionFamily::Move;
		case EWBActionType::Attack: return EWBRuntimeMatchActionFamily::Attack;
		case EWBActionType::Pass: return EWBRuntimeMatchActionFamily::Pass;
		case EWBActionType::PassResponse: return EWBRuntimeMatchActionFamily::PassResponse;
		case EWBActionType::EndTurn: return EWBRuntimeMatchActionFamily::EndTurn;
		default: return EWBRuntimeMatchActionFamily::CoreAction;
		}
	}
	switch (Action.Family)
	{
	case EWBMatchActionFamily::Summon: return EWBRuntimeMatchActionFamily::Summon;
	case EWBMatchActionFamily::Equip: return EWBRuntimeMatchActionFamily::Equip;
	case EWBMatchActionFamily::Activation: return EWBRuntimeMatchActionFamily::Activation;
	case EWBMatchActionFamily::Discard: return EWBRuntimeMatchActionFamily::Discard;
	default: return EWBRuntimeMatchActionFamily::CoreAction;
	}
}

FString PublicActionLabel(const FWBMatchLegalAction& Action)
{
	switch (RuntimeFamily(Action))
	{
	case EWBRuntimeMatchActionFamily::Move: return TEXT("Move");
	case EWBRuntimeMatchActionFamily::Attack: return TEXT("Attack");
	case EWBRuntimeMatchActionFamily::Pass:
	case EWBRuntimeMatchActionFamily::PassResponse: return TEXT("Pass");
	case EWBRuntimeMatchActionFamily::EndTurn: return TEXT("End Turn");
	case EWBRuntimeMatchActionFamily::Summon: return TEXT("Summon");
	case EWBRuntimeMatchActionFamily::Equip: return TEXT("Equip");
	case EWBRuntimeMatchActionFamily::Activation: return TEXT("Activate");
	case EWBRuntimeMatchActionFamily::Discard: return TEXT("Discard");
	default: return TEXT("Action");
	}
}

FName ActionTypeName(const FWBMatchLegalAction& Action)
{
	switch (Action.Family)
	{
	case EWBMatchActionFamily::Summon: return FName(TEXT("summon"));
	case EWBMatchActionFamily::Equip: return FName(TEXT("equip"));
	case EWBMatchActionFamily::Activation: return FName(TEXT("activation"));
	case EWBMatchActionFamily::Discard: return FName(TEXT("discard"));
	default: return CoreActionName(Action.CoreAction.Type);
	}
}

bool IsNPCTrace(const FWBTraceEvent& Event)
{
	const FString Kind = Event.Kind.GetPlainNameString().ToLower();
	return Kind.Contains(TEXT("npc"));
}
}

UWBRuntimeMatchHostComponent::UWBRuntimeMatchHostComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWBRuntimeMatchHostComponent::~UWBRuntimeMatchHostComponent() = default;

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::InitializeDevelopmentMatch(
	const int32 InitialViewerPlayerId,
	const bool bFragileFirstHero)
{
#if UE_BUILD_SHIPPING
	return MakeResult(false, TEXT("development_match_unavailable"));
#else
	return InitializeMatch(BuildDeterministicDevelopmentSetup(bFragileFirstHero), InitialViewerPlayerId);
#endif
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::InitializeMatch(
	const FWBMatchInitializationRequest& Request,
	const int32 InitialViewerPlayerId)
{
	TUniquePtr<WBMatchCoordinator> Candidate = MakeUnique<WBMatchCoordinator>();
	const FWBMatchOperationResult Result = Candidate->InitializeMatch(Request);
	if (!Result.bOk)
	{
		FWBRuntimeMatchCommandResult Failure = MakeResult(false, Result.Reason);
		OnMatchInitializationFailed.Broadcast();
		return Failure;
	}

	Coordinator = MoveTemp(Candidate);
	InitializationRequest = Request;
	LatestOperationResult = Result;
	CurrentViewerPlayerId = InitialViewerPlayerId;
	++MatchGeneration;
	DecisionRevision = 0;
	PresentationRevision = 0;
	TerminalBroadcastGeneration = INDEX_NONE;
	HeroDefinitionIds.Reset();
	for (const FWBMatchPlayerSetup& Player : Request.Players)
	{
		HeroDefinitionIds.Add(Player.HeroCardId);
	}
	ClearSelectionInternal(false);

	FWBRuntimeMatchCommandResult RefreshResult = RefreshFromCoordinator(TEXT("match_initialized"));
	if (!RefreshResult.bOk)
	{
		Coordinator.Reset();
		CurrentObservation = FWBMatchObservation();
		CurrentLegalDecisions.Reset();
		OnMatchInitializationFailed.Broadcast();
		return RefreshResult;
	}

	OnMatchInitialized.Broadcast();
	return RefreshResult;
}

bool UWBRuntimeMatchHostComponent::IsMatchInitialized() const
{
	return Coordinator.IsValid() && Coordinator->IsInitialized();
}

int32 UWBRuntimeMatchHostComponent::GetCurrentViewerPlayerId() const
{
	return CurrentViewerPlayerId;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SetCurrentViewerPlayerId(const int32 ViewerPlayerId)
{
	if (!IsMatchInitialized())
	{
		return MakeResult(false, TEXT("match_not_initialized"));
	}
	if (!CurrentObservation.PublicTurn.Players.ContainsByPredicate([ViewerPlayerId](const FWBPublicPlayerTurnSummary& Player)
	{
		return Player.PlayerId == ViewerPlayerId;
	}))
	{
		return MakeResult(false, TEXT("invalid_viewer_player_id"));
	}
	CurrentViewerPlayerId = ViewerPlayerId;
	ClearSelectionInternal(false);
	return RefreshFromCoordinator(TEXT("viewer_changed"));
}

FWBRuntimeMatchPresentation UWBRuntimeMatchHostComponent::GetCurrentPresentation() const { return CurrentPresentation; }
TArray<FWBRuntimeLegalActionPresentation> UWBRuntimeMatchHostComponent::GetCurrentLegalActions() const { return LegalActionPresentations; }
TArray<FWBRuntimeHandCardPresentation> UWBRuntimeMatchHostComponent::GetCurrentHandCards() const { return HandCardPresentations; }

FWBRuntimeSelectionPresentation UWBRuntimeMatchHostComponent::GetCurrentSelection() const
{
	FWBRuntimeSelectionPresentation Selection;
	Selection.SelectedUnitId = SelectedUnitId;
	Selection.SelectedCardInstanceId = SelectedCardInstanceId;
	Selection.bHasActionFamilyFilter = bHasSelectedActionFamily;
	Selection.ActionFamilyFilter = SelectedActionFamily;
	Selection.ResolvedActionId = PendingSelectedActionId;
	Selection.AmbiguousActionIds = AmbiguousActionIds;
	Selection.StatusReason = SelectionStatusReason;
	return Selection;
}

TArray<FWBRuntimeLegalActionPresentation> UWBRuntimeMatchHostComponent::GetActionsForCurrentSelection() const
{
	TArray<FWBRuntimeLegalActionPresentation> Result;
	for (int32 Index = 0; Index < CurrentLegalDecisions.Num() && Index < LegalActionPresentations.Num(); ++Index)
	{
		if (ActionMatchesCurrentSelection(CurrentLegalDecisions[Index]))
		{
			Result.Add(LegalActionPresentations[Index]);
		}
	}
	return Result;
}
TArray<FWBRuntimeUnitPresentation> UWBRuntimeMatchHostComponent::GetCurrentUnits() const { return UnitPresentations; }
TArray<FWBRuntimeBoardTilePresentation> UWBRuntimeMatchHostComponent::GetCurrentTiles() const { return TilePresentations; }

TArray<FString> UWBRuntimeMatchHostComponent::GetSelectableCardInstanceIds() const
{
	TSet<FString> Unique;
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		FString InstanceId;
		switch (Action.Family)
		{
		case EWBMatchActionFamily::Summon: InstanceId = Action.SummonRequest.SourceInstanceId; break;
		case EWBMatchActionFamily::Equip: InstanceId = Action.EquipRequest.SourceInstanceId; break;
		case EWBMatchActionFamily::Discard: InstanceId = Action.DiscardCardInstanceId; break;
		default: break;
		}
		if (!InstanceId.IsEmpty()) Unique.Add(InstanceId);
	}
	TArray<FString> Result = Unique.Array();
	Result.Sort();
	return Result;
}

TArray<int32> UWBRuntimeMatchHostComponent::GetSelectableUnitIds() const
{
	TSet<int32> Unique;
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		int32 SourceUnitId = -1;
		if (Action.Family == EWBMatchActionFamily::CoreAction) SourceUnitId = Action.CoreAction.SourceUnitId;
		else if (Action.Family == EWBMatchActionFamily::Activation) SourceUnitId = Action.ActivationCommand.Source.SourceUnitId;
		if (SourceUnitId >= 0) Unique.Add(SourceUnitId);
	}
	TArray<int32> Result = Unique.Array();
	Result.Sort();
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SelectUnit(const int32 UnitId)
{
	if (!GetSelectableUnitIds().Contains(UnitId)) return MakeResult(false, TEXT("unit_not_selectable"));
	SelectedUnitId = UnitId;
	SelectedCardInstanceId.Reset();
	PendingSelectedActionId.Reset();
	AmbiguousActionIds.Reset();
	bHasSelectedActionFamily = false;
	SelectionStatusReason = TEXT("unit_selected");
	RebuildHighlights();
	RebuildHandPresentations();
	OnSelectionChanged.Broadcast();
	return MakeResult(true, TEXT("unit_selected"));
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SelectCardInstance(const FString& CardInstanceId)
{
	if (!GetSelectableCardInstanceIds().Contains(CardInstanceId)) return MakeResult(false, TEXT("card_not_selectable"));
	SelectedUnitId = -1;
	SelectedCardInstanceId = CardInstanceId;
	PendingSelectedActionId.Reset();
	AmbiguousActionIds.Reset();
	bHasSelectedActionFamily = false;
	SelectionStatusReason = TEXT("card_selected");
	RebuildHighlights();
	RebuildHandPresentations();
	OnSelectionChanged.Broadcast();
	return MakeResult(true, TEXT("card_selected"));
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SelectTile(const FIntPoint Tile)
{
	FWBRuntimeMatchCommandResult Result = MakeResult(false, TEXT("selected_tile_has_no_legal_action"));
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		if (ActionMatchesCurrentSelection(Action) && TargetTileForAction(Action) == Tile)
		{
			Result.CandidateActionIds.Add(Action.ActionId);
		}
	}
	Result.CandidateActionIds.Sort();
	if (Result.CandidateActionIds.Num() > 1)
	{
		Result.Reason = TEXT("selected_tile_action_ambiguous");
		AmbiguousActionIds = Result.CandidateActionIds;
		PendingSelectedActionId.Reset();
		SelectionStatusReason = Result.Reason;
		OnSelectionChanged.Broadcast();
		return Result;
	}
	if (Result.CandidateActionIds.Num() == 0)
	{
		SelectionStatusReason = Result.Reason;
		return Result;
	}
	PendingSelectedActionId = Result.CandidateActionIds[0];
	AmbiguousActionIds.Reset();
	Result.bOk = true;
	Result.Reason = TEXT("action_selected");
	SelectionStatusReason = Result.Reason;
	Result.ActionId = PendingSelectedActionId;
	for (FWBRuntimeBoardTilePresentation& Entry : TilePresentations) Entry.bSelected = Entry.Tile == Tile;
	SynchronizeBoardActor();
	OnSelectionChanged.Broadcast();
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SelectUnitTarget(const int32 UnitId)
{
	if (SelectedUnitId < 0 && SelectedCardInstanceId.IsEmpty())
	{
		return SelectUnit(UnitId);
	}

	FWBRuntimeMatchCommandResult Result = MakeResult(false, TEXT("selected_unit_has_no_legal_action"));
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		int32 TargetUnitId = -1;
		if (Action.Family == EWBMatchActionFamily::CoreAction) TargetUnitId = Action.CoreAction.TargetUnitId;
		else if (Action.Family == EWBMatchActionFamily::Equip) TargetUnitId = Action.EquipRequest.TargetUnitId;
		else if (Action.Family == EWBMatchActionFamily::Activation) TargetUnitId = Action.ActivationCommand.EffectRequest.Target.TargetUnitId;
		if (ActionMatchesCurrentSelection(Action) && TargetUnitId == UnitId)
		{
			Result.CandidateActionIds.Add(Action.ActionId);
		}
	}
	Result.CandidateActionIds.Sort();
	if (Result.CandidateActionIds.Num() > 1)
	{
		Result.Reason = TEXT("selected_unit_action_ambiguous");
		AmbiguousActionIds = Result.CandidateActionIds;
		PendingSelectedActionId.Reset();
		SelectionStatusReason = Result.Reason;
		OnSelectionChanged.Broadcast();
		return Result;
	}
	if (Result.CandidateActionIds.Num() == 0)
	{
		SelectionStatusReason = Result.Reason;
		return Result;
	}
	PendingSelectedActionId = Result.CandidateActionIds[0];
	AmbiguousActionIds.Reset();
	Result.bOk = true;
	Result.Reason = TEXT("action_selected");
	Result.ActionId = PendingSelectedActionId;
	SelectionStatusReason = Result.Reason;
	OnSelectionChanged.Broadcast();
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SelectActionFamily(const EWBRuntimeMatchActionFamily Family)
{
	const bool bAvailable = GetActionsForCurrentSelection().ContainsByPredicate([Family](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.Family == Family;
	});
	if (!bAvailable)
	{
		return MakeResult(false, TEXT("action_family_not_available"));
	}
	bHasSelectedActionFamily = true;
	SelectedActionFamily = Family;
	PendingSelectedActionId.Reset();
	AmbiguousActionIds.Reset();
	SelectionStatusReason = TEXT("action_family_selected");
	const TArray<FWBRuntimeLegalActionPresentation> FamilyActions = GetActionsForCurrentSelection();
	const bool bAllNoTarget = !FamilyActions.IsEmpty() && !FamilyActions.ContainsByPredicate([](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.bRequiresTargetSelection;
	});
	if (FamilyActions.Num() == 1 && bAllNoTarget)
	{
		PendingSelectedActionId = FamilyActions[0].ActionId;
		SelectionStatusReason = TEXT("action_selected");
	}
	else if (FamilyActions.Num() > 1 && bAllNoTarget)
	{
		for (const FWBRuntimeLegalActionPresentation& Action : FamilyActions)
		{
			AmbiguousActionIds.Add(Action.ActionId);
		}
		AmbiguousActionIds.Sort();
		SelectionStatusReason = TEXT("action_family_ambiguous");
	}
	RebuildHighlights();
	OnSelectionChanged.Broadcast();
	FWBRuntimeMatchCommandResult Result = MakeResult(true, SelectionStatusReason, PendingSelectedActionId);
	Result.CandidateActionIds = AmbiguousActionIds;
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::ChooseActionCandidate(const FString& ActionId)
{
	if (!AmbiguousActionIds.Contains(ActionId))
	{
		return MakeResult(false, TEXT("ambiguous_action_candidate_not_found"), ActionId);
	}
	int32 MatchCount = 0;
	FindCurrentAction(ActionId, MatchCount);
	if (MatchCount != 1)
	{
		return MakeResult(false, TEXT("stale_or_illegal_action"), ActionId);
	}
	PendingSelectedActionId = ActionId;
	AmbiguousActionIds.Reset();
	SelectionStatusReason = TEXT("ambiguity_resolved");
	OnSelectionChanged.Broadcast();
	return MakeResult(true, TEXT("ambiguity_resolved"), ActionId);
}

void UWBRuntimeMatchHostComponent::ClearSelection() { ClearSelectionInternal(true); }

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SubmitSelectedAction()
{
	if (PendingSelectedActionId.IsEmpty()) return MakeResult(false, TEXT("selected_action_missing"));
	return SubmitLegalActionById(PendingSelectedActionId);
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SubmitLegalActionById(const FString& ActionId)
{
	return SubmitLegalActionAtRevision(ActionId, MatchGeneration, DecisionRevision);
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::SubmitLegalActionAtRevision(
	const FString& ActionId,
	const int32 ExpectedMatchGeneration,
	const int32 ExpectedDecisionRevision)
{
	const auto Reject = [this, &ActionId](const FString& Reason)
	{
		FWBRuntimeMatchCommandResult Result = MakeResult(false, Reason, ActionId);
		OnActionRejected.Broadcast();
		return Result;
	};
	if (!IsMatchInitialized()) return Reject(TEXT("match_not_initialized"));
	if (IsGameOver()) return Reject(TEXT("game_over"));
	if (ExpectedMatchGeneration != MatchGeneration) return Reject(TEXT("stale_match_generation"));
	if (ExpectedDecisionRevision != DecisionRevision) return Reject(TEXT("stale_decision_revision"));

	int32 MatchCount = 0;
	FindCurrentAction(ActionId, MatchCount);
	if (MatchCount == 0) return Reject(TEXT("stale_or_illegal_action"));
	if (MatchCount > 1) return Reject(TEXT("legal_action_id_ambiguous"));

	OnActionSubmitted.Broadcast();
	LatestOperationResult = Coordinator->SubmitActionId(CurrentObservation.PublicTurn.PriorityPlayerId, ActionId);
	if (!LatestOperationResult.bOk)
	{
		return Reject(LatestOperationResult.Reason);
	}

	const bool bHadNPCEvents = LatestOperationResult.TraceEvents.ContainsByPredicate(IsNPCTrace);
	ClearSelectionInternal(false);
	FWBRuntimeMatchCommandResult Result = RefreshFromCoordinator(TEXT("action_resolved"));
	Result.ActionId = ActionId;
	if (bHadNPCEvents) OnNPCPhaseResolved.Broadcast();
	OnActionResolved.Broadcast();
	return Result;
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::EndTurn()
{
	TArray<FString> Candidates;
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		if (Action.Family == EWBMatchActionFamily::CoreAction && Action.CoreAction.Type == EWBActionType::EndTurn)
		{
			Candidates.Add(Action.ActionId);
		}
	}
	if (Candidates.Num() != 1)
	{
		FWBRuntimeMatchCommandResult Result = MakeResult(false, Candidates.IsEmpty() ? TEXT("end_turn_not_legal") : TEXT("end_turn_ambiguous"));
		Result.CandidateActionIds = Candidates;
		return Result;
	}
	return SubmitLegalActionById(Candidates[0]);
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::RefreshPresentation()
{
	if (!IsMatchInitialized()) return MakeResult(false, TEXT("match_not_initialized"));
	ClearSelectionInternal(false);
	return RefreshFromCoordinator(TEXT("presentation_refreshed"));
}

bool UWBRuntimeMatchHostComponent::IsGameOver() const { return CurrentPresentation.bGameOver; }
int32 UWBRuntimeMatchHostComponent::GetWinnerPlayerId() const { return CurrentPresentation.WinnerPlayerId; }
const FWBMatchObservation& UWBRuntimeMatchHostComponent::GetCurrentObservation() const { return CurrentObservation; }
const FWBMatchOperationResult& UWBRuntimeMatchHostComponent::GetLatestOperationResult() const { return LatestOperationResult; }
const WBMatchCoordinator* UWBRuntimeMatchHostComponent::GetCoordinatorForInspection() const { return Coordinator.Get(); }

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::RefreshFromCoordinator(const FString& StatusMessage)
{
	if (!IsMatchInitialized()) return MakeResult(false, TEXT("match_not_initialized"));
	CurrentObservation = Coordinator->BuildObservation(CurrentViewerPlayerId);
	CurrentLegalDecisions = CurrentObservation.LegalActions;
	++DecisionRevision;
	++PresentationRevision;
	RebuildPresentationModels(StatusMessage);
	SynchronizeBoardActor();
	OnObservationRefreshed.Broadcast();
	OnDecisionChanged.Broadcast();
	OnBoardPresentationRefreshed.Broadcast();
	if (CurrentPresentation.bGameOver && TerminalBroadcastGeneration != MatchGeneration)
	{
		TerminalBroadcastGeneration = MatchGeneration;
		OnGameOver.Broadcast();
	}
	return MakeResult(true, StatusMessage);
}

FWBRuntimeMatchCommandResult UWBRuntimeMatchHostComponent::MakeResult(
	const bool bOk,
	const FString& Reason,
	const FString& ActionId) const
{
	FWBRuntimeMatchCommandResult Result;
	Result.bOk = bOk;
	Result.Reason = Reason;
	Result.ActionId = ActionId;
	Result.MatchGeneration = MatchGeneration;
	Result.DecisionRevision = DecisionRevision;
	return Result;
}

void UWBRuntimeMatchHostComponent::RebuildPresentationModels(const FString& StatusMessage)
{
	TSet<int32> PreviousUnitIds;
	for (const FWBRuntimeUnitPresentation& Unit : UnitPresentations)
	{
		PreviousUnitIds.Add(Unit.UnitId);
	}
	CurrentPresentation = FWBRuntimeMatchPresentation();
	CurrentPresentation.CurrentPlayerId = CurrentObservation.PublicTurn.CurrentPlayerId;
	CurrentPresentation.ViewerPlayerId = CurrentViewerPlayerId;
	CurrentPresentation.Phase = CurrentObservation.PublicTurn.Phase;
	CurrentPresentation.TurnNumber = CurrentObservation.PublicTurn.TurnNumber;
	CurrentPresentation.bGameOver = CurrentObservation.PublicTurn.bGameOver;
	CurrentPresentation.WinnerPlayerId = CurrentObservation.PublicTurn.WinnerPlayerId;
	if (CurrentPresentation.bGameOver)
	{
		CurrentPresentation.DefeatedPlayerId = CurrentPresentation.WinnerPlayerId == 0 ? 1 : 0;
		CurrentPresentation.TerminalReason = TEXT("hero_defeated");
		CurrentPresentation.FinalTurnNumber = CurrentObservation.PublicTurn.TurnNumber;
		CurrentPresentation.FinalPresentationRevision = PresentationRevision;
	}
	CurrentPresentation.MatchGeneration = MatchGeneration;
	CurrentPresentation.DecisionRevision = DecisionRevision;
	CurrentPresentation.PresentationRevision = PresentationRevision;
	CurrentPresentation.bHasPendingDecision = !CurrentObservation.LegalActions.IsEmpty() && !CurrentPresentation.bGameOver;
	CurrentPresentation.StatusMessage = StatusMessage;
	for (const FWBPublicPlayerTurnSummary& Player : CurrentObservation.PublicTurn.Players)
	{
		if (Player.PlayerId == CurrentObservation.PublicTurn.CurrentPlayerId) CurrentPresentation.RemainingMP = Player.RemainingMP;
	}
	CurrentPresentation.NPCTraceEventCount = LatestOperationResult.TraceEvents.FilterByPredicate(IsNPCTrace).Num();

	UnitPresentations.Reset();
	for (const FWBPublicUnitBoardSummary& Unit : CurrentObservation.PublicBoard.Units)
	{
		FWBRuntimeUnitPresentation Presentation;
		Presentation.UnitId = Unit.UnitId;
		Presentation.OwnerId = Unit.OwnerId;
		Presentation.bNeutralNPC = Unit.OwnerId < 0;
		Presentation.bHero = HeroDefinitionIds.Contains(Unit.CardId);
		Presentation.Tile = FIntPoint(Unit.X, Unit.Y);
		Presentation.HP = Unit.HP;
		Presentation.MaxHP = Unit.MaxHP;
		Presentation.ATK = Unit.ATK;
		Presentation.AR = Unit.AR;
		Presentation.BaseRL = Unit.BaseRL;
		Presentation.CurrentRL = Unit.CurrentRL;
		Presentation.RLUsed = Unit.RLUsed;
		Presentation.AttacksRemaining = Unit.AttacksLeft;
		Presentation.PublicDefinitionId = Unit.CardId;
		for (const FWBPublicUnitStatusSummary& Status : Unit.Statuses)
		{
			FWBRuntimePublicStatusPresentation PublicStatus;
			PublicStatus.StatusId = Status.StatusId;
			PublicStatus.TurnsRemaining = Status.TurnsRemaining;
			Presentation.Statuses.Add(PublicStatus);
		}
		UnitPresentations.Add(Presentation);
		if (PreviousUnitIds.Remove(Unit.UnitId) > 0)
		{
			OnUnitPresentationUpdated.Broadcast(Unit.UnitId);
		}
		else
		{
			OnUnitPresentationAdded.Broadcast(Unit.UnitId);
		}
	}
	TArray<int32> RemovedUnitIds = PreviousUnitIds.Array();
	RemovedUnitIds.Sort();
	for (const int32 UnitId : RemovedUnitIds)
	{
		OnUnitPresentationRemoved.Broadcast(UnitId);
	}

	TilePresentations.Reset();
	for (int32 Y = 0; Y < CurrentObservation.PublicBoard.BoardHeight; ++Y)
	{
		for (int32 X = 0; X < CurrentObservation.PublicBoard.BoardWidth; ++X)
		{
			FWBRuntimeBoardTilePresentation Tile;
			Tile.Tile = FIntPoint(X, Y);
			TilePresentations.Add(Tile);
		}
	}
	for (const FWBRuntimeUnitPresentation& Unit : UnitPresentations)
	{
		const int32 Index = Unit.Tile.Y * CurrentObservation.PublicBoard.BoardWidth + Unit.Tile.X;
		if (TilePresentations.IsValidIndex(Index))
		{
			TilePresentations[Index].bOccupied = true;
			TilePresentations[Index].OccupantUnitId = Unit.UnitId;
		}
	}
	for (const FWBObservedMarkerSummary& Marker : CurrentObservation.CardZones.PublicSummary.Markers)
	{
		if (Marker.PublicState != EWBMarkerPublicState::Hidden) continue;
		const int32 Index = Marker.Tile.Y * CurrentObservation.PublicBoard.BoardWidth + Marker.Tile.X;
		if (TilePresentations.IsValidIndex(Index))
		{
			TilePresentations[Index].bHasConcealedMarker = true;
			TilePresentations[Index].PublicMarkerId = Marker.MarkerId;
		}
	}

	LegalActionPresentations.Reset();
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		FWBRuntimeLegalActionPresentation Presentation;
		Presentation.ActionId = Action.ActionId;
		Presentation.Family = RuntimeFamily(Action);
		Presentation.ActionType = ActionTypeName(Action);
		Presentation.PublicLabel = PublicActionLabel(Action);
		Presentation.PlayerId = Action.PlayerId;
		Presentation.MatchGeneration = MatchGeneration;
		Presentation.DecisionRevision = DecisionRevision;
		if (Action.Family == EWBMatchActionFamily::CoreAction)
		{
			Presentation.SourceUnitId = Action.CoreAction.SourceUnitId;
			Presentation.TargetUnitId = Action.CoreAction.TargetUnitId;
		}
		else if (Action.Family == EWBMatchActionFamily::Summon) Presentation.SourceCardInstanceId = Action.SummonRequest.SourceInstanceId;
		else if (Action.Family == EWBMatchActionFamily::Equip)
		{
			Presentation.SourceCardInstanceId = Action.EquipRequest.SourceInstanceId;
			Presentation.TargetUnitId = Action.EquipRequest.TargetUnitId;
		}
		else if (Action.Family == EWBMatchActionFamily::Activation)
		{
			Presentation.SourceUnitId = Action.ActivationCommand.Source.SourceUnitId;
			Presentation.TargetUnitId = Action.ActivationCommand.EffectRequest.Target.TargetUnitId;
			if (!Action.ActivationCommand.Source.SourceEffectId.IsEmpty())
			{
				Presentation.PublicLabel = Action.ActivationCommand.Source.SourceEffectId;
			}
		}
		else if (Action.Family == EWBMatchActionFamily::Discard) Presentation.SourceCardInstanceId = Action.DiscardCardInstanceId;
		Presentation.TargetTile = TargetTileForAction(Action);
		Presentation.bRequiresTargetSelection = Presentation.TargetUnitId >= 0
			|| (Presentation.TargetTile.X >= 0 && Presentation.TargetTile.Y >= 0);
		LegalActionPresentations.Add(Presentation);
	}
	RebuildHandPresentations();
	if (CurrentPresentation.bGameOver)
	{
		CurrentLegalDecisions.Reset();
		LegalActionPresentations.Reset();
		HandCardPresentations.Reset();
		ClearSelectionInternal(false);
	}
	else RebuildHighlights();
}

void UWBRuntimeMatchHostComponent::RebuildHandPresentations()
{
	HandCardPresentations.Reset();
	TSet<FString> SelectableCards;
	for (const FString& CardInstanceId : GetSelectableCardInstanceIds())
	{
		SelectableCards.Add(CardInstanceId);
	}
	for (int32 Index = 0; Index < CurrentObservation.CardZones.OwnHand.Cards.Num(); ++Index)
	{
		const FWBObservedCardRef& Card = CurrentObservation.CardZones.OwnHand.Cards[Index];
		FWBRuntimeHandCardPresentation Presentation;
		Presentation.CardInstanceId = Card.InstanceId;
		Presentation.DefinitionId = Card.CardId;
		Presentation.DisplayName = Card.CardId;
		Presentation.HandIndex = Index;
		Presentation.bSelectable = !CurrentPresentation.bGameOver && SelectableCards.Contains(Card.InstanceId);
		Presentation.bSelected = SelectedCardInstanceId == Card.InstanceId;

		for (const FWBRuntimeLegalActionPresentation& Action : LegalActionPresentations)
		{
			if (Action.SourceCardInstanceId == Card.InstanceId)
			{
				Presentation.AvailableActionFamilies.AddUnique(Action.Family);
			}
		}
		if (Presentation.AvailableActionFamilies.Contains(EWBRuntimeMatchActionFamily::Summon))
		{
			Presentation.CardType = FName(TEXT("Character"));
		}
		else if (Presentation.AvailableActionFamilies.Contains(EWBRuntimeMatchActionFamily::Equip))
		{
			Presentation.CardType = FName(TEXT("Wand"));
		}
		else
		{
			Presentation.CardType = FName(TEXT("Card"));
		}

		Presentation.AvailableActionFamilies.Sort([](const EWBRuntimeMatchActionFamily A, const EWBRuntimeMatchActionFamily B)
		{
			return static_cast<uint8>(A) < static_cast<uint8>(B);
		});
		HandCardPresentations.Add(Presentation);
	}
}

void UWBRuntimeMatchHostComponent::RebuildHighlights()
{
	for (FWBRuntimeBoardTilePresentation& Tile : TilePresentations)
	{
		Tile.Highlight = EWBRuntimeBoardHighlight::None;
		Tile.bSelected = false;
	}
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		if (!ActionMatchesCurrentSelection(Action)) continue;
		const FIntPoint Target = TargetTileForAction(Action);
		const int32 Index = Target.Y * CurrentObservation.PublicBoard.BoardWidth + Target.X;
		if (!TilePresentations.IsValidIndex(Index)) continue;
		const int32 TargetMatchCount = CurrentLegalDecisions.FilterByPredicate([this, Target](const FWBMatchLegalAction& Candidate)
		{
			return ActionMatchesCurrentSelection(Candidate) && TargetTileForAction(Candidate) == Target;
		}).Num();
		if (TargetMatchCount > 1)
		{
			TilePresentations[Index].Highlight = EWBRuntimeBoardHighlight::Ambiguous;
			continue;
		}
		switch (Action.Family)
		{
		case EWBMatchActionFamily::Summon: TilePresentations[Index].Highlight = EWBRuntimeBoardHighlight::Summon; break;
		case EWBMatchActionFamily::Equip: TilePresentations[Index].Highlight = EWBRuntimeBoardHighlight::Equip; break;
		case EWBMatchActionFamily::Activation: TilePresentations[Index].Highlight = EWBRuntimeBoardHighlight::Activation; break;
		default:
			TilePresentations[Index].Highlight = Action.CoreAction.Type == EWBActionType::Attack
				? EWBRuntimeBoardHighlight::Attack
				: EWBRuntimeBoardHighlight::Move;
			break;
		}
	}
	SynchronizeBoardActor();
}

void UWBRuntimeMatchHostComponent::SynchronizeBoardActor()
{
	if (BoardActor != nullptr)
	{
		BoardActor->ApplyRuntimePresentation(CurrentObservation.PublicBoard, TilePresentations, UnitPresentations);
	}
}

const FWBMatchLegalAction* UWBRuntimeMatchHostComponent::FindCurrentAction(const FString& ActionId, int32& OutMatchCount) const
{
	OutMatchCount = 0;
	const FWBMatchLegalAction* Match = nullptr;
	for (const FWBMatchLegalAction& Action : CurrentLegalDecisions)
	{
		if (Action.ActionId == ActionId) { ++OutMatchCount; Match = &Action; }
	}
	return Match;
}

FIntPoint UWBRuntimeMatchHostComponent::TargetTileForAction(const FWBMatchLegalAction& Action) const
{
	FWBTile Tile;
	int32 TargetUnitId = -1;
	if (Action.Family == EWBMatchActionFamily::CoreAction)
	{
		Tile = Action.CoreAction.ToTile;
		TargetUnitId = Action.CoreAction.TargetUnitId;
	}
	else if (Action.Family == EWBMatchActionFamily::Summon) Tile = Action.SummonRequest.TargetTile;
	else if (Action.Family == EWBMatchActionFamily::Equip) TargetUnitId = Action.EquipRequest.TargetUnitId;
	else if (Action.Family == EWBMatchActionFamily::Activation)
	{
		Tile = Action.ActivationCommand.EffectRequest.Target.TargetTile;
		TargetUnitId = Action.ActivationCommand.EffectRequest.Target.TargetUnitId;
	}
	if (TargetUnitId >= 0)
	{
		for (const FWBRuntimeUnitPresentation& Unit : UnitPresentations)
		{
			if (Unit.UnitId == TargetUnitId) return Unit.Tile;
		}
	}
	return FIntPoint(Tile.X, Tile.Y);
}

bool UWBRuntimeMatchHostComponent::ActionMatchesCurrentSelection(const FWBMatchLegalAction& Action) const
{
	bool bSourceMatches = false;
	if (SelectedUnitId >= 0)
	{
		bSourceMatches = (Action.Family == EWBMatchActionFamily::CoreAction && Action.CoreAction.SourceUnitId == SelectedUnitId)
			|| (Action.Family == EWBMatchActionFamily::Activation && Action.ActivationCommand.Source.SourceUnitId == SelectedUnitId);
	}
	else if (!SelectedCardInstanceId.IsEmpty())
	{
		bSourceMatches = (Action.Family == EWBMatchActionFamily::Summon && Action.SummonRequest.SourceInstanceId == SelectedCardInstanceId)
			|| (Action.Family == EWBMatchActionFamily::Equip && Action.EquipRequest.SourceInstanceId == SelectedCardInstanceId)
			|| (Action.Family == EWBMatchActionFamily::Discard && Action.DiscardCardInstanceId == SelectedCardInstanceId);
	}
	return bSourceMatches && (!bHasSelectedActionFamily || RuntimeFamily(Action) == SelectedActionFamily);
}

void UWBRuntimeMatchHostComponent::ClearSelectionInternal(const bool bBroadcast)
{
	SelectedUnitId = -1;
	SelectedCardInstanceId.Reset();
	PendingSelectedActionId.Reset();
	AmbiguousActionIds.Reset();
	bHasSelectedActionFamily = false;
	SelectionStatusReason.Reset();
	for (FWBRuntimeBoardTilePresentation& Tile : TilePresentations)
	{
		Tile.Highlight = EWBRuntimeBoardHighlight::None;
		Tile.bSelected = false;
	}
	if (BoardActor != nullptr) BoardActor->ClearHighlights();
	if (bBroadcast) OnSelectionChanged.Broadcast();
}
