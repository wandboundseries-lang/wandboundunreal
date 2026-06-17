#include "WBRuntimeResultSerializer.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBReplayTrace.h"

namespace
{
TSharedRef<FJsonObject> PublicPlayerTurnSummaryToJsonObject(const FWBPublicPlayerTurnSummary& Player)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("player_id"), Player.PlayerId);
	Object->SetNumberField(TEXT("remaining_mp"), Player.RemainingMP);
	Object->SetNumberField(TEXT("last_mp_roll"), Player.LastMPRoll);
	Object->SetNumberField(TEXT("walls_left"), Player.WallsLeft);
	Object->SetNumberField(TEXT("wall_removals_left"), Player.WallRemovalsLeft);
	return Object;
}

TSharedRef<FJsonObject> PublicTurnSummaryToJsonObject(const FWBPublicTurnSummary& Summary)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("current_player_id"), Summary.CurrentPlayerId);
	Object->SetNumberField(TEXT("priority_player_id"), Summary.PriorityPlayerId);
	Object->SetNumberField(TEXT("turn_number"), Summary.TurnNumber);
	Object->SetStringField(TEXT("phase"), Summary.Phase.ToString());
	Object->SetBoolField(TEXT("game_over"), Summary.bGameOver);

	TArray<TSharedPtr<FJsonValue>> Players;
	Players.Reserve(Summary.Players.Num());
	for (const FWBPublicPlayerTurnSummary& Player : Summary.Players)
	{
		Players.Add(MakeShared<FJsonValueObject>(PublicPlayerTurnSummaryToJsonObject(Player)));
	}
	Object->SetArrayField(TEXT("players"), MoveTemp(Players));
	return Object;
}

TSharedRef<FJsonObject> PublicUnitStatusSummaryToJsonObject(const FWBPublicUnitStatusSummary& Status)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("status_id"), Status.StatusId.GetPlainNameString());
	Object->SetNumberField(TEXT("turns_remaining"), Status.TurnsRemaining);
	return Object;
}

TSharedRef<FJsonObject> PublicUnitBoardSummaryToJsonObject(const FWBPublicUnitBoardSummary& Unit)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("unit_id"), Unit.UnitId);
	Object->SetNumberField(TEXT("owner_id"), Unit.OwnerId);
	Object->SetStringField(TEXT("card_id"), Unit.CardId);
	Object->SetNumberField(TEXT("x"), Unit.X);
	Object->SetNumberField(TEXT("y"), Unit.Y);
	Object->SetNumberField(TEXT("hp"), Unit.HP);
	Object->SetNumberField(TEXT("max_hp"), Unit.MaxHP);
	Object->SetNumberField(TEXT("atk"), Unit.ATK);
	Object->SetNumberField(TEXT("ar"), Unit.AR);
	Object->SetNumberField(TEXT("rl_total"), Unit.RLTotal);
	Object->SetNumberField(TEXT("rl_used"), Unit.RLUsed);
	Object->SetNumberField(TEXT("attacks_left"), Unit.AttacksLeft);

	TArray<TSharedPtr<FJsonValue>> Statuses;
	Statuses.Reserve(Unit.Statuses.Num());
	for (const FWBPublicUnitStatusSummary& Status : Unit.Statuses)
	{
		Statuses.Add(MakeShared<FJsonValueObject>(PublicUnitStatusSummaryToJsonObject(Status)));
	}
	Object->SetArrayField(TEXT("statuses"), MoveTemp(Statuses));
	return Object;
}

TSharedRef<FJsonObject> PublicBoardSummaryToJsonObject(const FWBPublicBoardSummary& Summary)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("board_width"), Summary.BoardWidth);
	Object->SetNumberField(TEXT("board_height"), Summary.BoardHeight);

	TArray<TSharedPtr<FJsonValue>> Units;
	Units.Reserve(Summary.Units.Num());
	for (const FWBPublicUnitBoardSummary& Unit : Summary.Units)
	{
		Units.Add(MakeShared<FJsonValueObject>(PublicUnitBoardSummaryToJsonObject(Unit)));
	}
	Object->SetArrayField(TEXT("units"), MoveTemp(Units));
	return Object;
}

TArray<TSharedPtr<FJsonValue>> TraceEventsToJsonValues(const TArray<FWBTraceEvent>& TraceEvents)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(TraceEvents.Num());
	for (const FWBTraceEvent& TraceEvent : TraceEvents)
	{
		Values.Add(MakeShared<FJsonValueObject>(WBReplayTrace::TraceEventToJsonObject(TraceEvent)));
	}

	return Values;
}
}

TSharedRef<FJsonObject> WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonObject(
	const FWBRuntimeSelectedActionResult& Result)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), RuntimeSelectedActionResultSchemaVersion);

	TSharedRef<FJsonObject> SelectedAction = MakeShared<FJsonObject>();
	SelectedAction->SetStringField(TEXT("type"), Result.SelectedActionType.ToString());
	SelectedAction->SetStringField(TEXT("action_id"), Result.SelectedActionId);
	Root->SetObjectField(TEXT("selected_action"), SelectedAction);

	TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetBoolField(TEXT("ok"), Result.ApplyResult.bOk);
	ResultObject->SetStringField(TEXT("reason"), Result.ApplyResult.Reason);
	Root->SetObjectField(TEXT("result"), ResultObject);

	TSharedRef<FJsonObject> MPRoll = MakeShared<FJsonObject>();
	MPRoll->SetBoolField(TEXT("consumed"), Result.bConsumedMPRoll);
	MPRoll->SetNumberField(TEXT("value"), Result.ConsumedMPRoll);
	Root->SetObjectField(TEXT("mp_roll"), MPRoll);

	Root->SetArrayField(TEXT("traces"), TraceEventsToJsonValues(Result.ApplyResult.TraceEvents));
	Root->SetObjectField(
		TEXT("final_public_turn_summary"),
		PublicTurnSummaryToJsonObject(Result.FinalPublicTurnSummary));
	Root->SetObjectField(
		TEXT("final_public_board_summary"),
		PublicBoardSummaryToJsonObject(Result.FinalPublicBoardSummary));

	return Root;
}

bool WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(
	const FWBRuntimeSelectedActionResult& Result,
	FString& OutJson)
{
	OutJson.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(RuntimeSelectedActionResultToJsonObject(Result), Writer);
}
