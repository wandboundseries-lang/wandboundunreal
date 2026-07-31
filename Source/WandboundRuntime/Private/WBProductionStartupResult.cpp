#include "WBProductionStartupResult.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 ProductionStartedExitCode = 0;
constexpr int32 ProductionBlockedExitCode = 12;
constexpr int32 ProductionInvalidExitCode = 13;

FString NormalizeResultCode(
	const FWBProductionRuntimeBootstrapResult& Bootstrap)
{
	if (Bootstrap.bOk)
	{
		return FString();
	}
	if (Bootstrap.Reason.StartsWith(TEXT("production_match_spec_blocked_")))
	{
		return Bootstrap.Reason;
	}
	if (Bootstrap.Reason.Contains(TEXT("digest")))
	{
		return TEXT("production_digest_mismatch");
	}
	if (Bootstrap.Reason.Contains(TEXT("unsupported")))
	{
		return TEXT("production_match_spec_blocked_by_unsupported_definitions");
	}
	return TEXT("production_bundle_invalid");
}
}

FString WBProductionStartupResult::GetDefaultResultPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("SmokeTest/WandboundProductionStartupResult.json"));
}

FWBProductionStartupResult WBProductionStartupResult::FromBootstrap(
	const FWBProductionRuntimeBootstrapRequest& Request,
	const FWBProductionRuntimeBootstrapResult& Bootstrap)
{
	FWBProductionStartupResult Result;
	Result.bBundleLoaded = Bootstrap.Database.IsValid();
	Result.BundleDigest = Bootstrap.Database.IsValid()
		? Bootstrap.Database->ContentDigest
		: FString();
	Result.FormatId = Bootstrap.ActiveFormat.FormatId;
	Result.FormatVersion = Bootstrap.ActiveFormat.FormatVersion;
	Result.FormatDigest = Bootstrap.ActiveFormat.Digest;
	Result.GameStartAddendumId =
		Bootstrap.GameStartAddendum.AddendumId;
	Result.GameStartAddendumVersion =
		Bootstrap.GameStartAddendum.AddendumVersion;
	Result.GameStartAddendumDigest =
		Bootstrap.GameStartAddendum.Digest;
	Result.bMatchSpecPresent = !Request.MatchSpecificationPath.IsEmpty();
	Result.bBlocked =
		Bootstrap.Reason.StartsWith(TEXT("production_match_spec_blocked_"));
	Result.ResultCode = NormalizeResultCode(Bootstrap);
	return Result;
}

FWBProductionStartupResult
WBProductionStartupResult::StartedFromBootstrap(
	const FWBProductionStartupResult& BootstrapResult,
	const WBMatchCoordinator& Coordinator,
	const int32 Generation,
	const int32 Revision,
	const bool bPlayableDecisionReached)
{
	FWBProductionStartupResult Result = BootstrapResult;
	Result.bMatchInitialized = Coordinator.IsInitialized();
	Result.bHeroSpawnBatchCommitted =
		Coordinator.WasHeroSpawnBatchCommitted();
	Result.bHeroSetupTriggersResolved =
		Coordinator.WereHeroSetupTriggersResolved();
	Result.bOpeningHandsDrawn =
		Coordinator.WereOpeningHandsDrawn();
	const FWBTurnStartSequenceState& TurnStart =
		Coordinator.GetTurnStartSequenceState();
	Result.bTurnStartCompleted =
		Coordinator.WasTurnStartCompleted();
	Result.bTurnStartDrawSkipped =
		TurnStart.bDrawSkipped;
	Result.bTurnStartMPGenerated =
		TurnStart.bMPGenerated;
	Result.bTurnStartResourcesReset =
		TurnStart.bResourcesReset;
	Result.bTurnStartStatusesResolved =
		TurnStart.bStatusesResolved;
	Result.bTurnStartEffectsResolved =
		TurnStart.bEffectsResolved;
	Result.bPlayableDecisionReached =
		bPlayableDecisionReached;
	Result.bBlocked = false;
	Result.ResultCode = bPlayableDecisionReached
		&& Result.bMatchInitialized
		&& Result.bHeroSpawnBatchCommitted
		&& Result.bHeroSetupTriggersResolved
		&& Result.bOpeningHandsDrawn
		&& Result.bTurnStartCompleted
		&& Result.bTurnStartMPGenerated
		&& Result.bTurnStartResourcesReset
		&& Result.bTurnStartStatusesResolved
		&& Result.bTurnStartEffectsResolved
			? FString(TEXT("production_started"))
			: FString(TEXT("production_bundle_invalid"));
	Result.FirstPlayer = Coordinator.GetFirstPlayerId();
	Result.ActivePlayer =
		Coordinator.GetState().CurrentPlayer;
	Result.TurnNumber =
		Coordinator.GetState().TurnNumber;
	Result.Generation = Generation;
	Result.Revision = Revision;
	return Result;
}

