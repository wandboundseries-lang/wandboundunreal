#include "WBProductionDiscardActivationSmoke.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBCardActivationSourceGate.h"
#include "WBProductionMatchReplayRuntime.h"

namespace WBDiscardActivationSmokePrivate
{
enum class EDiscardActivationSmokePath : uint8
{
	PassThrough,
	Negated
};

const FWBMatchLegalAction* FindDiscard(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& InstanceId)
{
	return Actions.FindByPredicate(
		[&InstanceId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Discard
				&& Action.DiscardCardInstanceId == InstanceId;
		});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId,
	const FString& InstanceId = FString(),
	const int32 TargetUnitId = INDEX_NONE)
{
	return Actions.FindByPredicate(
		[&EffectId, &InstanceId, TargetUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Activation
				&& Action.ActivationCommand.Source.SourceEffectId == EffectId
				&& (InstanceId.IsEmpty()
					|| Action.ActivationCommand.Source.SourceCardInstanceId == InstanceId)
				&& (TargetUnitId == INDEX_NONE
					|| Action.ActivationCommand.EffectRequest.Target.TargetUnitId
						== TargetUnitId);
		});
}

const FWBMatchLegalAction* FindPass(const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

bool SubmitAndCapture(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FString& OutReason)
{
	const FWBMatchOperationResult Operation =
		Coordinator.SubmitActionId(Action.PlayerId, Action.ActionId);
	if (!Operation.bOk)
	{
		OutReason = Operation.Reason;
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

bool DrainResponseByPassing(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	FString& OutReason)
{
	for (int32 Step = 0; Step < 12; ++Step)
	{
		if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Response)
		{
			return true;
		}
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindPass(Legal.Actions)
			: nullptr;
		if (Pass == nullptr
			|| !SubmitAndCapture(Coordinator, Recorder, *Pass, OutReason))
		{
			if (OutReason.IsEmpty())
			{
				OutReason = TEXT("discard_activation_smoke_pass_missing");
			}
			return false;
		}
	}
	OutReason = TEXT("discard_activation_smoke_response_stuck");
	return false;
}

bool FindCardInOwnDiscard(
	const WBMatchCoordinator& Coordinator,
	const int32 PlayerId,
	const FString& InstanceId,
	int32* OutZoneIndex = nullptr)
{
	const FWBObservedZoneSummary& Discard =
		Coordinator.BuildObservation(PlayerId).CardZones.OwnDiscard;
	for (int32 Index = 0; Index < Discard.Cards.Num(); ++Index)
	{
		if (Discard.Cards[Index].InstanceId == InstanceId)
		{
			if (OutZoneIndex != nullptr)
			{
				*OutZoneIndex = Index;
			}
			return true;
		}
	}
	return false;
}

bool VerifyReceiptPrivacy(
	const FWBProductionMatchReplayReceipt& Receipt,
	const TArray<FString>& PrivateInstanceIds,
	FString& OutJson)
{
	OutJson = WBProductionMatchReplay::SerializeReceipt(Receipt);
	TSharedPtr<FJsonObject> Object;
	if (!FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(OutJson), Object)
		|| !Object.IsValid()
		|| Object->Values.Num() != 8
		|| OutJson.Contains(TEXT("state_digest"))
		|| OutJson.Contains(TEXT("trace_digest"))
		|| OutJson.Contains(TEXT("zone_index")))
	{
		return false;
	}
	for (const FString& InstanceId : PrivateInstanceIds)
	{
		if (OutJson.Contains(InstanceId))
		{
			return false;
		}
	}
	return true;
}

bool RunPath(
	const FWBProductionRuntimeBootstrapResult& Bootstrap,
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest,
	const EDiscardActivationSmokePath Path,
	FWBProductionDiscardActivationSmokeResult& OutResult,
	FString& OutReceiptJson)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		OutResult.Reason = Started.Reason;
		return false;
	}

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap), Coordinator))
	{
		OutResult.Reason = Recorder.GetReceipt().FailureCode;
		return false;
	}

	const FWBCardZonePlayerObservation InitialObservation =
		Coordinator.BuildObservation(0).CardZones;
	TArray<FString> EchoInstances;
	for (const FWBObservedCardRef& Card : InitialObservation.OwnHand.Cards)
	{
		if (Card.CardId == TEXT("discard_fixture_echo"))
		{
			EchoInstances.Add(Card.InstanceId);
		}
	}
	EchoInstances.Sort();
	if (EchoInstances.Num() != 2)
	{
		OutResult.Reason = TEXT("discard_activation_smoke_duplicate_sources_missing");
		return false;
	}

	for (const FString& InstanceId : EchoInstances)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Discard = Legal.bOk
			? FindDiscard(Legal.Actions, InstanceId)
			: nullptr;
		if (Discard == nullptr
			|| !SubmitAndCapture(
				Coordinator, Recorder, *Discard, OutResult.Reason))
		{
			if (OutResult.Reason.IsEmpty())
			{
				OutResult.Reason = TEXT("discard_activation_smoke_discard_missing");
			}
			return false;
		}
	}

	int32 SourceIndexBefore = INDEX_NONE;
	if (!FindCardInOwnDiscard(
		Coordinator, 0,
		EchoInstances[0], &SourceIndexBefore)
		|| !FindCardInOwnDiscard(
			Coordinator, 0,
			EchoInstances[1]))
	{
		OutResult.Reason = TEXT("discard_activation_smoke_source_setup_mismatch");
		return false;
	}

	const FWBPlayerStateData* TargetPlayer = Coordinator.GetState().GetPlayerById(1);
	const FWBUnitState* TargetHero = TargetPlayer != nullptr
		? Coordinator.GetState().GetUnitById(TargetPlayer->HeroUnitId)
		: nullptr;
	if (TargetHero == nullptr)
	{
		OutResult.Reason = TEXT("discard_activation_smoke_target_missing");
		return false;
	}
	const int32 TargetUnitId = TargetHero->UnitId;
	const int32 HPBefore = TargetHero->HP;
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Activation = Legal.bOk
		? FindActivation(
			Legal.Actions,
			TEXT("discard_fixture_damage"),
			EchoInstances[0],
			TargetUnitId)
		: nullptr;
	if (Activation == nullptr
		|| Activation->ActivationCommand.Source.SourceZone
			!= EWBCardZone::Discard
		|| Activation->ActivationCommand.Source.SourceUnitId != INDEX_NONE
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Activation, OutResult.Reason))
	{
		if (OutResult.Reason.IsEmpty())
		{
			OutResult.Reason = TEXT("discard_activation_smoke_activation_missing");
		}
		return false;
	}

	int32 SourceIndexPending = INDEX_NONE;
	if (Coordinator.GetPendingEffectActivationStack().Num() != 1
		|| !FindCardInOwnDiscard(
			Coordinator, 0,
			EchoInstances[0], &SourceIndexPending)
		|| SourceIndexPending != SourceIndexBefore
		|| Coordinator.GetState().GetUnitById(TargetUnitId)->HP != HPBefore)
	{
		OutResult.Reason = TEXT("discard_activation_smoke_pending_source_mismatch");
		return false;
	}

	const FWBMatchObservation OpponentObservation = Coordinator.BuildObservation(1);
	FString OpponentActionIds;
	for (const FWBMatchLegalAction& Action : OpponentObservation.LegalActions)
	{
		OpponentActionIds += Action.ActionId;
	}
	const FWBObservedZoneSummary* HiddenDiscard =
		OpponentObservation.CardZones.PublicSummary.PlayerDiscards.FindByPredicate(
			[](const FWBObservedZoneSummary& Summary)
			{
				return Summary.OwnerPlayerId == 0;
			});
	if (HiddenDiscard == nullptr
		|| !HiddenDiscard->Cards.IsEmpty()
		|| OpponentActionIds.Contains(EchoInstances[0])
		|| OpponentActionIds.Contains(EchoInstances[1]))
	{
		OutResult.Reason = TEXT("discard_activation_smoke_privacy_mismatch");
		return false;
	}

	const FWBMatchLegalActionGenerationResult ResponseLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Response = Path == EDiscardActivationSmokePath::Negated
		? FindActivation(
			ResponseLegal.Actions, TEXT("discard_fixture_negate"))
		: FindPass(ResponseLegal.Actions);
	if (Response == nullptr
		|| !SubmitAndCapture(
			Coordinator, Recorder, *Response, OutResult.Reason)
		|| !DrainResponseByPassing(Coordinator, Recorder, OutResult.Reason))
	{
		if (OutResult.Reason.IsEmpty())
		{
			OutResult.Reason = TEXT("discard_activation_smoke_response_failed");
		}
		return false;
	}

	const bool bNegated = Path == EDiscardActivationSmokePath::Negated;
	int32 SourceIndexAfter = INDEX_NONE;
	const FString UsageKey =
		WBCardActivationSourceGate::BuildDefaultUsageKeyForSource(
			0,
			INDEX_NONE,
			TEXT("discard_fixture_echo"),
			TEXT("discard_fixture_damage"),
			EWBCardActivationSourceZone::Discard,
			EchoInstances[0]);
	const FString OtherUsageKey =
		WBCardActivationSourceGate::BuildDefaultUsageKeyForSource(
			0,
			INDEX_NONE,
			TEXT("discard_fixture_echo"),
			TEXT("discard_fixture_damage"),
			EWBCardActivationSourceZone::Discard,
			EchoInstances[1]);
	const FWBMatchLegalActionGenerationResult AfterLegal =
		Coordinator.EnumerateLegalActions();
	if (Coordinator.GetState().GetUnitById(TargetUnitId)->HP
			!= (bNegated ? HPBefore : HPBefore - 1)
		|| !FindCardInOwnDiscard(
			Coordinator, 0,
			EchoInstances[0], &SourceIndexAfter)
		|| SourceIndexAfter != SourceIndexBefore
		|| !FindCardInOwnDiscard(
			Coordinator, 0,
			EchoInstances[1])
		|| !Coordinator.GetState().HasActivationUsageKeyThisTurn(0, UsageKey)
		|| Coordinator.GetState().HasActivationUsageKeyThisTurn(0, OtherUsageKey)
		|| !AfterLegal.bOk
		|| FindActivation(
			AfterLegal.Actions,
			TEXT("discard_fixture_damage"),
			EchoInstances[1],
			TargetUnitId) == nullptr)
	{
		OutResult.Reason = TEXT("discard_activation_smoke_outcome_mismatch");
		return false;
	}

	const FString ArchiveBytes =
		WBProductionMatchReplay::Serialize(Recorder.GetArchive());
	FString PersistedBytes;
	const FWBProductionMatchReplayPersistenceResult Loaded =
		WBProductionMatchReplayPersistence::Load(
			Recorder.GetArchivePathForServer(), PersistedBytes);
	if (!Loaded.bOk || PersistedBytes != ArchiveBytes)
	{
		OutResult.Reason = Loaded.bOk
			? FString(TEXT("discard_activation_smoke_archive_mismatch"))
			: Loaded.FailureCode;
		return false;
	}

	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.SerializedArchive = PersistedBytes;
	ReplayRequest.BootstrapRequest = BootstrapRequest;
	const FWBProductionMatchReplayRunResult Replay =
		FWBProductionMatchReplayRunner::Run(ReplayRequest);
	if (!Replay.bValid
		|| Replay.RecordsVerified != Recorder.GetArchive().Records.Num()
		|| Replay.FinalStateDigest != Coordinator.GetCurrentStateDigest()
		|| Replay.FinalTraceDigest != Coordinator.GetCurrentTraceDigest()
		|| Replay.FinalGeneration != Coordinator.GetCoordinatorGeneration()
		|| Replay.FinalRevision != Coordinator.GetCoordinatorRevision())
	{
		OutResult.Reason = Replay.FailureCode.IsEmpty()
			? FString(TEXT("discard_activation_smoke_fresh_replay_mismatch"))
			: Replay.FailureCode;
		return false;
	}

	if (!VerifyReceiptPrivacy(
		Recorder.GetReceipt(), EchoInstances, OutReceiptJson))
	{
		OutResult.Reason = TEXT("discard_activation_smoke_receipt_privacy_mismatch");
		return false;
	}

	++OutResult.ScenariosVerified;
	OutResult.RecordsVerified += Replay.RecordsVerified;
	OutResult.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	OutResult.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	return true;
}
}

