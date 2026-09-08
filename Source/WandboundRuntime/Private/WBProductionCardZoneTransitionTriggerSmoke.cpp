#include "WBProductionCardZoneTransitionTriggerSmoke.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneTransitionTrigger.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionMatchReplay.h"

namespace
{
struct FScenarioResult
{
	FWBGameStateData State;
	TArray<FWBTraceEvent> Traces;
	int32 TriggerCount = 0;
	int32 TransitionCount = 0;
	FString StateDigest;
	FString TraceDigest;
};

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;
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

void AddCard(
	FWBGameStateData& State,
	EWBCardZone Zone,
	const FString& InstanceId,
	const FString& CardId)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), 0);
	TArray<FWBZoneCardEntry>* Entries = Zone == EWBCardZone::Deck
		? &Zones->Deck : Zone == EWBCardZone::Hand
			? &Zones->Hand : &Zones->Discard;
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = 0;
	Entry.Zone = Zone;
	Entry.ZoneIndex = Entries->Num();
	Entries->Add(MoveTemp(Entry));
}

bool TransferAndResolve(
	FScenarioResult& Out,
	const FWBCardDefinitionRepository& Repository,
	EWBCardZone Source,
	EWBCardZone Destination,
	const FString& InstanceId,
	int32 ExpectedTriggerCount,
	int32 RootOrder,
	FString& OutReason)
{
	FWBCardZoneTransferRequest Request;
	Request.PlayerId = 0;
	Request.SourceZone = Source;
	Request.DestinationZone = Destination;
	Request.CardInstanceId = InstanceId;
	Request.DestinationPlacement = EWBOrderedZonePlacement::Append;
	FWBCardZoneTransitionContext Context;
	Context.Cause = EWBCardZoneTransitionCause::Rule;
	Context.SourceActionId = FString::Printf(
		TEXT("zone_trigger_smoke:%d"), RootOrder);
	Context.ContinuationId = TEXT("zone_trigger_smoke");
	Context.ResolutionOrder = RootOrder;
	const FWBCardLifecycleResult Root = WBCardLifecycle::TransferExactCard(
		Out.State, Request, Context);
	if (!Root.bOk || Root.TransitionEvents.Num() != 1)
	{
		OutReason = Root.bOk
			? TEXT("zone_trigger_smoke_root_event_missing") : Root.Reason;
		return false;
	}
	WBCardZoneTransition::AppendRedactedTraces(
		Root.TransitionEvents, Out.Traces);
	const FWBCardZoneTransitionTriggerResult TriggerResult =
		WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
			Out.State, Repository, Root.TransitionEvents);
	if (!TriggerResult.bOk
		|| TriggerResult.ResolvedTriggerCount != ExpectedTriggerCount)
	{
		OutReason = TriggerResult.bOk
			? TEXT("zone_trigger_smoke_trigger_count_mismatch")
			: TriggerResult.Reason;
		return false;
	}
	Out.Traces.Append(TriggerResult.TraceEvents);
	Out.TriggerCount += TriggerResult.ResolvedTriggerCount;
	Out.TransitionCount += 1 + TriggerResult.NestedTransitionEvents.Num();
	return true;
}

