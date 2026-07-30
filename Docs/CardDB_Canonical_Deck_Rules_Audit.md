# CardDB Canonical Deck Rules Audit

The current canonical Rules Bible does not contain enough deck-construction
evidence to authorize a production match. Rule 23.3 explicitly delegates
faction, format, and card-count restrictions to an active format document, and
no active canonical format document exists in the tracked reference copy.

## Canonical Explicit Rules

- A legal deck contains at least one non-Hybrid Character eligible to be
  chosen as Hero (`Rules Bible 23.1`).
- Each player chooses a Hero from their deck, removes it from the deck, and
  spawns it on the fixed Hero tile (`Rules Bible 1.6 and 6.2`).
- Each player draws 6 cards after Hero selection (`Rules Bible 6.5`).
- A coin flip or mode rule selects the first player (`Rules Bible 6.1`).
- Each setup kit contains exactly 2 Trap markers and 2 NPC markers
  (`Rules Bible 6.3 and 23.2`).
- Drawing from an empty deck fails unless a mode supplies another rule
  (`Rules Bible 22.2`).

## Missing Production Rules

The canonical sources do not state a required, minimum, or maximum deck size;
allowed player-deck card types; duplicate limits; shuffle or ordering policy;
whether NPCs, Traps, or Wands belong in player decks; whether mirrored decks
are legal; or a deterministic first-player policy for this production match.

Tutorial and AI decks are implementation examples. The AI source explicitly
describes its reduced deck as a fast headless-test baseline, so those lists
cannot authorize a production deck or copy count.

## Result

The missing active-format rules block the minimum-deck solver before card
selection. No reduced deck or `match_spec.json` is canonically legitimate.
The machine-readable record is
`Docs/CardDB_Canonical_Deck_Rules_Audit.json`.
