#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationSourceGate.h"
#include "WBEffectRequest.h"

enum class EWBCardDefinitionKind : uint8
{
	Unknown,
	Character,
	Hybrid,
	Wand,
	Action,
	Terrain,
	Wall,
	Trap,
	NPC,
	Fixture
};

enum class EWBCardEffectTargetRequirement : uint8
{
	None,
	Unit,
	Tile,
	WallEdge
};

enum class EWBCardEffectAttackDefenderRequirement : uint8
{
	Any,
	OwnHeroCurrentDefender,
	OwnCurrentDefender
};

enum class EWBCardEffectTargetControllerRequirement : uint8
{
	Any,
	Self
};

enum class EWBCardEffectTargetRelationRequirement : uint8
{
	Any,
	OrthogonallyAdjacentToOwnHero,
	OtherThanOwnHero
};

struct WANDBOUNDCORE_API FWBCardEffectActivationCondition
{
	EWBCardEffectAttackDefenderRequirement AttackDefender =
		EWBCardEffectAttackDefenderRequirement::Any;
	EWBCardEffectTargetControllerRequirement TargetController =
		EWBCardEffectTargetControllerRequirement::Any;
	EWBCardEffectTargetRelationRequirement TargetRelation =
		EWBCardEffectTargetRelationRequirement::Any;
	FString RequiredTargetFaction;
};

enum class EWBSetupSummonTriggerScope : uint8
{
	OwnWhenSummoned,
	CharacterSummoned,
	UnitSummoned,
	YouSummonUnit,
	OpponentSummonsUnit,
	FactionSummoned
};

struct WANDBOUNDCORE_API FWBSetupSummonTriggerDefinition
{
	FString TriggerId;
	EWBSetupSummonTriggerScope Scope =
		EWBSetupSummonTriggerScope::OwnWhenSummoned;
	FString FactionId;
	int32 DrawCount = 0;
	bool bMandatory = true;
};

struct WANDBOUNDCORE_API FWBCardEffectDefinition
{
	FString EffectId;
	FString PublicLabel;
	EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::None;
	TArray<FWBGenericEffectPayload> Payloads;
	FWBCardActivationSourceGateDefinition SourceGate;
	FWBCardEffectActivationCondition ActivationCondition;
};

enum class EWBTurnStartTriggerScope : uint8
{
	AtStartOfYourTurn,
	AtStartOfEachTurn
};

struct WANDBOUNDCORE_API FWBTurnStartTriggerDefinition
{
	FString TriggerId;
	EWBTurnStartTriggerScope Scope =
		EWBTurnStartTriggerScope::AtStartOfYourTurn;
	EWBCardEffectTargetRequirement TargetRequirement =
		EWBCardEffectTargetRequirement::None;
	TArray<FWBGenericEffectPayload> Payloads;
	int32 DrawCount = 0;
	bool bMandatory = true;
};

enum class EWBAfterDamageRequirement : uint8
{
	DamageResolved,
	PositiveHPDamage
};

enum class EWBAfterDamageParticipantRole : uint8
{
	Attacker,
	HitUnit,
	FinalDamageRecipient,
	BattleParticipant
};

enum class EWBAfterDamageTargetRole : uint8
{
	None,
	Self,
	Attacker,
	HitUnit,
	FinalDamageRecipient,
	OpposingBattleUnit
};

struct WANDBOUNDCORE_API FWBAfterDamageTriggerDefinition
{
	FString TriggerId;
	EWBAfterDamageParticipantRole SourceRole =
		EWBAfterDamageParticipantRole::BattleParticipant;
	EWBAfterDamageRequirement DamageRequirement =
		EWBAfterDamageRequirement::PositiveHPDamage;
	EWBAfterDamageTargetRole TargetRole =
		EWBAfterDamageTargetRole::None;
	TArray<FWBGenericEffectPayload> Payloads;
	bool bMandatory = true;
	bool bOncePerTurn = false;
	bool bOncePerTurnPerOpposingUnit = false;
};

enum class EWBPreDamageAttackTriggerSourceRole : uint8
{
	Unknown,
	CurrentDefender
};

enum class EWBPreDamageAttackTriggerTiming : uint8
{
	Unknown,
	AfterPreHitBeforeCalculateDamage
};

enum class EWBDeterministicRandomBranchKind : uint8
{
	Unknown,
	CoinFlip
};

enum class EWBPendingBattleHitModifierOperation : uint8
{
	Unknown,
	ReflectToAttacker,
	AddRawDamage
};

struct WANDBOUNDCORE_API FWBPendingBattleHitModifierDefinition
{
	EWBPendingBattleHitModifierOperation Operation =
		EWBPendingBattleHitModifierOperation::Unknown;
	int32 Amount = 0;
};

struct WANDBOUNDCORE_API FWBPreDamageAttackTriggerDefinition
{
	FString TriggerId;
	EWBPreDamageAttackTriggerSourceRole SourceRole =
		EWBPreDamageAttackTriggerSourceRole::Unknown;
	EWBPreDamageAttackTriggerTiming Timing =
		EWBPreDamageAttackTriggerTiming::Unknown;
	EWBDeterministicRandomBranchKind RandomBranch =
		EWBDeterministicRandomBranchKind::Unknown;
	FWBPendingBattleHitModifierDefinition Heads;
	FWBPendingBattleHitModifierDefinition Tails;
	bool bMandatory = true;
	bool bOncePerTurn = false;
};

