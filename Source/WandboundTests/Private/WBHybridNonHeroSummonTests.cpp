#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBHybridSummon.h"
#include "WBMarkerResolution.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionHybridNonHeroSmoke.h"
#include "WBProductionHybridReplacementSmoke.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBPublicBoardSummary.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString FixturePath(const FString& Name)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/HybridNonHeroFixture"),
		Name);
}

FWBProductionRuntimeBootstrapRequest MakeRequest()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FixturePath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FixturePath(TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	return Request;
}

FWBProductionRuntimeBootstrapRequest MakeHeroRequest()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/HybridReplacementFixture/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/HybridReplacementFixture/match_spec.json"));
	Request.bAllowTestBundle = true;
	return Request;
}

const FWBMatchLegalAction* FindCharacterSummon(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& CardId)
{
	return Actions.FindByPredicate([&CardId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& !Action.bHybridSummon
			&& Action.SummonRequest.SourceCardId == CardId;
	});
}

const FWBMatchLegalAction* FindEquip(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 TargetUnitId)
{
	return Actions.FindByPredicate([TargetUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip
			&& Action.EquipRequest.SourceCardId == TEXT("hybrid_nonhero_wand")
			&& Action.EquipRequest.TargetUnitId == TargetUnitId;
	});
}

const FWBPublicUnitBoardSummary* FindPublicUnit(
	const FWBMatchObservation& Observation,
	const FString& CardId)
{
	return Observation.PublicBoard.Units.FindByPredicate(
		[&CardId](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.CardId == CardId;
		});
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(Kind);
	});
}

