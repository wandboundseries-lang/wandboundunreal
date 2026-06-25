#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBCardActivationTargetPresentation.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

constexpr int32 TargetPresentationSourceUnitId = 1;
constexpr int32 TargetPresentationTargetUnitId = 2;

FWBPlayerStateData MakeTargetPresentationPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeTargetPresentationUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 5;
	Unit.RLUsed = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeTargetPresentationState(
	const FString& SourceCardId = TEXT("visible_target_presentation_source"),
	const FString& TargetCardId = TEXT("visible_target_presentation_target"))
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 16;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeTargetPresentationPlayer(0));
	State.Players.Add(MakeTargetPresentationPlayer(1));
	State.AddUnitForTest(MakeTargetPresentationUnit(TargetPresentationSourceUnitId, 0, SourceCardId, FWBTile(4, 4)));
	State.AddUnitForTest(MakeTargetPresentationUnit(TargetPresentationTargetUnitId, 1, TargetCardId, FWBTile(5, 4)));
	State.AddUnitForTest(MakeTargetPresentationUnit(3, 1, TEXT("visible_target_presentation_target_b"), FWBTile(6, 4)));
	State.AddUnitForTest(MakeTargetPresentationUnit(4, 0, TEXT("visible_target_presentation_source_b"), FWBTile(4, 5)));
	return State;
}

FWBCardActivationSourceGateDefinition MakeTargetPresentationGate()
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

FWBGenericEffectPayload MakeTargetPresentationHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBGenericEffectPayload MakeTargetPresentationDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardDefinition MakeTargetPresentationDefinition(
	const FString& CardId,
	const FString& EffectId,
	const FString& PublicLabel,
	const EWBCardEffectTargetRequirement TargetRequirement)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicLabel;
	Definition.Kind = EWBCardDefinitionKind::Fixture;

	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = TargetRequirement;
	Effect.SourceGate = MakeTargetPresentationGate();
	Effect.Payloads.Add(MakeTargetPresentationHealPayload(1));
	Definition.ActivatedEffects.Add(Effect);
	return Definition;
}

FWBEffectTargetRef MakeUnitTarget(const int32 UnitId = TargetPresentationTargetUnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBEffectTargetRef MakeTileTarget(const int32 X = 3, const int32 Y = 4)
{
	FWBEffectTargetRef Target;
	Target.TargetTile = FWBTile(X, Y);
	return Target;
}

FWBEffectTargetRef MakeWallTarget()
{
	FWBEffectTargetRef Target;
	Target.TargetWallEdge = FWBWallEdge(FWBTile(2, 2), FWBTile(2, 3));
	return Target;
}

FWBCardActivationSourceGateContext MakeTargetPresentationContext(
	const FString& SourceCardId,
	const int32 SourceUnitId = TargetPresentationSourceUnitId)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.SourceCardId = SourceCardId;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationCandidateSource MakeTargetPresentationSource(
	const FWBCardDefinition& Definition,
	const TArray<FWBEffectTargetRef>& Targets,
	const int32 SourceUnitId = TargetPresentationSourceUnitId)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = Definition;
	Source.CandidateTargets = Targets;
	Source.SourceGateContext = MakeTargetPresentationContext(Definition.CardId, SourceUnitId);
	return Source;
}

FWBCardActivationLegalActionGenerationResult GenerateTargetPresentationActions(
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

FWBCardActivationTargetPresentationSnapshot BuildTargetPresentationSnapshot(
	const FWBCardActivationLegalActionGenerationResult& Result,
	const FWBGameStateData& State)
{
	return WBCardActivationTargetPresentation::BuildSnapshot(
		Result.ActionSet,
		WBPublicBoardSummary::Build(State));
}

FWBCardActivationLegalAction MakeDirectTargetPresentationAction(
	const FString& ActionId,
	const FString& PublicLabel,
	const FWBEffectTargetRef& Target)
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = ActionId;
	Action.PublicLabel = PublicLabel;
	Action.PlayerId = 0;
	Action.SourceUnitId = TargetPresentationSourceUnitId;
	Action.Target = Target;
	return Action;
}

