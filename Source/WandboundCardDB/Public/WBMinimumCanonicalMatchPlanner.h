#pragma once

#include "CoreMinimal.h"
#include "WBProductionCardDatabase.h"

enum class EWBMinimumHeroEvidenceType : uint8
{
	CanonicalExplicit,
	NarrativeOnly,
	DevelopmentOnly,
	ModelOnly,
	Ambiguous
};

enum class EWBMinimumHeroStatus : uint8
{
	CanonicalHero,
	NotAHero,
	Ambiguous,
	MissingDefinition,
	UnsupportedBehavior
};

enum class EWBMinimumDefinitionStatus : uint8
{
	AlreadyTransferred,
	EligibleBehaviorFree,
	EligibleExistingPayload,
	EligibleExistingEquip,
	EligibleExistingActivation,
	BlockedUnsupportedPassive,
	BlockedUnsupportedReact,
	BlockedUnsupportedTiming,
	BlockedUnsupportedTargeting,
	BlockedUnsupportedMovement,
	BlockedUnsupportedTerrain,
	BlockedUnsupportedEffect,
	BlockedMissingReference,
	BlockedSchemaMismatch,
	BlockedCanonicalAmbiguity
};

struct WANDBOUNDCARDDB_API FWBMinimumHeroEvidence
{
	FString DefinitionId;
	EWBMinimumHeroEvidenceType EvidenceType =
		EWBMinimumHeroEvidenceType::Ambiguous;
	bool bDefinitionPresent = true;
	bool bBehaviorSupported = true;
};

struct WANDBOUNDCARDDB_API FWBMinimumDefinitionFacts
{
	bool bAlreadyTransferred = false;
	bool bBehaviorFree = false;
	bool bCompleteSemanticMapping = true;
	bool bExistingPayloadSupported = false;
	bool bExistingEquipSupported = false;
	bool bExistingActivationSupported = false;
	bool bUnsupportedPassive = false;
	bool bUnsupportedReact = false;
	bool bUnsupportedTiming = false;
	bool bUnsupportedTargeting = false;
	bool bUnsupportedMovement = false;
	bool bUnsupportedTerrain = false;
	bool bUnsupportedEffect = false;
	bool bMissingReference = false;
	bool bSchemaMismatch = false;
	bool bCanonicalAmbiguity = false;
};

struct WANDBOUNDCARDDB_API FWBMinimumDeckRuleEvidence
{
	bool bHasConflictingRules = false;
	bool bDeckSizeCanonicalExplicit = false;
	int32 RequiredDeckSize = 0;
	bool bAllowedCardTypesCanonicalExplicit = false;
	TArray<EWBProductionCardType> AllowedCardTypes;
	bool bDuplicateLimitCanonicalExplicit = false;
	int32 MaximumCopiesPerDefinition = 0;
	bool bDeckOrderingCanonicalExplicit = false;
	bool bIdenticalDecksCanonicalExplicit = false;
	bool bIdenticalDecksAllowed = false;
	bool bFirstPlayerPolicyCanonicalExplicit = false;
	bool bStartingHandCanonicalExplicit = false;
	int32 StartingHandSize = 0;
	bool bMarkerSetupCanonicalExplicit = false;
};

struct WANDBOUNDCARDDB_API FWBMinimumDefinitionCandidate
{
	FString DefinitionId;
	EWBProductionCardType Type = EWBProductionCardType::Unknown;
	EWBMinimumDefinitionStatus Status =
		EWBMinimumDefinitionStatus::BlockedUnsupportedEffect;
	bool bTestFixture = false;
	bool bDevelopmentOnly = false;
};

struct WANDBOUNDCARDDB_API FWBMinimumDefinitionCopyRequirement
{
	FString DefinitionId;
	int32 CopiesPerDeck = 0;
};

struct WANDBOUNDCARDDB_API FWBMinimumCanonicalMatchInput
{
	TArray<FWBMinimumHeroEvidence> HeroEvidence;
	FWBMinimumDeckRuleEvidence DeckRules;
	TArray<FWBMinimumDefinitionCandidate> Definitions;
};

struct WANDBOUNDCARDDB_API FWBMinimumCanonicalMatchPlan
{
	bool bCanBuildCanonicalMatch = false;
	TArray<FString> RequiredHeroIds;
	TArray<FString> RequiredDefinitionIds;
	int32 RequiredDeckSize = 0;
	TArray<FWBMinimumDefinitionCopyRequirement> RequiredCopies;
	TArray<FString> DefinitionsToTransfer;
	TArray<FString> BlockingReasons;
};

class WANDBOUNDCARDDB_API WBMinimumCanonicalMatchPlanner
{
public:
	static EWBMinimumHeroStatus ClassifyHero(
		const FWBMinimumHeroEvidence& Evidence);

	static EWBMinimumDefinitionStatus ClassifyDefinition(
		const FWBMinimumDefinitionFacts& Facts);

	static FWBMinimumCanonicalMatchPlan BuildPlan(
		const FWBMinimumCanonicalMatchInput& Input);
};
