#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
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

FString UsageFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadUsageFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *UsageFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutFixture) && OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	return Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		? *Child
		: nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	return Object.IsValid() && Object->TryGetArrayField(FieldName, Values) ? Values : nullptr;
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	double Number = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, Number))
	{
		return false;
	}

	OutValue = static_cast<int32>(Number);
	return true;
}

bool ReadExpectedOk(const TSharedPtr<FJsonObject>& Fixture, bool& bOutOk)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	return Expected.IsValid() && Expected->TryGetBoolField(TEXT("ok"), bOutOk);
}

FString ReadExpectedReasonContains(const TSharedPtr<FJsonObject>& Fixture)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	FString Reason;
	if (Expected.IsValid())
	{
		Expected->TryGetStringField(TEXT("reason_contains"), Reason);
	}
	return Reason;
}

bool ReadExpectedUsage(
	const TSharedPtr<FJsonObject>& Fixture,
	int32& OutPlayerId,
	FString& OutUsageKey,
	bool& bOutMarked)
{
	const TSharedPtr<FJsonObject> Usage = GetObjectField(GetObjectField(Fixture, TEXT("expected")), TEXT("usage"));
	if (!Usage.IsValid())
	{
		return false;
	}

	return TryReadIntegerField(Usage, TEXT("player_id"), OutPlayerId)
		&& Usage->TryGetStringField(TEXT("usage_key"), OutUsageKey)
		&& Usage->TryGetBoolField(TEXT("marked"), bOutMarked);
}

bool ReadExpectedCommandUsageCommit(
	const TSharedPtr<FJsonObject>& Fixture,
	bool& bOutMarkUsageOnSuccess,
	int32& OutPlayerId,
	FString& OutUsageKey)
{
	const TSharedPtr<FJsonObject> Commit = GetObjectField(GetObjectField(Fixture, TEXT("expected")), TEXT("command_usage_commit"));
	if (!Commit.IsValid())
	{
		return false;
	}

	return Commit->TryGetBoolField(TEXT("mark_usage_on_success"), bOutMarkUsageOnSuccess)
		&& TryReadIntegerField(Commit, TEXT("player_id"), OutPlayerId)
		&& Commit->TryGetStringField(TEXT("usage_key"), OutUsageKey);
}

void ExpectUsageState(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State)
{
	int32 PlayerId = -1;
	FString UsageKey;
	bool bMarked = false;
	if (!ReadExpectedUsage(Fixture, PlayerId, UsageKey, bMarked))
	{
		return;
	}

	Test.TestEqual(
		*FString::Printf(TEXT("%s usage key state"), *FixtureName),
		State.HasActivationUsageKeyThisTurn(PlayerId, UsageKey),
		bMarked);
}

void ExpectForbiddenTraceSubstrings(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TArray<TSharedPtr<FJsonValue>>* ForbiddenValues = GetArrayField(Expected, TEXT("forbidden_trace_substrings"));
	if (ForbiddenValues == nullptr)
	{
		return;
	}

	const FString SerializedTraceEvents = WBReplayTrace::SerializeEvents(TraceEvents);
	for (const TSharedPtr<FJsonValue>& Value : *ForbiddenValues)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			continue;
		}

		const FString Forbidden = Value->AsString();
		Test.TestFalse(
			*FString::Printf(TEXT("%s trace excludes %s"), *FixtureName, *Forbidden),
			SerializedTraceEvents.Contains(Forbidden));
	}
}

TArray<FString> CandidateIds(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Ids;
	Ids.Reserve(Result.Candidates.Num());
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Ids.Add(Candidate.ActivationCandidateId);
	}
	return Ids;
}

TArray<FString> LegalActivationActionIds(const FWBCardActivationLegalActionGenerationResult& Result)
{
	TArray<FString> Ids;
	Ids.Reserve(Result.ActionSet.Actions.Num());
	for (const FWBCardActivationLegalAction& Action : Result.ActionSet.Actions)
	{
		Ids.Add(Action.ActivationActionId);
	}
	return Ids;
}