int32 TraceIndex(const TArray<FWBTraceEvent>& Events, const TCHAR* Kind)
{
	return Events.IndexOfByPredicate([Kind](const FWBTraceEvent& Event)
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

const FWBEquippedCardEntry* FindEquippedTo(
	const FWBGameStateData& State,
	const int32 UnitId)
{
	return State.GetCardZoneState().EquippedCards.FindByPredicate(
		[UnitId](const FWBEquippedCardEntry& Entry)
		{
			return Entry.EquippedToUnitId == UnitId;
		});
}

struct FNonHeroScenario
{
	bool bOk = false;
	FString Reason;
	FWBProductionRuntimeBootstrapRequest Request;
	FWBProductionRuntimeBootstrapResult Bootstrap;
	FWBGameStateData BeforeState;
	FWBGameStateData AfterState;
	TArray<FWBHybridSummonPlan> Plans;
	FWBHybridSummonPlan SelectedPlan;
	FWBHybridSummonResult DirectResult;
	FWBMatchOperationResult CoordinatorResult;
	FWBMatchLegalActionGenerationResult Continued;
	FWBMatchObservation PublicForPlayer;
	FWBMatchObservation PublicForOpponent;
	FWBProductionMatchReplayArchive Archive;
	FString SerializedArchive;
	FString ReceiptJson;
	FWBProductionMatchReplayRunResult Replay;
	FString FinalStateDigest;
	FString FinalTraceDigest;
	int32 HeroId = -1;
	int32 AlphaId = -1;
	int32 BetaId = -1;
	int32 NewHybridId = -1;
	FWBTile HeroTile;
	FWBTile AlphaTile;
	FWBTile BetaTile;
	FString AlphaWandInstance;
	FString BetaWandInstance;
	FString HandWandInstance;
	int32 Generation = -1;
	int32 Revision = -1;
};

bool Submit(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FString& OutReason)
{
	const FWBMatchOperationResult Applied = Coordinator.SubmitActionId(
		Action.PlayerId, Action.ActionId);
	if (!Applied.bOk)
	{
		OutReason = Applied.Reason;
		return false;
	}
	Recorder.CaptureCommittedActions(Coordinator);
	if (!Recorder.IsAvailable())
	{
		OutReason = Recorder.GetReceipt().FailureCode;
		return false;
	}
	return true;
}

FNonHeroScenario BuildScenario()
{
	FNonHeroScenario S;
	S.Request = MakeRequest();
	S.Bootstrap = WBProductionRuntimeBootstrap::Build(S.Request);
	if (!S.Bootstrap.bOk)
	{
		S.Reason = S.Bootstrap.Reason;
		return S;
	}
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		S.Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		S.Reason = Started.Reason;
		return S;
	}
	const FWBMatchObservation Initial = Coordinator.BuildObservation(0);
	const FWBPublicUnitBoardSummary* Hero =
		Initial.PublicBoard.Units.FindByPredicate(
			[](const FWBPublicUnitBoardSummary& Unit)
			{
				return Unit.OwnerId == 0 && Unit.bHeroUnit;
			});
	if (Hero == nullptr)
	{
		S.Reason = TEXT("fixture_hero_missing");
		return S;
	}
	S.HeroId = Hero->UnitId;
	S.HeroTile = FWBTile(Hero->X, Hero->Y);

	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(
		WBProductionMatchReplayRuntime::BuildMetadata(S.Bootstrap),
		Coordinator))
	{
		S.Reason = Recorder.GetReceipt().FailureCode;
		return S;
	}
	const FWBMatchLegalAction* AlphaSummon = FindCharacterSummon(
		Started.NextLegalActions,
		TEXT("hybrid_nonhero_sacrifice_alpha"));
	if (AlphaSummon == nullptr || !Submit(Coordinator, Recorder, *AlphaSummon, S.Reason))
	{
		if (S.Reason.IsEmpty()) S.Reason = TEXT("alpha_summon_missing");
		return S;
	}
	const FWBPublicUnitBoardSummary* Alpha = FindPublicUnit(
		Coordinator.BuildObservation(0),
		TEXT("hybrid_nonhero_sacrifice_alpha"));
	if (Alpha == nullptr)
	{
		S.Reason = TEXT("alpha_missing");
		return S;
	}
	S.AlphaId = Alpha->UnitId;
	S.AlphaTile = FWBTile(Alpha->X, Alpha->Y);

	const FWBMatchLegalActionGenerationResult AfterAlpha =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* BetaSummon = AfterAlpha.bOk
		? FindCharacterSummon(
			AfterAlpha.Actions,
			TEXT("hybrid_nonhero_sacrifice_beta"))
		: nullptr;
	if (BetaSummon == nullptr || !Submit(Coordinator, Recorder, *BetaSummon, S.Reason))
	{
		if (S.Reason.IsEmpty()) S.Reason = TEXT("beta_summon_missing");
		return S;
	}
	const FWBPublicUnitBoardSummary* Beta = FindPublicUnit(
		Coordinator.BuildObservation(0),
		TEXT("hybrid_nonhero_sacrifice_beta"));
	if (Beta == nullptr)
	{
		S.Reason = TEXT("beta_missing");
		return S;
	}
	S.BetaId = Beta->UnitId;
	S.BetaTile = FWBTile(Beta->X, Beta->Y);

	FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* AlphaEquip = Legal.bOk
		? FindEquip(Legal.Actions, S.AlphaId)
		: nullptr;
	if (AlphaEquip == nullptr)
	{
		S.Reason = TEXT("alpha_equip_missing");
		return S;
	}
	S.AlphaWandInstance = AlphaEquip->EquipRequest.SourceInstanceId;
	if (!Submit(Coordinator, Recorder, *AlphaEquip, S.Reason)) return S;

	Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* BetaEquip = Legal.bOk
		? FindEquip(Legal.Actions, S.BetaId)
		: nullptr;
	if (BetaEquip == nullptr)
	{
		S.Reason = TEXT("beta_equip_missing");
		return S;
	}
	S.BetaWandInstance = BetaEquip->EquipRequest.SourceInstanceId;
	if (!Submit(Coordinator, Recorder, *BetaEquip, S.Reason)) return S;

	Legal = Coordinator.EnumerateLegalActions();
	if (!Legal.bOk)
	{
		S.Reason = Legal.Reason;
		return S;
	}
	const FWBMatchLegalAction* HybridAction = Legal.Actions.FindByPredicate(
		[&S](const FWBMatchLegalAction& Action)
		{
			return Action.bHybridSummon
				&& !Action.bHybridHeroReplacement
				&& Action.HybridSummonPlan.SacrificedUnitId == S.AlphaId
				&& Action.HybridSummonPlan.DestinationTile == S.AlphaTile
				&& Action.HybridSummonPlan.WandPaymentSource
					== EWBHybridWandPaymentSource::SacrificedUnit;
		});
	if (HybridAction == nullptr)
	{
		S.Reason = TEXT("nonhero_hybrid_action_missing");
		return S;
	}
	S.SelectedPlan = HybridAction->HybridSummonPlan;
	S.Generation = Coordinator.GetCoordinatorGeneration();
	S.Revision = Coordinator.GetCoordinatorRevision();
	S.BeforeState = Coordinator.GetState();
	const FWBHybridSummonPlanResult PlanResult = WBHybridSummon::BuildSummonPlans(
		S.BeforeState,
		S.Bootstrap.InitializationRequest.Repository,
		0,
		S.SelectedPlan.HybridCardInstanceId,
		S.Generation,
		S.Revision);
	if (!PlanResult.bOk)
	{
		S.Reason = PlanResult.Reason;
		return S;
	}
	S.Plans = PlanResult.Plans;
	const FWBHybridSummonPlan* HandPlan = S.Plans.FindByPredicate(
		[&S](const FWBHybridSummonPlan& Plan)
		{
			return Plan.SacrificedUnitId == S.AlphaId
				&& Plan.DestinationTile == S.AlphaTile
				&& Plan.WandPaymentSource == EWBHybridWandPaymentSource::Hand;
		});
	if (HandPlan == nullptr)
	{
		S.Reason = TEXT("hand_payment_plan_missing");
		return S;
	}
	S.HandWandInstance = HandPlan->WandPaymentCardInstanceId;

	FWBGameStateData DirectState = S.BeforeState;
	S.DirectResult = WBHybridSummon::ExecuteSummon(
		DirectState,
		S.Bootstrap.InitializationRequest.Repository,
		S.SelectedPlan,
		S.Generation,
		S.Revision);
	if (!S.DirectResult.bOk)
	{
		S.Reason = S.DirectResult.Reason;
		return S;
	}

	S.CoordinatorResult = Coordinator.SubmitActionId(
		HybridAction->PlayerId,
		HybridAction->ActionId);
	if (!S.CoordinatorResult.bOk)
	{
		S.Reason = S.CoordinatorResult.Reason;
		return S;
	}
	Recorder.CaptureCommittedActions(Coordinator);
	S.AfterState = Coordinator.GetState();
	S.NewHybridId = S.DirectResult.NewHybridUnitId;
	S.Continued = Coordinator.EnumerateLegalActions();
	S.PublicForPlayer = Coordinator.BuildObservation(0);
	S.PublicForOpponent = Coordinator.BuildObservation(1);
	S.Archive = Recorder.GetArchive();
	S.SerializedArchive = WBProductionMatchReplay::Serialize(S.Archive);
	S.ReceiptJson = WBProductionMatchReplay::SerializeReceipt(Recorder.GetReceipt());
	S.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	S.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	FWBProductionMatchReplayRunRequest ReplayRequest;
	ReplayRequest.SerializedArchive = S.SerializedArchive;
	ReplayRequest.BootstrapRequest = S.Request;
	S.Replay = FWBProductionMatchReplayRunner::Run(ReplayRequest);
	S.bOk = true;
	return S;
}

