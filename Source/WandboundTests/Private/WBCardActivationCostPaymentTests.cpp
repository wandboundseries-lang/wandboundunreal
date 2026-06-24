#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationCostPayment.h"
#include "WBCardActivationExpansion.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakePaymentPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakePaymentUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 RLTotal = 3,
	const int32 RLUsed = 1)
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
	return Unit;
}

FWBGameStateData MakePaymentState(
	const int32 SourceRLTotal = 3,
	const int32 SourceRLUsed = 1,
	const int32 TargetHP = 2,
	const int32 TargetMaxHP = 5)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakePaymentPlayer(0, 2));
	State.Players.Add(MakePaymentPlayer(1, 0));
	State.AddUnitForTest(MakePaymentUnit(1, 0, FWBTile(4, 4), 5, 5, SourceRLTotal, SourceRLUsed));
	State.AddUnitForTest(MakePaymentUnit(2, 1, FWBTile(5, 4), TargetHP, TargetMaxHP, 4, 1));
	return State;
}

FWBCardActivationCostPaymentRequest MakePaymentRequest(
	const int32 RequiredRR = 2,
	const int32 SourceUnitId = 1,
	const int32 PlayerId = 0)
{
	FWBCardActivationCostPaymentRequest Request;
	Request.PlayerId = PlayerId;
	Request.SourceUnitId = SourceUnitId;
	Request.RequiredRR = RequiredRR;
	Request.CostKind = FName(TEXT("RR"));
	return Request;
}

FWBGenericEffectPayload MakePaymentHealPayload(const int32 Amount = 1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakePaymentDamagePayload(const int32 Amount = 1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBEffectTargetRef MakePaymentUnitTarget(const int32 TargetUnitId = 2)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = TargetUnitId;
	return Target;
}

FWBCardActivationCommand MakePaymentCommandWithPayloads(
	const int32 RequiredRR,
	const TArray<FWBGenericEffectPayload>& Payloads,
	const int32 TargetUnitId = 2,
	const bool bWithUsageCommit = false)
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = 1;
	Command.Source.SourceCardId = TEXT("fixture_payment_card");
	Command.Source.SourceEffectId = TEXT("fixture_payment_effect");
	Command.DebugActivationId = TEXT("fixture_payment_activation");
	Command.EffectRequest.Target = MakePaymentUnitTarget(TargetUnitId);
	Command.EffectRequest.Payloads = Payloads;
	Command.CostPaymentCommit.bPayCostOnSuccess = true;
	Command.CostPaymentCommit.PlayerId = 0;
	Command.CostPaymentCommit.SourceUnitId = 1;
	Command.CostPaymentCommit.RequiredRR = RequiredRR;
	Command.CostPaymentCommit.CostKind = FName(TEXT("RR"));
	if (bWithUsageCommit)
	{
		Command.UsageCommit.bMarkUsageOnSuccess = true;
		Command.UsageCommit.PlayerId = 0;
		Command.UsageCommit.UsageKey = TEXT("fixture_payment_usage_key");
	}
	return Command;
}

FWBCardActivationCommand MakePaymentCommand(const int32 RequiredRR = 2)
{
	return MakePaymentCommandWithPayloads(RequiredRR, { MakePaymentHealPayload() });
}

FWBCardActivationSourceGateDefinition MakePaymentCostGate(const int32 RequiredRR, const bool bOncePerTurn = false)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.CostGate.bRequiresExternalAffordability = true;
	Gate.CostGate.RequiredRR = RequiredRR;
	Gate.CostGate.CostKind = FName(TEXT("RR"));
	Gate.bOncePerTurn = bOncePerTurn;
	Gate.OncePerTurnKey = bOncePerTurn ? FString(TEXT("fixture_payment_once")) : FString();
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardEffectDefinition MakePaymentEffect(
	const int32 RequiredRR = 2,
	const FString& EffectId = TEXT("heal_1"),
	const bool bOncePerTurn = false)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = TEXT("Heal 1 HP");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = MakePaymentCostGate(RequiredRR, bOncePerTurn);
	Effect.Payloads.Add(MakePaymentHealPayload());
	return Effect;
}

FWBCardDefinition MakePaymentCard(const int32 RequiredRR = 2, const bool bOncePerTurn = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("fixture_payment_card");
	Definition.PublicName = TEXT("Fixture Payment Card");
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakePaymentEffect(RequiredRR, TEXT("heal_1"), bOncePerTurn));
	return Definition;
}

