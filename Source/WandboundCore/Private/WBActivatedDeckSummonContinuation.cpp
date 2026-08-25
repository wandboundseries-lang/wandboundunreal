#include "WBActivatedDeckSummonContinuation.h"

#include "WBCardDefinitionRepository.h"
#include "WBCardZoneState.h"
#include "WBTerminalOutcome.h"

namespace
{
constexpr int32 ActivatedDeckSummonBoardSize = 9;

FWBActivatedDeckSummonContinuationResult MakeActivatedDeckSummonFailure(
	const FString& Reason)
{
	FWBActivatedDeckSummonContinuationResult Result;
	Result.Reason = Reason;
	return Result;
}

bool EquippedLess(const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
{
	if (A.EquipOrder != B.EquipOrder) return A.EquipOrder < B.EquipOrder;
	if (A.SlotId != B.SlotId) return A.SlotId < B.SlotId;
	return A.Card.InstanceId < B.Card.InstanceId;
}

void NormalizeActivatedDeckSummonZone(TArray<FWBZoneCardEntry>& Entries)
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].ZoneIndex = Index;
	}
}

bool ReleaseSnapshotWandsToDiscard(
	FWBGameStateData& State,
	const FWBActivatedEffectSourceSnapshot& Snapshot,
	FString& OutReason)
{
	FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), Snapshot.ControllerPlayerId);
	if (PlayerZones == nullptr)
	{
		OutReason = TEXT("player_zones_missing");
		return false;
	}
	for (const FWBEquippedCardEntry& Wand : Snapshot.EquippedWands)
	{
		FWBZoneCardEntry Existing;
		if (WBCardZoneState::FindCardByInstanceId(
			State.GetCardZoneState(), Wand.Card.InstanceId, Existing))
		{
			OutReason = TEXT("sacrifice_snapshot_wand_already_zoned");
			return false;
		}
		FWBZoneCardEntry Discarded;
		Discarded.Card = Wand.Card;
		Discarded.Zone = EWBCardZone::Discard;
		Discarded.ZoneIndex = PlayerZones->Discard.Num();
		PlayerZones->Discard.Add(MoveTemp(Discarded));
	}
	NormalizeActivatedDeckSummonZone(PlayerZones->Discard);
	WBCardZoneState::SortOrderedZonesDeterministically(
		State.GetMutableCardZoneStateForTest());
	OutReason.Reset();
	return true;
}

TArray<FWBZoneCardEntry> BuildEligibleDeckEntries(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 PlayerId,
	const FString& RequiredFaction)
{
	TArray<FWBZoneCardEntry> Eligible;
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), PlayerId);
	if (Zones == nullptr) return Eligible;
	for (const FWBZoneCardEntry& Entry : Zones->Deck)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Entry.Card.CardId);
		if (Lookup.bFound
			&& Lookup.Definition.Kind == EWBCardDefinitionKind::Character
			&& Lookup.Definition.PublicFactions.Contains(RequiredFaction))
		{
			Eligible.Add(Entry);
		}
	}
	Eligible.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
	{
		if (A.ZoneIndex != B.ZoneIndex) return A.ZoneIndex < B.ZoneIndex;
		return A.Card.InstanceId < B.Card.InstanceId;
	});
	return Eligible;
}

FWBTraceEvent MakeTrace(
	const FName Kind,
	const FWBActivatedEffectSourceSnapshot& Snapshot,
	const FString& ActionId,
	const FString& FrameId)
{
	FWBTraceEvent Trace;
	Trace.Kind = Kind;
	Trace.ActionId = ActionId;
	Trace.PlayerId = Snapshot.ControllerPlayerId;
	Trace.SourceUnitId = Snapshot.SourceUnitId;
	Trace.CardId = Snapshot.SourceCardId;
	Trace.FromTile = Snapshot.SourceTile;
	Trace.PreviousBaseRL = Snapshot.BaseRLSnapshot;
	Trace.PreviousCurrentRL = Snapshot.CurrentRLSnapshot;
	Trace.PreviousRLUsed = Snapshot.RLUsedSnapshot;
	Trace.CardCount = Snapshot.EquippedWands.Num();
	Trace.PendingEffectFrameId = FrameId;
	Trace.bHeroUnit = Snapshot.bWasHero;
	Trace.bOk = true;
	return Trace;
}
}

