#include "WBUnitStatQuery.h"

#include "WBBoardGeometry.h"
#include "WBCharacterPassiveEligibility.h"
#include "WBRules.h"
#include "WBTerrainRules.h"

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
	const FWBGridGeometryProfile Geometry =
		WBRules::GetAttackGeometryProfile(
			State, &Repository, Source.UnitId);
	const FWBGeometryLineQueryResult Line = WBBoardGeometry::QueryLine(
		State, Geometry, From, To,
		!Aura.bBlockedByWalls, Aura.bBlockedByUnits);
	if (!Line.bOk || Line.Distance <= 0
		|| Line.Distance > WBUnitStatQuery::GetAuraRangeAR(State, Source.UnitId))
	{
		return false;
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
	Result.EffectiveValue = GetIntrinsicAR(State, UnitId);
	Result.AppliedModifierCount = Result.EffectiveValue != Result.StoredValue
		? 1 : 0;
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
		if (Unit.UnitId != UnitId
			&& Unit.GetControllerPlayerIdForRules()
				!= Target->GetControllerPlayerIdForRules()
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
	return FMath::Max(0, GetIntrinsicAR(State, UnitId));
}

int32 WBUnitStatQuery::GetIntrinsicAR(
	const FWBGameStateData& State,
	const int32 UnitId)
{
	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	if (Unit == nullptr)
	{
		return 0;
	}
	return FMath::Max(
		0,
		Unit->AR + WBTerrainRules::GetOccupantARModifier(
			State.GetTerrainAt(FWBTile(Unit->X, Unit->Y))));
}
