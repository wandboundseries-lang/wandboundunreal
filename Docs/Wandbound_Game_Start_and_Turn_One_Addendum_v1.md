# Wandbound Game Start And Turn One Addendum v1

Authority: product owner approved
Approval date: 2026-07-30
Addendum ID: `game_start_turn_one_v1`
Version: 1

This addendum is authoritative Wandbound canon. Its setup timing and
first-player Turn 1 clauses supersede conflicting clauses in Rules Bible v2
and v2.1, including sequential Hero deployment and the blanket prohibition on
first-player attacks during Turn 1.

## Authority Registration

For the exact conflicts above, this product-owner-approved addendum has higher
priority than the Rules Bible. All Rules Bible clauses outside those conflicts
remain authoritative.

## Setup Order

1. Validate Active Format decks, Setup Kits, Heroes, and launch capacity.
2. Use the match seed once to select the first player.
3. Place the first player's concealed markers, then the second player's.
4. Commit both Heroes atomically.
5. Collect summon triggers from the shared post-spawn state.
6. Resolve the first player's trigger batch, then the second player's batch.
7. Resolve mandatory setup consequences.
8. Draw six opening cards for each player without replacing prior setup draws.
9. Begin the selected first player's Turn 1.

Both Hero spawn tiles are reserved during marker placement. Hero placement is
a real summon. Multiple simultaneous triggers controlled by one player
require an authoritative, stable, replay-recorded ordering choice. Manual
React actions and priority passing are suppressed during setup, while
mandatory effects and their required choices continue.

## Board Regions

The 9 by 9 board has four player-relative own rows, middle row 4 as the
neutral row, and four opponent rows. Player 1's own/opponent orientation is
the reverse of Player 0's.

## First Player Turn 1

Only the selected first player's first turn is restricted.

- Summons may enter that player's own half or the neutral row, but not the
  opponent half.
- Any movement or relocation is illegal when any affected unit crosses
  OwnHalf/NeutralRow or NeutralRow/OpponentHalf boundaries. This applies to
  movement, teleport, push, pull, swap, forced movement, and future equivalent
  relocation operations.
- Neutral NPCs may be attacked anywhere when ordinary attack legality passes.
- Opponent-controlled units may not be attacked.

The restriction is inactive during setup, after the first player's first turn,
and during the second player's first turn.
