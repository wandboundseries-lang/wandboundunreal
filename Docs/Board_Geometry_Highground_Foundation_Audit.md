# Board Geometry and Highground Foundation Audit

## Verified Baseline

- Repository: `wandboundseries-lang/wandboundunreal`
- Branch: `main`
- `HEAD`: `0f8eb6866e512818066d05030ea185c5b86a15b5`
- `origin/main`: `0f8eb6866e512818066d05030ea185c5b86a15b5`
- Baseline commit: `Add vocabulary foundation reconciliation`
- Baseline automation: 2,407 succeeded, 0 failed, 0 warnings, 0 not run.
- Baseline task files were clean. Three unrelated untracked paths were present and remain untouched.
- Staged files before and after the pass: zero.

## Current Authority Audit

Before this pass, movement and attack geometry were separately implemented in
`WBRules`, NPC pathing had its own orthogonal direction list, and continuous AR
auras had a second diagonal line/wall algorithm in `WBUnitStatQuery`. CardDB
accepted only `orthogonal_adjacent` movement and `orthogonal_line` attacks.

The existing `EWBCombatCapability::AttacksDiagonally` capability was used as an
additive capability in Vex coverage and could also be supplied by equipped
definition data. It did not mean diagonal-only. This pass preserves that
production behavior: it adds diagonal attack geometry to the definition's base
geometry and never removes the base geometry.

The read-only Godot implementation still blocks a diagonal when either source
corner edge is walled. That conflicts with the owner-approved rule in this pass.
The Unreal implementation follows the newer owner canon: a diagonal is blocked
only when both two-edge orthogonal routes are blocked. No Godot file was changed.

## Geometry Model

`FWBGridGeometryProfile` represents orthogonal-only, diagonal-only, or both.
Character and Hybrid definitions now carry independent movement and attack
profiles. Defaults remain orthogonal-only, preserving every current production
card without data changes.

`WBBoardGeometry` is the shared authority for:

- board bounds;
- orthogonal and diagonal adjacency;
- orthogonal and diagonal line classification;
- geometry-specific line distance;
- orthogonal wall edges;
- canonical diagonal two-route wall checks;
- multi-step line traversal;
- intervening-unit blocking;
- deterministic movement/path direction order.

`WBRules` keeps compatibility wrappers for established callers, but delegates
geometry to `WBBoardGeometry`.

## Diagonal Movement

- A diagonal step changes both X and Y by exactly one.
- Diagonal-only definitions generate only diagonal moves.
- Orthogonal-only definitions generate only orthogonal moves.
- Both geometry is available only when explicitly declared.
- Destination occupancy, Rooted, Stunned, MP, walls, board bounds, ownership,
  priority, normal-turn phase, and turn-one relocation restrictions remain the
  shared legality gates.
- Legal action generation remains deterministic and keeps movement before attack
  and End Turn.
- `WBEffectRunner` now applies repository-aware Move legality, so generated and
  submitted definition-driven moves use the same authority.
- Existing PostMove coordinator behavior is unchanged and passed regression.

## Diagonal Attack

- Diagonal-only definitions attack only uninterrupted diagonal lines.
- Each diagonal tile step costs one AR.
- An intermediate unit directly on the diagonal blocks the attack.
- Units on side tiles around a crossed corner do not block.
- Attack declaration, legal attack generation, redirect revalidation,
  Counterattack, and NPC attack legality use the shared line authority.
- Existing `AttacksDiagonally` remains additive to the base definition geometry.

## Diagonal Wall Algorithm

For each diagonal step from S to D, the two intermediate side tiles define two
orthogonal two-edge routes. Each route is blocked if either edge on that route is
walled. The diagonal step is blocked only when both routes are blocked.

The same check runs independently for every step of a multi-tile diagonal line.
It supports every wall combination that closes both routes; it is not coded to a
single corner example.

## Aura Geometry Reconciliation

`WBUnitStatQuery` no longer owns a separate diagonal traversal implementation.
AttackLine auras query `WBBoardGeometry` with the source unit's definition-driven
attack profile and the aura's own wall/unit flags.

