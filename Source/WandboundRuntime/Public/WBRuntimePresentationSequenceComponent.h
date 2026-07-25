#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBRuntimePresentationEvent.h"
#include "WBRuntimePresentationSequenceComponent.generated.h"

UENUM(BlueprintType)
enum class EWBRuntimePresentationPlaybackState : uint8
{
	Idle,
	Preparing,
	Playing,
	Waiting,
	Skipping,
	Completed,
	Cancelled,
	Failed
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationSequenceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FWBRuntimePresentationEventDelegate,
	FWBRuntimePresentationEvent,
	Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWBRuntimePresentationSequenceDelegate);

UCLASS(ClassGroup = (Wandbound))
class WANDBOUNDRUNTIME_API UWBRuntimePresentationSequenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimePresentationSequenceComponent();

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationEventDelegate OnPresentationEventStarted;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationEventDelegate OnPresentationEventCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceDelegate OnPresentationSequenceCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceDelegate OnPresentationSequenceCancelled;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceResult EnqueueEvents(
		const TArray<FWBRuntimePresentationEvent>& Events,
		int32 MatchGeneration,
		int32 DecisionRevision);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceResult BeginSequence();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceResult AdvanceSequence();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceResult SkipCurrentEvent();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	FWBRuntimePresentationSequenceResult SkipAll();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	void CancelSequence();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Presentation")
	void SetPlaybackSpeed(float InPlaybackSpeed);

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	float GetPlaybackSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	bool IsSequenceActive() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	FWBRuntimePresentationEvent GetCurrentEvent() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	int32 GetPendingEventCount() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	EWBRuntimePresentationPlaybackState GetPlaybackState() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	int32 GetSourceMatchGeneration() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	int32 GetSourceDecisionRevision() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Presentation")
	FString GetFailureReason() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FWBRuntimePresentationEvent> QueuedEvents;

	UPROPERTY(Transient)
	FWBRuntimePresentationEvent CurrentEvent;

	EWBRuntimePresentationPlaybackState PlaybackState = EWBRuntimePresentationPlaybackState::Idle;
	int32 CurrentEventIndex = INDEX_NONE;
	int32 SourceMatchGeneration = INDEX_NONE;
	int32 SourceDecisionRevision = INDEX_NONE;
	float PlaybackSpeed = 1.0f;
	FString FailureReason;
	FTimerHandle AdvanceTimer;

	FWBRuntimePresentationSequenceResult MakeResult(bool bOk, const FString& Reason) const;
	void StartCurrentEvent();
	void CompleteSequence();
	void ClearAdvanceTimer();

	UFUNCTION()
	void HandleAdvanceTimer();
};
