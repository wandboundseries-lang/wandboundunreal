#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WBRuntimePresentationEvent.h"
#include "WBRuntimePresentationAssetBinding.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;

UENUM(BlueprintType)
enum class EWBRuntimePresentationUnitCategory : uint8
{
	Any,
	PlayerHero,
	PlayerUnit,
	NeutralNPC,
	ConcealedMarker,
	RevealedTrap,
	RevealedNPCMarker
};

UENUM(BlueprintType)
enum class EWBRuntimePresentationAttachmentPolicy : uint8
{
	SourceLocation,
	TargetLocation,
	SourceTile,
	DestinationTile,
	AttachToSource,
	AttachToTarget
};

UENUM(BlueprintType)
enum class EWBRuntimePresentationCleanupPolicy : uint8
{
	EventCompletion,
	SequenceCompletion,
	Manual
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationBindingContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimePresentationEventType EventType =
		EWBRuntimePresentationEventType::MatchInitialized;

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimePresentationUnitCategory UnitCategory =
		EWBRuntimePresentationUnitCategory::Any;

	UPROPERTY(BlueprintReadOnly)
	FString PublicDefinitionId;

	UPROPERTY(BlueprintReadOnly)
	FName PublicMarkerType;

	UPROPERTY(BlueprintReadOnly)
	bool bPublicDefinitionIdAllowed = false;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationAssetBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWBRuntimePresentationEventType EventType =
		EWBRuntimePresentationEventType::MatchInitialized;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWBRuntimePresentationUnitCategory UnitCategory =
		EWBRuntimePresentationUnitCategory::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString PublicDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StablePriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimMontage> AnimationMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> AnimationSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USkeletalMesh> SkeletalMeshOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> StaticMeshOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float PresentationDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float PlaybackRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWBRuntimePresentationAttachmentPolicy AttachmentPolicy =
		EWBRuntimePresentationAttachmentPolicy::TargetLocation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWBRuntimePresentationCleanupPolicy CleanupPolicy =
		EWBRuntimePresentationCleanupPolicy::EventCompletion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOptional = true;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeUnitAssetProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWBRuntimePresentationUnitCategory UnitCategory =
		EWBRuntimePresentationUnitCategory::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString PublicDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StablePriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> MoveAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> AttackAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> HitAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> DeathAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimSequenceBase> SummonAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector VisualScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PrimarySocketName;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationBindingResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFoundConfiguredBinding = false;

	UPROPERTY(BlueprintReadOnly)
	bool bUsePrimitiveFallback = true;

	UPROPERTY(BlueprintReadOnly)
	int32 Specificity = 0;

	UPROPERTY(BlueprintReadOnly)
	FString Reason = TEXT("presentation_asset_fallback");

	UPROPERTY(BlueprintReadOnly)
	FWBRuntimePresentationAssetBinding Binding;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeUnitProfileResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFoundConfiguredProfile = false;

	UPROPERTY(BlueprintReadOnly)
	bool bUsePrimitiveFallback = true;

	UPROPERTY(BlueprintReadOnly)
	int32 Specificity = 0;

	UPROPERTY(BlueprintReadOnly)
	FString Reason = TEXT("unit_asset_profile_fallback");

	UPROPERTY(BlueprintReadOnly)
	FWBRuntimeUnitAssetProfile Profile;
};

UCLASS(BlueprintType)
class WANDBOUNDRUNTIME_API UWBRuntimePresentationAssetSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wandbound|Presentation Assets")
	TArray<FWBRuntimePresentationAssetBinding> EventBindings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wandbound|Presentation Assets")
	TArray<FWBRuntimeUnitAssetProfile> UnitProfiles;
};