FWBProductionStartupResult WBProductionStartupResult::Started(
	const FString& BundleDigest,
	const bool bMatchSpecPresent,
	const int32 Generation,
	const int32 Revision,
	const bool bPlayableDecisionReached)
{
	FWBProductionStartupResult Result;
	Result.bBundleLoaded = true;
	Result.BundleDigest = BundleDigest;
	Result.bMatchSpecPresent = bMatchSpecPresent;
	Result.bMatchInitialized = true;
	Result.bTurnStartCompleted = true;
	Result.bTurnStartMPGenerated = true;
	Result.bTurnStartResourcesReset = true;
	Result.bTurnStartStatusesResolved = true;
	Result.bTurnStartEffectsResolved = true;
	Result.bPlayableDecisionReached = bPlayableDecisionReached;
	Result.ResultCode = TEXT("production_started");
	Result.Generation = Generation;
	Result.Revision = Revision;
	return Result;
}

FString WBProductionStartupResult::Serialize(
	const FWBProductionStartupResult& Result)
{
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schema_version"), Result.SchemaVersion);
	Writer->WriteValue(TEXT("startup_mode"), Result.StartupMode);
	Writer->WriteValue(TEXT("bundle_loaded"), Result.bBundleLoaded);
	Writer->WriteValue(TEXT("bundle_digest"), Result.BundleDigest);
	Writer->WriteValue(TEXT("format_id"), Result.FormatId);
	Writer->WriteValue(TEXT("format_version"), Result.FormatVersion);
	Writer->WriteValue(TEXT("format_digest"), Result.FormatDigest);
	Writer->WriteValue(
		TEXT("game_start_addendum_id"),
		Result.GameStartAddendumId);
	Writer->WriteValue(
		TEXT("game_start_addendum_version"),
		Result.GameStartAddendumVersion);
	Writer->WriteValue(
		TEXT("game_start_addendum_digest"),
		Result.GameStartAddendumDigest);
	Writer->WriteValue(
		TEXT("match_spec_present"),
		Result.bMatchSpecPresent);
	Writer->WriteValue(
		TEXT("match_initialized"),
		Result.bMatchInitialized);
	Writer->WriteValue(
		TEXT("hero_spawn_batch_committed"),
		Result.bHeroSpawnBatchCommitted);
	Writer->WriteValue(
		TEXT("hero_setup_triggers_resolved"),
		Result.bHeroSetupTriggersResolved);
	Writer->WriteValue(
		TEXT("opening_hands_drawn"),
		Result.bOpeningHandsDrawn);
	Writer->WriteValue(
		TEXT("turn_start_completed"),
		Result.bTurnStartCompleted);
	Writer->WriteValue(
		TEXT("turn_start_draw_skipped"),
		Result.bTurnStartDrawSkipped);
	Writer->WriteValue(
		TEXT("turn_start_mp_generated"),
		Result.bTurnStartMPGenerated);
	Writer->WriteValue(
		TEXT("turn_start_resources_reset"),
		Result.bTurnStartResourcesReset);
	Writer->WriteValue(
		TEXT("turn_start_statuses_resolved"),
		Result.bTurnStartStatusesResolved);
	Writer->WriteValue(
		TEXT("turn_start_effects_resolved"),
		Result.bTurnStartEffectsResolved);
	Writer->WriteValue(
		TEXT("playable_decision_reached"),
		Result.bPlayableDecisionReached);
	Writer->WriteValue(TEXT("blocked"), Result.bBlocked);
	Writer->WriteValue(TEXT("result_code"), Result.ResultCode);
	Writer->WriteValue(TEXT("first_player"), Result.FirstPlayer);
	Writer->WriteValue(
		TEXT("active_player"),
		Result.ActivePlayer);
	Writer->WriteValue(
		TEXT("turn_number"),
		Result.TurnNumber);
	Writer->WriteValue(TEXT("generation"), Result.Generation);
	Writer->WriteValue(TEXT("revision"), Result.Revision);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Json;
}

bool WBProductionStartupResult::Write(
	const FWBProductionStartupResult& Result,
	const FString& ResultPath)
{
	const FString Path = ResultPath.IsEmpty()
		? GetDefaultResultPath()
		: ResultPath;
	return IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(Path),
			true)
		&& FFileHelper::SaveStringToFile(
			Serialize(Result),
			*Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

int32 WBProductionStartupResult::ExitCodeForResult(
	const FWBProductionStartupResult& Result)
{
	if (Result.ResultCode == TEXT("production_started"))
	{
		return ProductionStartedExitCode;
	}
	return Result.bBlocked
		? ProductionBlockedExitCode
		: ProductionInvalidExitCode;
}

bool WBProductionStartupResult::IsStartupProbeRequested(
	const TCHAR* CommandLine)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CommandLine != nullptr
		&& FParse::Param(
			CommandLine,
			TEXT("WandboundProductionStartupProbe"));
#endif
}

void WBProductionStartupResult::RequestProbeExit(
	const FWBProductionStartupResult& Result)
{
	if (!IsStartupProbeRequested(FCommandLine::Get()))
	{
		return;
	}
	if (GLog != nullptr)
	{
		GLog->FlushThreadedLogs();
		GLog->Flush();
	}
	FPlatformMisc::RequestExitWithStatus(
		true,
		ExitCodeForResult(Result),
		TEXT("WandboundProductionStartupProbe"));
}