const FNonHeroScenario& GetScenario()
{
	static const FNonHeroScenario Scenario = BuildScenario();
	return Scenario;
}

bool RejectsWithoutMutation(
	FAutomationTestBase& Test,
	const FNonHeroScenario& S,
	FWBHybridSummonPlan Altered,
	const bool bStale = false)
{
	FWBGameStateData State = S.BeforeState;
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(State);
	if (bStale) ++Altered.BeforeRevision;
	const FWBHybridSummonResult Rejected = WBHybridSummon::ExecuteSummon(
		State,
		S.Bootstrap.InitializationRequest.Repository,
		Altered,
		S.Generation,
		S.Revision);
	return Test.TestFalse(TEXT("Altered plan rejected"), Rejected.bOk)
		&& Test.TestEqual(
			TEXT("Rejected plan leaves state unchanged"),
			WBProductionMatchReplay::BuildGameStateDigest(State),
			Before);
}

bool RunCase(FAutomationTestBase& Test, const FString& CaseName)
{
	const FNonHeroScenario& S = GetScenario();
	if (!Test.TestTrue(*S.Reason, S.bOk)) return false;
	const FWBUnitState* Hero = S.AfterState.GetUnitById(S.HeroId);
	const FWBUnitState* Sacrificed = S.AfterState.GetUnitById(S.AlphaId);
	const FWBUnitState* Hybrid = S.AfterState.GetUnitById(S.NewHybridId);
	const FWBPlayerStateData* Player = S.AfterState.GetPlayerById(0);

	if (CaseName.Contains(TEXT("Planner.")))
	{
		const bool bHasHero = S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.HeroId
					&& P.bBecomesReplacementHero;
			});
		const bool bHasAlpha = S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& !P.bBecomesReplacementHero;
			});
		const bool bAllSacrificesValid = S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId != S.HeroId
					&& P.SacrificedUnitId != S.AlphaId
					&& P.SacrificedUnitId != S.BetaId;
			}) == false;
		const bool bAllNonHeroAdjacent = !S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return !P.bBecomesReplacementHero
					&& FMath::Abs(P.DestinationTile.X - S.HeroTile.X)
						+ FMath::Abs(P.DestinationTile.Y - S.HeroTile.Y) != 1;
			});
		const bool bAlphaOwnTile = S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& P.DestinationTile == S.AlphaTile;
			});
		const bool bAlphaCannotUseBetaTile = !S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& P.DestinationTile == S.BetaTile;
			});
		return Test.TestTrue(TEXT("Hero branch remains"), bHasHero)
			&& Test.TestTrue(TEXT("Controlled non-Hero branch exists"), bHasAlpha)
			&& Test.TestTrue(TEXT("Only controlled active Characters are sacrifices"), bAllSacrificesValid)
			&& Test.TestTrue(TEXT("Non-Hero destinations are orthogonally adjacent"), bAllNonHeroAdjacent)
			&& Test.TestTrue(TEXT("Sacrificed adjacent tile is eligible"), bAlphaOwnTile)
			&& Test.TestTrue(TEXT("Other occupied adjacent tile is excluded"), bAlphaCannotUseBetaTile);
	}
	if (CaseName.Contains(TEXT("Payment.")))
	{
		const bool bHand = S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& P.WandPaymentSource == EWBHybridWandPaymentSource::Hand;
			});
		const bool bAttached = S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& P.WandPaymentCardInstanceId == S.AlphaWandInstance;
			});
		const bool bOtherExcluded = !S.Plans.ContainsByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& P.WandPaymentCardInstanceId == S.BetaWandInstance;
			});
		TSet<FString> Ids;
		bool bUnique = true;
		for (const FWBHybridSummonPlan& Plan : S.Plans)
		{
			const FString Id = WBHybridSummon::BuildStableActionId(Plan);
			bUnique &= !Ids.Contains(Id);
			Ids.Add(Id);
		}
		return Test.TestTrue(TEXT("Hand payment exists"), bHand)
			&& Test.TestTrue(TEXT("Selected-unit equipment payment exists"), bAttached)
			&& Test.TestTrue(TEXT("Other-unit equipment excluded"), bOtherExcluded)
			&& Test.TestTrue(TEXT("Payment plans are unique"), bUnique);
	}
	if (CaseName.Contains(TEXT("Cap.")))
	{
		FWBGameStateData AtFour = S.BeforeState;
		FWBUnitState Fourth;
		Fourth.UnitId = 1000;
		Fourth.OwnerId = 0;
		Fourth.CardId = TEXT("hybrid_nonhero_filler");
		Fourth.X = 8;
		Fourth.Y = 8;
		AtFour.Units.Add(Fourth);
		const FWBHybridSummonPlanResult Four = WBHybridSummon::BuildSummonPlans(
			AtFour, S.Bootstrap.InitializationRequest.Repository, 0,
			S.SelectedPlan.HybridCardInstanceId, S.Generation, S.Revision);
		FWBUnitState Fifth = Fourth;
		Fifth.UnitId = 1001;
		Fifth.X = 7;
		AtFour.Units.Add(Fifth);
		const FWBHybridSummonPlanResult Five = WBHybridSummon::BuildSummonPlans(
			AtFour, S.Bootstrap.InitializationRequest.Repository, 0,
			S.SelectedPlan.HybridCardInstanceId, S.Generation, S.Revision);
		return Test.TestTrue(TEXT("Four-unit completed transaction legal"), Four.bOk)
			&& Test.TestFalse(TEXT("Completed count above four rejected"), Five.bOk)
			&& Test.TestEqual(TEXT("Cap failure typed"), Five.Code, EWBHybridSummonResultCode::HybridUnitCapExceeded);
	}
	if (CaseName.Contains(TEXT("Atomic.")))
	{
		FWBHybridSummonPlan Altered = S.SelectedPlan;
		bool bStale = false;
		if (CaseName.Contains(TEXT("Sacrifice"))) Altered.SacrificedUnitId += 1000;
		else if (CaseName.Contains(TEXT("Payment"))) Altered.WandPaymentCardInstanceId = TEXT("invalid_wand");
		else if (CaseName.Contains(TEXT("Destination"))) Altered.DestinationTile = FWBTile(8, 8);
		else if (CaseName.Contains(TEXT("Stale"))) bStale = true;
		else Altered.bBecomesReplacementHero = true;
		return RejectsWithoutMutation(Test, S, Altered, bStale);
	}
	if (CaseName.Contains(TEXT("Execution.")))
	{
		const FWBCardDefinitionRepositoryLookupResult Definition =
			WBCardDefinitionRepository::FindCardById(
				S.Bootstrap.InitializationRequest.Repository,
				TEXT("hybrid_nonhero_summon"));
		return Test.TestNotNull(TEXT("Sacrifice record retained"), Sacrificed)
			&& Test.TestFalse(TEXT("Sacrifice is inactive"), Sacrificed->IsUnitOnBoard())
			&& Test.TestNotNull(TEXT("Hybrid created"), Hybrid)
			&& Test.TestTrue(TEXT("Hybrid active"), Hybrid != nullptr && Hybrid->IsUnitOnBoard())
			&& Test.TestEqual(TEXT("Printed HP"), Hybrid->HP, Definition.Definition.CharacterStats.HP)
			&& Test.TestEqual(TEXT("Hero ID unchanged"), Player->HeroUnitId, S.HeroId)
			&& Test.TestNotEqual(TEXT("Hybrid is not Hero"), S.NewHybridId, S.HeroId)
			&& Test.TestEqual(TEXT("Destination"), FWBTile(Hybrid->X, Hybrid->Y), S.AlphaTile)
			&& Test.TestEqual(TEXT("No immediate attacks"), Hybrid->AttacksLeft, 0)
			&& Test.TestEqual(TEXT("No immediate MP"), Hybrid->MPRemaining, 0);
	}
	if (CaseName.Contains(TEXT("Cleanup.")))
	{
		const FWBHybridSummonPlan* HandPlan = S.Plans.FindByPredicate(
			[&S](const FWBHybridSummonPlan& P)
			{
				return P.SacrificedUnitId == S.AlphaId
					&& P.DestinationTile == S.AlphaTile
					&& P.WandPaymentSource == EWBHybridWandPaymentSource::Hand;
			});
		if (!Test.TestNotNull(TEXT("Hand plan exists"), HandPlan)) return false;
		FWBGameStateData HandState = S.BeforeState;
		const FWBHybridSummonResult HandResult = WBHybridSummon::ExecuteSummon(
			HandState, S.Bootstrap.InitializationRequest.Repository, *HandPlan,
			S.Generation, S.Revision);
		const FWBEquippedCardEntry* BetaEquipment = FindEquippedTo(HandState, S.BetaId);
		return Test.TestTrue(TEXT("Hand-payment transaction succeeds"), HandResult.bOk)
			&& Test.TestEqual(TEXT("Hand payment discarded once"), CountDiscardInstance(HandState, 0, S.HandWandInstance), 1)
			&& Test.TestEqual(TEXT("Alpha equipment cleaned once"), CountDiscardInstance(HandState, 0, S.AlphaWandInstance), 1)
			&& Test.TestNotNull(TEXT("Other-unit equipment untouched"), BetaEquipment)
			&& Test.TestNull(TEXT("No orphan sacrifice attachment"), FindEquippedTo(HandState, S.AlphaId));
	}
	if (CaseName.Contains(TEXT("Terminal.")))
	{
		if (CaseName.Contains(TEXT("LaterLoss")))
		{
			FWBGameStateData DeathState = S.AfterState;
			DeathState.GetMutableUnitById(S.HeroId)->HP = 0;
			const FWBApplyActionResult Death = WBDeathResolution::ApplyZeroHPDeathResolution(DeathState);
			return Test.TestTrue(TEXT("Later Hero death resolves"), Death.bOk)
				&& Test.TestTrue(TEXT("Later Hero death is terminal"), DeathState.bGameOver);
		}
		return Test.TestFalse(TEXT("Sacrifice is not terminal"), S.AfterState.bGameOver)
			&& Test.TestEqual(TEXT("Winner unset"), S.AfterState.WinnerPlayerId, -1)
			&& Test.TestEqual(TEXT("Loser unset"), S.AfterState.TerminalOutcome.LoserPlayerId, -1)
			&& Test.TestEqual(TEXT("Terminal reason none"), S.AfterState.TerminalOutcome.Reason, EWBTerminalReason::None);
	}
	if (CaseName.Contains(TEXT("Marker.")))
	{
		FWBGameStateData MarkerState = S.BeforeState;
		MarkerState.GetMutableCardZoneStateForTest().MarkerPlaceholders[0].Tile =
			S.AlphaTile;
		const FWBHybridSummonResult Summoned = WBHybridSummon::ExecuteSummon(
			MarkerState, S.Bootstrap.InitializationRequest.Repository,
			S.SelectedPlan, S.Generation, S.Revision);
		if (!Test.TestTrue(TEXT("Hybrid summon succeeds before marker"), Summoned.bOk)) return false;
		FWBUnitState* Mortal = MarkerState.GetMutableUnitById(Summoned.NewHybridUnitId);
		if (CaseName.Contains(TEXT("Death")))
		{
			Mortal->HP = 1;
			Mortal->SetArmorForTest(0, 0);
		}
		const FWBMarkerResolutionResult Marker = WBMarkerResolution::ResolveMarkerAtUnitTile(
			MarkerState, S.Bootstrap.InitializationRequest.Repository,
			Summoned.NewHybridUnitId);
		return Test.TestTrue(TEXT("Existing marker path runs"), Marker.bOk && Marker.bMarkerFound)
			&& Test.TestTrue(TEXT("Trap damage applies"), Marker.bTrapDamageApplied)
			&& Test.TestEqual(TEXT("Original Hero remains authoritative"), MarkerState.GetPlayerById(0)->HeroUnitId, S.HeroId)
			&& Test.TestFalse(TEXT("Living original Hero prevents terminal"), MarkerState.bGameOver);
	}
	if (CaseName.Contains(TEXT("ActionId.")))
	{
		FWBHybridSummonPlan Changed = S.SelectedPlan;
		if (CaseName.Contains(TEXT("SacrificeChanges"))) Changed.SacrificedUnitId = S.BetaId;
		else if (CaseName.Contains(TEXT("DestinationChanges"))) Changed.DestinationTile = S.HeroTile;
		else if (CaseName.Contains(TEXT("PaymentChanges")))
		{
			Changed.WandPaymentSource = EWBHybridWandPaymentSource::Hand;
			Changed.WandPaymentCardInstanceId = S.HandWandInstance;
			Changed.WandPaymentUnitId = -1;
		}
		const FString OriginalId = WBHybridSummon::BuildStableActionId(S.SelectedPlan);
		const FString ChangedId = WBHybridSummon::BuildStableActionId(Changed);
		if (CaseName.Contains(TEXT("SamePlanStable")))
			return Test.TestEqual(TEXT("Same plan ID stable"), OriginalId, WBHybridSummon::BuildStableActionId(S.SelectedPlan));
		if (CaseName.Contains(TEXT("HeroReplacementIdsUnchanged")))
			return Test.TestTrue(TEXT("Established ID format retained"), OriginalId.StartsWith(TEXT("hybrid_summon:p0:i")) && OriginalId.Contains(TEXT(":w")));
		return Test.TestNotEqual(TEXT("Changed choice changes ID"), OriginalId, ChangedId);
	}
	if (CaseName.Contains(TEXT("Trace.")))
	{
		const TArray<FWBTraceEvent>& T = S.CoordinatorResult.TraceEvents;
		return Test.TestTrue(TEXT("Unit sacrifice trace"), HasTrace(T, TEXT("unit_sacrificed")))
			&& Test.TestTrue(TEXT("Safe payment trace"), HasTrace(T, TEXT("wand_payment_committed")))
			&& Test.TestTrue(TEXT("Hybrid summoned trace"), HasTrace(T, TEXT("hybrid_summoned")))
			&& Test.TestTrue(TEXT("Order deterministic"), TraceIndex(T, TEXT("unit_sacrificed")) < TraceIndex(T, TEXT("wand_payment_committed")) && TraceIndex(T, TEXT("wand_payment_committed")) < TraceIndex(T, TEXT("hybrid_summoned")))
			&& Test.TestFalse(TEXT("No Hero sacrifice event"), HasTrace(T, TEXT("hero_sacrifice_committed")))
			&& Test.TestFalse(TEXT("No Hero replacement event"), HasTrace(T, TEXT("hero_replacement_committed")))
			&& Test.TestFalse(TEXT("No Hero defeat event"), HasTrace(T, TEXT("hero_defeated")))
			&& Test.TestFalse(TEXT("No game-over event"), HasTrace(T, TEXT("game_over")));
	}
	if (CaseName.Contains(TEXT("Replay.")))
	{
		return Test.TestEqual(TEXT("One record per submitted action"), S.Archive.Records.Num(), 5)
			&& Test.TestTrue(TEXT("Fresh replay valid"), S.Replay.bValid)
			&& Test.TestFalse(TEXT("Fresh replay nonterminal"), S.Replay.bTerminal)
			&& Test.TestEqual(TEXT("Fresh replay Hero unchanged"), S.Replay.FinalHeroUnitIds[0], S.HeroId)
			&& Test.TestEqual(TEXT("State digest parity"), S.Replay.FinalStateDigest, S.FinalStateDigest)
			&& Test.TestEqual(TEXT("Trace digest parity"), S.Replay.FinalTraceDigest, S.FinalTraceDigest)
			&& Test.TestEqual(TEXT("Generation parity"), S.Replay.FinalGeneration, S.CoordinatorResult.CoordinatorGeneration)
			&& Test.TestEqual(TEXT("Revision parity"), S.Replay.FinalRevision, S.CoordinatorResult.CoordinatorRevision);
	}
	if (CaseName.Contains(TEXT("Regression.")))
	{
		static const FWBProductionHybridReplacementSmokeResult HeroSmoke =
			WBProductionHybridReplacementSmoke::Run(MakeHeroRequest());
		return Test.TestTrue(*HeroSmoke.Reason, HeroSmoke.bOk)
			&& Test.TestNotEqual(TEXT("Hero replacement changes Hero ID"), HeroSmoke.OldHeroUnitId, HeroSmoke.NewHeroUnitId);
	}
	if (CaseName.Contains(TEXT("Privacy.")))
	{
		TSharedPtr<FJsonObject> Receipt;
		const bool bParsed = FJsonSerializer::Deserialize(
			TJsonReaderFactory<>::Create(S.ReceiptJson), Receipt)
			&& Receipt.IsValid();
		const bool bOpponentLeak =
			WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
				S.PublicForOpponent.CardZones,
				TEXT("hybrid_nonhero_wand"));
		const bool bPaymentLeak = S.CoordinatorResult.TraceEvents.ContainsByPredicate(
			[&S](const FWBTraceEvent& Trace)
			{
				return Trace.CardInstanceId == S.AlphaWandInstance
					|| Trace.CardId == TEXT("hybrid_nonhero_wand");
			});
		return Test.TestFalse(TEXT("Opponent hand/payment hidden"), bOpponentLeak)
			&& Test.TestFalse(TEXT("Paid Wand omitted from public-safe trace"), bPaymentLeak)
			&& Test.TestTrue(TEXT("Receipt parses"), bParsed)
			&& Test.TestEqual(TEXT("Receipt exactly eight fields"), Receipt->Values.Num(), 8);
	}
	if (CaseName.Contains(TEXT("Authority.")))
	{
		FString CoordinatorSource;
		FString SmokeSource;
		FString CodecSource;
		FFileHelper::LoadFileToString(CoordinatorSource, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp")));
		FFileHelper::LoadFileToString(SmokeSource, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/Private/WBProductionHybridNonHeroSmoke.cpp")));
		FFileHelper::LoadFileToString(CodecSource, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Public/WBActionCodec.h")));
		return Test.TestTrue(TEXT("Coordinator owns unified Hybrid action"), CoordinatorSource.Contains(TEXT("BuildSummonPlans")) && CoordinatorSource.Contains(TEXT("ExecuteSummon")))
			&& Test.TestFalse(TEXT("Smoke has no mutable-state access"), SmokeSource.Contains(TEXT("GetMutableStateForTest")))
			&& Test.TestFalse(TEXT("Smoke has no death mutation"), SmokeSource.Contains(TEXT("WBDeathResolution")))
			&& Test.TestFalse(TEXT("Action codec remains Hybrid-free"), CodecSource.Contains(TEXT("Hybrid")));
	}

	return Test.TestNotNull(TEXT("Original Hero active"), Hero)
		&& Test.TestTrue(TEXT("Original Hero remains active"), Hero->IsUnitOnBoard())
		&& Test.TestNotNull(TEXT("Hybrid active"), Hybrid)
		&& Test.TestTrue(TEXT("Legal actions continue"), S.Continued.bOk && !S.Continued.Actions.IsEmpty());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBHybridNonHeroFixtureBundleLoads,
	"Wandbound.Hybrid.NonHero.Fixture.BundleLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBHybridNonHeroFixtureBundleLoads::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(
			FixturePath(TEXT("root_manifest.json")));
	return TestTrue(*Loaded.Reason, Loaded.bOk && Loaded.Snapshot.IsValid())
		&& TestEqual(
			TEXT("Canonical fixture digest"),
			Loaded.Snapshot->ContentDigest,
			FString(TEXT("42f743203d5d07b674fcc68c603effb35700160d4443e765835f2662ed34c108")));
}

#define WB_NONHERO_CASE(TypeName, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TypeName, PrettyName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool TypeName::RunTest(const FString&) { return RunCase(*this, TEXT(PrettyName)); }

WB_NONHERO_CASE(FWBNonHeroPlannerControlled, "Wandbound.Hybrid.NonHero.Planner.ControlledCharacterEligible")
WB_NONHERO_CASE(FWBNonHeroPlannerCoexist, "Wandbound.Hybrid.NonHero.Planner.HeroAndNonHeroBranchesCoexist")
WB_NONHERO_CASE(FWBNonHeroPlannerOpponent, "Wandbound.Hybrid.NonHero.Planner.OpponentCharacterExcluded")
WB_NONHERO_CASE(FWBNonHeroPlannerInactive, "Wandbound.Hybrid.NonHero.Planner.InactiveCharacterExcluded")
WB_NONHERO_CASE(FWBNonHeroPlannerHybrid, "Wandbound.Hybrid.NonHero.Planner.HybridUnitExcludedAsCharacterSacrifice")
WB_NONHERO_CASE(FWBNonHeroPlannerAdjacent, "Wandbound.Hybrid.NonHero.Planner.AdjacentDestinationsEnumerated")
WB_NONHERO_CASE(FWBNonHeroPlannerDiagonal, "Wandbound.Hybrid.NonHero.Planner.DiagonalDestinationExcluded")
WB_NONHERO_CASE(FWBNonHeroPlannerBounds, "Wandbound.Hybrid.NonHero.Planner.OutOfBoundsDestinationExcluded")
WB_NONHERO_CASE(FWBNonHeroPlannerOccupied, "Wandbound.Hybrid.NonHero.Planner.OccupiedDestinationExcluded")
WB_NONHERO_CASE(FWBNonHeroPlannerOwnTile, "Wandbound.Hybrid.NonHero.Planner.SacrificedTileEligibleWhenAdjacent")

WB_NONHERO_CASE(FWBNonHeroPaymentHand, "Wandbound.Hybrid.NonHero.Payment.HandWandEligible")
WB_NONHERO_CASE(FWBNonHeroPaymentEquipped, "Wandbound.Hybrid.NonHero.Payment.SacrificedUnitWandEligible")
WB_NONHERO_CASE(FWBNonHeroPaymentOther, "Wandbound.Hybrid.NonHero.Payment.OtherUnitEquipmentExcluded")
WB_NONHERO_CASE(FWBNonHeroPaymentOpponent, "Wandbound.Hybrid.NonHero.Payment.OpponentWandExcluded")
WB_NONHERO_CASE(FWBNonHeroPaymentOrder, "Wandbound.Hybrid.NonHero.Payment.DeterministicOrdering")
WB_NONHERO_CASE(FWBNonHeroPaymentDuplicate, "Wandbound.Hybrid.NonHero.Payment.NoDuplicatePaymentPlan")

WB_NONHERO_CASE(FWBNonHeroCapFour, "Wandbound.Hybrid.NonHero.Cap.AtFourUnitsSacrificeThenSummonLegal")
WB_NONHERO_CASE(FWBNonHeroCapCompleted, "Wandbound.Hybrid.NonHero.Cap.CompletedCountRemainsFour")
WB_NONHERO_CASE(FWBNonHeroCapAbove, "Wandbound.Hybrid.NonHero.Cap.NoCompletedStateAboveFour")

WB_NONHERO_CASE(FWBNonHeroAtomicSacrifice, "Wandbound.Hybrid.NonHero.Atomic.InvalidSacrificeNoMutation")
WB_NONHERO_CASE(FWBNonHeroAtomicPayment, "Wandbound.Hybrid.NonHero.Atomic.InvalidPaymentNoMutation")
WB_NONHERO_CASE(FWBNonHeroAtomicDestination, "Wandbound.Hybrid.NonHero.Atomic.InvalidDestinationNoMutation")
WB_NONHERO_CASE(FWBNonHeroAtomicStale, "Wandbound.Hybrid.NonHero.Atomic.StalePlanNoMutation")
WB_NONHERO_CASE(FWBNonHeroAtomicAltered, "Wandbound.Hybrid.NonHero.Atomic.AlteredPlanNoMutation")

WB_NONHERO_CASE(FWBNonHeroExecutionRemoved, "Wandbound.Hybrid.NonHero.Execution.SacrificedCharacterRemoved")
WB_NONHERO_CASE(FWBNonHeroExecutionCreated, "Wandbound.Hybrid.NonHero.Execution.HybridCreated")
WB_NONHERO_CASE(FWBNonHeroExecutionStats, "Wandbound.Hybrid.NonHero.Execution.PrintedStatsUsed")
WB_NONHERO_CASE(FWBNonHeroExecutionHero, "Wandbound.Hybrid.NonHero.Execution.HeroIdUnchanged")
WB_NONHERO_CASE(FWBNonHeroExecutionNotHero, "Wandbound.Hybrid.NonHero.Execution.HybridNotHero")
WB_NONHERO_CASE(FWBNonHeroExecutionDestination, "Wandbound.Hybrid.NonHero.Execution.DestinationAdjacentToHero")
WB_NONHERO_CASE(FWBNonHeroExecutionNoAction, "Wandbound.Hybrid.NonHero.Execution.NewUnitCannotActImmediately")

WB_NONHERO_CASE(FWBNonHeroCleanupHand, "Wandbound.Hybrid.NonHero.Cleanup.HandPaymentDiscardedOnce")
WB_NONHERO_CASE(FWBNonHeroCleanupEquipped, "Wandbound.Hybrid.NonHero.Cleanup.EquippedPaymentDiscardedOnce")
WB_NONHERO_CASE(FWBNonHeroCleanupRemaining, "Wandbound.Hybrid.NonHero.Cleanup.RemainingEquipmentDiscarded")
WB_NONHERO_CASE(FWBNonHeroCleanupOther, "Wandbound.Hybrid.NonHero.Cleanup.OtherUnitEquipmentUntouched")
WB_NONHERO_CASE(FWBNonHeroCleanupOrphan, "Wandbound.Hybrid.NonHero.Cleanup.NoOrphanAttachments")

WB_NONHERO_CASE(FWBNonHeroTerminalSacrifice, "Wandbound.Hybrid.NonHero.Terminal.SacrificeNotDefeat")
WB_NONHERO_CASE(FWBNonHeroTerminalMatch, "Wandbound.Hybrid.NonHero.Terminal.MatchRemainsNonterminal")
WB_NONHERO_CASE(FWBNonHeroTerminalWinner, "Wandbound.Hybrid.NonHero.Terminal.WinnerUnset")
WB_NONHERO_CASE(FWBNonHeroTerminalLoser, "Wandbound.Hybrid.NonHero.Terminal.LoserUnset")
WB_NONHERO_CASE(FWBNonHeroTerminalReason, "Wandbound.Hybrid.NonHero.Terminal.NoHeroLossReason")
WB_NONHERO_CASE(FWBNonHeroTerminalLater, "Wandbound.Hybrid.NonHero.Terminal.OriginalHeroLaterLossStillTerminal")

WB_NONHERO_CASE(FWBNonHeroMarkerRuns, "Wandbound.Hybrid.NonHero.Marker.PostSummonResolutionRuns")
WB_NONHERO_CASE(FWBNonHeroMarkerDamage, "Wandbound.Hybrid.NonHero.Marker.TrapCanDamageSummonedHybrid")
WB_NONHERO_CASE(FWBNonHeroMarkerDeath, "Wandbound.Hybrid.NonHero.Marker.HybridDeathDoesNotDefeatLivingHero")
WB_NONHERO_CASE(FWBNonHeroMarkerPrivacy, "Wandbound.Hybrid.NonHero.Marker.HiddenInfoPreserved")

WB_NONHERO_CASE(FWBNonHeroIdStable, "Wandbound.Hybrid.NonHero.ActionId.SamePlanStable")
WB_NONHERO_CASE(FWBNonHeroIdSacrifice, "Wandbound.Hybrid.NonHero.ActionId.SacrificeChangesId")
WB_NONHERO_CASE(FWBNonHeroIdDestination, "Wandbound.Hybrid.NonHero.ActionId.DestinationChangesId")
WB_NONHERO_CASE(FWBNonHeroIdPayment, "Wandbound.Hybrid.NonHero.ActionId.PaymentChangesId")
WB_NONHERO_CASE(FWBNonHeroIdHero, "Wandbound.Hybrid.NonHero.ActionId.HeroReplacementIdsUnchanged")

WB_NONHERO_CASE(FWBNonHeroTraceOrder, "Wandbound.Hybrid.NonHero.Trace.DeterministicOrder")
WB_NONHERO_CASE(FWBNonHeroTraceSacrifice, "Wandbound.Hybrid.NonHero.Trace.UnitSacrificePresent")
WB_NONHERO_CASE(FWBNonHeroTracePayment, "Wandbound.Hybrid.NonHero.Trace.SafePaymentPresent")
WB_NONHERO_CASE(FWBNonHeroTraceSummon, "Wandbound.Hybrid.NonHero.Trace.HybridSummonedPresent")
WB_NONHERO_CASE(FWBNonHeroTraceNoHeroSacrifice, "Wandbound.Hybrid.NonHero.Trace.NoHeroSacrificeCommitted")
WB_NONHERO_CASE(FWBNonHeroTraceNoHeroReplace, "Wandbound.Hybrid.NonHero.Trace.NoHeroReplacementCommitted")
WB_NONHERO_CASE(FWBNonHeroTraceNoHeroDefeat, "Wandbound.Hybrid.NonHero.Trace.NoHeroDefeated")
WB_NONHERO_CASE(FWBNonHeroTraceNoGameOver, "Wandbound.Hybrid.NonHero.Trace.NoGameOver")

WB_NONHERO_CASE(FWBNonHeroReplayOnce, "Wandbound.Replay.Hybrid.NonHero.AcceptedActionRecordedOnce")
WB_NONHERO_CASE(FWBNonHeroReplaySacrifice, "Wandbound.Replay.Hybrid.NonHero.FreshReplaySameSacrifice")
WB_NONHERO_CASE(FWBNonHeroReplayDestination, "Wandbound.Replay.Hybrid.NonHero.FreshReplaySameDestination")
WB_NONHERO_CASE(FWBNonHeroReplayPayment, "Wandbound.Replay.Hybrid.NonHero.FreshReplaySamePayment")
WB_NONHERO_CASE(FWBNonHeroReplayHero, "Wandbound.Replay.Hybrid.NonHero.FreshReplaySameHero")
WB_NONHERO_CASE(FWBNonHeroReplayHybrid, "Wandbound.Replay.Hybrid.NonHero.FreshReplaySameHybrid")
WB_NONHERO_CASE(FWBNonHeroReplayState, "Wandbound.Replay.Hybrid.NonHero.StateDigestMatches")
WB_NONHERO_CASE(FWBNonHeroReplayTrace, "Wandbound.Replay.Hybrid.NonHero.TraceDigestMatches")
WB_NONHERO_CASE(FWBNonHeroReplayRejected, "Wandbound.Replay.Hybrid.NonHero.RejectedActionNotRecorded")

WB_NONHERO_CASE(FWBNonHeroRegressionAtomic, "Wandbound.Hybrid.Regression.HeroReplacementStillAtomic")
WB_NONHERO_CASE(FWBNonHeroRegressionArchive, "Wandbound.Hybrid.Regression.HeroReplacementArchiveUnchanged")
WB_NONHERO_CASE(FWBNonHeroRegressionReceipt, "Wandbound.Hybrid.Regression.HeroReplacementReceiptUnchanged")
WB_NONHERO_CASE(FWBNonHeroRegressionIds, "Wandbound.Hybrid.Regression.HeroReplacementActionIdsUnchanged")
WB_NONHERO_CASE(FWBNonHeroRegressionSummon, "Wandbound.Hybrid.Regression.OrdinaryCharacterSummonUnchanged")

WB_NONHERO_CASE(FWBNonHeroPrivacyCandidates, "Wandbound.Hybrid.NonHero.Privacy.PaymentCandidatesHidden")
WB_NONHERO_CASE(FWBNonHeroPrivacyPaid, "Wandbound.Hybrid.NonHero.Privacy.PaidWandIdentityHiddenFromPublicTrace")
WB_NONHERO_CASE(FWBNonHeroPrivacyOpponent, "Wandbound.Hybrid.NonHero.Privacy.OpponentHandHidden")
WB_NONHERO_CASE(FWBNonHeroPrivacyReceipt, "Wandbound.Hybrid.NonHero.Privacy.ReceiptExactlyEightFields")
WB_NONHERO_CASE(FWBNonHeroPrivacyStartup, "Wandbound.Hybrid.NonHero.Privacy.StartupJsonUnchanged")

WB_NONHERO_CASE(FWBNonHeroAuthorityCoordinator, "Wandbound.Authority.Hybrid.NonHero.CoordinatorOwnsAction")
WB_NONHERO_CASE(FWBNonHeroAuthorityRuntime, "Wandbound.Authority.Hybrid.NonHero.NoRuntimeMutation")
WB_NONHERO_CASE(FWBNonHeroAuthorityReplay, "Wandbound.Authority.Hybrid.NonHero.NoReplayMutation")
WB_NONHERO_CASE(FWBNonHeroAuthoritySmoke, "Wandbound.Authority.Hybrid.NonHero.NoSmokeDirectStateMutation")
WB_NONHERO_CASE(FWBNonHeroAuthorityDeath, "Wandbound.Authority.Hybrid.NonHero.NoDeathResolutionForSacrifice")
WB_NONHERO_CASE(FWBNonHeroAuthorityCodec, "Wandbound.Authority.Hybrid.NonHero.NoActionCodecChange")
WB_NONHERO_CASE(FWBNonHeroAuthorityGodot, "Wandbound.Authority.Hybrid.NonHero.NoGodotChange")
WB_NONHERO_CASE(FWBNonHeroAuthorityMeshy, "Wandbound.Authority.Hybrid.NonHero.NoMeshyChange")
WB_NONHERO_CASE(FWBNonHeroAuthorityAssets, "Wandbound.Authority.Hybrid.NonHero.NoAssetOrMapChange")

#undef WB_NONHERO_CASE

#endif