struct WANDBOUNDCORE_API FWBAfterCSNInheritanceTriggerDefinition
{
	FString TriggerId;
	int32 DrawCount = 0;
	bool bMandatory = true;
};

enum class EWBAfterUnitDestroyedSourceScope : uint8
{
	Unknown,
	DestroyedSelf,
	ControlledFactionUnitDestroyed
};

enum class EWBPostDestructionEffectOperation : uint8
{
	Unknown,
	SummonCharacterFromDeckToDestroyedTile,
	ApplyPersistentStatDeltaToTriggerSource
};

enum class EWBPostDestructionTarget : uint8
{
	Unknown,
	TriggerSource
};

struct WANDBOUNDCORE_API FWBUnitStatDeltaDefinition
{
	int32 ATKDelta = 0;
	int32 MaxHPDelta = 0;
	int32 CurrentHPDelta = 0;
};

enum class EWBContinuousAuraTargetRelation : uint8
{
	Unknown,
	Enemy
};

enum class EWBContinuousStat : uint8
{
	Unknown,
	AR
};

enum class EWBContinuousStatOperation : uint8
{
	Unknown,
	Add
};

enum class EWBContinuousAuraRangeStat : uint8
{
	Unknown,
	AR
};

enum class EWBContinuousAuraGeometry : uint8
{
	Unknown,
	AttackLine
};

struct WANDBOUNDCORE_API FWBContinuousStatAuraDefinition
{
	FString AuraId;
	EWBContinuousAuraTargetRelation TargetRelation =
		EWBContinuousAuraTargetRelation::Unknown;
	EWBContinuousStat TargetStat = EWBContinuousStat::Unknown;
	EWBContinuousStatOperation Operation =
		EWBContinuousStatOperation::Unknown;
	int32 Amount = 0;
	EWBContinuousAuraRangeStat RangeStat =
		EWBContinuousAuraRangeStat::Unknown;
	EWBContinuousAuraGeometry Geometry =
		EWBContinuousAuraGeometry::Unknown;
	bool bBlockedByWalls = true;
	bool bBlockedByUnits = true;
	int32 MinimumResult = 0;
};

struct WANDBOUNDCORE_API FWBAfterUnitDestroyedTriggerDefinition
{
	FString TriggerId;
	EWBAfterUnitDestroyedSourceScope SourceScope =
		EWBAfterUnitDestroyedSourceScope::Unknown;
	EWBPostDestructionEffectOperation Operation =
		EWBPostDestructionEffectOperation::Unknown;
	FString RequiredFaction;
	int32 SummonCount = 0;
	bool bMandatory = true;
	bool bIgnoreSummoningConditions = false;
	bool bApplyCSNInheritance = false;
	EWBPostDestructionTarget Target = EWBPostDestructionTarget::Unknown;
	FWBUnitStatDeltaDefinition StatDelta;
};

struct WANDBOUNDCORE_API FWBCardCharacterStatsDefinition
{
	int32 HP = 0;
	int32 ATK = 0;
	int32 AR = 0;
	int32 RL = 0;
};

struct WANDBOUNDCORE_API FWBCardWandStatsDefinition
{
	int32 RR = 0;
};

struct WANDBOUNDCORE_API FWBCardHybridSummonDefinition
{
	int32 SacrificeCount = 0;
	FName SacrificeRequirement;
	int32 WandPaymentCount = 0;
	TArray<FName> WandPaymentSources;
	FName HeroDestination;
	FName NonHeroDestination;
};

struct WANDBOUNDCORE_API FWBCardDefinition
{
	FString CardId;
	FString PublicName;
	FString PublicCategory;
	TArray<FString> PublicFactions;
	TArray<FString> PublicTags;
	FString PublicRulesText;
	EWBCardDefinitionKind Kind = EWBCardDefinitionKind::Unknown;
	FWBCardCharacterStatsDefinition CharacterStats;
	FWBCardWandStatsDefinition WandStats;
	TSet<EWBCombatCapability> GrantedCombatCapabilitiesWhileEquipped;
	FWBCardHybridSummonDefinition HybridSummon;
	int32 TrapDamage = 0;
	TArray<FWBCardEffectDefinition> ActivatedEffects;
	TArray<FWBSetupSummonTriggerDefinition> SetupSummonTriggers;
	TArray<FWBTurnStartTriggerDefinition> TurnStartTriggers;
	TArray<FWBAfterDamageTriggerDefinition> AfterDamageTriggers;
	TArray<FWBPreDamageAttackTriggerDefinition> PreDamageAttackTriggers;
	TArray<FWBAfterCSNInheritanceTriggerDefinition>
		AfterCSNInheritanceTriggers;
	TArray<FWBAfterUnitDestroyedTriggerDefinition>
		AfterUnitDestroyedTriggers;
	TArray<FWBContinuousStatAuraDefinition> ContinuousStatAuras;
};
