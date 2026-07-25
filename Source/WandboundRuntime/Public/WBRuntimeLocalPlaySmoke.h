#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WBRuntimeLocalPlaySmoke.generated.h"

class AWBRuntimeLocalPlayGameMode;
class AWBRuntimeMatchBootstrapActor;

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeLocalPlaySmokeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly)
	FString MapName;

	UPROPERTY(BlueprintReadOnly)
	FString GameModeClass;

	UPROPERTY(BlueprintReadOnly)
	FString BootstrapState;

	UPROPERTY(BlueprintReadOnly)
	int32 MatchGeneration = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PresentationRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TileCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 VisibleUnitCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 VisibleHeroCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ConcealedMarkerCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OwnHandCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 LegalActionCount = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bActionSubmitted = false;

	UPROPERTY(BlueprintReadOnly)
	bool bEndTurnSubmitted = false;

	UPROPERTY(BlueprintReadOnly)
	bool bGameOver = false;

	UPROPERTY(BlueprintReadOnly)
	int32 WinnerPlayerId = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 ProcessExitCode = 1;
};

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeLocalPlaySmokeCoordinator : public UObject
{
	GENERATED_BODY()

public:
	UWBRuntimeLocalPlaySmokeCoordinator();

	static bool IsSmokeRequested(const TCHAR* CommandLine);
	static FString GetDefaultResultPath();
	static FString SerializeResult(const FWBRuntimeLocalPlaySmokeResult& Result);

	bool RunSmoke(
		AWBRuntimeLocalPlayGameMode* GameMode,
		AWBRuntimeMatchBootstrapActor* Bootstrap,
		const FString& ExpectedMapPackage = TEXT("/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev"));

	bool HasStarted() const;
	FWBRuntimeLocalPlaySmokeResult GetLastResult() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetExitRequestForTesting(TFunction<void(uint8)> InExitRequest);
	void SetResultPathForTesting(const FString& InResultPath);
#endif

private:
	bool bStarted = false;
	FWBRuntimeLocalPlaySmokeResult LastResult;
	FString ResultPathOverride;
	TFunction<void(uint8)> ExitRequest;

	bool Finish(bool bSuccess, const FString& FailureReason);
	bool WriteResult() const;
	void RefreshPublicResult(AWBRuntimeMatchBootstrapActor* Bootstrap);
};
