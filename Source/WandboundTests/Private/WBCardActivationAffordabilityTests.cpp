#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationAffordability.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeAffordabilityPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeAffordabilityUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 RLTotal = 3,
	const int32 RLUsed = 1,
	const bool bRemoved = false,
	const bool bDefeated = false)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = bDefeated ? 0 : 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.bRemovedFromBoard = bRemoved;
	Unit.bDefeated = bDefeated;
	return Unit;
}

FWBGameStateData MakeAffordabilityState(
	const int32 SourceRLTotal = 3,
	const int32 SourceRLUsed = 1,
	const int32 SourceOwnerId = 0,
	const bool bSourceRemoved = false,
	const bool bSourceDefeated = false)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeAffordabilityPlayer(0, 2));
	State.Players.Add(MakeAffordabilityPlayer(1, 0));
	State.AddUnitForTest(MakeAffordabilityUnit(
		1,
		SourceOwnerId,
		FWBTile(4, 4),
		SourceRLTotal,
		SourceRLUsed,
		bSourceRemoved,
		bSourceDefeated));
	State.AddUnitForTest(MakeAffordabilityUnit(2, 1, FWBTile(5, 4), 4, 1));
	return State;
}

FWBCardActivationAffordabilityRequest MakeAffordabilityRequest(
	const int32 PlayerId = 0,
	const int32 SourceUnitId = 1,
	const int32 RequiredRR = 2)
{
	FWBCardActivationAffordabilityRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceUnitId = SourceUnitId;
	Request.SourceCardId = TEXT("fixture_affordability");
	Request.SourceEffectId = TEXT("effect_1");
	Request.RequiredRR = RequiredRR;
	Request.CostKind = FName(TEXT("RR"));
	return Request;
}

FWBCardActivationSourceGateDefinition MakeAffordabilityGate(const int32 RequiredRR)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.CostGate.bRequiresExternalAffordability = true;
	Gate.CostGate.RequiredRR = RequiredRR;
	Gate.CostGate.CostKind = FName(TEXT("RR"));
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardActivationSourceGateContext MakeAffordabilitySourceContext()
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = 1;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBGenericEffectPayload MakeAffordabilityHealPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardEffectDefinition MakeAffordabilityEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const int32 RequiredRR)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = MakeAffordabilityGate(RequiredRR);
	Effect.Payloads.Add(MakeAffordabilityHealPayload());
	return Effect;
}

FWBCardDefinition MakeAffordabilityCard(
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

FWBEffectTargetRef MakeAffordabilityTarget(const int32 UnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardActivationCandidateSource MakeAffordabilitySource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = 1;
	Source.SourceGateContext = MakeAffordabilitySourceContext();
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	return Source;
}

class FUnitRLAffordabilityProvider final : public IWBCardActivationAffordabilityProvider
{
public:
	virtual FWBCardActivationAffordabilityResult QueryAffordability(
		const FWBGameStateData& State,
		const FWBCardActivationAffordabilityRequest& Request) const override
	{
		return WBCardActivationAffordability::QueryFromUnitRL(State, Request);
	}
};

TArray<FWBCardActivationCandidateSource> PrepareSourcesWithUnitRL(
	const FWBGameStateData& State,
	const TArray<FWBCardActivationCandidateSource>& Sources)
{
	FUnitRLAffordabilityProvider Provider;
	TArray<FWBCardActivationCandidateSource> PreparedSources;
	PreparedSources.Reserve(Sources.Num());
	for (const FWBCardActivationCandidateSource& Source : Sources)
	{
		PreparedSources.Add(WBCardActivationAffordability::BuildCandidateSourceWithAffordability(
			State,
			Source,
			Provider));
	}

	return PreparedSources;
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

FString AffordabilityFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadAffordabilityFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AffordabilityFixturePath(FileName)))
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

