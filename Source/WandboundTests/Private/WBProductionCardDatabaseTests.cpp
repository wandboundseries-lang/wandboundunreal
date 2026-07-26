#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCardDataProvider.h"
#include "WBProductionMatchSpecification.h"
#include "WBProductionRuntimeBootstrap.h"
#include "WBRuntimeMatchHostComponent.h"

namespace
{
FString ProductionFixtureRoot()
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data"),
		TEXT("CardDB"),
		TEXT("TestFixtures"),
		TEXT("ProductionPipeline"));
}

FString ProductionFixturePath(const FString& RelativePath)
{
	return FPaths::Combine(ProductionFixtureRoot(), RelativePath);
}

FWBProductionCardDatabaseLoadResult LoadProductionFixture()
{
	return WBProductionCardDatabase::LoadManifestSuite(
		ProductionFixturePath(TEXT("root_manifest.json")));
}

bool HasDiagnostic(
	const FWBProductionCardDatabaseLoadResult& Result,
	const FString& Code)
{
	for (const FWBProductionCardDBDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Code == Code)
		{
			return true;
		}
	}
	return false;
}

bool HasMatchDiagnostic(
	const FWBProductionMatchSpecificationLoadResult& Result,
	const FString& Code)
{
	for (const FWBProductionCardDBDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Code == Code)
		{
			return true;
		}
	}
	return false;
}

FString ReadFixtureText(const FString& RelativePath)
{
	FString Text;
	FFileHelper::LoadFileToString(Text, *ProductionFixturePath(RelativePath));
	return Text;
}

FString ValidMatchSpecJson(const FWBProductionCardDatabase& Database)
{
	FString Json = ReadFixtureText(TEXT("match_spec.json"));
	Json.ReplaceInline(
		TEXT("DIGEST_PENDING_FIRST_VALIDATION"),
		*Database.ContentDigest);
	return Json;
}

FString MakeSandbox(const FString& TestName)
{
	const FString Sandbox = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("CardDBProductionTests"),
		TestName + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
	IFileManager::Get().MakeDirectory(*Sandbox, true);
	FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(
		*Sandbox,
		*ProductionFixtureRoot(),
		true);
	return Sandbox;
}

bool ReplaceInSandboxFile(
	const FString& Sandbox,
	const FString& RelativePath,
	const FString& From,
	const FString& To)
{
	const FString Path = FPaths::Combine(Sandbox, RelativePath);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)
		|| !Text.Contains(From))
	{
		return false;
	}
	Text.ReplaceInline(*From, *To);
	return FFileHelper::SaveStringToFile(Text, *Path);
}

FWBProductionCardDatabaseLoadResult LoadSandbox(const FString& Sandbox)
{
	return WBProductionCardDatabase::LoadManifestSuite(
		FPaths::Combine(Sandbox, TEXT("root_manifest.json")));
}

bool ReverseCharacterCards(const FString& Sandbox)
{
	const FString Path = FPaths::Combine(
		Sandbox,
		TEXT("bundles/characters.json"));
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(
			TJsonReaderFactory<>::Create(Json),
			Root)
		|| !Root.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* ExistingCards = nullptr;
	if (!Root->TryGetArrayField(TEXT("cards"), ExistingCards)
		|| ExistingCards == nullptr
		|| ExistingCards->Num() != 2)
	{
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> Reversed = *ExistingCards;
	Reversed.Swap(0, 1);
	Root->SetArrayField(TEXT("cards"), Reversed);
	Json.Reset();
	const TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&Json);
	return FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)
		&& FFileHelper::SaveStringToFile(Json, *Path);
}

