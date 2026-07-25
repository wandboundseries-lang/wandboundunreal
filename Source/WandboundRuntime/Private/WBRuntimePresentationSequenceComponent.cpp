#include "WBRuntimePresentationSequenceComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"

UWBRuntimePresentationSequenceComponent::UWBRuntimePresentationSequenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FWBRuntimePresentationSequenceResult UWBRuntimePresentationSequenceComponent::EnqueueEvents(
	const TArray<FWBRuntimePresentationEvent>& Events,
	const int32 MatchGeneration,
	const int32 DecisionRevision)
{
	if (IsSequenceActive())
	{
		return MakeResult(false, TEXT("presentation_sequence_active"));
	}
	if (SourceMatchGeneration != INDEX_NONE && MatchGeneration < SourceMatchGeneration)
	{
		return MakeResult(false, TEXT("stale_match_generation"));
	}

	ClearAdvanceTimer();
	QueuedEvents = Events;
	CurrentEvent = FWBRuntimePresentationEvent();
	CurrentEventIndex = INDEX_NONE;
	SourceMatchGeneration = MatchGeneration;
	SourceDecisionRevision = DecisionRevision;
	FailureReason.Reset();
	PlaybackState = EWBRuntimePresentationPlaybackState::Idle;
	return MakeResult(true, TEXT("presentation_events_enqueued"));
}

FWBRuntimePresentationSequenceResult UWBRuntimePresentationSequenceComponent::BeginSequence()
{
	if (IsSequenceActive())
	{
		return MakeResult(false, TEXT("presentation_sequence_active"));
	}
	if (PlaybackState == EWBRuntimePresentationPlaybackState::Completed)
	{
		return MakeResult(false, TEXT("presentation_sequence_already_completed"));
	}

	PlaybackState = EWBRuntimePresentationPlaybackState::Preparing;
	CurrentEventIndex = 0;
	if (QueuedEvents.IsEmpty())
	{
		CompleteSequence();
		return MakeResult(true, TEXT("presentation_sequence_completed"));
	}

	StartCurrentEvent();
	return MakeResult(true, TEXT("presentation_sequence_started"));
}

FWBRuntimePresentationSequenceResult UWBRuntimePresentationSequenceComponent::AdvanceSequence()
{
	if (!IsSequenceActive() || !QueuedEvents.IsValidIndex(CurrentEventIndex))
	{
		return MakeResult(false, TEXT("presentation_sequence_not_active"));
	}

	ClearAdvanceTimer();
	OnPresentationEventCompleted.Broadcast(CurrentEvent);
	++CurrentEventIndex;
	if (!QueuedEvents.IsValidIndex(CurrentEventIndex))
	{
		CompleteSequence();
		return MakeResult(true, TEXT("presentation_sequence_completed"));
	}

	StartCurrentEvent();
	return MakeResult(true, TEXT("presentation_sequence_advanced"));
}

FWBRuntimePresentationSequenceResult UWBRuntimePresentationSequenceComponent::SkipCurrentEvent()
{
	if (!IsSequenceActive())
	{
		return MakeResult(false, TEXT("presentation_sequence_not_active"));
	}
	PlaybackState = EWBRuntimePresentationPlaybackState::Skipping;
	return AdvanceSequence();
}

FWBRuntimePresentationSequenceResult UWBRuntimePresentationSequenceComponent::SkipAll()
{
	if (!IsSequenceActive())
	{
		return MakeResult(false, TEXT("presentation_sequence_not_active"));
	}

	ClearAdvanceTimer();
	PlaybackState = EWBRuntimePresentationPlaybackState::Skipping;
	if (QueuedEvents.IsValidIndex(CurrentEventIndex))
	{
		OnPresentationEventCompleted.Broadcast(CurrentEvent);
	}
	CurrentEventIndex = QueuedEvents.Num();
	CompleteSequence();
	return MakeResult(true, TEXT("presentation_sequence_skipped"));
}

void UWBRuntimePresentationSequenceComponent::CancelSequence()
{
	ClearAdvanceTimer();
	const bool bWasActive = IsSequenceActive();
	QueuedEvents.Reset();
	CurrentEvent = FWBRuntimePresentationEvent();
	CurrentEventIndex = INDEX_NONE;
	PlaybackState = EWBRuntimePresentationPlaybackState::Cancelled;
	if (bWasActive)
	{
		OnPresentationSequenceCancelled.Broadcast();
	}
}

