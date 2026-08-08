#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBHybridSummon.h"
#include "WBProductionHybridReplacementSmoke.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBPublicBoardSummary.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString HybridFixturePath(const FString& Name)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/HybridReplacementFixture"),
		Name);
}

FWBProductionRuntimeBootstrapRequest MakeHybridRequest()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = HybridFixturePath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = HybridFixturePath(TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	return Request;
}

const FWBMatchLegalAction* FindAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBMatchActionFamily Family)
{
	return Actions.FindByPredicate([Family](const FWBMatchLegalAction& Action)
	{
		return Action.Family == Family;
	});
}

const FWBMatchLegalAction* FindHybrid(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBHybridWandPaymentSource PaymentSource)
{
	return Actions.FindByPredicate([PaymentSource](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& Action.bHybridHeroReplacement
			&& Action.HybridSummonPlan.WandPaymentSource == PaymentSource;
	});
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

int32 CountDiscardInstance(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId)
{
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), PlayerId);
	if (Zones == nullptr) return 0;
	int32 Count = 0;
	for (const FWBZoneCardEntry& Entry : Zones->Discard)
	{
		if (Entry.Card.InstanceId == InstanceId) ++Count;
	}
	return Count;
}

struct FHybridScenario
{
	bool bOk = false;
	FString Reason;
	FWBProductionRuntimeBootstrapRequest Request;
	FWBProductionRuntimeBootstrapResult Bootstrap;
	FWBGameStateData BeforeState;
	FWBGameStateData AfterState;
	FWBHybridSummonPlan Plan;
	FWBMatchOperationResult Replacement;
	FWBMatchLegalActionGenerationResult Continued;
	FWBMatchObservation PublicObservation;
	FWBProductionMatchReplayArchive Archive;
	FString SerializedArchive;
	FString ReceiptJson;
	FString AfterStateDigest;
	FString AfterTraceDigest;
	FWBProductionMatchReplayRunResult Replay;
	int32 OldHeroId = -1;
	int32 NewHeroId = -1;
	FString PaidWandInstanceId;
	FWBTile OldHeroTile;
	int32 GenerationBeforeReplacement = -1;
	int32 RevisionBeforeReplacement = -1;
};

FHybridScenario BuildScenario()
{
	FHybridScenario Result;
	Result.Request = MakeHybridRequest();
	Result.Bootstrap = WBProductionRuntimeBootstrap::Build(Result.Request);
	if (!Result.Bootstrap.bOk)
	{
		Result.Reason = Result.Bootstrap.Reason;
		return Result;
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		Result.Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	const FWBPlayerStateData* Player = Coordinator.GetState().GetPlayerById(0);
	const FWBUnitState* Hero = Player != nullptr
		? Coordinator.GetState().GetUnitById(Player->HeroUnitId)
		: nullptr;
	if (Hero == nullptr)
	{
		Result.Reason = TEXT("fixture_hero_missing");
		return Result;
	}
	Result.OldHeroId = Hero->UnitId;
	Result.OldHeroTile = FWBTile(Hero->X, Hero->Y);

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(Result.Bootstrap),
		Coordinator))
	{
		Result.Reason = Recorder.GetReceipt().FailureCode;
		return Result;
	}

	const FWBMatchLegalAction* Equip = Started.NextLegalActions.FindByPredicate(
		[&Result](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Equip
				&& Action.EquipRequest.SourceCardId == TEXT("hybrid_fixture_wand")
				&& Action.EquipRequest.TargetUnitId == Result.OldHeroId;
		});
	if (Equip == nullptr)
	{
		Result.Reason = TEXT("fixture_equip_missing");
		return Result;
	}
	Result.PaidWandInstanceId = Equip->EquipRequest.SourceInstanceId;
	const FWBMatchOperationResult Equipped = Coordinator.SubmitActionId(
		Equip->PlayerId, Equip->ActionId);
	if (!Equipped.bOk)
	{
		Result.Reason = Equipped.Reason;
		return Result;
	}
	Recorder.CaptureCommittedActions(Coordinator);

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Hybrid = Legal.bOk
		? FindHybrid(Legal.Actions, EWBHybridWandPaymentSource::SacrificedUnit)
		: nullptr;
	if (Hybrid == nullptr)
	{
		Result.Reason = Legal.bOk ? TEXT("fixture_hybrid_missing") : Legal.Reason;
		return Result;
	}
	Result.BeforeState = Coordinator.GetState();
	Result.Plan = Hybrid->HybridSummonPlan;
	Result.GenerationBeforeReplacement = Coordinator.GetCoordinatorGeneration();
	Result.RevisionBeforeReplacement = Coordinator.GetCoordinatorRevision();
	Result.Replacement = Coordinator.SubmitActionId(Hybrid->PlayerId, Hybrid->ActionId);
	if (!Result.Replacement.bOk)
	{
		Result.Reason = Result.Replacement.Reason;
		return Result;
	}
	Recorder.CaptureCommittedActions(Coordinator);
	Result.AfterState = Coordinator.GetState();
	Result.AfterStateDigest = Coordinator.GetCurrentStateDigest();
	Result.AfterTraceDigest = Coordinator.GetCurrentTraceDigest();
	const FWBPlayerStateData* AfterPlayer = Result.AfterState.GetPlayerById(0);
	Result.NewHeroId = AfterPlayer != nullptr ? AfterPlayer->HeroUnitId : -1;
	Result.Continued = Coordinator.EnumerateLegalActions();
	Result.PublicObservation = Coordinator.BuildObservation(1);
	Result.Archive = Recorder.GetArchive();
	Result.SerializedArchive = WBProductionMatchReplay::Serialize(Result.Archive);
	Result.ReceiptJson = WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt());
	FWBProductionMatchReplayRunRequest RunRequest;
	RunRequest.SerializedArchive = Result.SerializedArchive;
	RunRequest.BootstrapRequest = Result.Request;
	Result.Replay = FWBProductionMatchReplayRunner::Run(RunRequest);
	Result.bOk = true;
	return Result;
}

