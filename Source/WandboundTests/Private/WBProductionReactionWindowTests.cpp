#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBCardZoneObservation.h"
#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionHybridNonHeroSmoke.h"
#include "WBProductionHybridReplacementSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBProductionReactionWindowSmoke.h"
#include "WBProductionRuntimeBootstrap.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardInstanceRef MakeCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerPlayerId;
	return Card;
}

FWBCardEffectDefinition MakeResponseEffect(const FString& Key)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = TEXT("canonical_react");
	Effect.PublicLabel = TEXT("Respond");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = Key;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("canonical_react"));
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const bool bHasResponse)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	if (bHasResponse)
	{
		Definition.ActivatedEffects.Add(
			MakeResponseEffect(CardId + TEXT("_react_once")));
	}
	return Definition;
}

FWBCardDefinition MakeFiller()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("reaction_filler");
	Definition.PublicName = TEXT("Reaction Filler");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBCardDefinition MakeTrap(const int32 Damage)
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("reaction_trap");
	Definition.PublicName = TEXT("Reaction Trap");
	Definition.Kind = EWBCardDefinitionKind::Trap;
	Definition.TrapDamage = Damage;
	return Definition;
}

FWBCardDefinition MakeNPC()
{
	FWBCardDefinition Definition = MakeCharacter(TEXT("reaction_npc"), false);
	Definition.Kind = EWBCardDefinitionKind::NPC;
	return Definition;
}

FWBSetupMarkerPlacement MakeMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = PlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap
		? TEXT("reaction_trap")
		: TEXT("reaction_npc");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBMatchPlayerSetup MakePlayer(const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(TEXT("reaction_p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == 0 ? TEXT("reaction_hero_a") : TEXT("reaction_hero_b");
	Setup.OrderedDeck.Add(MakeCard(Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	if (PlayerId == 0)
	{
		Setup.OrderedDeck.Add(MakeCard(TEXT("reaction_summon_instance"), TEXT("reaction_student"), 0));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("reaction_p%d_hidden_%d"), PlayerId, Index),
			TEXT("reaction_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeRequest(
	const bool bPlayer0Responds,
	const bool bPlayer1Responds,
	const bool bTrap = false,
	const int32 TrapDamage = 1)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 77731;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("reaction_window_tests");
	Request.Repository.SourceVersion = TEXT("reaction_window_v1");
	Request.Repository.Definitions = {
		MakeCharacter(TEXT("reaction_hero_a"), bPlayer0Responds),
		MakeCharacter(TEXT("reaction_hero_b"), bPlayer1Responds),
		MakeCharacter(TEXT("reaction_student"), false),
		MakeFiller(),
		MakeTrap(TrapDamage),
		MakeNPC()
	};
	Request.Players = { MakePlayer(0), MakePlayer(1) };
	Request.MarkerPlacements = {
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(2, 8), 2),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(3, 7), 3),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(2, 0), 6),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(3, 1), 7)
	};
	return Request;
}

const FWBMatchLegalAction* FindSummon(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& CardId = TEXT("reaction_student"))
{
	return Actions.FindByPredicate([&CardId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Summon
			&& (Action.SummonRequest.SourceCardId == CardId
				|| Action.HybridSummonPlan.HybridDefinitionId == CardId);
	});
}

const FWBMatchLegalAction* FindCore(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const int32 TargetUnitId = -1)
{
	return Actions.FindByPredicate([TargetUnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& (TargetUnitId == -1
				|| Action.ActivationCommand.EffectRequest.Target.TargetUnitId == TargetUnitId);
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

struct FOpenedReaction
{
	bool bOk = false;
	FString Reason;
	FWBMatchInitializationRequest Request;
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult SummonResult;
	int32 SummonedUnitId = -1;
};

FOpenedReaction OpenCharacterReaction(
	const bool bPlayer0Responds,
	const bool bPlayer1Responds,
	const bool bMoveMarkerToSummon = false,
	const int32 TrapDamage = 1)
{
	FOpenedReaction Result;
	Result.Request = MakeRequest(
		bPlayer0Responds,
		bPlayer1Responds,
		bMoveMarkerToSummon,
		TrapDamage);
	const FWBMatchOperationResult Started =
		Result.Coordinator.InitializeMatch(Result.Request);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	const FWBMatchLegalAction* Summon = FindSummon(Started.NextLegalActions);
	if (Summon == nullptr)
	{
		Result.Reason = TEXT("summon_missing");
		return Result;
	}
	if (bMoveMarkerToSummon)
	{
		FWBCardZoneState& Zones =
			Result.Coordinator.GetMutableStateForTest().GetMutableCardZoneStateForTest();
		if (Zones.MarkerPlaceholders.IsEmpty())
		{
			Result.Reason = TEXT("marker_missing");
			return Result;
		}
		Zones.MarkerPlaceholders[0].Tile = Summon->SummonRequest.TargetTile;
	}
	Result.SummonResult = Result.Coordinator.SubmitActionId(
		Summon->PlayerId,
		Summon->ActionId);
	if (!Result.SummonResult.bOk)
	{
		Result.Reason = Result.SummonResult.Reason;
		return Result;
	}
	for (const FWBUnitState& Unit : Result.Coordinator.GetState().Units)
	{
		if (Unit.CardId == TEXT("reaction_student") && Unit.OwnerId == 0)
		{
			Result.SummonedUnitId = Unit.UnitId;
		}
	}
	Result.bOk = true;
	return Result;
}

bool SubmitPass(WBMatchCoordinator& Coordinator, FWBMatchOperationResult& Out)
{
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Pass = Legal.bOk
		? FindCore(Legal.Actions, EWBActionType::PassResponse)
		: nullptr;
	if (Pass == nullptr)
	{
		return false;
	}
	Out = Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId);
	return Out.bOk;
}

bool SubmitReact(WBMatchCoordinator& Coordinator, FWBMatchOperationResult& Out)
{
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* React = Legal.bOk
		? FindActivation(Legal.Actions)
		: nullptr;
	if (React == nullptr)
	{
		return false;
	}
	Out = Coordinator.SubmitActionId(React->PlayerId, React->ActionId);
	return Out.bOk;
}

bool CloseReactionByPassing(WBMatchCoordinator& Coordinator)
{
	int32 Guard = 0;
	while (Coordinator.GetState().HasOpenReactionWindow() && ++Guard <= 2)
	{
		FWBMatchOperationResult Pass;
		if (!SubmitPass(Coordinator, Pass))
		{
			return false;
		}
	}
	return !Coordinator.GetState().HasOpenReactionWindow();
}

FString ReplayFixturePath(const TCHAR* Folder, const TCHAR* File)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay"),
		Folder,
		File);
}

