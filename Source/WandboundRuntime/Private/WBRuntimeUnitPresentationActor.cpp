#include "WBRuntimeUnitPresentationActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "WBRuntimePresentationAssetRegistry.h"

AWBRuntimeUnitPresentationActor::AWBRuntimeUnitPresentationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(SceneRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VisualMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	VisualMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	VisualMesh->SetCanEverAffectNavigation(false);
	SkeletalVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalVisual"));
	SkeletalVisual->SetupAttachment(SceneRoot);
	SkeletalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	SkeletalVisual->SetCanEverAffectNavigation(false);
	SkeletalVisual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CylinderMesh.Object);
	}
	VisualMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.75f));
}

void AWBRuntimeUnitPresentationActor::ApplyPresentation(
	const FWBRuntimeUnitPresentation& InPresentation,
	const FVector& WorldLocation)
{
	Presentation = InPresentation;
	SetActorLocation(WorldLocation);
	SetVisualScale(ActiveVisualScale);
}

void AWBRuntimeUnitPresentationActor::BeginPresentationEvent(
	const FWBRuntimePresentationEvent& Event,
	const FVector& SourceWorldLocation,
	const FVector& DestinationWorldLocation,
	const float DurationSeconds)
{
	ActivePresentationType = Event.Type;
	PresentationStartLocation = SourceWorldLocation;
	PresentationEndLocation = DestinationWorldLocation;
	PresentationElapsedSeconds = 0.0f;
	PresentationDurationSeconds = FMath::Max(DurationSeconds, KINDA_SMALL_NUMBER);
	bPresentationActive = true;
	SetActorLocation(SourceWorldLocation);

	if (Event.Type == EWBRuntimePresentationEventType::UnitSummoned
		|| Event.Type == EWBRuntimePresentationEventType::NPCSpawned)
	{
		SetVisualScale(FVector::ZeroVector);
	}
	else
	{
		SetVisualScale(ActiveVisualScale);
	}
	SetActorTickEnabled(true);
}

void AWBRuntimeUnitPresentationActor::CompletePresentationEvent(
	const FWBRuntimePresentationEvent& Event,
	const FVector& FinalWorldLocation)
{
	bPresentationActive = false;
	SetActorTickEnabled(false);
	SetActorLocation(FinalWorldLocation);
	SetVisualScale(
		Event.Type == EWBRuntimePresentationEventType::UnitDefeated
			|| Event.Type == EWBRuntimePresentationEventType::HeroDefeated
			? FVector::ZeroVector
			: ActiveVisualScale);
}

void AWBRuntimeUnitPresentationActor::SnapToWorldLocation(const FVector& WorldLocation)
{
	bPresentationActive = false;
	SetActorTickEnabled(false);
	SetActorLocation(WorldLocation);
	StopBoundAnimation();
	SetVisualScale(ActiveVisualScale);
}