FWBCardActivationSourceGateContext MakePaymentGateContext(const int32 RequiredRR = 2, const int32 AvailableRL = 2)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = 1;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.bCostsSatisfiedExternally = true;
	Context.CostContext.bHasExternalAffordability = true;
	Context.CostContext.bExternallyAffordable = true;
	Context.CostContext.SuppliedRequiredRR = RequiredRR;
	Context.CostContext.SuppliedAvailableRL = AvailableRL;
	Context.CostContext.CostKind = FName(TEXT("RR"));
	Context.ActivationUsageKey = TEXT("fixture_payment_once");
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationExpansionRequest MakePaymentExpansionRequest(const int32 RequiredRR = 2)
{
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = 0;
	Request.SourceUnitId = 1;
	Request.CardDefinition = MakePaymentCard(RequiredRR);
	Request.EffectId = TEXT("heal_1");
	Request.Target = MakePaymentUnitTarget(2);
	Request.SourceGateContext = MakePaymentGateContext(RequiredRR, 2);
	return Request;
}

FWBCardActivationCandidateSource MakePaymentCandidateSource(const int32 RequiredRR = 2, const bool bOncePerTurn = false)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = 1;
	Source.CardDefinition = MakePaymentCard(RequiredRR, bOncePerTurn);
	Source.CandidateTargets.Add(MakePaymentUnitTarget(2));
	Source.SourceGateContext = MakePaymentGateContext(RequiredRR, 2);
	return Source;
}

const FWBTraceEvent* FindTraceEventByKind(const TArray<FWBTraceEvent>& TraceEvents, const FName Kind)
{
	for (const FWBTraceEvent& Event : TraceEvents)
	{
		if (Event.Kind == Kind)
		{
			return &Event;
		}
	}
	return nullptr;
}

TArray<FString> TraceKinds(const TArray<FWBTraceEvent>& TraceEvents)
{
	TArray<FString> Kinds;
	for (const FWBTraceEvent& Event : TraceEvents)
	{
		Kinds.Add(Event.Kind.ToString());
	}
	return Kinds;
}

TArray<FString> ActivationCandidateIds(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Ids.Add(Candidate.ActivationCandidateId);
	}
	return Ids;
}

TArray<FString> ActivationActionIds(const FWBCardActivationLegalActionGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationLegalAction& Action : Result.ActionSet.Actions)
	{
		Ids.Add(Action.ActivationActionId);
	}
	return Ids;
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

FString PaymentFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadPaymentFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *PaymentFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, OutFixture);
	return OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* ChildObject = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, ChildObject)
		&& ChildObject != nullptr
		&& ChildObject->IsValid())
	{
		return *ChildObject;
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

void ExpectCostPaymentCommit(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& ExpectedCommit,
	const FWBCardActivationCostPaymentCommit& Commit)
{
	bool bExpectedPayCost = Commit.bPayCostOnSuccess;
	if (ExpectedCommit.IsValid() && ExpectedCommit->TryGetBoolField(TEXT("pay_cost_on_success"), bExpectedPayCost))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s pay cost"), *Label), Commit.bPayCostOnSuccess, bExpectedPayCost);
	}

	int32 ExpectedInteger = -1;
	if (TryReadIntegerField(ExpectedCommit, TEXT("player_id"), ExpectedInteger))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s player id"), *Label), Commit.PlayerId, ExpectedInteger);
	}
	if (TryReadIntegerField(ExpectedCommit, TEXT("source_unit_id"), ExpectedInteger))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s source unit id"), *Label), Commit.SourceUnitId, ExpectedInteger);
	}
	if (TryReadIntegerField(ExpectedCommit, TEXT("required_rr"), ExpectedInteger))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s required rr"), *Label), Commit.RequiredRR, ExpectedInteger);
	}

	FString ExpectedCostKind;
	if (ExpectedCommit.IsValid() && ExpectedCommit->TryGetStringField(TEXT("cost_kind"), ExpectedCostKind))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s cost kind"), *Label), Commit.CostKind.ToString(), ExpectedCostKind);
	}
}