FWBProductionRuntimeBootstrapRequest MakeBootstrapRequest(const TCHAR* Folder)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = ReplayFixturePath(Folder, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = ReplayFixturePath(Folder, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	return Request;
}

bool AddResponseToDefinition(
	FWBMatchInitializationRequest& Request,
	const FString& CardId)
{
	for (FWBCardDefinition& Definition : Request.Repository.Definitions)
	{
		if (Definition.CardId == CardId)
		{
			Definition.ActivatedEffects.Add(
				MakeResponseEffect(CardId + TEXT("_react_once")));
			return true;
		}
	}
	return false;
}

struct FHybridReactionScenario
{
	bool bOk = false;
	FString Reason;
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult HybridResult;
	FString HybridActionId;
	int32 OriginalHeroUnitId = -1;
	int32 SacrificedUnitId = -1;
	int32 HybridUnitId = -1;
};

FHybridReactionScenario BuildHybridHeroReaction(const bool bTerminalTrap = false)
{
	FHybridReactionScenario Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(
			MakeBootstrapRequest(TEXT("HybridReplacementFixture")));
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}
	FWBMatchInitializationRequest Request = Bootstrap.InitializationRequest;
	if (!AddResponseToDefinition(Request, TEXT("hybrid_fixture_hero_beta")))
	{
		Result.Reason = TEXT("opponent_hero_definition_missing");
		return Result;
	}
	if (bTerminalTrap)
	{
		for (FWBCardDefinition& Definition : Request.Repository.Definitions)
		{
			if (Definition.Kind == EWBCardDefinitionKind::Trap)
			{
				Definition.TrapDamage = 999;
			}
		}
	}
	const FWBMatchOperationResult Started = Result.Coordinator.InitializeMatch(Request);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	const FWBPlayerStateData* Player = Result.Coordinator.GetState().GetPlayerById(0);
	Result.OriginalHeroUnitId = Player != nullptr ? Player->HeroUnitId : -1;
	if (bTerminalTrap)
	{
		FWBCardZoneState& Zones = Result.Coordinator.GetMutableStateForTest().GetMutableCardZoneStateForTest();
		if (Zones.MarkerPlaceholders.IsEmpty())
		{
			Result.Reason = TEXT("terminal_marker_missing");
			return Result;
		}
		const FWBUnitState* Hero = Result.Coordinator.GetState().GetUnitById(Result.OriginalHeroUnitId);
		Zones.MarkerPlaceholders[0].Tile = Hero != nullptr
			? FWBTile(Hero->X, Hero->Y)
			: FWBTile(-1, -1);
	}
	const FWBMatchLegalAction* Equip = Started.NextLegalActions.FindByPredicate(
		[&Result](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Equip
				&& Action.EquipRequest.TargetUnitId == Result.OriginalHeroUnitId;
		});
	if (Equip == nullptr
		|| !Result.Coordinator.SubmitActionId(Equip->PlayerId, Equip->ActionId).bOk)
	{
		Result.Reason = TEXT("hybrid_equip_failed");
		return Result;
	}
	const FWBMatchLegalActionGenerationResult Legal = Result.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Hybrid = Legal.Actions.FindByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Summon
				&& Action.bHybridHeroReplacement;
		});
	if (Hybrid == nullptr)
	{
		Result.Reason = TEXT("hybrid_replacement_missing");
		return Result;
	}
	Result.HybridActionId = Hybrid->ActionId;
	Result.HybridResult = Result.Coordinator.SubmitActionId(
		Hybrid->PlayerId,
		Hybrid->ActionId);
	if (!Result.HybridResult.bOk)
	{
		Result.Reason = Result.HybridResult.Reason;
		return Result;
	}
	Player = Result.Coordinator.GetState().GetPlayerById(0);
	Result.HybridUnitId = Player != nullptr ? Player->HeroUnitId : -1;
	Result.bOk = true;
	return Result;
}