void AWBRuntimeUnitPresentationActor::ApplyAssetProfile(
	const FWBRuntimeUnitProfileResolution& Resolution,
	UWBRuntimePresentationAssetRegistry* Registry,
	const int32 MatchGeneration)
{
	StopBoundAnimation();
	LastAssetDiagnostic.Reset();
	bHasAssetProfile = Resolution.bFoundConfiguredProfile;
	ActiveAssetProfile = Resolution.Profile;
	bUsingSkeletalPresentation = false;
	bUsingPrimitiveFallback = true;
	ActiveVisualScale = PresentationBaseScale;

	USkeletalMesh* SkeletalMesh = nullptr;
	UStaticMesh* StaticMesh = nullptr;
	UMaterialInterface* Material = nullptr;
	if (Resolution.bFoundConfiguredProfile && Registry != nullptr)
	{
		SkeletalMesh = Registry->LoadSkeletalMesh(
			Resolution.Profile.SkeletalMesh,
			MatchGeneration);
		if (SkeletalMesh == nullptr)
		{
			StaticMesh = Registry->LoadStaticMesh(
				Resolution.Profile.StaticMesh,
				MatchGeneration);
		}
		Material = Registry->LoadMaterial(
			Resolution.Profile.MaterialOverride,
			MatchGeneration);
		LastAssetDiagnostic = Registry->GetLastDiagnostic();
	}

	if (SkeletalMesh != nullptr)
	{
		SkeletalVisual->SetSkeletalMeshAsset(SkeletalMesh);
		SkeletalVisual->SetVisibility(true);
		SkeletalVisual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SkeletalVisual->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		VisualMesh->SetVisibility(false);
		VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bUsingSkeletalPresentation = true;
		bUsingPrimitiveFallback = false;
		ActiveVisualScale = Resolution.Profile.VisualScale;
		SkeletalVisual->SetRelativeLocation(Resolution.Profile.LocationOffset);
		SkeletalVisual->SetRelativeRotation(Resolution.Profile.RotationOffset);
	}
	else
	{
		SkeletalVisual->SetVisibility(false);
		SkeletalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VisualMesh->SetVisibility(true);
		VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		if (StaticMesh != nullptr)
		{
			VisualMesh->SetStaticMesh(StaticMesh);
			bUsingPrimitiveFallback = false;
			ActiveVisualScale = Resolution.Profile.VisualScale;
			VisualMesh->SetRelativeLocation(Resolution.Profile.LocationOffset);
			VisualMesh->SetRelativeRotation(Resolution.Profile.RotationOffset);
		}
	}

	if (Material != nullptr)
	{
		(bUsingSkeletalPresentation
			? static_cast<UMeshComponent*>(SkeletalVisual.Get())
			: static_cast<UMeshComponent*>(VisualMesh.Get()))->SetMaterial(0, Material);
	}
	SetVisualScale(ActiveVisualScale);
}

bool AWBRuntimeUnitPresentationActor::PlayBoundAnimation(
	const FWBRuntimePresentationAssetBinding& Binding,
	UWBRuntimePresentationAssetRegistry* Registry,
	const int32 MatchGeneration)
{
	LastAssetDiagnostic.Reset();
	if (Binding.AnimationMontage.IsNull()
		&& Binding.AnimationSequence.IsNull()
		&& !bHasAssetProfile)
	{
		return false;
	}
	if (!bUsingSkeletalPresentation || Registry == nullptr
		|| SkeletalVisual == nullptr || SkeletalVisual->GetSkeletalMeshAsset() == nullptr)
	{
		LastAssetDiagnostic = TEXT("skeletal_presentation_unavailable");
		return false;
	}

	if (UAnimMontage* Montage =
		Registry->LoadMontage(Binding.AnimationMontage, MatchGeneration))
	{
		if (Montage->GetSkeleton() != SkeletalVisual->GetSkeletalMeshAsset()->GetSkeleton())
		{
			LastAssetDiagnostic = TEXT("animation_skeleton_incompatible");
			return false;
		}
		if (UAnimInstance* AnimInstance = SkeletalVisual->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage, FMath::Max(Binding.PlaybackRate, 0.01f));
			return true;
		}
		LastAssetDiagnostic = TEXT("animation_instance_unavailable");
		return false;
	}

	UAnimSequenceBase* Sequence =
		Registry->LoadAnimation(Binding.AnimationSequence, MatchGeneration);
	if (Sequence == nullptr && bHasAssetProfile)
	{
		const TSoftObjectPtr<UAnimSequenceBase>* ProfileAnimation = nullptr;
		switch (Binding.EventType)
		{
		case EWBRuntimePresentationEventType::UnitMoved:
		case EWBRuntimePresentationEventType::NPCMoved:
			ProfileAnimation = &ActiveAssetProfile.MoveAnimation;
			break;
		case EWBRuntimePresentationEventType::AttackDeclared:
		case EWBRuntimePresentationEventType::NPCAttacked:
			ProfileAnimation = &ActiveAssetProfile.AttackAnimation;
			break;
		case EWBRuntimePresentationEventType::AttackImpact:
		case EWBRuntimePresentationEventType::DamageApplied:
		case EWBRuntimePresentationEventType::HPChanged:
		case EWBRuntimePresentationEventType::ArmorChanged:
			ProfileAnimation = &ActiveAssetProfile.HitAnimation;
			break;
		case EWBRuntimePresentationEventType::UnitDefeated:
		case EWBRuntimePresentationEventType::HeroDefeated:
			ProfileAnimation = &ActiveAssetProfile.DeathAnimation;
			break;
		case EWBRuntimePresentationEventType::UnitSummoned:
		case EWBRuntimePresentationEventType::NPCSpawned:
			ProfileAnimation = &ActiveAssetProfile.SummonAnimation;
			break;
		default:
			break;
		}
		if (ProfileAnimation != nullptr)
		{
			Sequence = Registry->LoadAnimation(*ProfileAnimation, MatchGeneration);
		}
	}
	if (Sequence == nullptr)
	{
		LastAssetDiagnostic = Registry->GetLastDiagnostic();
		return false;
	}
	if (Sequence->GetSkeleton() != SkeletalVisual->GetSkeletalMeshAsset()->GetSkeleton())
	{
		LastAssetDiagnostic = TEXT("animation_skeleton_incompatible");
		return false;
	}
	SkeletalVisual->SetPlayRate(FMath::Max(Binding.PlaybackRate, 0.01f));
	SkeletalVisual->PlayAnimation(Sequence, Binding.bLoop);
	return true;
}

