#include "WBRuntimePresentationAssetPlaybackComponent.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraShakeBase.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "WBRuntimePresentationAssetRegistry.h"
#include "WBRuntimeUnitPresentationActor.h"

UWBRuntimePresentationAssetPlaybackComponent::
UWBRuntimePresentationAssetPlaybackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimePresentationAssetPlaybackComponent::Configure(
	UWBRuntimePresentationAssetRegistry* InRegistry,
	ACameraActor* InBoardCamera)
{
	if (Registry != InRegistry)
	{
		StopAllPresentationAssets();
	}
	Registry = InRegistry;
	BoardCamera = InBoardCamera;
	LastDiagnostic.Reset();
}

FWBRuntimePresentationAssetPlaybackResult
UWBRuntimePresentationAssetPlaybackComponent::BeginEventPlayback(
	const FWBRuntimePresentationEvent& Event,
	const FWBRuntimePresentationBindingResolution& Resolution,
	AWBRuntimeUnitPresentationActor* SourceActor,
	AWBRuntimeUnitPresentationActor* TargetActor,
	const FVector& SourceLocation,
	const FVector& TargetLocation,
	const int32 MatchGeneration)
{
	StopCurrentEventAssets();
	ActiveSequenceIndex = Event.SequenceIndex;

	FWBRuntimePresentationAssetPlaybackResult Result;
	Result.bUsedPrimitiveFallback = Resolution.bUsePrimitiveFallback;
	if (Registry == nullptr)
	{
		return Result;
	}

	FWBRuntimePresentationAssetBinding Binding = Resolution.Binding;
	Binding.EventType = Event.Type;
	const bool bUseSourceActor =
		Binding.AttachmentPolicy == EWBRuntimePresentationAttachmentPolicy::AttachToSource;
	const bool bUseTargetActor =
		Binding.AttachmentPolicy == EWBRuntimePresentationAttachmentPolicy::AttachToTarget;
	AWBRuntimeUnitPresentationActor* AnimationActor =
		Event.Type == EWBRuntimePresentationEventType::AttackImpact
			|| Event.Type == EWBRuntimePresentationEventType::DamageApplied
			|| Event.Type == EWBRuntimePresentationEventType::HPChanged
			|| Event.Type == EWBRuntimePresentationEventType::ArmorChanged
			|| Event.Type == EWBRuntimePresentationEventType::TrapTriggered
			|| Event.Type == EWBRuntimePresentationEventType::UnitDefeated
			|| Event.Type == EWBRuntimePresentationEventType::HeroDefeated
			? TargetActor
			: SourceActor;

	if (AnimationActor != nullptr
		&& AnimationActor->PlayBoundAnimation(Binding, Registry, MatchGeneration))
	{
		AnimatedActors.AddUnique(AnimationActor);
		Result.bUsedConfiguredAsset = true;
		Result.bUsedPrimitiveFallback = false;
	}
	else if (AnimationActor != nullptr
		&& (!Binding.AnimationMontage.IsNull() || !Binding.AnimationSequence.IsNull()))
	{
		LastDiagnostic = AnimationActor->GetLastAssetDiagnostic();
	}

	const FVector AssetLocation =
		Binding.AttachmentPolicy == EWBRuntimePresentationAttachmentPolicy::SourceLocation
			|| Binding.AttachmentPolicy == EWBRuntimePresentationAttachmentPolicy::SourceTile
			|| bUseSourceActor
			? SourceLocation + Binding.LocationOffset
			: TargetLocation + Binding.LocationOffset;
	USceneComponent* AttachComponent = nullptr;
	if (bUseSourceActor && SourceActor != nullptr)
	{
		AttachComponent = SourceActor->GetAssetAttachmentComponent();
	}
	else if (bUseTargetActor && TargetActor != nullptr)
	{
		AttachComponent = TargetActor->GetAssetAttachmentComponent();
	}

	if (UNiagaraSystem* System = Resolution.bFoundConfiguredBinding
		? Registry->LoadNiagaraSystem(Binding.NiagaraSystem, MatchGeneration)
		: nullptr)
	{
		if (UWorld* World = GetWorld())
		{
			UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				System,
				AssetLocation,
				Binding.RotationOffset,
				Binding.Scale,
				true,
				true);
			if (Component != nullptr)
			{
				if (AttachComponent != nullptr)
				{
					Component->AttachToComponent(
						AttachComponent,
						FAttachmentTransformRules::KeepWorldTransform,
						Binding.SocketName);
				}
				ActiveVFXComponents.Add(Component);
				Result.bUsedConfiguredAsset = true;
				Result.bUsedPrimitiveFallback = false;
			}
		}
	}

	if (USoundBase* Sound = Resolution.bFoundConfiguredBinding
		? Registry->LoadSound(Binding.Sound, MatchGeneration)
		: nullptr)
	{
		if (UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(
			this,
			Sound,
			AssetLocation,
			Binding.RotationOffset,
			Binding.VolumeMultiplier,
			Binding.PitchMultiplier))
		{
			if (AttachComponent != nullptr)
			{
				Audio->AttachToComponent(
					AttachComponent,
					FAttachmentTransformRules::KeepWorldTransform,
					Binding.SocketName);
			}
			ActiveAudioComponents.Add(Audio);
			Result.bUsedConfiguredAsset = true;
			Result.bUsedPrimitiveFallback = false;
		}
	}

	if (Resolution.bFoundConfiguredBinding)
	{
		if (const TSubclassOf<UCameraShakeBase> CameraShake =
			Registry->LoadCameraShake(Binding.CameraShake, MatchGeneration))
		{
			if (UWorld* World = GetWorld())
			{
				if (APlayerController* Controller = World->GetFirstPlayerController())
				{
					if (Controller->PlayerCameraManager != nullptr)
					{
						Controller->PlayerCameraManager->StartCameraShake(
							CameraShake,
							FMath::Max(Binding.Scale.X, 0.0f));
						Result.bUsedConfiguredAsset = true;
						Result.bUsedPrimitiveFallback = false;
					}
				}
			}
		}
	}

	if (!Registry->GetLastDiagnostic().IsEmpty())
	{
		LastDiagnostic = Registry->GetLastDiagnostic();
	}
	Result.Diagnostic = LastDiagnostic;
	return Result;
}

