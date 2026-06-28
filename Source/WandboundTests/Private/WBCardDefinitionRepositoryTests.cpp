#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBGenericEffectPayload MakeRepositoryDamagePayload(const int32 Amount = 2)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeRepositoryHealPayload(const int32 Amount = 2)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBCardEffectDefinition MakeRepositoryEffect(
	const FString& EffectId = TEXT("deal_2"),
	const FString& PublicLabel = TEXT("Deal 2 damage"),
	const EWBCardEffectTargetRequirement TargetRequirement = EWBCardEffectTargetRequirement::Unit)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = TargetRequirement;
	Effect.Payloads.Add(MakeRepositoryDamagePayload());
	return Effect;
}

FWBCardDefinition MakeRepositoryDefinition(
	const FString& CardId = TEXT("card_alpha"),
	const FString& PublicName = TEXT("Card Alpha"),
	const FString& EffectId = TEXT("deal_2"),
	const FString& PublicLabel = TEXT("Deal 2 damage"))
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeRepositoryEffect(EffectId, PublicLabel));
	return Definition;
}

FWBCardDefinitionRepository MakeRepository(
	const TArray<FWBCardDefinition>& Definitions,
	const FString& RepositoryId = TEXT("fixture_repository"),
	const FString& SourceVersion = TEXT("test_v1"))
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = RepositoryId;
	Repository.SourceVersion = SourceVersion;
	Repository.Definitions = Definitions;
	return Repository;
}

FWBPlayerStateData MakeRepositoryPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeRepositoryUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 3;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeRepositoryState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeRepositoryPlayer(0));
	State.Players.Add(MakeRepositoryPlayer(1, 0));
	State.AddUnitForTest(MakeRepositoryUnit(1, 0, FWBTile(4, 4)));
	State.AddUnitForTest(MakeRepositoryUnit(2, 1, FWBTile(5, 4)));
	return State;
}

FWBAction MakeRepositoryCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FString LoadRepositorySourceText(const FString& RelativePath)
{
	FString Text;
	FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
	return Text;
}

