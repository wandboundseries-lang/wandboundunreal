#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "WBProductionAfterDamageTriggerSmoke.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBProductionAfterDamageTriggerSmokeTest,
	"Wandbound.AfterDamage.Fixture.ProductionSmokeAndFreshReplay",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FWBProductionAfterDamageTriggerSmokeTest::RunTest(const FString&)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/AfterDamageTriggerFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;

	const FWBProductionAfterDamageTriggerSmokeResult First =
		WBProductionAfterDamageTriggerSmoke::Run(Request);
	const FWBProductionAfterDamageTriggerSmokeResult Second =
		WBProductionAfterDamageTriggerSmoke::Run(Request);
	if (!First.bOk)
	{
		AddError(FString::Printf(
			TEXT("First production after-damage smoke failed: %s"),
			*First.Reason));
	}
	if (!Second.bOk)
	{
		AddError(FString::Printf(
			TEXT("Second production after-damage smoke failed: %s"),
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
	TestTrue(TEXT("Replay verifies at least one record"),
		First.RecordsVerified > 0);
	return true;
}

#endif