TArray<FString> TargetPresentationIds(const FWBCardActivationTargetPresentationSnapshot& Snapshot)
{
	TArray<FString> Ids;
	for (const FWBCardActivationTargetPresentationEntry& Entry : Snapshot.Entries)
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

FString TargetPresentationPublicSurfaceText(const FWBCardActivationTargetPresentationSnapshot& Snapshot)
{
	FString Text;
	for (const FWBCardActivationTargetPresentationEntry& Entry : Snapshot.Entries)
	{
		Text += Entry.PublicActivationLabel;
		Text += TEXT("|");
		Text += Entry.PublicTargetLabel;
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

FWBAction MakeTargetPresentationCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = TargetPresentationSourceUnitId;
	Action.TargetUnitId = TargetPresentationTargetUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FString TargetPresentationFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadTargetPresentationFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *TargetPresentationFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, OutFixture);
	return OutFixture.IsValid();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationUnitTest, "Wandbound.Core.CardActivationTargetPresentation.UnitTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationUnitTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetPresentationState(TEXT("visible_unit_source"), TEXT("visible_unit_target"));
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateTargetPresentationActions(
			State,
			{ MakeTargetPresentationSource(
				MakeTargetPresentationDefinition(TEXT("visible_unit_source"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit),
				{ MakeUnitTarget() }) });
	TestTrue(TEXT("Unit target legal action generation ok"), Result.bOk);

	const FWBCardActivationTargetPresentationSnapshot Snapshot =
		BuildTargetPresentationSnapshot(Result, State);
	TestEqual(TEXT("Unit snapshot count"), Snapshot.Entries.Num(), 1);
	const FWBCardActivationTargetPresentationEntry& Entry = Snapshot.Entries[0];
	TestEqual(TEXT("Unit target kind"), Entry.TargetKind, EWBCardActivationTargetPresentationKind::Unit);
	TestEqual(TEXT("Unit target label"), Entry.PublicTargetLabel, FString(TEXT("Choose Unit")));
	TestEqual(TEXT("Activation label"), Entry.PublicActivationLabel, FString(TEXT("Deal 2 damage")));
	TestEqual(TEXT("Source public card"), Entry.SourcePublicCardId, FString(TEXT("visible_unit_source")));
	TestEqual(TEXT("Target public card"), Entry.TargetPublicCardId, FString(TEXT("visible_unit_target")));
	TestTrue(TEXT("Source public unit flag"), Entry.bHasPublicSourceUnit);
	TestTrue(TEXT("Target public unit flag"), Entry.bHasPublicTargetUnit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationMissingTargetTest, "Wandbound.Core.CardActivationTargetPresentation.MissingPublicTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationMissingTargetTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetPresentationState(TEXT("visible_missing_target_source"), TEXT("visible_missing_target"));
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateTargetPresentationActions(
			State,
			{ MakeTargetPresentationSource(
				MakeTargetPresentationDefinition(TEXT("visible_missing_target_source"), TEXT("deal_2"), TEXT("Deal 2 damage"), EWBCardEffectTargetRequirement::Unit),
				{ MakeUnitTarget() }) });
	TestTrue(TEXT("Missing target generation ok"), Result.bOk);

	FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	Summary.Units.RemoveAll(
		[](const FWBPublicUnitBoardSummary& Unit)
		{
			return Unit.UnitId == TargetPresentationTargetUnitId;
		});

	const FWBCardActivationTargetPresentationSnapshot Snapshot =
		WBCardActivationTargetPresentation::BuildSnapshot(Result.ActionSet, Summary);
	TestEqual(TEXT("Missing public target kind still unit"), Snapshot.Entries[0].TargetKind, EWBCardActivationTargetPresentationKind::Unit);
	TestFalse(TEXT("Missing public target flag"), Snapshot.Entries[0].bHasPublicTargetUnit);
	TestTrue(TEXT("Missing public target card empty"), Snapshot.Entries[0].TargetPublicCardId.IsEmpty());
	TestTrue(TEXT("Missing target entry still exists"), Snapshot.Entries.Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationNonUnitKindsTest, "Wandbound.Core.CardActivationTargetPresentation.NoneTileWallKinds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationNonUnitKindsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetPresentationState(TEXT("visible_non_unit_source"));
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(MakeDirectTargetPresentationAction(TEXT("activate:none"), FString(), FWBEffectTargetRef()));
	Set.Actions.Add(MakeDirectTargetPresentationAction(TEXT("activate:tile"), TEXT("Place Wall"), MakeTileTarget(3, 4)));
	Set.Actions.Add(MakeDirectTargetPresentationAction(TEXT("activate:wall"), TEXT("Destroy Wall"), MakeWallTarget()));

	FWBEffectTargetRef PartialTarget;
	PartialTarget.TargetTile.X = 2;
	Set.Actions.Add(MakeDirectTargetPresentationAction(TEXT("activate:unknown"), TEXT("Mystery"), PartialTarget));

	const FWBCardActivationTargetPresentationSnapshot Snapshot =
		WBCardActivationTargetPresentation::BuildSnapshot(Set, WBPublicBoardSummary::Build(State));
	TestEqual(TEXT("Non-unit count"), Snapshot.Entries.Num(), 4);
	TestEqual(TEXT("None kind"), Snapshot.Entries[0].TargetKind, EWBCardActivationTargetPresentationKind::None);
	TestEqual(TEXT("None label"), Snapshot.Entries[0].PublicTargetLabel, FString(TEXT("Activate")));
	TestEqual(TEXT("None activation label fallback"), Snapshot.Entries[0].PublicActivationLabel, FString(TEXT("Activate")));
	TestEqual(TEXT("Tile kind"), Snapshot.Entries[1].TargetKind, EWBCardActivationTargetPresentationKind::Tile);
	TestEqual(TEXT("Tile label"), Snapshot.Entries[1].PublicTargetLabel, FString(TEXT("Choose Tile")));
	TestEqual(TEXT("Tile value"), Snapshot.Entries[1].TargetTile, FWBTile(3, 4));
	TestEqual(TEXT("Wall kind"), Snapshot.Entries[2].TargetKind, EWBCardActivationTargetPresentationKind::WallEdge);
	TestEqual(TEXT("Wall label"), Snapshot.Entries[2].PublicTargetLabel, FString(TEXT("Choose Wall")));
	TestEqual(TEXT("Wall value"), Snapshot.Entries[2].TargetWallEdge, MakeWallTarget().TargetWallEdge);
	TestEqual(TEXT("Unknown kind"), Snapshot.Entries[3].TargetKind, EWBCardActivationTargetPresentationKind::Unknown);
	TestEqual(TEXT("Unknown label"), Snapshot.Entries[3].PublicTargetLabel, FString(TEXT("Choose Target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationOrderingAndLookupTest, "Wandbound.Core.CardActivationTargetPresentation.OrderingAndLookup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationOrderingAndLookupTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetPresentationState(TEXT("visible_target_order_a"), TEXT("visible_target_a"));
	State.GetMutableUnitById(3)->CardId = TEXT("visible_target_b");
	State.GetMutableUnitById(4)->CardId = TEXT("visible_target_order_b");

	FWBCardDefinition FirstDefinition = MakeTargetPresentationDefinition(
		TEXT("visible_target_order_a"),
		TEXT("heal_1"),
		TEXT("Heal 1 HP"),
		EWBCardEffectTargetRequirement::Unit);
	FirstDefinition.ActivatedEffects.Add(
		MakeTargetPresentationDefinition(
			TEXT("unused"),
			TEXT("deal_1"),
			TEXT("Deal 1 damage"),
			EWBCardEffectTargetRequirement::Unit).ActivatedEffects[0]);
	const FWBCardDefinition SecondDefinition = MakeTargetPresentationDefinition(
		TEXT("visible_target_order_b"),
		TEXT("burn_1"),
		TEXT("Apply Burn"),
		EWBCardEffectTargetRequirement::Unit);

	const TArray<FWBCardActivationCandidateSource> Sources = {
		MakeTargetPresentationSource(FirstDefinition, { MakeUnitTarget(2), MakeUnitTarget(3) }),
		MakeTargetPresentationSource(SecondDefinition, { MakeUnitTarget(2) }, 4)
	};
	const FWBCardActivationLegalActionGenerationResult Result =
		GenerateTargetPresentationActions(State, Sources);
	TestTrue(TEXT("Ordered generation ok"), Result.bOk);

	const FWBCardActivationTargetPresentationSnapshot Snapshot =
		BuildTargetPresentationSnapshot(Result, State);
	CompareStringArrays(*this, TEXT("Order preserves ids"), TargetPresentationIds(Snapshot), ActionIds(Result));

	FWBCardActivationTargetPresentationEntry FoundEntry;
	TestTrue(TEXT("Lookup exact succeeds"), WBCardActivationTargetPresentation::TryFindEntryByActivationActionId(Snapshot, Snapshot.Entries[0].ActivationActionId, FoundEntry));
	TestEqual(TEXT("Lookup exact id"), FoundEntry.ActivationActionId, Snapshot.Entries[0].ActivationActionId);
	TestFalse(TEXT("Lookup missing fails"), WBCardActivationTargetPresentation::TryFindEntryByActivationActionId(Snapshot, TEXT("missing_activation_id"), FoundEntry));

	FWBCardActivationLegalActionSet DuplicateSet = Result.ActionSet;
	DuplicateSet.Actions.Add(Result.ActionSet.Actions[0]);
	const FWBCardActivationTargetPresentationSnapshot DuplicateSnapshot =
		WBCardActivationTargetPresentation::BuildSnapshot(DuplicateSet, WBPublicBoardSummary::Build(State));
	TestFalse(TEXT("Lookup duplicate fails"), WBCardActivationTargetPresentation::TryFindEntryByActivationActionId(DuplicateSnapshot, Result.ActionSet.Actions[0].ActivationActionId, FoundEntry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationHiddenMetadataTest, "Wandbound.Core.CardActivationTargetPresentation.HiddenMetadataAndNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationHiddenMetadataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTargetPresentationState(TEXT("visible_safe_source"), TEXT("visible_safe_target"));
	State.Players[0].Hand.Add(TEXT("SECRET_TARGET_HAND_DO_NOT_LEAK"));
	State.Players[0].Deck.Add(TEXT("SECRET_TARGET_DECK_DO_NOT_LEAK"));
	State.Players[0].Discard.Add(TEXT("SECRET_TARGET_DISCARD_DO_NOT_LEAK"));

	const int32 InitialTargetHP = State.GetUnitById(TargetPresentationTargetUnitId)->HP;
	const int32 InitialTargetStatusCount = State.GetUnitById(TargetPresentationTargetUnitId)->Statuses.Num();

	FWBCardActivationLegalAction Action = MakeDirectTargetPresentationAction(
		TEXT("activate:hidden"),
		TEXT("Heal 1 HP"),
		MakeUnitTarget());
	Action.Candidate.SourceCardId = TEXT("SECRET_TARGET_SOURCE_CARD_DO_NOT_LEAK");
	Action.Candidate.SourceEffectId = TEXT("SECRET_TARGET_EFFECT_DO_NOT_LEAK");
	Action.Command.DebugActivationId = TEXT("SECRET_TARGET_DEBUG_DO_NOT_LEAK");
	Action.Command.UsageCommit.UsageKey = TEXT("SECRET_TARGET_USAGE_DO_NOT_LEAK");
	Action.Command.CostPaymentCommit.CostKind = FName(TEXT("SECRET_TARGET_COST_DO_NOT_LEAK"));
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(Action);

	const FWBCardActivationTargetPresentationSnapshot Snapshot =
		WBCardActivationTargetPresentation::BuildSnapshot(Set, WBPublicBoardSummary::Build(State));
	TestEqual(TEXT("Hidden source uses public summary"), Snapshot.Entries[0].SourcePublicCardId, FString(TEXT("visible_safe_source")));
	TestEqual(TEXT("Hidden target uses public summary"), Snapshot.Entries[0].TargetPublicCardId, FString(TEXT("visible_safe_target")));

	const FString PublicText = TargetPresentationPublicSurfaceText(Snapshot);
	const TArray<FString> ForbiddenSubstrings = {
		TEXT("SECRET_TARGET_SOURCE_CARD_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_EFFECT_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_DEBUG_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_USAGE_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_COST_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_HAND_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_DECK_DO_NOT_LEAK"),
		TEXT("SECRET_TARGET_DISCARD_DO_NOT_LEAK"),
		TEXT("damage_effect"),
		TEXT("heal_effect"),
		TEXT("EffectRunner"),
		TEXT("Rules"),
		TEXT("schema"),
		TEXT("hook")
	};
	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Target presentation excludes %s"), *Forbidden), PublicText.Contains(Forbidden, ESearchCase::IgnoreCase));
	}

	TestEqual(TEXT("Target presentation did not mutate HP"), State.GetUnitById(TargetPresentationTargetUnitId)->HP, InitialTargetHP);
	TestEqual(TEXT("Target presentation did not mutate statuses"), State.GetUnitById(TargetPresentationTargetUnitId)->Statuses.Num(), InitialTargetStatusCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationSeparationTest, "Wandbound.Core.CardActivationTargetPresentation.SourceGuardsAndSeparation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationSeparationTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardActivationTargetPresentation.cpp"));
	FString Source;
	TestTrue(TEXT("Target presentation source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));
	TestFalse(TEXT("No game state dependency"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("No fixture zone dependency"), Source.Contains(TEXT("FixtureZoneContext")));
	TestFalse(TEXT("No usage key dependency"), Source.Contains(TEXT("UsageKey")));
	TestFalse(TEXT("No debug activation dependency"), Source.Contains(TEXT("DebugActivationId")));
	TestFalse(TEXT("No source effect dependency"), Source.Contains(TEXT("SourceEffectId")));
	TestFalse(TEXT("No cost metadata dependency"), Source.Contains(TEXT("CostPayment")));
	TestFalse(TEXT("No EffectRunner dependency"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No WBRules dependency"), Source.Contains(TEXT("WBRules")));

	FWBGameStateData State = MakeTargetPresentationState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	FWBCardActivationLegalActionSet Set;
	Set.Actions.Add(MakeDirectTargetPresentationAction(TEXT("activate:separation"), TEXT("Activate"), MakeUnitTarget()));
	const FWBCardActivationTargetPresentationSnapshot Snapshot =
		WBCardActivationTargetPresentation::BuildSnapshot(Set, WBPublicBoardSummary::Build(State));
	TestEqual(TEXT("Separation snapshot count"), Snapshot.Entries.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	CompareStringArrays(*this, TEXT("FWBAction legal ids unchanged"), AfterIds, BaselineIds);
	for (const FString& ActionId : AfterIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move codec unchanged"), WBActionCodec::MakeActionId(MakeTargetPresentationCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack codec unchanged"), WBActionCodec::MakeActionId(MakeTargetPresentationCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn codec unchanged"), WBActionCodec::MakeActionId(MakeTargetPresentationCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass codec unchanged"), WBActionCodec::MakeActionId(MakeTargetPresentationCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse codec unchanged"), WBActionCodec::MakeActionId(MakeTargetPresentationCodecAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	FString CodecSource;
	TestTrue(
		TEXT("Action codec source loads"),
		FFileHelper::LoadFileToString(
			CodecSource,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"))));
	TestFalse(TEXT("WBActionCodec has no activation ids"), CodecSource.Contains(TEXT("activate:"), ESearchCase::IgnoreCase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationTargetPresentationFixtureScenariosTest, "Wandbound.Core.CardActivationTargetPresentation.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationTargetPresentationFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_target_presentation_unit_target.json"),
		TEXT("card_activation_target_presentation_missing_public_target.json"),
		TEXT("card_activation_target_presentation_none_target.json"),
		TEXT("card_activation_target_presentation_tile_target.json"),
		TEXT("card_activation_target_presentation_wall_edge_target.json"),
		TEXT("card_activation_target_presentation_ordered.json"),
		TEXT("card_activation_target_presentation_clean_labels.json"),
		TEXT("card_activation_target_presentation_public_card_ids_only.json"),
		TEXT("card_activation_target_presentation_duplicate_lookup_fails.json"),
		TEXT("card_activation_target_presentation_hidden_metadata_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadTargetPresentationFixture(FixtureName, Fixture));
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

		const int32 InitialTargetHP = State.GetUnitById(TargetPresentationTargetUnitId) != nullptr ? State.GetUnitById(TargetPresentationTargetUnitId)->HP : -1;
		const int32 InitialTargetStatusCount = State.GetUnitById(TargetPresentationTargetUnitId) != nullptr ? State.GetUnitById(TargetPresentationTargetUnitId)->Statuses.Num() : -1;

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::BuildCardActivationTargetPresentationSnapshot);

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
			if (const FWBUnitState* Unit = State.GetUnitById(TargetPresentationTargetUnitId))
			{
				TestEqual(*FString::Printf(TEXT("%s target HP unchanged"), *FixtureName), Unit->HP, InitialTargetHP);
				TestEqual(*FString::Printf(TEXT("%s target status count unchanged"), *FixtureName), Unit->Statuses.Num(), InitialTargetStatusCount);
			}
		}
	}

	return true;
}

#endif
