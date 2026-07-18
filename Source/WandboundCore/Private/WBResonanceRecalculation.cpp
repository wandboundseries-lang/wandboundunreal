#include "WBResonanceRecalculation.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBCardZoneState.h"

namespace
{
FWBResonanceRecalculationResult MakeFailure(
	const int32 UnitId,
	const TCHAR* Reason)
{
	FWBResonanceRecalculationResult Result;
	Result.UnitId = UnitId;
	Result.FailureReason = Reason;
	return Result;
}

int32 SlotSortRank(const FString& SlotId)
{
	if (SlotId == TEXT("wand"))
	{
		return 0;
	}

	const FString Prefix = TEXT("wand_");
	if (SlotId.StartsWith(Prefix))
	{
		const FString Suffix = SlotId.RightChop(Prefix.Len());
		if (Suffix.IsNumeric())
		{
			return FCString::Atoi(*Suffix);
		}
	}

	return 1000000;
}

bool SortEquippedEntriesForRecalculation(
	const FWBEquippedCardEntry& A,
	const FWBEquippedCardEntry& B)
{
	if (A.EquipOrder != B.EquipOrder)
	{
		return A.EquipOrder < B.EquipOrder;
	}

	const int32 SlotRankA = SlotSortRank(A.SlotId);
	const int32 SlotRankB = SlotSortRank(B.SlotId);
	if (SlotRankA != SlotRankB)
	{
		return SlotRankA < SlotRankB;
	}

	if (A.SlotId != B.SlotId)
	{
		return A.SlotId < B.SlotId;
	}

	if (A.Card.InstanceId != B.Card.InstanceId)
	{
		return A.Card.InstanceId < B.Card.InstanceId;
	}

	return A.Card.CardId < B.Card.CardId;
}

bool SortResonanceModifiersForRecalculation(
	const FWBResonanceModifierState& A,
	const FWBResonanceModifierState& B)
{
	if (A.SourceId != B.SourceId)
	{
		return A.SourceId < B.SourceId;
	}

	const uint8 TargetA = static_cast<uint8>(A.Target);
	const uint8 TargetB = static_cast<uint8>(B.Target);
	if (TargetA != TargetB)
	{
		return TargetA < TargetB;
	}

	const uint8 OperationA = static_cast<uint8>(A.Operation);
	const uint8 OperationB = static_cast<uint8>(B.Operation);
	if (OperationA != OperationB)
	{
		return OperationA < OperationB;
	}

	return A.Amount < B.Amount;
}

FWBResonanceRecalculationResult CalculateUnitInternal(
	const FWBGameStateData& State,
	const int32 UnitId,
	const FWBCardDefinitionRepository& Repository)
{
	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	if (Unit == nullptr || !Unit->IsUnitOnBoard())
	{
		return MakeFailure(UnitId, TEXT("unit_not_found"));
	}

	FWBResonanceRecalculationResult Result;
	Result.UnitId = Unit->UnitId;
	Result.OwnerPlayerId = Unit->OwnerId;
	Result.PreviousBaseRL = Unit->GetBaseRLForRules();
	Result.PreviousCurrentRL = Unit->GetCurrentRLForRules();
	Result.PreviousRLUsed = Unit->RLUsed;

	if (!FWBGameStateData::IsValidPlayerId(Unit->OwnerId)
		|| State.GetPlayerById(Unit->OwnerId) == nullptr)
	{
		Result.FailureReason = TEXT("invalid_unit_owner");
		return Result;
	}

	if (Result.PreviousBaseRL < 0 || Result.PreviousCurrentRL < 0 || Unit->RLUsed < 0)
	{
		Result.FailureReason = TEXT("invalid_rl_state");
		return Result;
	}

	TSet<FString> SeenEquippedInstanceIds;
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		const FWBUnitState* EquippedUnit = State.GetUnitById(Entry.EquippedToUnitId);
		if (EquippedUnit == nullptr || !EquippedUnit->IsUnitOnBoard())
		{
			Result.FailureReason = TEXT("equipped_unit_not_found");
			return Result;
		}

		if (Entry.Card.InstanceId.IsEmpty())
		{
			Result.FailureReason = TEXT("equipped_instance_id_missing");
			return Result;
		}

		if (SeenEquippedInstanceIds.Contains(Entry.Card.InstanceId))
		{
			Result.FailureReason = TEXT("duplicate_card_attachment");
			return Result;
		}
		SeenEquippedInstanceIds.Add(Entry.Card.InstanceId);
	}