bool ReadProjectFile(const FString& RelativePath, FString& OutText)
{
	return FFileHelper::LoadFileToString(
		OutText,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBValidBundleTest,
	"Wandbound.CardDB.Production.Schema.ValidBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBValidBundleTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	TestTrue(TEXT("Production fixture loads"), Result.bOk);
	TestTrue(TEXT("Snapshot exists"), Result.Snapshot.IsValid());
	if (Result.Snapshot.IsValid())
	{
		TestEqual(TEXT("Definition count"), Result.Snapshot->Records.Num(), 7);
		TestEqual(TEXT("Digest length"), Result.Snapshot->ContentDigest.Len(), 64);
		AddInfo(FString::Printf(
			TEXT("PRODUCTION_FIXTURE_DIGEST=%s"),
			*Result.Snapshot->ContentDigest));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBUnknownFieldRejectedTest,
	"Wandbound.CardDB.Production.Schema.UnknownFieldRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBUnknownFieldRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("unknown_field"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/characters.json"),
			TEXT("\"schema_version\": 1,"),
			TEXT("\"schema_version\": 1, \"private_owner\": 0,")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Unknown field rejected"), Result.bOk);
	TestTrue(TEXT("Named diagnostic"), HasDiagnostic(Result, TEXT("unknown_field")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBUnknownMetadataFieldRejectedTest,
	"Wandbound.CardDB.Production.Schema.UnknownMetadataFieldRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBUnknownMetadataFieldRejectedTest::RunTest(
	const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("unknown_metadata"));
	TestTrue(
		TEXT("Fixture edited"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("root_manifest.json"),
			TEXT("\"notes\": \"Synthetic production-pipeline validation only.\""),
			TEXT("\"notes\": \"Synthetic production-pipeline validation only.\", \"private_state\": true")));
	const FWBProductionCardDatabaseLoadResult Result =
		LoadSandbox(Sandbox);
	TestFalse(TEXT("Unknown metadata field fails"), Result.bOk);
	TestTrue(
		TEXT("Closed metadata diagnostic"),
		HasDiagnostic(Result, TEXT("unknown_field")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBDuplicateIdRejectedTest,
	"Wandbound.CardDB.Production.Schema.DuplicateIdRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBDuplicateIdRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("duplicate_id"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/characters.json"),
			TEXT("fixture_guard"),
			TEXT("fixture_student")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Duplicate rejected"), Result.bOk);
	TestTrue(
		TEXT("Duplicate diagnostic"),
		HasDiagnostic(Result, TEXT("duplicate_definition_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBMalformedIdRejectedTest,
	"Wandbound.CardDB.Production.Schema.MalformedIdRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBMalformedIdRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("malformed_id"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/characters.json"),
			TEXT("\"card_id\": \"fixture_guard\""),
			TEXT("\"card_id\": \"Fixture-Guard\"")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Malformed id rejected"), Result.bOk);
	TestTrue(
		TEXT("Malformed id diagnostic"),
		HasDiagnostic(Result, TEXT("definition_id_invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBInvalidNumberRejectedTest,
	"Wandbound.CardDB.Production.Schema.InvalidNumberRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBInvalidNumberRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("invalid_number"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/characters.json"),
			TEXT("\"hp\": 10"),
			TEXT("\"hp\": -1")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Invalid number rejected"), Result.bOk);
	TestTrue(
		TEXT("Numeric diagnostic"),
		HasDiagnostic(Result, TEXT("invalid_numeric_range")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBUnsupportedTypeRejectedTest,
	"Wandbound.CardDB.Production.Schema.UnsupportedCardTypeRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBUnsupportedTypeRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("unsupported_type"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/characters.json"),
			TEXT("\"kind\": \"character\""),
			TEXT("\"kind\": \"react_effect\"")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Unsupported type rejected"), Result.bOk);
	TestTrue(
		TEXT("Type diagnostic"),
		HasDiagnostic(Result, TEXT("unsupported_card_type")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBUnsupportedEffectRejectedTest,
	"Wandbound.CardDB.Production.Schema.UnsupportedEffectRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBUnsupportedEffectRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("unsupported_effect"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/wands.json"),
			TEXT("\"type\": \"status_effect\""),
			TEXT("\"type\": \"draw_cards\"")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Unsupported effect rejected"), Result.bOk);
	TestTrue(
		TEXT("Effect diagnostic"),
		HasDiagnostic(Result, TEXT("unsupported_effect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBSourceManifestMismatchTest,
	"Wandbound.CardDB.Production.Schema.SourceManifestMismatchRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBSourceManifestMismatchTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("source_manifest"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/wands.json"),
			TEXT("manifests/equipment.manifest.json"),
			TEXT("manifests/core.manifest.json")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Ownership mismatch rejected"), Result.bOk);
	TestTrue(
		TEXT("Provenance diagnostic"),
		HasDiagnostic(Result, TEXT("source_manifest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBManifestCycleRejectedTest,
	"Wandbound.CardDB.Production.Manifest.CycleRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBManifestCycleRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("cycle"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("manifests/equipment.manifest.json"),
			TEXT("\"batches\": ["),
			TEXT("\"includes\": [{\"name\":\"core\",\"path\":\"manifests/core.manifest.json\"}], \"batches\": [")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Cycle rejected"), Result.bOk);
	TestTrue(
		TEXT("Cycle diagnostic"),
		HasDiagnostic(Result, TEXT("manifest_include_cycle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBDuplicateOwnershipRejectedTest,
	"Wandbound.CardDB.Production.Manifest.DuplicateOwnershipRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBDuplicateOwnershipRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("duplicate_ownership"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("manifests/core.manifest.json"),
			TEXT("\"bundles\": ["),
			TEXT("\"bundles\": [{\"name\":\"wands_duplicate\",\"path\":\"bundles/wands.json\"},")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Duplicate ownership rejected"), Result.bOk);
	TestTrue(
		TEXT("Ownership diagnostic"),
		HasDiagnostic(Result, TEXT("duplicate_manifest_ownership")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBMissingIncludeRejectedTest,
	"Wandbound.CardDB.Production.Manifest.MissingIncludeRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBMissingIncludeRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("missing_include"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("manifests/core.manifest.json"),
			TEXT("manifests/equipment.manifest.json"),
			TEXT("manifests/missing.manifest.json")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Missing include rejected"), Result.bOk);
	TestTrue(
		TEXT("Missing include diagnostic"),
		HasDiagnostic(Result, TEXT("manifest_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBTraversalRejectedTest,
	"Wandbound.CardDB.Production.Manifest.TraversalRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBTraversalRejectedTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("traversal"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("root_manifest.json"),
			TEXT("manifests/core.manifest.json"),
			TEXT("../core.manifest.json")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	TestFalse(TEXT("Traversal rejected"), Result.bOk);
	TestTrue(
		TEXT("Path diagnostic"),
		HasDiagnostic(Result, TEXT("manifest_path_invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBDigestStableTest,
	"Wandbound.CardDB.Production.Manifest.DigestStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBDigestStableTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult A = LoadProductionFixture();
	const FWBProductionCardDatabaseLoadResult B = LoadProductionFixture();
	TestTrue(TEXT("First load"), A.bOk && A.Snapshot.IsValid());
	TestTrue(TEXT("Second load"), B.bOk && B.Snapshot.IsValid());
	if (A.Snapshot.IsValid() && B.Snapshot.IsValid())
	{
		TestEqual(
			TEXT("Digest is stable"),
			A.Snapshot->ContentDigest,
			B.Snapshot->ContentDigest);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBEquivalentOrderTest,
	"Wandbound.CardDB.Production.Manifest.EquivalentOrderStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBEquivalentOrderTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Original = LoadProductionFixture();
	const FString Sandbox = MakeSandbox(TEXT("equivalent_order"));
	TestTrue(TEXT("Card order reversed"), ReverseCharacterCards(Sandbox));
	const FWBProductionCardDatabaseLoadResult Reordered = LoadSandbox(Sandbox);
	TestTrue(TEXT("Original loads"), Original.bOk && Original.Snapshot.IsValid());
	TestTrue(TEXT("Reordered loads"), Reordered.bOk && Reordered.Snapshot.IsValid());
	if (Original.Snapshot.IsValid() && Reordered.Snapshot.IsValid())
	{
		TestEqual(
			TEXT("Equivalent snapshot digest"),
			Original.Snapshot->ContentDigest,
			Reordered.Snapshot->ContentDigest);
		TestEqual(
			TEXT("Equivalent definition order"),
			Original.Snapshot->GetDefinitionIds(),
			Reordered.Snapshot->GetDefinitionIds());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBRegistryTypedLookupTest,
	"Wandbound.CardDB.Production.Registry.TypedLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBRegistryTypedLookupTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	TestTrue(TEXT("Fixture loads"), Result.bOk && Result.Snapshot.IsValid());
	if (Result.Snapshot.IsValid())
	{
		TestNotNull(
			TEXT("Character lookup"),
			Result.Snapshot->FindCharacter(TEXT("fixture_guard")));
		TestNotNull(
			TEXT("Hero lookup"),
			Result.Snapshot->FindHero(TEXT("fixture_hero_alpha")));
		TestNotNull(
			TEXT("Wand lookup"),
			Result.Snapshot->FindWand(TEXT("fixture_ember_wand")));
		TestNull(
			TEXT("Wrong typed lookup fails"),
			Result.Snapshot->FindWand(TEXT("fixture_guard")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBRegistryDeterministicIterationTest,
	"Wandbound.CardDB.Production.Registry.DeterministicIteration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBRegistryDeterministicIterationTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	TestTrue(TEXT("Fixture loads"), Result.bOk && Result.Snapshot.IsValid());
	if (Result.Snapshot.IsValid())
	{
		const TArray<FString> Expected = {
			TEXT("fixture_ember_wand"),
			TEXT("fixture_guard"),
			TEXT("fixture_hero_alpha"),
			TEXT("fixture_hero_beta"),
			TEXT("fixture_npc"),
			TEXT("fixture_student"),
			TEXT("fixture_trap")
		};
		TestEqual(
			TEXT("Definition ids sorted"),
			Result.Snapshot->GetDefinitionIds(),
			Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBRegistryProvenanceTest,
	"Wandbound.CardDB.Production.Registry.ProvenanceRetained",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBRegistryProvenanceTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	const FWBProductionCardRecord* Wand =
		Result.Snapshot.IsValid()
			? Result.Snapshot->FindWand(TEXT("fixture_ember_wand"))
			: nullptr;
	TestNotNull(TEXT("Wand found"), Wand);
	if (Wand != nullptr)
	{
		TestEqual(
			TEXT("Manifest retained"),
			Wand->SourceManifestPath,
			FString(TEXT("manifests/equipment.manifest.json")));
		TestEqual(
			TEXT("Bundle retained"),
			Wand->SourceBundlePath,
			FString(TEXT("bundles/wands.json")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBCharacterStatsPreservedTest,
	"Wandbound.CardDB.Production.Character.StatsAndPatternsPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBCharacterStatsPreservedTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	const FWBProductionCardRecord* Character =
		Result.Snapshot.IsValid()
			? Result.Snapshot->FindCharacter(TEXT("fixture_student"))
			: nullptr;
	TestNotNull(TEXT("Character found"), Character);
	if (Character != nullptr)
	{
		TestEqual(TEXT("HP"), Character->CoreDefinition.CharacterStats.HP, 8);
		TestEqual(TEXT("ATK"), Character->CoreDefinition.CharacterStats.ATK, 3);
		TestEqual(TEXT("AR"), Character->CoreDefinition.CharacterStats.AR, 2);
		TestEqual(TEXT("RL"), Character->CoreDefinition.CharacterStats.RL, 3);
		TestEqual(
			TEXT("Movement"),
			Character->Movement.Pattern,
			FString(TEXT("orthogonal_adjacent")));
		TestEqual(
			TEXT("Attack"),
			Character->Attack.Pattern,
			FString(TEXT("orthogonal_line")));
		TestEqual(TEXT("Attack range"), Character->Attack.Range, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBHeroRoleTest,
	"Wandbound.CardDB.Production.Hero.RoleAndPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBHeroRoleTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	const FWBProductionCardRecord* Hero =
		Result.Snapshot.IsValid()
			? Result.Snapshot->FindHero(TEXT("fixture_hero_alpha"))
			: nullptr;
	TestNotNull(TEXT("Hero found"), Hero);
	if (Hero != nullptr)
	{
		TestTrue(TEXT("Hero role"), Hero->bHeroRole);
		TestEqual(
			TEXT("Placement"),
			Hero->HeroMatchStartPlacement,
			FString(TEXT("canonical_hero_spawn")));
		TestEqual(
			TEXT("Core kind remains Character"),
			Hero->CoreDefinition.Kind,
			EWBCardDefinitionKind::Character);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBWandDataTest,
	"Wandbound.CardDB.Production.Wand.EquipAndActivationPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBWandDataTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	const FWBProductionCardRecord* Wand =
		Result.Snapshot.IsValid()
			? Result.Snapshot->FindWand(TEXT("fixture_ember_wand"))
			: nullptr;
	TestNotNull(TEXT("Wand found"), Wand);
	if (Wand != nullptr)
	{
		TestEqual(TEXT("RR"), Wand->CoreDefinition.WandStats.RR, 1);
		TestEqual(
			TEXT("Equip target"),
			Wand->Equip.TargetRequirement,
			FString(TEXT("owned_unit")));
		TestEqual(
			TEXT("Activation count"),
			Wand->CoreDefinition.ActivatedEffects.Num(),
			1);
		if (!Wand->CoreDefinition.ActivatedEffects.IsEmpty())
		{
			const FWBCardEffectDefinition& Effect =
				Wand->CoreDefinition.ActivatedEffects[0];
			TestEqual(
				TEXT("Equipped source"),
				Effect.SourceGate.RequiredZone,
				EWBCardActivationSourceZone::Equipped);
			TestEqual(
				TEXT("Required RR"),
				Effect.SourceGate.CostGate.RequiredRR,
				1);
			TestTrue(TEXT("Once per turn"), Effect.SourceGate.bOncePerTurn);
			TestEqual(
				TEXT("Target requirement"),
				Effect.TargetRequirement,
				EWBCardEffectTargetRequirement::Unit);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBPublicMetadataTest,
	"Wandbound.CardDB.Production.PublicMetadata.Preserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBPublicMetadataTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	FWBProductionCardDataProvider Provider;
	Provider.Configure(Result.Snapshot);
	FWBProductionPublicCardData PublicData;
	FString Reason;
	TestTrue(
		TEXT("Public lookup"),
		Provider.GetPublicPresentationData(
			TEXT("fixture_student"),
			PublicData,
			Reason));
	TestEqual(
		TEXT("Display name"),
		PublicData.DisplayName,
		FString(TEXT("Fixture Student")));
	TestEqual(
		TEXT("Card type"),
		PublicData.CardType,
		FString(TEXT("Character")));
	TestEqual(
		TEXT("Faction"),
		PublicData.Factions,
		TArray<FString>{ TEXT("csn") });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBProviderSnapshotReuseTest,
	"Wandbound.CardDB.Production.Provider.ImmutableSnapshotReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBProviderSnapshotReuseTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	FWBProductionCardDataProvider Provider;
	Provider.Configure(Result.Snapshot);
	TestTrue(TEXT("Provider configured"), Provider.IsConfigured());
	TestTrue(
		TEXT("Same immutable snapshot"),
		Provider.GetSnapshot() == Result.Snapshot);
	TestEqual(
		TEXT("Digest retained"),
		Provider.GetContentDigest(),
		Result.Snapshot.IsValid()
			? Result.Snapshot->ContentDigest
			: FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBProviderMissingLookupTest,
	"Wandbound.CardDB.Production.Provider.MissingLookupDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBProviderMissingLookupTest::RunTest(const FString&)
{
	FWBProductionCardDataProvider Provider;
	Provider.Configure(LoadProductionFixture().Snapshot);
	const FWBProductionCardDataLookupResult Result =
		Provider.GetCharacterDefinition(TEXT("missing_card"));
	TestFalse(TEXT("Lookup fails"), Result.bOk);
	TestEqual(
		TEXT("Explicit reason"),
		Result.Reason,
		FString(TEXT("card_definition_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionMatchSpecValidTest,
	"Wandbound.CardDB.Production.MatchSpec.ValidBuildsRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionMatchSpecValidTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Database = LoadProductionFixture();
	TestTrue(TEXT("Database loads"), Database.bOk && Database.Snapshot.IsValid());
	if (!Database.Snapshot.IsValid())
	{
		return true;
	}
	const FWBProductionMatchSpecificationLoadResult Result =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			ValidMatchSpecJson(*Database.Snapshot),
			TEXT("memory://match_spec"),
			*Database.Snapshot);
	TestTrue(TEXT("Match spec valid"), Result.bOk);
	TestEqual(TEXT("Two players"), Result.InitializationRequest.Players.Num(), 2);
	if (Result.InitializationRequest.Players.Num() == 2)
	{
		TestEqual(
			TEXT("Hero plus deck"),
			Result.InitializationRequest.Players[0].OrderedDeck.Num(),
			8);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionMatchSpecDigestRejectedTest,
	"Wandbound.CardDB.Production.MatchSpec.DigestMismatchRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionMatchSpecDigestRejectedTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Database = LoadProductionFixture();
	FString Json = ValidMatchSpecJson(*Database.Snapshot);
	Json.ReplaceInline(
		*Database.Snapshot->ContentDigest,
		TEXT("0000000000000000000000000000000000000000000000000000000000000000"));
	const FWBProductionMatchSpecificationLoadResult Result =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			Json,
			TEXT("memory://match_spec"),
			*Database.Snapshot);
	TestFalse(TEXT("Digest mismatch fails"), Result.bOk);
	TestTrue(
		TEXT("Digest diagnostic"),
		HasMatchDiagnostic(
			Result,
			TEXT("definition_bundle_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionMatchSpecMissingDefinitionTest,
	"Wandbound.CardDB.Production.MatchSpec.MissingDefinitionRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionMatchSpecMissingDefinitionTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Database = LoadProductionFixture();
	FString Json = ValidMatchSpecJson(*Database.Snapshot);
	Json.ReplaceInline(TEXT("fixture_student"), TEXT("missing_card"));
	const FWBProductionMatchSpecificationLoadResult Result =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			Json,
			TEXT("memory://match_spec"),
			*Database.Snapshot);
	TestFalse(TEXT("Missing definition fails"), Result.bOk);
	TestTrue(
		TEXT("Missing definition diagnostic"),
		HasMatchDiagnostic(Result, TEXT("deck_definition_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionMatchSpecWrongDeckTypeTest,
	"Wandbound.CardDB.Production.MatchSpec.HeroInDeckRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionMatchSpecWrongDeckTypeTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Database = LoadProductionFixture();
	FString Json = ValidMatchSpecJson(*Database.Snapshot);
	Json.ReplaceInline(
		TEXT("\"fixture_student\","),
		TEXT("\"fixture_hero_alpha\","));
	const FWBProductionMatchSpecificationLoadResult Result =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			Json,
			TEXT("memory://match_spec"),
			*Database.Snapshot);
	TestFalse(TEXT("Hero deck entry fails"), Result.bOk);
	TestTrue(
		TEXT("Wrong type diagnostic"),
		HasMatchDiagnostic(Result, TEXT("deck_definition_wrong_type")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionMatchSpecDeterministicInitializationTest,
	"Wandbound.CardDB.Production.MatchSpec.DeterministicInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionMatchSpecDeterministicInitializationTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Database = LoadProductionFixture();
	const FString Json = ValidMatchSpecJson(*Database.Snapshot);
	const FWBProductionMatchSpecificationLoadResult SpecA =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			Json,
			TEXT("memory://a"),
			*Database.Snapshot);
	const FWBProductionMatchSpecificationLoadResult SpecB =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			Json,
			TEXT("memory://b"),
			*Database.Snapshot);
	WBMatchCoordinator MatchA;
	WBMatchCoordinator MatchB;
	const FWBMatchOperationResult A =
		MatchA.InitializeMatch(SpecA.InitializationRequest);
	const FWBMatchOperationResult B =
		MatchB.InitializeMatch(SpecB.InitializationRequest);
	TestTrue(
		*FString::Printf(TEXT("First match initializes (%s)"), *A.Reason),
		A.bOk);
	TestTrue(
		*FString::Printf(TEXT("Second match initializes (%s)"), *B.Reason),
		B.bOk);
	TestEqual(
		TEXT("Same current player"),
		MatchA.GetState().CurrentPlayer,
		MatchB.GetState().CurrentPlayer);
	TestEqual(
		TEXT("Same legal action count"),
		A.NextLegalActions.Num(),
		B.NextLegalActions.Num());
	for (int32 Index = 0;
		Index < FMath::Min(A.NextLegalActions.Num(), B.NextLegalActions.Num());
		++Index)
	{
		TestEqual(
			FString::Printf(TEXT("Action %d"), Index),
			A.NextLegalActions[Index].ActionId,
			B.NextLegalActions[Index].ActionId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBPlayableDecisionTest,
	"Wandbound.CardDB.Production.Bootstrap.ReachesPlayableDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBPlayableDecisionTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Database = LoadProductionFixture();
	const FWBProductionMatchSpecificationLoadResult Spec =
		WBProductionMatchSpecification::ParseAndBuildRequestForTest(
			ValidMatchSpecJson(*Database.Snapshot),
			TEXT("memory://match"),
			*Database.Snapshot);
	WBMatchCoordinator Match;
	const FWBMatchOperationResult Result =
		Match.InitializeMatch(Spec.InitializationRequest);
	TestTrue(
		*FString::Printf(TEXT("Match initializes (%s)"), *Result.Reason),
		Result.bOk);
	TestTrue(TEXT("Legal decisions exist"), !Result.NextLegalActions.IsEmpty());
	TestEqual(TEXT("First player retained"), Match.GetFirstPlayerId(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBHiddenDefinitionDataTest,
	"Wandbound.CardDB.Production.HiddenInfo.DefinitionsContainNoPrivateState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBHiddenDefinitionDataTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadProductionFixture();
	FString Json;
	TestTrue(
		TEXT("Snapshot serializes"),
		Result.Snapshot.IsValid()
			&& WBProductionCardDatabase::SnapshotToCanonicalJson(
				*Result.Snapshot,
				Json));
	TestFalse(TEXT("No hand state"), Json.Contains(TEXT("\"hand\"")));
	TestFalse(TEXT("No deck order"), Json.Contains(TEXT("ordered_deck")));
	TestFalse(TEXT("No owner state"), Json.Contains(TEXT("owner_player")));
	TestFalse(TEXT("No concealed identity"), Json.Contains(TEXT("concealed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBHiddenDiagnosticSafetyTest,
	"Wandbound.CardDB.Production.HiddenInfo.DiagnosticsExcludeValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBHiddenDiagnosticSafetyTest::RunTest(const FString&)
{
	const FString Sandbox = MakeSandbox(TEXT("hidden_diagnostic"));
	TestTrue(
		TEXT("Fixture mutated"),
		ReplaceInSandboxFile(
			Sandbox,
			TEXT("bundles/characters.json"),
			TEXT("\"schema_version\": 1,"),
			TEXT("\"schema_version\": 1, \"private_hand\": \"SECRET_TOKEN\",")));
	const FWBProductionCardDatabaseLoadResult Result = LoadSandbox(Sandbox);
	FString DiagnosticText;
	for (const FWBProductionCardDBDiagnostic& Diagnostic : Result.Diagnostics)
	{
		DiagnosticText += Diagnostic.Code
			+ Diagnostic.ManifestPath
			+ Diagnostic.DefinitionId
			+ Diagnostic.FieldPath
			+ Diagnostic.Message
			+ Diagnostic.RecommendedAction;
	}
	TestFalse(
		TEXT("Private value excluded"),
		DiagnosticText.Contains(TEXT("SECRET_TOKEN")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBAuthorityGuardTest,
	"Wandbound.CardDB.Production.Authority.NoRulesMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBAuthorityGuardTest::RunTest(const FString&)
{
	FString LoaderSource;
	FString ProviderSource;
	TestTrue(
		TEXT("Loader source read"),
		ReadProjectFile(
			TEXT("Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp"),
			LoaderSource));
	TestTrue(
		TEXT("Provider source read"),
		ReadProjectFile(
			TEXT("Source/WandboundCardDB/Private/WBProductionCardDataProvider.cpp"),
			ProviderSource));
	const FString Combined = LoaderSource + ProviderSource;
	TestFalse(TEXT("No EffectRunner"), Combined.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No GenerateLegalActions"), Combined.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No mutable state API"), Combined.Contains(TEXT("GetMutable")));
	TestFalse(TEXT("No action codec"), Combined.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBModelIsolationGuardTest,
	"Wandbound.CardDB.Production.Authority.CharacterModelIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBModelIsolationGuardTest::RunTest(const FString&)
{
	FString Header;
	FString Source;
	TestTrue(
		TEXT("Header read"),
		ReadProjectFile(
			TEXT("Source/WandboundCardDB/Public/WBProductionCardDatabase.h"),
			Header));
	TestTrue(
		TEXT("Source read"),
		ReadProjectFile(
			TEXT("Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp"),
			Source));
	const FString Combined = Header + Source;
	TestFalse(TEXT("No asset path"), Combined.Contains(TEXT("FSoftObjectPath")));
	TestFalse(TEXT("No model importer"), Combined.Contains(TEXT("Interchange")));
	TestFalse(TEXT("No asset import"), Combined.Contains(TEXT("AssetImportTask")));
	TestFalse(TEXT("No presentation package"), Combined.Contains(TEXT("/Game/")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBModuleBoundaryTest,
	"Wandbound.CardDB.Production.Authority.ModuleBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBModuleBoundaryTest::RunTest(const FString&)
{
	FString BuildRules;
	TestTrue(
		TEXT("Build rules read"),
		ReadProjectFile(
			TEXT("Source/WandboundCardDB/WandboundCardDB.Build.cs"),
			BuildRules));
	TestFalse(
		TEXT("No Runtime dependency"),
		BuildRules.Contains(TEXT("\"WandboundRuntime\"")));
	TestFalse(
		TEXT("No Editor dependency"),
		BuildRules.Contains(TEXT("\"WandboundEditor\"")));
	TestFalse(
		TEXT("No Engine dependency"),
		BuildRules.Contains(TEXT("\"Engine\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionRuntimeBootstrapMissingInputsTest,
	"Wandbound.CardDB.Production.RuntimeBootstrap.MissingInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionRuntimeBootstrapMissingInputsTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	const FWBProductionRuntimeBootstrapResult Result =
		WBProductionRuntimeBootstrap::Build(Request);
	TestFalse(TEXT("Missing input fails"), Result.bOk);
	TestEqual(
		TEXT("Explicit reason"),
		Result.Reason,
		FString(TEXT("production_card_bundle_missing")));
	TestFalse(TEXT("No partial snapshot"), Result.Database.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionRuntimeBootstrapBundlePolicyTest,
	"Wandbound.CardDB.Production.RuntimeBootstrap.BundlePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionRuntimeBootstrapBundlePolicyTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		ProductionFixturePath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath =
		ProductionFixturePath(TEXT("match_spec.json"));
	const FWBProductionRuntimeBootstrapResult Rejected =
		WBProductionRuntimeBootstrap::Build(Request);
	TestFalse(TEXT("Test bundle rejected by production policy"), Rejected.bOk);
	TestEqual(
		TEXT("Policy reason"),
		Rejected.Reason,
		FString(TEXT("production_card_bundle_kind_disallowed")));

	Request.bAllowTestBundle = true;
	const FWBProductionRuntimeBootstrapResult Accepted =
		WBProductionRuntimeBootstrap::Build(Request);
	TestTrue(TEXT("Explicit test fixture accepted"), Accepted.bOk);
	TestTrue(TEXT("Immutable snapshot retained"), Accepted.Database.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionRuntimeBootstrapHostTest,
	"Wandbound.CardDB.Production.RuntimeBootstrap.HostPlayable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionRuntimeBootstrapHostTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		ProductionFixturePath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath =
		ProductionFixturePath(TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(Request);
	TestTrue(TEXT("Bootstrap succeeds"), Bootstrap.bOk);

	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>();
	const FWBRuntimeMatchCommandResult Result =
		Host->InitializeMatch(Bootstrap.InitializationRequest, 0);
	TestTrue(TEXT("One production host initializes"), Result.bOk);
	TestTrue(
		TEXT("Production host reaches a decision"),
		!Host->GetCurrentLegalActions().IsEmpty());
	TestEqual(
		TEXT("Viewer sees only own opening hand"),
		Host->GetCurrentHandCards().Num(),
		6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionRuntimePublicPresentationTest,
	"Wandbound.CardDB.Production.Presentation.PublicMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionRuntimePublicPresentationTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		ProductionFixturePath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath =
		ProductionFixturePath(TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(Request);
	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>();
	TestTrue(
		TEXT("Host initializes"),
		Bootstrap.bOk
			&& Host->InitializeMatch(
				Bootstrap.InitializationRequest,
				0).bOk);

	const TArray<FWBRuntimeUnitPresentation> Units =
		Host->GetCurrentUnits();
	TestTrue(TEXT("Visible Heroes have metadata"), Units.Num() >= 2);
	for (const FWBRuntimeUnitPresentation& Unit : Units)
	{
		TestFalse(TEXT("Visible unit name"), Unit.DisplayName.IsEmpty());
		TestFalse(TEXT("Visible unit category"), Unit.PublicCategory.IsEmpty());
	}
	for (const FWBRuntimeHandCardPresentation& Card
		: Host->GetCurrentHandCards())
	{
		TestFalse(TEXT("Own hand display name"), Card.DisplayName.IsEmpty());
		TestFalse(TEXT("Own hand card type"), Card.CardType.IsNone());
	}

	const TArray<FWBRuntimeLegalActionPresentation> InitialActions =
		Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* Equip =
		InitialActions.FindByPredicate(
			[](const FWBRuntimeLegalActionPresentation& Action)
			{
				return Action.Family
					== EWBRuntimeMatchActionFamily::Equip;
			});
	TestNotNull(TEXT("Fixture Wand can equip"), Equip);
	if (Equip != nullptr)
	{
		TestTrue(
			TEXT("Equip succeeds"),
			Host->SubmitLegalActionById(Equip->ActionId).bOk);
		Host->SkipAllPresentationEvents();
		const TArray<FWBRuntimeLegalActionPresentation> PostEquipActions =
			Host->GetCurrentLegalActions();
		const FWBRuntimeLegalActionPresentation* Activation =
			PostEquipActions.FindByPredicate(
				[](const FWBRuntimeLegalActionPresentation& Action)
				{
					return Action.Family
						== EWBRuntimeMatchActionFamily::Activation;
				});
		TestNotNull(TEXT("Equipped Wand exposes activation"), Activation);
		if (Activation != nullptr)
		{
			TestEqual(
				TEXT("Public activation label"),
				Activation->PublicLabel,
				FString(TEXT("Apply Burn")));
			TestEqual(
				TEXT("Public target prompt"),
				Activation->PublicTargetPrompt,
				FString(TEXT("Choose a unit")));
			TestFalse(
				TEXT("Internal effect id is not exposed as label"),
				Activation->PublicLabel
					== FString(TEXT("fixture_apply_burn")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionCardDBTransferReportTest,
	"Wandbound.CardDB.Production.TransferAudit.StableAndFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionCardDBTransferReportTest::RunTest(const FString&)
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	TestTrue(
		TEXT("Transfer report loads"),
		ReadProjectFile(
			TEXT("Docs/CardDB_Production_Transfer_Report.json"),
			Json)
			&& FJsonSerializer::Deserialize(
				TJsonReaderFactory<>::Create(Json),
				Root)
			&& Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}
	TestEqual(
		TEXT("Canonical definition count"),
		static_cast<int32>(
			Root->GetNumberField(TEXT("canonical_definition_count"))),
		244);
	const TArray<TSharedPtr<FJsonValue>>* Definitions = nullptr;
	TestTrue(
		TEXT("Definition records present"),
		Root->TryGetArrayField(TEXT("definitions"), Definitions)
			&& Definitions != nullptr
			&& Definitions->Num() == 244);
	FString PreviousId;
	if (Definitions != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Definitions)
		{
			const TSharedPtr<FJsonObject> Definition = Value->AsObject();
			const FString Id =
				Definition->GetStringField(TEXT("definition_id"));
			TestTrue(TEXT("Stable definition order"), PreviousId < Id);
			TestFalse(
				TEXT("No unsupported canonical definition is ready"),
				Definition->GetStringField(TEXT("unreal_status"))
					.StartsWith(TEXT("Transferred")));
			TestFalse(
				TEXT("Source provenance retained"),
				Definition->GetStringField(TEXT("source_manifest"))
					.IsEmpty());
			PreviousId = Id;
		}
	}
	return true;
}

#endif
