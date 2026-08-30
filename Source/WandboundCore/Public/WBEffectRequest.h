#pragma once

#include "CoreMinimal.h"
#include "WBArmorEffect.h"
#include "WBDamageEffect.h"
#include "WBHealEffect.h"
#include "WBReplayTrace.h"
#include "WBStatusEffect.h"
#include "WBTypes.h"

enum class EWBGenericEffectOp : uint8
{
	Unknown,
	ArmorEffect,
	StatusEffect,
	DamageEffect,
	HealEffect,
	NegatePendingEffect,
	PreventPendingAttack,
	RedirectPendingAttack,
	RegisterPendingAttackHPDamageSubstitution,
	ReplacePendingAttackDefenderFromHand,
	SacrificeSourceThenSummonCharacterFromDeckToSourceTile,
	SetTerrain
};

enum class EWBEffectTileRangeMetric : uint8
{
	Unknown,
	Manhattan
};

enum class EWBEffectRangeStat : uint8
{
	Unknown,
	AR
};

struct WANDBOUNDCORE_API FWBSetTerrainEffectRequest
{
	FName TerrainId;
	EWBEffectTileRangeMetric RangeMetric = EWBEffectTileRangeMetric::Unknown;
	EWBEffectRangeStat RangeStat = EWBEffectRangeStat::Unknown;
	bool bAllowOccupied = false;
	bool bRequireLineOfSight = false;
};

enum class EWBEffectAuxiliaryCardZone : uint8
{
	None,
	Hand
};

enum class EWBEffectReplacementCardKind : uint8
{
	Unknown,
	Character
};

enum class EWBEffectInheritancePolicy : uint8
{
	None,
	TransferEquippedWandsAndAddSourceCurrentRL
};

struct WANDBOUNDCORE_API FWBEffectAuxiliaryCardSelection
{
	EWBEffectAuxiliaryCardZone Zone = EWBEffectAuxiliaryCardZone::None;
	FString CardInstanceId;
	FString CardId;
	EWBDeclarationProvenance TargetDeclaration = EWBDeclarationProvenance::Automatic;
};

struct WANDBOUNDCORE_API FWBEffectSourceRef
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	FString SourceCardId;
	FString SourceEffectId;
	EWBActivationProvenance ActivationProvenance =
		EWBActivationProvenance::ResolutionOnly;

	int32 GetCasterUnitId() const
	{
		return WBIsActivation(ActivationProvenance) && SourceUnitId >= 0
			? SourceUnitId
			: INDEX_NONE;
	}
};

struct WANDBOUNDCORE_API FWBEffectTargetRef
{
	int32 TargetUnitId = -1;
	FWBTile TargetTile;
	FWBWallEdge TargetWallEdge;
	EWBDeclarationProvenance TargetDeclaration = EWBDeclarationProvenance::Automatic;
};

struct WANDBOUNDCORE_API FWBGenericEffectPayload
{
	EWBGenericEffectOp Operation = EWBGenericEffectOp::Unknown;
	FWBArmorEffectRequest ArmorEffect;
	FWBStatusEffectRequest StatusEffect;
	FWBDamageEffectRequest DamageEffect;
	FWBHealEffectRequest HealEffect;
	FWBSetTerrainEffectRequest SetTerrainEffect;
	FString PendingEffectFrameId;
	FString PendingAttackContinuationId;
	FString RequiredSourceFaction;
	FString RequiredReplacementFaction;
	EWBEffectReplacementCardKind RequiredReplacementKind =
		EWBEffectReplacementCardKind::Unknown;
	EWBEffectInheritancePolicy InheritancePolicy =
		EWBEffectInheritancePolicy::None;
};

struct WANDBOUNDCORE_API FWBEffectRequest
{
	FWBEffectSourceRef Source;
	FWBEffectTargetRef Target;
	FWBEffectAuxiliaryCardSelection AuxiliaryCardSelection;
	TArray<FWBGenericEffectPayload> Payloads;
};

struct WANDBOUNDCORE_API FWBEffectRequestResult
{
	bool bOk = false;
	FString Reason;
	FWBEffectRequest Request;
	TArray<FWBApplyActionResult> PayloadResults;
	TArray<FWBTraceEvent> TraceEvents;
};
