#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionRuntimeBootstrap.h"

namespace
{
FString InitialCanonicalRoot()
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/InitialCanonical"));
}

FString InitialCanonicalPath(const FString& RelativePath)
{
	return FPaths::Combine(InitialCanonicalRoot(), RelativePath);
}

FWBProductionCardDatabaseLoadResult LoadInitialCanonical()
{
	return WBProductionCardDatabase::LoadManifestSuite(
		InitialCanonicalPath(TEXT("root_manifest.json")));
}

void ReportLoadDiagnostics(
	FAutomationTestBase& Test,
	const FWBProductionCardDatabaseLoadResult& Result)
{
	if (Result.bOk)
	{
		return;
	}
	for (const FWBProductionCardDBDiagnostic& Diagnostic : Result.Diagnostics)
	{
		Test.AddError(FString::Printf(
			TEXT("%s at %s: %s"),
			*Diagnostic.Code,
			*Diagnostic.FieldPath,
			*Diagnostic.Message));
	}
}

FString ReadProjectText(const FString& RelativePath)
{
	FString Text;
	FFileHelper::LoadFileToString(
		Text,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
	return Text;
}

bool IsSafeDefinitionId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 128 || !(FChar::IsAlpha(Value[0]) || Value[0] == TEXT('_')))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!(FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-')))
		{
			return false;
		}
	}
	return true;
}

FString MakeProductionSandbox(const FString& Name)
{
	const FString Sandbox = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("InitialCanonicalTests"),
		Name + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
	IFileManager::Get().MakeDirectory(*Sandbox, true);
	FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(
		*Sandbox,
		*InitialCanonicalRoot(),
		true);
	return Sandbox;
}

