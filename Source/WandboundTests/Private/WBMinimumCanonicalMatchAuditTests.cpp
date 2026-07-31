#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ReadMinimumAuditFile(const FString& RelativePath)
{
	FString Text;
	FFileHelper::LoadFileToString(
		Text,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
	return Text;
}

TSharedPtr<FJsonObject> ReadMinimumAuditJson(
	FAutomationTestBase& Test,
	const FString& RelativePath)
{
	const FString Text = ReadMinimumAuditFile(RelativePath);
	TSharedPtr<FJsonObject> Root;
	Test.TestTrue(
		*FString::Printf(TEXT("%s parses"), *RelativePath),
		FJsonSerializer::Deserialize(
			TJsonReaderFactory<>::Create(Text),
			Root)
			&& Root.IsValid());
	return Root;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditCoverageTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.AllDefinitionsAndHeroesClassified",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditCoverageTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Root = ReadMinimumAuditJson(
		*this,
		TEXT("Docs/CardDB_Minimum_Canonical_Match_Plan.json"));
	if (!Root.IsValid())
	{
		return false;
	}
	TestEqual(
		TEXT("All canonical definitions counted"),
		static_cast<int32>(
			Root->GetNumberField(TEXT("source_definition_count"))),
		244);
	TestEqual(
		TEXT("Production count unchanged"),
		static_cast<int32>(
			Root->GetNumberField(
				TEXT("resulting_production_definition_count"))),
		10);
	TestEqual(
		TEXT("No unrelated definition transferred"),
		static_cast<int32>(
			Root->GetNumberField(TEXT("transferred_this_pass"))),
		0);
	TestFalse(
		TEXT("No unsupported match declared"),
		Root->GetBoolField(TEXT("can_build_canonical_match")));
	TestFalse(
		TEXT("No match specification fabricated"),
		Root->GetBoolField(TEXT("match_spec_created")));

	const TArray<TSharedPtr<FJsonValue>>& Heroes =
		Root->GetArrayField(TEXT("hero_evidence"));
	TestEqual(TEXT("Every Character reviewed for Hero evidence"), Heroes.Num(), 113);
	int32 CanonicalHeroCount = 0;
	for (const TSharedPtr<FJsonValue>& Value : Heroes)
	{
		const TSharedPtr<FJsonObject> Hero = Value->AsObject();
		CanonicalHeroCount +=
			Hero->GetStringField(TEXT("status")) == TEXT("CanonicalHero")
				? 1
				: 0;
	}
	TestEqual(TEXT("Only explicit tutorial Heroes accepted"), CanonicalHeroCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditEligibilityTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.SupportedExpansionIsNotTransferred",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditEligibilityTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Root = ReadMinimumAuditJson(
		*this,
		TEXT("Docs/CardDB_Minimum_Canonical_Match_Plan.json"));
	if (!Root.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>& Definitions =
		Root->GetArrayField(TEXT("definitions"));
	TestEqual(TEXT("Definition audit exhaustive"), Definitions.Num(), 244);

	TArray<FString> EligibleIds;
	int32 TransferredCount = 0;
	for (const TSharedPtr<FJsonValue>& Value : Definitions)
	{
		const TSharedPtr<FJsonObject> Definition = Value->AsObject();
		const FString Status =
			Definition->GetStringField(TEXT("status"));
		if (Status.StartsWith(TEXT("Eligible")))
		{
			EligibleIds.Add(
				Definition->GetStringField(TEXT("definition_id")));
		}
		TransferredCount +=
			Definition->GetBoolField(TEXT("transferred_this_pass"))
				? 1
				: 0;
	}
	EligibleIds.Sort();
	const TArray<FString> ExpectedIds = {
		TEXT("char_test_healer"),
		TEXT("trap_generic_01"),
		TEXT("wand_equip_mender_thread") };
	TestTrue(TEXT("Only complete existing mappings eligible"), EligibleIds == ExpectedIds);
	TestEqual(TEXT("Eligible cards remain untransferred"), TransferredCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditDeckEvidenceTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.MissingFormatFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditDeckEvidenceTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Root = ReadMinimumAuditJson(
		*this,
		TEXT("Docs/CardDB_Minimum_Canonical_Match_Plan.json"));
	if (!Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Solver =
		Root->GetObjectField(TEXT("solver_result"));
	const TArray<TSharedPtr<FJsonValue>>& Blocks =
		Solver->GetArrayField(TEXT("blocking_reasons"));
	for (const FString& Required : {
		FString(TEXT("canonical_allowed_card_types_missing")),
		FString(TEXT("canonical_deck_ordering_missing")),
		FString(TEXT("canonical_deck_size_missing")),
		FString(TEXT("canonical_duplicate_limit_missing")),
		FString(TEXT("canonical_first_player_policy_missing")),
		FString(TEXT("canonical_identical_deck_policy_missing")) })
	{
		TestTrue(
			*Required,
			Blocks.ContainsByPredicate(
				[&Required](const TSharedPtr<FJsonValue>& Value)
				{
					return Value->AsString() == Required;
				}));
	}
	TestEqual(
		TEXT("No required deck IDs invented"),
		Solver->GetArrayField(TEXT("required_definition_ids")).Num(),
		0);
	TestTrue(
		TEXT("Later approved format now supplies production match spec"),
		IFileManager::Get().FileExists(
			*FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Data/CardDB/Production/InitialCanonical/match_spec.json"))));
	TestTrue(
		TEXT("Later approved Active Format is registered"),
		IFileManager::Get().FileExists(
			*FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Data/CardDB/Production/InitialCanonical/active_format_v1.json"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditStatusTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.ProductionStatusNamesRequirements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditStatusTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Status = ReadMinimumAuditJson(
		*this,
		TEXT("Data/CardDB/Production/InitialCanonical/match_status.json"));
	if (!Status.IsValid())
	{
		return false;
	}
	TestEqual(
		TEXT("Approved format makes production match available"),
		Status->GetStringField(TEXT("reason")),
		FString(TEXT("production_match_spec_available")));
	const TArray<TSharedPtr<FJsonValue>>& Missing =
		Status->GetArrayField(TEXT("missing_requirements"));
	TestEqual(TEXT("No production requirements missing"), Missing.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditReportCountsTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.TransferReportCountsAccurate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditReportCountsTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Plan = ReadMinimumAuditJson(
		*this,
		TEXT("Docs/CardDB_Minimum_Canonical_Match_Plan.json"));
	const TSharedPtr<FJsonObject> Transfer = ReadMinimumAuditJson(
		*this,
		TEXT("Docs/CardDB_Production_Transfer_Report.json"));
	if (!Plan.IsValid() || !Transfer.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Counts =
		Plan->GetObjectField(TEXT("definition_status_counts"));
	int32 CountTotal = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry :
		Counts->Values)
	{
		CountTotal += static_cast<int32>(Entry.Value->AsNumber());
	}
	TestEqual(TEXT("Status counts cover all definitions"), CountTotal, 244);
	TestEqual(
		TEXT("Transferred status count unchanged"),
		static_cast<int32>(
			Counts->GetNumberField(TEXT("AlreadyTransferred"))),
		10);
	TestEqual(
		TEXT("Exactly one activation newly eligible"),
		static_cast<int32>(
			Counts->GetNumberField(TEXT("EligibleExistingActivation"))),
		1);
	TestEqual(
		TEXT("Exactly one equip newly eligible"),
		static_cast<int32>(
			Counts->GetNumberField(TEXT("EligibleExistingEquip"))),
		1);
	TestEqual(
		TEXT("Exactly one payload newly eligible"),
		static_cast<int32>(
			Counts->GetNumberField(TEXT("EligibleExistingPayload"))),
		1);
	const TSharedPtr<FJsonObject> MinimumAudit =
		Transfer->GetObjectField(TEXT("minimum_canonical_match_audit"));
	TestEqual(
		TEXT("Transfer report retains zero new transfers"),
		static_cast<int32>(
			MinimumAudit->GetNumberField(TEXT("transferred_this_pass"))),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditDeterminismTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.PlanOrderingAndRankingStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditDeterminismTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Root = ReadMinimumAuditJson(
		*this,
		TEXT("Docs/CardDB_Minimum_Canonical_Match_Plan.json"));
	if (!Root.IsValid())
	{
		return false;
	}
	FString PreviousId;
	for (const TSharedPtr<FJsonValue>& Value :
		Root->GetArrayField(TEXT("definitions")))
	{
		const FString Id =
			Value->AsObject()->GetStringField(TEXT("definition_id"));
		TestTrue(
			*FString::Printf(TEXT("%s deterministically ordered"), *Id),
			PreviousId.IsEmpty() || PreviousId < Id);
		PreviousId = Id;
	}
	const TArray<TSharedPtr<FJsonValue>>& Ranking =
		Root->GetArrayField(TEXT("effect_family_ranking"));
	TestEqual(TEXT("Four ranked families"), Ranking.Num(), 4);
	TestEqual(
		TEXT("Target constraints remain first"),
		Ranking[0]->AsObject()->GetStringField(TEXT("effect_family")),
		FString(TEXT("activation_target_constraints")));
	TestEqual(
		TEXT("Exact target-family unlock count"),
		static_cast<int32>(
			Ranking[0]->AsObject()->GetNumberField(
				TEXT("definitions_unlocked_count"))),
		4);
	TestEqual(
		TEXT("React timing remains last"),
		Ranking[3]->AsObject()->GetStringField(TEXT("effect_family")),
		FString(TEXT("response_reactions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMinimumCanonicalAuditAuthorityTest,
	"Wandbound.CardDB.MinimumCanonical.Audit.AuthorityBoundariesPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMinimumCanonicalAuditAuthorityTest::RunTest(const FString&)
{
	const FString Planner = ReadMinimumAuditFile(
		TEXT("Source/WandboundCardDB/Private/WBMinimumCanonicalMatchPlanner.cpp"));
	const FString StartupResult = ReadMinimumAuditFile(
		TEXT("Source/WandboundRuntime/Private/WBProductionStartupResult.cpp"));
	const FString BuildRules = ReadMinimumAuditFile(
		TEXT("Source/WandboundCardDB/WandboundCardDB.Build.cs"));
	for (const FString& Forbidden : {
		FString(TEXT("WBRules")),
		FString(TEXT("WBEffectRunner")),
		FString(TEXT("WBActionCodec")),
		FString(TEXT("GenerateLegalActions")) })
	{
		TestFalse(*Forbidden, Planner.Contains(Forbidden));
	}
	for (const FString& Forbidden : {
		FString(TEXT("\"hand\"")),
		FString(TEXT("\"deck\"")),
		FString(TEXT("\"marker_identity\"")),
		FString(TEXT("\"effect_parameters\"")) })
	{
		TestFalse(*Forbidden, StartupResult.Contains(Forbidden));
	}
	TestTrue(
		TEXT("Approved production match spec is staged"),
		BuildRules.Contains(
			TEXT("Production/InitialCanonical/match_spec.json")));
	TestTrue(
		TEXT("Approved Active Format is staged"),
		BuildRules.Contains(TEXT("ActiveFormat.schema.json"))
			&& BuildRules.Contains(
				TEXT("Production/InitialCanonical/active_format_v1.json")));
	TestTrue(
		TEXT("Approved game-start addendum is staged"),
		BuildRules.Contains(TEXT("GameStartAddendum.schema.json"))
			&& BuildRules.Contains(
				TEXT("Production/InitialCanonical/game_start_addendum_v1.json")));
	return true;
}

#endif