TArray<FString> ExpectedStringArray(const TSharedPtr<FJsonObject>& Fixture, const TCHAR* FieldName)
{
	TArray<FString> Strings;
	const TArray<TSharedPtr<FJsonValue>>* Values = GetArrayField(GetObjectField(Fixture, TEXT("expected")), FieldName);
	if (Values == nullptr)
	{
		return Strings;
	}

	Strings.Reserve(Values->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (Value.IsValid() && Value->Type == EJson::String)
		{
			Strings.Add(Value->AsString());
		}
	}
	return Strings;
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

int32 ReadExpectedIntOrDefault(const TSharedPtr<FJsonObject>& Fixture, const TCHAR* FieldName, const int32 DefaultValue)
{
	int32 Value = DefaultValue;
	TryReadIntegerField(GetObjectField(Fixture, TEXT("expected")), FieldName, Value);
	return Value;
}

void ExpectUsageCommitMatchesFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const FWBCardActivationCommand& Command,
	const TSharedPtr<FJsonObject>& Fixture)
{
	bool bExpectedMarkUsageOnSuccess = false;
	int32 ExpectedPlayerId = -1;
	FString ExpectedUsageKey;
	Test.TestTrue(
		*FString::Printf(TEXT("%s expected command usage commit reads"), *FixtureName),
		ReadExpectedCommandUsageCommit(Fixture, bExpectedMarkUsageOnSuccess, ExpectedPlayerId, ExpectedUsageKey));
	Test.TestEqual(*FString::Printf(TEXT("%s mark usage"), *FixtureName), Command.UsageCommit.bMarkUsageOnSuccess, bExpectedMarkUsageOnSuccess);
	Test.TestEqual(*FString::Printf(TEXT("%s usage player"), *FixtureName), Command.UsageCommit.PlayerId, ExpectedPlayerId);
	Test.TestEqual(*FString::Printf(TEXT("%s usage key"), *FixtureName), Command.UsageCommit.UsageKey, ExpectedUsageKey);
}

FWBPlayerStateData MakeUsagePlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeUsageUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile, const int32 HP, const int32 MaxHP)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_usage_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeUsageState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 9;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeUsagePlayer(0));
	State.Players.Add(MakeUsagePlayer(1));
	State.AddUnitForTest(MakeUsageUnit(1, 0, FWBTile(4, 4), 5, 5));
	State.AddUnitForTest(MakeUsageUnit(2, 1, FWBTile(5, 4), 2, 6));
	return State;
}

