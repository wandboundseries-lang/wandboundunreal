#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeMatchPresentation.generated.h"

UENUM(BlueprintType)
enum class EWBRuntimeBoardHighlight : uint8
{
	None,
	Move,
	Attack,
	Summon,
	Equip,
	Activation,
	Ambiguous
};

UENUM(BlueprintType)
enum class EWBRuntimeMatchActionFamily : uint8
{
	CoreAction,
	Move,
	Attack,
	Pass,
	PassResponse,
	EndTurn,
	Summon,
	Equip,
	Activation,
	Discard
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeHandCardPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString CardInstanceId;

	UPROPERTY(BlueprintReadOnly)
	FString DefinitionId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FName CardType;

	UPROPERTY(BlueprintReadOnly)
	int32 HandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 RR = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MPCost = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bSelectable = false;

	UPROPERTY(BlueprintReadOnly)
	bool bSelected = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<EWBRuntimeMatchActionFamily> AvailableActionFamilies;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeBoardTilePresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FIntPoint Tile = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly)
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly)
	int32 OccupantUnitId = -1;

	UPROPERTY(BlueprintReadOnly)
	bool bHasConcealedMarker = false;

	UPROPERTY(BlueprintReadOnly)
	int32 PublicMarkerId = -1;

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimeBoardHighlight Highlight = EWBRuntimeBoardHighlight::None;

	UPROPERTY(BlueprintReadOnly)
	bool bSelected = false;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimePublicStatusPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName StatusId;

	UPROPERTY(BlueprintReadOnly)
	int32 TurnsRemaining = 0;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeUnitPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 UnitId = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 OwnerId = -1;

	UPROPERTY(BlueprintReadOnly)
	bool bNeutralNPC = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHero = false;

	UPROPERTY(BlueprintReadOnly)
	FIntPoint Tile = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly)
	int32 HP = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxHP = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ATK = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 AR = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 BaseRL = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentRL = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RLUsed = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 AttacksRemaining = 0;

	UPROPERTY(BlueprintReadOnly)
	FString PublicDefinitionId;

	UPROPERTY(BlueprintReadOnly)
	TArray<FWBRuntimePublicStatusPresentation> Statuses;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeLegalActionPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ActionId;

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimeMatchActionFamily Family = EWBRuntimeMatchActionFamily::CoreAction;

	UPROPERTY(BlueprintReadOnly)
	FName ActionType;

	UPROPERTY(BlueprintReadOnly)
	int32 PlayerId = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceUnitId = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetUnitId = -1;

	UPROPERTY(BlueprintReadOnly)
	FIntPoint TargetTile = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly)
	FString SourceCardInstanceId;

	UPROPERTY(BlueprintReadOnly)
	FString PublicLabel;

	UPROPERTY(BlueprintReadOnly)
	int32 MatchGeneration = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DecisionRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bRequiresTargetSelection = false;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeSelectionPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectedUnitId = -1;

	UPROPERTY(BlueprintReadOnly)
	FString SelectedCardInstanceId;

	UPROPERTY(BlueprintReadOnly)
	bool bHasActionFamilyFilter = false;

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimeMatchActionFamily ActionFamilyFilter = EWBRuntimeMatchActionFamily::CoreAction;

	UPROPERTY(BlueprintReadOnly)
	FString ResolvedActionId;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> AmbiguousActionIds;

	UPROPERTY(BlueprintReadOnly)
	FString StatusReason;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeMatchPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayerId = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 ViewerPlayerId = -1;

	UPROPERTY(BlueprintReadOnly)
	FName Phase;

	UPROPERTY(BlueprintReadOnly)
	int32 TurnNumber = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RemainingMP = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bGameOver = false;

	UPROPERTY(BlueprintReadOnly)
	int32 WinnerPlayerId = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 DefeatedPlayerId = -1;

	UPROPERTY(BlueprintReadOnly)
	FString TerminalReason;

	UPROPERTY(BlueprintReadOnly)
	int32 FinalTurnNumber = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 FinalPresentationRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MatchGeneration = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DecisionRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PresentationRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasPendingDecision = false;

	UPROPERTY(BlueprintReadOnly)
	FString StatusMessage;

	UPROPERTY(BlueprintReadOnly)
	int32 NPCTraceEventCount = 0;
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeMatchCommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	UPROPERTY(BlueprintReadOnly)
	FString ActionId;

	UPROPERTY(BlueprintReadOnly)
	int32 MatchGeneration = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DecisionRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> CandidateActionIds;
};