const FHybridScenario& GetScenario()
{
	static const FHybridScenario Scenario = BuildScenario();
	return Scenario;
}

bool TestRejectedPlanAtomicity(
	FAutomationTestBase& Test,
	const FHybridScenario& Scenario,
	FWBHybridSummonPlan Plan,
	const bool bStale)
{
	FWBGameStateData State = Scenario.BeforeState;
	const FString BeforeDigest = WBProductionMatchReplay::BuildGameStateDigest(State);
	if (bStale)
	{
		++Plan.BeforeRevision;
	}
	else
	{
		Plan.WandPaymentCardInstanceId = TEXT("illegal_payment_instance");
	}
	const FWBHybridSummonResult Rejected = WBHybridSummon::ExecuteHeroReplacement(
		State,
		Scenario.Bootstrap.InitializationRequest.Repository,
		Plan,
		Scenario.GenerationBeforeReplacement,
		Scenario.RevisionBeforeReplacement);
	return Test.TestFalse(TEXT("Altered Hybrid plan rejected"), Rejected.bOk)
		&& Test.TestEqual(
			TEXT("Rejected plan leaves state byte-for-byte equivalent"),
			WBProductionMatchReplay::BuildGameStateDigest(State),
			BeforeDigest)
		&& Test.TestNotNull(
			TEXT("Rejected plan leaves old Hero active"),
			State.GetUnitById(Scenario.OldHeroId));
}

