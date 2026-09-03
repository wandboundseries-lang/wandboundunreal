#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneState.h"
#include "WBStatusTypes.h"
#include "WBTerminalOutcome.h"
#include "WBTypes.h"

enum class EWBResonanceModifierTarget : uint8
{
	CurrentRL
};

enum class EWBResonanceModifierOperation : uint8
{
	Add
};

struct WANDBOUNDCORE_API FWBResonanceModifierState
{
	FString SourceId;
	EWBResonanceModifierTarget Target = EWBResonanceModifierTarget::CurrentRL;
	EWBResonanceModifierOperation Operation = EWBResonanceModifierOperation::Add;
	int32 Amount = 0;
};

struct WANDBOUNDCORE_API FWBUnitState
{
	int32 UnitId = -1;
	int32 OwnerPlayerId = -1;
	int32 ControllerPlayerId = -1;
	// Compatibility mirror of ControllerPlayerId for legacy fixtures/consumers.
	int32 OwnerId = -1;
	FString CardId;
	int32 X = -1;
	int32 Y = -1;
	int32 HP = 1;
	int32 MaxHP = 1;
	int32 CurrentArmor = 0;
	int32 MaxArmor = 0;
	int32 ATK = 1;
	int32 AR = 1;
	int32 BaseRL = 0;
	int32 CurrentRL = 0;
	// Compatibility mirror for older tests and serializers. Rules truth uses CurrentRL.
	int32 RLTotal = 0;
	int32 RLUsed = 0;
	int32 AttacksLeft = 0;
	int32 MaxAttacksPerTurn = 1;
	int32 MPRemaining = 0;
	int32 NPCSpawnOrder = INDEX_NONE;
	int32 NPCCreationTurnNumber = INDEX_NONE;
	int32 NPCTriggeredByUnitId = INDEX_NONE;
	bool bDefeated = false;
	bool bRemovedFromBoard = false;
	TSet<FName> Statuses;
	TMap<FName, int32> StatusTurnsRemaining;
	// Canonical gameplay authority. The fields above are compatibility mirrors.
	TArray<FWBStatusInstanceState> StatusStates;
	TSet<FName> Passives;
	TSet<EWBCombatCapability> CombatCapabilities;
	TArray<FWBResonanceModifierState> ResonanceModifiers;

	bool IsUnitOnBoard() const;
	int32 GetOwnerPlayerIdForRules() const;
	int32 GetControllerPlayerIdForRules() const;
	void SetOwnerAndControllerForRules(int32 InOwnerPlayerId, int32 InControllerPlayerId);
	void SetControllerPlayerIdForRules(int32 InControllerPlayerId);
	void NormalizeIdentityForRules();
	int32 GetBaseRLForRules() const;
	int32 GetCurrentRLForRules() const;
	int32 GetAvailableRLForRules() const;
	void SetCanonicalRL(int32 InBaseRL, int32 InCurrentRL, int32 InRLUsed);
	int32 GetCurrentArmor() const;
	int32 GetMaxArmor() const;
	void SetArmorForTest(int32 InCurrentArmor, int32 InMaxArmor);
	void MarkUnitDefeated();
	void RemoveUnitFromBoard();
	bool HasStatus(FName StatusId) const;
	void AddStatus(
		FName StatusId,
		int32 TurnsRemaining = 0,
		const FWBStatusSourceProvenance& Source = FWBStatusSourceProvenance());
	void RemoveStatus(FName StatusId);
	int32 GetStatusTurnsRemaining(FName StatusId) const;
	void SetStatusTurnsRemaining(FName StatusId, int32 TurnsRemaining);
	const FWBStatusInstanceState* GetStatusState(FName StatusId) const;
	FWBStatusInstanceState* GetMutableStatusState(FName StatusId);
	void NormalizeStatusStateForRules();
	TArray<FWBStatusInstanceState> GetSortedStatusStatesForRules() const;
	TArray<FName> GetSortedStatusIdsForTrace() const;
};

enum class EWBGamePhase : uint8
{
	NormalTurn,
	Response
};