void UWBRuntimePresentationAssetPlaybackComponent::CompleteEventPlayback(
	const int32 SequenceIndex)
{
	if (ActiveSequenceIndex != INDEX_NONE && SequenceIndex != ActiveSequenceIndex)
	{
		return;
	}
	StopCurrentEventAssets();
}

void UWBRuntimePresentationAssetPlaybackComponent::StopAllPresentationAssets()
{
	StopCurrentEventAssets();
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (Controller->PlayerCameraManager != nullptr)
			{
				Controller->PlayerCameraManager->StopAllCameraShakes(true);
			}
			if (BoardCamera != nullptr)
			{
				Controller->SetViewTarget(BoardCamera);
			}
		}
	}
}

int32 UWBRuntimePresentationAssetPlaybackComponent::GetActiveAudioComponentCount() const
{
	return ActiveAudioComponents.Num();
}

int32 UWBRuntimePresentationAssetPlaybackComponent::GetActiveVFXComponentCount() const
{
	return ActiveVFXComponents.Num();
}

FString UWBRuntimePresentationAssetPlaybackComponent::GetLastDiagnostic() const
{
	return LastDiagnostic;
}

void UWBRuntimePresentationAssetPlaybackComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopAllPresentationAssets();
	Registry = nullptr;
	BoardCamera = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UWBRuntimePresentationAssetPlaybackComponent::StopCurrentEventAssets()
{
	for (UAudioComponent* Component : ActiveAudioComponents)
	{
		if (IsValid(Component))
		{
			Component->Stop();
			Component->DestroyComponent();
		}
	}
	ActiveAudioComponents.Reset();

	for (UNiagaraComponent* Component : ActiveVFXComponents)
	{
		if (IsValid(Component))
		{
			Component->DeactivateImmediate();
			Component->DestroyComponent();
		}
	}
	ActiveVFXComponents.Reset();

	for (AWBRuntimeUnitPresentationActor* Actor : AnimatedActors)
	{
		if (IsValid(Actor))
		{
			Actor->StopBoundAnimation();
		}
	}
	AnimatedActors.Reset();
	ActiveSequenceIndex = INDEX_NONE;
}
