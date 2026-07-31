# Wandbound Active Format v1

Authority: product owner approved
Approval date: 2026-07-30
Format ID: `active_format_v1`
Version: 1

This document is authoritative Wandbound canon. It supplies the active-format
rules delegated by Rules Bible v2.1 section 23.3.

Game-start timing and first-player Turn 1 behavior are linked to
`game_start_turn_one_v1`, version 1.

## Main Deck

- A stored main deck contains 1 through 30 cards.
- Definition IDs are unique.
- The deck contains at least one supported non-Hybrid Character.
- Traps and NPCs are excluded.
- Unknown definitions, unknown categories, and definitions whose complete
  behavior is not supported fail validation.
- Other player-card categories are legal only when their complete behavior is
  supported by the production engine.

## Setup Kit

Each player has a separate Setup Kit with exactly two Trap slots and two NPC
slots. Every slot references a supported definition of the matching category.
Definition IDs may repeat. Setup Kit entries neither count toward main-deck
size nor inherit main-deck singleton restrictions.

## Hero And Launch

The selected Hero is an evidence-approved, supported, non-Hybrid Character
that appears exactly once in that player's main deck. It is removed before
shuffle and deployment. A launch deck must leave at least six cards after
Hero removal for the standard opening hand, making seven cards the current
minimum launch-ready deck.

## Randomization

The authoritative match seed drives the first-player coin flip and both deck
shuffles. Mirrored decks are legal. No wall-clock or platform-order input is
part of randomization.
