#include "WBRuntimeLocalPlaySmoke.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectIterator.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeLocalPlayGameMode.h"
#include "WBRuntimeMatchBootstrapActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeMatchHUDWidget.h"
#include "WBRuntimePlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBRuntimeLocalPlaySmoke, Log, All);

namespace
{
constexpr uint8 SmokeSuccessExitCode = 0;
constexpr uint8 SmokeFailureExitCode = 10;

FString BootstrapStateName(const EWBRuntimeLocalPlayState State)
{
	switch (State)
	{
	case EWBRuntimeLocalPlayState::Uninitialized: return TEXT("uninitialized");
	case EWBRuntimeLocalPlayState::Starting: return TEXT("starting");
	case EWBRuntimeLocalPlayState::Ready: return TEXT("ready");
	case EWBRuntimeLocalPlayState::Failed: return TEXT("failed");
	case EWBRuntimeLocalPlayState::ShuttingDown: return TEXT("shutting_down");
	default: return TEXT("unknown");
	}
}

template <typename TActor>
int32 CountActors(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<TActor> It(World); It; ++It) ++Count;
	return Count;
}
}

UWBRuntimeLocalPlaySmokeCoordinator::UWBRuntimeLocalPlaySmokeCoordinator()
{
	ExitRequest = [](const uint8 ExitCode)
	{
		FPlatformMisc::RequestExitWithStatus(false, ExitCode, TEXT("WandboundLocalPlayPackagedSmoke"));
	};
}

bool UWBRuntimeLocalPlaySmokeCoordinator::IsSmokeRequested(const TCHAR* CommandLine)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CommandLine != nullptr && FParse::Param(CommandLine, TEXT("WandboundLocalPlaySmoke"));
#endif
}

FString UWBRuntimeLocalPlaySmokeCoordinator::GetDefaultResultPath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("AutomationReports/WandboundLocalPlayPackagedSmoke/result.json"));
}

FString UWBRuntimeLocalPlaySmokeCoordinator::SerializeResult(const FWBRuntimeLocalPlaySmokeResult& Result)
{
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("success"), Result.bSuccess);
	Writer->WriteValue(TEXT("failure_reason"), Result.FailureReason);
	Writer->WriteValue(TEXT("map_name"), Result.MapName);
	Writer->WriteValue(TEXT("game_mode_class"), Result.GameModeClass);
	Writer->WriteValue(TEXT("bootstrap_state"), Result.BootstrapState);
	Writer->WriteValue(TEXT("match_generation"), Result.MatchGeneration);
	Writer->WriteValue(TEXT("presentation_revision"), Result.PresentationRevision);
	Writer->WriteValue(TEXT("tile_count"), Result.TileCount);
	Writer->WriteValue(TEXT("visible_unit_count"), Result.VisibleUnitCount);
	Writer->WriteValue(TEXT("visible_hero_count"), Result.VisibleHeroCount);
	Writer->WriteValue(TEXT("concealed_marker_count"), Result.ConcealedMarkerCount);
	Writer->WriteValue(TEXT("own_hand_count"), Result.OwnHandCount);
	Writer->WriteValue(TEXT("legal_action_count"), Result.LegalActionCount);
	Writer->WriteValue(TEXT("action_submitted"), Result.bActionSubmitted);
	Writer->WriteValue(TEXT("end_turn_submitted"), Result.bEndTurnSubmitted);
	Writer->WriteValue(TEXT("game_over"), Result.bGameOver);
	Writer->WriteValue(TEXT("winner_player_id"), Result.WinnerPlayerId);
	Writer->WriteValue(TEXT("process_exit_code"), Result.ProcessExitCode);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Json;
}

