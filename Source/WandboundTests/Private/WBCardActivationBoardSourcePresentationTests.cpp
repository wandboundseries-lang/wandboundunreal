#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

constexpr int32 BoardPresentationSourceUnitId = 1;
constexpr int32 BoardPresentationTargetUnitId = 2;

FWBPlayerStateData MakeBoardPresentationPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeBoardPresentationUnit(
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

FWBGameStateData MakeBoardPresentationState(
	const FString& SourceCardId = TEXT("fixture_board_presentation_card"),
	const FString& TargetCardId = TEXT("fixture_board_presentation_target"),
	const int32 TargetHP = 5,
	const int32 TargetMaxHP = 6)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 15;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeBoardPresentationPlayer(0));
	State.Players.Add(MakeBoardPresentationPlayer(1));
	State.AddUnitForTest(MakeBoardPresentationUnit(BoardPresentationSourceUnitId, 0, SourceCardId, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeBoardPresentationUnit(BoardPresentationTargetUnitId, 1, TargetCardId, FWBTile(5, 4), TargetHP, TargetMaxHP));
	State.AddUnitForTest(MakeBoardPresentationUnit(3, 1, TEXT("fixture_board_presentation_target_b"), FWBTile(6, 4), 4, 6));
	State.AddUnitForTest(MakeBoardPresentationUnit(4, 0, TEXT("fixture_board_presentation_card_b"), FWBTile(4, 5), 5, 5));
	State.GetMutableUnitById(BoardPresentationTargetUnitId)->SetArmorForTest(1, 3);
	return State;
}

FWBCardActivationSourceGateDefinition MakeBoardPresentationGate()
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

FWBGenericEffectPayload MakeBoardPresentationDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeBoardPresentationHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeBoardPresentationArmorPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = EWBArmorEffectOp::AddCurrentArmor;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeBoardPresentationStatusPayload(const FName StatusId, const int32 Duration)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::StatusEffect;
	Payload.StatusEffect.Operation = EWBStatusEffectOp::ApplyStatus;
	Payload.StatusEffect.StatusId = StatusId;
	Payload.StatusEffect.Duration = Duration;
	Payload.StatusEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardEffectDefinition MakeBoardPresentationEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBGenericEffectPayload& Payload)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = MakeBoardPresentationGate();
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardDefinition MakeBoardPresentationDefinition(
	const FString& CardId,
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBGenericEffectPayload& Payload)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicLabel;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeBoardPresentationEffect(EffectId, PublicLabel, Payload));
	return Definition;
}

FWBEffectTargetRef MakeBoardPresentationTarget(const int32 TargetUnitId = BoardPresentationTargetUnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = TargetUnitId;
	return Target;
}

FWBCardActivationSourceGateContext MakeBoardPresentationContext(
	const FString& SourceCardId,
	const int32 SourceUnitId = BoardPresentationSourceUnitId)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.SourceCardId = SourceCardId;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationCandidateSource MakeBoardPresentationSource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = BoardPresentationSourceUnitId)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	Source.SourceGateContext = MakeBoardPresentationContext(Definition.CardId, SourceUnitId);
	return Source;
}

FWBCardActivationLegalActionGenerationResult GenerateBoardPresentationActions(
	const FWBGameStateData& State,
	const TArray<FWBCardActivationCandidateSource>& Sources)
{
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	if (!CandidateResult.bOk)
	{
		FWBCardActivationLegalActionGenerationResult Result;
		Result.Reason = CandidateResult.Reason;
		return Result;
	}

	return WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
}

FWBCardActivationLegalActionGenerationResult GenerateBoardPresentationActions(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets)
{
	return GenerateBoardPresentationActions(
		State,
		{ MakeBoardPresentationSource(Definition, Targets) });
}

FWBCardActivationLegalActionGenerationResult GenerateBoardPresentationActions(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition)
{
	return GenerateBoardPresentationActions(
		State,
		Definition,
		{ MakeBoardPresentationTarget() });
}

FWBCardActivationLegalActionPresentationSnapshot BuildBoardPresentationSnapshot(
	const FWBCardActivationLegalActionGenerationResult& Result,
	const FWBGameStateData& State)
{
	return WBCardActivationLegalActionPresentation::BuildSnapshot(
		Result.ActionSet,
		WBPublicBoardSummary::Build(State));
}

