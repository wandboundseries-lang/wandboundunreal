#include "WBMinimumCanonicalMatchPlanner.h"

namespace
{
bool IsEligibleDefinitionStatus(const EWBMinimumDefinitionStatus Status)
{
	switch (Status)
	{
	case EWBMinimumDefinitionStatus::AlreadyTransferred:
	case EWBMinimumDefinitionStatus::EligibleBehaviorFree:
	case EWBMinimumDefinitionStatus::EligibleExistingPayload:
	case EWBMinimumDefinitionStatus::EligibleExistingEquip:
	case EWBMinimumDefinitionStatus::EligibleExistingActivation:
		return true;
	default:
		return false;
	}
}

void AddBlockingReason(
	TArray<FString>& BlockingReasons,
	const FString& Reason)
{
	if (!BlockingReasons.Contains(Reason))
	{
		BlockingReasons.Add(Reason);
	}
}
}

EWBMinimumHeroStatus WBMinimumCanonicalMatchPlanner::ClassifyHero(
	const FWBMinimumHeroEvidence& Evidence)
{
	if (!Evidence.bDefinitionPresent)
	{
		return EWBMinimumHeroStatus::MissingDefinition;
	}
	if (Evidence.EvidenceType == EWBMinimumHeroEvidenceType::NarrativeOnly
		|| Evidence.EvidenceType == EWBMinimumHeroEvidenceType::DevelopmentOnly
		|| Evidence.EvidenceType == EWBMinimumHeroEvidenceType::ModelOnly)
	{
		return EWBMinimumHeroStatus::NotAHero;
	}
	if (Evidence.EvidenceType == EWBMinimumHeroEvidenceType::Ambiguous)
	{
		return EWBMinimumHeroStatus::Ambiguous;
	}
	return Evidence.bBehaviorSupported
		? EWBMinimumHeroStatus::CanonicalHero
		: EWBMinimumHeroStatus::UnsupportedBehavior;
}

EWBMinimumDefinitionStatus WBMinimumCanonicalMatchPlanner::ClassifyDefinition(
	const FWBMinimumDefinitionFacts& Facts)
{
	if (Facts.bAlreadyTransferred)
	{
		return EWBMinimumDefinitionStatus::AlreadyTransferred;
	}
	if (Facts.bCanonicalAmbiguity)
	{
		return EWBMinimumDefinitionStatus::BlockedCanonicalAmbiguity;
	}
	if (Facts.bMissingReference)
	{
		return EWBMinimumDefinitionStatus::BlockedMissingReference;
	}
	if (Facts.bSchemaMismatch)
	{
		return EWBMinimumDefinitionStatus::BlockedSchemaMismatch;
	}
	if (Facts.bUnsupportedPassive)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedPassive;
	}
	if (Facts.bUnsupportedReact)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedReact;
	}
	if (Facts.bUnsupportedTiming)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedTiming;
	}
	if (Facts.bUnsupportedTargeting)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedTargeting;
	}
	if (Facts.bUnsupportedMovement)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedMovement;
	}
	if (Facts.bUnsupportedTerrain)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedTerrain;
	}
	if (Facts.bUnsupportedEffect || !Facts.bCompleteSemanticMapping)
	{
		return EWBMinimumDefinitionStatus::BlockedUnsupportedEffect;
	}
	if (Facts.bBehaviorFree)
	{
		return EWBMinimumDefinitionStatus::EligibleBehaviorFree;
	}
	if (Facts.bExistingEquipSupported)
	{
		return EWBMinimumDefinitionStatus::EligibleExistingEquip;
	}
	if (Facts.bExistingActivationSupported)
	{
		return EWBMinimumDefinitionStatus::EligibleExistingActivation;
	}
	if (Facts.bExistingPayloadSupported)
	{
		return EWBMinimumDefinitionStatus::EligibleExistingPayload;
	}
	return EWBMinimumDefinitionStatus::BlockedUnsupportedEffect;
}