bool WBProductionDiscardActivationSmoke::IsRequested(const TCHAR* CommandLine)
{
	return FParse::Param(
		CommandLine != nullptr ? CommandLine : FCommandLine::Get(),
		TEXT("WandboundProductionDiscardActivationSmoke"));
}

FString WBProductionDiscardActivationSmoke::GetReceiptPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionDiscardActivationReceipt.json"));
}

FWBProductionDiscardActivationSmokeResult WBProductionDiscardActivationSmoke::Run(
	const FWBProductionRuntimeBootstrapRequest& BootstrapRequest)
{
	FWBProductionDiscardActivationSmokeResult Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}

	FString ReceiptJson;
	const WBDiscardActivationSmokePrivate::EDiscardActivationSmokePath Paths[] = {
		WBDiscardActivationSmokePrivate::EDiscardActivationSmokePath::PassThrough,
		WBDiscardActivationSmokePrivate::EDiscardActivationSmokePath::Negated
	};
	for (const WBDiscardActivationSmokePrivate::EDiscardActivationSmokePath Path : Paths)
	{
		if (!WBDiscardActivationSmokePrivate::RunPath(
			Bootstrap, BootstrapRequest, Path, Result, ReceiptJson))
		{
			return Result;
		}
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
	Result.Reason = TEXT("production_discard_activation_verified");
	return Result;
}
