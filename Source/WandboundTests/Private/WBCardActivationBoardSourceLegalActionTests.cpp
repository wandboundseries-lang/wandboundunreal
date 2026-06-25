#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCostPaymentVerifier.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardActivationLegalActionReplayVerifier.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

constexpr int32 BoardLegalSourceUnitId = 1;
constexpr int32 BoardLegalTargetUnitId = 2;

FWBPlayerStateData MakeBoardLegalPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeBoardLegalUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 5;
	Unit.RLUsed = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeBoardLegalState(
	const FString& SourceCardId = TEXT("fixture_board_legal_card"),
	const int32 TargetHP = 5,
	const int32 TargetMaxHP = 6)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 13;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeBoardLegalPlayer(0));
	State.Players.Add(MakeBoardLegalPlayer(1));
	State.AddUnitForTest(MakeBoardLegalUnit(BoardLegalSourceUnitId, 0, SourceCardId, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeBoardLegalUnit(BoardLegalTargetUnitId, 1, TEXT("fixture_target"), FWBTile(5, 4), TargetHP, TargetMaxHP));
	State.AddUnitForTest(MakeBoardLegalUnit(3, 1, TEXT("fixture_target_b"), FWBTile(6, 4), 4, 6));
	State.GetMutableUnitById(BoardLegalTargetUnitId)->SetArmorForTest(1, 3);
	return State;
}

FWBCardActivationSourceGateDefinition MakeBoardLegalGate()
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresFixtureZoneOwnership = true;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.bBlockedByStunned = true;
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBGenericEffectPayload MakeBoardLegalDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeBoardLegalHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeBoardLegalArmorPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = EWBArmorEffectOp::AddCurrentArmor;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeBoardLegalStatusPayload(const FName StatusId, const int32 Duration)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardEffectDefinition MakeBoardLegalEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBGenericEffectPayload& Payload,
	FWBCardActivationSourceGateDefinition Gate)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = Gate;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardEffectDefinition MakeBoardLegalEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBGenericEffectPayload& Payload)
{
	return MakeBoardLegalEffect(EffectId, PublicLabel, Payload, MakeBoardLegalGate());
}

FWBCardDefinition MakeBoardLegalDefinition(
	const FString& CardId,
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBGenericEffectPayload& Payload,
	FWBCardActivationSourceGateDefinition Gate)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicLabel;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeBoardLegalEffect(EffectId, PublicLabel, Payload, Gate));
	return Definition;
}

FWBCardDefinition MakeBoardLegalDefinition(
	const FString& CardId,
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBGenericEffectPayload& Payload)
{
	return MakeBoardLegalDefinition(CardId, EffectId, PublicLabel, Payload, MakeBoardLegalGate());
}

FWBEffectTargetRef MakeBoardLegalTarget(const int32 TargetUnitId = BoardLegalTargetUnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = TargetUnitId;
	return Target;
}

TArray<FWBEffectTargetRef> MakeBoardLegalDefaultTargets()
{
	TArray<FWBEffectTargetRef> Targets;
	Targets.Add(MakeBoardLegalTarget());
	return Targets;
}

FWBCardActivationSourceGateContext MakeBoardLegalContext(
	const FString& SourceCardId,
	const int32 SourceUnitId = BoardLegalSourceUnitId)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.SourceCardId = SourceCardId;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationCandidateSource MakeBoardLegalSource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = BoardLegalSourceUnitId,
	const bool bIncludeSourceCardId = true)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	Source.SourceGateContext = MakeBoardLegalContext(bIncludeSourceCardId ? Definition.CardId : FString(), SourceUnitId);
	return Source;
}

FWBCardActivationCandidateGenerationResult GenerateBoardLegalCandidates(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const bool bIncludeSourceCardId = true)
{
	return WBCardActivationCandidateGenerator::GenerateCandidates(
		State,
		{ MakeBoardLegalSource(Definition, Targets, BoardLegalSourceUnitId, bIncludeSourceCardId) });
}