bool ExpectQueryFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	FWBCardActivationAffordabilityRequest Request;
	FString ParseReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s parses request: %s"), *FixtureName, *ParseReason),
		ParseCardActivationAffordabilityRequestFromFixture(Fixture, Request, ParseReason));

	const FWBCardActivationAffordabilityResult Result =
		WBCardActivationAffordability::QueryFromUnitRL(State, Request);

	bool bExpectedOk = false;
	Test.TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), Expected->TryGetBoolField(TEXT("ok"), bExpectedOk));
	Test.TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), Result.bOk, bExpectedOk);

	if (Expected->HasField(TEXT("affordable")))
	{
		bool bExpectedAffordable = false;
		Test.TestTrue(*FString::Printf(TEXT("%s expected affordable reads"), *FixtureName), Expected->TryGetBoolField(TEXT("affordable"), bExpectedAffordable));
		Test.TestEqual(*FString::Printf(TEXT("%s affordable"), *FixtureName), Result.bAffordable, bExpectedAffordable);
	}

	if (Expected->HasField(TEXT("reason")))
	{
		FString ExpectedReason;
		Test.TestTrue(*FString::Printf(TEXT("%s expected reason reads"), *FixtureName), Expected->TryGetStringField(TEXT("reason"), ExpectedReason));
		Test.TestEqual(*FString::Printf(TEXT("%s reason"), *FixtureName), Result.Reason, ExpectedReason);
	}

	int32 ExpectedCurrentRL = -1;
	if (TryReadIntegerField(Expected, TEXT("current_rl"), ExpectedCurrentRL))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s current rl"), *FixtureName), Result.CurrentRL, ExpectedCurrentRL);
	}

	int32 ExpectedRLUsed = -1;
	if (TryReadIntegerField(Expected, TEXT("rl_used"), ExpectedRLUsed))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s rl used"), *FixtureName), Result.RLUsed, ExpectedRLUsed);
	}

	int32 ExpectedAvailableRL = -1;
	if (TryReadIntegerField(Expected, TEXT("available_rl"), ExpectedAvailableRL))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s available rl"), *FixtureName), Result.AvailableRL, ExpectedAvailableRL);
	}

	FWBApplyActionResult OperationResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString OperationReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *OperationReason),
		ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
	Test.TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::QueryCardActivationAffordability);
	Test.TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, Result.bOk);
	Test.TestEqual(*FString::Printf(TEXT("%s operation reason"), *FixtureName), OperationResult.Reason, Result.Reason);
	Test.TestEqual(*FString::Printf(TEXT("%s emits no traces"), *FixtureName), OperationResult.TraceEvents.Num(), 0);

	if (GetArrayField(Expected, TEXT("final_units")) != nullptr)
	{
		FString ExpectReason;
		Test.TestTrue(
			*FString::Printf(TEXT("%s final RL/unit state: %s"), *FixtureName, *ExpectReason),
			ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	}

	return true;
}

bool ExpectCandidateFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	TArray<FWBCardActivationCandidateSource> Sources;
	FString ParseReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s parses sources: %s"), *FixtureName, *ParseReason),
		ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

	const TArray<FWBCardActivationCandidateSource> PreparedSources =
		PrepareSourcesWithUnitRL(State, Sources);
	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, PreparedSources);

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

	if (GetArrayField(Expected, TEXT("final_units")) != nullptr)
	{
		FString ExpectReason;
		Test.TestTrue(
			*FString::Printf(TEXT("%s final RL/unit state: %s"), *FixtureName, *ExpectReason),
			ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	}

	return true;
}