FWBMinimumCanonicalMatchPlan WBMinimumCanonicalMatchPlanner::BuildPlan(
	const FWBMinimumCanonicalMatchInput& Input)
{
	FWBMinimumCanonicalMatchPlan Plan;

	TArray<FString> CanonicalHeroes;
	for (const FWBMinimumHeroEvidence& Evidence : Input.HeroEvidence)
	{
		const EWBMinimumHeroStatus Status = ClassifyHero(Evidence);
		if (Status == EWBMinimumHeroStatus::CanonicalHero)
		{
			CanonicalHeroes.AddUnique(Evidence.DefinitionId);
		}
		else if (Status == EWBMinimumHeroStatus::Ambiguous)
		{
			AddBlockingReason(
				Plan.BlockingReasons,
				TEXT("production_match_spec_blocked_by_hero_evidence"));
		}
		else if (Status == EWBMinimumHeroStatus::UnsupportedBehavior)
		{
			AddBlockingReason(
				Plan.BlockingReasons,
				TEXT("production_match_spec_blocked_by_unsupported_definitions"));
		}
	}
	CanonicalHeroes.Sort();
	if (CanonicalHeroes.Num() < 2)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("production_match_spec_blocked_by_hero_evidence"));
	}
	else
	{
		Plan.RequiredHeroIds.Append(CanonicalHeroes.GetData(), 2);
	}

	const FWBMinimumDeckRuleEvidence& Rules = Input.DeckRules;
	if (Rules.bHasConflictingRules)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_deck_rules_conflicting"));
	}
	if (!Rules.bDeckSizeCanonicalExplicit || Rules.RequiredDeckSize <= 0)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_deck_size_missing"));
	}
	if (!Rules.bAllowedCardTypesCanonicalExplicit
		|| Rules.AllowedCardTypes.IsEmpty())
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_allowed_card_types_missing"));
	}
	if (!Rules.bDuplicateLimitCanonicalExplicit
		|| Rules.MaximumCopiesPerDefinition <= 0)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_duplicate_limit_missing"));
	}
	if (!Rules.bDeckOrderingCanonicalExplicit)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_deck_ordering_missing"));
	}
	if (!Rules.bIdenticalDecksCanonicalExplicit)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_identical_deck_policy_missing"));
	}
	if (!Rules.bFirstPlayerPolicyCanonicalExplicit)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_first_player_policy_missing"));
	}
	if (!Rules.bStartingHandCanonicalExplicit || Rules.StartingHandSize <= 0)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_starting_hand_rule_missing"));
	}
	if (!Rules.bMarkerSetupCanonicalExplicit)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("canonical_marker_setup_rule_missing"));
	}

	Plan.RequiredDeckSize = Rules.RequiredDeckSize;
	if (!Plan.BlockingReasons.IsEmpty())
	{
		Plan.BlockingReasons.Sort();
		return Plan;
	}

	TArray<FWBMinimumDefinitionCandidate> EligibleDefinitions =
		Input.Definitions.FilterByPredicate(
			[&Plan, &Rules](const FWBMinimumDefinitionCandidate& Candidate)
			{
				return !Candidate.bTestFixture
					&& !Candidate.bDevelopmentOnly
					&& IsEligibleDefinitionStatus(Candidate.Status)
					&& Rules.AllowedCardTypes.Contains(Candidate.Type)
					&& !Plan.RequiredHeroIds.Contains(Candidate.DefinitionId);
			});
	EligibleDefinitions.Sort(
		[](const FWBMinimumDefinitionCandidate& Left,
			const FWBMinimumDefinitionCandidate& Right)
		{
			return Left.DefinitionId < Right.DefinitionId;
		});

	int32 RemainingCopies = Rules.RequiredDeckSize;
	for (const FWBMinimumDefinitionCandidate& Candidate : EligibleDefinitions)
	{
		if (RemainingCopies <= 0)
		{
			break;
		}
		FWBMinimumDefinitionCopyRequirement Requirement;
		Requirement.DefinitionId = Candidate.DefinitionId;
		Requirement.CopiesPerDeck = FMath::Min(
			RemainingCopies,
			Rules.MaximumCopiesPerDefinition);
		Plan.RequiredCopies.Add(Requirement);
		RemainingCopies -= Requirement.CopiesPerDeck;
		Plan.RequiredDefinitionIds.AddUnique(Candidate.DefinitionId);
		if (Candidate.Status
			!= EWBMinimumDefinitionStatus::AlreadyTransferred)
		{
			Plan.DefinitionsToTransfer.AddUnique(Candidate.DefinitionId);
		}
	}
	if (RemainingCopies > 0)
	{
		AddBlockingReason(
			Plan.BlockingReasons,
			TEXT("production_match_spec_blocked_by_unsupported_definitions"));
		Plan.RequiredCopies.Reset();
		Plan.RequiredDefinitionIds.Reset();
		Plan.DefinitionsToTransfer.Reset();
		return Plan;
	}

	for (const FString& HeroId : Plan.RequiredHeroIds)
	{
		Plan.RequiredDefinitionIds.AddUnique(HeroId);
		const FWBMinimumDefinitionCandidate* HeroDefinition =
			Input.Definitions.FindByPredicate(
				[&HeroId](const FWBMinimumDefinitionCandidate& Candidate)
				{
					return Candidate.DefinitionId == HeroId;
				});
		if (HeroDefinition == nullptr
			|| !IsEligibleDefinitionStatus(HeroDefinition->Status)
			|| HeroDefinition->bTestFixture
			|| HeroDefinition->bDevelopmentOnly)
		{
			AddBlockingReason(
				Plan.BlockingReasons,
				TEXT("production_match_spec_blocked_by_unsupported_definitions"));
		}
		else if (HeroDefinition->Status
			!= EWBMinimumDefinitionStatus::AlreadyTransferred)
		{
			Plan.DefinitionsToTransfer.AddUnique(HeroId);
		}
	}

	Plan.RequiredDefinitionIds.Sort();
	Plan.DefinitionsToTransfer.Sort();
	Plan.BlockingReasons.Sort();
	Plan.bCanBuildCanonicalMatch = Plan.BlockingReasons.IsEmpty();
	return Plan;
}
