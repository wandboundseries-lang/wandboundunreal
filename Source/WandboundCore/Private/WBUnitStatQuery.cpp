#include "WBUnitStatQuery.h"

#include "WBCharacterPassiveEligibility.h"
#include "WBRules.h"

namespace
{
bool HasStrictAttackLine(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBUnitState& Source,
	const FWBUnitState& Target,
	const FWBContinuousStatAuraDefinition& Aura)
{
	const FWBTile From(Source.X, Source.Y);
	const FWBTile To(Target.X, Target.Y);
	const int32 DeltaX = FMath::Abs(To.X - From.X);
	const int32 DeltaY = FMath::Abs(To.Y - From.Y);
	const bool bOrthogonal = (DeltaX == 0) != (DeltaY == 0);
	const bool bDiagonal = DeltaX > 0 && DeltaX == DeltaY
		&& WBRules::UnitHasCombatCapability(
			State, &Repository, Source.UnitId,
			EWBCombatCapability::AttacksDiagonally);
	if (!bOrthogonal && !bDiagonal)
	{
		return false;
	}

	const int32 Distance = bDiagonal ? DeltaX : DeltaX + DeltaY;
	if (Distance <= 0
		|| Distance > WBUnitStatQuery::GetAuraRangeAR(State, Source.UnitId))
	{
		return false;
	}

	const int32 StepX = DeltaX == 0 ? 0 : (To.X > From.X ? 1 : -1);
	const int32 StepY = DeltaY == 0 ? 0 : (To.Y > From.Y ? 1 : -1);
	FWBTile Current = From;
	while (Current != To)
	{
		const FWBTile Next(Current.X + StepX, Current.Y + StepY);
		if (Aura.bBlockedByWalls)
		{
			if (StepX != 0 && StepY != 0)
			{
				const FWBTile Horizontal(Current.X + StepX, Current.Y);
				const FWBTile Vertical(Current.X, Current.Y + StepY);
				if (WBRules::HasWallBetween(State, Current, Horizontal)
					|| WBRules::HasWallBetween(State, Current, Vertical))
				{
					return false;
				}
			}
			else if (WBRules::HasWallBetween(State, Current, Next))
			{
				return false;
			}
		}
		if (Aura.bBlockedByUnits && Next != To && State.UnitIdAt(Next) != -1)
		{
			return false;
		}
		Current = Next;
	}
	return true;
}
}

FWBEffectiveUnitStatResult WBUnitStatQuery::GetEffectiveAR(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 UnitId)
{
	FWBEffectiveUnitStatResult Result;
	const FWBUnitState* Target = State.GetUnitById(UnitId);
	if (Target == nullptr)
	{
		Result.Reason = TEXT("unit_missing");
		return Result;
	}

	Result.StoredValue = Target->AR;
	Result.EffectiveValue = Target->AR;
	if (!Target->IsUnitOnBoard() || Target->bDefeated)
	{
		Result.bOk = true;
		return Result;
	}
	if (WBRules::UnitHasCombatCapability(
		State, &Repository, UnitId,
		EWBCombatCapability::ImmuneToEnemyEffects))
	{
		Result.bOk = true;
		return Result;
	}

	TArray<const FWBUnitState*> Sources;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId != UnitId && Unit.OwnerId != Target->OwnerId
			&& WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(Unit))
		{
			Sources.Add(&Unit);
		}
	}
	Sources.Sort([](const FWBUnitState& A, const FWBUnitState& B)
	{
		return A.UnitId < B.UnitId;
	});

	int32 MinimumResult = 0;
	for (const FWBUnitState* Source : Sources)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Source->CardId);
		if (!Lookup.bFound)
		{
			continue;
		}
		TArray<FWBContinuousStatAuraDefinition> Auras =
			Lookup.Definition.ContinuousStatAuras;
		Auras.Sort([](const FWBContinuousStatAuraDefinition& A,
			const FWBContinuousStatAuraDefinition& B)
		{
			return A.AuraId < B.AuraId;
		});
		for (const FWBContinuousStatAuraDefinition& Aura : Auras)
		{
			if (Aura.TargetRelation != EWBContinuousAuraTargetRelation::Enemy
				|| Aura.TargetStat != EWBContinuousStat::AR
				|| Aura.Operation != EWBContinuousStatOperation::Add
				|| Aura.RangeStat != EWBContinuousAuraRangeStat::AR
				|| Aura.Geometry != EWBContinuousAuraGeometry::AttackLine
				|| !HasStrictAttackLine(State, Repository, *Source, *Target, Aura))
			{
				continue;
			}
			Result.EffectiveValue += Aura.Amount;
			MinimumResult = FMath::Max(MinimumResult, Aura.MinimumResult);
			++Result.AppliedModifierCount;
		}
	}
	Result.EffectiveValue = FMath::Max(MinimumResult, Result.EffectiveValue);
	Result.bOk = true;
	return Result;
}

int32 WBUnitStatQuery::GetAuraRangeAR(
	const FWBGameStateData& State,
	const int32 UnitId)
{
	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	return Unit != nullptr ? FMath::Max(0, Unit->AR) : 0;
}
