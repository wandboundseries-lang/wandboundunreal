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
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeCandidatePlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeCandidateUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 CurrentArmor = 0,
	const int32 MaxArmor = 0)
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
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(CurrentArmor, MaxArmor);
	return Unit;
}

FWBGameStateData MakeCandidateState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeCandidatePlayer(0, 2));
	State.Players.Add(MakeCandidatePlayer(1, 0));
	State.AddUnitForTest(MakeCandidateUnit(1, 0, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeCandidateUnit(2, 1, FWBTile(5, 4), 5, 6, 1, 3));
	State.AddUnitForTest(MakeCandidateUnit(3, 1, FWBTile(6, 4), 3, 5));
	State.AddUnitForTest(MakeCandidateUnit(4, 0, FWBTile(4, 5), 5, 5));
	return State;
}

FWBGenericEffectPayload MakeCandidateArmorPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = EWBArmorEffectOp::AddCurrentArmor;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeCandidateStatusPayload(const FName StatusId, const int32 Duration)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeCandidateDamagePayload(const int32 Amount, const bool bBypassArmor = false)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = bBypassArmor;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeCandidateHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBCardDefinition MakeCandidateDefinition(
	const FString& CardId,
	const FString& EffectId,
	const FString& PublicLabel,
	const EWBCardEffectTargetRequirement TargetRequirement,
	const TArray<FWBGenericEffectPayload>& Payloads)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Fixture;

	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = TargetRequirement;
	Effect.Payloads = Payloads;
	Definition.ActivatedEffects.Add(Effect);
	return Definition;
}

FWBEffectTargetRef MakeUnitTarget(const int32 UnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardActivationCandidateSource MakeCandidateSource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = 1)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	return Source;
}

FWBAction MakeCandidateMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeCandidateAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeCandidatePlayerAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FString CandidateFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

TSharedPtr<FJsonObject> ParseCandidateJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

bool LoadCandidateFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CandidateFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseCandidateJsonObject(Json);
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

bool ExpectCandidateSourcesFromFixture(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TArray<FWBCardActivationCandidate>& Candidates)
{
	const TArray<TSharedPtr<FJsonValue>>* CandidateSources = GetArrayField(Expected, TEXT("candidate_sources"));
	if (CandidateSources == nullptr)
	{
		return true;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s source count"), *Label), Candidates.Num(), CandidateSources->Num());
	const int32 NumToCompare = FMath::Min(Candidates.Num(), CandidateSources->Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		const TSharedPtr<FJsonObject> SourceObject = (*CandidateSources)[Index].IsValid()
			? (*CandidateSources)[Index]->AsObject()
			: nullptr;
		if (!SourceObject.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s candidate source %d malformed"), *Label, Index));
			return false;
		}

		const FWBCardActivationCandidate& Candidate = Candidates[Index];
		int32 ExpectedInteger = -1;
		if (TryReadIntegerField(SourceObject, TEXT("player_id"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s candidate %d player"), *Label, Index), Candidate.PlayerId, ExpectedInteger);
		}
		if (TryReadIntegerField(SourceObject, TEXT("source_unit_id"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s candidate %d source"), *Label, Index), Candidate.SourceUnitId, ExpectedInteger);
		}
		if (TryReadIntegerField(SourceObject, TEXT("target_unit_id"), ExpectedInteger))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s candidate %d target"), *Label, Index), Candidate.Target.TargetUnitId, ExpectedInteger);
		}

		FString ExpectedString;
		if (SourceObject->TryGetStringField(TEXT("source_card_id"), ExpectedString))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s candidate %d card"), *Label, Index), Candidate.SourceCardId, ExpectedString);
		}
		if (SourceObject->TryGetStringField(TEXT("source_effect_id"), ExpectedString))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s candidate %d effect"), *Label, Index), Candidate.SourceEffectId, ExpectedString);
		}
	}

	return Candidates.Num() == CandidateSources->Num();
}

bool ExpectCandidateGenerationFromFixture(
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

	ExpectCandidateSourcesFromFixture(Test, FixtureName, Expected, Result.Candidates);
	return true;
}

bool ApplyExpectedCandidateFromFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationCandidateGenerationResult& Result,
	FWBGameStateData& State)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	int32 ApplyIndex = -1;
	if (!TryReadIntegerField(Expected, TEXT("apply_candidate_index"), ApplyIndex))
	{
		return true;
	}

	if (!Result.Candidates.IsValidIndex(ApplyIndex))
	{
		Test.AddError(FString::Printf(TEXT("%s missing apply candidate %d"), *FixtureName, ApplyIndex));
		return false;
	}

	const FWBCardActivationCommandResult CommandResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Result.Candidates[ApplyIndex].Command);
	Test.TestTrue(*FString::Printf(TEXT("%s candidate apply ok"), *FixtureName), CommandResult.bOk);

	FString ExpectReason;
	if (GetArrayField(Expected, TEXT("expected_trace_order")) != nullptr)
	{
		Test.TestTrue(
			*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason),
			ExpectTraceOrder(Fixture, CommandResult.TraceEvents, ExpectReason));
	}
	if (GetArrayField(Expected, TEXT("final_units")) != nullptr)
	{
		Test.TestTrue(
			*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason),
			ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	}

	return CommandResult.bOk;
}

