#include "WBCharacterConstruction.h"

int32 WBCharacterConstruction::AllocateNextUnitId(
	const FWBGameStateData& State)
{
	int32 MaxUnitId = INDEX_NONE;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId == MAX_int32)
		{
			return INDEX_NONE;
		}
		MaxUnitId = FMath::Max(MaxUnitId, Unit.UnitId);
	}
	return MaxUnitId + 1;
}

bool WBCharacterConstruction::IsValidCharacterDefinition(
	const FWBCardDefinition& Definition)
{
	return Definition.Kind == EWBCardDefinitionKind::Character
		&& Definition.CharacterStats.HP > 0
		&& Definition.CharacterStats.ATK >= 0
		&& Definition.CharacterStats.AR >= 0
		&& Definition.CharacterStats.RL >= 0;
}

FWBCharacterConstructionResult WBCharacterConstruction::BuildUnit(
	const FWBCardDefinition& Definition,
	const FWBCharacterConstructionRequest& Request)
{
	FWBCharacterConstructionResult Result;
	if (Request.UnitId < 0
		|| !FWBGameStateData::IsValidPlayerId(Request.OwnerPlayerId)
		|| !FWBGameStateData::IsValidPlayerId(Request.ControllerPlayerId)
		|| Request.CardId.IsEmpty()
		|| Definition.CardId != Request.CardId)
	{
		Result.Reason = TEXT("character_construction_context_invalid");
		return Result;
	}
	if (!IsValidCharacterDefinition(Definition))
	{
		Result.Reason = Definition.Kind == EWBCardDefinitionKind::Character
			? FString(TEXT("invalid_character_stats"))
			: FString(TEXT("source_card_not_character"));
		return Result;
	}

	FWBUnitState Unit;
	Unit.UnitId = Request.UnitId;
	Unit.SetOwnerAndControllerForRules(
		Request.OwnerPlayerId, Request.ControllerPlayerId);
	Unit.CardId = Request.CardId;
	Unit.X = Request.Tile.X;
	Unit.Y = Request.Tile.Y;
	Unit.HP = Definition.CharacterStats.HP;
	Unit.MaxHP = Definition.CharacterStats.HP;
	Unit.ATK = Definition.CharacterStats.ATK;
	Unit.AR = Definition.CharacterStats.AR;
	Unit.SetCanonicalRL(
		Definition.CharacterStats.RL,
		Definition.CharacterStats.RL,
		0);
	Unit.AttacksLeft = 0;
	Unit.MaxAttacksPerTurn = 1;
	Unit.MPRemaining = 0;

	Result.bOk = true;
	Result.Unit = MoveTemp(Unit);
	return Result;
}