FString LoadRuntimeSourceText()
{
	const FString RuntimePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *RuntimePath, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *RuntimePath, TEXT("*.cpp"), true, false);

	Files.Sort();
	FString Combined;
	for (const FString& File : Files)
	{
		FString FileText;
		if (FFileHelper::LoadFileToString(FileText, *File))
		{
			Combined += FileText;
			Combined += TEXT("\n");
		}
	}
	return Combined;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryEmptyIdTest, "Wandbound.Core.CardDefinitionRepository.EmptyRepositoryIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryEmptyIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ MakeRepositoryDefinition() }, TEXT("")));
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("repository_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryOneCardTest, "Wandbound.Core.CardDefinitionRepository.ValidOneCard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryOneCardTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ MakeRepositoryDefinition() }));
	TestTrue(TEXT("Repository valid"), Result.bOk);
	TestEqual(TEXT("Definition count"), Result.DefinitionCount, 1);
	TestTrue(TEXT("Reason empty"), Result.Reason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryMultipleCardsTest, "Wandbound.Core.CardDefinitionRepository.ValidMultipleCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryMultipleCardsTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository({
		MakeRepositoryDefinition(TEXT("card_beta"), TEXT("Card Beta"), TEXT("heal_2"), TEXT("Heal 2 HP")),
		MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha"))
	});
	const FWBCardDefinitionRepositoryValidationResult Result = WBCardDefinitionRepository::ValidateRepository(Repository);
	TestTrue(TEXT("Repository valid"), Result.bOk);
	TestEqual(TEXT("Definition count"), Result.DefinitionCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryDuplicateCardIdTest, "Wandbound.Core.CardDefinitionRepository.DuplicateCardIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryDuplicateCardIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository({
		MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")),
		MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha Copy"), TEXT("heal_2"), TEXT("Heal 2 HP"))
	});
	const FWBCardDefinitionRepositoryValidationResult Result = WBCardDefinitionRepository::ValidateRepository(Repository);
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("duplicate_card_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryEmptyCardIdTest, "Wandbound.Core.CardDefinitionRepository.EmptyCardIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryEmptyCardIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ MakeRepositoryDefinition(TEXT(""), TEXT("Nameless")) }));
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("card_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryMissingPublicNameTest, "Wandbound.Core.CardDefinitionRepository.MissingPublicNameFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryMissingPublicNameTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("")) }));
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("public_name_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryMissingEffectIdTest, "Wandbound.Core.CardDefinitionRepository.MissingEffectIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryMissingEffectIdTest::RunTest(const FString& Parameters)
{
	FWBCardDefinition Definition = MakeRepositoryDefinition();
	Definition.ActivatedEffects[0].EffectId.Reset();
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ Definition }));
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("effect_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryDuplicateEffectIdTest, "Wandbound.Core.CardDefinitionRepository.DuplicateEffectIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryDuplicateEffectIdTest::RunTest(const FString& Parameters)
{
	FWBCardDefinition Definition = MakeRepositoryDefinition();
	FWBCardEffectDefinition DuplicateEffect = MakeRepositoryEffect(TEXT("deal_2"), TEXT("Deal again"));
	DuplicateEffect.Payloads[0] = MakeRepositoryHealPayload();
	Definition.ActivatedEffects.Add(DuplicateEffect);
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ Definition }));
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("duplicate_effect_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryPublicLabelGuardTest, "Wandbound.Core.CardDefinitionRepository.PublicLabelInternalTermFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryPublicLabelGuardTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::ValidateRepository(MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha"), TEXT("deal_2"), TEXT("damage_effect")) }));
	FString ForbiddenTerm;
	TestFalse(TEXT("Repository invalid"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("public_label_contains_internal_term")));
	TestTrue(TEXT("Helper detects EffectRunner"), WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(TEXT("Use EffectRunner"), ForbiddenTerm));
	TestEqual(TEXT("Forbidden term"), ForbiddenTerm, FString(TEXT("EffectRunner")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryContainsCardIdTest, "Wandbound.Core.CardDefinitionRepository.ContainsCardId", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryContainsCardIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository({
		MakeRepositoryDefinition(TEXT("card_beta"), TEXT("Card Beta")),
		MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha"))
	});
	TestTrue(TEXT("Contains alpha"), WBCardDefinitionRepository::ContainsCardId(Repository, TEXT("card_alpha")));
	TestFalse(TEXT("Does not contain missing"), WBCardDefinitionRepository::ContainsCardId(Repository, TEXT("card_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryFindSuccessTest, "Wandbound.Core.CardDefinitionRepository.FindCardByIdSuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryFindSuccessTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryLookupResult Result =
		WBCardDefinitionRepository::FindCardById(MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) }), TEXT("card_alpha"));
	TestTrue(TEXT("Found"), Result.bFound);
	TestEqual(TEXT("Card id"), Result.Definition.CardId, FString(TEXT("card_alpha")));
	TestTrue(TEXT("Reason empty"), Result.Reason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryFindMissingTest, "Wandbound.Core.CardDefinitionRepository.FindCardByIdMissing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryFindMissingTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryLookupResult Result =
		WBCardDefinitionRepository::FindCardById(MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) }), TEXT("card_missing"));
	TestFalse(TEXT("Not found"), Result.bFound);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("card_definition_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryFindEmptyIdTest, "Wandbound.Core.CardDefinitionRepository.EmptyLookupIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryFindEmptyIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepositoryLookupResult Result =
		WBCardDefinitionRepository::FindCardById(MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) }), TEXT(""));
	TestFalse(TEXT("Not found"), Result.bFound);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("card_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryCardIdOrderTest, "Wandbound.Core.CardDefinitionRepository.CardIdsDeterministicOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryCardIdOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository({
		MakeRepositoryDefinition(TEXT("card_zeta"), TEXT("Card Zeta")),
		MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")),
		MakeRepositoryDefinition(TEXT("card_beta"), TEXT("Card Beta"))
	});
	const TArray<FString> OrderedIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Repository);
	TestEqual(TEXT("First"), OrderedIds[0], FString(TEXT("card_alpha")));
	TestEqual(TEXT("Second"), OrderedIds[1], FString(TEXT("card_beta")));
	TestEqual(TEXT("Third"), OrderedIds[2], FString(TEXT("card_zeta")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryDefinitionOrderTest, "Wandbound.Core.CardDefinitionRepository.DefinitionsDeterministicOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryDefinitionOrderTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository({
		MakeRepositoryDefinition(TEXT("card_zeta"), TEXT("Card Zeta")),
		MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha"))
	});
	const TArray<FWBCardDefinition> OrderedDefinitions = WBCardDefinitionRepository::GetDefinitionsInDeterministicOrder(Repository);
	TestEqual(TEXT("First definition"), OrderedDefinitions[0].CardId, FString(TEXT("card_alpha")));
	TestEqual(TEXT("Second definition"), OrderedDefinitions[1].CardId, FString(TEXT("card_zeta")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryBuildCopiesTest, "Wandbound.Core.CardDefinitionRepository.BuildCopiesAndValidates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryBuildCopiesTest::RunTest(const FString& Parameters)
{
	FWBCardDefinitionRepository Repository;
	const TArray<FWBCardDefinition> Definitions = { MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) };
	const FWBCardDefinitionRepositoryValidationResult Result =
		WBCardDefinitionRepository::BuildRepositoryFromDefinitions(TEXT("built_repository"), TEXT("source_v1"), Definitions, Repository);
	TestTrue(TEXT("Build valid"), Result.bOk);
	TestEqual(TEXT("Repository id"), Repository.RepositoryId, FString(TEXT("built_repository")));
	TestEqual(TEXT("Source version"), Repository.SourceVersion, FString(TEXT("source_v1")));
	TestEqual(TEXT("Definition copied"), Repository.Definitions[0].CardId, FString(TEXT("card_alpha")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryInputMutationTest, "Wandbound.Core.CardDefinitionRepository.DoesNotMutateInputDefinitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryInputMutationTest::RunTest(const FString& Parameters)
{
	FWBCardDefinitionRepository Repository;
	TArray<FWBCardDefinition> Definitions = { MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) };
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(TEXT("built_repository"), TEXT("source_v1"), Definitions, Repository);
	Definitions[0].CardId = TEXT("card_mutated");
	Definitions[0].ActivatedEffects[0].EffectId = TEXT("effect_mutated");

	TestEqual(TEXT("Repository card stable"), Repository.Definitions[0].CardId, FString(TEXT("card_alpha")));
	TestEqual(TEXT("Repository effect stable"), Repository.Definitions[0].ActivatedEffects[0].EffectId, FString(TEXT("deal_2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositorySeparateFromGameStateTest, "Wandbound.Core.CardDefinitionRepository.SeparateFromGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositorySeparateFromGameStateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRepositoryState();
	FWBCardDefinitionRepository Repository = MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) });
	const FString GameStateHeader = LoadRepositorySourceText(TEXT("Source/WandboundCore/Public/WBGameStateData.h"));

	TestTrue(TEXT("State exists without repository"), State.GetPlayerById(0) != nullptr);
	TestTrue(TEXT("Repository validates separately"), WBCardDefinitionRepository::ValidateRepository(Repository).bOk);
	TestFalse(TEXT("Game state has no repository field"), GameStateHeader.Contains(TEXT("FWBCardDefinitionRepository")));
	TestFalse(TEXT("Game state has no repository helper"), GameStateHeader.Contains(TEXT("CardDefinitionRepository")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryLegalGenerationUnchangedTest, "Wandbound.Core.CardDefinitionRepository.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRepositoryState();
	const TArray<FString> BeforeIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	const FWBCardDefinitionRepository Repository = MakeRepository({ MakeRepositoryDefinition(TEXT("card_alpha"), TEXT("Card Alpha")) });
	WBCardDefinitionRepository::ValidateRepository(Repository);
	WBCardDefinitionRepository::FindCardById(Repository, TEXT("card_alpha"));
	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	TestTrue(TEXT("Legal IDs unchanged"), BeforeIds == AfterIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("No activation id %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryActionCodecUnchangedTest, "Wandbound.Core.CardDefinitionRepository.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeRepositoryCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeRepositoryCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeRepositoryCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeRepositoryCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositoryRuntimeSourceUnchangedTest, "Wandbound.Core.CardDefinitionRepository.RuntimeSourceUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositoryRuntimeSourceUnchangedTest::RunTest(const FString& Parameters)
{
	const FString RuntimeSource = LoadRuntimeSourceText();
	const FString ProviderHeader = LoadRepositorySourceText(TEXT("Source/WandboundRuntime/Public/WBProductionActivationDataProvider.h"));
	const FString ProviderSource = LoadRepositorySourceText(TEXT("Source/WandboundRuntime/Private/WBProductionActivationDataProvider.cpp"));
	const FString ExecutionHandoffHeader = LoadRepositorySourceText(TEXT("Source/WandboundRuntime/Public/WBProductionActivationExecutionHandoff.h"));
	const FString ExecutionHandoffSource = LoadRepositorySourceText(TEXT("Source/WandboundRuntime/Private/WBProductionActivationExecutionHandoff.cpp"));
	FString RuntimeSourceWithoutProductionProvider = RuntimeSource;
	RuntimeSourceWithoutProductionProvider.ReplaceInline(*ProviderHeader, TEXT(""));
	RuntimeSourceWithoutProductionProvider.ReplaceInline(*ProviderSource, TEXT(""));
	RuntimeSourceWithoutProductionProvider.ReplaceInline(*ExecutionHandoffHeader, TEXT(""));
	RuntimeSourceWithoutProductionProvider.ReplaceInline(*ExecutionHandoffSource, TEXT(""));

	TestTrue(TEXT("Production provider accepts repository input"), ProviderHeader.Contains(TEXT("FWBCardDefinitionRepository")));
	TestTrue(TEXT("Production provider consumes repository helper"), ProviderSource.Contains(TEXT("WBCardDefinitionRepository")));
	TestTrue(TEXT("Production execution handoff accepts repository input"), ExecutionHandoffHeader.Contains(TEXT("FWBCardDefinitionRepository")));
	TestTrue(TEXT("Production execution handoff consumes repository helper"), ExecutionHandoffSource.Contains(TEXT("WBCardDefinitionRepository")));
	TestFalse(TEXT("Runtime outside production provider/handoff does not include repository helper"), RuntimeSourceWithoutProductionProvider.Contains(TEXT("WBCardDefinitionRepository")));
	TestFalse(TEXT("Runtime outside production provider/handoff does not own repository struct"), RuntimeSourceWithoutProductionProvider.Contains(TEXT("FWBCardDefinitionRepository")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionRepositorySchemaValidatorBoundaryTest, "Wandbound.Core.CardDefinitionRepository.SchemaValidatorBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionRepositorySchemaValidatorBoundaryTest::RunTest(const FString& Parameters)
{
	const FString ValidatorSource = LoadRepositorySourceText(TEXT("Source/WandboundTests/Private/WBCardDBSchemaFixtureValidationTests.cpp"));
	const FString RepositorySource = LoadRepositorySourceText(TEXT("Source/WandboundCore/Private/WBCardDefinitionRepository.cpp"));

	TestTrue(TEXT("Existing CardDB schema validator source present"), ValidatorSource.Contains(TEXT("Wandbound.Core.CardDB")));
	TestFalse(TEXT("Repository does not parse JSON"), RepositorySource.Contains(TEXT("Json")));
	TestFalse(TEXT("Repository does not load files"), RepositorySource.Contains(TEXT("FFileHelper")));
	TestFalse(TEXT("Repository does not generate activation candidates"), RepositorySource.Contains(TEXT("WBCardActivationCandidateGenerator")));
	return true;
}

#endif