	FString ZoneStateReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(State.GetCardZoneState(), ZoneStateReason))
	{
		Result.FailureReason = ZoneStateReason.IsEmpty()
			? FString(TEXT("zone_state_invalid"))
			: ZoneStateReason;
		return Result;
	}

	const int32 RecalculatedBaseRL = Result.PreviousBaseRL;
	TArray<FWBResonanceModifierState> SortedModifiers = Unit->ResonanceModifiers;
	SortedModifiers.Sort(SortResonanceModifiersForRecalculation);

	int64 RecalculatedCurrentRL64 = RecalculatedBaseRL;
	for (const FWBResonanceModifierState& Modifier : SortedModifiers)
	{
		if (Modifier.SourceId.IsEmpty())
		{
			Result.FailureReason = TEXT("invalid_modifier_source");
			return Result;
		}

		if (Modifier.Target != EWBResonanceModifierTarget::CurrentRL)
		{
			Result.FailureReason = TEXT("unsupported_modifier_target");
			return Result;
		}

		if (Modifier.Operation != EWBResonanceModifierOperation::Add)
		{
			Result.FailureReason = TEXT("unsupported_modifier_operation");
			return Result;
		}

		RecalculatedCurrentRL64 += Modifier.Amount;
		if (RecalculatedCurrentRL64 > TNumericLimits<int32>::Max()
			|| RecalculatedCurrentRL64 < TNumericLimits<int32>::Min())
		{
			Result.FailureReason = TEXT("current_rl_overflow");
			return Result;
		}

		Result.OrderedModifierSourceIds.Add(Modifier.SourceId);
	}

	const int32 RecalculatedCurrentRL = static_cast<int32>(RecalculatedCurrentRL64);
	if (RecalculatedCurrentRL < 0)
	{
		Result.FailureReason = TEXT("invalid_rl_state");
		return Result;
	}

	TArray<FWBEquippedCardEntry> EquippedEntriesForUnit;
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		if (Entry.EquippedToUnitId == UnitId)
		{
			EquippedEntriesForUnit.Add(Entry);
		}
	}
	EquippedEntriesForUnit.Sort(SortEquippedEntriesForRecalculation);

	int64 RecalculatedRLUsed64 = 0;
	for (const FWBEquippedCardEntry& Entry : EquippedEntriesForUnit)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
		if (!Lookup.bFound)
		{
			Result.FailureReason = TEXT("card_definition_not_found");
			return Result;
		}

		if (Lookup.Definition.Kind != EWBCardDefinitionKind::Wand)
		{
			Result.FailureReason = TEXT("equipped_card_not_wand");
			return Result;
		}

		const int32 RR = Lookup.Definition.WandStats.RR;
		if (RR < 0)
		{
			Result.FailureReason = TEXT("invalid_rr");
			return Result;
		}

		RecalculatedRLUsed64 += RR;
		if (RecalculatedRLUsed64 > TNumericLimits<int32>::Max())
		{
			Result.FailureReason = TEXT("rl_used_overflow");
			return Result;
		}

		Result.OrderedEquippedCardInstanceIds.Add(Entry.Card.InstanceId);
	}

	Result.bSucceeded = true;
	Result.FailureReason = TEXT("success");
	Result.RecalculatedBaseRL = RecalculatedBaseRL;
	Result.RecalculatedCurrentRL = RecalculatedCurrentRL;
	Result.RecalculatedRLUsed = static_cast<int32>(RecalculatedRLUsed64);
	Result.AvailableRL = Result.RecalculatedCurrentRL - Result.RecalculatedRLUsed;
	Result.bIsOverflowing = Result.AvailableRL < 0;
	return Result;
}

TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	JsonValues.Reserve(Values.Num());
	for (const FString& Value : Values)
	{
		JsonValues.Add(MakeShared<FJsonValueString>(Value));
	}
	return JsonValues;
}
}

FWBResonanceRecalculationResult WBResonanceRecalculation::CalculateUnit(
	const FWBGameStateData& State,
	const int32 UnitId,
	const FWBCardDefinitionRepository& Repository)
{
	return CalculateUnitInternal(State, UnitId, Repository);
}

FWBResonanceRecalculationResult WBResonanceRecalculation::RecalculateUnit(
	FWBGameStateData& State,
	const int32 UnitId,
	const FWBCardDefinitionRepository& Repository)
{
	const FWBResonanceRecalculationResult Result = CalculateUnitInternal(State, UnitId, Repository);
	if (!Result.bSucceeded)
	{
		return Result;
	}

	FWBUnitState* Unit = State.GetMutableUnitById(UnitId);
	if (Unit == nullptr || !Unit->IsUnitOnBoard())
	{
		return MakeFailure(UnitId, TEXT("unit_not_found"));
	}

	Unit->SetCanonicalRL(
		Result.RecalculatedBaseRL,
		Result.RecalculatedCurrentRL,
		Result.RecalculatedRLUsed);
	return Result;
}

TArray<FWBResonanceRecalculationResult> WBResonanceRecalculation::RecalculateAllUnits(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	TArray<int32> UnitIds;
	UnitIds.Reserve(State.Units.Num());
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard())
		{
			UnitIds.Add(Unit.UnitId);
		}
	}

	UnitIds.Sort();

	TArray<FWBResonanceRecalculationResult> Results;
	Results.Reserve(UnitIds.Num());
	for (const int32 UnitId : UnitIds)
	{
		Results.Add(RecalculateUnit(State, UnitId, Repository));
	}
	return Results;
}

bool WBResonanceRecalculation::RecalculationResultToJsonStringForTest(
	const FWBResonanceRecalculationResult& Result,
	FString& OutJson)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("succeeded"), Result.bSucceeded);
	Object->SetStringField(TEXT("reason"), Result.FailureReason);
	Object->SetNumberField(TEXT("unit_id"), Result.UnitId);
	Object->SetNumberField(TEXT("owner_player_id"), Result.OwnerPlayerId);
	Object->SetNumberField(TEXT("previous_base_rl"), Result.PreviousBaseRL);
	Object->SetNumberField(TEXT("previous_current_rl"), Result.PreviousCurrentRL);
	Object->SetNumberField(TEXT("previous_rl_used"), Result.PreviousRLUsed);
	Object->SetNumberField(TEXT("base_rl"), Result.RecalculatedBaseRL);
	Object->SetNumberField(TEXT("current_rl"), Result.RecalculatedCurrentRL);
	Object->SetNumberField(TEXT("rl_used"), Result.RecalculatedRLUsed);
	Object->SetNumberField(TEXT("available_rl"), Result.AvailableRL);
	Object->SetBoolField(TEXT("overflowing"), Result.bIsOverflowing);
	Object->SetArrayField(
		TEXT("modifier_sources"),
		StringArrayToJsonValues(Result.OrderedModifierSourceIds));
	Object->SetArrayField(
		TEXT("equipped_card_instance_ids"),
		StringArrayToJsonValues(Result.OrderedEquippedCardInstanceIds));

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object, Writer);
}
