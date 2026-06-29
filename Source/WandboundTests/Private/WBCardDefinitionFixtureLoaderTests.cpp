#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardDefinitionFixtureLoader.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString CardDefinitionFixtureDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("CardDefinitionRepositoryFixtures"));
}

FString CardDefinitionFixturePath(const FString& Filename)
{
	return FPaths::Combine(CardDefinitionFixtureDir(), Filename);
}

FString CardDefinitionExpectedExportPath(const FString& Filename)
{
	return FPaths::Combine(CardDefinitionFixtureDir(), TEXT("ExpectedExports"), Filename);
}

FWBCardDefinitionFixtureLoadResult LoadCardDefinitionFixture(const FString& Filename)
{
	return WBCardDefinitionFixtureLoader::LoadRepositoryFromFile(CardDefinitionFixturePath(Filename));
}

FString ExportFixtureResult(const FWBCardDefinitionFixtureLoadResult& Result)
{
	FString ExportJson;
	WBCardDefinitionFixtureLoader::FixtureLoadResultToJsonStringForTest(Result, ExportJson);
	return ExportJson;
}

FString LoadExpectedFixtureExport(const FString& Filename)
{
	FString Json;
	FFileHelper::LoadFileToString(Json, *CardDefinitionExpectedExportPath(Filename));
	Json.TrimStartAndEndInline();
	return Json;
}

bool HasFixtureDiagnostic(const FWBCardDefinitionFixtureLoadResult& Result, const FString& Code)
{
	for (const FWBCardDefinitionFixtureLoadDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Code == Code)
		{
			return true;
		}
	}
	return false;
}

FString LoadProjectSourceText(const FString& RelativePath)
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

FWBPlayerStateData MakeFixtureLoaderPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeFixtureLoaderUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_loader_unit_%d"), UnitId);
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

FWBGameStateData MakeFixtureLoaderState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeFixtureLoaderPlayer(0));
	State.Players.Add(MakeFixtureLoaderPlayer(1, 0));
	State.AddUnitForTest(MakeFixtureLoaderUnit(1, 0, FWBTile(4, 4)));
	State.AddUnitForTest(MakeFixtureLoaderUnit(2, 1, FWBTile(5, 4)));
	return State;
}

