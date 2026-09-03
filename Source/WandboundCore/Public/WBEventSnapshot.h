#pragma once

#include "CoreMinimal.h"
#include "WBStatusTypes.h"
#include "WBTypes.h"

struct FWBGameStateData;
struct FWBUnitState;

enum class EWBEventKind : uint8
{
	Unknown,
	Attack,
	AfterDamage,
	Destruction,
	TurnStart,
	Summon,
	Inheritance,
	Status,
	Marker,
	NPCSpawn,
	Activation
};

enum class EWBTriggerEligibilityPolicy : uint8
{
	SnapshotAtCollection,
	LiveSourceAtResolution,
	Hybrid
};

struct WANDBOUNDCORE_API FWBEventIdentitySnapshot
{
	FString EventId;
	EWBEventKind Kind = EWBEventKind::Unknown;
	int32 TurnNumber = INDEX_NONE;
	FString SourceActionId;
	FString ContinuationId;
	EWBDeclarationProvenance ActionDeclaration =
		EWBDeclarationProvenance::Automatic;
	EWBDeclarationProvenance TargetDeclaration =
		EWBDeclarationProvenance::Automatic;

	bool IsValid() const;
};

struct WANDBOUNDCORE_API FWBUnitParticipantSnapshot
{
	int32 UnitId = INDEX_NONE;
	FString CardId;
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	FWBTile Tile = FWBTile(-1, -1);
	bool bWasHero = false;

	bool IsCaptured() const;
};

struct WANDBOUNDCORE_API FWBEventSourceSnapshot
{
	int32 SourceUnitId = INDEX_NONE;
	FString SourceCardId;
	FString SourceCardInstanceId;
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	FWBTile SourceTile = FWBTile(-1, -1);
	bool bWasHero = false;
	EWBActivationProvenance ActivationProvenance =
		EWBActivationProvenance::ResolutionOnly;

	int32 GetCasterUnitId() const;
	FWBUnitParticipantSnapshot AsParticipant() const;
};

class WANDBOUNDCORE_API WBEventSnapshot
{
public:
	static FWBEventIdentitySnapshot MakeIdentity(
		EWBEventKind Kind,
		const FString& EventId,
		int32 TurnNumber,
		const FString& SourceActionId = FString(),
		const FString& ContinuationId = FString(),
		EWBDeclarationProvenance ActionDeclaration =
			EWBDeclarationProvenance::Automatic,
		EWBDeclarationProvenance TargetDeclaration =
			EWBDeclarationProvenance::Automatic);

	static FWBUnitParticipantSnapshot CaptureUnitParticipant(
		const FWBGameStateData& State,
		const FWBUnitState& Unit);

	static FWBEventSourceSnapshot CaptureUnitSource(
		const FWBGameStateData& State,
		const FWBUnitState& Unit,
		EWBActivationProvenance ActivationProvenance =
			EWBActivationProvenance::ResolutionOnly);

	static FWBEventSourceSnapshot FromStatusSource(
		const FWBStatusSourceProvenance& Source);
};