bool RunScenario(
	const FWBCardDefinitionRepository& Repository,
	FScenarioResult& Out,
	FString& OutReason)
{
	Out = FScenarioResult();
	Out.State = MakeState();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		AddCard(Out.State, EWBCardZone::Deck,
			FString::Printf(TEXT("PRIVATE_DRAW_%d"), Index),
			TEXT("zone_trigger_filler"));
	}
	AddCard(Out.State, EWBCardZone::Hand,
		TEXT("PRIVATE_SELF_ENTER"), TEXT("zone_trigger_self_enter"));
	AddCard(Out.State, EWBCardZone::Hand,
		TEXT("PRIVATE_SUBJECT"), TEXT("zone_trigger_subject"));
	AddCard(Out.State, EWBCardZone::Discard,
		TEXT("PRIVATE_SELF_LEAVE"), TEXT("zone_trigger_self_leave"));
	AddCard(Out.State, EWBCardZone::Discard,
		TEXT("PRIVATE_OBSERVER_A"), TEXT("zone_trigger_observer"));
	AddCard(Out.State, EWBCardZone::Discard,
		TEXT("PRIVATE_OBSERVER_B"), TEXT("zone_trigger_observer"));

	if (!TransferAndResolve(Out, Repository,
		EWBCardZone::Hand, EWBCardZone::Discard,
		TEXT("PRIVATE_SELF_ENTER"), 3, 0, OutReason)
		|| !TransferAndResolve(Out, Repository,
			EWBCardZone::Discard, EWBCardZone::Hand,
			TEXT("PRIVATE_SELF_LEAVE"), 1, 1, OutReason)
		|| !TransferAndResolve(Out, Repository,
			EWBCardZone::Hand, EWBCardZone::Discard,
			TEXT("PRIVATE_SUBJECT"), 2, 2, OutReason))
	{
		return false;
	}
	if (Out.TriggerCount != 6 || Out.TransitionCount != 9
		|| Out.State.HasOpenReactionWindow()
		|| Out.State.HasPendingPrivateCardChoice())
	{
		OutReason = TEXT("zone_trigger_smoke_final_state_mismatch");
		return false;
	}
	const FString PublicTrace = WBReplayTrace::SerializeEvents(Out.Traces);
	for (const TCHAR* Token : { TEXT("PRIVATE_"), TEXT("zone_trigger_observer"),
		TEXT("card_instance_id"), TEXT("card_id"), TEXT("zone_index") })
	{
		if (PublicTrace.Contains(Token))
		{
			OutReason = TEXT("zone_trigger_smoke_privacy_mismatch");
			return false;
		}
	}

	FWBGameStateData ExtractState = MakeState();
	AddCard(ExtractState, EWBCardZone::Hand,
		TEXT("PRIVATE_EXTRACT"), TEXT("zone_trigger_subject"));
	const FWBCardLifecycleResult Extract =
		WBCardLifecycle::RemoveExactCardFromHand(
			ExtractState, 0, TEXT("PRIVATE_EXTRACT"));
	if (!Extract.bOk || !Extract.TransitionEvents.IsEmpty())
	{
		OutReason = TEXT("zone_trigger_smoke_extract_event");
		return false;
	}

	Out.StateDigest = WBProductionMatchReplay::BuildGameStateDigest(Out.State);
	Out.TraceDigest = WBProductionMatchReplay::BuildTraceDigest(Out.Traces);
	OutReason.Reset();
	return true;
}
}

bool WBProductionCardZoneTransitionTriggerSmoke::IsRequested(
	const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionCardZoneTransitionTriggerSmoke"));
}

FString WBProductionCardZoneTransitionTriggerSmoke::GetReceiptPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionCardZoneTransitionTriggerReceipt.json"));
}

FWBProductionCardZoneTransitionTriggerSmokeResult
WBProductionCardZoneTransitionTriggerSmoke::Run()
{
	FWBProductionCardZoneTransitionTriggerSmokeResult Result;
	const FString ManifestPath =
		TEXT("Data/Replay/CardZoneTransitionTriggerFixture/root_manifest.json");
	const FWBProductionCardDatabaseLoadResult Database =
		WBProductionCardDatabase::LoadManifestSuite(ManifestPath);
	if (!Database.bOk || !Database.Snapshot.IsValid())
	{
		Result.Reason = Database.Reason;
		return Result;
	}
	FScenarioResult First;
	FScenarioResult Second;
	if (!RunScenario(Database.Snapshot->CoreRepository, First, Result.Reason)
		|| !RunScenario(Database.Snapshot->CoreRepository, Second, Result.Reason))
	{
		return Result;
	}
	if (First.StateDigest != Second.StateDigest
		|| First.TraceDigest != Second.TraceDigest
		|| WBReplayTrace::SerializeEvents(First.Traces)
			!= WBReplayTrace::SerializeEvents(Second.Traces))
	{
		Result.Reason = TEXT("zone_trigger_smoke_determinism_mismatch");
		return Result;
	}

	FWBProductionMatchReplayReceipt Receipt;
	Receipt.bAvailable = true;
	Receipt.SchemaVersion = WBProductionMatchReplay::SchemaVersion;
	Receipt.OpaqueMatchId = TEXT("card_zone_transition_trigger_smoke");
	Receipt.RecordCount = First.TransitionCount;
	Receipt.bComplete = true;
	Receipt.bTerminal = false;
	Receipt.FinalReplayDigest = WBProductionMatchReplay::HashUtf8(
		First.StateDigest + TEXT("|") + First.TraceDigest);
	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	if (!FFileHelper::SaveStringToFile(
		WBProductionMatchReplay::SerializeReceipt(Receipt), *ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_card_zone_transition_trigger_verified");
	Result.TriggerCount = First.TriggerCount;
	Result.TransitionCount = First.TransitionCount;
	Result.FinalStateDigest = First.StateDigest;
	Result.FinalTraceDigest = First.TraceDigest;
	return Result;
}
