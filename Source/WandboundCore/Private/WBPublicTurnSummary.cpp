#include "WBPublicTurnSummary.h"

namespace
{
FName MakePublicPhaseName(const EWBGamePhase Phase)
{
	switch (Phase)
	{
	case EWBGamePhase::Response:
		return FName(TEXT("response"));
	case EWBGamePhase::NormalTurn:
	default:
		return FName(TEXT("normal_turn"));
	}
}
}

FWBPublicTurnSummary WBPublicTurnSummary::Build(const FWBGameStateData& State)
{
	FWBPublicTurnSummary Summary;
	Summary.CurrentPlayerId = State.CurrentPlayer;
	Summary.PriorityPlayerId = State.PriorityPlayer;
	Summary.TurnNumber = State.TurnNumber;
	Summary.Phase = MakePublicPhaseName(State.Phase);
	Summary.bGameOver = State.bGameOver;

	Summary.Players.Reserve(State.Players.Num());
	for (const FWBPlayerStateData& Player : State.Players)
	{
		FWBPublicPlayerTurnSummary PlayerSummary;
		PlayerSummary.PlayerId = Player.PlayerId;
		PlayerSummary.RemainingMP = Player.RemainingMP;
		PlayerSummary.LastMPRoll = Player.LastMPRoll;
		PlayerSummary.WallsLeft = Player.WallsLeft;
		PlayerSummary.WallRemovalsLeft = Player.WallRemovalsLeft;
		Summary.Players.Add(PlayerSummary);
	}

	return Summary;
}
