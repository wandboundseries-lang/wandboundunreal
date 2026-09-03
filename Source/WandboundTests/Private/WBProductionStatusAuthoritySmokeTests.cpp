#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionStatusAuthoritySmoke.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionStatusAuthoritySmokeTest,
	"Wandbound.StatusAuthority.Fixture.ProductionSmokeAndFreshReplay",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWBProductionStatusAuthoritySmokeTest::RunTest(const FString&)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/StatusAuthorityFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;

	const FWBProductionCardDatabaseLoadResult Database =
		WBProductionCardDatabase::LoadManifestSuite(
			Request.CardBundleManifestPath);
	TestTrue(TEXT("Status-authority fixture database loads"), Database.bOk);
	if (Database.Snapshot.IsValid())
	{
		AddInfo(FString::Printf(
			TEXT("STATUS_AUTHORITY_BUNDLE_DIGEST=%s"),
			*Database.Snapshot->ContentDigest));
	}

	const FWBProductionStatusAuthoritySmokeResult First =
		WBProductionStatusAuthoritySmoke::Run(Request);
	const FWBProductionStatusAuthoritySmokeResult Second =
		WBProductionStatusAuthoritySmoke::Run(Request);
	if (!First.bOk)
	{
		AddError(FString::Printf(
			TEXT("First production status-authority smoke failed: %s"),
			*First.Reason));
	}
	if (!Second.bOk)
	{
		AddError(FString::Printf(
			TEXT("Second production status-authority smoke failed: %s"),
			*Second.Reason));
	}
	TestTrue(TEXT("First production smoke succeeds"), First.bOk);
	TestTrue(TEXT("Second production smoke succeeds"), Second.bOk);
	TestEqual(TEXT("Replay record count deterministic"),
		First.RecordsVerified, Second.RecordsVerified);
	TestEqual(TEXT("Final state digest deterministic"),
		First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("Final trace digest deterministic"),
		First.FinalTraceDigest, Second.FinalTraceDigest);
	TestTrue(TEXT("Replay verifies submitted decisions"),
		First.RecordsVerified >= 4);
	return true;
}

#endif
