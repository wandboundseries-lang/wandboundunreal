#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardActivationSourceGate.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeGatePlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeGateUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeGateState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeGatePlayer(0, 2));
	State.Players.Add(MakeGatePlayer(1, 0));
	State.AddUnitForTest(MakeGateUnit(1, 0, FWBTile(4, 4)));
	State.AddUnitForTest(MakeGateUnit(2, 1, FWBTile(5, 4)));
	State.AddUnitForTest(MakeGateUnit(3, 1, FWBTile(6, 4)));
	State.AddUnitForTest(MakeGateUnit(4, 0, FWBTile(4, 5)));
	return State;
}

FWBGenericEffectPayload MakeGateHealPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeGateStatusPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = FName(TEXT("Burn"));
	Payload.StatusEffect.Duration = 1;
	Payload.StatusEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardEffectDefinition MakeGateEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBCardActivationSourceGateDefinition& SourceGate,
	const FWBGenericEffectPayload& Payload = MakeGateHealPayload())
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = SourceGate;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardActivationSourceGateDefinition MakeBoardGate()
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.bBlockedByStunned = true;
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardActivationSourceGateContext MakeBoardContext(const int32 PlayerId = 0, const int32 SourceUnitId = 1)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = PlayerId;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.bCostsSatisfiedExternally = true;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBEffectTargetRef MakeGateUnitTarget(const int32 UnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardDefinition MakeGateDefinition(const FString& CardId, const TArray<FWBCardEffectDefinition>& Effects)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects = Effects;
	return Definition;
}

FWBCardActivationCandidateSource MakeGateSource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = 1)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.SourceGateContext = MakeBoardContext(0, SourceUnitId);
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	return Source;
}

TArray<FString> CandidateIds(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Ids.Add(Candidate.ActivationCandidateId);
	}
	return Ids;
}

TArray<FString> CandidateLabels(const FWBCardActivationCandidateGenerationResult& Result)
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

FString GateFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadGateFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GateFixturePath(FileName)))
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
		CompareStringArrays(Test, FixtureName + TEXT(" candidate ids"), CandidateIds(Result), ExpectedIds);
	}

	TArray<FString> ExpectedLabels;
	if (ReadStringArrayField(Expected, TEXT("public_labels"), ExpectedLabels))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" public labels"), CandidateLabels(Result), ExpectedLabels);
	}

	return true;
}