void AWBRuntimeUnitPresentationActor::StopBoundAnimation()
{
	if (SkeletalVisual == nullptr)
	{
		return;
	}
	if (UAnimInstance* AnimInstance = SkeletalVisual->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.0f);
	}
	SkeletalVisual->Stop();
}

USceneComponent* AWBRuntimeUnitPresentationActor::GetAssetAttachmentComponent() const
{
	return bUsingSkeletalPresentation
		? static_cast<USceneComponent*>(SkeletalVisual.Get())
		: static_cast<USceneComponent*>(VisualMesh.Get());
}

int32 AWBRuntimeUnitPresentationActor::GetStableUnitId() const
{
	return Presentation.UnitId;
}

FWBRuntimeUnitPresentation AWBRuntimeUnitPresentationActor::GetPresentation() const
{
	return Presentation;
}

bool AWBRuntimeUnitPresentationActor::IsUsingSkeletalPresentation() const
{
	return bUsingSkeletalPresentation;
}

bool AWBRuntimeUnitPresentationActor::IsUsingPrimitiveFallback() const
{
	return bUsingPrimitiveFallback;
}

FString AWBRuntimeUnitPresentationActor::GetLastAssetDiagnostic() const
{
	return LastAssetDiagnostic;
}

void AWBRuntimeUnitPresentationActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPresentationActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	PresentationElapsedSeconds += DeltaSeconds;
	const float Alpha = FMath::Clamp(
		PresentationElapsedSeconds / PresentationDurationSeconds,
		0.0f,
		1.0f);
	if (ActivePresentationType == EWBRuntimePresentationEventType::UnitMoved
		|| ActivePresentationType == EWBRuntimePresentationEventType::NPCMoved)
	{
		SetActorLocation(FMath::Lerp(PresentationStartLocation, PresentationEndLocation, Alpha));
	}
	else if (ActivePresentationType == EWBRuntimePresentationEventType::UnitSummoned
		|| ActivePresentationType == EWBRuntimePresentationEventType::NPCSpawned)
	{
		SetActorLocation(PresentationEndLocation);
		SetVisualScale(ActiveVisualScale * Alpha);
	}
	else if (ActivePresentationType == EWBRuntimePresentationEventType::UnitDefeated
		|| ActivePresentationType == EWBRuntimePresentationEventType::HeroDefeated)
	{
		SetVisualScale(ActiveVisualScale * (1.0f - Alpha));
	}
	else
	{
		const float Pulse = 1.0f + FMath::Sin(Alpha * PI) * 0.18f;
		SetVisualScale(ActiveVisualScale * Pulse);
	}

	if (Alpha >= 1.0f)
	{
		bPresentationActive = false;
		SetActorTickEnabled(false);
	}
}

void AWBRuntimeUnitPresentationActor::SetVisualScale(const FVector& Scale)
{
	if (bUsingSkeletalPresentation)
	{
		SkeletalVisual->SetRelativeScale3D(Scale);
	}
	else
	{
		VisualMesh->SetRelativeScale3D(Scale);
	}
}