FHybridReactionScenario BuildHybridNonHeroReaction()
{
	FHybridReactionScenario Result;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(
			MakeBootstrapRequest(TEXT("HybridNonHeroFixture")));
	if (!Bootstrap.bOk)
	{
		Result.Reason = Bootstrap.Reason;
		return Result;
	}
	FWBMatchInitializationRequest Request = Bootstrap.InitializationRequest;
	if (!AddResponseToDefinition(Request, TEXT("hybrid_nonhero_hero_beta")))
	{
		Result.Reason = TEXT("opponent_hero_definition_missing");
		return Result;
	}
	const FWBMatchOperationResult Started = Result.Coordinator.InitializeMatch(Request);
	if (!Started.bOk)
	{
		Result.Reason = Started.Reason;
		return Result;
	}
	const FWBPlayerStateData* Player = Result.Coordinator.GetState().GetPlayerById(0);
	Result.OriginalHeroUnitId = Player != nullptr ? Player->HeroUnitId : -1;

	const FString SacrificeCards[] = {
		TEXT("hybrid_nonhero_sacrifice_alpha"),
		TEXT("hybrid_nonhero_sacrifice_beta")
	};
	for (const FString& CardId : SacrificeCards)
	{
		const FWBMatchLegalActionGenerationResult Legal = Result.Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Summon = FindSummon(Legal.Actions, CardId);
		if (Summon == nullptr)
		{
			Result.Reason = TEXT("nonhero_sacrifice_summon_missing");
			return Result;
		}
		const FWBMatchOperationResult Summoned = Result.Coordinator.SubmitActionId(
			Summon->PlayerId,
			Summon->ActionId);
		if (!Summoned.bOk || !CloseReactionByPassing(Result.Coordinator))
		{
			Result.Reason = Summoned.bOk
				? TEXT("nonhero_prerequisite_response_failed")
				: Summoned.Reason;
			return Result;
		}
	}
	const FWBUnitState* Sacrifice = Result.Coordinator.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("hybrid_nonhero_sacrifice_alpha")
				&& Unit.IsUnitOnBoard();
		});
	Result.SacrificedUnitId = Sacrifice != nullptr ? Sacrifice->UnitId : -1;
	const FWBMatchLegalActionGenerationResult EquipLegal = Result.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Equip = EquipLegal.Actions.FindByPredicate(
		[&Result](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Equip
				&& Action.EquipRequest.TargetUnitId == Result.SacrificedUnitId;
		});
	if (Equip == nullptr
		|| !Result.Coordinator.SubmitActionId(Equip->PlayerId, Equip->ActionId).bOk)
	{
		Result.Reason = TEXT("nonhero_equip_failed");
		return Result;
	}
	const FWBMatchLegalActionGenerationResult HybridLegal = Result.Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Hybrid = HybridLegal.Actions.FindByPredicate(
		[&Result](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Summon
				&& Action.bHybridSummon
				&& !Action.bHybridHeroReplacement
				&& Action.HybridSummonPlan.SacrificedUnitId == Result.SacrificedUnitId;
		});
	if (Hybrid == nullptr)
	{
		Result.Reason = TEXT("nonhero_hybrid_missing");
		return Result;
	}
	Result.HybridActionId = Hybrid->ActionId;
	Result.HybridResult = Result.Coordinator.SubmitActionId(
		Hybrid->PlayerId,
		Hybrid->ActionId);
	if (!Result.HybridResult.bOk)
	{
		Result.Reason = Result.HybridResult.Reason;
		return Result;
	}
	const FWBUnitState* HybridUnit = Result.Coordinator.GetState().Units.FindByPredicate(
		[](const FWBUnitState& Unit)
		{
			return Unit.CardId == TEXT("hybrid_nonhero_summon")
				&& Unit.IsUnitOnBoard();
		});
	Result.HybridUnitId = HybridUnit != nullptr ? HybridUnit->UnitId : -1;
	Result.bOk = true;
	return Result;
}

bool HasOnlyResponseFamilies(const TArray<FWBMatchLegalAction>& Actions)
{
	return !Actions.IsEmpty() && !Actions.ContainsByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family != EWBMatchActionFamily::Activation
				&& !(Action.Family == EWBMatchActionFamily::CoreAction
					&& Action.CoreAction.Type == EWBActionType::PassResponse);
		});
}

struct FReplayReaction
{
	bool bOk = false;
	FString Reason;
	FString StateDigest;
	FString TraceDigest;
	FString FreshStateDigest;
	FString FreshTraceDigest;
	TArray<FWBMatchCommittedActionRecord> Records;
	TArray<FWBTraceEvent> FinalActionTrace;
};

FReplayReaction BuildReplayReaction()
{
	FReplayReaction Result;
	const FWBMatchInitializationRequest Request = MakeRequest(false, true);
	WBMatchCoordinator Original;
	const FWBMatchOperationResult Started = Original.InitializeMatch(Request);
	const FWBMatchLegalAction* Summon = Started.bOk
		? FindSummon(Started.NextLegalActions)
		: nullptr;
	if (Summon == nullptr)
	{
		Result.Reason = Started.bOk ? TEXT("summon_missing") : Started.Reason;
		return Result;
	}
	if (!Original.SubmitActionId(Summon->PlayerId, Summon->ActionId).bOk)
	{
		Result.Reason = TEXT("summon_failed");
		return Result;
	}
	FWBMatchOperationResult ReactResult;
	if (!SubmitReact(Original, ReactResult))
	{
		Result.Reason = ReactResult.Reason.IsEmpty() ? TEXT("react_failed") : ReactResult.Reason;
		return Result;
	}
	Result.Records = Original.GetCommittedActionRecords();
	Result.FinalActionTrace = ReactResult.TraceEvents;
	Result.StateDigest = Original.GetCurrentStateDigest();
	Result.TraceDigest = Original.GetCurrentTraceDigest();

	WBMatchCoordinator Fresh;
	const FWBMatchOperationResult FreshStarted = Fresh.InitializeMatch(Request);
	if (!FreshStarted.bOk)
	{
		Result.Reason = FreshStarted.Reason;
		return Result;
	}
	for (const FWBMatchCommittedActionRecord& Record : Result.Records)
	{
		const FWBMatchOperationResult Applied = Fresh.SubmitActionId(
			Record.ActingPlayer,
			Record.ChosenActionId);
		if (!Applied.bOk)
		{
			Result.Reason = Applied.Reason;
			return Result;
		}
	}
	Result.FreshStateDigest = Fresh.GetCurrentStateDigest();
	Result.FreshTraceDigest = Fresh.GetCurrentTraceDigest();
	Result.bOk = true;
	return Result;
}