FWBGenericEffectPayload MakeUsageHealPayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = Amount;
	Payload.HealEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBCardActivationCommand MakeUsageCommand()
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = 1;
	Command.Source.SourceCardId = TEXT("fixture_usage_rules_card");
	Command.Source.SourceEffectId = TEXT("fixture_usage_rules_effect");
	Command.EffectRequest.Target.TargetUnitId = 2;
	Command.EffectRequest.Payloads.Add(MakeUsageHealPayload(2));
	Command.UsageCommit.bMarkUsageOnSuccess = true;
	Command.UsageCommit.PlayerId = 0;
	Command.UsageCommit.UsageKey = TEXT("fixture_usage_rules_key");
	return Command;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationUsageDirectFixtureTest, "Wandbound.Core.CardActivationUsageMarking.DirectApplyFixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationUsageDirectFixtureTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_usage_marked_on_success.json"),
		TEXT("card_activation_usage_not_marked_on_failure.json"),
		TEXT("card_activation_usage_commit_invalid_fails_atomic.json"),
		TEXT("card_activation_usage_already_used_fails.json"),
		TEXT("card_activation_usage_not_public_trace.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadUsageFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

		FWBApplyActionResult ApplyResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString ApplyReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies operation: %s"), *FixtureName, *ApplyReason),
			ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::ApplyCardActivationCommand);

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ReadExpectedOk(Fixture, bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), ApplyResult.bOk, bExpectedOk);

		const FString ExpectedReasonContains = ReadExpectedReasonContains(Fixture);
		if (!ExpectedReasonContains.IsEmpty())
		{
			TestTrue(
				*FString::Printf(TEXT("%s reason contains %s"), *FixtureName, *ExpectedReasonContains),
				ApplyResult.Reason.Contains(ExpectedReasonContains));
		}

		FString ExpectReason;
		TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
		ExpectUsageState(*this, FixtureName, Fixture, State);
		ExpectForbiddenTraceSubstrings(*this, FixtureName, Fixture, ApplyResult.TraceEvents);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationUsageCandidateFixtureTest, "Wandbound.Core.CardActivationUsageMarking.CandidateExclusionAndResetFixtures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationUsageCandidateFixtureTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_usage_candidate_excluded_after_success.json"),
		TEXT("card_activation_usage_resets_on_turn_start.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadUsageFixture(FixtureName, Fixture));
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

		const FWBCardActivationCandidateGenerationResult InitialResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		TestTrue(*FString::Printf(TEXT("%s initial generation ok"), *FixtureName), InitialResult.bOk);
		TestEqual(*FString::Printf(TEXT("%s initial candidate count"), *FixtureName), InitialResult.Candidates.Num(), ReadExpectedIntOrDefault(Fixture, TEXT("candidate_count"), -1));
		CompareStringArrays(*this, FixtureName + TEXT(" candidate ids"), CandidateIds(InitialResult), ExpectedStringArray(Fixture, TEXT("candidate_ids")));
		if (InitialResult.Candidates.Num() > 0)
		{
			int32 ExpectedPlayerId = -1;
			FString ExpectedUsageKey;
			bool bExpectedMarked = false;
			TestTrue(*FString::Printf(TEXT("%s usage expectation reads"), *FixtureName), ReadExpectedUsage(Fixture, ExpectedPlayerId, ExpectedUsageKey, bExpectedMarked));
			TestTrue(*FString::Printf(TEXT("%s candidate has usage commit"), *FixtureName), InitialResult.Candidates[0].Command.UsageCommit.bMarkUsageOnSuccess);
			TestEqual(*FString::Printf(TEXT("%s candidate usage player"), *FixtureName), InitialResult.Candidates[0].Command.UsageCommit.PlayerId, ExpectedPlayerId);
			TestEqual(*FString::Printf(TEXT("%s candidate usage key"), *FixtureName), InitialResult.Candidates[0].Command.UsageCommit.UsageKey, ExpectedUsageKey);
		}

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationCandidates);

		const int32 ApplyCandidateIndex = ReadExpectedIntOrDefault(Fixture, TEXT("apply_candidate_index"), -1);
		TestTrue(*FString::Printf(TEXT("%s candidate index valid"), *FixtureName), InitialResult.Candidates.IsValidIndex(ApplyCandidateIndex));
		if (!InitialResult.Candidates.IsValidIndex(ApplyCandidateIndex))
		{
			return false;
		}

		const FWBCardActivationCommandResult ApplyResult =
			WBEffectRunner::ApplyCardActivationCommand(State, InitialResult.Candidates[ApplyCandidateIndex].Command);
		TestTrue(*FString::Printf(TEXT("%s apply candidate succeeds"), *FixtureName), ApplyResult.bOk);

		FString ExpectReason;
		TestTrue(*FString::Printf(TEXT("%s apply trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
		if (FixtureName.Contains(TEXT("resets_on_turn_start")))
		{
			int32 PlayerId = -1;
			FString UsageKey;
			bool bExpectedMarkedAfterReset = false;
			TestTrue(*FString::Printf(TEXT("%s usage expectation reads"), *FixtureName), ReadExpectedUsage(Fixture, PlayerId, UsageKey, bExpectedMarkedAfterReset));
			TestTrue(*FString::Printf(TEXT("%s usage marked after apply"), *FixtureName), State.HasActivationUsageKeyThisTurn(PlayerId, UsageKey));
		}
		else
		{
			ExpectUsageState(*this, FixtureName, Fixture, State);
		}

		const FWBCardActivationCandidateGenerationResult PostApplyResult =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
		TestTrue(*FString::Printf(TEXT("%s post-apply generation ok"), *FixtureName), PostApplyResult.bOk);
		TestEqual(*FString::Printf(TEXT("%s post-apply candidate count"), *FixtureName), PostApplyResult.Candidates.Num(), ReadExpectedIntOrDefault(Fixture, TEXT("post_apply_candidate_count"), -1));

		const int32 ExpectedPostTurnStartCount = ReadExpectedIntOrDefault(Fixture, TEXT("post_turn_start_candidate_count"), -1);
		if (ExpectedPostTurnStartCount >= 0)
		{
			const int32 TurnStartPlayerId = ReadExpectedIntOrDefault(Fixture, TEXT("turn_start_player_id"), -1);
			const int32 TurnStartRoll = ReadExpectedIntOrDefault(Fixture, TEXT("turn_start_mp_roll"), -1);
			FString ResetReason;
			TestTrue(
				*FString::Printf(TEXT("%s turn-start resource setup: %s"), *FixtureName, *ResetReason),
				State.ApplyTurnStartResourceSetupForPlayer(TurnStartPlayerId, TurnStartRoll, ResetReason));
			ExpectUsageState(*this, FixtureName, Fixture, State);

			const FWBCardActivationCandidateGenerationResult PostResetResult =
				WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
			TestTrue(*FString::Printf(TEXT("%s post-reset generation ok"), *FixtureName), PostResetResult.bOk);
			TestEqual(*FString::Printf(TEXT("%s post-reset candidate count"), *FixtureName), PostResetResult.Candidates.Num(), ExpectedPostTurnStartCount);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationUsageLegalActionFixtureTest, "Wandbound.Core.CardActivationUsageMarking.LegalActionFamilyFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationUsageLegalActionFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("card_activation_usage_preserved_through_legal_action_family.json");
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Legal action usage fixture loads"), LoadUsageFixture(FixtureName, Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(TEXT("State builds"), BuildGameStateFromFixture(Fixture, State, StateReason));

	TArray<FWBCardActivationCandidateSource> Sources;
	FString ParseReason;
	TestTrue(TEXT("Sources parse"), ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);
	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Candidate count"), CandidateResult.Candidates.Num(), 1);
	ExpectUsageCommitMatchesFixture(*this, FixtureName, CandidateResult.Candidates[0].Command, Fixture);

	const FWBCardActivationLegalActionGenerationResult LegalResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	TestTrue(TEXT("Legal generation ok"), LegalResult.bOk);
	CompareStringArrays(*this, TEXT("Legal action ids"), LegalActivationActionIds(LegalResult), ExpectedStringArray(Fixture, TEXT("activation_action_ids")));
	TestEqual(TEXT("Legal action count"), LegalResult.ActionSet.Actions.Num(), ReadExpectedIntOrDefault(Fixture, TEXT("activation_action_count"), -1));
	ExpectUsageCommitMatchesFixture(*this, FixtureName, LegalResult.ActionSet.Actions[0].Command, Fixture);
	ExpectUsageState(*this, FixtureName, Fixture, State);

	FWBApplyActionResult OperationResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString OperationReason;
	TestTrue(TEXT("Fixture legal action operation applies"), ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
	TestEqual(TEXT("Operation kind"), OperationKind, EWBFixtureOperationKind::GenerateCardActivationLegalActions);
	TestTrue(TEXT("Operation ok"), OperationResult.bOk);
	ExpectUsageState(*this, FixtureName, Fixture, State);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationUsageRulesAndCodecGuardTest, "Wandbound.Core.CardActivationUsageMarking.RulesValidationAndCodecGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationUsageRulesAndCodecGuardTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeUsageState();
	const FWBCardActivationCommand Command = MakeUsageCommand();

	const FWBActionQueryResult Query = WBRules::CanApplyCardActivationCommand(State, Command);
	TestTrue(TEXT("Rules validation accepts command"), Query.bOk);
	TestFalse(TEXT("Rules validation does not mark usage"), State.HasActivationUsageKeyThisTurn(0, TEXT("fixture_usage_rules_key")));

	const FWBCardActivationCommandResult ApplyResult = WBEffectRunner::ApplyCardActivationCommand(State, Command);
	TestTrue(TEXT("Effect runner applies command"), ApplyResult.bOk);
	TestTrue(TEXT("Effect runner marks usage"), State.HasActivationUsageKeyThisTurn(0, TEXT("fixture_usage_rules_key")));

	const TArray<FString> FWBActionIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	for (const FString& ActionId : FWBActionIds)
	{
		TestFalse(*FString::Printf(TEXT("FWBAction id is not activation %s"), *ActionId), ActionId.StartsWith(TEXT("activate:")));
	}

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec remains activation-free"), ActionCodecSource.Contains(TEXT("activate:")));

	return true;
}

#endif
