#include "WBBattlePhase.h"

namespace
{
FWBTraceEvent MakeBattleBoundary(const FWBGameStateData& State, const FName Kind)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.PlayerId = State.BattlePhase.DeclaringPlayerId;
	Event.SourceUnitId = State.BattlePhase.OriginalAttackerUnitId;
	Event.TargetUnitId = State.BattlePhase.OriginalDefenderUnitId;
	Event.TurnNumber = State.BattlePhase.TurnNumber;
	Event.MatchPhase = FName(TEXT("battle"));
	Event.bOk = true;
	return Event;
}
}

bool WBBattlePhase::Validate(const FWBGameStateData& State, FString& OutReason)
{
	OutReason.Reset();
	const FWBBattlePhaseState& Battle = State.BattlePhase;
	const FWBPendingAttackState& Attack = State.PendingAttack;
	if (!Battle.bActive)
	{
		if (Attack.bActive && WBIsPlayerDeclared(Attack.AttackDeclaration))
		{
			OutReason = TEXT("player_attack_missing_battle");
			return false;
		}
		return true;
	}
	// Counter authority describes the current combatant, not the root declaration.
	if (State.bGameOver || !Attack.bActive
		|| (!Attack.bCounter && (Attack.AuthorityKind != EWBAttackAuthorityKind::Player
			|| !WBIsPlayerDeclared(Attack.AttackDeclaration)))
		|| Battle.BattleId.IsEmpty() || Battle.RootActionId.IsEmpty()
		|| Battle.RootContinuationId.IsEmpty()
		|| Battle.RootActionId != Attack.DeclarationActionId
		|| Battle.RootContinuationId != Attack.ContinuationId
		|| !FWBGameStateData::IsValidPlayerId(Battle.DeclaringPlayerId)
		|| Battle.DeclaringPlayerId != State.CurrentPlayer
		|| Battle.TurnNumber != State.TurnNumber
		|| Battle.OriginalAttackerUnitId < 0 || Battle.OriginalDefenderUnitId < 0
		|| Battle.OriginalAttackerUnitId == Battle.OriginalDefenderUnitId
		|| Battle.OriginalAttackerUnitId != Attack.OriginalAttackerUnitId
		|| Battle.OriginalDefenderUnitId != Attack.OriginalDefenderUnitId)
	{
		OutReason = TEXT("invalid_battle_phase_state");
		return false;
	}
	return true;
}

bool WBBattlePhase::Begin(FWBGameStateData& State, const int32 Generation,
	const int32 Revision, TArray<FWBTraceEvent>& OutEvents, FString& OutReason)
{
	const FWBPendingAttackState& Attack = State.PendingAttack;
	if (State.IsBattlePhaseActive() || State.bGameOver || !Attack.bActive
		|| Attack.bCounter || Attack.AuthorityKind != EWBAttackAuthorityKind::Player
		|| !WBIsPlayerDeclared(Attack.AttackDeclaration)
		|| Attack.Stage != EWBAttackContinuationStage::PreHit)
	{
		OutReason = TEXT("battle_declaration_invalid");
		return false;
	}
	FWBBattlePhaseState Battle;
	Battle.bActive = true;
	Battle.RootActionId = Attack.DeclarationActionId;
	Battle.RootContinuationId = Attack.ContinuationId;
	Battle.DeclaringPlayerId = Attack.AttackingPlayerId;
	Battle.OriginalAttackerUnitId = Attack.OriginalAttackerUnitId;
	Battle.OriginalDefenderUnitId = Attack.OriginalDefenderUnitId;
	Battle.TurnNumber = State.TurnNumber;
	Battle.BattleId = FString::Printf(TEXT("battle:g%d:r%d:t%d:%s"),
		Generation, Revision, State.TurnNumber, *Attack.ContinuationId);
	State.BattlePhase = MoveTemp(Battle);
	if (!Validate(State, OutReason))
	{
		State.ClearBattlePhase();
		return false;
	}
	OutEvents.Add(MakeBattleBoundary(State, FName(TEXT("battle_phase_started"))));
	return true;
}

void WBBattlePhase::End(FWBGameStateData& State, TArray<FWBTraceEvent>& OutEvents)
{
	if (!State.IsBattlePhaseActive())
	{
		return;
	}
	FWBTraceEvent Event = MakeBattleBoundary(State, FName(TEXT("battle_phase_ended")));
	Event.Reason = State.bGameOver ? TEXT("terminal") : TEXT("complete");
	OutEvents.Add(MoveTemp(Event));
	State.ClearBattlePhase();
}

FWBPublicBattleSummary WBBattlePhase::BuildPublicSummary(const FWBGameStateData& State)
{
	FWBPublicBattleSummary Summary;
	if (State.IsBattlePhaseActive())
	{
		Summary.bActive = true;
		Summary.DeclaringPlayerId = State.BattlePhase.DeclaringPlayerId;
		Summary.OriginalAttackerUnitId = State.BattlePhase.OriginalAttackerUnitId;
		Summary.OriginalDefenderUnitId = State.BattlePhase.OriginalDefenderUnitId;
	}
	return Summary;
}