bool ExpectCostPaymentTraceFields(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TArray<FWBTraceEvent>& TraceEvents)
{
	const TArray<TSharedPtr<FJsonValue>>* TraceFieldValues = GetArrayField(ExpectedObject, TEXT("trace_fields"));
	if (TraceFieldValues == nullptr)
	{
		return true;
	}

	for (int32 Index = 0; Index < TraceFieldValues->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> TraceFieldObject = (*TraceFieldValues)[Index].IsValid()
			? (*TraceFieldValues)[Index]->AsObject()
			: nullptr;
		if (!TraceFieldObject.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s malformed trace_fields[%d]"), *FixtureName, Index));
			return false;
		}

		FString Kind;
		if (!TraceFieldObject->TryGetStringField(TEXT("kind"), Kind))
		{
			Test.AddError(FString::Printf(TEXT("%s missing trace_fields[%d].kind"), *FixtureName, Index));
			return false;
		}

		const FWBTraceEvent* Event = FindTraceEventByKind(TraceEvents, FName(*Kind));
		if (Event == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("%s missing trace kind %s"), *FixtureName, *Kind));
			return false;
		}

		int32 ExpectedInteger = -1;
		if (TryReadIntegerField(TraceFieldObject, TEXT("player_id"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s player id"), *FixtureName, *Kind), Event->PlayerId, ExpectedInteger);
		}
		if (TryReadIntegerField(TraceFieldObject, TEXT("source_unit_id"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s source unit id"), *FixtureName, *Kind), Event->SourceUnitId, ExpectedInteger);
		}
		if (TryReadIntegerField(TraceFieldObject, TEXT("cost_amount"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s cost amount"), *FixtureName, *Kind), Event->CostAmount, ExpectedInteger);
		}
		if (TryReadIntegerField(TraceFieldObject, TEXT("previous_rl_used"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s previous rl used"), *FixtureName, *Kind), Event->PreviousRLUsed, ExpectedInteger);
		}
		if (TryReadIntegerField(TraceFieldObject, TEXT("new_rl_used"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s new rl used"), *FixtureName, *Kind), Event->NewRLUsed, ExpectedInteger);
		}
		if (TryReadIntegerField(TraceFieldObject, TEXT("available_rl_before"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s available before"), *FixtureName, *Kind), Event->AvailableRLBefore, ExpectedInteger);
		}
		if (TryReadIntegerField(TraceFieldObject, TEXT("available_rl_after"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s available after"), *FixtureName, *Kind), Event->AvailableRLAfter, ExpectedInteger);
		}

		FString ExpectedCostKind;
		if (TraceFieldObject->TryGetStringField(TEXT("cost_kind"), ExpectedCostKind))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s %s cost kind"), *FixtureName, *Kind), Event->CostKind.ToString(), ExpectedCostKind);
		}
	}

	return true;
}

bool ExpectPaymentFixtureApplication(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture)
{
	FWBGameStateData State;
	FString StateReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason),
		BuildGameStateFromFixture(Fixture, State, StateReason));

	FWBApplyActionResult Result;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString OperationReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
		ApplyFixtureOperation(Fixture, State, Result, OperationKind, OperationReason));

	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	bool bExpectedOk = Result.bOk;
	if (Expected->TryGetBoolField(TEXT("ok"), bExpectedOk))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), Result.bOk, bExpectedOk);
	}

	FString ReasonContains;
	if (Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains))
	{
		Test.TestTrue(
			*FString::Printf(TEXT("%s reason contains %s, got %s"), *FixtureName, *ReasonContains, *Result.Reason),
			Result.Reason.Contains(ReasonContains));
	}

	if (GetArrayField(Expected, TEXT("expected_trace_order")) != nullptr)
	{
		FString TraceReason;
		Test.TestTrue(
			*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *TraceReason),
			ExpectTraceOrder(Fixture, Result.TraceEvents, TraceReason));
	}

	if (GetArrayField(Expected, TEXT("final_units")) != nullptr)
	{
		FString UnitReason;
		Test.TestTrue(
			*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *UnitReason),
			ExpectUnitStatusSummary(Fixture, State, UnitReason));
	}

	bool bExpectedCostPaid = false;
	if (Expected->TryGetBoolField(TEXT("cost_paid"), bExpectedCostPaid))
	{
		Test.TestEqual(
			*FString::Printf(TEXT("%s cost paid trace"), *FixtureName),
			FindTraceEventByKind(Result.TraceEvents, FName(TEXT("card_activation_cost_paid"))) != nullptr,
			bExpectedCostPaid);
	}

	bool bExpectedCostNotPaid = false;
	if (Expected->TryGetBoolField(TEXT("cost_not_paid"), bExpectedCostNotPaid))
	{
		Test.TestEqual(
			*FString::Printf(TEXT("%s cost not paid trace"), *FixtureName),
			FindTraceEventByKind(Result.TraceEvents, FName(TEXT("card_activation_cost_paid"))) == nullptr,
			bExpectedCostNotPaid);
	}

	const TSharedPtr<FJsonObject> Usage = GetObjectField(Expected, TEXT("usage"));
	if (Usage.IsValid())
	{
		int32 PlayerId = -1;
		FString UsageKey;
		bool bMarked = false;
		Test.TestTrue(*FString::Printf(TEXT("%s usage player id"), *FixtureName), TryReadIntegerField(Usage, TEXT("player_id"), PlayerId));
		Test.TestTrue(*FString::Printf(TEXT("%s usage key"), *FixtureName), Usage->TryGetStringField(TEXT("usage_key"), UsageKey));
		Test.TestTrue(*FString::Printf(TEXT("%s usage marked field"), *FixtureName), Usage->TryGetBoolField(TEXT("marked"), bMarked));
		Test.TestEqual(
			*FString::Printf(TEXT("%s usage marked"), *FixtureName),
			State.HasActivationUsageKeyThisTurn(PlayerId, UsageKey),
			bMarked);
	}

	ExpectCostPaymentTraceFields(Test, FixtureName, Expected, Result.TraceEvents);

	TArray<FString> ForbiddenSubstrings;
	if (ReadStringArrayField(Expected, TEXT("forbidden_substrings"), ForbiddenSubstrings))
	{
		const FString SerializedTrace = WBReplayTrace::SerializeEvents(Result.TraceEvents);
		for (const FString& ForbiddenSubstring : ForbiddenSubstrings)
		{
			Test.TestFalse(
				*FString::Printf(TEXT("%s trace hides %s"), *FixtureName, *ForbiddenSubstring),
				SerializedTrace.Contains(ForbiddenSubstring, ESearchCase::IgnoreCase));
		}
	}

	TArray<FString> ForbiddenTraceKinds;
	if (ReadStringArrayField(Expected, TEXT("forbidden_trace_kinds"), ForbiddenTraceKinds))
	{
		const TArray<FString> ActualTraceKinds = TraceKinds(Result.TraceEvents);
		for (const FString& ForbiddenKind : ForbiddenTraceKinds)
		{
			Test.TestFalse(
				*FString::Printf(TEXT("%s no %s trace"), *FixtureName, *ForbiddenKind),
				ActualTraceKinds.Contains(ForbiddenKind));
		}
	}

	return true;
}

bool ExpectPaymentCandidateFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture)
{
	FWBGameStateData State;
	FString StateReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason),
		BuildGameStateFromFixture(Fixture, State, StateReason));

	TArray<FWBCardActivationCandidateSource> Sources;
	FString ParseReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s parses sources: %s"), *FixtureName, *ParseReason),
		ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	bool bExpectedOk = CandidateResult.bOk;
	if (Expected->TryGetBoolField(TEXT("ok"), bExpectedOk))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), CandidateResult.bOk, bExpectedOk);
	}

	int32 ExpectedCandidateCount = -1;
	if (TryReadIntegerField(Expected, TEXT("candidate_count"), ExpectedCandidateCount))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s candidate count"), *FixtureName), CandidateResult.Candidates.Num(), ExpectedCandidateCount);
	}

	TArray<FString> ExpectedCandidateIds;
	if (ReadStringArrayField(Expected, TEXT("candidate_ids"), ExpectedCandidateIds))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" candidate ids"), ActivationCandidateIds(CandidateResult), ExpectedCandidateIds);
	}

	if (CandidateResult.Candidates.Num() > 0)
	{
		ExpectCostPaymentCommit(
			Test,
			FixtureName + TEXT(" candidate commit"),
			GetObjectField(Expected, TEXT("cost_payment_commit")),
			CandidateResult.Candidates[0].Command.CostPaymentCommit);
	}

	FWBApplyActionResult OperationResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString OperationReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *OperationReason),
		ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
	Test.TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationCandidates);
	Test.TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, CandidateResult.bOk);

	return true;
}

bool ExpectPaymentLegalActionFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture)
{
	FWBGameStateData State;
	FString StateReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason),
		BuildGameStateFromFixture(Fixture, State, StateReason));

	TArray<FWBCardActivationCandidateSource> Sources;
	FString ParseReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s parses sources: %s"), *FixtureName, *ParseReason),
		ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	Test.TestTrue(*FString::Printf(TEXT("%s candidate generation ok"), *FixtureName), CandidateResult.bOk);

	const FWBCardActivationLegalActionGenerationResult LegalResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	if (!Expected.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("%s missing expected"), *FixtureName));
		return false;
	}

	bool bExpectedOk = LegalResult.bOk;
	if (Expected->TryGetBoolField(TEXT("ok"), bExpectedOk))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), LegalResult.bOk, bExpectedOk);
	}

	int32 ExpectedActionCount = -1;
	if (TryReadIntegerField(Expected, TEXT("activation_action_count"), ExpectedActionCount))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s action count"), *FixtureName), LegalResult.ActionSet.Actions.Num(), ExpectedActionCount);
	}

	TArray<FString> ExpectedActionIds;
	if (ReadStringArrayField(Expected, TEXT("activation_action_ids"), ExpectedActionIds))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" action ids"), ActivationActionIds(LegalResult), ExpectedActionIds);
	}

	if (LegalResult.ActionSet.Actions.Num() > 0)
	{
		ExpectCostPaymentCommit(
			Test,
			FixtureName + TEXT(" legal action commit"),
			GetObjectField(Expected, TEXT("cost_payment_commit")),
			LegalResult.ActionSet.Actions[0].Command.CostPaymentCommit);
	}

	FWBApplyActionResult OperationResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString OperationReason;
	Test.TestTrue(
		*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *OperationReason),
		ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
	Test.TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationLegalActions);
	Test.TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, LegalResult.bOk);

	return true;
}