enum class EWBReactionWindowKind : uint8
{
	None,
	PreHit,
	PostHit,
	PostMove,
	PostSummon,
	PostEffect
};

struct WANDBOUNDCORE_API FWBReactionWindowState
{
	EWBReactionWindowKind Kind = EWBReactionWindowKind::None;
	int32 OriginatingPlayerId = -1;
	int32 ConsecutivePassCount = 0;
	FString SourceActionId;
	int32 SourceUnitId = -1;
	int32 TargetUnitId = -1;

	bool IsOpen() const;
	void Reset();
};

struct WANDBOUNDCORE_API FWBPlayerStateData
{
	int32 PlayerId = -1;
	int32 HeroUnitId = -1;
	int32 WallsLeft = 0;
	int32 WallRemovalsLeft = 0;
	int32 RemainingMP = 0;
	int32 LastMPRoll = 0;
	TArray<FString> Deck;
	TArray<FString> Hand;
	TArray<FString> Discard;
};

enum class EWBAttackContinuationStage : uint8
{
	None,
	PreHit,
	Damage,
	PostHit,
	Counter,
	Complete,
	CalculateDamage,
	SubstituteDamage,
	ApplyDamage,
	CounterEligibility,
	AfterDamage,
	AutomaticPreDamageModifiers
};

enum class EWBAttackAuthorityKind : uint8
{
	Player,
	NeutralNPC
};

struct WANDBOUNDCORE_API FWBPendingAttackState
{
	struct FDamageCalculation
	{
		bool bValid = false;
		int32 HitUnitId = INDEX_NONE;
		int32 RawAttackDamage = 0;
		int32 PreviousHP = 0;
		int32 PreviousArmor = 0;
		int32 CalculatedArmor = 0;
		int32 ArmorAbsorbedAmount = 0;
		int32 CalculatedHPDamage = 0;
		bool bFrozenBreak = false;
		bool bPrevented = false;
	};

	struct FDamageSubstitution
	{
		bool bActive = false;
		int32 ProtectedUnitId = INDEX_NONE;
		int32 SubstituteUnitId = INDEX_NONE;
	};

	bool bActive = false;
	EWBAttackAuthorityKind AuthorityKind = EWBAttackAuthorityKind::Player;
	EWBAttackContinuationStage Stage = EWBAttackContinuationStage::None;
	int32 AttackerUnitId = -1;
	int32 DefenderUnitId = -1;
	int32 OriginalAttackerUnitId = -1;
	int32 OriginalDefenderUnitId = -1;
	FDamageCalculation DamageCalculation;
	FDamageSubstitution DamageSubstitution;
	int32 FinalDamageRecipientUnitId = INDEX_NONE;
	int32 AttackingPlayerId = -1;
	FWBTile AttackerTile;
	FWBTile DefenderTile;
	FString DeclarationActionId;
	FString ContinuationId;
	EWBDeclarationProvenance AttackDeclaration = EWBDeclarationProvenance::Automatic;
	EWBDeclarationProvenance TargetDeclaration = EWBDeclarationProvenance::Automatic;
	bool bPrevented = false;
	bool bDamageResolved = false;
	bool bPostHitCompleted = false;
	bool bFrozenBroken = false;
	bool bCounter = false;
	bool bAutomaticPreDamageModifiersProcessed = false;
	bool bPendingBattleHitReflectedToAttacker = false;
	bool bCounterSuppressedByPendingHitTransform = false;
	int32 PendingHitTransformSourceUnitId = INDEX_NONE;
	int32 RawDamageModifier = 0;
};

struct WANDBOUNDCORE_API FWBNPCPhaseContinuationState
{
	bool bActive = false;
	int32 PhaseOwnerPlayerId = -1;
	TArray<int32> OrderedNPCUnitIds;
	int32 QueueIndex = 0;
	int32 CurrentNPCUnitId = INDEX_NONE;
	int32 CurrentActionSequence = INDEX_NONE;
	int32 CurrentPathStepIndex = 0;
	bool bCurrentNPCStarted = false;
	bool bCurrentNPCMadeProgress = false;
	bool bWaitingForAttackContinuation = false;

