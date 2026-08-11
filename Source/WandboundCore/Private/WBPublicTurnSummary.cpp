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
	if (State.HasOpenReactionWindow())
	{
		switch (State.ReactionWindow.Kind)
		{
		case EWBReactionWindowKind::PreHit: Summary.ReactionWindowKind = FName(TEXT("pre_hit")); break;
		case EWBReactionWindowKind::PostHit: Summary.ReactionWindowKind = FName(TEXT("post_hit")); break;
		case EWBReactionWindowKind::PostMove: Summary.ReactionWindowKind = FName(TEXT("post_move")); break;
		case EWBReactionWindowKind::PostSummon: Summary.ReactionWindowKind = FName(TEXT("post_summon")); break;
		case EWBReactionWindowKind::PostEffect: Summary.ReactionWindowKind = FName(TEXT("post_effect")); break;
		case EWBReactionWindowKind::None:
		default: break;
		}
	}
	Summary.bGameOver = State.bGameOver;
	Summary.WinnerPlayerId = State.WinnerPlayerId;
	if (State.bGameOver)
	{
		Summary.LoserPlayerId = State.TerminalOutcome.LoserPlayerId;
		Summary.TerminalReason =
			WBTerminalOutcomeNames::ReasonToName(State.TerminalOutcome.Reason);
		Summary.TerminalSource =
			WBTerminalOutcomeNames::SourceToName(State.TerminalOutcome.Source);
		Summary.TerminalTurn = State.TerminalOutcome.TurnNumber;
	}

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