FWBAction MakeCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentHelperTest, "Wandbound.Core.CardActivationCostPayment.Helper", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentHelperTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePaymentState(3, 1);
	const FWBCardActivationCostPaymentResult Affordable = WBCardActivationCostPayment::CanPayCost(State, MakePaymentRequest(2));
	TestTrue(TEXT("CanPayCost affordable"), Affordable.bOk);
	TestEqual(TEXT("Affordable previous RLUsed"), Affordable.PreviousRLUsed, 1);
	TestEqual(TEXT("Affordable available before"), Affordable.AvailableRLBefore, 2);
	TestEqual(TEXT("Affordable new RLUsed"), Affordable.NewRLUsed, 3);
	TestEqual(TEXT("Affordable available after"), Affordable.AvailableRLAfter, 0);
	TestEqual(TEXT("CanPayCost read-only"), State.GetUnitById(1)->RLUsed, 1);

	FWBGameStateData UnaffordableState = MakePaymentState(3, 2);
	const FWBCardActivationCostPaymentResult Unaffordable =
		WBCardActivationCostPayment::CanPayCost(UnaffordableState, MakePaymentRequest(2));
	TestFalse(TEXT("CanPayCost unaffordable"), Unaffordable.bOk);
	TestEqual(TEXT("Unaffordable reason"), Unaffordable.Reason, FString(TEXT("cost_not_affordable")));
	TestEqual(TEXT("Unaffordable no mutation"), UnaffordableState.GetUnitById(1)->RLUsed, 2);

	const FWBCardActivationCostPaymentResult Paid = WBCardActivationCostPayment::PayCost(State, MakePaymentRequest(2));
	TestTrue(TEXT("PayCost ok"), Paid.bOk);
	TestEqual(TEXT("PayCost new RLUsed"), State.GetUnitById(1)->RLUsed, 3);
	TestEqual(TEXT("PayCost available after"), Paid.AvailableRLAfter, 0);

	FWBGameStateData ZeroState = MakePaymentState(3, 1);
	const FWBCardActivationCostPaymentResult ZeroPaid = WBCardActivationCostPayment::PayCost(ZeroState, MakePaymentRequest(0));
	TestTrue(TEXT("PayCost zero RR ok"), ZeroPaid.bOk);
	TestEqual(TEXT("PayCost zero no-op"), ZeroState.GetUnitById(1)->RLUsed, 1);

	FWBGameStateData InvalidSourceState = MakePaymentState(3, 1);
	const FWBCardActivationCostPaymentResult InvalidSource =
		WBCardActivationCostPayment::PayCost(InvalidSourceState, MakePaymentRequest(1, 99));
	TestFalse(TEXT("PayCost invalid source fails"), InvalidSource.bOk);
	TestEqual(TEXT("PayCost invalid source reason"), InvalidSource.Reason, FString(TEXT("source_unit_missing")));
	TestEqual(TEXT("PayCost invalid source no mutation"), InvalidSourceState.GetUnitById(1)->RLUsed, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentPropagationTest, "Wandbound.Core.CardActivationCostPayment.ExpansionCandidateLegalPropagation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentPropagationTest::RunTest(const FString& Parameters)
{
	const FWBCardActivationExpansionResult ExpansionResult =
		WBCardActivationExpansion::BuildActivationCommand(MakePaymentExpansionRequest(2));
	TestTrue(TEXT("Expansion ok"), ExpansionResult.bOk);
	TestTrue(TEXT("Expansion commit pays"), ExpansionResult.Command.CostPaymentCommit.bPayCostOnSuccess);
	TestEqual(TEXT("Expansion commit player"), ExpansionResult.Command.CostPaymentCommit.PlayerId, 0);
	TestEqual(TEXT("Expansion commit source"), ExpansionResult.Command.CostPaymentCommit.SourceUnitId, 1);
	TestEqual(TEXT("Expansion commit rr"), ExpansionResult.Command.CostPaymentCommit.RequiredRR, 2);
	TestEqual(TEXT("Expansion commit kind"), ExpansionResult.Command.CostPaymentCommit.CostKind, FName(TEXT("RR")));

	const FWBCardActivationExpansionResult NoCostExpansionResult =
		WBCardActivationExpansion::BuildActivationCommand(MakePaymentExpansionRequest(0));
	TestTrue(TEXT("Zero external cost expansion ok"), NoCostExpansionResult.bOk);
	TestTrue(TEXT("Zero external cost still commits when gate requires affordability"), NoCostExpansionResult.Command.CostPaymentCommit.bPayCostOnSuccess);
	TestEqual(TEXT("Zero external cost rr"), NoCostExpansionResult.Command.CostPaymentCommit.RequiredRR, 0);

	FWBGameStateData State = MakePaymentState();
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { MakePaymentCandidateSource(2) });
	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Candidate count"), CandidateResult.Candidates.Num(), 1);
	TestTrue(TEXT("Candidate retains commit"), CandidateResult.Candidates[0].Command.CostPaymentCommit.bPayCostOnSuccess);
	TestEqual(TEXT("Candidate commit rr"), CandidateResult.Candidates[0].Command.CostPaymentCommit.RequiredRR, 2);

	const FWBCardActivationLegalActionGenerationResult LegalResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	TestTrue(TEXT("Legal action generation ok"), LegalResult.bOk);
	TestEqual(TEXT("Legal action count"), LegalResult.ActionSet.Actions.Num(), 1);
	TestTrue(TEXT("Legal action retains commit"), LegalResult.ActionSet.Actions[0].Command.CostPaymentCommit.bPayCostOnSuccess);
	TestEqual(TEXT("Legal action commit rr"), LegalResult.ActionSet.Actions[0].Command.CostPaymentCommit.RequiredRR, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentApplyTest, "Wandbound.Core.CardActivationCostPayment.ApplyAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentApplyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData SuccessState = MakePaymentState(3, 1, 2, 5);
	const FWBCardActivationCommandResult SuccessResult =
		WBEffectRunner::ApplyCardActivationCommand(SuccessState, MakePaymentCommand(2));
	TestTrue(TEXT("Activation payment success ok"), SuccessResult.bOk);
	TestEqual(TEXT("Activation payment increments RLUsed"), SuccessState.GetUnitById(1)->RLUsed, 3);
	TestEqual(TEXT("Activation effect applied"), SuccessState.GetUnitById(2)->HP, 3);
	const FWBTraceEvent* CostTrace = FindTraceEventByKind(SuccessResult.TraceEvents, FName(TEXT("card_activation_cost_paid")));
	TestNotNull(TEXT("Cost paid trace exists"), CostTrace);
	if (CostTrace != nullptr)
	{
		TestEqual(TEXT("Cost trace amount"), CostTrace->CostAmount, 2);
		TestEqual(TEXT("Cost trace previous RLUsed"), CostTrace->PreviousRLUsed, 1);
		TestEqual(TEXT("Cost trace new RLUsed"), CostTrace->NewRLUsed, 3);
		TestEqual(TEXT("Cost trace available before"), CostTrace->AvailableRLBefore, 2);
		TestEqual(TEXT("Cost trace available after"), CostTrace->AvailableRLAfter, 0);
		TestEqual(TEXT("Cost trace kind"), CostTrace->CostKind, FName(TEXT("RR")));
	}

	FWBGameStateData MissingTargetState = MakePaymentState(3, 1, 2, 5);
	const FWBCardActivationCommandResult MissingTargetResult =
		WBEffectRunner::ApplyCardActivationCommand(MissingTargetState, MakePaymentCommandWithPayloads(2, { MakePaymentHealPayload() }, 99));
	TestFalse(TEXT("Invalid effect target fails"), MissingTargetResult.bOk);
	TestEqual(TEXT("Invalid effect target reason"), MissingTargetResult.Reason, FString(TEXT("missing_effect_target_unit")));
	TestEqual(TEXT("Invalid target does not pay"), MissingTargetState.GetUnitById(1)->RLUsed, 1);
	TestEqual(TEXT("Invalid target no effect"), MissingTargetState.GetUnitById(2)->HP, 2);
	TestNull(TEXT("Invalid target no cost trace"), FindTraceEventByKind(MissingTargetResult.TraceEvents, FName(TEXT("card_activation_cost_paid"))));

	FWBGameStateData UnaffordableState = MakePaymentState(3, 2, 2, 5);
	const FWBCardActivationCommandResult UnaffordableResult =
		WBEffectRunner::ApplyCardActivationCommand(UnaffordableState, MakePaymentCommand(2));
	TestFalse(TEXT("Unaffordable activation fails"), UnaffordableResult.bOk);
	TestEqual(TEXT("Unaffordable activation reason"), UnaffordableResult.Reason, FString(TEXT("cost_not_affordable")));
	TestEqual(TEXT("Unaffordable activation no payment"), UnaffordableState.GetUnitById(1)->RLUsed, 2);
	TestEqual(TEXT("Unaffordable activation no effect"), UnaffordableState.GetUnitById(2)->HP, 2);

	FWBGameStateData ZeroState = MakePaymentState(3, 1, 2, 5);
	const FWBCardActivationCommandResult ZeroResult =
		WBEffectRunner::ApplyCardActivationCommand(ZeroState, MakePaymentCommand(0));
	TestTrue(TEXT("Zero RR activation ok"), ZeroResult.bOk);
	TestEqual(TEXT("Zero RR no RL mutation"), ZeroState.GetUnitById(1)->RLUsed, 1);
	CostTrace = FindTraceEventByKind(ZeroResult.TraceEvents, FName(TEXT("card_activation_cost_paid")));
	TestNotNull(TEXT("Zero RR cost trace exists"), CostTrace);
	if (CostTrace != nullptr)
	{
		TestEqual(TEXT("Zero RR cost trace amount"), CostTrace->CostAmount, 0);
		TestEqual(TEXT("Zero RR cost trace new RLUsed"), CostTrace->NewRLUsed, 1);
	}

	FWBGameStateData InvalidPaymentSourceState = MakePaymentState(3, 1, 2, 5);
	FWBCardActivationCommand InvalidPaymentSourceCommand = MakePaymentCommand(1);
	InvalidPaymentSourceCommand.CostPaymentCommit.SourceUnitId = 99;
	const FWBCardActivationCommandResult InvalidPaymentSourceResult =
		WBEffectRunner::ApplyCardActivationCommand(InvalidPaymentSourceState, InvalidPaymentSourceCommand);
	TestFalse(TEXT("Invalid payment source fails"), InvalidPaymentSourceResult.bOk);
	TestEqual(TEXT("Invalid payment source reason"), InvalidPaymentSourceResult.Reason, FString(TEXT("source_unit_missing")));
	TestEqual(TEXT("Invalid payment source no mutation"), InvalidPaymentSourceState.GetUnitById(1)->RLUsed, 1);

	FWBGameStateData UsageState = MakePaymentState(3, 1, 2, 5);
	const FWBCardActivationCommandResult UsageResult =
		WBEffectRunner::ApplyCardActivationCommand(UsageState, MakePaymentCommandWithPayloads(2, { MakePaymentHealPayload() }, 2, true));
	TestTrue(TEXT("Payment plus usage ok"), UsageResult.bOk);
	TestEqual(TEXT("Payment plus usage RLUsed"), UsageState.GetUnitById(1)->RLUsed, 3);
	TestTrue(TEXT("Payment plus usage marked"), UsageState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_payment_usage_key")));
	const TArray<FString> ExpectedUsageTraceOrder = {
		TEXT("card_activation_resolved"),
		TEXT("card_activation_cost_paid"),
		TEXT("effect_request_resolved"),
		TEXT("heal_effect_resolved"),
		TEXT("card_activation_usage_marked")
	};
	CompareStringArrays(*this, TEXT("Payment plus usage trace order"), TraceKinds(UsageResult.TraceEvents), ExpectedUsageTraceOrder);

	FWBGameStateData UsagePaymentFailureState = MakePaymentState(3, 2, 2, 5);
	const FWBCardActivationCommandResult UsagePaymentFailureResult =
		WBEffectRunner::ApplyCardActivationCommand(UsagePaymentFailureState, MakePaymentCommandWithPayloads(2, { MakePaymentHealPayload() }, 2, true));
	TestFalse(TEXT("Usage not marked if payment fails"), UsagePaymentFailureResult.bOk);
	TestFalse(TEXT("Failed payment leaves usage unmarked"), UsagePaymentFailureState.HasActivationUsageKeyThisTurn(0, TEXT("fixture_payment_usage_key")));

	FWBGameStateData MixedFailureState = MakePaymentState(3, 1, 2, 5);
	const FWBCardActivationCommandResult MixedFailureResult =
		WBEffectRunner::ApplyCardActivationCommand(
			MixedFailureState,
			MakePaymentCommandWithPayloads(2, { MakePaymentHealPayload(1), MakePaymentDamagePayload(-1) }));
	TestFalse(TEXT("Mixed payload invalid activation fails"), MixedFailureResult.bOk);
	TestEqual(TEXT("Mixed payload invalid reason"), MixedFailureResult.Reason, FString(TEXT("negative_damage_effect_amount")));
	TestEqual(TEXT("Mixed payload invalid no payment"), MixedFailureState.GetUnitById(1)->RLUsed, 1);
	TestEqual(TEXT("Mixed payload invalid no partial effect"), MixedFailureState.GetUnitById(2)->HP, 2);
	TestEqual(TEXT("Mixed payload invalid no traces"), MixedFailureResult.TraceEvents.Num(), 0);

	FWBGameStateData HiddenTraceState = MakePaymentState(3, 1, 2, 5);
	FWBCardActivationCommand HiddenTraceCommand = MakePaymentCommand(2);
	HiddenTraceCommand.Source.SourceCardId = TEXT("secret_deck_card_token");
	HiddenTraceCommand.Source.SourceEffectId = TEXT("secret_hand_effect_token");
	HiddenTraceCommand.DebugActivationId = TEXT("secret_debug_activation_token");
	const FWBCardActivationCommandResult HiddenTraceResult =
		WBEffectRunner::ApplyCardActivationCommand(HiddenTraceState, HiddenTraceCommand);
	TestTrue(TEXT("Hidden metadata trace activation ok"), HiddenTraceResult.bOk);
	const FString SerializedTrace = WBReplayTrace::SerializeEvents(HiddenTraceResult.TraceEvents);
	TestFalse(TEXT("Trace hides source card id"), SerializedTrace.Contains(TEXT("secret_deck_card_token")));
	TestFalse(TEXT("Trace hides source effect id"), SerializedTrace.Contains(TEXT("secret_hand_effect_token")));
	TestFalse(TEXT("Trace hides debug activation id"), SerializedTrace.Contains(TEXT("secret_debug_activation_token")));
	TestFalse(TEXT("Trace hides SourceCardId field"), SerializedTrace.Contains(TEXT("SourceCardId")));
	TestFalse(TEXT("Trace hides SourceEffectId field"), SerializedTrace.Contains(TEXT("SourceEffectId")));
	TestFalse(TEXT("Trace hides DebugActivationId field"), SerializedTrace.Contains(TEXT("DebugActivationId")));

	FWBGameStateData ExactFillState = MakePaymentState(3, 1, 2, 5);
	const FWBCardActivationCommandResult ExactFillResult =
		WBEffectRunner::ApplyCardActivationCommand(ExactFillState, MakePaymentCommand(2));
	TestTrue(TEXT("Exact RL fill succeeds"), ExactFillResult.bOk);
	TestEqual(TEXT("Exact RL fill no overflow cleanup"), ExactFillState.GetUnitById(1)->RLUsed, ExactFillState.GetUnitById(1)->RLTotal);
	const FString ExactFillTrace = WBReplayTrace::SerializeEvents(ExactFillResult.TraceEvents);
	TestFalse(TEXT("Exact fill emits no overflow trace"), ExactFillTrace.Contains(TEXT("overflow"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("Exact fill emits no wand trace"), ExactFillTrace.Contains(TEXT("wand"), ESearchCase::IgnoreCase));

	FWBGameStateData BeyondFillState = MakePaymentState(3, 2, 2, 5);
	const FWBCardActivationCommandResult BeyondFillResult =
		WBEffectRunner::ApplyCardActivationCommand(BeyondFillState, MakePaymentCommand(2));
	TestFalse(TEXT("Beyond RL fill fails"), BeyondFillResult.bOk);
	TestEqual(TEXT("Beyond RL fill reason"), BeyondFillResult.Reason, FString(TEXT("cost_not_affordable")));
	TestEqual(TEXT("Beyond RL fill no mutation"), BeyondFillState.GetUnitById(1)->RLUsed, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentFixtureScenariosTest, "Wandbound.Core.CardActivationCostPayment.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_cost_payment_paid_on_success.json"),
		TEXT("card_activation_cost_payment_not_paid_on_effect_failure.json"),
		TEXT("card_activation_cost_payment_unaffordable_fails_no_mutation.json"),
		TEXT("card_activation_cost_payment_zero_rr_noop_success.json"),
		TEXT("card_activation_cost_payment_invalid_source_fails.json"),
		TEXT("card_activation_cost_payment_preserved_through_candidate.json"),
		TEXT("card_activation_cost_payment_preserved_through_legal_action_family.json"),
		TEXT("card_activation_cost_payment_with_usage_marking_success.json"),
		TEXT("card_activation_cost_payment_trace_no_hidden_metadata.json"),
		TEXT("card_activation_cost_payment_no_overflow_wand_behavior.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadPaymentFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FString Operation;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), Fixture->TryGetStringField(TEXT("operation"), Operation));
		if (Operation == TEXT("generate_card_activation_candidates"))
		{
			ExpectPaymentCandidateFixture(*this, FixtureName, Fixture);
		}
		else if (Operation == TEXT("generate_card_activation_legal_actions"))
		{
			ExpectPaymentLegalActionFixture(*this, FixtureName, Fixture);
		}
		else
		{
			ExpectPaymentFixtureApplication(*this, FixtureName, Fixture);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCostPaymentSeparationTest, "Wandbound.Core.CardActivationCostPayment.SeparateFromFWBActionAndCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCostPaymentSeparationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakePaymentState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { MakePaymentCandidateSource(2) });
	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Activation candidate generated"), CandidateResult.Candidates.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal action IDs unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id unchanged"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no activation IDs"), ActionCodecSource.Contains(TEXT("activate:")));

	const FString RulesSourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBRules.cpp"));
	FString RulesSource;
	TestTrue(TEXT("Rules source loads"), FFileHelper::LoadFileToString(RulesSource, *RulesSourcePath));
	TestFalse(TEXT("WBRules GenerateLegalActions has no activation action literal"), RulesSource.Contains(TEXT("EWBActionType::Activate")));

	return true;
}

#endif