	void Reset()
	{
		*this = FWBNPCPhaseContinuationState();
	}
};

struct WANDBOUNDCORE_API FWBPendingNPCSpawnState
{
	int32 PendingSpawnId = -1;
	int32 SourceMarkerId = -1;
	int32 MarkerOwnerPlayerId = -1;
	FString NPCDefinitionId;
	FWBTile OriginTile;
	int32 SpawnOrder = INDEX_NONE;
	int32 TriggeredByUnitId = -1;
	int32 TriggeredByOwnerId = -1;
	int32 CreatedTurnNumber = -1;
	int32 RetryCount = 0;
};

enum class EWBUnitDestructionCause : uint8
{
	Unknown,
	BattleDamage,
	EffectDamage,
	StatusDamage,
	ExplicitDestroy,
	ReplacementEffect
};

struct WANDBOUNDCORE_API FWBPostDestructionObserverSourceSnapshot
{
	int32 SourceUnitId = INDEX_NONE;
	FString SourceCardId;
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	int32 SourceOrder = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBUnitDestructionSnapshot
{
	FString EventId;
	int32 DestroyedUnitId = INDEX_NONE;
	FString DestroyedCardId;
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	FWBTile LastTile;
	bool bWasHero = false;
	EWBUnitDestructionCause Cause = EWBUnitDestructionCause::Unknown;
	int32 BaseRLSnapshot = 0;
	int32 CurrentRLSnapshot = 0;
	int32 RLUsedSnapshot = 0;
	TArray<FWBEquippedCardEntry> EquippedWands;
	bool bCharacterPassiveEligible = false;
	TArray<FWBPostDestructionObserverSourceSnapshot> ObserverSources;
	int32 ResolutionOrder = INDEX_NONE;
	int32 NextTriggerIndex = 0;
	int32 NextObserverTriggerIndex = 0;
};

struct WANDBOUNDCORE_API FWBActivatedEffectSourceSnapshot
{
	int32 SourceUnitId = INDEX_NONE;
	FString SourceCardId;
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	FWBTile SourceTile = FWBTile(-1, -1);
	bool bWasHero = false;
	int32 BaseRLSnapshot = 0;
	int32 CurrentRLSnapshot = 0;
	int32 RLUsedSnapshot = 0;
	TArray<FWBEquippedCardEntry> EquippedWands;
};

enum class EWBMandatoryDeckChoiceOrigin : uint8
{
	Unknown,
	PostDestructionTrigger,
	ActivatedEffectContinuation
};

struct WANDBOUNDCORE_API FWBPendingMandatoryDeckChoiceState
{
	bool bActive = false;
	EWBMandatoryDeckChoiceOrigin Origin =
		EWBMandatoryDeckChoiceOrigin::Unknown;
	FString ChoiceId;
	// Populated only for the legacy post-destruction origin.
	FString DestructionEventId;
	FString TriggerId;
	FString SourceActionId;
	FString SourceEffectFrameId;
	int32 ControllerPlayerId = INDEX_NONE;
	FString RequiredFaction;
	FWBTile DestinationTile = FWBTile(-1, -1);
	// Populated only for the post-destruction origin.
	FWBUnitDestructionSnapshot SourceSnapshot;
	// Populated only for the activated-effect continuation origin.
	FWBActivatedEffectSourceSnapshot ActivatedEffectSourceSnapshot;
	bool bApplyCSNInheritance = false;
	TArray<FString> EligibleCardInstanceIds;
	int32 ResumePriorityPlayerId = INDEX_NONE;
	int32 ResumeMatchPhase = INDEX_NONE;