FWBActivatedDeckSummonContinuationResult
WBActivatedDeckSummonContinuation::Resolve(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBCardActivationCommand& Command,
	const FString& ActivationActionId,
	const FString& PendingEffectFrameId,
	const int32 ResumePriorityPlayerId,
	const int32 ResumeMatchPhase)
{
	const FWBGenericEffectPayload* Payload = nullptr;
	for (const FWBGenericEffectPayload& Candidate :
		Command.EffectRequest.Payloads)
	{
		if (Candidate.Operation != EWBGenericEffectOp::
			SacrificeSourceThenSummonCharacterFromDeckToSourceTile)
		{
			continue;
		}
		if (Payload != nullptr)
		{
			return MakeActivatedDeckSummonFailure(
				TEXT("multiple_activated_deck_summon_continuations"));
		}
		Payload = &Candidate;
	}

	FWBActivatedDeckSummonContinuationResult Result;
	Result.bOk = true;
	if (Payload == nullptr)
	{
		return Result;
	}
	Result.bHandled = true;

	if (!FWBGameStateData::IsValidPlayerId(Command.Source.PlayerId)
		|| Command.Source.SourceUnitId < 0
		|| ActivationActionId.IsEmpty()
		|| PendingEffectFrameId.IsEmpty()
		|| Payload->RequiredSourceFaction.IsEmpty()
		|| Payload->RequiredReplacementFaction.IsEmpty()
		|| Payload->RequiredReplacementKind
			!= EWBEffectReplacementCardKind::Character
		|| Payload->InheritancePolicy != EWBEffectInheritancePolicy::
			TransferEquippedWandsAndAddSourceCurrentRL)
	{
		return MakeActivatedDeckSummonFailure(
			TEXT("activated_deck_summon_metadata_invalid"));
	}

	const FWBUnitState* Source = State.GetUnitById(Command.Source.SourceUnitId);
	if (Source == nullptr || Source->bDefeated || !Source->IsUnitOnBoard())
	{
		return MakeActivatedDeckSummonFailure(
			TEXT("activated_deck_summon_source_unavailable"));
	}
	if (Source->OwnerId != Command.Source.PlayerId
		|| Source->CardId != Command.Source.SourceCardId)
	{
		return MakeActivatedDeckSummonFailure(
			TEXT("activated_deck_summon_source_mismatch"));
	}
	const FWBCardDefinitionRepositoryLookupResult SourceDefinition =
		WBCardDefinitionRepository::FindCardById(Repository, Source->CardId);
	if (!SourceDefinition.bFound
		|| !SourceDefinition.Definition.PublicFactions.Contains(
			Payload->RequiredSourceFaction))
	{
		return MakeActivatedDeckSummonFailure(
			TEXT("activated_deck_summon_source_faction_mismatch"));
	}
	const FWBTile SourceTile(Source->X, Source->Y);
	if (SourceTile.X < 0 || SourceTile.X >= ActivatedDeckSummonBoardSize
		|| SourceTile.Y < 0 || SourceTile.Y >= ActivatedDeckSummonBoardSize)
	{
		return MakeActivatedDeckSummonFailure(
			TEXT("activated_deck_summon_source_tile_invalid"));
	}

	FWBGameStateData WorkingState = State;
	const FWBUnitState* WorkingSource = WorkingState.GetUnitById(
		Command.Source.SourceUnitId);
	FWBActivatedEffectSourceSnapshot Snapshot;
	Snapshot.SourceUnitId = WorkingSource->UnitId;
	Snapshot.SourceCardId = WorkingSource->CardId;
	Snapshot.ControllerPlayerId = WorkingSource->OwnerId;
	Snapshot.SourceTile = SourceTile;
	const FWBPlayerStateData* Player = WorkingState.GetPlayerById(
		WorkingSource->OwnerId);
	Snapshot.bWasHero = Player != nullptr
		&& Player->HeroUnitId == WorkingSource->UnitId;
	Snapshot.BaseRLSnapshot = WorkingSource->GetBaseRLForRules();
	Snapshot.CurrentRLSnapshot = WorkingSource->GetCurrentRLForRules();
	Snapshot.RLUsedSnapshot = WorkingSource->RLUsed;
	for (const FWBEquippedCardEntry& Entry :
		WorkingState.GetCardZoneState().EquippedCards)
	{
		if (Entry.EquippedToUnitId == WorkingSource->UnitId)
		{
			Snapshot.EquippedWands.Add(Entry);
		}
	}
	Snapshot.EquippedWands.Sort(EquippedLess);

	FWBCardZoneState& Zones = WorkingState.GetMutableCardZoneStateForTest();
	for (const FWBEquippedCardEntry& Wand : Snapshot.EquippedWands)
	{
		const int32 Removed = Zones.EquippedCards.RemoveAll(
			[&Wand](const FWBEquippedCardEntry& Entry)
			{
				return Entry.Card.InstanceId == Wand.Card.InstanceId;
			});
		if (Removed != 1)
		{
			return MakeActivatedDeckSummonFailure(
				TEXT("sacrifice_snapshot_wand_unavailable"));
		}
	}

	FWBUnitState* MutableSource = WorkingState.GetMutableUnitById(
		WorkingSource->UnitId);
	MutableSource->ResonanceModifiers.Reset();
	MutableSource->SetCanonicalRL(
		MutableSource->GetBaseRLForRules(),
		MutableSource->GetBaseRLForRules(), 0);
	MutableSource->RemoveUnitFromBoard();
	if (WorkingState.HasPendingAttack()
		&& (WorkingState.PendingAttack.AttackerUnitId == MutableSource->UnitId
			|| WorkingState.PendingAttack.DefenderUnitId == MutableSource->UnitId))
	{
		WorkingState.ClearPendingAttack();
	}

	Result.TraceEvents.Add(MakeTrace(
		FName(TEXT("activated_effect_source_snapshotted")),
		Snapshot, ActivationActionId, PendingEffectFrameId));
	Result.TraceEvents.Add(MakeTrace(
		FName(TEXT("unit_sacrificed")), Snapshot,
		ActivationActionId, PendingEffectFrameId));

	if (Snapshot.bWasHero)
	{
		FString ReleaseReason;
		if (!ReleaseSnapshotWandsToDiscard(
			WorkingState, Snapshot, ReleaseReason))
		{
			return MakeActivatedDeckSummonFailure(ReleaseReason);
		}
		WorkingState.bGameOver = true;
		WorkingState.WinnerPlayerId = 1 - Snapshot.ControllerPlayerId;
		WorkingState.TerminalOutcome.bTerminal = true;
		WorkingState.TerminalOutcome.WinnerPlayerId =
			WorkingState.WinnerPlayerId;
		WorkingState.TerminalOutcome.LoserPlayerId =
			Snapshot.ControllerPlayerId;
		WorkingState.TerminalOutcome.Reason =
			EWBTerminalReason::HeroDefeatedWithoutReplacement;
		WorkingState.TerminalOutcome.Source = EWBTerminalSource::Effect;
		WorkingState.TerminalOutcome.TurnNumber = WorkingState.TurnNumber;
		FWBTraceEvent HeroTrace = MakeTrace(
			FName(TEXT("hero_sacrifice_committed")), Snapshot,
			ActivationActionId, PendingEffectFrameId);
		HeroTrace.WinningPlayerId = WorkingState.WinnerPlayerId;
		Result.TraceEvents.Add(MoveTemp(HeroTrace));
		State = MoveTemp(WorkingState);
		return Result;
	}

	const TArray<FWBZoneCardEntry> Eligible = BuildEligibleDeckEntries(
		WorkingState, Repository, Snapshot.ControllerPlayerId,
		Payload->RequiredReplacementFaction);
	if (Eligible.IsEmpty())
	{
		FString ReleaseReason;
		if (!ReleaseSnapshotWandsToDiscard(
			WorkingState, Snapshot, ReleaseReason))
		{
			return MakeActivatedDeckSummonFailure(ReleaseReason);
		}
		FWBTraceEvent NoChoice = MakeTrace(
			FName(TEXT("activated_effect_deck_summon_completed")),
			Snapshot, ActivationActionId, PendingEffectFrameId);
		NoChoice.Reason = TEXT("no_eligible_deck_character");
		Result.TraceEvents.Add(MoveTemp(NoChoice));
		State = MoveTemp(WorkingState);
		return Result;
	}

	FWBPendingMandatoryDeckChoiceState Choice;
	Choice.bActive = true;
	Choice.Origin = EWBMandatoryDeckChoiceOrigin::
		ActivatedEffectContinuation;
	Choice.ChoiceId = PendingEffectFrameId + TEXT(":deck_summon");
	Choice.SourceActionId = ActivationActionId;
	Choice.SourceEffectFrameId = PendingEffectFrameId;
	Choice.ControllerPlayerId = Snapshot.ControllerPlayerId;
	Choice.RequiredFaction = Payload->RequiredReplacementFaction;
	Choice.DestinationTile = Snapshot.SourceTile;
	Choice.ActivatedEffectSourceSnapshot = Snapshot;
	Choice.bApplyCSNInheritance = true;
	Choice.ResumePriorityPlayerId = ResumePriorityPlayerId;
	Choice.ResumeMatchPhase = ResumeMatchPhase;
	for (const FWBZoneCardEntry& Entry : Eligible)
	{
		Choice.EligibleCardInstanceIds.Add(Entry.Card.InstanceId);
	}
	WorkingState.PendingMandatoryDeckChoice = MoveTemp(Choice);

	FWBTraceEvent Opened = MakeTrace(
		FName(TEXT("activated_effect_deck_choice_opened")),
		Snapshot, ActivationActionId, PendingEffectFrameId);
	Opened.CardCount = Eligible.Num();
	Result.TraceEvents.Add(MoveTemp(Opened));
	Result.bPendingChoice = true;
	State = MoveTemp(WorkingState);
	return Result;
}
