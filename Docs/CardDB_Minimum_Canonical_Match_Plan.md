# CardDB Minimum Canonical Match Plan

## Decision

No production match specification can be created from the tracked canonical
evidence. The production suite remains at 10 definitions and
`match_spec.json` remains absent.

## Hero Evidence

Rules Bible 23.1 requires at least one non-Hybrid Character eligible as Hero,
and setup removes the chosen Hero from each player's deck before spawning it.
Tracked tutorial setup explicitly chooses `char_test_01` and `char_test_03`.
Those two definitions are supported and are the only explicit Hero choices used
for this production audit.

Character type, narrative importance, AI/development use, model availability,
and legacy test naming are not independently sufficient Hero evidence.

## Deck Evidence

Rules Bible 23.3 delegates faction, format, and card-count restrictions to the
active format document. No active format document exists in the tracked
reference. The following production inputs are therefore unknown:

- deck card count;
- allowed player-deck card types;
- duplicate limit;
- deck ordering or shuffle policy;
- whether identical player decks are legal;
- deterministic first-player policy for this match.

Starting hand size and marker setup are explicit, but they do not determine a
legal deck. Tutorial and reduced AI decks are examples, not format authority.

## Supported Expansion Audit

All 244 canonical definitions were classified. The existing 10 remain
transferred. Three untransferred definitions have complete behavior mappings to
existing production runtime paths:

| Definition | Classification | Existing path |
|---|---|---|
| `char_test_healer` | EligibleExistingActivation | once-per-turn heal activation |
| `wand_equip_mender_thread` | EligibleExistingEquip | Wand equip plus heal activation |
| `trap_generic_01` | EligibleExistingPayload | canonical generic trap damage |

They were not transferred because no legal minimum deck can be solved and none
independently lifts the production startup block. Unsupported passives,
reactions, timing, target constraints, movement, terrain, effects, schema
mismatches, and ambiguous semantics remain fail-closed.

## Deterministic Solver

`WBMinimumCanonicalMatchPlanner` accepts explicit Hero evidence, explicit deck
rules, and classified definitions. It rejects inferred or conflicting rules,
development fixtures, unsupported definitions, and incomplete semantic
mappings. When all evidence exists, it sorts IDs and computes the smallest
deterministic transfer set while respecting allowed types and copy limits.

For the current canonical inputs the solver stops before card selection with
named evidence blockers. It does not invent a reduced deck.

## Effect-Family Ranking

| Effect family | Definitions unlocked | Match impact | Complexity | Timing dependency | Recommendation |
|---|---:|---|---|---|---|
| Activation target constraints | 4 exact: Cinder Oracle, Glimmers Bow, tazer, Vipers Embrace | Adds supported Character/Wand activations; does not supply format rules | Medium | Normal-turn activation | Recommended behavior family after format data exists |
| Passive event hooks | Up to 77 primary-blocker candidates; exact count requires per-passive work | Broad Character potential; no format impact | Very high | Multiple event phases | Split into narrower canonical families first |
| Terrain effects | Up to 32 primary-blocker candidates; exact count requires terrain subfamily audit | Terrain-linked cards; no format impact | High | Activation and event timing | Defer |
| Response reactions | 10 | React definitions only; no deck evidence requires them | Very high | Response windows and priority | Not the minimum next family |

No new effect implementation is unavoidable for the current match block. The
missing active-format evidence stops the solver before effect-family selection.

The single next task is: add the authoritative active format document or
machine-readable format data defining deck card count, allowed card types, copy
limits, ordering/shuffle policy, mirrored-deck legality, and deterministic
first-player policy.

The complete machine-readable evidence, per-definition classification, blocker
list, and effect-family ranking are in
`Docs/CardDB_Minimum_Canonical_Match_Plan.json`.
