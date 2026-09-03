#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneState.h"
#include "WBTypes.h"

struct FWBCardDefinitionRepository;
struct FWBGameStateData;
enum class EWBCardDefinitionKind : uint8;

enum class EWBPrivateCardChoiceTiming : uint8
{
	Unknown,
	ActivationDeclaration,
	ResolutionContinuation
};

enum class EWBPrivateCardChoiceRequirement : uint8
{
	Mandatory,
	Optional
};

enum class EWBPrivateCardChoiceContinuationKind : uint8
{
	Unknown,
	PostDestructionTrigger,
	ActivatedEffectContinuation
};

struct WANDBOUNDCORE_API FWBPrivateCardChoiceFilter
{
	EWBCardDefinitionKind RequiredKind =
		static_cast<EWBCardDefinitionKind>(0);
	FString RequiredFaction;
	FString RequiredCardId;
};

struct WANDBOUNDCORE_API FWBPrivateCardChoiceDescriptor
{
	FString ChoiceId;
	int32 ChoosingPlayerId = INDEX_NONE;
	EWBCardZone SourceZone = EWBCardZone::Unknown;
	EWBPrivateCardChoiceTiming Timing = EWBPrivateCardChoiceTiming::Unknown;
	EWBPrivateCardChoiceRequirement Requirement =
		EWBPrivateCardChoiceRequirement::Mandatory;
	EWBDeclarationProvenance TargetDeclaration =
		EWBDeclarationProvenance::Automatic;
	EWBPrivateCardChoiceContinuationKind ContinuationKind =
		EWBPrivateCardChoiceContinuationKind::Unknown;
	FString SourceActionId;
	FString SourceEffectFrameId;
	FWBPrivateCardChoiceFilter Filter;
	TArray<FString> FrozenCandidateInstanceIds;
	int32 ResumePriorityPlayerId = INDEX_NONE;
	int32 ResumeMatchPhase = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBPrivateCardChoiceCandidate
{
	FString CardInstanceId;
	FString CardId;
	int32 ZoneIndex = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBPrivateCardChoiceCandidateResult
{
	bool bOk = false;
	FString Reason;
	TArray<FWBPrivateCardChoiceCandidate> Candidates;
};

struct WANDBOUNDCORE_API FWBPrivateCardChoiceSelectionResult
{
	bool bOk = false;
	FString Reason;
	FWBPrivateCardChoiceCandidate Selected;
};

class WANDBOUNDCORE_API WBPrivateCardChoice
{
public:
	static bool IsSupportedPrivateZone(EWBCardZone Zone);

	static FWBPrivateCardChoiceCandidateResult EnumerateCandidates(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBPrivateCardChoiceDescriptor& Descriptor);

	static FWBPrivateCardChoiceCandidateResult FreezeCandidates(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		FWBPrivateCardChoiceDescriptor& InOutDescriptor);

	static FWBPrivateCardChoiceSelectionResult ValidateSelection(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBPrivateCardChoiceDescriptor& Descriptor,
		const FString& CardInstanceId,
		bool bRequireFrozenCandidate);
};