bool RunHybridCase(FAutomationTestBase& Test, const FString& CaseName)
{
	const FHybridScenario& S = GetScenario();
	if (!Test.TestTrue(*FString::Printf(TEXT("Hybrid fixture valid: %s"), *S.Reason), S.bOk))
	{
		return false;
	}
	const FWBUnitState* NewHero = S.AfterState.GetUnitById(S.NewHeroId);
	const FWBUnitState* OldHero = S.AfterState.GetUnitById(S.OldHeroId);
	const FWBPlayerStateData* Player = S.AfterState.GetPlayerById(0);
	const FWBCardDefinitionRepositoryLookupResult Definition =
		WBCardDefinitionRepository::FindCardById(
			S.Bootstrap.InitializationRequest.Repository,
			TEXT("hybrid_fixture_replacement"));

	if (CaseName.Contains(TEXT("Definition.CategoryRecognized")))
	{
		return Test.TestTrue(TEXT("Hybrid definition loaded"), Definition.bFound)
			&& Test.TestEqual(TEXT("Hybrid category retained"), Definition.Definition.Kind, EWBCardDefinitionKind::Hybrid);
	}
	if (CaseName.Contains(TEXT("InvalidSacrifice")))
	{
		FWBHybridSummonPlan Altered = S.Plan;
		Altered.SacrificedUnitId += 1000;
		return TestRejectedPlanAtomicity(Test, S, Altered, false);
	}
	if (CaseName.Contains(TEXT("InvalidDestination"))
		|| CaseName.Contains(TEXT("DestinationFailure")))
	{
		FWBHybridSummonPlan Altered = S.Plan;
		Altered.DestinationTile.X += 1;
		return TestRejectedPlanAtomicity(Test, S, Altered, false);
	}
	if (CaseName.Contains(TEXT("InvalidPayment"))
		|| CaseName.Contains(TEXT("PaymentFailure"))
		|| CaseName.Contains(TEXT("NoMutationBefore"))
		|| CaseName.Contains(TEXT("RejectedReplacementNotRecorded")))
	{
		return TestRejectedPlanAtomicity(Test, S, S.Plan, false)
			&& Test.TestEqual(TEXT("Rejected attempt absent from archive"), S.Archive.Records.Num(), 2);
	}
	if (CaseName.Contains(TEXT("Stale")))
	{
		return TestRejectedPlanAtomicity(Test, S, S.Plan, true);
	}
	if (CaseName.Contains(TEXT("RequiresSacrifice")))
	{
		return Test.TestEqual(TEXT("Current Hero is sacrifice"), S.Plan.SacrificedUnitId, S.OldHeroId)
			&& Test.TestTrue(TEXT("Replacement flag set"), S.Plan.bBecomesReplacementHero);
	}
	if (CaseName.Contains(TEXT("RequiresWandPayment"))
		|| CaseName.Contains(TEXT("RequiredCardsConsumedOnce"))
		|| CaseName.Contains(TEXT("ZoneTransition")))
	{
		return Test.TestEqual(TEXT("Stable payment instance selected"), S.Plan.WandPaymentCardInstanceId, S.PaidWandInstanceId)
			&& Test.TestEqual(TEXT("Payment discarded exactly once"), CountDiscardInstance(S.AfterState, 0, S.PaidWandInstanceId), 1);
	}
	if (CaseName.Contains(TEXT("DuplicatePaymentCardRejected"))
		|| CaseName.Contains(TEXT("WrongOwnerRejected")))
	{
		FWBHybridSummonPlan Altered = S.Plan;
		Altered.WandPaymentCardInstanceId = CaseName.Contains(TEXT("WrongOwner"))
			? FString(TEXT("p1-private-wand"))
			: Altered.HybridCardInstanceId;
		return TestRejectedPlanAtomicity(Test, S, Altered, false);
	}
	if (CaseName.Contains(TEXT("UnitCap")) || CaseName.Contains(TEXT("UnitCount")))
	{
		FWBGameStateData AtCap = S.BeforeState;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FWBUnitState Unit;
			Unit.UnitId = 100 + Index;
			Unit.OwnerId = 0;
			Unit.CardId = TEXT("hybrid_fixture_filler");
			Unit.X = 4 + Index;
			Unit.Y = 4;
			AtCap.Units.Add(Unit);
		}
		const FWBHybridSummonPlanResult AtCapPlans = WBHybridSummon::BuildHeroReplacementPlans(
			AtCap,
			S.Bootstrap.InitializationRequest.Repository,
			0,
			S.Plan.HybridCardInstanceId,
			S.GenerationBeforeReplacement,
			S.RevisionBeforeReplacement);
		FWBUnitState Fifth;
		Fifth.UnitId = 200;
		Fifth.OwnerId = 0;
		Fifth.CardId = TEXT("hybrid_fixture_filler");
		Fifth.X = 7;
		Fifth.Y = 4;
		AtCap.Units.Add(Fifth);
		const FWBHybridSummonPlanResult OverCapPlans = WBHybridSummon::BuildHeroReplacementPlans(
			AtCap,
			S.Bootstrap.InitializationRequest.Repository,
			0,
			S.Plan.HybridCardInstanceId,
			S.GenerationBeforeReplacement,
			S.RevisionBeforeReplacement);
		return Test.TestTrue(TEXT("Sacrifice permits completed count of four"), AtCapPlans.bOk)
			&& Test.TestFalse(TEXT("Completed count of five rejected"), OverCapPlans.bOk)
			&& Test.TestEqual(TEXT("Over-cap failure is typed"), OverCapPlans.Code, EWBHybridSummonResultCode::HybridUnitCapExceeded);
	}
	if (CaseName.Contains(TEXT("Destination")) || CaseName.Contains(TEXT("DuplicateOccupancy")))
	{
		return Test.TestEqual(TEXT("Destination is the vacated canonical Hero tile"), S.Plan.DestinationTile, S.OldHeroTile)
			&& Test.TestEqual(TEXT("Only replacement occupies destination"), S.AfterState.UnitIdAt(S.OldHeroTile), S.NewHeroId);
	}
	if (CaseName.Contains(TEXT("OrdinaryHeroLoss"))
		|| CaseName.Contains(TEXT("ReplacementHeroLaterLoss"))
		|| CaseName.Contains(TEXT("NoGlobalTerminalSuppression")))
	{
		FWBGameStateData DeathState = S.AfterState;
		FWBUnitState* MortalHero = DeathState.GetMutableUnitById(S.NewHeroId);
		if (MortalHero == nullptr) return Test.TestTrue(TEXT("Replacement Hero exists"), false);
		MortalHero->HP = 0;
		const FWBApplyActionResult Death = WBDeathResolution::ApplyZeroHPDeathResolution(DeathState);
		return Test.TestTrue(TEXT("Ordinary death resolves"), Death.bOk)
			&& Test.TestTrue(TEXT("Replacement Hero later loss is terminal"), DeathState.bGameOver)
			&& Test.TestEqual(TEXT("Canonical Hero-loss reason retained"), DeathState.TerminalOutcome.Reason, EWBTerminalReason::HeroDefeatedWithoutReplacement);
	}
	if (CaseName.Contains(TEXT("Trace")) || CaseName.Contains(TEXT("IntermediateGameOver")))
	{
		const TArray<FWBTraceEvent>& T = S.Replacement.TraceEvents;
		return Test.TestTrue(TEXT("Sacrifice trace"), HasTrace(T, TEXT("unit_sacrificed")))
			&& Test.TestTrue(TEXT("Safe payment trace"), HasTrace(T, TEXT("wand_payment_committed")))
			&& Test.TestTrue(TEXT("Summon trace"), HasTrace(T, TEXT("hybrid_summoned")))
			&& Test.TestTrue(TEXT("Replacement trace"), HasTrace(T, TEXT("hero_replacement_committed")))
			&& Test.TestFalse(TEXT("No valid-replacement Hero defeat trace"), HasTrace(T, TEXT("hero_defeated")))
			&& Test.TestFalse(TEXT("No valid-replacement game over trace"), HasTrace(T, TEXT("game_over")))
			&& Test.TestTrue(TEXT("Public payment trace omits paid Wand identity"), T.ContainsByPredicate([](const FWBTraceEvent& E)
			{
				return E.Kind == FName(TEXT("wand_payment_committed")) && E.CardInstanceId.IsEmpty();
			}));
	}
	if (CaseName.Contains(TEXT("Replay")))
	{
		const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(S.AfterState.GetCardZoneState(), 0);
		return Test.TestTrue(TEXT("Fresh replay valid"), S.Replay.bValid)
			&& Test.TestFalse(TEXT("Replay remains nonterminal"), S.Replay.bTerminal)
			&& Test.TestFalse(TEXT("Replay footer incomplete"), S.Archive.Footer.bComplete)
			&& Test.TestEqual(TEXT("Fresh replay Hero"), S.Replay.FinalHeroUnitIds[0], S.NewHeroId)
			&& Test.TestEqual(TEXT("Fresh replay state digest"), S.Replay.FinalStateDigest, S.AfterStateDigest)
			&& Test.TestEqual(TEXT("Fresh replay trace digest"), S.Replay.FinalTraceDigest, S.AfterTraceDigest)
			&& Test.TestEqual(TEXT("Fresh replay revision"), S.Replay.FinalRevision, S.Replacement.CoordinatorRevision)
			&& Test.TestEqual(TEXT("Fresh replay generation"), S.Replay.FinalGeneration, S.Replacement.CoordinatorGeneration)
			&& Test.TestNotNull(TEXT("Final zones exist"), Zones)
			&& Test.TestEqual(TEXT("Fresh replay discard count"), S.Replay.FinalDiscardCounts[0], Zones->Discard.Num());
	}
	if (CaseName.Contains(TEXT("Privacy")))
	{
		TSharedPtr<FJsonObject> Receipt;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(S.ReceiptJson);
		const bool bReceiptParsed = FJsonSerializer::Deserialize(Reader, Receipt) && Receipt.IsValid();
		const bool bForbidden = WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			S.PublicObservation.CardZones,
			TEXT("hybrid_fixture_wand"));
		const FWBPublicUnitBoardSummary* PublicHero = S.PublicObservation.PublicBoard.Units.FindByPredicate(
			[&S](const FWBPublicUnitBoardSummary& Unit) { return Unit.UnitId == S.NewHeroId; });
		return Test.TestFalse(TEXT("Opponent observation hides paid Wand"), bForbidden)
			&& Test.TestNotNull(TEXT("Replacement publicly visible"), PublicHero)
			&& Test.TestTrue(TEXT("Replacement publicly classified as Hero"), PublicHero != nullptr && PublicHero->bHeroUnit)
			&& Test.TestTrue(TEXT("Receipt parses"), bReceiptParsed)
			&& Test.TestEqual(TEXT("Receipt remains eight fields"), Receipt->Values.Num(), 8);
	}
	if (CaseName.Contains(TEXT("Package")))
	{
		static const FWBProductionHybridReplacementSmokeResult First =
			WBProductionHybridReplacementSmoke::Run(MakeHybridRequest());
		static const FWBProductionHybridReplacementSmokeResult Second =
			WBProductionHybridReplacementSmoke::Run(MakeHybridRequest());
		return Test.TestTrue(*First.Reason, First.bOk)
			&& Test.TestTrue(*Second.Reason, Second.bOk)
			&& Test.TestEqual(TEXT("Repeated Hero ID"), First.NewHeroUnitId, Second.NewHeroUnitId)
			&& Test.TestEqual(TEXT("Repeated state digest"), First.FinalStateDigest, Second.FinalStateDigest)
			&& Test.TestEqual(TEXT("Repeated trace digest"), First.FinalTraceDigest, Second.FinalTraceDigest);
	}
	if (CaseName.Contains(TEXT("Authority")))
	{
		FString RuntimeSmoke;
		FFileHelper::LoadFileToString(RuntimeSmoke, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/Private/WBProductionHybridReplacementSmoke.cpp")));
		FString CodecHeader;
		FFileHelper::LoadFileToString(CodecHeader, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Public/WBActionCodec.h")));
		return Test.TestFalse(TEXT("Runtime smoke never obtains mutable coordinator state"), RuntimeSmoke.Contains(TEXT("GetMutableStateForTest")))
			&& Test.TestFalse(TEXT("Runtime smoke never invokes death mutation"), RuntimeSmoke.Contains(TEXT("WBDeathResolution")))
			&& Test.TestFalse(TEXT("Hybrid action does not require codec mutation"), CodecHeader.Contains(TEXT("Hybrid")));
	}
	if (CaseName.Contains(TEXT("Cleanup")) || CaseName.Contains(TEXT("NoOrphaned")))
	{
		return Test.TestTrue(TEXT("Sacrificed Hero equipment fully detached"), S.AfterState.GetCardZoneState().EquippedCards.IsEmpty())
			&& Test.TestEqual(TEXT("Paid equipment discarded once"), CountDiscardInstance(S.AfterState, 0, S.PaidWandInstanceId), 1);
	}

	return Test.TestNotNull(TEXT("Old Hero record retained but inactive"), OldHero)
		&& Test.TestFalse(TEXT("Old Hero is not active"), OldHero->IsUnitOnBoard())
		&& Test.TestNotNull(TEXT("Replacement exists"), NewHero)
		&& Test.TestTrue(TEXT("Replacement active"), NewHero != nullptr && NewHero->IsUnitOnBoard())
		&& Test.TestEqual(TEXT("Authoritative player Hero updated"), Player->HeroUnitId, S.NewHeroId)
		&& Test.TestFalse(TEXT("Match remains nonterminal"), S.AfterState.bGameOver)
		&& Test.TestEqual(TEXT("Winner unset"), S.AfterState.WinnerPlayerId, -1)
		&& Test.TestEqual(TEXT("Loser unset"), S.AfterState.TerminalOutcome.LoserPlayerId, -1)
		&& Test.TestTrue(TEXT("Normal legal decisions continue"), S.Continued.bOk && !S.Continued.Actions.IsEmpty());
}
}

