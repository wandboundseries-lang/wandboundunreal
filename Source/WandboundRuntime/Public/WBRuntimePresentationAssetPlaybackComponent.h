#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBRuntimePresentationAssetBinding.h"
#include "WBRuntimePresentationAssetPlaybackComponent.generated.h"

class ACameraActor;
class AWBRuntimeUnitPresentationActor;
class UAudioComponent;
class UNiagaraComponent;
class UWBRuntimePresentationAssetRegistry;

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationAssetPlaybackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bOk = true;

	UPROPERTY(BlueprintReadOnly)
	bool bUsedConfiguredAsset = false;

	UPROPERTY(BlueprintReadOnly)
	bool bUsedPrimitiveFallback = true;

	UPROPERTY(BlueprintReadOnly)
	FString Diagnostic;
};

UCLASS(ClassGroup = (Wandbound))
class WANDBOUNDRUNTIME_API UWBRuntimePresentationAssetPlaybackComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimePresentationAssetPlaybackComponent();

	void Configure(
		UWBRuntimePresentationAssetRegistry* InRegistry,
		ACameraActor* InBoardCamera);

	FWBRuntimePresentationAssetPlaybackResult BeginEventPlayback(
		const FWBRuntimePresentationEvent& Event,
		const FWBRuntimePresentationBindingResolution& Resolution,
		AWBRuntimeUnitPresentationActor* SourceActor,
		AWBRuntimeUnitPresentationActor* TargetActor,
		const FVector& SourceLocation,
		const FVector& TargetLocation,
		int32 MatchGeneration);

	void CompleteEventPlayback(int32 SequenceIndex);
	void StopAllPresentationAssets();

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation Assets")
	int32 GetActiveAudioComponentCount() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation Assets")
	int32 GetActiveVFXComponentCount() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation Assets")
	FString GetLastDiagnostic() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimePresentationAssetRegistry> Registry;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BoardCamera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ActiveAudioComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ActiveVFXComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AWBRuntimeUnitPresentationActor>> AnimatedActors;

	int32 ActiveSequenceIndex = INDEX_NONE;
	FString LastDiagnostic;

	void StopCurrentEventAssets();
};