bool UWBRuntimeLocalPlaySmokeCoordinator::RunSmoke(
	AWBRuntimeLocalPlayGameMode* GameMode,
	AWBRuntimeMatchBootstrapActor* Bootstrap,
	const FString& ExpectedMapPackage)
{
	if (bStarted) return false;
	bStarted = true;
	LastResult = FWBRuntimeLocalPlaySmokeResult();
	UWorld* World = GameMode != nullptr ? GameMode->GetWorld() : nullptr;
	LastResult.MapName = World != nullptr ? World->GetOutermost()->GetName() : FString();
	LastResult.GameModeClass = GameMode != nullptr ? GameMode->GetClass()->GetPathName() : FString();
	LastResult.BootstrapState = Bootstrap != nullptr ? BootstrapStateName(Bootstrap->GetLocalPlayState()) : TEXT("missing");

	if (World == nullptr) return Finish(false, TEXT("smoke_world_missing"));
	if (LastResult.MapName != ExpectedMapPackage) return Finish(false, TEXT("smoke_map_mismatch"));
	if (GameMode->GetClass() != AWBRuntimeLocalPlayGameMode::StaticClass()) return Finish(false, TEXT("smoke_game_mode_mismatch"));
	if (Bootstrap == nullptr || !Bootstrap->IsLocalPlayReady()) return Finish(false, TEXT("smoke_bootstrap_not_ready"));
	if (CountActors<AWBRuntimeMatchBootstrapActor>(World) != 1) return Finish(false, TEXT("smoke_bootstrap_count_mismatch"));
	if (CountActors<AWBBoardViewActor>(World) != 1) return Finish(false, TEXT("smoke_board_count_mismatch"));
	if (CountActors<ACameraActor>(World) != 1) return Finish(false, TEXT("smoke_camera_count_mismatch"));

	int32 HostCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UWBRuntimeMatchHostComponent*> Hosts;
		It->GetComponents(Hosts);
		HostCount += Hosts.Num();
	}
	if (HostCount != 1) return Finish(false, TEXT("smoke_host_count_mismatch"));

	int32 HUDCount = 0;
	for (TObjectIterator<UWBRuntimeMatchHUDWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World) ++HUDCount;
	}
	if (HUDCount != 1) return Finish(false, TEXT("smoke_hud_count_mismatch"));

	UWBRuntimeMatchHostComponent* Host = Bootstrap->GetMatchHost();
	AWBBoardViewActor* Board = Bootstrap->GetBoardActor();
	AWBRuntimePlayerController* Controller = Bootstrap->GetRuntimeController();
	if (Host == nullptr || !Host->IsMatchInitialized()) return Finish(false, TEXT("smoke_host_not_initialized"));
	if (Board == nullptr) return Finish(false, TEXT("smoke_board_missing"));
	if (Controller == nullptr || !Controller->IsRuntimeInteractionBound()) return Finish(false, TEXT("smoke_controller_not_bound"));

	RefreshPublicResult(Bootstrap);
	const FWBRuntimeMatchPresentation InitialPresentation = Host->GetCurrentPresentation();
	if (LastResult.TileCount != 81) return Finish(false, TEXT("smoke_tile_count_mismatch"));
	if (LastResult.VisibleUnitCount != 2 || LastResult.VisibleHeroCount != 2) return Finish(false, TEXT("smoke_initial_units_mismatch"));
	if (LastResult.ConcealedMarkerCount != 8) return Finish(false, TEXT("smoke_marker_count_mismatch"));
	if (LastResult.OwnHandCount != 6) return Finish(false, TEXT("smoke_hand_count_mismatch"));
	if (LastResult.LegalActionCount <= 0) return Finish(false, TEXT("smoke_legal_actions_missing"));
	if (InitialPresentation.bGameOver || InitialPresentation.Phase != FName(TEXT("normal_turn"))) return Finish(false, TEXT("smoke_initial_phase_not_playable"));
	if (Board->GetAppliedPresentationRevision() != InitialPresentation.PresentationRevision) return Finish(false, TEXT("smoke_board_revision_mismatch"));

	const TArray<FWBRuntimeLegalActionPresentation> InitialActions = Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* ActionToSubmit = InitialActions.FindByPredicate([](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.Family != EWBRuntimeMatchActionFamily::EndTurn
			&& Action.Family != EWBRuntimeMatchActionFamily::Pass
			&& Action.Family != EWBRuntimeMatchActionFamily::PassResponse;
	});
	if (ActionToSubmit == nullptr) return Finish(false, TEXT("smoke_non_turn_action_missing"));
	const FWBRuntimeMatchCommandResult ActionResult = Host->SubmitLegalActionAtRevision(
		ActionToSubmit->ActionId,
		ActionToSubmit->MatchGeneration,
		ActionToSubmit->DecisionRevision);
	if (!ActionResult.bOk) return Finish(false, FString::Printf(TEXT("smoke_action_failed:%s"), *ActionResult.Reason));
	LastResult.bActionSubmitted = true;
	const int32 RevisionAfterAction = Host->GetCurrentPresentation().PresentationRevision;
	if (RevisionAfterAction <= InitialPresentation.PresentationRevision) return Finish(false, TEXT("smoke_action_revision_not_advanced"));
	if (Board->GetAppliedPresentationRevision() != RevisionAfterAction) return Finish(false, TEXT("smoke_board_revision_after_action_mismatch"));

	const FWBRuntimeMatchCommandResult EndTurnResult = Host->EndTurn();
	if (!EndTurnResult.bOk) return Finish(false, FString::Printf(TEXT("smoke_end_turn_failed:%s"), *EndTurnResult.Reason));
	LastResult.bEndTurnSubmitted = true;
	RefreshPublicResult(Bootstrap);
	if (!LastResult.bGameOver && LastResult.PresentationRevision <= RevisionAfterAction) return Finish(false, TEXT("smoke_end_turn_revision_not_advanced"));
	if (Board->GetAppliedPresentationRevision() != LastResult.PresentationRevision) return Finish(false, TEXT("smoke_final_board_revision_mismatch"));
	return Finish(true, FString());
}