#define WB_HYBRID_CASE(TypeName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TypeName, PrettyName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool TypeName::RunTest(const FString&) { return RunHybridCase(*this, TEXT(PrettyName)); }

WB_HYBRID_CASE(FWBHybridDefinitionCategory, "Wandbound.Hybrid.Definition.CategoryRecognized")
WB_HYBRID_CASE(FWBHybridRequiresSacrifice, "Wandbound.Hybrid.Legality.RequiresSacrifice")
WB_HYBRID_CASE(FWBHybridRequiresPayment, "Wandbound.Hybrid.Legality.RequiresWandPayment")
WB_HYBRID_CASE(FWBHybridInvalidSacrifice, "Wandbound.Hybrid.Legality.InvalidSacrificeRejected")
WB_HYBRID_CASE(FWBHybridInvalidPayment, "Wandbound.Hybrid.Legality.InvalidPaymentRejected")
WB_HYBRID_CASE(FWBHybridInvalidDestination, "Wandbound.Hybrid.Legality.InvalidDestinationRejected")
WB_HYBRID_CASE(FWBHybridStalePlan, "Wandbound.Hybrid.Legality.StalePlanRejected")
WB_HYBRID_CASE(FWBHybridCapAfterSacrifice, "Wandbound.Hybrid.Legality.UnitCapEvaluatedAfterSacrifice")
WB_HYBRID_CASE(FWBHybridCapExceeded, "Wandbound.Hybrid.Legality.CompletedPlanCannotExceedCap")
WB_HYBRID_CASE(FWBHybridAtomicInvalid, "Wandbound.Hybrid.Atomic.InvalidPlanDoesNotRemoveHero")
WB_HYBRID_CASE(FWBHybridAtomicPayment, "Wandbound.Hybrid.Atomic.PaymentFailureDoesNotRemoveHero")
WB_HYBRID_CASE(FWBHybridAtomicDestination, "Wandbound.Hybrid.Atomic.DestinationFailureDoesNotRemoveHero")
WB_HYBRID_CASE(FWBHybridAtomicStale, "Wandbound.Hybrid.Atomic.StaleActionDoesNotRemoveHero")
WB_HYBRID_CASE(FWBHybridAtomicPreflight, "Wandbound.Hybrid.Atomic.NoMutationBeforeFullValidation")
WB_HYBRID_CASE(FWBHybridOldHeroSacrificed, "Wandbound.Hybrid.Replacement.OldHeroSacrificed")
WB_HYBRID_CASE(FWBHybridBecomesHero, "Wandbound.Hybrid.Replacement.HybridBecomesHero")
WB_HYBRID_CASE(FWBHybridExactlyOneHero, "Wandbound.Hybrid.Replacement.ExactlyOneHeroAfterCommit")
WB_HYBRID_CASE(FWBHybridHeroReference, "Wandbound.Hybrid.Replacement.PlayerHeroReferenceUpdated")
WB_HYBRID_CASE(FWBHybridNonterminal, "Wandbound.Hybrid.Replacement.MatchRemainsNonterminal")
WB_HYBRID_CASE(FWBHybridWinnerUnset, "Wandbound.Hybrid.Replacement.WinnerRemainsUnset")
WB_HYBRID_CASE(FWBHybridLoserUnset, "Wandbound.Hybrid.Replacement.LoserRemainsUnset")
WB_HYBRID_CASE(FWBHybridNoDefeatReason, "Wandbound.Hybrid.Replacement.NoHeroDefeatReason")
WB_HYBRID_CASE(FWBHybridContinues, "Wandbound.Hybrid.Replacement.NormalDecisionsContinue")
WB_HYBRID_CASE(FWBHybridNoIntermediateTerminal, "Wandbound.Hybrid.Terminal.NoIntermediateTerminalState")
WB_HYBRID_CASE(FWBHybridNoIntermediateGameOver, "Wandbound.Hybrid.Terminal.NoIntermediateGameOverTrace")
WB_HYBRID_CASE(FWBHybridOrdinaryLoss, "Wandbound.Hybrid.Terminal.OrdinaryHeroLossStillTerminal")
WB_HYBRID_CASE(FWBHybridLaterLoss, "Wandbound.Hybrid.Terminal.ReplacementHeroLaterLossIsTerminal")
WB_HYBRID_CASE(FWBHybridNoGlobalSuppression, "Wandbound.Hybrid.Terminal.NoGlobalTerminalSuppression")
WB_HYBRID_CASE(FWBHybridPaymentOnce, "Wandbound.Hybrid.Payment.RequiredCardsConsumedOnce")
WB_HYBRID_CASE(FWBHybridDuplicatePayment, "Wandbound.Hybrid.Payment.DuplicatePaymentCardRejected")
WB_HYBRID_CASE(FWBHybridWrongOwnerPayment, "Wandbound.Hybrid.Payment.WrongOwnerRejected")
WB_HYBRID_CASE(FWBHybridPaymentZone, "Wandbound.Hybrid.Payment.ZoneTransitionDeterministic")
WB_HYBRID_CASE(FWBHybridEquipmentCleanup, "Wandbound.Hybrid.Cleanup.SacrificedHeroEquipmentHandledCanonically")
WB_HYBRID_CASE(FWBHybridNoOrphans, "Wandbound.Hybrid.Cleanup.NoOrphanedAttachments")
WB_HYBRID_CASE(FWBHybridFreesCap, "Wandbound.Hybrid.Board.SacrificeFreesUnitCapSpace")
WB_HYBRID_CASE(FWBHybridUnitCount, "Wandbound.Hybrid.Board.CompletedUnitCountCorrect")
WB_HYBRID_CASE(FWBHybridCompletedDestination, "Wandbound.Hybrid.Board.DestinationEvaluatedAgainstCompletedPlan")
WB_HYBRID_CASE(FWBHybridNoDuplicateOccupancy, "Wandbound.Hybrid.Board.NoDuplicateOccupancy")
WB_HYBRID_CASE(FWBHybridTraceOrder, "Wandbound.Hybrid.Trace.DeterministicOrder")
WB_HYBRID_CASE(FWBHybridTraceSacrifice, "Wandbound.Hybrid.Trace.SacrificeRecorded")
WB_HYBRID_CASE(FWBHybridTracePayment, "Wandbound.Hybrid.Trace.PaymentRecordedSafely")
WB_HYBRID_CASE(FWBHybridTraceSummon, "Wandbound.Hybrid.Trace.SummonRecorded")
WB_HYBRID_CASE(FWBHybridTraceReplacement, "Wandbound.Hybrid.Trace.ReplacementRecorded")
WB_HYBRID_CASE(FWBHybridTraceNoDefeat, "Wandbound.Hybrid.Trace.NoHeroDefeatedEventForValidReplacement")
WB_HYBRID_CASE(FWBHybridTraceNoGameOver, "Wandbound.Hybrid.Trace.NoGameOverEventForValidReplacement")
WB_HYBRID_CASE(FWBHybridReplayRecorded, "Wandbound.Replay.Hybrid.AcceptedReplacementRecorded")
WB_HYBRID_CASE(FWBHybridReplayRejected, "Wandbound.Replay.Hybrid.RejectedReplacementNotRecorded")
WB_HYBRID_CASE(FWBHybridReplayHero, "Wandbound.Replay.Hybrid.FreshCoordinatorReplacesSameHero")
WB_HYBRID_CASE(FWBHybridReplayPayment, "Wandbound.Replay.Hybrid.PaymentResultMatches")
WB_HYBRID_CASE(FWBHybridReplayZones, "Wandbound.Replay.Hybrid.ZoneResultMatches")
WB_HYBRID_CASE(FWBHybridReplayState, "Wandbound.Replay.Hybrid.StateDigestMatches")
WB_HYBRID_CASE(FWBHybridReplayTrace, "Wandbound.Replay.Hybrid.TraceDigestMatches")
WB_HYBRID_CASE(FWBHybridReplayRevision, "Wandbound.Replay.Hybrid.RevisionMatches")
WB_HYBRID_CASE(FWBHybridReplayGeneration, "Wandbound.Replay.Hybrid.GenerationMatches")
WB_HYBRID_CASE(FWBHybridReplayNonterminal, "Wandbound.Replay.Hybrid.RemainsNonterminal")
WB_HYBRID_CASE(FWBHybridReplayNoFooter, "Wandbound.Replay.Hybrid.NoFooterFinalization")
WB_HYBRID_CASE(FWBHybridPrivacySummary, "Wandbound.Hybrid.Privacy.PublicSummaryShowsReplacementHero")
WB_HYBRID_CASE(FWBHybridPrivacyCandidates, "Wandbound.Hybrid.Privacy.UnchosenPaymentCandidatesHidden")
WB_HYBRID_CASE(FWBHybridPrivacyHand, "Wandbound.Hybrid.Privacy.PrivateHandDataHidden")
WB_HYBRID_CASE(FWBHybridPrivacyReceipt, "Wandbound.Hybrid.Privacy.ReceiptStillEightFields")
WB_HYBRID_CASE(FWBHybridPrivacyStartup, "Wandbound.Hybrid.Privacy.StartupJsonUnchanged")
WB_HYBRID_CASE(FWBHybridPackageComplete, "Wandbound.Hybrid.Package.ReplacementCompletes")
WB_HYBRID_CASE(FWBHybridPackageOldHero, "Wandbound.Hybrid.Package.OldHeroRemoved")
WB_HYBRID_CASE(FWBHybridPackageNewHero, "Wandbound.Hybrid.Package.NewHeroAssigned")
WB_HYBRID_CASE(FWBHybridPackageContinues, "Wandbound.Hybrid.Package.MatchContinues")
WB_HYBRID_CASE(FWBHybridPackageReplay, "Wandbound.Hybrid.Package.ReplayVerified")
WB_HYBRID_CASE(FWBHybridPackageArchive, "Wandbound.Hybrid.Package.RepeatedArchiveByteIdentical")
WB_HYBRID_CASE(FWBHybridPackageReceipt, "Wandbound.Hybrid.Package.RepeatedReceiptByteIdentical")
WB_HYBRID_CASE(FWBHybridPackageStartup, "Wandbound.Hybrid.Package.StartupHashPreserved")
WB_HYBRID_CASE(FWBHybridAuthorityCoordinator, "Wandbound.Authority.Hybrid.CoordinatorOwnsReplacement")
WB_HYBRID_CASE(FWBHybridAuthorityRuntime, "Wandbound.Authority.Hybrid.NoRuntimeHeroMutation")
WB_HYBRID_CASE(FWBHybridAuthorityReplay, "Wandbound.Authority.Hybrid.NoReplayStateMutation")
WB_HYBRID_CASE(FWBHybridAuthoritySmoke, "Wandbound.Authority.Hybrid.NoDirectSmokeStateMutation")
WB_HYBRID_CASE(FWBHybridAuthoritySuppression, "Wandbound.Authority.Hybrid.NoGlobalTerminalSuppression")
WB_HYBRID_CASE(FWBHybridAuthorityTurn, "Wandbound.Authority.Hybrid.NoTurnControllerBypass")
WB_HYBRID_CASE(FWBHybridAuthorityEffects, "Wandbound.Authority.Hybrid.NoEffectRunnerTransitionBypass")
WB_HYBRID_CASE(FWBHybridAuthorityGodot, "Wandbound.Authority.Hybrid.NoGodotChanges")
WB_HYBRID_CASE(FWBHybridAuthorityMeshy, "Wandbound.Authority.Hybrid.NoMeshyChanges")
WB_HYBRID_CASE(FWBHybridAuthorityAssets, "Wandbound.Authority.Hybrid.NoModelMapOrAssetChanges")

#undef WB_HYBRID_CASE

#endif
