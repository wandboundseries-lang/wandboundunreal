#include "WBProductionCardZoneTransitionSmoke.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneTransition.h"
#include "WBProductionMatchReplay.h"

namespace WBCardZoneTransitionSmokePrivate
{
struct FScenarioResult
{
	FWBGameStateData State;
	TArray<FWBCardZoneTransitionSnapshot> Events;
	TArray<FWBTraceEvent> TraceEvents;
	FString StateDigest;
	FString TraceDigest;
};

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 4;
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBPlayerStateData Player;
		Player.PlayerId = PlayerId;
		State.Players.Add(Player);
		FWBPlayerCardZoneState Zones;
		Zones.PlayerId = PlayerId;
		State.GetMutableCardZoneStateForTest().PlayerZones.Add(Zones);
	}
	return State;
}

TArray<FWBZoneCardEntry>* GetEntries(
	FWBGameStateData& State,
	const EWBCardZone Zone)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), 0);
	if (Zone == EWBCardZone::Deck)
	{
		return &Zones->Deck;
	}
	if (Zone == EWBCardZone::Hand)
	{
		return &Zones->Hand;
	}
	return Zone == EWBCardZone::Discard ? &Zones->Discard : nullptr;
}

void AddCard(
	FWBGameStateData& State,
	const EWBCardZone Zone,
	const TCHAR* InstanceId,
	const TCHAR* CardId)
{
	TArray<FWBZoneCardEntry>* Entries = GetEntries(State, Zone);
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = 0;
	Entry.Zone = Zone;
	Entry.ZoneIndex = Entries->Num();
	Entries->Add(MoveTemp(Entry));
}

FWBCardZoneTransitionContext MakeContext(
	const EWBCardZoneTransitionCause Cause,
	const int32 Order)
{
	FWBCardZoneTransitionContext Context;
	Context.Cause = Cause;
	Context.SourceActionId = FString::Printf(
		TEXT("zone_transition_smoke:o%d"), Order);
	Context.ContinuationId = TEXT("zone_transition_smoke");
	Context.ActionDeclaration =
		EWBDeclarationProvenance::PlayerDeclared;
	Context.ResolutionOrder = Order;
	return Context;
}

bool AppendResult(
	const FWBCardLifecycleResult& Result,
	const EWBCardZone ExpectedSource,
	const EWBCardZone ExpectedDestination,
	FScenarioResult& Out,
	FString& OutReason)
{
	if (!Result.bOk || Result.TransitionEvents.Num() != 1)
	{
		OutReason = Result.bOk
			? FString(TEXT("zone_transition_smoke_event_count"))
			: Result.Reason;
		return false;
	}
	const FWBCardZoneTransitionSnapshot& Snapshot =
		Result.TransitionEvents[0];
	if (!Snapshot.IsValid()
		|| Snapshot.SourceZone != ExpectedSource
		|| Snapshot.DestinationZone != ExpectedDestination)
	{
		OutReason = TEXT("zone_transition_smoke_snapshot_mismatch");
		return false;
	}
	Out.Events.Add(Snapshot);
	WBCardZoneTransition::AppendRedactedTraces(
		Result.TransitionEvents, Out.TraceEvents);
	return true;
}

bool Transfer(
	FScenarioResult& Out,
	const EWBCardZone Source,
	const EWBCardZone Destination,
	const TCHAR* InstanceId,
	const EWBCardZoneTransitionCause Cause,
	const int32 Order,
	FString& OutReason)
{
	FWBCardZoneTransferRequest Request;
	Request.PlayerId = 0;
	Request.SourceZone = Source;
	Request.DestinationZone = Destination;
	Request.CardInstanceId = InstanceId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	return AppendResult(
		WBCardLifecycle::TransferExactCard(
			Out.State, Request, MakeContext(Cause, Order)),
		Source,
		Destination,
		Out,
		OutReason);
}

