#include "WBProductionHybridReplacementSmoke.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBPublicBoardSummary.h"

namespace
{
const FWBMatchLegalAction* FindEquip(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& WandCardId,
	const int32 HeroUnitId)
{
	return Actions.FindByPredicate(
		[&WandCardId, HeroUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Equip
				&& Action.EquipRequest.SourceCardId == WandCardId
				&& Action.EquipRequest.TargetUnitId == HeroUnitId;
		});
}

const FWBMatchLegalAction* FindHybridReplacement(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 HeroUnitId)
{
	return Actions.FindByPredicate(
		[HeroUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Summon
				&& Action.bHybridHeroReplacement
				&& Action.HybridSummonPlan.SacrificedUnitId == HeroUnitId
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
}

bool WBProductionHybridReplacementSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionHybridReplacementSmoke"));
}

FString WBProductionHybridReplacementSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionHybridReplacementReceipt.json"));
}

FWBProductionHybridReplacementSmokeResult
WBProductionHybridReplacementSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionHybridReplacementSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	const FWBPlayerStateData* Player = Coordinator.GetState().GetPlayerById(0);
	if (Player == nullptr || Player->HeroUnitId < 0)
	{
		Result.Reason = TEXT("hybrid_smoke_hero_missing");
		return Result;
	}
	Result.OldHeroUnitId = Player->HeroUnitId;
	const FWBUnitState* OldHero = Coordinator.GetState().GetUnitById(Result.OldHeroUnitId);
	if (OldHero == nullptr)
	{
		Result.Reason = TEXT("hybrid_smoke_hero_missing");
		return Result;
	}
	const FWBTile OldHeroTile(OldHero->X, OldHero->Y);

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	const FWBMatchLegalAction* Equip = FindEquip(
		Started.NextLegalActions,
		TEXT("hybrid_fixture_wand"),
		Result.OldHeroUnitId);
	FWBMatchOperationResult Operation;
	if (Equip == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Equip, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("hybrid_smoke_equip_missing");
		return Result;
	}
	const FString PaidWandInstanceId = Equip->EquipRequest.SourceInstanceId;

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Replacement = Legal.bOk
		? FindHybridReplacement(Legal.Actions, Result.OldHeroUnitId)
		: nullptr;
	if (Replacement == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Replacement, Operation, Result.Reason))
	{
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("hybrid_smoke_replacement_missing");
		return Result;
	}
	Result.NewHeroUnitId = Operation.bOk
		? Coordinator.GetState().GetPlayerById(0)->HeroUnitId
		: -1;
	const FWBUnitState* Removed = Coordinator.GetState().GetUnitById(Result.OldHeroUnitId);
	const FWBUnitState* NewHero = Coordinator.GetState().GetUnitById(Result.NewHeroUnitId);
	if (Operation.bTerminal
		|| Coordinator.GetState().bGameOver
		|| Coordinator.GetState().WinnerPlayerId != -1
		|| Result.NewHeroUnitId < 0
		|| Result.NewHeroUnitId == Result.OldHeroUnitId
		|| Removed == nullptr || Removed->IsUnitOnBoard()
		|| NewHero == nullptr || !NewHero->IsUnitOnBoard()
		|| NewHero->CardId != TEXT("hybrid_fixture_replacement")
		|| FWBTile(NewHero->X, NewHero->Y) != OldHeroTile
		|| !Coordinator.GetState().GetCardZoneState().EquippedCards.IsEmpty()
		|| CountDiscardInstance(Coordinator.GetState(), 0, PaidWandInstanceId) != 1)
	{
		Result.Reason = TEXT("hybrid_smoke_replacement_state_mismatch");
		return Result;
	}
	for (const FWBTraceEvent& Trace : Operation.TraceEvents)
	{
		if (Trace.Kind == FName(TEXT("hero_defeated"))
			|| Trace.Kind == FName(TEXT("terminal_state_committed"))
			|| Trace.Kind == FName(TEXT("game_over")))
		{
			Result.Reason = TEXT("hybrid_smoke_terminal_trace_emitted");
			return Result;
		}
	}
	const FWBPublicBoardSummary PublicBoard =
		WBPublicBoardSummary::Build(Coordinator.GetState());
	const FWBPublicUnitBoardSummary* PublicHero = PublicBoard.Units.FindByPredicate(
		[&Result](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.UnitId == Result.NewHeroUnitId;
		});
	if (PublicHero == nullptr || !PublicHero->bHeroUnit
		|| PublicBoard.Units.ContainsByPredicate(
			[&Result](const FWBPublicUnitBoardSummary& Unit)
			{
				return Unit.UnitId == Result.OldHeroUnitId;
			}))
	{
		Result.Reason = TEXT("hybrid_smoke_public_summary_mismatch");
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
		if (Result.Reason.IsEmpty()) Result.Reason = TEXT("hybrid_smoke_continued_action_missing");
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
			? FString(TEXT("hybrid_smoke_archive_mismatch"))
			: Loaded.FailureCode;
		return Result;
	}

	FWBProductionMatchReplayRunRequest RunRequest;
	RunRequest.SerializedArchive = PersistedBytes;
	RunRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(RunRequest);
	const auto* FinalZones = Coordinator.GetState().GetCardZoneState().PlayerZones.FindByPredicate(
		[](const auto& Candidate)
		{
			return Candidate.PlayerId == 0;
		});
	if (!Replay.bValid || Replay.bTerminal || Replay.bComplete
		|| Replay.FinalHeroUnitIds.Num() < 1
		|| Replay.FinalHeroUnitIds[0] != Result.NewHeroUnitId
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
			? FString(TEXT("hybrid_smoke_fresh_replay_mismatch"))
			: Replay.FailureCode;
		return Result;
	}

	const FString ReceiptPath = GetReceiptPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	if (!FFileHelper::SaveStringToFile(
		WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt()),
		*ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Reason = TEXT("replay_write_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("production_hybrid_replacement_verified");
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return Result;
}