The Vex-like one-wall diagonal case is now open under owner canon. Closing both
routes blocks the aura. Highground does not grant aura wall bypass because the
aura remains a non-Attack effect with its own `bBlockedByWalls` rule.

## Highground Terrain Metadata

`WBTerrainRules` is the small generic terrain metadata authority. Highground is
represented in the existing single terrain identity map with:

- entry MP modifier: +1;
- occupant AR modifier: +1;
- true Attack wall behavior: ignore walls.

No parallel elevation state, duration, owner, hazard, or stacked terrain identity
was added. SetTerrain accepts `highground`, and changing terrain immediately
changes query-time behavior without moving the unit or mutating raw stats.

## Highground MP Behavior

- Normal to Normal: 1 MP.
- Normal to Highground: 2 MP.
- Highground to Highground: 2 MP.
- Highground to Normal: 1 MP.
- Diagonal movement uses the same destination surcharge.
- Creating Highground under a stationary unit charges no MP.
- Player and NPC movement spend the exact validated cost.
- Highground never bypasses movement walls.

## Highground AR Behavior

`WBUnitStatQuery::GetIntrinsicAR` returns stored AR plus non-recursive terrain AR.
`GetEffectiveAR` then applies enemy continuous modifiers. Consequently:

- raw `FWBUnitState::AR` remains unchanged;
- entering/leaving or overwriting Highground updates effective AR immediately;
- `ImmuneToEnemyEffects` suppresses enemy effects but not the Highground benefit;
- Vex and Highground compose additively;
- AR-based SetTerrain targeting receives the Highground benefit;
- aura source range receives intrinsic terrain AR without recursively querying
  enemy aura-modified effective AR;
- public unit AR uses the effective query when a repository is available.

## Highground Attack Wall Behavior

A true Attack originating on Highground ignores walls while preserving its
declared orthogonal/diagonal geometry, AR range, and intervening-unit blocking.
This applies to player declarations, NPC declarations, redirects revalidated from
the original attacker, and Counterattacks from a defender on Highground.

Highground does not grant diagonal geometry, ATK, extra attacks, movement wall
bypass, effect/aura wall bypass, or a defensive AR penalty against incoming
attacks.

## CardDB and Definition Authority

Production CardDB parsing now supports these typed patterns:

- movement: `orthogonal_adjacent`, `diagonal_adjacent`,
  `orthogonal_or_diagonal_adjacent`;
- attack: `orthogonal_line`, `diagonal_line`,
  `orthogonal_or_diagonal_line`.

Unsupported patterns fail closed. Current production data remains unchanged and
there is no CardId or faction-specific geometry branch.

## NPC, Counter, Redirect, Turn-One, and PostMove Composition

- NPC pathing derives deterministic directions from the NPC's movement profile.
- Default production NPC definitions remain orthogonal-only.
- NPC path probes use current remaining MP, so an unaffordable Highground step is
  not selected; actual movement spends the same validated destination cost.
- NPC true attacks use definition geometry and Highground wall behavior.
- Counter range and line checks use the countering unit's geometry and effective AR.
- Pending-attack redirects revalidate final target geometry, range, walls, and
  intervening units through the shared line authority.
- Crash-In and Body Double regressions passed; their replacement/substitution
  semantics were not changed.
- Turn-one diagonal movement reuses `WBTurnOneRestrictions::QueryRelocation`.
- Existing coordinator PostMove opening behavior passed unchanged.

## Move and Declared Move Future Readiness

Current provenance is already structurally distinct:

- accepted player Move uses `FWBAction::Move`, stable `move:` action IDs, and
  accepted replay records;
- NPC movement uses NPC authority and `npc_moved` traces;
- forced/effect relocation and teleport-style operations use separate relocation
  APIs/traces and are not accepted player Move actions.

The future six-declared-moves Wand can count accepted player Move declarations at
coordinator/rules state scope without counting NPC or forced relocation. It will
still require canon for reset/ownership/response timing and a dedicated state
field plus legality/replay coverage. No limit or counter was implemented here.

## Existing Terrain Keywords

Swim, Ice Skate, Mudslide, and Lava Flow remain outside this pass. Their same-
terrain discounts are not fully wired through the current production movement
cost path. Highground remains a separate single terrain identity and no stacking
or inferred interaction was introduced.

