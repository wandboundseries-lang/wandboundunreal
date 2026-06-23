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
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeActivationActionPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeActivationActionUnit(
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

FWBGameStateData MakeActivationActionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeActivationActionPlayer(0, 2));
	State.Players.Add(MakeActivationActionPlayer(1, 0));
	State.AddUnitForTest(MakeActivationActionUnit(1, 0, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeActivationActionUnit(2, 1, FWBTile(5, 4), 5, 6, 1, 3));
	State.AddUnitForTest(MakeActivationActionUnit(3, 1, FWBTile(6, 4), 3, 5));
	State.AddUnitForTest(MakeActivationActionUnit(4, 0, FWBTile(4, 5), 5, 5));
	return State;
}

FWBGenericEffectPayload MakeActivationActionArmorPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = EWBArmorEffectOp::AddCurrentArmor;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeActivationActionStatusPayload(const FName StatusId, const int32 Duration)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeActivationActionDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = false;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBGenericEffectPayload MakeActivationActionHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBCardDefinition MakeActivationActionDefinition(
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

FWBEffectTargetRef MakeActivationActionUnitTarget(const int32 UnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardActivationCandidateSource MakeActivationActionSource(
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

FWBCardActivationCandidateGenerationResult GenerateCandidatesForDefinition(
	FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = 1)
{
	return WBCardActivationCandidateGenerator::GenerateCandidates(
		State,
		{ MakeActivationActionSource(Definition, Targets, SourceUnitId) });
}

FWBCardActivationLegalActionGenerationResult GenerateLegalActionsForDefinition(
	FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = 1)
{
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		GenerateCandidatesForDefinition(State, Definition, Targets, SourceUnitId);
	if (!CandidateResult.bOk)
	{
		FWBCardActivationLegalActionGenerationResult Result;
		Result.Reason = CandidateResult.Reason;
		return Result;
	}

	return WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
}

FWBAction MakeActivationActionMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeActivationActionAttackAction()
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

FWBAction MakeActivationActionPlayerAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
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

TArray<FString> ActivationActionLabels(const FWBCardActivationLegalActionGenerationResult& Result)
{
	TArray<FString> Labels;
	for (const FWBCardActivationLegalAction& Action : Result.ActionSet.Actions)
	{
		Labels.Add(Action.PublicLabel);
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

FString ActivationActionFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

TSharedPtr<FJsonObject> ParseActivationActionJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

bool LoadActivationActionFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ActivationActionFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseActivationActionJsonObject(Json);
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

bool ExpectActivationLegalActionsFromFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBCardActivationLegalActionGenerationResult& Result)
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

	FString ReasonContains;
	if (!Result.bOk && Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains))
	{
		Test.TestTrue(*FString::Printf(TEXT("%s reason contains"), *FixtureName), Result.Reason.Contains(ReasonContains));
	}

	int32 ExpectedActionCount = -1;
	if (TryReadIntegerField(Expected, TEXT("activation_action_count"), ExpectedActionCount))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s activation action count"), *FixtureName), Result.ActionSet.Actions.Num(), ExpectedActionCount);
	}

	TArray<FString> ExpectedIds;
	if (ReadStringArrayField(Expected, TEXT("activation_action_ids"), ExpectedIds))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" activation action ids"), ActivationActionIds(Result), ExpectedIds);
	}

	TArray<FString> ExpectedLabels;
	if (ReadStringArrayField(Expected, TEXT("public_labels"), ExpectedLabels))
	{
		CompareStringArrays(Test, FixtureName + TEXT(" public labels"), ActivationActionLabels(Result), ExpectedLabels);
	}

	TArray<FString> ForbiddenSubstrings;
	if (ReadStringArrayField(Expected, TEXT("forbidden_substrings"), ForbiddenSubstrings))
	{
		for (const FString& Label : ActivationActionLabels(Result))
		{
			for (const FString& Forbidden : ForbiddenSubstrings)
			{
				Test.TestFalse(*FString::Printf(TEXT("%s label excludes %s"), *FixtureName, *Forbidden), Label.Contains(Forbidden, ESearchCase::IgnoreCase));
			}
		}
	}

	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionEffectFamiliesTest, "Wandbound.Core.CardActivationLegalActionGeneration.EffectFamilies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionEffectFamiliesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData DamageState = MakeActivationActionState();
	const FWBCardActivationLegalActionGenerationResult DamageResult =
		GenerateLegalActionsForDefinition(
			DamageState,
			MakeActivationActionDefinition(TEXT("fixture_damage_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionDamagePayload(2) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Damage legal action generation ok"), DamageResult.bOk);
	TestEqual(TEXT("Damage action count"), DamageResult.ActionSet.Actions.Num(), 1);
	TestEqual(TEXT("Damage action id"), DamageResult.ActionSet.Actions[0].ActivationActionId, FString(TEXT("activate:p0:u1:cfixture_damage_card:edeal_2:t2:x-1:y-1:wa-1,-1:wb-1,-1")));
	TestEqual(TEXT("Damage public label"), DamageResult.ActionSet.Actions[0].PublicLabel, FString(TEXT("Deal 2 damage")));

	FWBGameStateData HealState = MakeActivationActionState();
	const FWBCardActivationLegalActionGenerationResult HealResult =
		GenerateLegalActionsForDefinition(
			HealState,
			MakeActivationActionDefinition(TEXT("fixture_heal_card"), TEXT("heal_2"), TEXT("Heal 2 HP"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionHealPayload(2) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Heal legal action generation ok"), HealResult.bOk);
	TestEqual(TEXT("Heal public label"), HealResult.ActionSet.Actions[0].PublicLabel, FString(TEXT("Heal 2 HP")));

	FWBGameStateData StatusState = MakeActivationActionState();
	const FWBCardActivationLegalActionGenerationResult StatusResult =
		GenerateLegalActionsForDefinition(
			StatusState,
			MakeActivationActionDefinition(TEXT("fixture_status_card"), TEXT("burn_2"), TEXT("Apply Burn"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionStatusPayload(FName(TEXT("Burn")), 2) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Status legal action generation ok"), StatusResult.bOk);
	TestEqual(TEXT("Status public label"), StatusResult.ActionSet.Actions[0].PublicLabel, FString(TEXT("Apply Burn")));

	FWBGameStateData ArmorState = MakeActivationActionState();
	const FWBCardActivationLegalActionGenerationResult ArmorResult =
		GenerateLegalActionsForDefinition(
			ArmorState,
			MakeActivationActionDefinition(TEXT("fixture_armor_card"), TEXT("armor_2"), TEXT("Restore Armor"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionArmorPayload(2) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Armor legal action generation ok"), ArmorResult.bOk);
	TestEqual(TEXT("Armor public label"), ArmorResult.ActionSet.Actions[0].PublicLabel, FString(TEXT("Restore Armor")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionOrderingAndIdTest, "Wandbound.Core.CardActivationLegalActionGeneration.OrderingAndIdStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionOrderingAndIdTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationActionState();

	FWBCardDefinition FirstDefinition;
	FirstDefinition.CardId = TEXT("fixture_order_card_a");
	FirstDefinition.PublicName = TEXT("Fixture Order A");
	FirstDefinition.Kind = EWBCardDefinitionKind::Fixture;
	FirstDefinition.ActivatedEffects.Add(MakeActivationActionDefinition(TEXT("unused"), TEXT("deal_1"), TEXT("Deal 1 damage"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionDamagePayload(1) }).ActivatedEffects[0]);
	FirstDefinition.ActivatedEffects.Add(MakeActivationActionDefinition(TEXT("unused"), TEXT("heal_1"), TEXT("Heal 1 HP"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionHealPayload(1) }).ActivatedEffects[0]);

	const FWBCardDefinition SecondDefinition =
		MakeActivationActionDefinition(TEXT("fixture_order_card_b"), TEXT("burn_1"), TEXT("Apply Burn"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionStatusPayload(FName(TEXT("Burn")), 1) });

	const TArray<FWBCardActivationCandidateSource> Sources = {
		MakeActivationActionSource(FirstDefinition, { MakeActivationActionUnitTarget(2), MakeActivationActionUnitTarget(3) }),
		MakeActivationActionSource(SecondDefinition, { MakeActivationActionUnitTarget(2) }, 4)
	};

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	const FWBCardActivationLegalActionGenerationResult FirstResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	const FWBCardActivationLegalActionGenerationResult SecondResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);

	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestTrue(TEXT("First legal action generation ok"), FirstResult.bOk);
	TestTrue(TEXT("Second legal action generation ok"), SecondResult.bOk);
	TestEqual(TEXT("Action count"), FirstResult.ActionSet.Actions.Num(), 5);
	TestTrue(TEXT("IDs stable"), ActivationActionIds(FirstResult) == ActivationActionIds(SecondResult));
	TestNotEqual(TEXT("Different targets have different ids"), FirstResult.ActionSet.Actions[0].ActivationActionId, FirstResult.ActionSet.Actions[1].ActivationActionId);

	const TArray<FString> ExpectedIds = {
		TEXT("activate:p0:u1:cfixture_order_card_a:edeal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_order_card_a:edeal_1:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_order_card_a:eheal_1:t2:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u1:cfixture_order_card_a:eheal_1:t3:x-1:y-1:wa-1,-1:wb-1,-1"),
		TEXT("activate:p0:u4:cfixture_order_card_b:eburn_1:t2:x-1:y-1:wa-1,-1:wb-1,-1")
	};
	CompareStringArrays(*this, TEXT("Activation action ids"), ActivationActionIds(FirstResult), ExpectedIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionPublicLabelAndMalformedTest, "Wandbound.Core.CardActivationLegalActionGeneration.PublicLabelAndMalformedPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionPublicLabelAndMalformedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationActionState();
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		GenerateCandidatesForDefinition(
			State,
			MakeActivationActionDefinition(TEXT("fixture_label_card"), TEXT("heal_1"), FString(), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionHealPayload(1) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Fallback candidate generation ok"), CandidateResult.bOk);

	const FWBCardActivationLegalActionGenerationResult FallbackResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	TestTrue(TEXT("Fallback legal action generation ok"), FallbackResult.bOk);
	TestEqual(TEXT("Fallback label"), FallbackResult.ActionSet.Actions[0].PublicLabel, FString(TEXT("Activate")));

	FWBCardActivationCandidate NonActivateCandidate = CandidateResult.Candidates[0];
	NonActivateCandidate.ActivationCandidateId = TEXT("fixture_custom_activation_id");
	const FWBCardActivationLegalActionGenerationResult NonActivateIdResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates({ NonActivateCandidate });
	TestTrue(TEXT("Non-activate candidate id accepted"), NonActivateIdResult.bOk);
	TestEqual(TEXT("Non-activate action id preserved"), NonActivateIdResult.ActionSet.Actions[0].ActivationActionId, FString(TEXT("fixture_custom_activation_id")));

	FWBCardActivationCandidate EmptyIdCandidate = CandidateResult.Candidates[0];
	EmptyIdCandidate.ActivationCandidateId.Reset();
	const FWBCardActivationLegalActionGenerationResult EmptyIdResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates({ EmptyIdCandidate });
	TestFalse(TEXT("Empty candidate id fails"), EmptyIdResult.bOk);
	TestTrue(TEXT("Empty candidate id reason"), EmptyIdResult.Reason.Contains(TEXT("missing_activation_candidate_id")));

	FWBCardActivationCandidate MissingPayloadCandidate = CandidateResult.Candidates[0];
	MissingPayloadCandidate.Command.EffectRequest.Payloads.Reset();
	const FWBCardActivationLegalActionGenerationResult MissingPayloadResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates({ MissingPayloadCandidate });
	TestFalse(TEXT("Missing payloads fail"), MissingPayloadResult.bOk);
	TestTrue(TEXT("Missing payload reason"), MissingPayloadResult.Reason.Contains(TEXT("missing_command_payloads")));

	FWBCardActivationCandidate UnsafeLabelCandidate = CandidateResult.Candidates[0];
	UnsafeLabelCandidate.PublicLabel = TEXT("damage_effect internal label");
	const FWBCardActivationLegalActionGenerationResult UnsafeLabelResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates({ UnsafeLabelCandidate });
	TestFalse(TEXT("Unsafe public label fails"), UnsafeLabelResult.bOk);
	TestTrue(TEXT("Unsafe label reason"), UnsafeLabelResult.Reason.Contains(TEXT("unsafe_public_label")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionPresentationTest, "Wandbound.Core.CardActivationLegalActionGeneration.PresentationSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionPresentationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationActionState();
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateLegalActionsForDefinition(
			State,
			MakeActivationActionDefinition(TEXT("fixture_presentation_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionDamagePayload(2) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Presentation source generation ok"), Result.bOk);

	const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
		WBCardActivationLegalActionPresentation::BuildSnapshot(Result.ActionSet);
	TestEqual(TEXT("Snapshot entry count"), Snapshot.Entries.Num(), 1);
	TestEqual(TEXT("Snapshot id"), Snapshot.Entries[0].ActivationActionId, Result.ActionSet.Actions[0].ActivationActionId);
	TestEqual(TEXT("Snapshot label"), Snapshot.Entries[0].PublicLabel, FString(TEXT("Deal 2 damage")));
	TestEqual(TEXT("Snapshot player"), Snapshot.Entries[0].PlayerId, 0);
	TestEqual(TEXT("Snapshot source"), Snapshot.Entries[0].SourceUnitId, 1);
	TestEqual(TEXT("Snapshot target"), Snapshot.Entries[0].TargetUnitId, 2);
	TestEqual(TEXT("Snapshot source card metadata"), Snapshot.Entries[0].SourceCardId, FString(TEXT("fixture_presentation_card")));
	TestEqual(TEXT("Snapshot source effect metadata"), Snapshot.Entries[0].SourceEffectId, FString(TEXT("deal_2")));

	FWBCardActivationLegalActionPresentationEntry FoundEntry;
	TestTrue(TEXT("Lookup existing id succeeds"), WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(Snapshot, Result.ActionSet.Actions[0].ActivationActionId, FoundEntry));
	TestEqual(TEXT("Lookup found id"), FoundEntry.ActivationActionId, Result.ActionSet.Actions[0].ActivationActionId);
	TestFalse(TEXT("Lookup missing id fails"), WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(Snapshot, TEXT("missing_activation_id"), FoundEntry));

	FWBCardActivationLegalActionSet DuplicateSet = Result.ActionSet;
	DuplicateSet.Actions.Add(Result.ActionSet.Actions[0]);
	const FWBCardActivationLegalActionPresentationSnapshot DuplicateSnapshot =
		WBCardActivationLegalActionPresentation::BuildSnapshot(DuplicateSet);
	TestFalse(TEXT("Duplicate lookup fails"), WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(DuplicateSnapshot, Result.ActionSet.Actions[0].ActivationActionId, FoundEntry));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionSeparationAndCodecTest, "Wandbound.Core.CardActivationLegalActionGeneration.SeparateFromFWBActionAndCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionSeparationAndCodecTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationActionState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateLegalActionsForDefinition(
			State,
			MakeActivationActionDefinition(TEXT("fixture_separation_card"), TEXT("heal_1"), TEXT("Heal 1 HP"), EWBCardEffectTargetRequirement::Unit, { MakeActivationActionHealPayload(1) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Activation legal action generation ok"), Result.bOk);
	TestEqual(TEXT("Activation action count"), Result.ActionSet.Actions.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal action ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeActivationActionMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeActivationActionAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeActivationActionPlayerAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeActivationActionPlayerAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeActivationActionPlayerAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no activation ids"), ActionCodecSource.Contains(TEXT("activate:")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionSourceGuardTest, "Wandbound.Core.CardActivationLegalActionGeneration.SourceGuardsNoRulesEffectRunnerOrActionCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationLegalActionGenerator.cpp"));
	FString Source;
	TestTrue(TEXT("Legal action generator source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));
	TestFalse(TEXT("No WBRules dependency"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No GenerateLegalActions dependency"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("No WBEffectRunner dependency"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No WBActionCodec dependency"), Source.Contains(TEXT("WBActionCodec")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionExplicitApplyAndHiddenMetadataTest, "Wandbound.Core.CardActivationLegalActionGeneration.NoMutationExplicitApplyAndHiddenMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionExplicitApplyAndHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeActivationActionState();
	State.Players[0].Deck.Add(TEXT("secret_activation_legal_deck_token"));
	State.Players[0].Hand.Add(TEXT("secret_activation_legal_hand_token"));
	State.Players[0].Discard.Add(TEXT("secret_activation_legal_discard_token"));

	const int32 InitialHP = State.GetUnitById(2)->HP;
	const int32 InitialArmor = State.GetUnitById(2)->GetCurrentArmor();
	const int32 InitialStatuses = State.GetUnitById(2)->Statuses.Num();

	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateLegalActionsForDefinition(
			State,
			MakeActivationActionDefinition(
				TEXT("secret_activation_legal_source_card_token"),
				TEXT("secret_activation_legal_source_effect_token"),
				TEXT("Deal 2 damage"),
				EWBCardEffectTargetRequirement::Unit,
				{ MakeActivationActionDamagePayload(2) }),
			{ MakeActivationActionUnitTarget(2) });
	TestTrue(TEXT("Activation legal action generation ok"), Result.bOk);
	TestEqual(TEXT("Generated action count"), Result.ActionSet.Actions.Num(), 1);
	TestEqual(TEXT("HP unchanged after generation"), State.GetUnitById(2)->HP, InitialHP);
	TestEqual(TEXT("Armor unchanged after generation"), State.GetUnitById(2)->GetCurrentArmor(), InitialArmor);
	TestEqual(TEXT("Statuses unchanged after generation"), State.GetUnitById(2)->Statuses.Num(), InitialStatuses);

	FWBCardActivationCommand Command = Result.ActionSet.Actions[0].Command;
	Command.DebugActivationId = TEXT("secret_activation_legal_debug_token");
	const FWBCardActivationCommandResult ApplyResult = WBEffectRunner::ApplyCardActivationCommand(State, Command);
	TestTrue(TEXT("Explicit apply succeeds"), ApplyResult.bOk);
	TestEqual(TEXT("HP changed only after explicit apply"), State.GetUnitById(2)->HP, 4);

	const FString SerializedTraceEvents = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	const TArray<FString> ForbiddenSubstrings = {
		TEXT("secret_activation_legal_source_card_token"),
		TEXT("secret_activation_legal_source_effect_token"),
		TEXT("secret_activation_legal_debug_token"),
		TEXT("secret_activation_legal_deck_token"),
		TEXT("secret_activation_legal_hand_token"),
		TEXT("secret_activation_legal_discard_token"),
		TEXT("\"deck\""),
		TEXT("\"hand\""),
		TEXT("\"discard\"")
	};

	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Trace excludes %s"), *Forbidden), SerializedTraceEvents.Contains(Forbidden));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationLegalActionFixtureScenariosTest, "Wandbound.Core.CardActivationLegalActionGeneration.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationLegalActionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_legal_actions_damage_effect.json"),
		TEXT("card_activation_legal_actions_heal_effect.json"),
		TEXT("card_activation_legal_actions_status_effect.json"),
		TEXT("card_activation_legal_actions_armor_effect.json"),
		TEXT("card_activation_legal_actions_ordered.json"),
		TEXT("card_activation_legal_actions_public_labels.json"),
		TEXT("card_activation_legal_actions_malformed_candidate_fails.json"),
		TEXT("card_activation_legal_actions_separate_from_fwbaction.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadActivationActionFixture(FixtureName, Fixture));
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
		const TArray<FString> BaselineFWBActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

		TArray<FWBCardActivationCandidateSource> Sources;
		FString ParseReason;
		TestTrue(
			*FString::Printf(TEXT("%s parses sources: %s"), *FixtureName, *ParseReason),
			ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

		const FWBCardActivationCandidateGenerationResult CandidateResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		FWBCardActivationLegalActionGenerationResult Result;
		if (CandidateResult.bOk)
		{
			Result = WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
		}
		else
		{
			Result.bOk = false;
			Result.Reason = CandidateResult.Reason;
		}
		ExpectActivationLegalActionsFromFixture(*this, FixtureName, Fixture, Result);

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationLegalActions);
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

		bool bFWBActionUnchanged = false;
		if (Expected.IsValid()
			&& Expected->TryGetBoolField(TEXT("fwbaction_legal_action_count_unchanged"), bFWBActionUnchanged)
			&& bFWBActionUnchanged)
		{
			const TArray<FString> AfterFWBActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
			CompareStringArrays(*this, FixtureName + TEXT(" FWBAction ids unchanged"), AfterFWBActionIds, BaselineFWBActionIds);
			for (const FString& ActionId : AfterFWBActionIds)
			{
				TestFalse(*FString::Printf(TEXT("%s FWBAction id is not activation %s"), *FixtureName, *ActionId), ActionId.StartsWith(TEXT("activate:")));
			}
		}
	}

	return true;
}

#endif
