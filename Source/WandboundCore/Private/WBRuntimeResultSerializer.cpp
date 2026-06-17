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
