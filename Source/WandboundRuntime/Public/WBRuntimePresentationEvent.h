#pragma once

#include "CoreMinimal.h"
#include "WBRuntimePresentationEvent.generated.h"

UENUM(BlueprintType)
enum class EWBRuntimePresentationEventType : uint8
{
	MatchInitialized,
	TurnStarted,
	TurnEnded,
	NPCPhaseStarted,
	NPCPhaseCompleted,
	UnitMoved,
	AttackDeclared,
	AttackImpact,
	DamageApplied,
	HPChanged,
	ArmorChanged,
	UnitSummoned,
	UnitSacrificed,
	WandPaymentCommitted,
	HeroReplaced,
	WandEquipped,
	ActivationResolved,
	MarkerRevealed,
	MarkerConsumed,
	TrapTriggered,
	NPCSpawned,
	NPCMoved,
	NPCAttacked,
	UnitDefeated,
	EquipmentDiscarded,
	HeroDefeated,
	GameOver
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SequenceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceTraceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceStageIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimePresentationEventType Type = EWBRuntimePresentationEventType::MatchInitialized;

	UPROPERTY(BlueprintReadOnly)
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceUnitId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetUnitId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FIntPoint SourceTile = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly)
	FIntPoint DestinationTile = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly)
	int32 DamageAmount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PreviousHP = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 NewHP = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 PreviousArmor = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 NewArmor = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 PreviousRLUsed = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 NewRLUsed = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 MarkerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FName PublicMarkerType;

	UPROPERTY(BlueprintReadOnly)
	int32 TurnNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 WinnerPlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FString PublicDefinitionId;

	UPROPERTY(BlueprintReadOnly)
	FString PublicLabel;

	UPROPERTY(BlueprintReadOnly)
	float SuggestedDurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bSkippable = true;

	UPROPERTY(BlueprintReadOnly)
	bool bTerminal = false;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePresentationTranslationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	UPROPERTY(BlueprintReadOnly)
	TArray<FWBRuntimePresentationEvent> Events;
};