## Replay and Privacy

- Replay schema remains 1.
- Public receipt remains exactly eight fields.
- `WBActionCodec.cpp` Git blob before/after:
  `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.
- No action ID or action serialization change was made.
- No hidden information was added to geometry or terrain state.
- Vex and Terrain packaged smokes each performed fresh replay verification.
- Receipt scans contain no state digest, trace digest, ordered deck, private, or
  opponent-hand fields.

Packaged deterministic evidence:

| Smoke | Archive SHA-256 | Receipt SHA-256 | Startup SHA-256 | Records | Final generation/revision |
|---|---|---|---|---:|---|
| Vex | `c697bab4a160f7aa2cd697d3436e91fda9a282526f35f95bb6b273f5835baa44` | `5aa0b99774a7a980eac1ece3aafb39637099222a55e91ece95db67ca7fd56414` | `c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c` | 13 | 1/14 |
| Terrain | `5953dab4f04cabe5e5d3ba24dd8421963562a59ddf20e3af84ec69d363bd5ced` | `792e76edb52447f09523a8aa699d11c72385105674a5d3f7c7bb704ff82635fb` | `c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c` | 22 | 1/23 |

Vex final state/trace digests:
`f78b95e4e2567aaaa2e3fcd5bc55d131f38f838a637753756103dcb56a5d0690` /
`69bb6305a2f70376d02ab34950760c531afa79f5ed564bb9ebfb39fdf5df0747`.

Terrain final state/trace digests:
`dad14fc8bc0a0cf9c6fe3f9c63bdba52497a7e296502bf12ce3136e4eded03d9` /
`2bd3ec042c7379fa1e49e7247c7b0849d750beb5010d6e7c218014d03a9ebe53`.

Both runs of each smoke exited 0, produced byte-identical artifacts, and had
empty stderr.

## Test Inventory

New focused automation file: `WBBoardGeometryHighgroundTests.cpp`.

New tests (13):

1. `Wandbound.Geometry.Primitives.OrthogonalDiagonalAndDistance`
2. `Wandbound.Geometry.DiagonalWalls.TwoRouteCanon`
3. `Wandbound.Geometry.Movement.DefinitionDrivenProfilesAndGeneration`
4. `Wandbound.Geometry.Movement.SharedGuards`
5. `Wandbound.Highground.Movement.DestinationCostAndMutation`
6. `Wandbound.Geometry.Attack.DiagonalRangeWallsAndUnits`
7. `Wandbound.Highground.Attack.WallBypassGeometryAndUnits`
8. `Wandbound.Geometry.Combat.RedirectCounterAndNPC`
9. `Wandbound.Geometry.Aura.CanonicalDiagonalWallsNoHighgroundBypass`
10. `Wandbound.Highground.AR.EffectiveCompositionAndImmediateTerrainMutation`
11. `Wandbound.Highground.AR.VexComposition`
12. `Wandbound.Highground.PublicState.SingleTerrainIdentity`
13. `Wandbound.Highground.NPC.PathingSkipsUnaffordableEntry`

Focused new tests: 13 succeeded. Affected regression groups all passed, including
Movement 17, Attack 40, NPC 5, Core NPC 17, Terrain Cartographers 9, Vex 14, Crash-In 9,
Body Double 13, Attack Redirect 15, Turn One 30, Public Board 16, Runtime Board
Summary 8, Vocabulary 16, PostMove 1, Counterability 3, and suspended Counter 1.

Final full Wandbound automation:

- 2,420 succeeded;
- 0 failed;
- 0 warnings;
- 0 not run;
- delta from baseline: +13.

## Build Matrix

| Configuration | Result | Time |
|---|---|---:|
| Editor Development non-unity | Succeeded, clean 132-action rebuild | 118.83 s |
| Game Development non-unity | Succeeded, clean 133-action rebuild | 101.25 s |
| Editor Development default | Succeeded, clean 210-action rebuild | 329.36 s |
| Game Development default | Succeeded, clean 185-action rebuild | 308.64 s |
| Editor Development forced-unity | Succeeded, clean 201-action rebuild | 264.68 s |
| Game Development forced-unity | Succeeded, clean 176-action rebuild | 270.34 s |

Both forced-unity targets used `-ForceUnity -DisableAdaptiveUnity` after clean
generated target outputs. The clean BuildCookRun separately rebuilt a 395-action
Editor/Game graph and therefore also compiled every final source and test file.

## BuildCookRun and Package

- Clean BuildCookRun: succeeded, exit 0, 610.03 seconds.
- Build graph: 395 actions; build succeeded in 494.94 seconds.
- Full cook: 532 discovered, 459 packaged, 0 incrementally skipped.
- Cook result: 0 errors, 0 warnings.
- Stage, package, and archive: succeeded.
- Standard packaged local-play smoke: two exits 0; result JSON byte-identical;
  SHA-256 `767ab5d8229e750322c0d1265eb29312338e70467ea039e585615ee538d5dde1`.
- Production Vex and Terrain replay smokes: two exits 0 each, byte-identical
  archive/receipt/startup artifacts, empty stderr, fresh replay verified.

## Exact Changed-File Manifest

All task-owned paths were clean before this pass.

1. `Docs/Board_Geometry_Highground_Foundation_Audit.md` - new audit.
2. `Docs/Board_Geometry_Highground_Foundation_Audit.json` - new machine-readable audit.
3. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp` - typed geometry pattern and Highground parsing.
4. `Source/WandboundCore/Private/WBBoardGeometry.cpp` - shared geometry implementation.
5. `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp` - definition geometry validation.
6. `Source/WandboundCore/Private/WBEffectRunner.cpp` - repository-aware player/NPC movement application.
7. `Source/WandboundCore/Private/WBNPCPhaseResolution.cpp` - shared profile pathing and MP spending.
8. `Source/WandboundCore/Private/WBPublicBoardSummary.cpp` - intrinsic AR and canonical Highground output.
9. `Source/WandboundCore/Private/WBRules.cpp` - geometry/terrain legality integration.
10. `Source/WandboundCore/Private/WBTerrainRules.cpp` - generic terrain metadata implementation.
11. `Source/WandboundCore/Private/WBUnitStatQuery.cpp` - intrinsic AR and shared aura line authority.
12. `Source/WandboundCore/Public/WBBoardGeometry.h` - geometry API.
13. `Source/WandboundCore/Public/WBCardDefinition.h` - movement/attack geometry profiles.
14. `Source/WandboundCore/Public/WBEffectRunner.h` - repository-aware movement overloads.
15. `Source/WandboundCore/Public/WBRules.h` - shared geometry/repository-aware rule APIs.
16. `Source/WandboundCore/Public/WBTerrainRules.h` - typed terrain metadata API.
17. `Source/WandboundCore/Public/WBTypes.h` - geometry enum/profile types.
18. `Source/WandboundCore/Public/WBUnitStatQuery.h` - intrinsic AR query.
19. `Source/WandboundTests/Private/WBBoardGeometryHighgroundTests.cpp` - focused coverage.

## Deferred Features

- Whimsy production cards.
- The opponent six-declared-moves Wand and any movement counter/cap.
- Defensive extra AR required to attack a unit on Highground.
- Sealed status work.
- Strategic AI behavior.
- Hyper-detailed telemetry.
- General production wiring for Swim, Ice Skate, Mudslide, and Lava Flow.

## Remaining Questions

- The future movement-limit Wand still needs owner canon for scope, reset timing,
  control changes, and whether a declared Move that is later replaced/prevented
  counts. No behavior was guessed.
- AttackLine effects currently reuse attack geometry only where their definition
  explicitly says AttackLine. Highground wall bypass remains true-Attack-only.
- No current production definition declares diagonal-only geometry; fixture
  definitions prove the production-ready authority without transferring Whimsy.

## Git Hygiene

- Zero staged files.
- No commit or push.
- `git diff --check`: no whitespace errors; only line-ending notices.
- No Config, map/content asset, Meshy, Godot, Blueprint, or action-codec change.
- Preserved unrelated untracked paths:
  `Content/Maps/NewProjectTest.umap`,
  `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, and
  `h origin main`.
