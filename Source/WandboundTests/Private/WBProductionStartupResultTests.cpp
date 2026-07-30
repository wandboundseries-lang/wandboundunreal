#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBProductionStartupResult.h"

namespace
{
FWBProductionRuntimeBootstrapResult MakeBootstrapFailure(
	const FString& Reason,
	const bool bBundleLoaded = false)
{
	FWBProductionRuntimeBootstrapResult Result;
	Result.Reason = Reason;
	if (bBundleLoaded)
	{
		TSharedPtr<FWBProductionCardDatabase> Database =
			MakeShared<FWBProductionCardDatabase>();
		Database->ContentDigest =
			TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		Result.Database = Database;
	}
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupDeckBlockTest,
	"Wandbound.Runtime.ProductionStartupResult.DeckEvidenceBlockWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupDeckBlockTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::FromBootstrap(
			Request,
			MakeBootstrapFailure(
				TEXT("production_match_spec_blocked_by_canonical_deck_evidence"),
				true));
	TestTrue(TEXT("Bundle loaded"), Result.bBundleLoaded);
	TestTrue(TEXT("Startup blocked"), Result.bBlocked);
	TestEqual(
		TEXT("Named deck blocker retained"),
		Result.ResultCode,
		FString(TEXT("production_match_spec_blocked_by_canonical_deck_evidence")));
	TestEqual(TEXT("Predictable blocked exit"), WBProductionStartupResult::ExitCodeForResult(Result), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupHeroBlockTest,
	"Wandbound.Runtime.ProductionStartupResult.HeroEvidenceBlockWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupHeroBlockTest::RunTest(const FString&)
{
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::FromBootstrap(
			FWBProductionRuntimeBootstrapRequest(),
			MakeBootstrapFailure(
				TEXT("production_match_spec_blocked_by_hero_evidence"),
				true));
	TestEqual(
		TEXT("Named Hero blocker retained"),
		Result.ResultCode,
		FString(TEXT("production_match_spec_blocked_by_hero_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupUnsupportedBlockTest,
	"Wandbound.Runtime.ProductionStartupResult.UnsupportedDefinitionsBlockWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupUnsupportedBlockTest::RunTest(const FString&)
{
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::FromBootstrap(
			FWBProductionRuntimeBootstrapRequest(),
			MakeBootstrapFailure(
				TEXT("production_match_spec_blocked_by_unsupported_definitions"),
				true));
	TestEqual(
		TEXT("Named unsupported blocker retained"),
		Result.ResultCode,
		FString(TEXT("production_match_spec_blocked_by_unsupported_definitions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupInvalidBundleTest,
	"Wandbound.Runtime.ProductionStartupResult.InvalidBundleWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupInvalidBundleTest::RunTest(const FString&)
{
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::FromBootstrap(
			FWBProductionRuntimeBootstrapRequest(),
			MakeBootstrapFailure(
				TEXT("production_card_bundle_not_found")));
	TestFalse(TEXT("Bundle not loaded"), Result.bBundleLoaded);
	TestEqual(TEXT("Invalid bundle normalized"), Result.ResultCode, FString(TEXT("production_bundle_invalid")));
	TestEqual(TEXT("Predictable invalid exit"), WBProductionStartupResult::ExitCodeForResult(Result), 13);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupBootstrapOnlyTest,
	"Wandbound.Runtime.ProductionStartupResult.BootstrapSuccessDoesNotPretendMatchStarted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupBootstrapOnlyTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapResult Bootstrap;
	Bootstrap.bOk = true;
	TSharedPtr<FWBProductionCardDatabase> Database =
		MakeShared<FWBProductionCardDatabase>();
	Database->ContentDigest = TEXT("digest");
	Bootstrap.Database = Database;
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::FromBootstrap(
			FWBProductionRuntimeBootstrapRequest(),
			Bootstrap);
	TestTrue(TEXT("Validated bundle retained"), Result.bBundleLoaded);
	TestFalse(TEXT("Match not initialized by bundle bootstrap"), Result.bMatchInitialized);
	TestFalse(TEXT("No playable decision claimed"), Result.bPlayableDecisionReached);
	TestTrue(
		TEXT("Started code waits for runtime initialization"),
		Result.ResultCode.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupDigestMismatchTest,
	"Wandbound.Runtime.ProductionStartupResult.DigestMismatchWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupDigestMismatchTest::RunTest(const FString&)
{
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::FromBootstrap(
			FWBProductionRuntimeBootstrapRequest(),
			MakeBootstrapFailure(
				TEXT("production_match_spec_digest_mismatch"),
				true));
	TestEqual(TEXT("Digest mismatch normalized"), Result.ResultCode, FString(TEXT("production_digest_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupSafeSerializationTest,
	"Wandbound.Runtime.ProductionStartupResult.SafeFieldsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupSafeSerializationTest::RunTest(const FString&)
{
	const FWBProductionStartupResult Result =
		WBProductionStartupResult::Started(
			TEXT("digest"),
			true,
			2,
			7,
			true);
	const FString Json = WBProductionStartupResult::Serialize(Result);
	for (const FString& Required : {
		FString(TEXT("\"schema_version\"")),
		FString(TEXT("\"startup_mode\"")),
		FString(TEXT("\"bundle_loaded\"")),
		FString(TEXT("\"bundle_digest\"")),
		FString(TEXT("\"match_spec_present\"")),
		FString(TEXT("\"match_initialized\"")),
		FString(TEXT("\"playable_decision_reached\"")),
		FString(TEXT("\"blocked\"")),
		FString(TEXT("\"result_code\"")),
		FString(TEXT("\"generation\"")),
		FString(TEXT("\"revision\"")) })
	{
		TestTrue(*Required, Json.Contains(Required));
	}
	for (const FString& Forbidden : {
		FString(TEXT("\"hand\"")),
		FString(TEXT("\"deck\"")),
		FString(TEXT("\"marker_identity\"")),
		FString(TEXT("\"effect_parameters\"")),
		FString(TEXT("C:\\\\")) })
	{
		TestFalse(*Forbidden, Json.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupFileWriteTest,
	"Wandbound.Runtime.ProductionStartupResult.FileWritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupFileWriteTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("ProductionStartupResultTests/result.json"));
	FWBProductionStartupResult Result;
	Result.bBlocked = true;
	Result.ResultCode =
		TEXT("production_match_spec_blocked_by_canonical_deck_evidence");
	TestTrue(TEXT("Result file written"), WBProductionStartupResult::Write(Result, Path));
	FString Json;
	TestTrue(TEXT("Result file readable"), FFileHelper::LoadFileToString(Json, *Path));
	TestTrue(TEXT("Named result serialized"), Json.Contains(Result.ResultCode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStartupProbePolicyTest,
	"Wandbound.Runtime.ProductionStartupResult.ProbeExitPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBProductionStartupProbePolicyTest::RunTest(const FString&)
{
	TestFalse(
		TEXT("Ordinary startup is not a probe"),
		WBProductionStartupResult::IsStartupProbeRequested(TEXT("-unattended")));
#if UE_BUILD_SHIPPING
	TestFalse(
		TEXT("Shipping ignores probe"),
		WBProductionStartupResult::IsStartupProbeRequested(
			TEXT("-WandboundProductionStartupProbe")));
#else
	TestTrue(
		TEXT("Explicit probe recognized"),
		WBProductionStartupResult::IsStartupProbeRequested(
			TEXT("-WandboundProductionStartupProbe")));
#endif
	return true;
}

#endif
