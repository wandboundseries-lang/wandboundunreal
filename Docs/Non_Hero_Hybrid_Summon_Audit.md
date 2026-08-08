# Non-Hero Hybrid Summon Audit

## Baseline

- Commit: `58ed67adcfa3432e1b736674ba3f5dfd15f6cd01` (`main`, synchronized with `origin/main`).
- Initial Wandbound automation count: 2,034.
- Staged files: none.
- Planned production files are clean at baseline; no planned edit overlaps a pre-existing dirty hunk.
- Pre-existing tracked and untracked work is outside this pass and must remain untouched.
- `WBActionCodec` is outside scope. Header SHA-256: `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`; implementation SHA-256: `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`.

## Canonical Contract

The committed `ProductionCardDB.schema.json`, production parser, and repository validation already require the complete Hybrid contract. This pass must not weaken it:

| Field | Required value |
|---|---|
| `sacrifice_count` | `1` |
| `sacrifice_requirement` | `controlled_character` |
| `wand_payment_count` | `1` |
| `wand_payment_sources` | exactly `hand`, `sacrificed_unit` |
| `hero_destination` | `sacrificed_hero_tile` |
| `non_hero_destination` | `adjacent_to_hero` |

A non-Hero branch sacrifices one active controlled Character, pays one eligible Wand, discards remaining equipment on that Character, and summons the Hybrid to an empty orthogonally adjacent tile around the authoritative Hero. The current Hero remains unchanged. The completed unit count is `active - 1 + 1`, with a cap of four including Hero. Sacrifice is not defeat and does not enter death resolution.

## Architecture Audit

| Path / symbol | Current role | Hero branch behavior | Non-Hero support before | Required generalization | Authority boundary | Replay impact | Privacy impact | Baseline dirty | Overlap |
|---|---|---|---|---|---|---|---|---|---|
| `Source/WandboundCore/Public/WBHybridSummon.h` / `FWBHybridSummonPlan` | Immutable complete transaction plan | Names the sacrificed current Hero and marks replacement | None | Rename the internal sacrifice field and expose unified planner/executor APIs while retaining compatibility wrappers | Core planner only; runtime never constructs plans | Stable action ID and replay schema remain unchanged | Plan stays internal to legal-action authority | No | No |
| `Source/WandboundCore/Private/WBHybridSummon.cpp` / planner | Enumerates validated Hero replacement payment plans | Hero tile only; hand or Hero equipment payment | Schema is recognized but non-Hero plans are not emitted | Enumerate active controlled Character sacrifices, completed-state destinations, and exact eligible payments in deterministic order | Rules/planner validates; no mutation | Same plan always produces the same existing Summon-family ID | Alternate payment candidates remain private legal actions | No | No |
| `Source/WandboundCore/Private/WBHybridSummon.cpp` / executor | Atomic copy-then-commit mutation | Pays, cleans equipment, removes old Hero, creates replacement Hero | None | Execute either branch; update `HeroUnitId` only for replacement | Mutation helper called by coordinator | One accepted Summon replay action; no schema change | Public-safe payment trace must omit Wand identity | No | No |
| `Source/WandboundCore/Public/WBMatchCoordinator.h` / `FWBMatchLegalAction` | Typed coordinator-owned legal action | Distinguishes Hero Hybrid replacement from ordinary summon | None | Distinguish unified Hybrid summon from ordinary Character summon | Coordinator remains sole production action authority | Action family remains Summon | Runtime sees only legal actions and public observations | No | No |
| `Source/WandboundCore/Private/WBMatchCoordinator.cpp` / enumerate and submit | Generates and submits complete actions | Calls Hero-only planner/executor then marker resolution | None | Call unified APIs and continue through existing marker/terminal/replay flow | Sole submit authority | One stable action ID and one committed record | No hidden-zone inspection added outside Core | No | No |
| `Source/WandboundCore/Private/WBTurnOneRestrictions.cpp` / `QuerySummonPlacement` | Shared summon placement gate | Applied to replacement destination | Reusable unchanged | Apply to every non-Hero destination | Existing legality authority | None | None | No | No |
| `Source/WandboundCore/Private/WBMarkerResolution.cpp` / post-summon resolution | Existing marker consequence path | Runs after replacement commit | Already generic by created unit ID | Reuse unchanged after non-Hero commit | Coordinator invokes after summon | Marker traces remain in the same accepted action | Existing marker visibility policy remains authoritative | No | No |
| `Source/WandboundCore/Private/WBDeathResolution.cpp` | Zero-HP removal and terminal authority | Not used for sacrifice | Must remain unused for non-Hero sacrifice | No change | Death semantics remain separate | No sacrifice death replay event | No impact | No | No |
| `Source/WandboundRuntime/Private/WBRuntimeTracePresentationTranslator.cpp` | Public trace-to-presentation translation | Emits sacrifice, safe payment, summon, and Hero replacement presentation | Generic events already map; Hero event is conditional on trace kind | No source change expected | Presentation consumes traces only | None | Does not inspect private payment candidates | No | No |
| `Data/Replay/HybridReplacementFixture/` | Existing packaged Hero regression fixture | Establishes byte-stable Hero baseline | None | No change | Fixture-only | Archive/receipt hashes must remain identical | No change | No | No |
| `Data/Replay/HybridNonHeroFixture/` | New isolated packaged fixture | Not applicable | Missing | Add non-Hero Character, Hybrid, hand/equipped Wands, destination occupancy, and markers | Fixture-only | Drives new packaged archive and fresh replay | Uses production observation/receipt boundaries | Missing | No |
| `Source/WandboundRuntime/Private/WBProductionHybridNonHeroSmoke.cpp` | New packaged production smoke | Not applicable | Missing | Select only coordinator-generated actions and verify fresh replay parity | No direct state mutation | Produces the existing eight-field receipt | Verifies public output omits payment identity | Missing | No |
| `Source/WandboundTests/Private/WBHybridNonHeroSummonTests.cpp` | New automation coverage | Retains existing Hero suite | Missing | Cover planner, atomicity, execution, cleanup, marker, terminal, replay, privacy, and guards | Tests may build fixture state; production authority unchanged | Proves fresh replay parity and Hero regression | Explicit privacy assertions | Missing | No |

## Compatibility Boundaries

- Keep `BuildHeroReplacementPlans` and `ExecuteHeroReplacement` as compatibility wrappers over unified behavior.
- Preserve the stable ID form `hybrid_summon:p{player}:i{hybrid_instance}:s{sacrificed_unit}:w{source}:i{wand_instance}:x{x}:y{y}`.
- Keep the existing Summon action family, replay schema version, public receipt shape, canonical startup data, and Hero replacement trace ordering.
- Keep ordinary summon, marker, terminal, and replay ownership in the coordinator.
- Do not implement sacrifice triggers, card-specific summon triggers, response windows, inheritance, or simultaneous dual-Hero changes.

