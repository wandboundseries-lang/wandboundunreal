#pragma once

#include "CoreMinimal.h"
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
};

struct WANDBOUNDCORE_API FWBCardDefinition
{
	FString CardId;
	FString PublicName;
	EWBCardDefinitionKind Kind = EWBCardDefinitionKind::Unknown;
	TArray<FWBCardEffectDefinition> ActivatedEffects;
};