FString SerializeRuntimeResultForCardActivation(const FWBCardActivationCommandResult& Result, const FWBGameStateData& State)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("card_activation_candidate_test"));
	Envelope.SelectedActionId = TEXT("card_activation_candidate_test");
	Envelope.ApplyResult.bOk = Result.bOk;
	Envelope.ApplyResult.Reason = Result.Reason;
	Envelope.ApplyResult.TraceEvents = Result.TraceEvents;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson);
	return SerializedJson;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateEffectFamiliesApplyTest, "Wandbound.Core.CardActivationCandidateGeneration.EffectFamiliesGenerateAndApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateEffectFamiliesApplyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData DamageState = MakeCandidateState();
	const FWBCardActivationCandidateGenerationResult DamageResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(DamageState, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_damage_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateDamagePayload(2) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Damage generation ok"), DamageResult.bOk);
	TestEqual(TEXT("Damage candidate count"), DamageResult.Candidates.Num(), 1);
	TestTrue(TEXT("Damage apply ok"), WBEffectRunner::ApplyCardActivationCommand(DamageState, DamageResult.Candidates[0].Command).bOk);
	TestEqual(TEXT("Damage target hp"), DamageState.GetUnitById(2)->HP, 4);

	FWBGameStateData HealState = MakeCandidateState();
	HealState.GetMutableUnitById(2)->HP = 2;
	const FWBCardActivationCandidateGenerationResult HealResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(HealState, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_heal_card"), TEXT("heal_2"), TEXT("Heal 2 HP"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateHealPayload(2) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Heal generation ok"), HealResult.bOk);
	TestEqual(TEXT("Heal candidate count"), HealResult.Candidates.Num(), 1);
	TestTrue(TEXT("Heal apply ok"), WBEffectRunner::ApplyCardActivationCommand(HealState, HealResult.Candidates[0].Command).bOk);
	TestEqual(TEXT("Heal target hp"), HealState.GetUnitById(2)->HP, 4);

	FWBGameStateData StatusState = MakeCandidateState();
	const FWBCardActivationCandidateGenerationResult StatusResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(StatusState, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_status_card"), TEXT("burn_2"), TEXT("Apply Burn"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateStatusPayload(FName(TEXT("Burn")), 2) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Status generation ok"), StatusResult.bOk);
	TestEqual(TEXT("Status candidate count"), StatusResult.Candidates.Num(), 1);
	TestTrue(TEXT("Status apply ok"), WBEffectRunner::ApplyCardActivationCommand(StatusState, StatusResult.Candidates[0].Command).bOk);
	TestTrue(TEXT("Status target has Burn"), StatusState.GetUnitById(2)->HasStatus(FName(TEXT("Burn"))));
	TestEqual(TEXT("Status target duration"), StatusState.GetUnitById(2)->GetStatusTurnsRemaining(FName(TEXT("Burn"))), 2);

	FWBGameStateData ArmorState = MakeCandidateState();
	const FWBCardActivationCandidateGenerationResult ArmorResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(ArmorState, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_armor_card"), TEXT("armor_2"), TEXT("Restore Armor"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateArmorPayload(2) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Armor generation ok"), ArmorResult.bOk);
	TestEqual(TEXT("Armor candidate count"), ArmorResult.Candidates.Num(), 1);
	TestTrue(TEXT("Armor apply ok"), WBEffectRunner::ApplyCardActivationCommand(ArmorState, ArmorResult.Candidates[0].Command).bOk);
	TestEqual(TEXT("Armor target current armor"), ArmorState.GetUnitById(2)->GetCurrentArmor(), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateOrderingAndIdTest, "Wandbound.Core.CardActivationCandidateGeneration.OrderingAndIdStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateOrderingAndIdTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCandidateState();

	FWBCardDefinition FirstDefinition;
	FirstDefinition.CardId = TEXT("fixture_order_card_a");
	FirstDefinition.PublicName = TEXT("Fixture Order A");
	FirstDefinition.Kind = EWBCardDefinitionKind::Fixture;
	FirstDefinition.ActivatedEffects.Add(MakeCandidateDefinition(TEXT("unused"), TEXT("deal_1"), TEXT("Deal 1 damage"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateDamagePayload(1) }).ActivatedEffects[0]);
	FirstDefinition.ActivatedEffects.Add(MakeCandidateDefinition(TEXT("unused"), TEXT("heal_1"), TEXT("Heal 1 HP"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateHealPayload(1) }).ActivatedEffects[0]);

	const FWBCardDefinition SecondDefinition =
		MakeCandidateDefinition(TEXT("fixture_order_card_b"), TEXT("burn_1"), TEXT("Apply Burn"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateStatusPayload(FName(TEXT("Burn")), 1) });

	const TArray<FWBCardActivationCandidateSource> Sources = {
		MakeCandidateSource(FirstDefinition, { MakeUnitTarget(2), MakeUnitTarget(3) }),
		MakeCandidateSource(SecondDefinition, { MakeUnitTarget(2) }, 4)
	};

	const FWBCardActivationCandidateGenerationResult FirstResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	const FWBCardActivationCandidateGenerationResult SecondResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);

	TestTrue(TEXT("First generation ok"), FirstResult.bOk);
	TestTrue(TEXT("Second generation ok"), SecondResult.bOk);
	TestEqual(TEXT("Candidate count"), FirstResult.Candidates.Num(), 5);
	TestTrue(TEXT("IDs stable"), CandidateIds(FirstResult) == CandidateIds(SecondResult));
	TestNotEqual(TEXT("Different targets have different ids"), FirstResult.Candidates[0].ActivationCandidateId, FirstResult.Candidates[1].ActivationCandidateId);

	const TArray<FString> ExpectedIds = {
		TEXT("activate:p0:u1:cfixture_order_card_a:edeal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_order_card_a:edeal_1:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_order_card_a:eheal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_order_card_a:eheal_1:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u4:cfixture_order_card_b:eburn_1:t2:x-1:y-1:wa-1,-1:wb-1,-1")
	};
	CompareStringArrays(*this, TEXT("Candidate IDs"), CandidateIds(FirstResult), ExpectedIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateFilteringTest, "Wandbound.Core.CardActivationCandidateGeneration.DynamicFilteringAndMalformedFailures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateFilteringTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinition DamageDefinition =
		MakeCandidateDefinition(TEXT("fixture_damage_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateDamagePayload(2) });

	FWBGameStateData StunnedState = MakeCandidateState();
	StunnedState.GetMutableUnitById(1)->AddStatus(FName(TEXT("Stunned")), 1);
	const FWBCardActivationCandidateGenerationResult StunnedResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(StunnedState, { MakeCandidateSource(DamageDefinition, { MakeUnitTarget(2) }) });
	TestTrue(TEXT("Stunned source generation ok"), StunnedResult.bOk);
	TestEqual(TEXT("Stunned source excluded"), StunnedResult.Candidates.Num(), 0);

	FWBGameStateData FrozenState = MakeCandidateState();
	FrozenState.GetMutableUnitById(1)->AddStatus(FName(TEXT("Frozen")), 1);
	const FWBCardActivationCandidateGenerationResult FrozenResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(FrozenState, { MakeCandidateSource(DamageDefinition, { MakeUnitTarget(2) }) });
	TestTrue(TEXT("Frozen source generation ok"), FrozenResult.bOk);
	TestEqual(TEXT("Frozen source excluded"), FrozenResult.Candidates.Num(), 0);

	FWBGameStateData NoAttackState = MakeCandidateState();
	NoAttackState.GetMutableUnitById(1)->AddStatus(FName(TEXT("no_attack")), 1);
	const FWBCardActivationCandidateGenerationResult NoAttackResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(NoAttackState, { MakeCandidateSource(DamageDefinition, { MakeUnitTarget(2) }) });
	TestTrue(TEXT("NoAttack source generation ok"), NoAttackResult.bOk);
	TestEqual(TEXT("NoAttack source does not block activation candidates"), NoAttackResult.Candidates.Num(), 1);

	FWBGameStateData RemovedSourceState = MakeCandidateState();
	RemovedSourceState.GetMutableUnitById(1)->MarkUnitDefeated();
	RemovedSourceState.GetMutableUnitById(1)->RemoveUnitFromBoard();
	const FWBCardActivationCandidateGenerationResult RemovedSourceResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(RemovedSourceState, { MakeCandidateSource(DamageDefinition, { MakeUnitTarget(2) }) });
	TestTrue(TEXT("Removed source generation ok"), RemovedSourceResult.bOk);
	TestEqual(TEXT("Removed source excluded"), RemovedSourceResult.Candidates.Num(), 0);

	FWBGameStateData RemovedTargetState = MakeCandidateState();
	RemovedTargetState.GetMutableUnitById(2)->MarkUnitDefeated();
	RemovedTargetState.GetMutableUnitById(2)->RemoveUnitFromBoard();
	const FWBCardActivationCandidateGenerationResult RemovedTargetResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(RemovedTargetState, { MakeCandidateSource(DamageDefinition, { MakeUnitTarget(2), MakeUnitTarget(3) }) });
	TestTrue(TEXT("Removed target generation ok"), RemovedTargetResult.bOk);
	TestEqual(TEXT("Only remaining target candidate"), RemovedTargetResult.Candidates.Num(), 1);
	TestEqual(TEXT("Remaining target id"), RemovedTargetResult.Candidates[0].Target.TargetUnitId, 3);

	FWBGameStateData MissingTargetState = MakeCandidateState();
	const FWBCardActivationCandidateGenerationResult MissingTargetResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(MissingTargetState, { MakeCandidateSource(DamageDefinition, { FWBEffectTargetRef() }) });
	TestTrue(TEXT("Missing unit target generation ok"), MissingTargetResult.bOk);
	TestEqual(TEXT("Missing target skipped"), MissingTargetResult.Candidates.Num(), 0);

	FWBCardDefinition MalformedDefinition = DamageDefinition;
	MalformedDefinition.CardId.Reset();
	const FWBCardActivationCandidateGenerationResult MalformedResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeCandidateState(), { MakeCandidateSource(MalformedDefinition, { MakeUnitTarget(2) }) });
	TestFalse(TEXT("Malformed card fails"), MalformedResult.bOk);
	TestEqual(TEXT("Malformed card reason"), MalformedResult.Reason, FString(TEXT("missing_card_id")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidatePublicLabelTest, "Wandbound.Core.CardActivationCandidateGeneration.PublicLabelPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidatePublicLabelTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCandidateState();
	const FWBCardActivationCandidateGenerationResult LabeledResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_label_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateDamagePayload(2) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Labeled generation ok"), LabeledResult.bOk);
	TestEqual(TEXT("Public label"), LabeledResult.Candidates[0].PublicLabel, FString(TEXT("Deal 2 damage")));
	TestFalse(TEXT("Public label excludes raw operation"), LabeledResult.Candidates[0].PublicLabel.Contains(TEXT("damage_effect")));
	TestFalse(TEXT("Public label excludes card id"), LabeledResult.Candidates[0].PublicLabel.Contains(TEXT("fixture_label_card")));

	FWBCardDefinition UnlabeledDefinition =
		MakeCandidateDefinition(TEXT("fixture_unlabeled_card"), TEXT("heal_1"), FString(), EWBCardEffectTargetRequirement::Unit, { MakeCandidateHealPayload(1) });
	const FWBCardActivationCandidateGenerationResult UnlabeledResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { MakeCandidateSource(UnlabeledDefinition, { MakeUnitTarget(2) }) });
	TestTrue(TEXT("Unlabeled generation ok"), UnlabeledResult.bOk);
	TestEqual(TEXT("Fallback label"), UnlabeledResult.Candidates[0].PublicLabel, FString(TEXT("Activate")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateNoMutationExplicitApplyTest, "Wandbound.Core.CardActivationCandidateGeneration.NoMutationAndExplicitApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateNoMutationExplicitApplyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCandidateState();
	const int32 InitialHP = State.GetUnitById(2)->HP;
	const int32 InitialArmor = State.GetUnitById(2)->GetCurrentArmor();
	const int32 InitialStatuses = State.GetUnitById(2)->Statuses.Num();

	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_no_mutation_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateDamagePayload(2) }),
				{ MakeUnitTarget(2) })
		});

	TestTrue(TEXT("Generation ok"), Result.bOk);
	TestEqual(TEXT("Generation candidate count"), Result.Candidates.Num(), 1);
	TestEqual(TEXT("HP unchanged after generation"), State.GetUnitById(2)->HP, InitialHP);
	TestEqual(TEXT("Armor unchanged after generation"), State.GetUnitById(2)->GetCurrentArmor(), InitialArmor);
	TestEqual(TEXT("Statuses unchanged after generation"), State.GetUnitById(2)->Statuses.Num(), InitialStatuses);

	const FWBCardActivationCommandResult ApplyResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Result.Candidates[0].Command);
	TestTrue(TEXT("Explicit apply succeeds"), ApplyResult.bOk);
	TestEqual(TEXT("HP changed only after explicit apply"), State.GetUnitById(2)->HP, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateSourceGuardTest, "Wandbound.Core.CardActivationCandidateGeneration.SourceGuardsNoLegalGenerationOrEffectRunner", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationCandidateGenerator.cpp"));
	FString Source;
	TestTrue(TEXT("Candidate generator source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));
	TestFalse(TEXT("No GenerateLegalActions dependency"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No WBEffectRunner dependency"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No ApplyCardActivationCommand call"), Source.Contains(TEXT("ApplyCardActivationCommand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateLegalGenerationAndCodecTest, "Wandbound.Core.CardActivationCandidateGeneration.LegalGenerationAndCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateLegalGenerationAndCodecTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCandidateState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeCandidateSource(
				MakeCandidateDefinition(TEXT("fixture_generation_guard_card"), TEXT("heal_1"), TEXT("Heal 1 HP"), EWBCardEffectTargetRequirement::Unit, { MakeCandidateHealPayload(1) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Candidate exists"), CandidateResult.Candidates.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("Legal action ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("Legal action id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeCandidateMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeCandidateAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeCandidatePlayerAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeCandidatePlayerAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeCandidatePlayerAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no activation candidate ids"), ActionCodecSource.Contains(TEXT("activate:")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateHiddenMetadataExclusionTest, "Wandbound.Core.CardActivationCandidateGeneration.HiddenMetadataExcludedFromTraceAndRuntimeSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateHiddenMetadataExclusionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCandidateState();
	State.Players[0].Deck.Add(TEXT("secret_candidate_deck_token"));
	State.Players[0].Hand.Add(TEXT("secret_candidate_hand_token"));
	State.Players[0].Discard.Add(TEXT("secret_candidate_discard_token"));

	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeCandidateSource(
				MakeCandidateDefinition(
					TEXT("secret_candidate_source_card_token"),
					TEXT("secret_candidate_source_effect_token"),
					TEXT("Deal 2 damage"),
					EWBCardEffectTargetRequirement::Unit,
					{ MakeCandidateDamagePayload(2) }),
				{ MakeUnitTarget(2) })
		});
	TestTrue(TEXT("Candidate generation ok"), Result.bOk);
	TestEqual(TEXT("Candidate count"), Result.Candidates.Num(), 1);
	TestTrue(TEXT("Candidate metadata contains secret card"), Result.Candidates[0].SourceCardId.Contains(TEXT("secret_candidate_source_card_token")));
	TestTrue(TEXT("Candidate metadata contains secret effect"), Result.Candidates[0].SourceEffectId.Contains(TEXT("secret_candidate_source_effect_token")));

	FWBCardActivationCommand Command = Result.Candidates[0].Command;
	Command.DebugActivationId = TEXT("secret_candidate_debug_token");
	const FWBCardActivationCommandResult ApplyResult = WBEffectRunner::ApplyCardActivationCommand(State, Command);
	TestTrue(TEXT("Apply succeeds"), ApplyResult.bOk);

	const FString SerializedTraceEvents = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	const FString SerializedRuntimeJson = SerializeRuntimeResultForCardActivation(ApplyResult, State);
	const TArray<FString> ForbiddenSubstrings = {
		TEXT("secret_candidate_source_card_token"),
		TEXT("secret_candidate_source_effect_token"),
		TEXT("secret_candidate_debug_token"),
		TEXT("secret_candidate_deck_token"),
		TEXT("secret_candidate_hand_token"),
		TEXT("secret_candidate_discard_token"),
		TEXT("\"deck\""),
		TEXT("\"hand\""),
		TEXT("\"discard\"")
	};

	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Forbidden), SerializedTraceEvents.Contains(Forbidden));
		TestFalse(*FString::Printf(TEXT("Runtime excludes %s"), *Forbidden), SerializedRuntimeJson.Contains(Forbidden));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationCandidateFixtureScenariosTest, "Wandbound.Core.CardActivationCandidateGeneration.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationCandidateFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_candidates_damage_effect.json"),
		TEXT("card_activation_candidates_heal_effect.json"),
		TEXT("card_activation_candidates_status_effect.json"),
		TEXT("card_activation_candidates_armor_effect.json"),
		TEXT("card_activation_candidates_mixed_effects_ordered.json"),
		TEXT("card_activation_candidates_stunned_source_excluded.json"),
		TEXT("card_activation_candidates_removed_source_excluded_or_fails.json"),
		TEXT("card_activation_candidates_removed_target_excluded.json"),
		TEXT("card_activation_candidates_no_targets_returns_empty.json"),
		TEXT("card_activation_candidates_candidate_id_stability.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadCandidateFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
		if (!StateReason.IsEmpty())
		{
			return false;
		}

		const int32 InitialHP = State.GetUnitById(2) != nullptr ? State.GetUnitById(2)->HP : -1;
		const int32 InitialArmor = State.GetUnitById(2) != nullptr ? State.GetUnitById(2)->GetCurrentArmor() : -1;
		const int32 InitialStatusCount = State.GetUnitById(2) != nullptr ? State.GetUnitById(2)->Statuses.Num() : -1;

		TArray<FWBCardActivationCandidateSource> Sources;
		FString ParseReason;
		TestTrue(
			*FString::Printf(TEXT("%s parses sources: %s"), *FixtureName, *ParseReason),
			ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

		const FWBCardActivationCandidateGenerationResult Result =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		ExpectCandidateGenerationFromFixture(*this, FixtureName, Fixture, Result);

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationCandidates);
		TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s operation reason"), *FixtureName), OperationResult.Reason, Result.Reason);

		const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
		bool bNoStateMutation = false;
		if (Expected.IsValid() && Expected->TryGetBoolField(TEXT("no_state_mutation"), bNoStateMutation) && bNoStateMutation)
		{
			if (const FWBUnitState* Unit2 = State.GetUnitById(2))
			{
				TestEqual(*FString::Printf(TEXT("%s unit 2 HP unchanged"), *FixtureName), Unit2->HP, InitialHP);
				TestEqual(*FString::Printf(TEXT("%s unit 2 armor unchanged"), *FixtureName), Unit2->GetCurrentArmor(), InitialArmor);
				TestEqual(*FString::Printf(TEXT("%s unit 2 statuses unchanged"), *FixtureName), Unit2->Statuses.Num(), InitialStatusCount);
			}
		}

		ApplyExpectedCandidateFromFixture(*this, FixtureName, Fixture, Result, State);

		bool bStabilityExpected = false;
		if (Expected.IsValid() && Expected->TryGetBoolField(TEXT("stability_expected"), bStabilityExpected) && bStabilityExpected)
		{
			FWBGameStateData SecondState;
			FString SecondStateReason;
			TestTrue(*FString::Printf(TEXT("%s second state builds: %s"), *FixtureName, *SecondStateReason), BuildGameStateFromFixture(Fixture, SecondState, SecondStateReason));
			const FWBCardActivationCandidateGenerationResult SecondResult =
				WBCardActivationCandidateGenerator::GenerateCandidates(SecondState, Sources);
			TestTrue(*FString::Printf(TEXT("%s second generation ok"), *FixtureName), SecondResult.bOk);
			TestTrue(*FString::Printf(TEXT("%s ids stable"), *FixtureName), CandidateIds(Result) == CandidateIds(SecondResult));
			if (SecondResult.Candidates.Num() >= 2)
			{
				TestNotEqual(
					*FString::Printf(TEXT("%s target ids distinct"), *FixtureName),
					SecondResult.Candidates[0].ActivationCandidateId,
					SecondResult.Candidates[1].ActivationCandidateId);
			}
		}
	}

	return true;
}

#endif
