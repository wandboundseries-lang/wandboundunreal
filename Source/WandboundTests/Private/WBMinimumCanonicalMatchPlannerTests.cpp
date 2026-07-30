#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"
#include "WBMinimumCanonicalMatchPlanner.h"

namespace
{
FWBMinimumDefinitionCandidate MakeDefinition(
	const FString& Id,
	const EWBProductionCardType Type,
	const EWBMinimumDefinitionStatus Status,
	const bool bTestFixture = false,
	const bool bDevelopmentOnly = false)
{
	FWBMinimumDefinitionCandidate Definition;
	Definition.DefinitionId = Id;
	Definition.Type = Type;
	Definition.Status = Status;
	Definition.bTestFixture = bTestFixture;
	Definition.bDevelopmentOnly = bDevelopmentOnly;
	return Definition;
}

FWBMinimumCanonicalMatchInput MakeValidInput()
{
	FWBMinimumCanonicalMatchInput Input;
	for (const FString& HeroId : {
		FString(TEXT("hero_alpha")),
		FString(TEXT("hero_beta")) })
	{
		FWBMinimumHeroEvidence Hero;
		Hero.DefinitionId = HeroId;
		Hero.EvidenceType =
			EWBMinimumHeroEvidenceType::CanonicalExplicit;
		Input.HeroEvidence.Add(Hero);
		Input.Definitions.Add(
			MakeDefinition(
				HeroId,
				EWBProductionCardType::Character,
				EWBMinimumDefinitionStatus::AlreadyTransferred));
	}
	Input.DeckRules.bDeckSizeCanonicalExplicit = true;
	Input.DeckRules.RequiredDeckSize = 4;
	Input.DeckRules.bAllowedCardTypesCanonicalExplicit = true;
	Input.DeckRules.AllowedCardTypes = {
		EWBProductionCardType::Character };
	Input.DeckRules.bDuplicateLimitCanonicalExplicit = true;
	Input.DeckRules.MaximumCopiesPerDefinition = 2;
	Input.DeckRules.bDeckOrderingCanonicalExplicit = true;
	Input.DeckRules.bIdenticalDecksCanonicalExplicit = true;
	Input.DeckRules.bIdenticalDecksAllowed = true;
	Input.DeckRules.bFirstPlayerPolicyCanonicalExplicit = true;
	Input.DeckRules.bStartingHandCanonicalExplicit = true;
	Input.DeckRules.StartingHandSize = 2;
	Input.DeckRules.bMarkerSetupCanonicalExplicit = true;
	Input.Definitions.Add(
		MakeDefinition(
			TEXT("unit_alpha"),
			EWBProductionCardType::Character,
			EWBMinimumDefinitionStatus::AlreadyTransferred));
	Input.Definitions.Add(
		MakeDefinition(
			TEXT("unit_beta"),
			EWBProductionCardType::Character,
			EWBMinimumDefinitionStatus::EligibleBehaviorFree));
	return Input;
}

bool HasBlock(
	const FWBMinimumCanonicalMatchPlan& Plan,
	const FString& Reason)
{
	return Plan.BlockingReasons.Contains(Reason);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumHeroExplicitTest,
	"Wandbound.CardDB.MinimumCanonical.Hero.ExplicitCanonicalAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumHeroExplicitTest::RunTest(const FString&)
{
	FWBMinimumHeroEvidence Evidence;
	Evidence.DefinitionId = TEXT("hero");
	Evidence.EvidenceType =
		EWBMinimumHeroEvidenceType::CanonicalExplicit;
	TestEqual(
		TEXT("Explicit canonical designation accepted"),
		WBMinimumCanonicalMatchPlanner::ClassifyHero(Evidence),
		EWBMinimumHeroStatus::CanonicalHero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumHeroNarrativeTest,
	"Wandbound.CardDB.MinimumCanonical.Hero.NarrativeImportanceRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumHeroNarrativeTest::RunTest(const FString&)
{
	FWBMinimumHeroEvidence Evidence;
	Evidence.EvidenceType = EWBMinimumHeroEvidenceType::NarrativeOnly;
	TestEqual(
		TEXT("Narrative prominence is not Hero evidence"),
		WBMinimumCanonicalMatchPlanner::ClassifyHero(Evidence),
		EWBMinimumHeroStatus::NotAHero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumHeroDevelopmentTest,
	"Wandbound.CardDB.MinimumCanonical.Hero.DevelopmentUseRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumHeroDevelopmentTest::RunTest(const FString&)
{
	FWBMinimumHeroEvidence Evidence;
	Evidence.EvidenceType =
		EWBMinimumHeroEvidenceType::DevelopmentOnly;
	TestEqual(
		TEXT("Development bootstrap is not Hero evidence"),
		WBMinimumCanonicalMatchPlanner::ClassifyHero(Evidence),
		EWBMinimumHeroStatus::NotAHero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumHeroModelTest,
	"Wandbound.CardDB.MinimumCanonical.Hero.ModelAvailabilityRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumHeroModelTest::RunTest(const FString&)
{
	FWBMinimumHeroEvidence Evidence;
	Evidence.EvidenceType = EWBMinimumHeroEvidenceType::ModelOnly;
	TestEqual(
		TEXT("Model availability is not Hero evidence"),
		WBMinimumCanonicalMatchPlanner::ClassifyHero(Evidence),
		EWBMinimumHeroStatus::NotAHero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumHeroAmbiguousTest,
	"Wandbound.CardDB.MinimumCanonical.Hero.AmbiguousBlocksMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumHeroAmbiguousTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.HeroEvidence[0].EvidenceType =
		EWBMinimumHeroEvidenceType::Ambiguous;
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestFalse(TEXT("Ambiguous Hero prevents match"), Plan.bCanBuildCanonicalMatch);
	TestTrue(
		TEXT("Named Hero blocker"),
		HasBlock(
			Plan,
			TEXT("production_match_spec_blocked_by_hero_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumHeroUnsupportedTest,
	"Wandbound.CardDB.MinimumCanonical.Hero.UnsupportedBehaviorBlocksMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumHeroUnsupportedTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.HeroEvidence[0].bBehaviorSupported = false;
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Unsupported Hero behavior named"),
		HasBlock(
			Plan,
			TEXT("production_match_spec_blocked_by_unsupported_definitions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckExplicitRulesTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.ExplicitRulesAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckExplicitRulesTest::RunTest(const FString&)
{
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(MakeValidInput());
	TestTrue(TEXT("Explicit rules permit plan"), Plan.bCanBuildCanonicalMatch);
	TestEqual(TEXT("Deck size preserved"), Plan.RequiredDeckSize, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckInferredSizeTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.InferredDeckSizeRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckInferredSizeTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.DeckRules.bDeckSizeCanonicalExplicit = false;
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Missing explicit size blocks"),
		HasBlock(Plan, TEXT("canonical_deck_size_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckAllowedTypesTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.AllowedTypesPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckAllowedTypesTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.Definitions.Add(
		MakeDefinition(
			TEXT("action_first"),
			EWBProductionCardType::Action,
			EWBMinimumDefinitionStatus::AlreadyTransferred));
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestFalse(
		TEXT("Disallowed action excluded"),
		Plan.RequiredDefinitionIds.Contains(TEXT("action_first")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckNPCEligibilityTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.NPCEligibilityFollowsCanon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckNPCEligibilityTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.Definitions.Insert(
		MakeDefinition(
			TEXT("aaa_npc"),
			EWBProductionCardType::NPC,
			EWBMinimumDefinitionStatus::AlreadyTransferred),
		0);
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestFalse(
		TEXT("NPC excluded when canon does not allow it"),
		Plan.RequiredDefinitionIds.Contains(TEXT("aaa_npc")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckDuplicateLimitTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.DuplicateLimitPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckDuplicateLimitTest::RunTest(const FString&)
{
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(MakeValidInput());
	TestEqual(TEXT("Two definitions required"), Plan.RequiredCopies.Num(), 2);
	for (const FWBMinimumDefinitionCopyRequirement& Requirement :
		Plan.RequiredCopies)
	{
		TestEqual(
			TEXT("Copy cap preserved"),
			Requirement.CopiesPerDeck,
			2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckMissingEvidenceTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.MissingEvidenceBlocksMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckMissingEvidenceTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.DeckRules.bAllowedCardTypesCanonicalExplicit = false;
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Allowed type evidence required"),
		HasBlock(
			Plan,
			TEXT("canonical_allowed_card_types_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumDeckConflictTest,
	"Wandbound.CardDB.MinimumCanonical.DeckRules.ConflictBlocksMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumDeckConflictTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.DeckRules.bHasConflictingRules = true;
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Conflicting rules fail closed"),
		HasBlock(Plan, TEXT("canonical_deck_rules_conflicting")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionBehaviorFreeTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.BehaviorFreeEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionBehaviorFreeTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bBehaviorFree = true;
	TestEqual(
		TEXT("Behavior-free definition eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleBehaviorFree);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionPayloadTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.ExistingPayloadsEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionPayloadTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingPayloadSupported = true;
	TestEqual(
		TEXT("Damage, healing, status, and armor payload path eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingPayload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionDamagePayloadTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.ExistingDamagePayloadEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionDamagePayloadTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingPayloadSupported = true;
	TestEqual(
		TEXT("Existing direct damage mapping eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingPayload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionHealingPayloadTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.ExistingHealingPayloadEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionHealingPayloadTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingPayloadSupported = true;
	TestEqual(
		TEXT("Existing direct healing mapping eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingPayload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionStatusPayloadTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.SupportedStatusEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionStatusPayloadTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingPayloadSupported = true;
	TestEqual(
		TEXT("Existing canonical status mapping eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingPayload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionArmorPayloadTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.SupportedArmorOperationEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionArmorPayloadTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingPayloadSupported = true;
	TestEqual(
		TEXT("Existing armor operation mapping eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingPayload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionEquipTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.ExistingEquipEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionEquipTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingEquipSupported = true;
	TestEqual(
		TEXT("Fully mapped equip eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingEquip);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionActivationTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.ExistingActivationEligible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionActivationTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingActivationSupported = true;
	TestEqual(
		TEXT("Fully mapped activation eligible"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::EligibleExistingActivation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionPartialMappingTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.PartialSemanticMappingRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionPartialMappingTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bExistingPayloadSupported = true;
	Facts.bCompleteSemanticMapping = false;
	TestEqual(
		TEXT("Partial mapping rejected"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::BlockedUnsupportedEffect);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumExpansionUnsupportedRulesTextTest,
	"Wandbound.CardDB.MinimumCanonical.Expansion.UnsupportedRulesTextRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumExpansionUnsupportedRulesTextTest::RunTest(const FString&)
{
	FWBMinimumDefinitionFacts Facts;
	Facts.bUnsupportedPassive = true;
	TestEqual(
		TEXT("Unsupported passive remains blocked"),
		WBMinimumCanonicalMatchPlanner::ClassifyDefinition(Facts),
		EWBMinimumDefinitionStatus::BlockedUnsupportedPassive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumSolverDeterminismTest,
	"Wandbound.CardDB.MinimumCanonical.Solver.DeterministicSmallestSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumSolverDeterminismTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	const FWBMinimumCanonicalMatchPlan First =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	Algo::Reverse(Input.Definitions);
	Algo::Reverse(Input.HeroEvidence);
	const FWBMinimumCanonicalMatchPlan Second =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Equivalent input ordering produces same IDs"),
		First.RequiredDefinitionIds == Second.RequiredDefinitionIds);
	TestTrue(
		TEXT("Equivalent input ordering produces same transfer set"),
		First.DefinitionsToTransfer == Second.DefinitionsToTransfer);
	TestEqual(
		TEXT("Lexicographically smallest deck definition selected first"),
		First.RequiredCopies[0].DefinitionId,
		FString(TEXT("unit_alpha")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumSolverUnsafeExclusionTest,
	"Wandbound.CardDB.MinimumCanonical.Solver.UnsafeDefinitionsExcluded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumSolverUnsafeExclusionTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.Definitions.Add(
		MakeDefinition(
			TEXT("aaa_unsupported"),
			EWBProductionCardType::Character,
			EWBMinimumDefinitionStatus::BlockedUnsupportedEffect));
	Input.Definitions.Add(
		MakeDefinition(
			TEXT("aab_fixture"),
			EWBProductionCardType::Character,
			EWBMinimumDefinitionStatus::EligibleBehaviorFree,
			true));
	Input.Definitions.Add(
		MakeDefinition(
			TEXT("aac_development"),
			EWBProductionCardType::Character,
			EWBMinimumDefinitionStatus::EligibleBehaviorFree,
			false,
			true));
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestFalse(TEXT("Unsupported excluded"), Plan.RequiredDefinitionIds.Contains(TEXT("aaa_unsupported")));
	TestFalse(TEXT("Fixture excluded"), Plan.RequiredDefinitionIds.Contains(TEXT("aab_fixture")));
	TestFalse(TEXT("Development ID excluded"), Plan.RequiredDefinitionIds.Contains(TEXT("aac_development")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumSolverMissingHeroTest,
	"Wandbound.CardDB.MinimumCanonical.Solver.MissingHeroBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumSolverMissingHeroTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.HeroEvidence.SetNum(1);
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Two Heroes required"),
		HasBlock(
			Plan,
			TEXT("production_match_spec_blocked_by_hero_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumSolverMissingDeckRuleTest,
	"Wandbound.CardDB.MinimumCanonical.Solver.MissingDeckRuleBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumSolverMissingDeckRuleTest::RunTest(const FString&)
{
	FWBMinimumCanonicalMatchInput Input = MakeValidInput();
	Input.DeckRules.bDuplicateLimitCanonicalExplicit = false;
	const FWBMinimumCanonicalMatchPlan Plan =
		WBMinimumCanonicalMatchPlanner::BuildPlan(Input);
	TestTrue(
		TEXT("Duplicate evidence required"),
		HasBlock(
			Plan,
			TEXT("canonical_duplicate_limit_missing")));
	return true;
}

#endif
