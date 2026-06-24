#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeCostGatePlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeCostGateUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 RLTotal = 5,
	const int32 RLUsed = 2)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(1, 3);
	return Unit;
}

FWBGameStateData MakeCostGateState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeCostGatePlayer(0, 2));
	State.Players.Add(MakeCostGatePlayer(1, 0));
	State.AddUnitForTest(MakeCostGateUnit(1, 0, FWBTile(4, 4), 5, 5, 5, 2));
	State.AddUnitForTest(MakeCostGateUnit(2, 1, FWBTile(5, 4), 3, 5, 4, 1));
	State.AddUnitForTest(MakeCostGateUnit(3, 1, FWBTile(6, 4), 3, 5, 4, 1));
	State.AddUnitForTest(MakeCostGateUnit(4, 0, FWBTile(4, 5), 5, 5, 5, 2));
	return State;
}

FWBCardActivationSourceGateDefinition MakeCostGateDefinition(
	const int32 RequiredRR,
	const bool bRequiresSimpleCosts = false)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.bRequiresCostsSatisfiedExternally = bRequiresSimpleCosts;
	Gate.CostGate.bRequiresExternalAffordability = true;
	Gate.CostGate.RequiredRR = RequiredRR;
	Gate.CostGate.CostKind = FName(TEXT("RR"));
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardActivationSourceGateContext MakeCostGateContext(
	const bool bHasExternalAffordability = true,
	const bool bExternallyAffordable = true,
	const int32 SuppliedRequiredRR = 2,
	const int32 SuppliedAvailableRL = 3,
	const bool bSimpleCostsSatisfied = true,
	const int32 SourceUnitId = 1)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.bCostsSatisfiedExternally = bSimpleCostsSatisfied;
	Context.CostContext.bHasExternalAffordability = bHasExternalAffordability;
	Context.CostContext.bExternallyAffordable = bExternallyAffordable;
	Context.CostContext.SuppliedRequiredRR = SuppliedRequiredRR;
	Context.CostContext.SuppliedAvailableRL = SuppliedAvailableRL;
	Context.CostContext.CostKind = FName(TEXT("RR"));
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBGenericEffectPayload MakeCostGateHealPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardEffectDefinition MakeCostGateEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBCardActivationSourceGateDefinition& SourceGate)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = SourceGate;
	Effect.Payloads.Add(MakeCostGateHealPayload());
	return Effect;
}

FWBCardDefinition MakeCostGateCard(
	const FString& CardId,
	const TArray<FWBCardEffectDefinition>& Effects)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects = Effects;
	return Definition;
}

FWBEffectTargetRef MakeCostGateUnitTarget(const int32 UnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardActivationCandidateSource MakeCostGateSource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const FWBCardActivationSourceGateContext& Context = MakeCostGateContext(),
	const int32 SourceUnitId = 1)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.SourceGateContext = Context;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	return Source;
}

TArray<FString> CostGateCandidateIds(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Ids.Add(Candidate.ActivationCandidateId);
	}
	return Ids;
}

TArray<FString> CostGateCandidateLabels(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Labels;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Labels.Add(Candidate.PublicLabel);
	}
	return Labels;
}

bool CompareStringArrays(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FString>& Actual,
	const TArray<FString>& Expected)
{
	Test.TestEqual(*FString::Printf(TEXT("%s count"), *Label), Actual.Num(), Expected.Num());
	const int32 NumToCompare = FMath::Min(Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s %d"), *Label, Index), Actual[Index], Expected[Index]);
	}

	return Actual == Expected;
}

FWBAction MakeCostGateAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	Action.TargetUnitId = 2;
	return Action;
}

FString CostGateFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadCostGateFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CostGateFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, OutFixture);
	return OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		&& Child->IsValid())
	{
		return *Child;
	}

	return nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Values))
	{
		return Values;
	}

	return nullptr;
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
}

bool ReadStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FString>& OutStrings)
{
	OutStrings.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = GetArrayField(Object, FieldName);
	if (Values == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			return false;
		}

		OutStrings.Add(Value->AsString());
	}

	return true;
}