bool LoadFile(const FString& RelativePath, FString& Out)
{
	return FFileHelper::LoadFileToString(
		Out,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionStateDefaultClosed, "Wandbound.Reaction.State.DefaultClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionStateDefaultClosed::RunTest(const FString& Parameters)
{
	const FWBGameStateData State;
	TestFalse(TEXT("Default reaction state is closed"), State.HasOpenReactionWindow());
	TestEqual(TEXT("Default kind is None"), static_cast<int32>(State.ReactionWindow.Kind), static_cast<int32>(EWBReactionWindowKind::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPackagedFixtureLoads, "Wandbound.Reaction.PackagedFixture.Loads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPackagedFixtureLoads::RunTest(const FString& Parameters)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(
			ReplayFixturePath(TEXT("ReactionWindowFixture"), TEXT("root_manifest.json")));
	TestTrue(TEXT("Reaction fixture CardDB loads"), Loaded.bOk);
	if (Loaded.Snapshot.IsValid())
	{
		AddInfo(FString::Printf(
			TEXT("REACTION_FIXTURE_DIGEST=%s"),
			*Loaded.Snapshot->ContentDigest));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPackagedSmokeDirect, "Wandbound.Reaction.PackagedSmoke.Direct", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPackagedSmokeDirect::RunTest(const FString& Parameters)
{
	const FWBProductionReactionWindowSmokeResult Result =
		WBProductionReactionWindowSmoke::Run(
			MakeBootstrapRequest(TEXT("ReactionWindowFixture")));
	TestTrue(
		*FString::Printf(TEXT("Production reaction smoke succeeds: %s"), *Result.Reason),
		Result.bOk);
	TestTrue(TEXT("Reaction action ID is stable and present"),
		!Result.ReactionActionId.IsEmpty());
	TestEqual(TEXT("Equip, Hybrid, React, and continuation replay"),
		Result.RecordsVerified, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionStateTypedKind, "Wandbound.Reaction.State.TypedWindowKind", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionStateTypedKind::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.ReactionWindow.Kind = EWBReactionWindowKind::PostSummon;
	TestTrue(TEXT("Typed state opens"), State.HasOpenReactionWindow());
	TestTrue(TEXT("Kinds remain distinct"), EWBReactionWindowKind::PostSummon != EWBReactionWindowKind::PostMove);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionStateSingleAuthority, "Wandbound.Reaction.State.SingleAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionStateSingleAuthority::RunTest(const FString& Parameters)
{
	FString RuntimeSource;
	TestTrue(TEXT("Runtime host source loads"), LoadFile(TEXT("Source/WandboundRuntime/Private/WBRuntimeMatchHostComponent.cpp"), RuntimeSource));
	TestFalse(TEXT("Runtime host does not mutate reaction state"), RuntimeSource.Contains(TEXT("ReactionWindow.")));
	TestFalse(TEXT("Runtime host does not clear reaction state"), RuntimeSource.Contains(TEXT("ClearReactionWindow")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionStateTerminalClears, "Wandbound.Reaction.State.TerminalClearsWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionStateTerminalClears::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	TestTrue(TEXT("Scenario opens"), Scenario.bOk && Scenario.Coordinator.GetState().HasOpenReactionWindow());
	if (!Scenario.bOk) return false;
	FWBGameStateData& State = Scenario.Coordinator.GetMutableStateForTest();
	State.bGameOver = true;
	State.WinnerPlayerId = 0;
	State.ClearReactionWindow();
	TestFalse(TEXT("Terminal state has no reaction"), State.HasOpenReactionWindow());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionCharacterOpens, "Wandbound.Reaction.PostSummon.CharacterOpensCanonicalWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionCharacterOpens::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	TestTrue(TEXT("Character summon succeeds"), Scenario.bOk);
	TestTrue(TEXT("Post-summon opens"), Scenario.Coordinator.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Post-summon kind"), static_cast<int32>(Scenario.Coordinator.GetState().ReactionWindow.Kind), static_cast<int32>(EWBReactionWindowKind::PostSummon));
	TestEqual(TEXT("Opponent receives first priority"), Scenario.Coordinator.GetState().PriorityPlayer, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionMarkerOrder, "Wandbound.Reaction.PostSummon.MarkerResolvesInCanonicalOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionMarkerOrder::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true, true, 1);
	TestTrue(TEXT("Marker scenario succeeds"), Scenario.bOk);
	const int32 Damage = TraceIndex(Scenario.SummonResult.TraceEvents, TEXT("trap_damage_resolved"));
	const int32 Open = TraceIndex(Scenario.SummonResult.TraceEvents, TEXT("reaction_window_opened"));
	TestTrue(TEXT("Trap resolves before window"), Damage >= 0 && Open > Damage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionHybridHeroAtomic, "Wandbound.Reaction.PostSummon.HybridHeroReplacementAtomicBeforeWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionHybridHeroAtomic::RunTest(const FString& Parameters)
{
	const FHybridReactionScenario Scenario = BuildHybridHeroReaction();
	TestTrue(*FString::Printf(TEXT("Hybrid Hero scenario succeeds: %s"), *Scenario.Reason), Scenario.bOk);
	if (!Scenario.bOk)
	{
		return false;
	}
	const FWBPlayerStateData* Player = Scenario.Coordinator.GetState().GetPlayerById(0);
	TestNotNull(TEXT("Acting player remains present"), Player);
	TestTrue(TEXT("Hybrid replaces the old Hero before response"),
		Player != nullptr
		&& Player->HeroUnitId == Scenario.HybridUnitId
		&& Scenario.HybridUnitId != Scenario.OriginalHeroUnitId);
	const FWBUnitState* OldHero =
		Scenario.Coordinator.GetState().GetUnitById(Scenario.OriginalHeroUnitId);
	TestTrue(TEXT("Old Hero is off-board before response"),
		OldHero == nullptr || !OldHero->IsUnitOnBoard());
	TestEqual(TEXT("Post-summon window is authoritative"),
		static_cast<int32>(Scenario.Coordinator.GetState().ReactionWindow.Kind),
		static_cast<int32>(EWBReactionWindowKind::PostSummon));
	const int32 ReplacementIndex = TraceIndex(Scenario.HybridResult.TraceEvents, TEXT("hero_replacement_committed"));
	const int32 OpenIndex = TraceIndex(Scenario.HybridResult.TraceEvents, TEXT("reaction_window_opened"));
	TestTrue(TEXT("Replacement commits before the response checkpoint"),
		ReplacementIndex != INDEX_NONE && OpenIndex > ReplacementIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionHybridNonHeroPreservesHero, "Wandbound.Reaction.PostSummon.HybridNonHeroPreservesHero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionHybridNonHeroPreservesHero::RunTest(const FString& Parameters)
{
	const FHybridReactionScenario Scenario = BuildHybridNonHeroReaction();
	TestTrue(*FString::Printf(TEXT("Non-Hero Hybrid scenario succeeds: %s"), *Scenario.Reason), Scenario.bOk);
	if (!Scenario.bOk)
	{
		return false;
	}
	const FWBPlayerStateData* Player = Scenario.Coordinator.GetState().GetPlayerById(0);
	TestTrue(TEXT("Original Hero identity is preserved"),
		Player != nullptr && Player->HeroUnitId == Scenario.OriginalHeroUnitId);
	TestTrue(TEXT("Non-Hero Hybrid exists independently"),
		Scenario.HybridUnitId >= 0 && Scenario.HybridUnitId != Scenario.OriginalHeroUnitId);
	const FWBUnitState* Sacrificed =
		Scenario.Coordinator.GetState().GetUnitById(Scenario.SacrificedUnitId);
	TestTrue(TEXT("Sacrificed unit is off-board before response"),
		Sacrificed == nullptr || !Sacrificed->IsUnitOnBoard());
	TestEqual(TEXT("Post-summon window is authoritative"),
		static_cast<int32>(Scenario.Coordinator.GetState().ReactionWindow.Kind),
		static_cast<int32>(EWBReactionWindowKind::PostSummon));
	const int32 SummonedIndex = TraceIndex(Scenario.HybridResult.TraceEvents, TEXT("hybrid_summoned"));
	const int32 OpenIndex = TraceIndex(Scenario.HybridResult.TraceEvents, TEXT("reaction_window_opened"));
	TestTrue(TEXT("Hybrid summon commits before the response checkpoint"),
		SummonedIndex != INDEX_NONE && OpenIndex > SummonedIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionTerminalMarkerSuppressesWindow, "Wandbound.Reaction.PostSummon.TerminalMarkerDoesNotOpenWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionTerminalMarkerSuppressesWindow::RunTest(const FString& Parameters)
{
	const FHybridReactionScenario Scenario = BuildHybridHeroReaction(true);
	TestTrue(*FString::Printf(TEXT("Terminal marker scenario succeeds: %s"), *Scenario.Reason), Scenario.bOk);
	if (!Scenario.bOk)
	{
		return false;
	}
	TestTrue(TEXT("Marker commits a terminal match"), Scenario.Coordinator.GetState().bGameOver);
	TestEqual(TEXT("Coordinator enters GameOver"),
		static_cast<int32>(Scenario.Coordinator.GetMatchPhase()),
		static_cast<int32>(EWBMatchLoopPhase::GameOver));
	TestFalse(TEXT("Terminal state cannot retain a reaction window"),
		Scenario.Coordinator.GetState().HasOpenReactionWindow());
	TestTrue(TEXT("Terminal marker resolution is traced"),
		HasTrace(Scenario.HybridResult.TraceEvents, TEXT("marker_triggered_game_over")));
	TestFalse(TEXT("No post-summon window opens after terminal commit"),
		HasTrace(Scenario.HybridResult.TraceEvents, TEXT("reaction_window_opened")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPriorityInitial, "Wandbound.Reaction.Priority.InitialPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPriorityInitial::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	TestEqual(TEXT("Opponent first"), Scenario.Coordinator.GetState().PriorityPlayer, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPriorityAlternates, "Wandbound.Reaction.Priority.Alternation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPriorityAlternates::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(true, true);
	FWBMatchOperationResult Pass;
	TestTrue(TEXT("First pass succeeds"), Scenario.bOk && SubmitPass(Scenario.Coordinator, Pass));
	TestEqual(TEXT("Priority alternates"), Scenario.Coordinator.GetState().PriorityPlayer, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPassProgression, "Wandbound.Reaction.Priority.PassProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPassProgression::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(true, true);
	FWBMatchOperationResult Pass;
	TestTrue(TEXT("First pass succeeds"), Scenario.bOk && SubmitPass(Scenario.Coordinator, Pass));
	TestEqual(TEXT("One pass recorded"), Scenario.Coordinator.GetState().ReactionWindow.ConsecutivePassCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionWindowClosure, "Wandbound.Reaction.Priority.WindowClosure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionWindowClosure::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(true, true);
	FWBMatchOperationResult Pass;
	TestTrue(TEXT("First pass succeeds"), Scenario.bOk && SubmitPass(Scenario.Coordinator, Pass));
	TestTrue(TEXT("Second pass succeeds"), SubmitPass(Scenario.Coordinator, Pass));
	TestFalse(TEXT("Two passes close"), Scenario.Coordinator.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Returns to action"), static_cast<int32>(Scenario.Coordinator.GetMatchPhase()), static_cast<int32>(EWBMatchLoopPhase::Action));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAutoPass, "Wandbound.Reaction.Priority.AutoPassWhenNoLegalReactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAutoPass::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(true, false);
	TestTrue(TEXT("Window opens for originating player's React"), Scenario.bOk && Scenario.Coordinator.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Opponent auto-passes to originator"), Scenario.Coordinator.GetState().PriorityPlayer, 0);
	TestEqual(TEXT("Auto-pass increments count"), Scenario.Coordinator.GetState().ReactionWindow.ConsecutivePassCount, 1);
	TestTrue(TEXT("Auto-pass traced"), HasTrace(Scenario.SummonResult.TraceEvents, TEXT("reaction_auto_passed")));
	return true;
}

#define WB_REACTION_ACTION_ABSENCE_TEST(ClassName, TestName, TypeValue) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) \
	{ \
		const FOpenedReaction Scenario = OpenCharacterReaction(false, true); \
		const FWBMatchLegalActionGenerationResult Legal = Scenario.Coordinator.EnumerateLegalActions(); \
		TestTrue(TEXT("Response actions enumerate"), Scenario.bOk && Legal.bOk); \
		TestNull(TEXT("Action family is absent"), FindCore(Legal.Actions, TypeValue)); \
		return true; \
	}

WB_REACTION_ACTION_ABSENCE_TEST(FWBReactionNoMove, "Wandbound.Reaction.Actions.NoMove", EWBActionType::Move)
WB_REACTION_ACTION_ABSENCE_TEST(FWBReactionNoAttack, "Wandbound.Reaction.Actions.NoAttack", EWBActionType::Attack)
WB_REACTION_ACTION_ABSENCE_TEST(FWBReactionNoEndTurn, "Wandbound.Reaction.Actions.NoEndTurn", EWBActionType::EndTurn)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionNoSummon, "Wandbound.Reaction.Actions.NoSummon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionNoSummon::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const auto Legal = Scenario.Coordinator.EnumerateLegalActions();
	TestFalse(TEXT("No summon"), Legal.Actions.ContainsByPredicate([](const auto& A){ return A.Family == EWBMatchActionFamily::Summon; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionNoEquip, "Wandbound.Reaction.Actions.NoEquip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionNoEquip::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const auto Legal = Scenario.Coordinator.EnumerateLegalActions();
	TestFalse(TEXT("No equip"), Legal.Actions.ContainsByPredicate([](const auto& A){ return A.Family == EWBMatchActionFamily::Equip; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPassPresent, "Wandbound.Reaction.Actions.PassResponsePresent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPassPresent::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const auto Legal = Scenario.Coordinator.EnumerateLegalActions();
	TestNotNull(TEXT("PassResponse present"), FindCore(Legal.Actions, EWBActionType::PassResponse));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionOnlyCanonical, "Wandbound.Reaction.Actions.OnlyCanonicalReactsExposed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionOnlyCanonical::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const auto Legal = Scenario.Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("Only React and pass families"), HasOnlyResponseFamilies(Legal.Actions));
	TestNotNull(TEXT("Canonical React present"), FindActivation(Legal.Actions));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionReactResetsPasses, "Wandbound.Reaction.Priority.ReactResetsPassCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionReactResetsPasses::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(true, true);
	FWBMatchOperationResult Result;
	TestTrue(TEXT("First pass succeeds"), SubmitPass(Scenario.Coordinator, Result));
	TestTrue(TEXT("React succeeds"), SubmitReact(Scenario.Coordinator, Result));
	TestEqual(TEXT("React resets pass count"), Scenario.Coordinator.GetState().ReactionWindow.ConsecutivePassCount, 0);
	TestEqual(TEXT("React transfers priority"), Scenario.Coordinator.GetState().PriorityPlayer, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAcceptedRecorded, "Wandbound.Replay.Reaction.AcceptedResponseRecorded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAcceptedRecorded::RunTest(const FString& Parameters)
{
	const FReplayReaction Replay = BuildReplayReaction();
	TestTrue(TEXT("Replay scenario succeeds"), Replay.bOk);
	TestEqual(TEXT("Summon and React recorded"), Replay.Records.Num(), 2);
	TestEqual(TEXT("React family recorded"), Replay.Records.Last().ActionFamily, FString(TEXT("activate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPassRecorded, "Wandbound.Replay.Reaction.PassRecorded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPassRecorded::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(true, true);
	FWBMatchOperationResult Result;
	TestTrue(TEXT("Pass succeeds"), SubmitPass(Scenario.Coordinator, Result));
	TestEqual(TEXT("Pass family recorded"), Scenario.Coordinator.GetCommittedActionRecords().Last().ActionFamily, FString(TEXT("pass_react")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAutoDeterministic, "Wandbound.Replay.Reaction.AutoResolutionDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAutoDeterministic::RunTest(const FString& Parameters)
{
	const FReplayReaction A = BuildReplayReaction();
	const FReplayReaction B = BuildReplayReaction();
	TestEqual(TEXT("State deterministic"), A.StateDigest, B.StateDigest);
	TestEqual(TEXT("Trace deterministic"), A.TraceDigest, B.TraceDigest);
	TestTrue(TEXT("Automatic passes traced"), HasTrace(A.FinalActionTrace, TEXT("reaction_auto_passed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionFreshReplay, "Wandbound.Replay.Reaction.FreshReplaySameWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionFreshReplay::RunTest(const FString& Parameters)
{
	const FReplayReaction Replay = BuildReplayReaction();
	TestTrue(TEXT("Fresh replay succeeds"), Replay.bOk);
	TestEqual(TEXT("Fresh state matches"), Replay.StateDigest, Replay.FreshStateDigest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionStateDigest, "Wandbound.Replay.Reaction.StateDigestMatches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionStateDigest::RunTest(const FString& Parameters)
{
	const FReplayReaction Replay = BuildReplayReaction();
	TestEqual(TEXT("State digest matches"), Replay.StateDigest, Replay.FreshStateDigest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionTraceDigest, "Wandbound.Replay.Reaction.TraceDigestMatches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionTraceDigest::RunTest(const FString& Parameters)
{
	const FReplayReaction Replay = BuildReplayReaction();
	TestEqual(TEXT("Trace digest matches"), Replay.TraceDigest, Replay.FreshTraceDigest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionRejectedNotRecorded, "Wandbound.Replay.Reaction.RejectedResponseNotRecorded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionRejectedNotRecorded::RunTest(const FString& Parameters)
{
	FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const int32 Before = Scenario.Coordinator.GetCommittedActionRecords().Num();
	const FWBMatchOperationResult Rejected = Scenario.Coordinator.SubmitActionId(1, TEXT("move:p1:u1:x0:y0"));
	TestFalse(TEXT("Illegal response rejected"), Rejected.bOk);
	TestEqual(TEXT("Rejected response not recorded"), Scenario.Coordinator.GetCommittedActionRecords().Num(), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPrivacyOpponentHand, "Wandbound.Reaction.Privacy.OpponentHandHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPrivacyOpponentHand::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const FWBMatchObservation Observation = Scenario.Coordinator.BuildObservation(0);
	TestTrue(TEXT("Opponent hidden identity absent"), !WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation.CardZones, TEXT("reaction_p1_hidden_")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPrivacyCandidates, "Wandbound.Reaction.Privacy.HiddenCandidatesNotExposed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPrivacyCandidates::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	TestEqual(TEXT("Non-priority viewer sees no legal candidates"), Scenario.Coordinator.BuildObservation(0).LegalActions.Num(), 0);
	TestTrue(TEXT("Priority viewer sees legal response"), HasOnlyResponseFamilies(Scenario.Coordinator.BuildObservation(1).LegalActions));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPrivacyReceipt, "Wandbound.Reaction.Privacy.ReceiptExactlyEightFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPrivacyReceipt::RunTest(const FString& Parameters)
{
	FWBProductionMatchReplayArchive Archive;
	Archive.Header.SchemaVersion = 1;
	WBProductionMatchReplay::RebuildIntegrity(Archive);
	const FString Json = WBProductionMatchReplay::SerializeReceipt(WBProductionMatchReplay::BuildReceipt(Archive, true));
	TSharedPtr<FJsonObject> Object;
	TestTrue(TEXT("Receipt parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid());
	TestEqual(TEXT("Receipt has exactly eight fields"), Object.IsValid() ? Object->Values.Num() : 0, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionPrivacyNoDigests, "Wandbound.Reaction.Privacy.NoProtectedDigestLeak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionPrivacyNoDigests::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	const FWBMatchObservation Observation = Scenario.Coordinator.BuildObservation(1);
	FString Source;
	TestTrue(TEXT("Observation source loads"), LoadFile(TEXT("Source/WandboundCore/Public/WBMatchCoordinator.h"), Source));
	TestFalse(TEXT("Public observation has no state digest"), Source.Contains(FString(TEXT("FWBMatchObservation\n{")) + TEXT("\n\tFString StateDigest")));
	TestTrue(TEXT("Observation remains usable"), !Observation.LegalActions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAuthorityCoordinator, "Wandbound.Authority.Reaction.CoordinatorOwnsWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAuthorityCoordinator::RunTest(const FString& Parameters)
{
	const FOpenedReaction Scenario = OpenCharacterReaction(false, true);
	TestTrue(TEXT("Coordinator state owns open window"), Scenario.Coordinator.GetState().HasOpenReactionWindow());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAuthorityRuntime, "Wandbound.Authority.Reaction.RuntimeCannotMutateWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAuthorityRuntime::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Runtime host loads"), LoadFile(TEXT("Source/WandboundRuntime/Private/WBRuntimeMatchHostComponent.cpp"), Source));
	TestFalse(TEXT("Runtime cannot clear window"), Source.Contains(TEXT("ClearReactionWindow")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAuthorityUI, "Wandbound.Authority.Reaction.UISelectsOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAuthorityUI::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("HUD source loads"), LoadFile(TEXT("Source/WandboundRuntime/Private/WBRuntimeMatchHUDWidget.cpp"), Source));
	TestFalse(TEXT("UI does not own reaction state"), Source.Contains(TEXT("ReactionWindow")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAuthorityCodec, "Wandbound.Authority.Reaction.NoActionCodecChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAuthorityCodec::RunTest(const FString& Parameters)
{
	FWBAction Pass;
	Pass.Type = EWBActionType::PassResponse;
	Pass.PlayerId = 1;
	TestEqual(TEXT("PassResponse ID remains stable"), WBActionCodec::MakeActionId(Pass), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionAuthoritySchema, "Wandbound.Authority.Reaction.NoReplaySchemaChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionAuthoritySchema::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Replay schema remains one"), WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

#define WB_REACTION_SOURCE_GUARD_TEST(ClassName, TestName, RelativePath, Forbidden) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) \
	{ \
		FString Diff; \
		TestTrue(TEXT("Audit JSON loads"), LoadFile(TEXT("Docs/Production_Reaction_Window_Audit.json"), Diff)); \
		TestFalse(TEXT("Protected path is not a required change"), Diff.Contains(FString(TEXT(RelativePath)) + TEXT("\\\",\\\"required_change\\\":\\\"") + TEXT(Forbidden))); \
		return true; \
	}

WB_REACTION_SOURCE_GUARD_TEST(FWBReactionAuthorityGodot, "Wandbound.Authority.Reaction.NoGodotChange", "Reference/GodotProject", "modify")
WB_REACTION_SOURCE_GUARD_TEST(FWBReactionAuthorityMeshy, "Wandbound.Authority.Reaction.NoMeshyChange", "Plugins/meshy", "modify")
WB_REACTION_SOURCE_GUARD_TEST(FWBReactionAuthorityAssets, "Wandbound.Authority.Reaction.NoAssetChange", ".uasset", "modify")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionMoveOpens, "Wandbound.Reaction.PostMove.OpensCanonicalWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionMoveOpens::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest(false, true));
	const FWBMatchLegalAction* Move = FindCore(Started.NextLegalActions, EWBActionType::Move);
	TestNotNull(TEXT("Move exists"), Move);
	if (Move == nullptr) return false;
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(Move->PlayerId, Move->ActionId);
	TestTrue(TEXT("Move succeeds"), Result.bOk);
	TestEqual(TEXT("Post-move typed kind"), static_cast<int32>(Coordinator.GetState().ReactionWindow.Kind), static_cast<int32>(EWBReactionWindowKind::PostMove));
	TestEqual(TEXT("Opponent first"), Coordinator.GetState().PriorityPlayer, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionInitialSetupSuppressed, "Wandbound.Reaction.InitialHeroSetup.ManualReactsSuppressed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionInitialSetupSuppressed::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest(true, true));
	TestTrue(TEXT("Initial setup succeeds"), Started.bOk);
	TestFalse(TEXT("Initial setup leaves no response window"), Coordinator.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Action phase begins"), static_cast<int32>(Coordinator.GetMatchPhase()), static_cast<int32>(EWBMatchLoopPhase::Action));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionEndTurnAuthority, "Wandbound.Reaction.Regression.EndTurnStillCoordinatorOwned", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionEndTurnAuthority::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const auto Started = Coordinator.InitializeMatch(MakeRequest(false, false));
	const FWBMatchLegalAction* End = FindCore(Started.NextLegalActions, EWBActionType::EndTurn);
	TestNotNull(TEXT("End turn exists"), End);
	if (End == nullptr) return false;
	TestTrue(TEXT("Coordinator executes end turn"), Coordinator.SubmitActionId(End->PlayerId, End->ActionId).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionHybridHeroArchive, "Wandbound.Reaction.Regression.HybridHeroArchiveUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionHybridHeroArchive::RunTest(const FString& Parameters)
{
	const FWBProductionRuntimeBootstrapRequest Request =
		MakeBootstrapRequest(TEXT("HybridReplacementFixture"));
	const FWBProductionHybridReplacementSmokeResult First =
		WBProductionHybridReplacementSmoke::Run(Request);
	FString FirstArchive;
	const FString ArchivePath =
		WBProductionMatchReplayPersistence::GetArchivePath(
			TEXT("hybrid_replacement_smoke_match"));
	const FWBProductionMatchReplayPersistenceResult FirstLoad =
		WBProductionMatchReplayPersistence::Load(ArchivePath, FirstArchive);
	const FWBProductionHybridReplacementSmokeResult Second =
		WBProductionHybridReplacementSmoke::Run(Request);
	FString SecondArchive;
	const FWBProductionMatchReplayPersistenceResult SecondLoad =
		WBProductionMatchReplayPersistence::Load(ArchivePath, SecondArchive);
	TestTrue(TEXT("Baseline Hero Hybrid smoke remains valid"), First.bOk && Second.bOk);
	TestTrue(TEXT("Both Hero Hybrid archives load"), FirstLoad.bOk && SecondLoad.bOk);
	TestEqual(TEXT("Hero Hybrid archive remains byte deterministic"), FirstArchive, SecondArchive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionHybridNonHeroArchive, "Wandbound.Reaction.Regression.HybridNonHeroArchiveUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionHybridNonHeroArchive::RunTest(const FString& Parameters)
{
	const FWBProductionRuntimeBootstrapRequest Request =
		MakeBootstrapRequest(TEXT("HybridNonHeroFixture"));
	const FWBProductionHybridNonHeroSmokeResult First =
		WBProductionHybridNonHeroSmoke::Run(Request);
	FString FirstArchive;
	const FString ArchivePath =
		WBProductionMatchReplayPersistence::GetArchivePath(
			TEXT("hybrid_non_hero_smoke_match"));
	const FWBProductionMatchReplayPersistenceResult FirstLoad =
		WBProductionMatchReplayPersistence::Load(ArchivePath, FirstArchive);
	const FWBProductionHybridNonHeroSmokeResult Second =
		WBProductionHybridNonHeroSmoke::Run(Request);
	FString SecondArchive;
	const FWBProductionMatchReplayPersistenceResult SecondLoad =
		WBProductionMatchReplayPersistence::Load(ArchivePath, SecondArchive);
	TestTrue(TEXT("Baseline non-Hero Hybrid smoke remains valid"), First.bOk && Second.bOk);
	TestTrue(TEXT("Both non-Hero Hybrid archives load"), FirstLoad.bOk && SecondLoad.bOk);
	TestEqual(TEXT("Non-Hero Hybrid archive remains byte deterministic"), FirstArchive, SecondArchive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionHybridActionIds, "Wandbound.Reaction.Regression.HybridActionIdsUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionHybridActionIds::RunTest(const FString& Parameters)
{
	const FHybridReactionScenario Hero = BuildHybridHeroReaction();
	const FHybridReactionScenario NonHero = BuildHybridNonHeroReaction();
	TestTrue(TEXT("Hybrid scenarios build"), Hero.bOk && NonHero.bOk);
	TestTrue(TEXT("Hero Hybrid retains established stable ID prefix"),
		Hero.HybridActionId.StartsWith(TEXT("hybrid_summon:p0:i"))
		&& Hero.HybridActionId.Contains(TEXT(":w")));
	TestTrue(TEXT("Non-Hero Hybrid retains established stable ID prefix"),
		NonHero.HybridActionId.StartsWith(TEXT("hybrid_summon:p0:i"))
		&& NonHero.HybridActionId.Contains(TEXT(":w")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionHybridHeroIdentity, "Wandbound.Reaction.Regression.HybridHeroIdentityUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionHybridHeroIdentity::RunTest(const FString& Parameters)
{
	const FHybridReactionScenario Hero = BuildHybridHeroReaction();
	const FHybridReactionScenario NonHero = BuildHybridNonHeroReaction();
	const FWBPlayerStateData* HeroPlayer = Hero.Coordinator.GetState().GetPlayerById(0);
	const FWBPlayerStateData* NonHeroPlayer = NonHero.Coordinator.GetState().GetPlayerById(0);
	TestTrue(TEXT("Hero Hybrid replaces the authoritative Hero identity"),
		Hero.bOk && HeroPlayer != nullptr
		&& HeroPlayer->HeroUnitId == Hero.HybridUnitId
		&& Hero.HybridUnitId != Hero.OriginalHeroUnitId);
	TestTrue(TEXT("Non-Hero Hybrid preserves the authoritative Hero identity"),
		NonHero.bOk && NonHeroPlayer != nullptr
		&& NonHeroPlayer->HeroUnitId == NonHero.OriginalHeroUnitId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionTurnStartUnchanged, "Wandbound.Reaction.Regression.TurnStartSequenceUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionTurnStartUnchanged::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const auto Started = Coordinator.InitializeMatch(MakeRequest(false, false));
	TestTrue(TEXT("Turn start completed"), Started.bOk && Coordinator.WasTurnStartCompleted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBReactionNPCUnchanged, "Wandbound.Reaction.Regression.NPCPhaseUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBReactionNPCUnchanged::RunTest(const FString& Parameters)
{
	FString CoordinatorSource;
	TestTrue(TEXT("Coordinator source loads"), LoadFile(TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp"), CoordinatorSource));
	TestTrue(TEXT("NPC phase begins through coordinator turn transition"), CoordinatorSource.Contains(TEXT("WBNPCPhaseResolution::BeginPhase")));
	TestTrue(TEXT("NPC phase resumes through coordinator authority"), CoordinatorSource.Contains(TEXT("ResumeNPCPhaseAndTurnTransition")));
	return true;
}

#undef WB_REACTION_ACTION_ABSENCE_TEST
#undef WB_REACTION_SOURCE_GUARD_TEST

#endif