FWBCardActivationCandidateGenerationResult GenerateBoardLegalCandidates(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const bool bIncludeSourceCardId = true)
{
	return GenerateBoardLegalCandidates(State, Definition, MakeBoardLegalDefaultTargets(), bIncludeSourceCardId);
}

FWBCardActivationLegalActionGenerationResult GenerateBoardLegalActions(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const bool bIncludeSourceCardId = true)
{
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		GenerateBoardLegalCandidates(State, Definition, Targets, bIncludeSourceCardId);
	if (!CandidateResult.bOk)
	{
		FWBCardActivationLegalActionGenerationResult Result;
		Result.Reason = CandidateResult.Reason;
		return Result;
	}

	return WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
}

FWBCardActivationLegalActionGenerationResult GenerateBoardLegalActions(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const bool bIncludeSourceCardId = true)
{
	return GenerateBoardLegalActions(State, Definition, MakeBoardLegalDefaultTargets(), bIncludeSourceCardId);
}

TArray<FString> BoardLegalActionIds(const FWBCardActivationLegalActionGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationLegalAction& Action : Result.ActionSet.Actions)
	{
		Ids.Add(Action.ActivationActionId);
	}
	return Ids;
}

bool ContainsAnyInternalLabelSubstring(const FString& Label)
{
	const TArray<FString> Forbidden = {
		TEXT("damage_effect"),
		TEXT("heal_effect"),
		TEXT("armor_effect"),
		TEXT("status_effect"),
		TEXT("EffectRunner"),
		TEXT("Rules"),
		TEXT("schema"),
		TEXT("hook")
	};

	for (const FString& Token : Forbidden)
	{
		if (Label.Contains(Token, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

FWBAction MakeBoardLegalCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = BoardLegalSourceUnitId;
	Action.TargetUnitId = BoardLegalTargetUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FString BoardLegalFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadBoardLegalFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *BoardLegalFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutFixture) && OutFixture.IsValid();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceLegalActionFamiliesTest, "Wandbound.Core.CardActivationBoardSourceLegalAction.EffectFamilies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceLegalActionFamiliesTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		FString CardId;
		FString EffectId;
		FString Label;
		FWBGenericEffectPayload Payload;
	};

	const TArray<FCase> Cases = {
		{ TEXT("fixture_board_damage_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), MakeBoardLegalDamagePayload(2) },
		{ TEXT("fixture_board_status_card"), TEXT("burn_2"), TEXT("Apply Burn"), MakeBoardLegalStatusPayload(FName(TEXT("Burn")), 2) },
		{ TEXT("fixture_board_armor_card"), TEXT("armor_2"), TEXT("Restore Armor"), MakeBoardLegalArmorPayload(2) },
		{ TEXT("fixture_board_heal_card"), TEXT("heal_2"), TEXT("Heal 2 HP"), MakeBoardLegalHealPayload(2) }
	};

	for (const FCase& TestCase : Cases)
	{
		FWBGameStateData State = MakeBoardLegalState(TestCase.CardId);
		const FWBCardActivationLegalActionGenerationResult Result =
			GenerateBoardLegalActions(
				State,
				MakeBoardLegalDefinition(TestCase.CardId, TestCase.EffectId, TestCase.Label, TestCase.Payload));
		TestTrue(*FString::Printf(TEXT("%s legal generation ok"), *TestCase.CardId), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s action count"), *TestCase.CardId), Result.ActionSet.Actions.Num(), 1);
		TestEqual(*FString::Printf(TEXT("%s action id"), *TestCase.CardId), Result.ActionSet.Actions[0].ActivationActionId,
			FString::Printf(TEXT("activate:p0:u1:c%s:e%s:t2:x-1:y-1:wa-1,-1:wb-1,-1"), *TestCase.CardId, *TestCase.EffectId));
		TestEqual(*FString::Printf(TEXT("%s label"), *TestCase.CardId), Result.ActionSet.Actions[0].PublicLabel, TestCase.Label);
		TestFalse(*FString::Printf(TEXT("%s label clean"), *TestCase.CardId), ContainsAnyInternalLabelSubstring(Result.ActionSet.Actions[0].PublicLabel));
		TestEqual(*FString::Printf(TEXT("%s source card preserved"), *TestCase.CardId), Result.ActionSet.Actions[0].Candidate.SourceCardId, TestCase.CardId);
		TestEqual(*FString::Printf(TEXT("%s effect preserved"), *TestCase.CardId), Result.ActionSet.Actions[0].Candidate.SourceEffectId, TestCase.EffectId);
		TestEqual(*FString::Printf(TEXT("%s command source card preserved"), *TestCase.CardId), Result.ActionSet.Actions[0].Command.Source.SourceCardId, TestCase.CardId);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceLegalActionOrderingTest, "Wandbound.Core.CardActivationBoardSourceLegalAction.OrderingAndIdStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceLegalActionOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardLegalState(TEXT("fixture_board_order_a"));
	FWBCardDefinition FirstDefinition;
	FirstDefinition.CardId = TEXT("fixture_board_order_a");
	FirstDefinition.PublicName = TEXT("Fixture Board Order A");
	FirstDefinition.Kind = EWBCardDefinitionKind::Fixture;
	FirstDefinition.ActivatedEffects.Add(MakeBoardLegalEffect(TEXT("deal_1"), TEXT("Deal 1 damage"), MakeBoardLegalDamagePayload(1)));
	FirstDefinition.ActivatedEffects.Add(MakeBoardLegalEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1)));

	FWBCardDefinition SecondDefinition;
	SecondDefinition.CardId = TEXT("fixture_board_order_b");
	SecondDefinition.PublicName = TEXT("Fixture Board Order B");
	SecondDefinition.Kind = EWBCardDefinitionKind::Fixture;
	SecondDefinition.ActivatedEffects.Add(MakeBoardLegalEffect(TEXT("burn_1"), TEXT("Apply Burn"), MakeBoardLegalStatusPayload(FName(TEXT("Burn")), 1)));

	State.AddUnitForTest(MakeBoardLegalUnit(4, 0, TEXT("fixture_board_order_b"), FWBTile(4, 5), 5, 5));

	TArray<FWBCardActivationCandidateSource> Sources;
	Sources.Add(MakeBoardLegalSource(FirstDefinition, { MakeBoardLegalTarget(2), MakeBoardLegalTarget(3) }, 1));
	Sources.Last().SourceGateContext.SourceCardId = TEXT("fixture_board_order_a");
	Sources.Add(MakeBoardLegalSource(SecondDefinition, { MakeBoardLegalTarget(2) }, 4));
	Sources.Last().SourceGateContext.SourceCardId = TEXT("fixture_board_order_b");

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	const FWBCardActivationLegalActionGenerationResult FirstResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	const FWBCardActivationLegalActionGenerationResult SecondResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);

	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestTrue(TEXT("Legal generation ok"), FirstResult.bOk);
	TestTrue(TEXT("Legal generation stable ok"), SecondResult.bOk);
	TestTrue(TEXT("IDs stable"), BoardLegalActionIds(FirstResult) == BoardLegalActionIds(SecondResult));
	TestNotEqual(TEXT("Different targets have different ids"), FirstResult.ActionSet.Actions[0].ActivationActionId, FirstResult.ActionSet.Actions[1].ActivationActionId);
	TestNotEqual(TEXT("Different effects have different ids"), FirstResult.ActionSet.Actions[0].ActivationActionId, FirstResult.ActionSet.Actions[2].ActivationActionId);
	TestNotEqual(TEXT("Different sources have different ids"), FirstResult.ActionSet.Actions[0].ActivationActionId, FirstResult.ActionSet.Actions[4].ActivationActionId);

	const TArray<FString> ExpectedIds = {
		TEXT("activate:p0:u1:cfixture_board_order_a:edeal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_board_order_a:edeal_1:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_board_order_a:eheal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_board_order_a:eheal_1:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u4:cfixture_board_order_b:eburn_1:t2:x-1:y-1:wa-1,-1:wb-1,-1")
	};
	TestEqual(TEXT("Ordered id count"), FirstResult.ActionSet.Actions.Num(), ExpectedIds.Num());
	for (int32 Index = 0; Index < ExpectedIds.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Ordered id %d"), Index), FirstResult.ActionSet.Actions[Index].ActivationActionId, ExpectedIds[Index]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceLegalActionCommitTest, "Wandbound.Core.CardActivationBoardSourceLegalAction.CostAndUsageCommitPreservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceLegalActionCommitTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_commit_card");
	FWBCardActivationSourceGateDefinition CostGate = MakeBoardLegalGate();
	CostGate.CostGate.bRequiresExternalAffordability = true;
	CostGate.CostGate.RequiredRR = 2;
	CostGate.CostGate.CostKind = FName(TEXT("RR"));

	FWBCardActivationCandidateSource CostSource = MakeBoardLegalSource(
		MakeBoardLegalDefinition(CardId, TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1), CostGate),
		{ MakeBoardLegalTarget() });
	CostSource.SourceGateContext.CostContext.bHasExternalAffordability = true;
	CostSource.SourceGateContext.CostContext.bExternallyAffordable = true;
	CostSource.SourceGateContext.CostContext.SuppliedRequiredRR = 2;
	CostSource.SourceGateContext.CostContext.SuppliedAvailableRL = 4;
	CostSource.SourceGateContext.CostContext.CostKind = FName(TEXT("RR"));

	const FWBCardActivationCandidateGenerationResult CostCandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardLegalState(CardId), { CostSource });
	const FWBCardActivationLegalActionGenerationResult CostActionResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CostCandidateResult.Candidates);
	TestTrue(TEXT("Cost candidate generation ok"), CostCandidateResult.bOk);
	TestTrue(TEXT("Cost legal action generation ok"), CostActionResult.bOk);
	TestTrue(TEXT("Candidate cost commit present"), CostCandidateResult.Candidates[0].Command.CostPaymentCommit.bPayCostOnSuccess);
	TestTrue(TEXT("Legal action cost commit present"), CostActionResult.ActionSet.Actions[0].Command.CostPaymentCommit.bPayCostOnSuccess);
	TestEqual(TEXT("Cost amount preserved"), CostActionResult.ActionSet.Actions[0].Command.CostPaymentCommit.RequiredRR, 2);
	TestEqual(TEXT("Cost source unit preserved"), CostActionResult.ActionSet.Actions[0].Command.CostPaymentCommit.SourceUnitId, BoardLegalSourceUnitId);

	FWBCardActivationSourceGateDefinition UsageGate = MakeBoardLegalGate();
	UsageGate.bOncePerTurn = true;
	FWBCardActivationCandidateSource UsageSource = MakeBoardLegalSource(
		MakeBoardLegalDefinition(CardId, TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1), UsageGate),
		{ MakeBoardLegalTarget() });
	UsageSource.SourceGateContext.ActivationUsageKey = TEXT("fixture_board_usage_key");

	const FWBCardActivationCandidateGenerationResult UsageCandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardLegalState(CardId), { UsageSource });
	const FWBCardActivationLegalActionGenerationResult UsageActionResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(UsageCandidateResult.Candidates);
	TestTrue(TEXT("Usage candidate generation ok"), UsageCandidateResult.bOk);
	TestTrue(TEXT("Usage legal action generation ok"), UsageActionResult.bOk);
	TestTrue(TEXT("Candidate usage commit present"), UsageCandidateResult.Candidates[0].Command.UsageCommit.bMarkUsageOnSuccess);
	TestTrue(TEXT("Legal action usage commit present"), UsageActionResult.ActionSet.Actions[0].Command.UsageCommit.bMarkUsageOnSuccess);
	TestEqual(TEXT("Usage key preserved"), UsageActionResult.ActionSet.Actions[0].Command.UsageCommit.UsageKey, FString(TEXT("fixture_board_usage_key")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceLegalActionReplayTest, "Wandbound.Core.CardActivationBoardSourceLegalAction.SelectedIdReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceLegalActionReplayTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_replay_card");
	FWBGameStateData State = MakeBoardLegalState(CardId, 5, 6);
	const FWBCardActivationLegalActionGenerationResult ActionResult =
		GenerateBoardLegalActions(
			State,
			MakeBoardLegalDefinition(CardId, TEXT("deal_2"), TEXT("Deal 2 damage"), MakeBoardLegalDamagePayload(2)));
	TestTrue(TEXT("Replay legal generation ok"), ActionResult.bOk);
	const FString SelectedId = ActionResult.ActionSet.Actions[0].ActivationActionId;

	const FWBCardActivationLegalActionReplayResult ReplayResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(State, ActionResult.ActionSet, SelectedId);
	TestTrue(TEXT("Replay selection ok"), ReplayResult.Selection.bOk);
	TestTrue(TEXT("Replay apply ok"), ReplayResult.ActivationResult.bOk);
	TestEqual(TEXT("Damage applied"), State.GetUnitById(BoardLegalTargetUnitId)->HP, 3);
	TestEqual(TEXT("Trace count"), ReplayResult.ActivationResult.TraceEvents.Num(), 3);
	TestEqual(TEXT("Parent trace"), ReplayResult.ActivationResult.TraceEvents[0].Kind, FName(TEXT("card_activation_resolved")));
	TestEqual(TEXT("Effect request trace"), ReplayResult.ActivationResult.TraceEvents[1].Kind, FName(TEXT("effect_request_resolved")));
	TestEqual(TEXT("Damage trace"), ReplayResult.ActivationResult.TraceEvents[2].Kind, FName(TEXT("damage_effect_resolved")));

	FWBCardActivationSourceGateDefinition CostGate = MakeBoardLegalGate();
	CostGate.CostGate.bRequiresExternalAffordability = true;
	CostGate.CostGate.RequiredRR = 2;
	CostGate.CostGate.CostKind = FName(TEXT("RR"));
	FWBCardActivationCandidateSource CostSource = MakeBoardLegalSource(
		MakeBoardLegalDefinition(CardId, TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1), CostGate),
		{ MakeBoardLegalTarget() });
	CostSource.SourceGateContext.CostContext.bHasExternalAffordability = true;
	CostSource.SourceGateContext.CostContext.bExternallyAffordable = true;
	CostSource.SourceGateContext.CostContext.SuppliedRequiredRR = 2;
	CostSource.SourceGateContext.CostContext.SuppliedAvailableRL = 4;
	CostSource.SourceGateContext.CostContext.CostKind = FName(TEXT("RR"));
	FWBGameStateData CostState = MakeBoardLegalState(CardId, 4, 6);
	const FWBCardActivationCandidateGenerationResult CostCandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(CostState, { CostSource });
	const FWBCardActivationLegalActionGenerationResult CostActionResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CostCandidateResult.Candidates);
	const FWBCardActivationLegalActionReplayResult CostReplayResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(CostState, CostActionResult.ActionSet, CostActionResult.ActionSet.Actions[0].ActivationActionId);
	TestTrue(TEXT("Cost replay succeeds"), CostReplayResult.ActivationResult.bOk);
	TestEqual(TEXT("Cost replay RL paid"), CostState.GetUnitById(BoardLegalSourceUnitId)->RLUsed, 3);
	FString CostReason;
	TestTrue(
		TEXT("Cost trace matches"),
		FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace(
			CostReplayResult.ActivationResult.TraceEvents,
			0,
			1,
			2,
			1,
			3,
			4,
			2,
			CostReason));

	FWBCardActivationSourceGateDefinition UsageGate = MakeBoardLegalGate();
	UsageGate.bOncePerTurn = true;
	FWBCardActivationCandidateSource UsageSource = MakeBoardLegalSource(
		MakeBoardLegalDefinition(CardId, TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1), UsageGate),
		{ MakeBoardLegalTarget() });
	UsageSource.SourceGateContext.ActivationUsageKey = TEXT("fixture_board_replay_usage_key");
	FWBGameStateData UsageState = MakeBoardLegalState(CardId, 4, 6);
	FWBCardActivationCandidateGenerationResult UsageCandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(UsageState, { UsageSource });
	FWBCardActivationLegalActionGenerationResult UsageActionResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(UsageCandidateResult.Candidates);
	const FWBCardActivationLegalActionReplayResult UsageReplayResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(UsageState, UsageActionResult.ActionSet, UsageActionResult.ActionSet.Actions[0].ActivationActionId);
	TestTrue(TEXT("Usage replay succeeds"), UsageReplayResult.ActivationResult.bOk);
	TestTrue(TEXT("Usage marked"), UsageState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_board_replay_usage_key")));
	UsageCandidateResult = WBCardActivationCandidateGenerator::GenerateCandidates(UsageState, { UsageSource });
	TestTrue(TEXT("Used source regeneration ok"), UsageCandidateResult.bOk);
	TestEqual(TEXT("Used source excluded afterward"), UsageCandidateResult.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceLegalActionBoundariesTest, "Wandbound.Core.CardActivationBoardSourceLegalAction.BoundariesAndHiddenMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceLegalActionBoundariesTest::RunTest(const FString& Parameters)
{
	const FString VisibleCardId = TEXT("visible_board_legal_card");
	FWBGameStateData State = MakeBoardLegalState(VisibleCardId, 5, 6);
	State.Players[0].Hand.Add(TEXT("secret_board_legal_hand"));
	State.Players[0].Deck.Add(TEXT("secret_board_legal_deck"));
	State.Players[0].Discard.Add(TEXT("secret_board_legal_discard"));

	FWBCardActivationCandidateSource Source = MakeBoardLegalSource(
		MakeBoardLegalDefinition(VisibleCardId, TEXT("secret_board_legal_effect"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1)),
		{ MakeBoardLegalTarget() });
	Source.SourceGateContext.FixtureZoneContext.Entries.Add({ TEXT("secret_board_legal_fixture_zone"), 0, EWBCardActivationSourceZone::Hand, -1 });
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { Source });
	const FWBCardActivationLegalActionGenerationResult ActionResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	TestTrue(TEXT("Hidden metadata candidate generation ok"), CandidateResult.bOk);
	TestTrue(TEXT("Hidden metadata legal generation ok"), ActionResult.bOk);

	FWBCardActivationCommand Command = ActionResult.ActionSet.Actions[0].Command;
	Command.DebugActivationId = TEXT("secret_board_legal_debug");
	FWBCardActivationLegalAction Action = ActionResult.ActionSet.Actions[0];
	Action.Command = Command;
	FWBCardActivationLegalActionSet ActionSet;
	ActionSet.Actions.Add(Action);
	const FWBCardActivationLegalActionReplayResult ReplayResult =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(State, ActionSet, Action.ActivationActionId);
	TestTrue(TEXT("Hidden metadata replay succeeds"), ReplayResult.ActivationResult.bOk);

	const FString TraceJson = WBReplayTrace::SerializeEvents(ReplayResult.ActivationResult.TraceEvents);
	const TArray<FString> ForbiddenTraceSubstrings = {
		TEXT("secret_board_legal_effect"),
		TEXT("secret_board_legal_debug"),
		TEXT("secret_board_legal_fixture_zone"),
		TEXT("secret_board_legal_hand"),
		TEXT("secret_board_legal_deck"),
		TEXT("secret_board_legal_discard"),
		TEXT("usage_key"),
		TEXT("debug_activation_id")
	};
	for (const FString& Forbidden : ForbiddenTraceSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Forbidden), TraceJson.Contains(Forbidden));
	}

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Visible source card id remains public"), Summary.Units[0].CardId, VisibleCardId);
	TestFalse(TEXT("Public summary excludes fixture zone secret"), Summary.Units[0].CardId.Contains(TEXT("secret_board_legal_fixture_zone")));

	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MakeBoardLegalState(VisibleCardId), 0));
	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MakeBoardLegalState(VisibleCardId), 0));
	TestTrue(TEXT("FWBAction legal generation unchanged"), BaselineIds == AfterIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction id is not activate %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id unchanged"), WBActionCodec::MakeActionId(MakeBoardLegalCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id unchanged"), WBActionCodec::MakeActionId(MakeBoardLegalCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id unchanged"), WBActionCodec::MakeActionId(MakeBoardLegalCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id unchanged"), WBActionCodec::MakeActionId(MakeBoardLegalCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id unchanged"), WBActionCodec::MakeActionId(MakeBoardLegalCodecAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	FString ActionCodecSource;
	TestTrue(
		TEXT("Action codec source loads"),
		FFileHelper::LoadFileToString(
			ActionCodecSource,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec has no activate ids"), ActionCodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));

	FWBGameStateData MismatchState = MakeBoardLegalState(TEXT("visible_board_legal_card"));
	const FWBCardActivationLegalActionGenerationResult MismatchActionResult =
		GenerateBoardLegalActions(
			MismatchState,
			MakeBoardLegalDefinition(TEXT("wrong_board_legal_card"), TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardLegalHealPayload(1)));
	TestTrue(TEXT("Mismatch legal action generation ok"), MismatchActionResult.bOk);
	TestEqual(TEXT("Mismatch creates no legal actions"), MismatchActionResult.ActionSet.Actions.Num(), 0);
	const FWBCardActivationLegalActionReplayResult MissingReplay =
		FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
			MismatchState,
			MismatchActionResult.ActionSet,
			TEXT("activate:p0:u1:cwrong_board_legal_card:eheal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"));
	TestFalse(TEXT("Mismatch selected id not found"), MissingReplay.Selection.bOk);
	TestEqual(TEXT("Mismatch selected id reason"), MissingReplay.Selection.Reason, FString(TEXT("activation_action_id_not_found")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceLegalActionFixtureScenariosTest, "Wandbound.Core.CardActivationBoardSourceLegalAction.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceLegalActionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_board_source_legal_action_damage.json"),
		TEXT("card_activation_board_source_legal_action_status.json"),
		TEXT("card_activation_board_source_legal_action_armor.json"),
		TEXT("card_activation_board_source_legal_action_heal.json"),
		TEXT("card_activation_board_source_legal_action_ordered.json"),
		TEXT("card_activation_board_source_legal_action_preserves_cost_commit.json"),
		TEXT("card_activation_board_source_legal_action_preserves_usage_commit.json"),
		TEXT("card_activation_board_source_replay_selected_id_success.json"),
		TEXT("card_activation_board_source_replay_card_mismatch_no_action.json"),
		TEXT("card_activation_board_source_legal_action_separate_from_fwbaction.json"),
		TEXT("card_activation_board_source_action_id_stability.json"),
		TEXT("card_activation_board_source_hidden_metadata_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadBoardLegalFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestTrue(
			*FString::Printf(TEXT("%s operation is activation legal-action fixture"), *FixtureName),
			OperationKind == EWBFixtureOperationKind::GenerateCardActivationLegalActions
				|| OperationKind == EWBFixtureOperationKind::ApplyCardActivationLegalActionById);

		TSharedPtr<FJsonObject> Expected;
		const TSharedPtr<FJsonObject>* ExpectedPtr = nullptr;
		TestTrue(
			*FString::Printf(TEXT("%s has expected"), *FixtureName),
			Fixture->TryGetObjectField(TEXT("expected"), ExpectedPtr) && ExpectedPtr != nullptr && ExpectedPtr->IsValid());
		Expected = ExpectedPtr != nullptr ? *ExpectedPtr : nullptr;
		if (!Expected.IsValid())
		{
			return false;
		}

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), Expected->TryGetBoolField(TEXT("ok"), bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), OperationResult.bOk, bExpectedOk);

		if (Expected->HasField(TEXT("expected_trace_order")))
		{
			FString TraceReason;
			TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *TraceReason), ExpectTraceOrder(Fixture, OperationResult.TraceEvents, TraceReason));
		}

		if (Expected->HasField(TEXT("final_units")))
		{
			FString UnitReason;
			TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *UnitReason), ExpectUnitStatusSummary(Fixture, State, UnitReason));
		}
	}

	return true;
}

#endif