void UWBRuntimePresentationSequenceComponent::SetPlaybackSpeed(const float InPlaybackSpeed)
{
	PlaybackSpeed = InPlaybackSpeed <= 0.0f ? 0.0f : FMath::Clamp(InPlaybackSpeed, 0.1f, 8.0f);
	if (PlaybackSpeed <= 0.0f && IsSequenceActive())
	{
		SkipAll();
	}
}

float UWBRuntimePresentationSequenceComponent::GetPlaybackSpeed() const
{
	return PlaybackSpeed;
}

bool UWBRuntimePresentationSequenceComponent::IsSequenceActive() const
{
	return PlaybackState == EWBRuntimePresentationPlaybackState::Preparing
		|| PlaybackState == EWBRuntimePresentationPlaybackState::Playing
		|| PlaybackState == EWBRuntimePresentationPlaybackState::Waiting
		|| PlaybackState == EWBRuntimePresentationPlaybackState::Skipping;
}

FWBRuntimePresentationEvent UWBRuntimePresentationSequenceComponent::GetCurrentEvent() const
{
	return CurrentEvent;
}

int32 UWBRuntimePresentationSequenceComponent::GetPendingEventCount() const
{
	if (PlaybackState == EWBRuntimePresentationPlaybackState::Completed
		|| PlaybackState == EWBRuntimePresentationPlaybackState::Cancelled
		|| PlaybackState == EWBRuntimePresentationPlaybackState::Failed)
	{
		return 0;
	}
	if (QueuedEvents.IsEmpty())
	{
		return 0;
	}
	if (!IsSequenceActive())
	{
		return QueuedEvents.Num();
	}
	return FMath::Max(QueuedEvents.Num() - CurrentEventIndex - 1, 0);
}

EWBRuntimePresentationPlaybackState UWBRuntimePresentationSequenceComponent::GetPlaybackState() const
{
	return PlaybackState;
}

int32 UWBRuntimePresentationSequenceComponent::GetSourceMatchGeneration() const
{
	return SourceMatchGeneration;
}

int32 UWBRuntimePresentationSequenceComponent::GetSourceDecisionRevision() const
{
	return SourceDecisionRevision;
}

FString UWBRuntimePresentationSequenceComponent::GetFailureReason() const
{
	return FailureReason;
}

void UWBRuntimePresentationSequenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelSequence();
	Super::EndPlay(EndPlayReason);
}

FWBRuntimePresentationSequenceResult UWBRuntimePresentationSequenceComponent::MakeResult(
	const bool bOk,
	const FString& Reason) const
{
	FWBRuntimePresentationSequenceResult Result;
	Result.bOk = bOk;
	Result.Reason = Reason;
	return Result;
}

void UWBRuntimePresentationSequenceComponent::StartCurrentEvent()
{
	if (!QueuedEvents.IsValidIndex(CurrentEventIndex))
	{
		FailureReason = TEXT("presentation_event_index_invalid");
		PlaybackState = EWBRuntimePresentationPlaybackState::Failed;
		OnPresentationSequenceCancelled.Broadcast();
		return;
	}

	CurrentEvent = QueuedEvents[CurrentEventIndex];
	PlaybackState = EWBRuntimePresentationPlaybackState::Playing;
	OnPresentationEventStarted.Broadcast(CurrentEvent);

	const float EffectiveDuration = PlaybackSpeed <= 0.0f
		? 0.0f
		: CurrentEvent.SuggestedDurationSeconds / PlaybackSpeed;
	if (EffectiveDuration <= KINDA_SMALL_NUMBER)
	{
		AdvanceSequence();
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		PlaybackState = EWBRuntimePresentationPlaybackState::Waiting;
		return;
	}

	PlaybackState = EWBRuntimePresentationPlaybackState::Waiting;
	World->GetTimerManager().SetTimer(
		AdvanceTimer,
		this,
		&UWBRuntimePresentationSequenceComponent::HandleAdvanceTimer,
		EffectiveDuration,
		false);
}

void UWBRuntimePresentationSequenceComponent::CompleteSequence()
{
	ClearAdvanceTimer();
	CurrentEvent = FWBRuntimePresentationEvent();
	PlaybackState = EWBRuntimePresentationPlaybackState::Completed;
	OnPresentationSequenceCompleted.Broadcast();
}

void UWBRuntimePresentationSequenceComponent::ClearAdvanceTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AdvanceTimer);
	}
	AdvanceTimer.Invalidate();
}

void UWBRuntimePresentationSequenceComponent::HandleAdvanceTimer()
{
	AdvanceSequence();
}