	void Reset()
	{
		*this = FWBPendingMandatoryDeckChoiceState();
	}
};

struct WANDBOUNDCORE_API FWBGameStateData
{
	int32 CurrentPlayer = 0;
	int32 PriorityPlayer = 0;
	int32 FirstPlayerId = INDEX_NONE;
	int32 TurnNumber = 1;
	EWBGamePhase Phase = EWBGamePhase::NormalTurn;
	bool bInitialSetupInProgress = false;
	bool bSuppressManualReactsDuringInitialHeroSetup = false;
	bool bGameOver = false;
	int32 WinnerPlayerId = -1;
	FWBTerminalOutcome TerminalOutcome;
	TArray<FWBUnitState> Units;
	TArray<FWBWallEdge> Walls;
	FName DefaultTerrainId = FName(TEXT("Normal"));
	TMap<int32, FName> TerrainByTileIndex;
	TArray<FWBPlayerStateData> Players;
	FWBReactionWindowState ReactionWindow;
	FWBPendingAttackState PendingAttack;
	FWBNPCPhaseContinuationState NPCPhaseContinuation;
	TArray<FWBPendingNPCSpawnState> PendingNPCSpawns;
	TArray<FWBUnitDestructionSnapshot> PendingUnitDestructionEvents;
	FWBPendingMandatoryDeckChoiceState PendingMandatoryDeckChoice;
	TMap<int32, TSet<FString>> ActivationUsageKeysThisTurn;
	FWBCardZoneState CardZoneState;

	static bool IsValidPlayerId(int32 PlayerId);
	static int32 TileToIndex(const FWBTile& Tile);
	int32 GetCurrentPlayerId() const;
	int32 GetActionPriorityPlayerId() const;
	const FWBPlayerStateData* GetPlayerById(int32 PlayerId) const;
	FWBPlayerStateData* GetMutablePlayerById(int32 PlayerId);
	const FWBPlayerStateData* GetCurrentPlayer() const;
	FWBPlayerStateData* GetMutableCurrentPlayer();
	const FWBCardZoneState& GetCardZoneState() const;
	FWBCardZoneState& GetMutableCardZoneStateForTest();
	void ClearCardZoneStateForTest();
	TArray<const FWBUnitState*> GetUnitsForPlayer(int32 PlayerId) const;
	TArray<FWBUnitState*> GetMutableUnitsForPlayer(int32 PlayerId);
	TArray<const FWBUnitState*> GetUnitsControlledByPlayer(int32 PlayerId) const;
	TArray<FWBUnitState*> GetMutableUnitsControlledByPlayer(int32 PlayerId);
	TArray<const FWBUnitState*> GetUnitsOwnedByPlayer(int32 PlayerId) const;
	bool IsNormalTurnPhase() const;
	bool IsResponsePhase() const;
	bool HasOpenReactionWindow() const;
	void ClearReactionWindow();
	void AdvanceTurnBasic();
	bool ResetActionResourcesForPlayer(int32 PlayerId, FString& OutReason);
	bool ApplyTurnStartMPRollForPlayer(int32 PlayerId, int32 ExplicitMPRoll, FString& OutReason);
	bool ResetTurnStartResourcesForPlayer(int32 PlayerId, FString& OutReason);
	bool ApplyTurnStartResourceSetupForPlayer(int32 PlayerId, int32 ExplicitMPRoll, FString& OutReason);
	bool HasActivationUsageKeyThisTurn(int32 PlayerId, const FString& Key) const;
	void MarkActivationUsageKeyForTest(int32 PlayerId, const FString& Key);
	void ClearActivationUsageKeysForPlayer(int32 PlayerId);
	bool HasPendingAttack() const;
	void ClearPendingAttack();
	bool HasPendingMandatoryDeckChoice() const;
	void ClearPendingMandatoryDeckChoice();
	void SetPendingAttackForTest(const FWBPendingAttackState& InPendingAttack);
	const FWBUnitState* GetUnitById(int32 UnitId) const;
	FWBUnitState* GetMutableUnitById(int32 UnitId);
	int32 UnitIdAt(const FWBTile& Tile) const;
	bool IsTileOccupied(const FWBTile& Tile) const;
	bool AddUnitForTest(const FWBUnitState& Unit);
	void AddWallForTest(const FWBWallEdge& Edge);
	bool RemoveWallForTest(const FWBWallEdge& Edge);
	FName GetTerrainAt(const FWBTile& Tile) const;
	bool SetTerrainAt(const FWBTile& Tile, FName TerrainId);
	void SetTerrainForTest(const FWBTile& Tile, FName TerrainId);
	void ClearTerrainForTest(const FWBTile& Tile);
};
