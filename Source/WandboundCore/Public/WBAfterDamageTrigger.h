#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBAfterDamageTriggerSourceKind : uint8
{
	Unit,
	EquippedWand
};

struct WANDBOUNDCORE_API FWBAfterDamageEventContext
{
	FWBEventIdentitySnapshot EventIdentity;
	FWBUnitParticipantSnapshot AttackerSnapshot;
	FWBUnitParticipantSnapshot HitUnitSnapshot;
	FWBUnitParticipantSnapshot FinalDamageRecipientSnapshot;
	int32 AttackerUnitId = INDEX_NONE;
	int32 HitUnitId = INDEX_NONE;
	int32 FinalDamageRecipientUnitId = INDEX_NONE;
	int32 AttackerControllerId = INDEX_NONE;
	int32 HitUnitControllerId = INDEX_NONE;
	int32 FinalRecipientControllerId = INDEX_NONE;
	int32 RawAttackDamage = 0;
	int32 ArmorAbsorbedAmount = 0;
	int32 CalculatedHPDamage = 0;
	int32 AppliedHPDamage = 0;
	bool bDamageSubstituted = false;
	bool bPrevented = false;
	bool bFrozenBreak = false;
	bool bCounterAttack = false;
	EWBDeclarationProvenance AttackDeclaration =
		EWBDeclarationProvenance::Automatic;
	EWBDeclarationProvenance TargetDeclaration =
		EWBDeclarationProvenance::Automatic;
	FString DeclarationActionId;
	FString AttackContinuationId;
	int32 HitUnitPreviousHP = 0;
	int32 HitUnitResultingHP = 0;
	int32 FinalRecipientPreviousHP = 0;
	int32 FinalRecipientResultingHP = 0;
};

struct WANDBOUNDCORE_API FWBAfterDamageTriggerInstance
{
	FWBEventSourceSnapshot SourceSnapshot;
	EWBTriggerEligibilityPolicy EligibilityPolicy =
		EWBTriggerEligibilityPolicy::SnapshotAtCollection;
	FString StableTriggerId;
	int32 ControllerPlayerId = INDEX_NONE;
	EWBAfterDamageTriggerSourceKind SourceKind =
		EWBAfterDamageTriggerSourceKind::Unit;
	int32 SourceUnitId = INDEX_NONE;
	FString SourceCardId;
	FString SourceCardInstanceId;
	int32 EquipOrder = INDEX_NONE;
	FWBAfterDamageTriggerDefinition Definition;
};

struct WANDBOUNDCORE_API FWBAfterDamageTriggerCollection
{
	bool bOk = false;
	FString Reason;
	FWBAfterDamageEventContext Context;
	TArray<FWBAfterDamageTriggerInstance> Triggers;
};

struct WANDBOUNDCORE_API FWBAfterDamageTriggerResolutionResult
{
	bool bOk = false;
	FString Reason;
	int32 ResolvedTriggerCount = 0;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBAfterDamageTrigger
{
public:
	static FWBAfterDamageTriggerCollection CaptureBeforeDamage(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository);

	static bool FinalizeContextAfterDamage(
		const FWBGameStateData& State,
		const TArray<FWBTraceEvent>& DamageTraceEvents,
		FWBAfterDamageTriggerCollection& InOutCollection,
		FString& OutReason);

	static FWBAfterDamageTriggerResolutionResult Resolve(
		FWBGameStateData& State,
		const FWBAfterDamageTriggerCollection& Collection);

	static FString BuildUsageKey(
		const FWBAfterDamageTriggerInstance& Trigger,
		const FWBAfterDamageEventContext& Context);
};
