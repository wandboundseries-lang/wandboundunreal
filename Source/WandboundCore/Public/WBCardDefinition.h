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
	OwnHeroCurrentDefender
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
};
