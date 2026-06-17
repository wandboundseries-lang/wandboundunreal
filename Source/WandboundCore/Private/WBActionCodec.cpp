#include "WBActionCodec.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool IsSetTile(const FWBTile& Tile)
{
	return Tile.X != -1 || Tile.Y != -1;
}

bool TryGetIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
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

TSharedRef<FJsonObject> ActionToJsonObject(const FWBAction& Action)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("id"), WBActionCodec::MakeActionId(Action));
	Object->SetStringField(TEXT("kind"), WBActionCodec::GetActionKind(Action.Type));
	Object->SetNumberField(TEXT("player_id"), Action.PlayerId);

	if (Action.SourceUnitId != -1)
	{
		Object->SetNumberField(TEXT("unit_id"), Action.SourceUnitId);
	}

	if (Action.TargetUnitId != -1)
	{
		Object->SetNumberField(TEXT("target_unit_id"), Action.TargetUnitId);
	}

	if (IsSetTile(Action.FromTile))
	{
		Object->SetNumberField(TEXT("from_x"), Action.FromTile.X);
		Object->SetNumberField(TEXT("from_y"), Action.FromTile.Y);
	}

	if (IsSetTile(Action.ToTile))
	{
		Object->SetNumberField(TEXT("to_x"), Action.ToTile.X);
		Object->SetNumberField(TEXT("to_y"), Action.ToTile.Y);
	}

	return Object;
}

FWBActionDecodeResult DecodeFailure(const FString& Reason)
{
	FWBActionDecodeResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

FWBActionDecodeResult DecodeActionObject(const TSharedPtr<FJsonObject>& Object, const FWBGameStateData& State)
{
	if (!Object.IsValid())
	{
		return DecodeFailure(TEXT("missing_chosen_action"));
	}

	FString Kind;
	if (!Object->TryGetStringField(TEXT("kind"), Kind))
	{
		return DecodeFailure(TEXT("missing_action_kind"));
	}

	FWBAction Action;
	if (!TryGetIntegerField(Object, TEXT("player_id"), Action.PlayerId))
	{
		return DecodeFailure(TEXT("missing_action_player_id"));
	}

	if (Kind == TEXT("move"))
	{
		Action.Type = EWBActionType::Move;
		if (!TryGetIntegerField(Object, TEXT("unit_id"), Action.SourceUnitId))
		{
			return DecodeFailure(TEXT("missing_move_unit_id"));
		}

		if (!TryGetIntegerField(Object, TEXT("to_x"), Action.ToTile.X)
			|| !TryGetIntegerField(Object, TEXT("to_y"), Action.ToTile.Y))
		{
			return DecodeFailure(TEXT("missing_move_destination"));
		}

		const bool bHasFromX = Object->HasField(TEXT("from_x"));
		const bool bHasFromY = Object->HasField(TEXT("from_y"));
		if (bHasFromX != bHasFromY)
		{
			return DecodeFailure(TEXT("incomplete_move_source_tile"));
		}

		if (bHasFromX)
		{
			if (!TryGetIntegerField(Object, TEXT("from_x"), Action.FromTile.X)
				|| !TryGetIntegerField(Object, TEXT("from_y"), Action.FromTile.Y))
			{
				return DecodeFailure(TEXT("invalid_move_source_tile"));
			}
		}
		else
		{
			const FWBUnitState* Unit = State.GetUnitById(Action.SourceUnitId);
			if (Unit == nullptr)
			{
				return DecodeFailure(TEXT("missing_move_source_tile"));
			}

			Action.FromTile = FWBTile(Unit->X, Unit->Y);
		}
	}
	else if (Kind == TEXT("attack"))
	{
		Action.Type = EWBActionType::Attack;
		if (!TryGetIntegerField(Object, TEXT("unit_id"), Action.SourceUnitId))
		{
			return DecodeFailure(TEXT("missing_attack_unit_id"));
		}

		if (!TryGetIntegerField(Object, TEXT("target_unit_id"), Action.TargetUnitId))
		{
			return DecodeFailure(TEXT("missing_attack_target_unit_id"));
		}

		const FWBUnitState* Attacker = State.GetUnitById(Action.SourceUnitId);
		if (Attacker != nullptr)
		{
			Action.FromTile = FWBTile(Attacker->X, Attacker->Y);
		}

		const FWBUnitState* Target = State.GetUnitById(Action.TargetUnitId);
		if (Target != nullptr)
		{
			Action.ToTile = FWBTile(Target->X, Target->Y);
		}
	}
	else if (Kind == TEXT("end_turn"))
	{
		Action.Type = EWBActionType::EndTurn;
	}
	else if (Kind == TEXT("pass"))
	{
		Action.Type = EWBActionType::Pass;
	}
	else if (Kind == TEXT("pass_response"))
	{
		Action.Type = EWBActionType::PassResponse;
	}
	else
	{
		return DecodeFailure(FString::Printf(TEXT("unsupported_action_kind:%s"), *Kind));
	}

	FString EncodedActionId;
	if (Object->TryGetStringField(TEXT("id"), EncodedActionId) && EncodedActionId != WBActionCodec::MakeActionId(Action))
	{
		return DecodeFailure(TEXT("action_id_mismatch"));
	}

	FWBActionDecodeResult Result;
	Result.bOk = true;
	Result.Action = Action;
	return Result;
}
}

FString WBActionCodec::GetActionKind(const EWBActionType Type)
{
	switch (Type)
	{
	case EWBActionType::Move:
		return TEXT("move");
	case EWBActionType::Attack:
		return TEXT("attack");
	case EWBActionType::EndTurn:
		return TEXT("end_turn");
	case EWBActionType::Pass:
		return TEXT("pass");
	case EWBActionType::PassResponse:
		return TEXT("pass_response");
	default:
		return TEXT("none");
	}
}

FString WBActionCodec::MakeActionId(const FWBAction& Action)
{
	switch (Action.Type)
	{
	case EWBActionType::Move:
		return FString::Printf(
			TEXT("move:p%d:u%d:%d,%d->%d,%d"),
			Action.PlayerId,
			Action.SourceUnitId,
			Action.FromTile.X,
			Action.FromTile.Y,
			Action.ToTile.X,
			Action.ToTile.Y);
	case EWBActionType::Attack:
		return FString::Printf(
			TEXT("attack:p%d:u%d:t%d"),
			Action.PlayerId,
			Action.SourceUnitId,
			Action.TargetUnitId);
	case EWBActionType::EndTurn:
		return FString::Printf(TEXT("end_turn:p%d"), Action.PlayerId);
	case EWBActionType::Pass:
		return FString::Printf(TEXT("pass:p%d"), Action.PlayerId);
	case EWBActionType::PassResponse:
		return FString::Printf(TEXT("pass_response:p%d"), Action.PlayerId);
	default:
		return FString::Printf(TEXT("none:p%d"), Action.PlayerId);
	}
}

TArray<FString> WBActionCodec::MakeActionIds(const TArray<FWBAction>& Actions)
{
	TArray<FString> ActionIds;
	ActionIds.Reserve(Actions.Num());
	for (const FWBAction& Action : Actions)
	{
		ActionIds.Add(MakeActionId(Action));
	}

	return ActionIds;
}

FString WBActionCodec::SerializeAction(const FWBAction& Action)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(ActionToJsonObject(Action), Writer);
	return Output;
}

FWBActionDecodeResult WBActionCodec::DecodeAction(const FString& SerializedAction, const FWBGameStateData& State)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SerializedAction);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return DecodeFailure(TEXT("malformed_action"));
	}

	return DecodeActionObject(Object, State);
}
