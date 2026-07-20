#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WBRuntimeMatchBootstrapActor.h"
#include "WBRuntimeLocalPlayGameMode.generated.h"

class AWBRuntimePlayerController;

UCLASS()
class WANDBOUNDRUNTIME_API AWBRuntimeLocalPlayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWBRuntimeLocalPlayGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	TSubclassOf<AWBRuntimeMatchBootstrapActor> BootstrapActorClass;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Local Play", meta = (DevelopmentOnly))
	FWBRuntimeLocalPlayResult StartLocalPlayForController(AWBRuntimePlayerController* LocalController);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Local Play", meta = (DevelopmentOnly))
	FWBRuntimeLocalPlayResult RestartDevelopmentMatch();

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	AWBRuntimeMatchBootstrapActor* GetBootstrapActor() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	FString GetStartupFailureReason() const;

	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AWBRuntimeMatchBootstrapActor> BootstrapActor;

	bool bOwnsBootstrapActor = false;
	FString StartupFailureReason;

	AWBRuntimeMatchBootstrapActor* EnsureSingleBootstrap();
};
