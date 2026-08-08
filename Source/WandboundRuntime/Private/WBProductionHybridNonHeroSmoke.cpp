#include "WBProductionHybridNonHeroSmoke.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "WBProductionMatchReplayRuntime.h"

namespace
{
const FWBMatchLegalAction* FindCharacterSummon(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& CardId)
{
	return Actions.FindByPredicate([&CardId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& !Action.bHybridSummon
			&& Action.SummonRequest.SourceCardId == CardId;
	});
}

const FWBMatchLegalAction* FindEquip(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 TargetUnitId)
{
	return Actions.FindByPredicate([TargetUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip
			&& Action.EquipRequest.SourceCardId == TEXT("hybrid_nonhero_wand")
			&& Action.EquipRequest.TargetUnitId == TargetUnitId;
	});
}

const FWBMatchLegalAction* FindNonHeroHybrid(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 SacrificedUnitId,
	const FWBTile& Destination)
{
	return Actions.FindByPredicate(
		[SacrificedUnitId, Destination](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Summon
				&& Action.bHybridSummon
				&& !Action.bHybridHeroReplacement
				&& Action.HybridSummonPlan.SacrificedUnitId
					== SacrificedUnitId
				&& Action.HybridSummonPlan.DestinationTile == Destination
				&& Action.HybridSummonPlan.WandPaymentSource
					== EWBHybridWandPaymentSource::SacrificedUnit;
		});
}

const FWBMatchLegalAction* FindDiscard(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Discard;
	});
}

const FWBPublicUnitBoardSummary* FindPublicUnitByCard(
	const FWBMatchObservation& Observation,
	const FString& CardId)
{
	return Observation.PublicBoard.Units.FindByPredicate(
		[&CardId](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.CardId == CardId;
		});
}

bool SubmitAndCapture(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FWBMatchOperationResult& OutOperation,
	FString& OutReason)
{
	OutOperation = Coordinator.SubmitActionId(Action.PlayerId, Action.ActionId);
	if (!OutOperation.bOk)
	{
		OutReason = OutOperation.Reason;
		return false;
	}
	Recorder.CaptureCommittedActions(Coordinator);
	if (!Recorder.IsAvailable())
	{
		OutReason = Recorder.GetReceipt().FailureCode;
		return false;
	}
	return true;
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

int32 CountDiscardInstance(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId)
{
	const auto* Zones = State.GetCardZoneState().PlayerZones.FindByPredicate(
		[PlayerId](const auto& Candidate)
		{
			return Candidate.PlayerId == PlayerId;
		});
	if (Zones == nullptr) return 0;
	int32 Count = 0;
	for (const FWBZoneCardEntry& Entry : Zones->Discard)
	{
		if (Entry.Card.InstanceId == InstanceId)
		{
			++Count;
		}
	}
	return Count;
}

bool ObservedCardContains(
	const FWBObservedCardRef& Card,
	const FString& Forbidden)
{
	return Card.InstanceId.Contains(Forbidden)
		|| Card.CardId.Contains(Forbidden);
}

bool ZoneContains(
	const FWBObservedZoneSummary& Zone,
	const FString& Forbidden)
{
	return Zone.Cards.ContainsByPredicate(
		[&Forbidden](const FWBObservedCardRef& Card)
		{
			return ObservedCardContains(Card, Forbidden);
		});
}

bool ObservationContains(
	const FWBMatchObservation& Observation,
	const FString& Forbidden)
{
	const FWBCardZonePlayerObservation& Zones = Observation.CardZones;
	if (ZoneContains(Zones.OwnHand, Forbidden)
		|| ZoneContains(Zones.OwnDiscard, Forbidden)
		|| ZoneContains(Zones.OwnDeck, Forbidden))
	{
		return true;
	}
	const FWBCardZonePublicSummary& Public = Zones.PublicSummary;
	for (const FWBObservedZoneSummary& Zone : Public.PlayerDecks)
	{
		if (ZoneContains(Zone, Forbidden)) return true;
	}
	for (const FWBObservedZoneSummary& Zone : Public.PlayerHands)
	{
		if (ZoneContains(Zone, Forbidden)) return true;
	}
	for (const FWBObservedZoneSummary& Zone : Public.PlayerDiscards)
	{
		if (ZoneContains(Zone, Forbidden)) return true;
	}
	return Public.Equipped.OwnVisibleEquippedCards.ContainsByPredicate(
		[&Forbidden](const FWBEquippedCardEntry& Entry)
		{
			return Entry.Card.InstanceId.Contains(Forbidden)
				|| Entry.Card.CardId.Contains(Forbidden);
		});
}
}

bool WBProductionHybridNonHeroSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionHybridNonHeroSmoke"));
}

FString WBProductionHybridNonHeroSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionHybridNonHeroReceipt.json"));
}

FWBProductionHybridNonHeroSmokeResult WBProductionHybridNonHeroSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionHybridNonHeroSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	const FWBMatchObservation InitialObservation = Coordinator.BuildObservation(0);
	const FWBPublicUnitBoardSummary* OriginalHero =
		InitialObservation.PublicBoard.Units.FindByPredicate(
			[](const FWBPublicUnitBoardSummary& Unit)
			{
				return Unit.OwnerId == 0 && Unit.bHeroUnit;
			});
	if (OriginalHero == nullptr)
	{
		Result.Reason = TEXT("hybrid_nonhero_smoke_hero_missing");
		return Result;
	}
	Result.OriginalHeroUnitId = OriginalHero->UnitId;

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	FWBMatchOperationResult Operation;
	const FWBMatchLegalAction* SummonAlpha = FindCharacterSummon(
		Started.NextLegalActions,
		TEXT("hybrid_nonhero_sacrifice_alpha"));
	if (SummonAlpha == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *SummonAlpha, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("hybrid_nonhero_smoke_alpha_summon_missing");
		return Result;
	}
	const FWBMatchObservation AlphaObservation = Coordinator.BuildObservation(0);
	const FWBPublicUnitBoardSummary* Alpha = FindPublicUnitByCard(
		AlphaObservation,
		TEXT("hybrid_nonhero_sacrifice_alpha"));
	if (Alpha == nullptr || Alpha->bHeroUnit)
	{
		Result.Reason = TEXT("hybrid_nonhero_smoke_alpha_missing");
		return Result;
	}
	Result.SacrificedUnitId = Alpha->UnitId;
	Result.Destination = FWBTile(Alpha->X, Alpha->Y);

	const FWBMatchLegalActionGenerationResult AfterAlpha =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* SummonBeta = AfterAlpha.bOk
		? FindCharacterSummon(
			AfterAlpha.Actions,
			TEXT("hybrid_nonhero_sacrifice_beta"))
		: nullptr;
	if (SummonBeta == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *SummonBeta, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("hybrid_nonhero_smoke_beta_summon_missing");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult AfterBeta =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Equip = AfterBeta.bOk
		? FindEquip(AfterBeta.Actions, Result.SacrificedUnitId)
		: nullptr;
	if (Equip == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Equip, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("hybrid_nonhero_smoke_equip_missing");
		return Result;
	}
	const FString PaidWandInstanceId = Equip->EquipRequest.SourceInstanceId;

	const FWBMatchLegalActionGenerationResult HybridLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Hybrid = HybridLegal.bOk
		? FindNonHeroHybrid(
			HybridLegal.Actions,
			Result.SacrificedUnitId,
			Result.Destination)
		: nullptr;
	if (Hybrid == nullptr)
	{
		Result.Reason = HybridLegal.bOk
			? FString(TEXT("hybrid_nonhero_smoke_action_missing"))
			: HybridLegal.Reason;
		return Result;
	}
	Result.PaymentSource = Hybrid->HybridSummonPlan.WandPaymentSource;
	if (!SubmitAndCapture(
		Coordinator, Recorder, *Hybrid, Operation, Result.Reason))
	{
		return Result;
	}

	const FWBMatchObservation AfterHybrid = Coordinator.BuildObservation(0);
	const FWBPublicUnitBoardSummary* PublicHero =
		AfterHybrid.PublicBoard.Units.FindByPredicate(
			[&Result](const FWBPublicUnitBoardSummary& Unit)
			{
				return Unit.UnitId == Result.OriginalHeroUnitId;
			});
	const FWBPublicUnitBoardSummary* PublicHybrid = FindPublicUnitByCard(
		AfterHybrid,
		TEXT("hybrid_nonhero_summon"));
	if (PublicHybrid != nullptr)
	{
		Result.NewHybridUnitId = PublicHybrid->UnitId;
	}
	const bool bSacrificeStillPublic =
		AfterHybrid.PublicBoard.Units.ContainsByPredicate(
			[&Result](const FWBPublicUnitBoardSummary& Unit)
			{
				return Unit.UnitId == Result.SacrificedUnitId;
			});
	const bool bPaidWandPublic = ObservationContains(
		Coordinator.BuildObservation(1),
		PaidWandInstanceId);
	const bool bPaidIdentityInTrace = Operation.TraceEvents.ContainsByPredicate(
		[&PaidWandInstanceId](const FWBTraceEvent& Trace)
		{
			return Trace.CardInstanceId == PaidWandInstanceId
				|| Trace.CardId == TEXT("hybrid_nonhero_wand");
		});
	if (Operation.bTerminal
		|| Coordinator.GetState().bGameOver
		|| PublicHero == nullptr || !PublicHero->bHeroUnit
		|| PublicHybrid == nullptr || PublicHybrid->bHeroUnit
		|| FWBTile(PublicHybrid->X, PublicHybrid->Y) != Result.Destination
		|| bSacrificeStillPublic
		|| !Coordinator.GetState().GetCardZoneState().EquippedCards.IsEmpty()
		|| CountDiscardInstance(
			Coordinator.GetState(), 0, PaidWandInstanceId) != 1
		|| bPaidWandPublic
		|| bPaidIdentityInTrace
		|| !HasTrace(Operation.TraceEvents, TEXT("unit_sacrificed"))
		|| !HasTrace(Operation.TraceEvents, TEXT("wand_payment_committed"))
		|| !HasTrace(Operation.TraceEvents, TEXT("hybrid_summoned"))
		|| HasTrace(Operation.TraceEvents, TEXT("hero_sacrifice_committed"))
		|| HasTrace(Operation.TraceEvents, TEXT("hero_replacement_committed"))
		|| HasTrace(Operation.TraceEvents, TEXT("hero_defeated"))
		|| HasTrace(Operation.TraceEvents, TEXT("game_over")))
	{
		Result.Reason = TEXT("hybrid_nonhero_smoke_state_mismatch");
		return Result;
	}

	const FWBMatchLegalActionGenerationResult Continued =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Discard = Continued.bOk
		? FindDiscard(Continued.Actions)
		: nullptr;
	if (Discard == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Discard, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty())
			Result.Reason = TEXT("hybrid_nonhero_smoke_continued_action_missing");
		return Result;
	}

	const FString ArchiveBytes =
		WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	FString PersistedBytes;
	const FWBProductionMatchReplayPersistenceResult Loaded =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(), PersistedBytes);
	if (!Loaded.bOk || PersistedBytes != ArchiveBytes
		|| Recorder.GetArchive().Footer.bComplete
		|| Recorder.GetArchive().Footer.bTerminal)
	{
		Result.Reason = Loaded.bOk
			? FString(TEXT("hybrid_nonhero_smoke_archive_mismatch"))
			: Loaded.FailureCode;
		return Result;
	}

	FWBProductionMatchReplayRunRequest RunRequest;
	RunRequest.SerializedArchive = PersistedBytes;
	RunRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(RunRequest);
	const auto* FinalZones =
		Coordinator.GetState().GetCardZoneState().PlayerZones.FindByPredicate(
			[](const auto& Candidate)
			{
				return Candidate.PlayerId == 0;
			});
	if (!Replay.bValid || Replay.bTerminal || Replay.bComplete
		|| Replay.FinalHeroUnitIds.Num() < 1
		|| Replay.FinalHeroUnitIds[0] != Result.OriginalHeroUnitId
		|| Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest()
		|| Replay.FinalGeneration != Coordinator.GetCoordinatorGeneration()
		|| Replay.FinalRevision != Coordinator.GetCoordinatorRevision()
		|| Replay.FinalEquippedCardCount != 0
		|| Replay.FinalDiscardCounts.Num() < 1
		|| FinalZones == nullptr
		|| Replay.FinalDiscardCounts[0] != FinalZones->Discard.Num())
	{
		Result.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("hybrid_nonhero_smoke_fresh_replay_mismatch"))
			: Replay.FailureCode;
		return Result;
	}

	const FString ReceiptJson =
		WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt());
	TSharedPtr<FJsonObject> ReceiptObject;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(ReceiptJson), ReceiptObject)
		|| !ReceiptObject.IsValid()
		|| ReceiptObject->Values.Num() != 8)
	{
		Result.Reason = TEXT("hybrid_nonhero_smoke_receipt_shape_mismatch");
		return Result;
	}
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
	Result.Reason = TEXT("production_hybrid_nonhero_verified");
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