bool UWBRuntimeLocalPlaySmokeCoordinator::HasStarted() const
{
	return bStarted;
}

FWBRuntimeLocalPlaySmokeResult UWBRuntimeLocalPlaySmokeCoordinator::GetLastResult() const
{
	return LastResult;
}

#if WITH_DEV_AUTOMATION_TESTS
void UWBRuntimeLocalPlaySmokeCoordinator::SetExitRequestForTesting(TFunction<void(uint8)> InExitRequest)
{
	ExitRequest = MoveTemp(InExitRequest);
}

void UWBRuntimeLocalPlaySmokeCoordinator::SetResultPathForTesting(const FString& InResultPath)
{
	ResultPathOverride = InResultPath;
}
#endif

bool UWBRuntimeLocalPlaySmokeCoordinator::Finish(const bool bSuccess, const FString& FailureReason)
{
	LastResult.bSuccess = bSuccess;
	LastResult.FailureReason = FailureReason;
	LastResult.ProcessExitCode = bSuccess ? SmokeSuccessExitCode : SmokeFailureExitCode;
	if (!WriteResult())
	{
		LastResult.bSuccess = false;
		LastResult.FailureReason = TEXT("smoke_result_write_failed");
		LastResult.ProcessExitCode = SmokeFailureExitCode;
	}
	if (LastResult.bSuccess)
	{
		UE_LOG(LogWBRuntimeLocalPlaySmoke, Display, TEXT("Wandbound packaged local-play smoke passed"));
	}
	else
	{
		UE_LOG(LogWBRuntimeLocalPlaySmoke, Error, TEXT("Wandbound packaged local-play smoke failed: %s"), *LastResult.FailureReason);
	}
	if (GLog != nullptr)
	{
		GLog->FlushThreadedLogs();
		GLog->Flush();
	}
	if (ExitRequest) ExitRequest(static_cast<uint8>(LastResult.ProcessExitCode));
	return LastResult.bSuccess;
}

bool UWBRuntimeLocalPlaySmokeCoordinator::WriteResult() const
{
	const FString ResultPath = ResultPathOverride.IsEmpty() ? GetDefaultResultPath() : ResultPathOverride;
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true)) return false;
	return FFileHelper::SaveStringToFile(
		SerializeResult(LastResult),
		*ResultPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UWBRuntimeLocalPlaySmokeCoordinator::RefreshPublicResult(AWBRuntimeMatchBootstrapActor* Bootstrap)
{
	UWBRuntimeMatchHostComponent* Host = Bootstrap != nullptr ? Bootstrap->GetMatchHost() : nullptr;
	if (Host == nullptr) return;
	const FWBRuntimeMatchPresentation Presentation = Host->GetCurrentPresentation();
	const TArray<FWBRuntimeUnitPresentation> Units = Host->GetCurrentUnits();
	const TArray<FWBRuntimeBoardTilePresentation> Tiles = Host->GetCurrentTiles();
	LastResult.BootstrapState = BootstrapStateName(Bootstrap->GetLocalPlayState());
	LastResult.MatchGeneration = Presentation.MatchGeneration;
	LastResult.PresentationRevision = Presentation.PresentationRevision;
	LastResult.TileCount = Tiles.Num();
	LastResult.VisibleUnitCount = Units.Num();
	LastResult.VisibleHeroCount = Units.FilterByPredicate([](const FWBRuntimeUnitPresentation& Unit) { return Unit.bHero; }).Num();
	LastResult.ConcealedMarkerCount = Tiles.FilterByPredicate([](const FWBRuntimeBoardTilePresentation& Tile) { return Tile.bHasConcealedMarker; }).Num();
	LastResult.OwnHandCount = Host->GetCurrentHandCards().Num();
	LastResult.LegalActionCount = Host->GetCurrentLegalActions().Num();
	LastResult.bGameOver = Presentation.bGameOver;
	LastResult.WinnerPlayerId = Presentation.WinnerPlayerId;
}