FWBCardDefinitionFixtureLoadResult LoadFixtureJsonString(const FString& Json)
{
	return WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(Json, TEXT("inline_metadata_fixture.json"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderValidDamageTest, "Wandbound.Core.CardDefinitionFixtureLoader.ValidDamageRepositoryLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderValidDamageTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_damage.json"));
	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}
	TestTrue(TEXT("Repository validates"), WBCardDefinitionRepository::ValidateRepository(Result.Repository).bOk);
	TestEqual(TEXT("Card count"), Result.Repository.Definitions.Num(), 1);
	TestEqual(TEXT("Expected export"), ExportFixtureResult(Result), LoadExpectedFixtureExport(TEXT("valid_fixture_repository_damage.export.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderValidHealTest, "Wandbound.Core.CardDefinitionFixtureLoader.ValidHealRepositoryLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderValidHealTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_heal.json"));
	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}
	TestEqual(TEXT("Operation"), Result.Repository.Definitions[0].ActivatedEffects[0].Payloads[0].Operation, EWBGenericEffectOp::HealEffect);
	TestEqual(TEXT("Heal amount"), Result.Repository.Definitions[0].ActivatedEffects[0].Payloads[0].HealEffect.Amount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderValidBurnStatusTest, "Wandbound.Core.CardDefinitionFixtureLoader.ValidStatusBurnRepositoryLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderValidBurnStatusTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_status_burn.json"));
	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}
	const FWBGenericEffectPayload& Payload = Result.Repository.Definitions[0].ActivatedEffects[0].Payloads[0];
	TestEqual(TEXT("Operation"), Payload.Operation, EWBGenericEffectOp::StatusEffect);
	TestEqual(TEXT("Status op"), Payload.StatusEffect.Operation, EWBStatusEffectOp::ApplyStatus);
	TestEqual(TEXT("Status id"), Payload.StatusEffect.StatusId, FName(TEXT("Burn")));
	TestEqual(TEXT("Duration"), Payload.StatusEffect.Duration, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderValidArmorTest, "Wandbound.Core.CardDefinitionFixtureLoader.ValidArmorRepositoryLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderValidArmorTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_armor.json"));
	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}
	const FWBGenericEffectPayload& Payload = Result.Repository.Definitions[0].ActivatedEffects[0].Payloads[0];
	TestEqual(TEXT("Operation"), Payload.Operation, EWBGenericEffectOp::ArmorEffect);
	TestEqual(TEXT("Armor op"), Payload.ArmorEffect.Operation, EWBArmorEffectOp::RestoreArmorToMax);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderValidMixedTest, "Wandbound.Core.CardDefinitionFixtureLoader.ValidMixedRepositoryOrdersCardIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderValidMixedTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_mixed.json"));
	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}
	const TArray<FString> CardIds = WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Result.Repository);
	TestEqual(TEXT("First card"), CardIds[0], FString(TEXT("fixture_alpha_damage")));
	TestEqual(TEXT("Second card"), CardIds[1], FString(TEXT("fixture_beta_armor")));
	TestEqual(TEXT("Third card"), CardIds[2], FString(TEXT("fixture_zeta_heal")));
	TestEqual(TEXT("Expected export"), ExportFixtureResult(Result), LoadExpectedFixtureExport(TEXT("valid_fixture_repository_mixed.export.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderCharacterStatsTest, "Wandbound.Core.CardDefinitionFixtureLoader.CharacterStatsLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderCharacterStatsTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT("{\"repository_id\":\"metadata_repo\",\"source_version\":\"test_v1\",\"cards\":[{\"card_id\":\"character_alpha\",\"public_name\":\"Brave Student\",\"kind\":\"character\",\"character_stats\":{\"hp\":3,\"atk\":1,\"ar\":1,\"rl\":2}}]}");
	const FWBCardDefinitionFixtureLoadResult Result = LoadFixtureJsonString(Json);

	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}

	TestEqual(TEXT("Kind"), Result.Repository.Definitions[0].Kind, EWBCardDefinitionKind::Character);
	TestEqual(TEXT("HP"), Result.Repository.Definitions[0].CharacterStats.HP, 3);
	TestEqual(TEXT("ATK"), Result.Repository.Definitions[0].CharacterStats.ATK, 1);
	TestEqual(TEXT("AR"), Result.Repository.Definitions[0].CharacterStats.AR, 1);
	TestEqual(TEXT("RL"), Result.Repository.Definitions[0].CharacterStats.RL, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderWandStatsTest, "Wandbound.Core.CardDefinitionFixtureLoader.WandStatsLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderWandStatsTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT("{\"repository_id\":\"metadata_repo\",\"source_version\":\"test_v1\",\"cards\":[{\"card_id\":\"wand_alpha\",\"public_name\":\"Copper Wand\",\"kind\":\"wand\",\"wand_stats\":{\"rr\":2}}]}");
	const FWBCardDefinitionFixtureLoadResult Result = LoadFixtureJsonString(Json);

	TestTrue(TEXT("Load succeeds"), Result.bOk);
	if (!Result.bOk)
	{
		return false;
	}

	TestEqual(TEXT("Kind"), Result.Repository.Definitions[0].Kind, EWBCardDefinitionKind::Wand);
	TestEqual(TEXT("RR"), Result.Repository.Definitions[0].WandStats.RR, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderBadCharacterStatsTest, "Wandbound.Core.CardDefinitionFixtureLoader.BadCharacterStatsFail", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderBadCharacterStatsTest::RunTest(const FString& Parameters)
{
	const FString MissingJson = TEXT("{\"repository_id\":\"metadata_repo\",\"cards\":[{\"card_id\":\"character_alpha\",\"public_name\":\"Brave Student\",\"kind\":\"character\"}]}");
	const FString MalformedJson = TEXT("{\"repository_id\":\"metadata_repo\",\"cards\":[{\"card_id\":\"character_alpha\",\"public_name\":\"Brave Student\",\"kind\":\"character\",\"character_stats\":{\"hp\":\"bad\",\"atk\":1,\"ar\":1,\"rl\":2}}]}");
	const FWBCardDefinitionFixtureLoadResult MissingResult = LoadFixtureJsonString(MissingJson);
	const FWBCardDefinitionFixtureLoadResult MalformedResult = LoadFixtureJsonString(MalformedJson);

	TestFalse(TEXT("Missing stats fail"), MissingResult.bOk);
	TestTrue(TEXT("Missing diagnostic present"), HasFixtureDiagnostic(MissingResult, TEXT("character_stats_malformed")));
	TestFalse(TEXT("Malformed stats fail"), MalformedResult.bOk);
	TestTrue(TEXT("Malformed diagnostic present"), HasFixtureDiagnostic(MalformedResult, TEXT("character_stats_malformed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderBadWandStatsTest, "Wandbound.Core.CardDefinitionFixtureLoader.BadWandStatsFail", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderBadWandStatsTest::RunTest(const FString& Parameters)
{
	const FString MissingJson = TEXT("{\"repository_id\":\"metadata_repo\",\"cards\":[{\"card_id\":\"wand_alpha\",\"public_name\":\"Copper Wand\",\"kind\":\"wand\"}]}");
	const FString MalformedJson = TEXT("{\"repository_id\":\"metadata_repo\",\"cards\":[{\"card_id\":\"wand_alpha\",\"public_name\":\"Copper Wand\",\"kind\":\"wand\",\"wand_stats\":{\"rr\":\"bad\"}}]}");
	const FWBCardDefinitionFixtureLoadResult MissingResult = LoadFixtureJsonString(MissingJson);
	const FWBCardDefinitionFixtureLoadResult MalformedResult = LoadFixtureJsonString(MalformedJson);

	TestFalse(TEXT("Missing stats fail"), MissingResult.bOk);
	TestTrue(TEXT("Missing diagnostic present"), HasFixtureDiagnostic(MissingResult, TEXT("wand_stats_malformed")));
	TestFalse(TEXT("Malformed stats fail"), MalformedResult.bOk);
	TestTrue(TEXT("Malformed diagnostic present"), HasFixtureDiagnostic(MalformedResult, TEXT("wand_stats_malformed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderMissingRepositoryIdTest, "Wandbound.Core.CardDefinitionFixtureLoader.MissingRepositoryIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderMissingRepositoryIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_missing_repository_id.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Diagnostic present"), HasFixtureDiagnostic(Result, TEXT("repository_id_missing")));
	TestEqual(TEXT("Repository empty"), Result.Repository.Definitions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderMissingCardsTest, "Wandbound.Core.CardDefinitionFixtureLoader.MissingCardsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderMissingCardsTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_cards_missing.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Diagnostic present"), HasFixtureDiagnostic(Result, TEXT("cards_missing")));
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("fixture_load_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderDuplicateCardIdTest, "Wandbound.Core.CardDefinitionFixtureLoader.DuplicateCardIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderDuplicateCardIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_duplicate_card_id.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Duplicate diagnostic present"), HasFixtureDiagnostic(Result, TEXT("duplicate_card_id")));
	TestTrue(TEXT("Repository validation diagnostic present"), HasFixtureDiagnostic(Result, TEXT("repository_validation_failed")));
	TestEqual(TEXT("Expected export"), ExportFixtureResult(Result), LoadExpectedFixtureExport(TEXT("invalid_fixture_repository_duplicate_card_id.export.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderMissingEffectIdTest, "Wandbound.Core.CardDefinitionFixtureLoader.MissingEffectIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderMissingEffectIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_missing_effect_id.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Diagnostic present"), HasFixtureDiagnostic(Result, TEXT("effect_id_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderDuplicateEffectIdTest, "Wandbound.Core.CardDefinitionFixtureLoader.DuplicateEffectIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderDuplicateEffectIdTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_duplicate_effect_id.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Duplicate diagnostic present"), HasFixtureDiagnostic(Result, TEXT("duplicate_effect_id")));
	TestTrue(TEXT("Repository validation diagnostic present"), HasFixtureDiagnostic(Result, TEXT("repository_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderUnsupportedPayloadTest, "Wandbound.Core.CardDefinitionFixtureLoader.UnsupportedPayloadFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderUnsupportedPayloadTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_unsupported_payload.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Diagnostic present"), HasFixtureDiagnostic(Result, TEXT("unsupported_payload_type")));
	TestEqual(TEXT("Expected export"), ExportFixtureResult(Result), LoadExpectedFixtureExport(TEXT("invalid_fixture_repository_unsupported_payload.export.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderUnsupportedTimingTest, "Wandbound.Core.CardDefinitionFixtureLoader.UnsupportedTimingFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderUnsupportedTimingTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_unsupported_timing.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Diagnostic present"), HasFixtureDiagnostic(Result, TEXT("unsupported_timing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderBadPublicLabelTest, "Wandbound.Core.CardDefinitionFixtureLoader.BadPublicLabelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderBadPublicLabelTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_bad_public_label.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Public label diagnostic present"), HasFixtureDiagnostic(Result, TEXT("public_label_contains_internal_term")));
	TestTrue(TEXT("Repository validation diagnostic present"), HasFixtureDiagnostic(Result, TEXT("repository_validation_failed")));
	TestEqual(TEXT("Expected export"), ExportFixtureResult(Result), LoadExpectedFixtureExport(TEXT("invalid_fixture_repository_bad_public_label.export.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderInvalidStatusTest, "Wandbound.Core.CardDefinitionFixtureLoader.InvalidStatusFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderInvalidStatusTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_invalid_status.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Diagnostic present"), HasFixtureDiagnostic(Result, TEXT("invalid_status_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderHiddenTokenSafeTest, "Wandbound.Core.CardDefinitionFixtureLoader.HiddenTokenExportSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderHiddenTokenSafeTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_hidden_token_safe.json"));
	const FString ExportJson = ExportFixtureResult(Result);
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Unsupported payload diagnostic present"), HasFixtureDiagnostic(Result, TEXT("unsupported_payload_type")));
	TestFalse(TEXT("Export hides secret token"), ExportJson.Contains(TEXT("SECRET")));
	TestEqual(TEXT("Expected export"), ExportJson, LoadExpectedFixtureExport(TEXT("invalid_fixture_repository_hidden_token_safe.export.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderStringAndFileEquivalentTest, "Wandbound.Core.CardDefinitionFixtureLoader.JsonStringAndFileEquivalent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderStringAndFileEquivalentTest::RunTest(const FString& Parameters)
{
	const FString FixturePath = CardDefinitionFixturePath(TEXT("valid_fixture_repository_damage.json"));
	FString Json;
	FFileHelper::LoadFileToString(Json, *FixturePath);

	const FWBCardDefinitionFixtureLoadResult FileResult = WBCardDefinitionFixtureLoader::LoadRepositoryFromFile(FixturePath);
	const FWBCardDefinitionFixtureLoadResult StringResult = WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(Json, FixturePath);
	TestEqual(TEXT("Exports match"), ExportFixtureResult(FileResult), ExportFixtureResult(StringResult));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderExportDeterministicTest, "Wandbound.Core.CardDefinitionFixtureLoader.ExportDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderExportDeterministicTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_mixed.json"));
	TestEqual(TEXT("Same export twice"), ExportFixtureResult(Result), ExportFixtureResult(Result));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderRepositoryValidationReusedTest, "Wandbound.Core.CardDefinitionFixtureLoader.RepositoryValidationReused", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderRepositoryValidationReusedTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("invalid_fixture_repository_duplicate_effect_id.json"));
	TestFalse(TEXT("Load fails"), Result.bOk);
	TestTrue(TEXT("Duplicate effect diagnostic present"), HasFixtureDiagnostic(Result, TEXT("duplicate_effect_id")));
	TestTrue(TEXT("Repository validation diagnostic present"), HasFixtureDiagnostic(Result, TEXT("repository_validation_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderDoesNotMutateGameStateTest, "Wandbound.Core.CardDefinitionFixtureLoader.DoesNotMutateGameState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderDoesNotMutateGameStateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeFixtureLoaderState();
	const TArray<FString> BeforeIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	const int32 BeforeCurrentPlayer = State.CurrentPlayer;
	const int32 BeforeUnitCount = State.Units.Num();
	const FString LoaderSource = LoadProjectSourceText(TEXT("Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp"));

	LoadCardDefinitionFixture(TEXT("valid_fixture_repository_damage.json"));

	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, BeforeCurrentPlayer);
	TestEqual(TEXT("Unit count unchanged"), State.Units.Num(), BeforeUnitCount);
	TestTrue(TEXT("Legal ids unchanged"), BeforeIds == WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0)));
	TestFalse(TEXT("No direct game state dependency in loader source"), LoaderSource.Contains(TEXT("FWBGameStateData")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderNoActionGenerationTest, "Wandbound.Core.CardDefinitionFixtureLoader.DoesNotGenerateActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderNoActionGenerationTest::RunTest(const FString& Parameters)
{
	const FString LoaderSource = LoadProjectSourceText(TEXT("Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp"));
	TestFalse(TEXT("No activation candidate generator"), LoaderSource.Contains(TEXT("WBCardActivationCandidateGenerator")));
	TestFalse(TEXT("No activation legal action generator"), LoaderSource.Contains(TEXT("WBCardActivationLegalActionGenerator")));
	TestFalse(TEXT("No legal action generation"), LoaderSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No action codec changes"), LoaderSource.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderNoEffectExecutionTest, "Wandbound.Core.CardDefinitionFixtureLoader.DoesNotExecuteEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderNoEffectExecutionTest::RunTest(const FString& Parameters)
{
	const FString LoaderSource = LoadProjectSourceText(TEXT("Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp"));
	TestFalse(TEXT("No effect runner"), LoaderSource.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No effect request execution"), LoaderSource.Contains(TEXT("ApplyEffectRequest")));
	TestFalse(TEXT("No direct damage application"), LoaderSource.Contains(TEXT("ApplyDamageEffect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderCoreOnlyTest, "Wandbound.Core.CardDefinitionFixtureLoader.CoreOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderCoreOnlyTest::RunTest(const FString& Parameters)
{
	const FString RuntimeSource = LoadRuntimeSourceText();
	TestFalse(TEXT("Runtime does not include fixture loader"), RuntimeSource.Contains(TEXT("WBCardDefinitionFixtureLoader")));
	TestFalse(TEXT("Runtime does not own fixture load result"), RuntimeSource.Contains(TEXT("FWBCardDefinitionFixtureLoadResult")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderExistingRepositoryTestsBoundaryTest, "Wandbound.Core.CardDefinitionFixtureLoader.ExistingRepositoryTestsBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderExistingRepositoryTestsBoundaryTest::RunTest(const FString& Parameters)
{
	const FString RepositoryTestsSource = LoadProjectSourceText(TEXT("Source/WandboundTests/Private/WBCardDefinitionRepositoryTests.cpp"));
	const FWBCardDefinitionFixtureLoadResult Result = LoadCardDefinitionFixture(TEXT("valid_fixture_repository_damage.json"));
	TestTrue(TEXT("Existing repository test source present"), RepositoryTestsSource.Contains(TEXT("Wandbound.Core.CardDefinitionRepository.ValidOneCard")));
	TestTrue(TEXT("Loaded repository still validates through repository helper"), WBCardDefinitionRepository::ValidateRepository(Result.Repository).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardDefinitionFixtureLoaderSchemaValidatorsBoundaryTest, "Wandbound.Core.CardDefinitionFixtureLoader.SchemaValidatorsBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardDefinitionFixtureLoaderSchemaValidatorsBoundaryTest::RunTest(const FString& Parameters)
{
	const FString FixtureValidationSource = LoadProjectSourceText(TEXT("Source/WandboundTests/Private/WBCardDBSchemaFixtureValidationTests.cpp"));
	const FString BundleValidationSource = LoadProjectSourceText(TEXT("Source/WandboundTests/Private/WBCardDBBundleSchemaFixtureValidationTests.cpp"));
	const FString LoaderSource = LoadProjectSourceText(TEXT("Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp"));
	TestTrue(TEXT("Schema fixture validation tests still present"), FixtureValidationSource.Contains(TEXT("Wandbound.Core.CardDB")));
	TestTrue(TEXT("Bundle schema fixture validation tests still present"), BundleValidationSource.Contains(TEXT("Wandbound.Core.CardDB")));
	TestFalse(TEXT("Core loader does not depend on test-only validator"), LoaderSource.Contains(TEXT("WBCardDBSchemaFixtureValidator")));
	TestFalse(TEXT("Core loader does not depend on test module"), LoaderSource.Contains(TEXT("WandboundTests")));
	return true;
}

#endif