FWBAction MakeAffordabilityAction(const EWBActionType Type, const int32 PlayerId = 0)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationAffordabilityQueryTest, "Wandbound.Core.CardActivationAffordability.QueryFromUnitRL", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationAffordabilityQueryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAffordabilityState(3, 1);
	const int32 SourceRLTotalBefore = State.GetUnitById(1)->RLTotal;
	const int32 SourceRLUsedBefore = State.GetUnitById(1)->RLUsed;

	FWBCardActivationAffordabilityResult Result =
		WBCardActivationAffordability::QueryFromUnitRL(State, MakeAffordabilityRequest(0, 1, 2));
	TestTrue(TEXT("Affordable query ok"), Result.bOk);
	TestTrue(TEXT("Affordable query affordable"), Result.bAffordable);
	TestEqual(TEXT("Current RL from RLTotal"), Result.CurrentRL, 3);
	TestEqual(TEXT("RL used"), Result.RLUsed, 1);
	TestEqual(TEXT("Available RL"), Result.AvailableRL, 2);
	TestEqual(TEXT("Cost kind"), Result.CostKind, FName(TEXT("RR")));

	Result = WBCardActivationAffordability::QueryFromUnitRL(MakeAffordabilityState(2, 1), MakeAffordabilityRequest(0, 1, 2));
	TestTrue(TEXT("Unaffordable query still ok"), Result.bOk);
	TestFalse(TEXT("Unaffordable result"), Result.bAffordable);
	TestEqual(TEXT("Unaffordable available RL"), Result.AvailableRL, 1);
	TestEqual(TEXT("Unaffordable reason empty"), Result.Reason, FString());

	Result = WBCardActivationAffordability::QueryFromUnitRL(MakeAffordabilityState(0, 0), MakeAffordabilityRequest(0, 1, 0));
	TestTrue(TEXT("Zero RR query ok"), Result.bOk);
	TestTrue(TEXT("Zero RR affordable"), Result.bAffordable);
	TestEqual(TEXT("Zero RR available RL"), Result.AvailableRL, 0);

	Result = WBCardActivationAffordability::QueryFromUnitRL(State, MakeAffordabilityRequest(2, 1, 2));
	TestFalse(TEXT("Invalid player fails"), Result.bOk);
	TestEqual(TEXT("Invalid player reason"), Result.Reason, FString(TEXT("invalid_player")));

	Result = WBCardActivationAffordability::QueryFromUnitRL(State, MakeAffordabilityRequest(0, 1, -1));
	TestFalse(TEXT("Negative RR fails"), Result.bOk);
	TestEqual(TEXT("Negative RR reason"), Result.Reason, FString(TEXT("invalid_cost_requirement")));

	Result = WBCardActivationAffordability::QueryFromUnitRL(State, MakeAffordabilityRequest(0, 99, 2));
	TestFalse(TEXT("Missing source fails"), Result.bOk);
	TestEqual(TEXT("Missing source reason"), Result.Reason, FString(TEXT("source_unit_missing")));

	Result = WBCardActivationAffordability::QueryFromUnitRL(MakeAffordabilityState(3, 1, 0, true, true), MakeAffordabilityRequest(0, 1, 2));
	TestFalse(TEXT("Removed source fails"), Result.bOk);
	TestEqual(TEXT("Removed source reason"), Result.Reason, FString(TEXT("source_unit_removed")));

	Result = WBCardActivationAffordability::QueryFromUnitRL(MakeAffordabilityState(3, 1, 1), MakeAffordabilityRequest(0, 1, 2));
	TestFalse(TEXT("Owner mismatch fails"), Result.bOk);
	TestEqual(TEXT("Owner mismatch reason"), Result.Reason, FString(TEXT("source_unit_owner_mismatch")));

	TestEqual(TEXT("Source RLTotal not mutated"), State.GetUnitById(1)->RLTotal, SourceRLTotalBefore);
	TestEqual(TEXT("Source RLUsed not mutated"), State.GetUnitById(1)->RLUsed, SourceRLUsedBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationAffordabilityContextTest, "Wandbound.Core.CardActivationAffordability.ContextPreparationAndCandidateFiltering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationAffordabilityContextTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAffordabilityState(3, 1);
	const int32 SourceRLUsedBefore = State.GetUnitById(1)->RLUsed;

	FWBCardActivationSourceGateContext Context = MakeAffordabilitySourceContext();
	const FWBCardActivationAffordabilityResult Result =
		WBCardActivationAffordability::QueryFromUnitRL(State, MakeAffordabilityRequest(0, 1, 2));
	WBCardActivationAffordability::ApplyResultToSourceGateContext(Result, Context);
	TestTrue(TEXT("Detailed affordability present"), Context.CostContext.bHasExternalAffordability);
	TestTrue(TEXT("Detailed affordable"), Context.CostContext.bExternallyAffordable);
	TestEqual(TEXT("Required RR copied"), Context.CostContext.SuppliedRequiredRR, 2);
	TestEqual(TEXT("Available RL copied"), Context.CostContext.SuppliedAvailableRL, 2);
	TestEqual(TEXT("Cost kind copied"), Context.CostContext.CostKind, FName(TEXT("RR")));
	TestTrue(TEXT("Simple flag synchronized"), Context.bCostsSatisfiedExternally);

	const FWBCardDefinition Definition = MakeAffordabilityCard(
		TEXT("fixture_affordability_context"),
		{ MakeAffordabilityEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), 2) });
	const FWBCardActivationCandidateSource Source =
		MakeAffordabilitySource(Definition, { MakeAffordabilityTarget(2) });
	FUnitRLAffordabilityProvider Provider;
	const FWBCardActivationCandidateSource PreparedSource =
		WBCardActivationAffordability::BuildCandidateSourceWithAffordability(State, Source, Provider);
	TestEqual(TEXT("Original source has no effect context"), Source.EffectIdToSourceGateContext.Num(), 0);
	TestEqual(TEXT("Prepared source has effect context"), PreparedSource.EffectIdToSourceGateContext.Num(), 1);

	FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { PreparedSource });
	TestTrue(TEXT("Prepared affordable source generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Prepared affordable source generates candidate"), CandidateResult.Candidates.Num(), 1);

	FWBGameStateData UnaffordableState = MakeAffordabilityState(3, 2);
	const FWBCardActivationCandidateSource UnaffordablePreparedSource =
		WBCardActivationAffordability::BuildCandidateSourceWithAffordability(UnaffordableState, Source, Provider);
	CandidateResult = WBCardActivationCandidateGenerator::GenerateCandidates(UnaffordableState, { UnaffordablePreparedSource });
	TestTrue(TEXT("Prepared unaffordable source generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Prepared unaffordable source excludes candidate"), CandidateResult.Candidates.Num(), 0);

	TestEqual(TEXT("No RL payment mutation"), State.GetUnitById(1)->RLUsed, SourceRLUsedBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationAffordabilityProviderAndPerEffectTest, "Wandbound.Core.CardActivationAffordability.FixedProviderAndPerEffectContexts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationAffordabilityProviderAndPerEffectTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAffordabilityState(3, 1);

	FWBFixedCardActivationAffordabilityProvider FixedProvider;
	FixedProvider.FixedResult.bOk = true;
	FixedProvider.FixedResult.bAffordable = true;
	FixedProvider.FixedResult.AvailableRL = 9;
	const FWBCardActivationAffordabilityResult FixedResult =
		FixedProvider.QueryAffordability(State, MakeAffordabilityRequest(0, 1, 4));
	TestTrue(TEXT("Fixed provider ok"), FixedResult.bOk);
	TestTrue(TEXT("Fixed provider affordable"), FixedResult.bAffordable);
	TestEqual(TEXT("Fixed provider copies player"), FixedResult.PlayerId, 0);
	TestEqual(TEXT("Fixed provider copies source"), FixedResult.SourceUnitId, 1);
	TestEqual(TEXT("Fixed provider copies required RR when unset"), FixedResult.RequiredRR, 4);
	TestEqual(TEXT("Fixed provider copies cost kind when unset"), FixedResult.CostKind, FName(TEXT("RR")));
	TestEqual(TEXT("Fixed provider leaves state RLUsed"), State.GetUnitById(1)->RLUsed, 1);

	const FWBCardDefinition Definition = MakeAffordabilityCard(
		TEXT("fixture_affordability_per_effect_direct"),
		{
			MakeAffordabilityEffect(TEXT("cheap"), TEXT("Cheap Effect"), 1),
			MakeAffordabilityEffect(TEXT("expensive"), TEXT("Expensive Effect"), 3)
		});
	const FWBCardActivationCandidateSource Source =
		MakeAffordabilitySource(Definition, { MakeAffordabilityTarget(2) });
	FUnitRLAffordabilityProvider UnitProvider;
	const FWBCardActivationCandidateSource PreparedSource =
		WBCardActivationAffordability::BuildCandidateSourceWithAffordability(State, Source, UnitProvider);
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { PreparedSource });
	TestTrue(TEXT("Per-effect generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Only cheap candidate generated"), CandidateResult.Candidates.Num(), 1);
	TestEqual(TEXT("Cheap effect generated"), CandidateResult.Candidates[0].SourceEffectId, FString(TEXT("cheap")));
	TestEqual(TEXT("Original source remains unprepared"), Source.EffectIdToSourceGateContext.Num(), 0);
	TestEqual(TEXT("Prepared source has two effect contexts"), PreparedSource.EffectIdToSourceGateContext.Num(), 2);
	TestEqual(TEXT("No per-effect RL payment mutation"), State.GetUnitById(1)->RLUsed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationAffordabilityFixtureTest, "Wandbound.Core.CardActivationAffordability.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationAffordabilityFixtureTest::RunTest(const FString& Parameters)
{
	const TArray<FString> QueryFixtureNames = {
		TEXT("card_activation_affordability_query_affordable.json"),
		TEXT("card_activation_affordability_query_unaffordable.json"),
		TEXT("card_activation_affordability_query_zero_rr.json"),
		TEXT("card_activation_affordability_query_missing_source_fails.json"),
		TEXT("card_activation_affordability_query_removed_source_fails.json"),
		TEXT("card_activation_affordability_query_owner_mismatch_fails.json"),
		TEXT("card_activation_affordability_does_not_mutate_rl.json")
	};

	for (const FString& FixtureName : QueryFixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadAffordabilityFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
		ExpectQueryFixture(*this, FixtureName, Fixture, State);
	}

	const TArray<FString> CandidateFixtureNames = {
		TEXT("card_activation_affordability_context_feeds_cost_gate_success.json"),
		TEXT("card_activation_affordability_context_feeds_cost_gate_excluded.json"),
		TEXT("card_activation_affordability_per_effect_costs_if_supported.json")
	};

	for (const FString& FixtureName : CandidateFixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadAffordabilityFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
		ExpectCandidateFixture(*this, FixtureName, Fixture, State);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationAffordabilitySeparationTest, "Wandbound.Core.CardActivationAffordability.SeparateFromPaymentLegalActionsEffectRunnerAndCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationAffordabilitySeparationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAffordabilityState(3, 1);
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	const int32 SourceRLTotalBefore = State.GetUnitById(1)->RLTotal;
	const int32 SourceRLUsedBefore = State.GetUnitById(1)->RLUsed;

	const FWBCardDefinition Definition = MakeAffordabilityCard(
		TEXT("fixture_affordability_separation"),
		{ MakeAffordabilityEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), 2) });
	const FWBCardActivationCandidateSource Source =
		MakeAffordabilitySource(Definition, { MakeAffordabilityTarget(2) });
	FUnitRLAffordabilityProvider Provider;
	const FWBCardActivationCandidateSource PreparedSource =
		WBCardActivationAffordability::BuildCandidateSourceWithAffordability(State, Source, Provider);
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { PreparedSource });
	TestTrue(TEXT("Affordability-prepared candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Affordability-prepared candidate generated"), CandidateResult.Candidates.Num(), 1);
	TestEqual(TEXT("Source RLTotal unchanged after prepare/generate"), State.GetUnitById(1)->RLTotal, SourceRLTotalBefore);
	TestEqual(TEXT("Source RLUsed unchanged after prepare/generate"), State.GetUnitById(1)->RLUsed, SourceRLUsedBefore);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal action ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeAffordabilityAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeAffordabilityAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeAffordabilityAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeAffordabilityAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeAffordabilityAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationAffordability.cpp"));
	FString SourceText;
	TestTrue(TEXT("Affordability source loads"), FFileHelper::LoadFileToString(SourceText, *SourcePath));
	TestFalse(TEXT("Affordability has no EffectRunner dependency"), SourceText.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Affordability has no legal action generation dependency"), SourceText.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Affordability has no activation command apply dependency"), SourceText.Contains(TEXT("ApplyCardActivationCommand")));
	TestFalse(TEXT("Affordability has no RLUsed mutation"), SourceText.Contains(TEXT("->RLUsed =")));
	TestFalse(TEXT("Affordability has no overflow behavior"), SourceText.Contains(TEXT("overflow"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("Affordability has no wand behavior"), SourceText.Contains(TEXT("wand"), ESearchCase::IgnoreCase));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no activation ids"), ActionCodecSource.Contains(TEXT("activate:")));

	return true;
}

#endif