bool RunScenario(FScenarioResult& Out, FString& OutReason)
{
	Out = FScenarioResult();
	Out.State = MakeState();
	AddCard(Out.State, EWBCardZone::Deck,
		TEXT("PRIVATE_DRAW"), TEXT("private_draw_card"));
	AddCard(Out.State, EWBCardZone::Deck,
		TEXT("PRIVATE_DECK_DISCARD"), TEXT("private_deck_discard_card"));
	AddCard(Out.State, EWBCardZone::Hand,
		TEXT("PRIVATE_HAND_DISCARD"), TEXT("private_hand_discard_card"));
	AddCard(Out.State, EWBCardZone::Hand,
		TEXT("PRIVATE_HAND_DECK"), TEXT("private_hand_deck_card"));
	AddCard(Out.State, EWBCardZone::Discard,
		TEXT("PRIVATE_DISCARD_HAND"), TEXT("private_discard_hand_card"));
	AddCard(Out.State, EWBCardZone::Discard,
		TEXT("PRIVATE_DISCARD_DECK"), TEXT("private_discard_deck_card"));

	const FWBCardLifecycleResult Draw = WBCardLifecycle::DrawOneCard(
		Out.State,
		0,
		MakeContext(EWBCardZoneTransitionCause::Draw, 0));
	if (!AppendResult(
		Draw,
		EWBCardZone::Deck,
		EWBCardZone::Hand,
		Out,
		OutReason)
		|| !Transfer(
			Out, EWBCardZone::Hand, EWBCardZone::Discard,
			TEXT("PRIVATE_HAND_DISCARD"),
			EWBCardZoneTransitionCause::Rule, 1, OutReason)
		|| !Transfer(
			Out, EWBCardZone::Discard, EWBCardZone::Hand,
			TEXT("PRIVATE_DISCARD_HAND"),
			EWBCardZoneTransitionCause::Effect, 2, OutReason)
		|| !Transfer(
			Out, EWBCardZone::Discard, EWBCardZone::Deck,
			TEXT("PRIVATE_DISCARD_DECK"),
			EWBCardZoneTransitionCause::Effect, 3, OutReason)
		|| !Transfer(
			Out, EWBCardZone::Deck, EWBCardZone::Discard,
			TEXT("PRIVATE_DECK_DISCARD"),
			EWBCardZoneTransitionCause::Effect, 4, OutReason)
		|| !Transfer(
			Out, EWBCardZone::Hand, EWBCardZone::Deck,
			TEXT("PRIVATE_HAND_DECK"),
			EWBCardZoneTransitionCause::Effect, 5, OutReason))
	{
		return false;
	}

	for (int32 Index = 0; Index < Out.Events.Num(); ++Index)
	{
		if (Out.Events[Index].ResolutionOrder != Index)
		{
			OutReason = TEXT("zone_transition_smoke_order_mismatch");
			return false;
		}
	}
	const FString PublicTrace =
		WBReplayTrace::SerializeEvents(Out.TraceEvents);
	const TCHAR* PrivateTokens[] = {
		TEXT("PRIVATE_"),
		TEXT("private_"),
		TEXT("card_instance_id"),
		TEXT("card_id"),
		TEXT("zone_index")
	};
	for (const TCHAR* Token : PrivateTokens)
	{
		if (PublicTrace.Contains(Token))
		{
			OutReason = TEXT("zone_transition_smoke_privacy_mismatch");
			return false;
		}
	}
	if (Out.State.HasOpenReactionWindow()
		|| Out.State.HasPendingAttack()
		|| Out.State.HasPendingSummon()
		|| Out.State.HasPendingPrivateCardChoice())
	{
		OutReason = TEXT("zone_transition_smoke_continuation_mismatch");
		return false;
	}

	FWBGameStateData ExtractState = MakeState();
	AddCard(ExtractState, EWBCardZone::Hand,
		TEXT("PRIVATE_EXTRACT"), TEXT("private_extract_card"));
	const FWBCardLifecycleResult Extract =
		WBCardLifecycle::RemoveExactCardFromHand(
			ExtractState, 0, TEXT("PRIVATE_EXTRACT"));
	if (!Extract.bOk || !Extract.TransitionEvents.IsEmpty())
	{
		OutReason = TEXT("zone_transition_smoke_extract_event");
		return false;
	}

	Out.StateDigest =
		WBProductionMatchReplay::BuildGameStateDigest(Out.State);
	Out.TraceDigest =
		WBProductionMatchReplay::BuildTraceDigest(Out.TraceEvents);
	OutReason.Reset();
	return true;
}
}

bool WBProductionCardZoneTransitionSmoke::IsRequested(
	const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionCardZoneTransitionSmoke"));
}

FString WBProductionCardZoneTransitionSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionCardZoneTransitionReceipt.json"));
}

FWBProductionCardZoneTransitionSmokeResult
WBProductionCardZoneTransitionSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionCardZoneTransitionSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	WBCardZoneTransitionSmokePrivate::FScenarioResult First;
	WBCardZoneTransitionSmokePrivate::FScenarioResult Second;
	if (!WBCardZoneTransitionSmokePrivate::RunScenario(
			First, Result.Reason)
		|| !WBCardZoneTransitionSmokePrivate::RunScenario(
			Second, Result.Reason))
	{
		return Result;
	}
	if (First.StateDigest != Second.StateDigest
		|| First.TraceDigest != Second.TraceDigest
		|| WBReplayTrace::SerializeEvents(First.TraceEvents)
			!= WBReplayTrace::SerializeEvents(Second.TraceEvents)
		|| First.Events.Num() != Second.Events.Num())
	{
		Result.Reason = TEXT("zone_transition_smoke_determinism_mismatch");
		return Result;
	}
	for (int32 Index = 0; Index < First.Events.Num(); ++Index)
	{
		if (First.Events[Index].EventIdentity.EventId
			!= Second.Events[Index].EventIdentity.EventId)
		{
			Result.Reason = TEXT("zone_transition_smoke_event_id_mismatch");
			return Result;
		}
	}

	FWBProductionMatchReplayReceipt Receipt;
	Receipt.bAvailable = true;
	Receipt.SchemaVersion = WBProductionMatchReplay::SchemaVersion;
	Receipt.OpaqueMatchId = TEXT("card_zone_transition_smoke");
	Receipt.RecordCount = First.Events.Num();
	Receipt.bComplete = true;
	Receipt.bTerminal = false;
	Receipt.FinalReplayDigest = WBProductionMatchReplay::HashUtf8(
		First.StateDigest + TEXT("|") + First.TraceDigest);
	const FString ReceiptJson =
		WBProductionMatchReplay::SerializeReceipt(Receipt);
	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	if (!FFileHelper::SaveStringToFile(
		ReceiptJson,
		*ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_card_zone_transition_verified");
	Result.TransitionCount = First.Events.Num();
	Result.FinalStateDigest = First.StateDigest;
	Result.FinalTraceDigest = First.TraceDigest;
	return Result;
}
