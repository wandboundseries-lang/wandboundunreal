#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationSourceGate.h"
#include "WBEffectRequest.h"

enum class EWBCardDefinitionKind : uint8
{
	Unknown,
	Character,
	Wand,
	Action,
	Terrain,
	Wall,
	Fixture
};

enum class EWBCardEffectTargetRequirement : uint8
{
	None,
	Unit,
	Tile,
	WallEdge
};

struct WANDBOUNDCORE_API FWBCardEffectDefinition
{
	FString EffectId;
	FString PublicLabel;
	EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::None;
	TArray<FWBGenericEffectPayload> Payloads;
	FWBCardActivationSourceGateDefinition SourceGate;
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

struct WANDBOUNDCORE_API FWBCardDefinition
{
	FString CardId;
	FString PublicName;
	EWBCardDefinitionKind Kind = EWBCardDefinitionKind::Unknown;
	FWBCardCharacterStatsDefinition CharacterStats;
	FWBCardWandStatsDefinition WandStats;
	TArray<FWBCardEffectDefinition> ActivatedEffects;
};