bool ReverseCards(const FString& BundlePath)
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *BundlePath)
		|| !FJsonSerializer::Deserialize(
			TJsonReaderFactory<>::Create(Json),
			Root)
		|| !Root.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
	if (!Root->TryGetArrayField(TEXT("cards"), Cards) || Cards == nullptr)
	{
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> Reversed = *Cards;
	Algo::Reverse(Reversed);
	Root->SetArrayField(TEXT("cards"), Reversed);
	Json.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	return FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)
		&& FFileHelper::SaveStringToFile(Json, *BundlePath);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalSuiteLoadsTest,
	"Wandbound.CardDB.InitialCanonical.Suite.LoadsProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalSuiteLoadsTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadInitialCanonical();
	ReportLoadDiagnostics(*this, Result);
	TestTrue(TEXT("Canonical production suite loads"), Result.bOk);
	TestTrue(TEXT("Immutable snapshot exists"), Result.Snapshot.IsValid());
	if (Result.Snapshot.IsValid())
	{
		TestEqual(TEXT("Definition count"), Result.Snapshot->Records.Num(), 10);
		TestEqual(
			TEXT("Production classification"),
			Result.Snapshot->BundleKind,
			EWBProductionBundleKind::Production);
		TestEqual(
			TEXT("Pinned digest"),
			Result.Snapshot->ContentDigest,
			FString(TEXT("b406557f2f190818fe3460621bbbdfaf84abe53623ff26aa934588aad68bedde")));
		TestEqual(
			TEXT("Lock path"),
			Result.Snapshot->BundleLockPath,
			FString(TEXT("bundle_lock.json")));
		const FString ExpectedTransferReportDigest =
			TEXT("987012918cc2b3b14bef864b05982da32d04992d0cb2bb8c1d1ea2ec2baeba2f");
		TestEqual(
			TEXT("Locked transfer report digest"),
			Result.Snapshot->LockedTransferReportDigest,
			ExpectedTransferReportDigest);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalFieldsTest,
	"Wandbound.CardDB.InitialCanonical.Fields.CanonicalStatsAndTerminology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalFieldsTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadInitialCanonical();
	if (!TestTrue(TEXT("Suite loads"), Result.bOk && Result.Snapshot.IsValid()))
	{
		return false;
	}
	const FWBProductionCardRecord* Crusader =
		Result.Snapshot->FindCharacter(TEXT("char_test_01"));
	const FWBProductionCardRecord* Monolith =
		Result.Snapshot->FindCharacter(TEXT("char_new_rl_monolith"));
	TestNotNull(TEXT("Arclight Crusader transferred"), Crusader);
	TestNotNull(TEXT("Resonance Monolith transferred"), Monolith);
	if (Crusader != nullptr)
	{
		TestEqual(TEXT("Canonical name"), Crusader->CoreDefinition.PublicName, FString(TEXT("Arclight Crusader")));
		TestEqual(TEXT("ATK"), Crusader->CoreDefinition.CharacterStats.ATK, 4);
		TestEqual(TEXT("AR is Attack Range"), Crusader->CoreDefinition.CharacterStats.AR, 2);
		TestEqual(TEXT("RL"), Crusader->CoreDefinition.CharacterStats.RL, 3);
		TestEqual(TEXT("Attack range equals AR"), Crusader->Attack.Range, 2);
	}
	if (Monolith != nullptr)
	{
		TestEqual(TEXT("Zero ATK preserved"), Monolith->CoreDefinition.CharacterStats.ATK, 0);
		TestEqual(TEXT("Zero AR preserved"), Monolith->CoreDefinition.CharacterStats.AR, 0);
		TestEqual(TEXT("Zero attack range preserved"), Monolith->Attack.Range, 0);
	}
	const FString Characters =
		ReadProjectText(TEXT("Data/CardDB/Production/InitialCanonical/definitions/characters.json"));
	TestFalse(TEXT("No invented Armor field"), Characters.Contains(TEXT("\"armor\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalEligibilityTest,
	"Wandbound.CardDB.InitialCanonical.Eligibility.FailClosedAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalEligibilityTest::RunTest(const FString&)
{
	const FString Audit = ReadProjectText(
		TEXT("Docs/CardDB_Initial_Canonical_Eligibility_Report.json"));
	TSharedPtr<FJsonObject> Root;
	TestTrue(
		TEXT("Eligibility report parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Audit), Root)
			&& Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
	TestTrue(
		TEXT("Candidate array present"),
		Root->TryGetArrayField(TEXT("candidates"), Candidates)
			&& Candidates != nullptr);
	if (Candidates == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("All deferred candidates classified"), Candidates->Num(), 26);
	int32 EligibleCount = 0;
	int32 ImplicitCount = 0;
	int32 ConflictCount = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Candidates)
	{
		const TSharedPtr<FJsonObject> Candidate = Value->AsObject();
		const FString Eligibility =
			Candidate->GetStringField(TEXT("eligibility"));
		EligibleCount += Eligibility == TEXT("EligibleBehaviorFree") ? 1 : 0;
		ImplicitCount += Eligibility == TEXT("UnsupportedImplicitBehavior") ? 1 : 0;
		ConflictCount += Eligibility == TEXT("SchemaConflict") ? 1 : 0;
		TestFalse(
			TEXT("No candidate silently strips semantics"),
			Candidate->GetStringField(TEXT("reason")).IsEmpty());
	}
	TestEqual(TEXT("Behavior-free count"), EligibleCount, 10);
	TestEqual(TEXT("Implicit behavior rejected"), ImplicitCount, 14);
	TestEqual(TEXT("Schema conflicts rejected"), ConflictCount, 2);

	const FString TransferReport = ReadProjectText(
		TEXT("Docs/CardDB_Production_Transfer_Report.json"));
	TSharedPtr<FJsonObject> TransferRoot;
	TestTrue(
		TEXT("Transfer report parses"),
		FJsonSerializer::Deserialize(
			TJsonReaderFactory<>::Create(TransferReport),
			TransferRoot)
			&& TransferRoot.IsValid());
	if (TransferRoot.IsValid())
	{
		TestEqual(
			TEXT("Transferred total"),
			static_cast<int32>(
				TransferRoot->GetNumberField(
					TEXT("canonical_definitions_transferred"))),
			10);
		const TSharedPtr<FJsonObject> StatusCounts =
			TransferRoot->GetObjectField(TEXT("status_counts"));
		TestEqual(
			TEXT("Unsupported definitions unchanged"),
			static_cast<int32>(
				StatusCounts->GetNumberField(TEXT("UnsupportedEffect"))),
			218);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalUnsupportedAbsentTest,
	"Wandbound.CardDB.InitialCanonical.Eligibility.UnsupportedDefinitionsAbsent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalUnsupportedAbsentTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadInitialCanonical();
	if (!TestTrue(TEXT("Suite loads"), Result.bOk && Result.Snapshot.IsValid()))
	{
		return false;
	}
	for (const FString& Id : {
		FString(TEXT("char_bm_anchor")),
		FString(TEXT("char_csn_echo")),
		FString(TEXT("char_ww_warder")),
		FString(TEXT("npc_raider")),
		FString(TEXT("npc_duelist_human")),
		FString(TEXT("officer_new_bulwark")),
		FString(TEXT("trap_generic_01")),
		FString(TEXT("wand_equip_csn_oathchain")),
		FString(TEXT("wand_equip_enemy_deadweight")),
		FString(TEXT("fixture_guard")),
		FString(TEXT("fixture_hero_alpha")) })
	{
		TestNull(*FString::Printf(TEXT("%s excluded"), *Id), Result.Snapshot->FindRecord(Id));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalOrderingTest,
	"Wandbound.CardDB.InitialCanonical.Determinism.SourceOrderingStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalOrderingTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Baseline = LoadInitialCanonical();
	const FString Sandbox = MakeProductionSandbox(TEXT("ordering"));
	TestTrue(
		TEXT("Character order reversed"),
		ReverseCards(FPaths::Combine(Sandbox, TEXT("definitions/characters.json"))));
	const FWBProductionCardDatabaseLoadResult Reordered =
		WBProductionCardDatabase::LoadManifestSuite(
			FPaths::Combine(Sandbox, TEXT("root_manifest.json")));
	TestTrue(TEXT("Reordered suite loads"), Reordered.bOk);
	if (Baseline.Snapshot.IsValid() && Reordered.Snapshot.IsValid())
	{
		TestEqual(
			TEXT("Semantic digest stable"),
			Reordered.Snapshot->ContentDigest,
			Baseline.Snapshot->ContentDigest);
		TestTrue(
			TEXT("Definition ordering stable"),
			Reordered.Snapshot->GetDefinitionIds()
				== Baseline.Snapshot->GetDefinitionIds());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalLockTamperTest,
	"Wandbound.CardDB.InitialCanonical.Determinism.LockTamperRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalLockTamperTest::RunTest(const FString&)
{
	const FString Sandbox = MakeProductionSandbox(TEXT("lock_tamper"));
	const FString LockPath = FPaths::Combine(Sandbox, TEXT("bundle_lock.json"));
	FString Lock = ReadProjectText(
		TEXT("Data/CardDB/Production/InitialCanonical/bundle_lock.json"));
	Lock.ReplaceInline(
		TEXT("\"definition_count\": 10"),
		TEXT("\"definition_count\": 9"));
	TestTrue(TEXT("Tampered lock written"), FFileHelper::SaveStringToFile(Lock, *LockPath));
	const FWBProductionCardDatabaseLoadResult Result =
		WBProductionCardDatabase::LoadManifestSuite(
			FPaths::Combine(Sandbox, TEXT("root_manifest.json")));
	TestFalse(TEXT("Tampered lock rejected"), Result.bOk);
	TestTrue(
		TEXT("Lock mismatch diagnostic"),
		Result.Diagnostics.ContainsByPredicate(
			[](const FWBProductionCardDBDiagnostic& Diagnostic)
			{
				return Diagnostic.Code == TEXT("bundle_lock_mismatch");
			}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalBlockedMatchTest,
	"Wandbound.CardDB.InitialCanonical.Runtime.NamedBlockedMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalBlockedMatchTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		InitialCanonicalPath(TEXT("root_manifest.json"));
	const FWBProductionRuntimeBootstrapResult Result =
		WBProductionRuntimeBootstrap::Build(Request);
	TestFalse(TEXT("No fabricated match starts"), Result.bOk);
	TestEqual(
		TEXT("Named canonical deck blocker"),
		Result.Reason,
		FString(TEXT("production_match_spec_blocked_by_canonical_deck_evidence")));
	TestTrue(TEXT("Production snapshot loaded first"), Result.Database.IsValid());
	TestFalse(TEXT("No test-bundle opt-in required"), Request.bAllowTestBundle);
	if (Result.Database.IsValid())
	{
		TestEqual(TEXT("Two Hero candidates"), Result.Database->HeroCandidateDefinitionIds.Num(), 2);
		TestEqual(
			TEXT("First Hero candidate"),
			Result.Database->HeroCandidateDefinitionIds[0],
			FString(TEXT("char_test_01")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalPackagingTest,
	"Wandbound.CardDB.InitialCanonical.Packaging.ExactAllowlist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalPackagingTest::RunTest(const FString&)
{
	const FString BuildRules =
		ReadProjectText(TEXT("Source/WandboundCardDB/WandboundCardDB.Build.cs"));
	TestTrue(TEXT("Production root staged"), BuildRules.Contains(TEXT("Production/InitialCanonical/root_manifest.json")));
	TestTrue(TEXT("Bundle lock staged"), BuildRules.Contains(TEXT("Production/InitialCanonical/bundle_lock.json")));
	TestTrue(TEXT("Blocked status staged"), BuildRules.Contains(TEXT("Production/InitialCanonical/match_status.json")));
	TestFalse(TEXT("No wildcard enumeration"), BuildRules.Contains(TEXT("SearchOption.AllDirectories")));
	TestFalse(TEXT("Test fixture not staged"), BuildRules.Contains(TEXT("TestFixtures/ProductionPipeline")));
	TestFalse(TEXT("Godot source not staged"), BuildRules.Contains(TEXT("Reference/GodotProject")));
	TestFalse(TEXT("Meshy content not staged"), BuildRules.Contains(TEXT("Meshy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalHiddenInfoTest,
	"Wandbound.CardDB.InitialCanonical.Security.PublicDataOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalHiddenInfoTest::RunTest(const FString&)
{
	FString SuiteText;
	for (const FString& RelativePath : {
		FString(TEXT("root_manifest.json")),
		FString(TEXT("bundle_manifest.json")),
		FString(TEXT("definitions/characters.json")),
		FString(TEXT("definitions/npcs.json")),
		FString(TEXT("bundle_lock.json")),
		FString(TEXT("match_status.json")) })
	{
		FString FileText;
		TestTrue(
			*FString::Printf(TEXT("%s readable"), *RelativePath),
			FFileHelper::LoadFileToString(
				FileText,
				*InitialCanonicalPath(RelativePath)));
		SuiteText += FileText;
	}
	for (const FString& Forbidden : {
		FString(TEXT("\"hand\"")),
		FString(TEXT("\"deck\"")),
		FString(TEXT("\"owner\"")),
		FString(TEXT("\"tile\"")),
		FString(TEXT("\"hidden\"")),
		FString(TEXT(".uasset")),
		FString(TEXT("/Game/")),
		FString(TEXT("Meshy")),
		FString(TEXT("\"test_only\": true")) })
	{
		TestFalse(
			*FString::Printf(TEXT("Public suite excludes %s"), *Forbidden),
			SuiteText.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBInitialCanonicalModelInteropTest,
	"Wandbound.CardDB.InitialCanonical.ModelInterop.DefinitionIdsReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBInitialCanonicalModelInteropTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Result = LoadInitialCanonical();
	if (!TestTrue(TEXT("Suite loads"), Result.bOk && Result.Snapshot.IsValid()))
	{
		return false;
	}
	int32 CharacterCount = 0;
	for (const FWBProductionCardRecord& Record : Result.Snapshot->Records)
	{
		if (Record.Type != EWBProductionCardType::Character)
		{
			continue;
		}
		++CharacterCount;
		TestTrue(
			*FString::Printf(
				TEXT("%s accepted by model manifest ID policy"),
				*Record.CoreDefinition.CardId),
			IsSafeDefinitionId(
				Record.CoreDefinition.CardId));
	}
	TestEqual(TEXT("Eight Character IDs ready"), CharacterCount, 8);
	TestFalse(
		TEXT("No model path in snapshot"),
		ReadProjectText(
			TEXT("Data/CardDB/Production/InitialCanonical/definitions/characters.json"))
			.Contains(TEXT("model")));
	return true;
}

#endif