FWBAction MakeGateAction(const EWBActionType Type, const int32 PlayerId = 0)
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceGateSourcePolicyTest, "Wandbound.Core.CardActivationSourceGate.SourcePolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceGateSourcePolicyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGateState();
	FWBCardActivationSourceGateDefinition Gate = MakeBoardGate();
	FWBCardActivationSourceGateContext Context = MakeBoardContext();

	FWBCardActivationSourceGateResult Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestTrue(TEXT("Board owner source passes"), Result.bOk);
	TestEqual(TEXT("Board owner reason"), Result.Reason, FString());

	Context.PlayerId = 1;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Wrong owner fails"), Result.bOk);
	TestEqual(TEXT("Wrong owner reason"), Result.Reason, FString(TEXT("source_unit_owner_mismatch")));

	Context = MakeBoardContext();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Stunned")), 1);
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Stunned source fails"), Result.bOk);
	TestEqual(TEXT("Stunned reason"), Result.Reason, FString(TEXT("source_unit_stunned")));

	State.GetMutableUnitById(1)->RemoveStatus(FName(TEXT("Stunned")));
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Frozen")), 1);
	Gate.bBlockedByFrozen = false;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestTrue(TEXT("Frozen source passes when not blocked"), Result.bOk);
	Gate.bBlockedByFrozen = true;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Frozen source fails when blocked"), Result.bOk);
	TestEqual(TEXT("Frozen reason"), Result.Reason, FString(TEXT("source_unit_frozen")));

	State.GetMutableUnitById(1)->RemoveStatus(FName(TEXT("Frozen")));
	Context.SourceZone = EWBCardActivationSourceZone::Hand;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Wrong source zone fails"), Result.bOk);
	TestEqual(TEXT("Wrong source zone reason"), Result.Reason, FString(TEXT("source_zone_mismatch")));

	FWBCardActivationSourceGateDefinition DefaultGate;
	FWBCardActivationSourceGateContext DefaultContext;
	DefaultContext.PlayerId = 0;
	DefaultContext.SourceZone = EWBCardActivationSourceZone::Fixture;
	Result = WBCardActivationSourceGate::Evaluate(State, DefaultGate, DefaultContext);
	TestTrue(TEXT("Fixture default passes"), Result.bOk);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceGateTimingCostAndOnceTest, "Wandbound.Core.CardActivationSourceGate.TimingCostAndOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceGateTimingCostAndOnceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGateState();
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Fixture;
	Gate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
	Gate.bHasExplicitSourceGate = true;
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceZone = EWBCardActivationSourceZone::Fixture;
	Context.bHasExplicitSourceGateContext = true;

	FWBCardActivationSourceGateResult Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestTrue(TEXT("Normal turn priority passes"), Result.bOk);

	Context.PlayerId = 1;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Wrong priority player fails"), Result.bOk);
	TestEqual(TEXT("Wrong priority reason"), Result.Reason, FString(TEXT("timing_not_normal_turn_priority")));

	Context.PlayerId = 0;
	Gate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Response window not supported"), Result.bOk);
	TestEqual(TEXT("Response reason"), Result.Reason, FString(TEXT("response_window_not_supported")));

	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresCostsSatisfiedExternally = true;
	Context.bCostsSatisfiedExternally = false;
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Costs not satisfied fails"), Result.bOk);
	TestEqual(TEXT("Cost reason"), Result.Reason, FString(TEXT("costs_not_satisfied")));

	Context.bCostsSatisfiedExternally = true;
	Gate.bOncePerTurn = true;
	Gate.OncePerTurnKey = TEXT("fixture_once_key");
	Context.ActivationUsageKey = TEXT("fixture_once_key");
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestTrue(TEXT("Once unused passes"), Result.bOk);

	State.MarkActivationUsageKeyForTest(0, TEXT("fixture_once_key"));
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestFalse(TEXT("Once used fails"), Result.bOk);
	TestEqual(TEXT("Once used reason"), Result.Reason, FString(TEXT("once_per_turn_already_used")));

	FString MarkReason;
	TestFalse(TEXT("Mark invalid player fails"), WBCardActivationSourceGate::MarkUsageIfAllowedForTest(State, 4, TEXT("key"), MarkReason));
	TestEqual(TEXT("Mark invalid reason"), MarkReason, FString(TEXT("invalid_player")));
	TestFalse(TEXT("Mark empty key fails"), WBCardActivationSourceGate::MarkUsageIfAllowedForTest(State, 0, FString(), MarkReason));
	TestEqual(TEXT("Mark empty reason"), MarkReason, FString(TEXT("usage_key_missing")));

	const FString DefaultUsageKey =
		WBCardActivationSourceGate::BuildDefaultUsageKey(0, 1, TEXT("fixture_card"), TEXT("fixture_effect"));
	TestEqual(TEXT("Default usage key"), DefaultUsageKey, FString(TEXT("activate_usage:p0:u1:cfixture_card:efixture_effect")));

	FString SetupReason;
	TestTrue(TEXT("Turn start setup succeeds"), State.ApplyTurnStartResourceSetupForPlayer(0, 4, SetupReason));
	TestFalse(TEXT("Usage key reset"), State.HasActivationUsageKeyThisTurn(0, TEXT("fixture_once_key")));
	Result = WBCardActivationSourceGate::Evaluate(State, Gate, Context);
	TestTrue(TEXT("Once passes after reset"), Result.bOk);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceGateCandidateFilteringTest, "Wandbound.Core.CardActivationSourceGate.CandidateFilteringAndOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceGateCandidateFilteringTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGateState();
	const int32 InitialHP = State.GetUnitById(2)->HP;

	FWBCardActivationSourceGateDefinition PassGate = MakeBoardGate();
	PassGate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
	FWBCardActivationSourceGateDefinition FailGate = PassGate;
	FailGate.RequiredZone = EWBCardActivationSourceZone::Equipped;

	FWBCardDefinition Definition = MakeGateDefinition(
		TEXT("fixture_gated_card"),
		{
			MakeGateEffect(TEXT("first"), TEXT("First"), PassGate),
			MakeGateEffect(TEXT("blocked"), TEXT("Blocked"), FailGate),
			MakeGateEffect(TEXT("second"), TEXT("Second"), PassGate)
		});

	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State,
			{ MakeGateSource(Definition, { MakeGateUnitTarget(2), MakeGateUnitTarget(3) }) });

	TestTrue(TEXT("Candidate generation ok"), Result.bOk);
	const TArray<FString> ExpectedIds = {
		TEXT("activate:p0:u1:cfixture_gated_card:efirst:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_gated_card:efirst:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_gated_card:esecond:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_gated_card:esecond:t3:x-1:y-1:wa-1,-1:wb-1,-1")
	};
	CompareStringArrays(*this, TEXT("Candidate IDs"), CandidateIds(Result), ExpectedIds);
	TestEqual(TEXT("No mutation"), State.GetUnitById(2)->HP, InitialHP);

	FWBGameStateData FrozenState = MakeGateState();
	FrozenState.GetMutableUnitById(1)->AddStatus(FName(TEXT("Frozen")), 1);
	FWBCardActivationSourceGateDefinition FrozenAllowedGate = MakeBoardGate();
	FrozenAllowedGate.bBlockedByFrozen = false;
	const FWBCardActivationCandidateGenerationResult FrozenAllowedResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			FrozenState,
			{ MakeGateSource(MakeGateDefinition(TEXT("fixture_frozen_allowed"), { MakeGateEffect(TEXT("allowed"), TEXT("Allowed"), FrozenAllowedGate) }), { MakeGateUnitTarget(2) }) });
	TestTrue(TEXT("Explicit frozen policy generation ok"), FrozenAllowedResult.bOk);
	TestEqual(TEXT("Explicit frozen allowed candidate count"), FrozenAllowedResult.Candidates.Num(), 1);

	FWBCardDefinition DefaultDefinition = MakeGateDefinition(
		TEXT("fixture_default_gate"),
		{ MakeGateEffect(TEXT("default"), TEXT("Default"), FWBCardActivationSourceGateDefinition()) });
	FWBCardActivationCandidateSource DefaultSource;
	DefaultSource.PlayerId = 0;
	DefaultSource.SourceUnitId = 1;
	DefaultSource.CardDefinition = DefaultDefinition;
	DefaultSource.CandidateTargets = { MakeGateUnitTarget(2) };
	const FWBCardActivationCandidateGenerationResult LegacyFrozenResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(FrozenState, { DefaultSource });
	TestTrue(TEXT("Legacy default frozen generation ok"), LegacyFrozenResult.bOk);
	TestEqual(TEXT("Legacy default frozen excluded"), LegacyFrozenResult.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceGateFixtureScenariosTest, "Wandbound.Core.CardActivationSourceGate.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceGateFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> StandaloneFixtureNames = {
		TEXT("card_activation_gate_board_source_owner_success.json"),
		TEXT("card_activation_gate_wrong_owner_fails.json"),
		TEXT("card_activation_gate_normal_turn_priority_success.json"),
		TEXT("card_activation_gate_normal_turn_wrong_player_excluded.json"),
		TEXT("card_activation_gate_once_per_turn_unused_success.json"),
		TEXT("card_activation_gate_once_per_turn_used_excluded.json")
	};

	for (const FString& FixtureName : StandaloneFixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGateFixture(FixtureName, Fixture));
		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::EvaluateCardActivationSourceGate);

		const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), Expected->TryGetBoolField(TEXT("ok"), bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), OperationResult.bOk, bExpectedOk);
		FString ReasonContains;
		if (!OperationResult.bOk && Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains))
		{
			TestTrue(*FString::Printf(TEXT("%s reason contains"), *FixtureName), OperationResult.Reason.Contains(ReasonContains));
		}
	}

	const TArray<FString> CandidateFixtureNames = {
		TEXT("card_activation_gate_stunned_source_excluded.json"),
		TEXT("card_activation_gate_frozen_source_policy.json"),
		TEXT("card_activation_gate_wrong_zone_excluded.json"),
		TEXT("card_activation_gate_external_cost_not_satisfied_excluded.json"),
		TEXT("card_activation_candidates_with_gates_ordered.json")
	};

	for (const FString& FixtureName : CandidateFixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadGateFixture(FixtureName, Fixture));
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
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationCandidates);
		TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, Result.bOk);
	}

	TSharedPtr<FJsonObject> ResetFixture;
	TestTrue(TEXT("reset fixture loads"), LoadGateFixture(TEXT("card_activation_gate_once_per_turn_reset_on_turn_start.json"), ResetFixture));
	FWBGameStateData ResetState;
	FString ResetStateReason;
	TestTrue(TEXT("reset fixture builds state"), BuildGameStateFromFixture(ResetFixture, ResetState, ResetStateReason));
	TestTrue(TEXT("usage key starts marked"), ResetState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_once_key")));
	FString SetupReason;
	TestTrue(TEXT("resource setup clears usage"), ResetState.ApplyTurnStartResourceSetupForPlayer(0, 4, SetupReason));
	TestFalse(TEXT("usage key cleared"), ResetState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_once_key")));

	FWBCardActivationSourceGateDefinition OnceGate;
	OnceGate.RequiredZone = EWBCardActivationSourceZone::Fixture;
	OnceGate.bOncePerTurn = true;
	OnceGate.OncePerTurnKey = TEXT("fixture_once_key");
	FWBCardActivationSourceGateContext OnceContext;
	OnceContext.PlayerId = 0;
	OnceContext.SourceZone = EWBCardActivationSourceZone::Fixture;
	OnceContext.ActivationUsageKey = TEXT("fixture_once_key");
	TestTrue(TEXT("gate passes after reset"), WBCardActivationSourceGate::Evaluate(ResetState, OnceGate, OnceContext).bOk);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceGateSeparationTest, "Wandbound.Core.CardActivationSourceGate.SeparateFromFWBActionAndCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceGateSeparationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGateState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	FWBCardActivationSourceGateDefinition Gate = MakeBoardGate();
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(
			State,
			{ MakeGateSource(MakeGateDefinition(TEXT("fixture_gate_separation"), { MakeGateEffect(TEXT("heal"), TEXT("Heal"), Gate) }), { MakeGateUnitTarget(2) }) });
	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Candidate count"), CandidateResult.Candidates.Num(), 1);

	const FWBCardActivationLegalActionGenerationResult LegalActivationResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	TestTrue(TEXT("Activation legal family ok"), LegalActivationResult.bOk);
	TestEqual(TEXT("Activation legal family count"), LegalActivationResult.ActionSet.Actions.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal action ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeGateAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeGateAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeGateAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeGateAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeGateAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString CandidateGeneratorPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationCandidateGenerator.cpp"));
	FString CandidateGeneratorSource;
	TestTrue(TEXT("Candidate generator source loads"), FFileHelper::LoadFileToString(CandidateGeneratorSource, *CandidateGeneratorPath));
	TestFalse(TEXT("No GenerateLegalActions dependency"), CandidateGeneratorSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No WBEffectRunner dependency"), CandidateGeneratorSource.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No ApplyCardActivationCommand call"), CandidateGeneratorSource.Contains(TEXT("ApplyCardActivationCommand")));

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