bool ExpectCandidateFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationCandidateGenerationResult& Result)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	bool bExpectedOk = false;
	Test.TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), Expected->TryGetBoolField(TEXT("ok"), bExpectedOk));
	Test.TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), Result.bOk, bExpectedOk);

	int32 ExpectedCandidateCount = -1;
	if (TryReadIntegerField(Expected, TEXT("candidate_count"), ExpectedCandidateCount))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s candidate count"), *FixtureName), Result.Candidates.Num(), ExpectedCandidateCount);
	}

	TArray<FString> ExpectedIds;
	if (ReadStringArrayField(Expected, TEXT("candidate_ids"), ExpectedIds))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" candidate ids"), CostGateCandidateIds(Result), ExpectedIds);
	}

	TArray<FString> ExpectedLabels;
	if (ReadStringArrayField(Expected, TEXT("public_labels"), ExpectedLabels))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" public labels"), CostGateCandidateLabels(Result), ExpectedLabels);
	}

	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostGateEvaluationTest, "Wandbound.Core.CardActivationCostGate.EvaluationReasons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostGateEvaluationTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeCostGateState();

	FWBCardActivationSourceGateResult Result =
		WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(2), MakeCostGateContext(true, true, 2, 3));
	TestTrue(TEXT("Affordable cost gate passes"), Result.bOk);
	TestEqual(TEXT("Affordable reason"), Result.Reason, FString());

	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(2), MakeCostGateContext(false, false, 0, 0));
	TestFalse(TEXT("Missing affordability fails"), Result.bOk);
	TestEqual(TEXT("Missing affordability reason"), Result.Reason, FString(TEXT("cost_affordability_missing")));

	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(2), MakeCostGateContext(true, false, 2, 3));
	TestFalse(TEXT("Not affordable fails"), Result.bOk);
	TestEqual(TEXT("Not affordable reason"), Result.Reason, FString(TEXT("cost_not_affordable")));

	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(2), MakeCostGateContext(true, true, 3, 3));
	TestFalse(TEXT("Requirement mismatch fails"), Result.bOk);
	TestEqual(TEXT("Requirement mismatch reason"), Result.Reason, FString(TEXT("cost_requirement_mismatch")));

	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(3), MakeCostGateContext(true, true, 3, 2));
	TestFalse(TEXT("Insufficient available RL fails"), Result.bOk);
	TestEqual(TEXT("Insufficient available RL reason"), Result.Reason, FString(TEXT("cost_not_affordable")));

	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(0), MakeCostGateContext(true, true, 0, 0));
	TestTrue(TEXT("Zero RR cost gate passes"), Result.bOk);

	FWBCardActivationSourceGateDefinition InvalidGate = MakeCostGateDefinition(-1);
	Result = WBCardActivationSourceGate::Evaluate(State, InvalidGate, MakeCostGateContext(true, true, 0, 0));
	TestFalse(TEXT("Negative required RR fails"), Result.bOk);
	TestEqual(TEXT("Negative required RR reason"), Result.Reason, FString(TEXT("invalid_cost_requirement")));

	FWBCardActivationSourceGateContext InvalidContext = MakeCostGateContext(true, true, -1, 0);
	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(0), InvalidContext);
	TestFalse(TEXT("Negative supplied required RR fails"), Result.bOk);
	TestEqual(TEXT("Negative supplied required RR reason"), Result.Reason, FString(TEXT("invalid_cost_requirement")));

	InvalidContext = MakeCostGateContext(true, true, 0, -1);
	Result = WBCardActivationSourceGate::Evaluate(State, MakeCostGateDefinition(0), InvalidContext);
	TestFalse(TEXT("Negative supplied available RL fails"), Result.bOk);
	TestEqual(TEXT("Negative supplied available RL reason"), Result.Reason, FString(TEXT("invalid_cost_requirement")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostGateSimpleFlagInteractionTest, "Wandbound.Core.CardActivationCostGate.SimpleAndDetailedBothRequired", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostGateSimpleFlagInteractionTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeCostGateState();
	const FWBCardActivationSourceGateDefinition Gate = MakeCostGateDefinition(2, true);

	FWBCardActivationSourceGateResult Result =
		WBCardActivationSourceGate::Evaluate(State, Gate, MakeCostGateContext(true, true, 2, 3, false));
	TestFalse(TEXT("Simple cost flag false fails first"), Result.bOk);
	TestEqual(TEXT("Simple cost flag false reason"), Result.Reason, FString(TEXT("costs_not_satisfied")));

	Result = WBCardActivationSourceGate::Evaluate(State, Gate, MakeCostGateContext(true, false, 2, 3, true));
	TestFalse(TEXT("Detailed cost gate false fails"), Result.bOk);
	TestEqual(TEXT("Detailed cost gate false reason"), Result.Reason, FString(TEXT("cost_not_affordable")));

	Result = WBCardActivationSourceGate::Evaluate(State, Gate, MakeCostGateContext(true, true, 2, 3, true));
	TestTrue(TEXT("Both simple and detailed costs pass"), Result.bOk);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostGateCandidateFilteringTest, "Wandbound.Core.CardActivationCostGate.CandidateFilteringAndOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostGateCandidateFilteringTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCostGateState();
	const int32 InitialTargetHP = State.GetUnitById(2)->HP;

	FWBCardActivationSourceGateDefinition PassGate = MakeCostGateDefinition(2);
	FWBCardActivationSourceGateDefinition FailGate = MakeCostGateDefinition(3);

	FWBCardDefinition Definition = MakeCostGateCard(
		TEXT("fixture_cost_filter"),
		{
			MakeCostGateEffect(TEXT("first"), TEXT("First"), PassGate),
			MakeCostGateEffect(TEXT("blocked"), TEXT("Blocked"), FailGate),
			MakeCostGateEffect(TEXT("second"), TEXT("Second"), PassGate)
		});

	FWBCardActivationSourceGateContext Context = MakeCostGateContext(true, true, 2, 3);
	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State,
			{ MakeCostGateSource(Definition, { MakeCostGateUnitTarget(2), MakeCostGateUnitTarget(3) }, Context) });

	TestTrue(TEXT("Cost-filtered generation ok"), Result.bOk);
	const TArray<FString> ExpectedIds = {
		TEXT("activate:p0:u1:cfixture_cost_filter:efirst:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_cost_filter:efirst:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_cost_filter:esecond:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_cost_filter:esecond:t3:x-1:y-1:wa-1,-1:wb-1,-1")
	};
	CompareStringArrays(*this, TEXT("Cost-filtered candidate IDs"), CostGateCandidateIds(Result), ExpectedIds);
	TestEqual(TEXT("Candidate generation does not mutate HP"), State.GetUnitById(2)->HP, InitialTargetHP);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostGateNoPaymentAndOnceTest, "Wandbound.Core.CardActivationCostGate.GenerationReadOnlyAndOnceUsagePayment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostGateNoPaymentAndOnceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCostGateState();
	const int32 SourceRLTotalBefore = State.GetUnitById(1)->RLTotal;
	const int32 SourceRLUsedBefore = State.GetUnitById(1)->RLUsed;
	const int32 TargetRLTotalBefore = State.GetUnitById(2)->RLTotal;
	const int32 TargetRLUsedBefore = State.GetUnitById(2)->RLUsed;
	const int32 TargetArmorBefore = State.GetUnitById(2)->GetCurrentArmor();

	FWBCardActivationSourceGateDefinition Gate = MakeCostGateDefinition(2);
	FWBCardDefinition Definition = MakeCostGateCard(
		TEXT("fixture_cost_no_payment"),
		{ MakeCostGateEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), Gate) });
	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State,
			{ MakeCostGateSource(Definition, { MakeCostGateUnitTarget(2) }, MakeCostGateContext(true, true, 2, 3)) });
	TestTrue(TEXT("No-payment generation ok"), Result.bOk);
	TestEqual(TEXT("No-payment candidate count"), Result.Candidates.Num(), 1);
	TestEqual(TEXT("Source RLTotal unchanged after generation"), State.GetUnitById(1)->RLTotal, SourceRLTotalBefore);
	TestEqual(TEXT("Source RLUsed unchanged after generation"), State.GetUnitById(1)->RLUsed, SourceRLUsedBefore);
	TestEqual(TEXT("Target RLTotal unchanged after generation"), State.GetUnitById(2)->RLTotal, TargetRLTotalBefore);
	TestEqual(TEXT("Target RLUsed unchanged after generation"), State.GetUnitById(2)->RLUsed, TargetRLUsedBefore);
	TestEqual(TEXT("Target armor unchanged after generation"), State.GetUnitById(2)->GetCurrentArmor(), TargetArmorBefore);

	const FWBCardActivationCommandResult ApplyResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Result.Candidates[0].Command);
	TestTrue(TEXT("Explicit activation apply succeeds"), ApplyResult.bOk);
	TestEqual(TEXT("Source RLTotal unchanged after apply"), State.GetUnitById(1)->RLTotal, SourceRLTotalBefore);
	TestEqual(TEXT("Source RLUsed paid after apply"), State.GetUnitById(1)->RLUsed, SourceRLUsedBefore + 2);
	TestEqual(TEXT("Target RLTotal unchanged after apply"), State.GetUnitById(2)->RLTotal, TargetRLTotalBefore);
	TestEqual(TEXT("Target RLUsed unchanged after apply"), State.GetUnitById(2)->RLUsed, TargetRLUsedBefore);

	FWBGameStateData OnceState = MakeCostGateState();
	FWBCardActivationSourceGateDefinition OnceGate = MakeCostGateDefinition(2);
	OnceGate.bOncePerTurn = true;
	OnceGate.OncePerTurnKey = TEXT("fixture_cost_once");
	FWBCardDefinition OnceDefinition = MakeCostGateCard(
		TEXT("fixture_cost_once_card"),
		{ MakeCostGateEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), OnceGate) });
	FWBCardActivationSourceGateContext OnceContext = MakeCostGateContext(true, true, 2, 3);
	OnceContext.ActivationUsageKey = TEXT("fixture_cost_once");
	const FWBCardActivationCandidateGenerationResult OnceBeforeResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			OnceState,
			{ MakeCostGateSource(OnceDefinition, { MakeCostGateUnitTarget(2) }, OnceContext) });
	TestTrue(TEXT("Cost plus once initial generation ok"), OnceBeforeResult.bOk);
	TestEqual(TEXT("Cost plus once initial candidate count"), OnceBeforeResult.Candidates.Num(), 1);
	TestTrue(TEXT("Cost plus once apply succeeds"), WBEffectRunner::ApplyCardActivationCommand(OnceState, OnceBeforeResult.Candidates[0].Command).bOk);
	TestTrue(TEXT("Usage marked after explicit apply"), OnceState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_cost_once")));
	TestEqual(TEXT("Once source RLUsed paid"), OnceState.GetUnitById(1)->RLUsed, SourceRLUsedBefore + 2);

	const FWBCardActivationCandidateGenerationResult OnceAfterResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			OnceState,
			{ MakeCostGateSource(OnceDefinition, { MakeCostGateUnitTarget(2) }, OnceContext) });
	TestTrue(TEXT("Cost plus once second generation ok"), OnceAfterResult.bOk);
	TestEqual(TEXT("Cost plus once used candidate count"), OnceAfterResult.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostGateFixtureScenariosTest, "Wandbound.Core.CardActivationCostGate.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostGateFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_cost_gate_affordable_success.json"),
		TEXT("card_activation_cost_gate_missing_affordability_excluded.json"),
		TEXT("card_activation_cost_gate_not_affordable_excluded.json"),
		TEXT("card_activation_cost_gate_requirement_mismatch_excluded.json"),
		TEXT("card_activation_cost_gate_available_rl_insufficient_excluded.json"),
		TEXT("card_activation_cost_gate_zero_rr_success.json"),
		TEXT("card_activation_cost_gate_simple_flag_and_detailed_gate_both_required.json"),
		TEXT("card_activation_cost_gate_no_payment_mutation.json"),
		TEXT("card_activation_cost_gate_order_preserved.json"),
		TEXT("card_activation_cost_gate_with_once_per_turn_usage.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadCostGateFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

		TArray<FWBCardActivationCandidateSource> Sources;
		FString ParseReason;
		TestTrue(
			*FString::Printf(TEXT("%s parses sources: %s"), *FixtureName, *ParseReason),
			ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

		const FWBCardActivationCandidateGenerationResult Result =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		ExpectCandidateFixture(*this, FixtureName, Fixture, Result);

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationCandidates);
		TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s operation reason"), *FixtureName), OperationResult.Reason, Result.Reason);

		const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
		if (GetArrayField(Expected, TEXT("final_units")) != nullptr)
		{
			FString ExpectReason;
			TestTrue(
				*FString::Printf(TEXT("%s final RL/unit state: %s"), *FixtureName, *ExpectReason),
				ExpectUnitStatusSummary(Fixture, State, ExpectReason));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostGateSeparationTest, "Wandbound.Core.CardActivationCostGate.SeparateFromLegalActionsEffectRunnerAndCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostGateSeparationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCostGateState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State,
			{
				MakeCostGateSource(
					MakeCostGateCard(
						TEXT("fixture_cost_separation"),
						{ MakeCostGateEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), MakeCostGateDefinition(2)) }),
					{ MakeCostGateUnitTarget(2) },
					MakeCostGateContext(true, true, 2, 3))
			});
	TestTrue(TEXT("Cost gate candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Cost gate candidate generated"), CandidateResult.Candidates.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal action ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeCostGateAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeCostGateAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeCostGateAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeCostGateAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeCostGateAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString CandidateGeneratorPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationCandidateGenerator.cpp"));
	FString CandidateGeneratorSource;
	TestTrue(TEXT("Candidate generator source loads"), FFileHelper::LoadFileToString(CandidateGeneratorSource, *CandidateGeneratorPath));
	TestFalse(TEXT("Candidate generator has no GenerateLegalActions dependency"), CandidateGeneratorSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Candidate generator has no WBEffectRunner dependency"), CandidateGeneratorSource.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Candidate generator has no ApplyCardActivationCommand call"), CandidateGeneratorSource.Contains(TEXT("ApplyCardActivationCommand")));

	const FString SourceGatePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationSourceGate.cpp"));
	FString SourceGateSource;
	TestTrue(TEXT("Source gate source loads"), FFileHelper::LoadFileToString(SourceGateSource, *SourceGatePath));
	TestFalse(TEXT("Source gate has no EffectRunner dependency"), SourceGateSource.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Source gate has no legal action generation"), SourceGateSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Source gate has no action codec dependency"), SourceGateSource.Contains(TEXT("WBActionCodec")));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no activation ids"), ActionCodecSource.Contains(TEXT("activate:")));

	return true;
}

#endif
