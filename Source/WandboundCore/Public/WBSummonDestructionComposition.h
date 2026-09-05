#pragma once

#include "CoreMinimal.h"
#include "WBCharacterSummon.h"
#include "WBDeathResolution.h"

enum class EWBDestructionSummonInheritancePolicy : uint8
{
	None,
	ApplyCSNInheritance
};

enum class EWBDestructionSummonPendingAttackPolicy : uint8
{
	NormalCleanup,
	PreserveAndRedirect
};

enum class EWBDestructionSummonHeroPolicy : uint8
{
	RejectHero,
	SummonThenCommitTerminal
};

struct WANDBOUNDCORE_API FWBSummonDestructionCompositionRequest
{
	int32 DestructionTargetUnitId = INDEX_NONE;
	EWBUnitDestructionCause DestructionCause =
		EWBUnitDestructionCause::ExplicitDestroy;
	FWBCharacterSummonRequest Summon;
	EWBDestructionSummonInheritancePolicy InheritancePolicy =
		EWBDestructionSummonInheritancePolicy::None;
	EWBDestructionSummonPendingAttackPolicy PendingAttackPolicy =
		EWBDestructionSummonPendingAttackPolicy::NormalCleanup;
	EWBDestructionSummonHeroPolicy HeroPolicy =
		EWBDestructionSummonHeroPolicy::RejectHero;
	FString PendingAttackContinuationId;
	FString TransactionId;
};

struct WANDBOUNDCORE_API FWBSummonDestructionCompositionResult
{
	bool bOk = false;
	FString Reason;
	int32 DestroyedUnitId = INDEX_NONE;
	int32 CreatedUnitId = INDEX_NONE;
	FWBUnitDestructionSnapshot DestructionSnapshot;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBSummonDestructionComposition
{
public:
	static FWBSummonDestructionCompositionResult Apply(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBSummonDestructionCompositionRequest& Request);
};