TArray<FString> PresentationIds(const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	TArray<FString> Ids;
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		Ids.Add(Entry.ActivationActionId);
	}

	return Ids;
}

TArray<FString> ActionIds(const FWBCardActivationLegalActionGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationLegalAction& Action : Result.ActionSet.Actions)
	{
		Ids.Add(Action.ActivationActionId);
	}

	return Ids;
}

FString PresentationPublicSurfaceText(const FWBCardActivationLegalActionPresentationSnapshot& Snapshot)
{
	FString Text;
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		Text += Entry.PublicLabel;
		Text += TEXT("|");
		Text += Entry.SourcePublicCardId;
		Text += TEXT("|");
		Text += Entry.TargetPublicCardId;
		Text += TEXT("|");
	}

	return Text;
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

FWBAction MakeBoardPresentationCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = BoardPresentationSourceUnitId;
	Action.TargetUnitId = BoardPresentationTargetUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FString BoardPresentationFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadBoardPresentationFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *BoardPresentationFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, OutFixture);
	return OutFixture.IsValid();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourcePresentationEffectFamiliesTest, "Wandbound.Core.CardActivationBoardSourcePresentation.EffectFamilies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourcePresentationEffectFamiliesTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		FString CardId;
		FString EffectId;
		FString PublicLabel;
		FWBGenericEffectPayload Payload;
	};

	const TArray<FCase> Cases = {
		{ TEXT("fixture_board_presentation_damage_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), MakeBoardPresentationDamagePayload(2) },
		{ TEXT("fixture_board_presentation_status_card"), TEXT("burn_2"), TEXT("Apply Burn"), MakeBoardPresentationStatusPayload(FName(TEXT("Burn")), 2) },
		{ TEXT("fixture_board_presentation_armor_card"), TEXT("armor_2"), TEXT("Restore Armor"), MakeBoardPresentationArmorPayload(2) },
		{ TEXT("fixture_board_presentation_heal_card"), TEXT("heal_2"), TEXT("Heal 2 HP"), MakeBoardPresentationHealPayload(2) }
	};

	for (const FCase& TestCase : Cases)
	{
		FWBGameStateData State = MakeBoardPresentationState(TestCase.CardId);
		const FWBCardActivationLegalActionGenerationResult Result =
			GenerateBoardPresentationActions(
				State,
				MakeBoardPresentationDefinition(TestCase.CardId, TestCase.EffectId, TestCase.PublicLabel, TestCase.Payload));
		TestTrue(*FString::Printf(TEXT("%s legal action generation ok"), *TestCase.CardId), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s action count"), *TestCase.CardId), Result.ActionSet.Actions.Num(), 1);

		const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
			BuildBoardPresentationSnapshot(Result, State);
		TestEqual(*FString::Printf(TEXT("%s snapshot entry count"), *TestCase.CardId), Snapshot.Entries.Num(), 1);
		const FWBCardActivationLegalActionPresentationEntry& Entry = Snapshot.Entries[0];
		TestEqual(*FString::Printf(TEXT("%s label"), *TestCase.CardId), Entry.PublicLabel, TestCase.PublicLabel);
		TestTrue(*FString::Printf(TEXT("%s has source"), *TestCase.CardId), Entry.bHasPublicSourceUnit);
		TestTrue(*FString::Printf(TEXT("%s has target"), *TestCase.CardId), Entry.bHasPublicTargetUnit);
		TestEqual(*FString::Printf(TEXT("%s source card from public summary"), *TestCase.CardId), Entry.SourcePublicCardId, TestCase.CardId);
		TestEqual(*FString::Printf(TEXT("%s target card from public summary"), *TestCase.CardId), Entry.TargetPublicCardId, FString(TEXT("fixture_board_presentation_target")));
		TestFalse(*FString::Printf(TEXT("%s label excludes internal effect op"), *TestCase.CardId), Entry.PublicLabel.Contains(TEXT("_effect"), ESearchCase::IgnoreCase));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourcePresentationOrderingTest, "Wandbound.Core.CardActivationBoardSourcePresentation.OrderingAndPublicCardIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourcePresentationOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardPresentationState(TEXT("fixture_board_order_a"), TEXT("fixture_target_a"));
	State.GetMutableUnitById(3)->CardId = TEXT("fixture_target_b");
	State.GetMutableUnitById(4)->CardId = TEXT("fixture_board_order_b");

	FWBCardDefinition FirstDefinition;
	FirstDefinition.CardId = TEXT("fixture_board_order_a");
	FirstDefinition.PublicName = TEXT("Fixture Board Order A");
	FirstDefinition.Kind = EWBCardDefinitionKind::Fixture;
	FirstDefinition.ActivatedEffects.Add(MakeBoardPresentationEffect(TEXT("deal_1"), TEXT("Deal 1 damage"), MakeBoardPresentationDamagePayload(1)));
	FirstDefinition.ActivatedEffects.Add(MakeBoardPresentationEffect(TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardPresentationHealPayload(1)));

	const FWBCardDefinition SecondDefinition =
		MakeBoardPresentationDefinition(TEXT("fixture_board_order_b"), TEXT("burn_1"), TEXT("Apply Burn"), MakeBoardPresentationStatusPayload(FName(TEXT("Burn")), 1));

	const TArray<FWBCardActivationCandidateSource> Sources = {
		MakeBoardPresentationSource(FirstDefinition, { MakeBoardPresentationTarget(2), MakeBoardPresentationTarget(3) }),
		MakeBoardPresentationSource(SecondDefinition, { MakeBoardPresentationTarget(2) }, 4)
	};
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateBoardPresentationActions(State, Sources);
	TestTrue(TEXT("Ordered legal action generation ok"), Result.bOk);
	TestEqual(TEXT("Ordered action count"), Result.ActionSet.Actions.Num(), 5);

	const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
		BuildBoardPresentationSnapshot(Result, State);
	CompareStringArrays(*this, TEXT("Snapshot preserves ids"), PresentationIds(Snapshot), ActionIds(Result));

	const TArray<FString> ExpectedSourceCards = {
		TEXT("fixture_board_order_a"),
		TEXT("fixture_board_order_a"),
		TEXT("fixture_board_order_a"),
		TEXT("fixture_board_order_a"),
		TEXT("fixture_board_order_b")
	};
	const TArray<FString> ExpectedTargetCards = {
		TEXT("fixture_target_a"),
		TEXT("fixture_target_b"),
		TEXT("fixture_target_a"),
		TEXT("fixture_target_b"),
		TEXT("fixture_target_a")
	};
	TArray<FString> ActualSourceCards;
	TArray<FString> ActualTargetCards;
	for (const FWBCardActivationLegalActionPresentationEntry& Entry : Snapshot.Entries)
	{
		ActualSourceCards.Add(Entry.SourcePublicCardId);
		ActualTargetCards.Add(Entry.TargetPublicCardId);
	}
	CompareStringArrays(*this, TEXT("Source public card ids"), ActualSourceCards, ExpectedSourceCards);
	CompareStringArrays(*this, TEXT("Target public card ids"), ActualTargetCards, ExpectedTargetCards);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourcePresentationLookupTest, "Wandbound.Core.CardActivationBoardSourcePresentation.MissingSummaryAndLookup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourcePresentationLookupTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardPresentationState(TEXT("fixture_board_lookup_card"));
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateBoardPresentationActions(
			State,
			MakeBoardPresentationDefinition(TEXT("fixture_board_lookup_card"), TEXT("deal_2"), TEXT("Deal 2 damage"), MakeBoardPresentationDamagePayload(2)));
	TestTrue(TEXT("Lookup legal action generation ok"), Result.bOk);

	FWBPublicBoardSummary MissingSourceSummary = WBPublicBoardSummary::Build(State);
	MissingSourceSummary.Units.RemoveAll(
		[](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.UnitId == BoardPresentationSourceUnitId;
		});
	const FWBCardActivationLegalActionPresentationSnapshot MissingSourceSnapshot =
		WBCardActivationLegalActionPresentation::BuildSnapshot(Result.ActionSet, MissingSourceSummary);
	TestFalse(TEXT("Missing source flag false"), MissingSourceSnapshot.Entries[0].bHasPublicSourceUnit);
	TestTrue(TEXT("Missing source card empty"), MissingSourceSnapshot.Entries[0].SourcePublicCardId.IsEmpty());
	TestTrue(TEXT("Target still present"), MissingSourceSnapshot.Entries[0].bHasPublicTargetUnit);

	FWBPublicBoardSummary MissingTargetSummary = WBPublicBoardSummary::Build(State);
	MissingTargetSummary.Units.RemoveAll(
		[](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.UnitId == BoardPresentationTargetUnitId;
		});
	const FWBCardActivationLegalActionPresentationSnapshot MissingTargetSnapshot =
		WBCardActivationLegalActionPresentation::BuildSnapshot(Result.ActionSet, MissingTargetSummary);
	TestTrue(TEXT("Source still present"), MissingTargetSnapshot.Entries[0].bHasPublicSourceUnit);
	TestFalse(TEXT("Missing target flag false"), MissingTargetSnapshot.Entries[0].bHasPublicTargetUnit);
	TestTrue(TEXT("Missing target card empty"), MissingTargetSnapshot.Entries[0].TargetPublicCardId.IsEmpty());

	FWBCardActivationLegalActionPresentationEntry FoundEntry;
	const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
		BuildBoardPresentationSnapshot(Result, State);
	TestTrue(TEXT("Lookup exact id succeeds"), WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(Snapshot, Result.ActionSet.Actions[0].ActivationActionId, FoundEntry));
	TestEqual(TEXT("Lookup exact id"), FoundEntry.ActivationActionId, Result.ActionSet.Actions[0].ActivationActionId);
	TestFalse(TEXT("Lookup missing fails"), WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(Snapshot, TEXT("missing_activation_action"), FoundEntry));

	FWBCardActivationLegalActionSet DuplicateSet = Result.ActionSet;
	DuplicateSet.Actions.Add(Result.ActionSet.Actions[0]);
	const FWBCardActivationLegalActionPresentationSnapshot DuplicateSnapshot =
		WBCardActivationLegalActionPresentation::BuildSnapshot(DuplicateSet, WBPublicBoardSummary::Build(State));
	TestFalse(TEXT("Lookup duplicate fails"), WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(DuplicateSnapshot, Result.ActionSet.Actions[0].ActivationActionId, FoundEntry));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourcePresentationHiddenMetadataTest, "Wandbound.Core.CardActivationBoardSourcePresentation.HiddenMetadataLabelsAndNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourcePresentationHiddenMetadataTest::RunTest(const FString& Parameters)
{
	const FString VisibleCardId = TEXT("visible_board_presentation_card");
	FWBGameStateData State = MakeBoardPresentationState(VisibleCardId);
	State.Players[0].Hand.Add(TEXT("SECRET_PRESENTATION_HAND_DO_NOT_LEAK"));
	State.Players[0].Deck.Add(TEXT("SECRET_PRESENTATION_DECK_DO_NOT_LEAK"));
	State.Players[0].Discard.Add(TEXT("SECRET_PRESENTATION_DISCARD_DO_NOT_LEAK"));

	const int32 InitialTargetHP = State.GetUnitById(BoardPresentationTargetUnitId)->HP;
	const int32 InitialTargetArmor = State.GetUnitById(BoardPresentationTargetUnitId)->GetCurrentArmor();
	const int32 InitialTargetStatusCount = State.GetUnitById(BoardPresentationTargetUnitId)->Statuses.Num();

	FWBCardActivationCandidateSource Source = MakeBoardPresentationSource(
		MakeBoardPresentationDefinition(VisibleCardId, TEXT("deal_2"), TEXT("Deal 2 damage"), MakeBoardPresentationDamagePayload(2)),
		{ MakeBoardPresentationTarget() });
	Source.SourceGateContext.ActivationUsageKey = TEXT("SECRET_PRESENTATION_USAGE_DO_NOT_LEAK");
	Source.SourceGateContext.CostContext.CostKind = FName(TEXT("SECRET_PRESENTATION_COST_DO_NOT_LEAK"));
	Source.SourceGateContext.FixtureZoneContext.Entries.Add({ TEXT("SECRET_PRESENTATION_ZONE_DO_NOT_LEAK"), 0, EWBCardActivationSourceZone::Hand, -1 });

	FWBCardActivationLegalActionGenerationResult Result =
		GenerateBoardPresentationActions(State, { Source });
	TestTrue(TEXT("Hidden metadata generation ok"), Result.bOk);
	TestEqual(TEXT("Hidden metadata action count"), Result.ActionSet.Actions.Num(), 1);
	Result.ActionSet.Actions[0].Candidate.SourceEffectId = TEXT("SECRET_PRESENTATION_EFFECT_DO_NOT_LEAK");
	Result.ActionSet.Actions[0].Command.DebugActivationId = TEXT("SECRET_PRESENTATION_DEBUG_DO_NOT_LEAK");
	Result.ActionSet.Actions[0].Command.UsageCommit.UsageKey = TEXT("SECRET_PRESENTATION_USAGE_DO_NOT_LEAK");
	Result.ActionSet.Actions[0].Command.CostPaymentCommit.CostKind = FName(TEXT("SECRET_PRESENTATION_COST_DO_NOT_LEAK"));

	const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
		BuildBoardPresentationSnapshot(Result, State);
	const FString PublicText = PresentationPublicSurfaceText(Snapshot);
	const TArray<FString> ForbiddenSubstrings = {
		TEXT("SECRET_PRESENTATION_EFFECT_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_USAGE_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_DEBUG_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_COST_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_ZONE_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_HAND_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_DECK_DO_NOT_LEAK"),
		TEXT("SECRET_PRESENTATION_DISCARD_DO_NOT_LEAK"),
		TEXT("damage_effect"),
		TEXT("EffectRunner"),
		TEXT("Rules"),
		TEXT("schema"),
		TEXT("hook")
	};
	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Presentation public text excludes %s"), *Forbidden), PublicText.Contains(Forbidden, ESearchCase::IgnoreCase));
	}

	TestEqual(TEXT("Snapshot did not mutate HP"), State.GetUnitById(BoardPresentationTargetUnitId)->HP, InitialTargetHP);
	TestEqual(TEXT("Snapshot did not mutate armor"), State.GetUnitById(BoardPresentationTargetUnitId)->GetCurrentArmor(), InitialTargetArmor);
	TestEqual(TEXT("Snapshot did not mutate statuses"), State.GetUnitById(BoardPresentationTargetUnitId)->Statuses.Num(), InitialTargetStatusCount);

	FWBCardActivationLegalActionGenerationResult UnsafeResult = Result;
	UnsafeResult.ActionSet.Actions[0].PublicLabel = FString();
	const FWBCardActivationLegalActionPresentationSnapshot FallbackSnapshot =
		BuildBoardPresentationSnapshot(UnsafeResult, State);
	TestEqual(TEXT("Empty presentation label falls back"), FallbackSnapshot.Entries[0].PublicLabel, FString(TEXT("Activate")));

	FWBCardActivationCandidate UnsafeLabelCandidate = Result.ActionSet.Actions[0].Candidate;
	UnsafeLabelCandidate.PublicLabel = TEXT("damage_effect internal label");
	const FWBCardActivationLegalActionGenerationResult UnsafeLabelResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates({ UnsafeLabelCandidate });
	TestFalse(TEXT("Unsafe label rejected before presentation"), UnsafeLabelResult.bOk);
	TestTrue(TEXT("Unsafe label reason"), UnsafeLabelResult.Reason.Contains(TEXT("unsafe_public_label")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourcePresentationSeparationTest, "Wandbound.Core.CardActivationBoardSourcePresentation.SourceGuardsAndSeparation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourcePresentationSeparationTest::RunTest(const FString& Parameters)
{
	const FString PresentationSourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationLegalAction.cpp"));
	FString PresentationSource;
	TestTrue(TEXT("Presentation source loads"), FFileHelper::LoadFileToString(PresentationSource, *PresentationSourcePath));
	TestFalse(TEXT("Presentation source does not inspect game state"), PresentationSource.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Presentation source does not inspect fixture zones"), PresentationSource.Contains(TEXT("FixtureZoneContext")));
	TestFalse(TEXT("Presentation source does not inspect usage keys"), PresentationSource.Contains(TEXT("UsageKey")));
	TestFalse(TEXT("Presentation source does not inspect debug ids"), PresentationSource.Contains(TEXT("DebugActivationId")));
	TestFalse(TEXT("Presentation source does not expose source effects"), PresentationSource.Contains(TEXT("SourceEffectId")));
	TestFalse(TEXT("Presentation source does not copy candidate card id"), PresentationSource.Contains(TEXT("Candidate.SourceCardId")));

	FWBGameStateData State = MakeBoardPresentationState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateBoardPresentationActions(
			State,
			MakeBoardPresentationDefinition(TEXT("fixture_board_presentation_card"), TEXT("heal_1"), TEXT("Heal 1 HP"), MakeBoardPresentationHealPayload(1)));
	TestTrue(TEXT("Activation legal action generation ok"), Result.bOk);
	const FWBCardActivationLegalActionPresentationSnapshot Snapshot =
		BuildBoardPresentationSnapshot(Result, State);
	TestEqual(TEXT("Activation presentation count"), Snapshot.Entries.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move codec unchanged"), WBActionCodec::MakeActionId(MakeBoardPresentationCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack codec unchanged"), WBActionCodec::MakeActionId(MakeBoardPresentationCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn codec unchanged"), WBActionCodec::MakeActionId(MakeBoardPresentationCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass codec unchanged"), WBActionCodec::MakeActionId(MakeBoardPresentationCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse codec unchanged"), WBActionCodec::MakeActionId(MakeBoardPresentationCodecAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	FString ActionCodecSource;
	TestTrue(
		TEXT("Action codec source loads"),
		FFileHelper::LoadFileToString(
			ActionCodecSource,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec has no activation ids"), ActionCodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourcePresentationFixtureScenariosTest, "Wandbound.Core.CardActivationBoardSourcePresentation.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourcePresentationFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_board_source_presentation_damage.json"),
		TEXT("card_activation_board_source_presentation_status.json"),
		TEXT("card_activation_board_source_presentation_armor.json"),
		TEXT("card_activation_board_source_presentation_heal.json"),
		TEXT("card_activation_board_source_presentation_ordered.json"),
		TEXT("card_activation_board_source_presentation_public_card_ids.json"),
		TEXT("card_activation_board_source_presentation_clean_labels.json"),
		TEXT("card_activation_board_source_presentation_missing_public_source_unit.json"),
		TEXT("card_activation_board_source_presentation_duplicate_lookup_fails.json"),
		TEXT("card_activation_board_source_presentation_hidden_metadata_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadBoardPresentationFixture(FixtureName, Fixture));
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

		const int32 InitialTargetHP = State.GetUnitById(BoardPresentationTargetUnitId) != nullptr ? State.GetUnitById(BoardPresentationTargetUnitId)->HP : -1;
		const int32 InitialTargetArmor = State.GetUnitById(BoardPresentationTargetUnitId) != nullptr ? State.GetUnitById(BoardPresentationTargetUnitId)->GetCurrentArmor() : -1;
		const int32 InitialTargetStatusCount = State.GetUnitById(BoardPresentationTargetUnitId) != nullptr ? State.GetUnitById(BoardPresentationTargetUnitId)->Statuses.Num() : -1;

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::BuildCardActivationPresentationSnapshot);

		const TSharedPtr<FJsonObject>* ExpectedPtr = nullptr;
		TestTrue(
			*FString::Printf(TEXT("%s has expected"), *FixtureName),
			Fixture->TryGetObjectField(TEXT("expected"), ExpectedPtr) && ExpectedPtr != nullptr && ExpectedPtr->IsValid());
		if (ExpectedPtr == nullptr || !ExpectedPtr->IsValid())
		{
			return false;
		}

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), (*ExpectedPtr)->TryGetBoolField(TEXT("ok"), bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), OperationResult.bOk, bExpectedOk);

		bool bNoStateMutation = false;
		if ((*ExpectedPtr)->TryGetBoolField(TEXT("no_state_mutation"), bNoStateMutation) && bNoStateMutation)
		{
			if (const FWBUnitState* Unit = State.GetUnitById(BoardPresentationTargetUnitId))
			{
				TestEqual(*FString::Printf(TEXT("%s target HP unchanged"), *FixtureName), Unit->HP, InitialTargetHP);
				TestEqual(*FString::Printf(TEXT("%s target armor unchanged"), *FixtureName), Unit->GetCurrentArmor(), InitialTargetArmor);
				TestEqual(*FString::Printf(TEXT("%s target status count unchanged"), *FixtureName), Unit->Statuses.Num(), InitialTargetStatusCount);
			}
		}
	}

	return true;
}

#endif
