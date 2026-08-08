# Wandbound Atomic Hybrid Hero Replacement Report

## Result

The coordinator-owned atomic current-Hero replacement path is implemented. It generates stable Summon-family actions, prevalidates sacrifice/payment/destination/cap/freshness, commits against a copied state, installs the Hybrid as Hero before coordinator terminal evaluation, records one accepted replay action, preserves marker handling, and continues nonterminal play.

Validation completed:

- Editor build: succeeded; final incremental build 30.14 seconds.
- Game build: succeeded; 317.47 seconds.
- Focused Hybrid tests: 58/58.
- Hybrid replay tests: 11/11.
- Hybrid authority tests: 10/10.
- Full automation: 2,034 succeeded, 0 failed, 0 not run; baseline was 1,955.
- Fresh clean BuildCookRun: succeeded in 1,011.95 seconds.
- Packaged Development map launch: map loaded; the generic quit wrapper required manual process termination and is not counted as a self-terminating smoke.
- Packaged production startup: two successful runs, byte-identical.
- Packaged partial replay regression: successful and baseline-identical.
- Packaged terminal replay regression: successful and baseline-identical.
- In-editor Hybrid production smoke: successful, including fresh coordinator replay.
- Packaged Hybrid smoke: passed twice with package-relative fixture paths and real process exit code 0.
- Packaged fresh replay verification: passed twice, including replacement Hero identity and final state/trace parity.
- Repeated packaged archive, receipt, and Hybrid startup JSON: byte-identical.
- Packaged canonical startup probe: exit 0 and baseline-identical SHA-256.
- `git diff --check`: no whitespace errors; only LF-to-CRLF working-copy notices.

Exact implementation/test errors fixed before final source validation:

1. `TArray::CountByPredicate` was unavailable; replaced with an explicit loop.
2. A new translation unit changed unity grouping and exposed a pre-existing duplicate anonymous `FindEndTurn`; the terminal smoke helper was narrowly renamed `FindTerminalEndTurn`.
3. A full-suite authority guard detected a Runtime dependency on `WBCardZoneState`; Runtime now reads the immutable public state snapshot directly. The targeted guard and full suite then passed.
4. The packaged Game build exposed display-case-sensitive Hybrid `FName` tokens in the CardDB semantic digest. The CardDB digest/canonical JSON boundary now lowercases the already-validated Hybrid contract tokens, preserving the Editor digest in packaged builds.

## Baseline

- Commit / `origin/main`: `f5d487cdc36421ccde84fa146d2ba8e50150e88b` / same.
- Branch: `main`.
- Staged files: none. LFS-staged files: none.
- Initial automation: 1,955 passed.
- Replay schema: 1; rules compatibility: 1.
- Production bundle digest: `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- Production match-spec SHA-256: `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.
- Startup SHA-256: `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Partial archive / receipt / replay digest: `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463`, `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9`, `391f0a6e836fc19439f110a5bd0a748367c00c826e73ad1d615fe53d9b492e7e`.
- Terminal archive / receipt / replay digest: `4e30424a56b613cbbda225295a0775473ed661cda390f172b609e529450235cc`, `5bcce2e1e9361e8848e4757a634cf82acdee30d2463a01f6f9f0023157e1ca76`, `95a9bd178298085120774097e8beea17a266b4ec6c048aa49dc9351f9c50dc6b`.
- `WBActionCodec.h`: source SHA-256 `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`, Git blob `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp`: source SHA-256 `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`, Git blob `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.

Baseline dirty tracked and untracked files are recorded in the audit work log and preserved. Task overlap exists in the already-dirty public-board summary files; only authoritative Hero classification was added there. No baseline hunk was reverted.

## Canonical Scope

Tracked canon establishes Hybrid category, one controlled-Character sacrifice, one Wand payment from hand or sacrificed-unit equipment, discard of paid/remaining equipment, current-Hero tile replacement, printed replacement stats, player Hero-ID reassignment, completed-transaction unit cap, and ordinary summon/marker continuation.

This pass implements only current-Hero sacrifice. It deliberately does not define inheritance, non-Hero Hybrid placement, sacrifice-trigger timing, card-specific summon triggers, response timing, or simultaneous dual-Hero resolution.

## Transaction Plan and Commit

The immutable plan records player, Hybrid instance/definition, old Hero, typed payment source/card/unit, destination, replacement flag, and before generation/revision. Planning performs no mutation and sorts payment choices by source then stable card instance ID.

Execution regenerates and exactly matches the complete plan. It then works on a state copy in this order:

1. Move the selected Wand to discard.
2. Move remaining old-Hero equipment to discard in deterministic equipment order.
3. Remove the Hybrid from hand.
4. mark the old Hero sacrificed and remove it from the board without defeat handling;
5. create the Hybrid with printed stats on the vacated Hero tile;
6. update the acting player's authoritative `HeroUnitId`;
7. commit the complete state once;
8. emit ordered traces and let the coordinator resolve marker/terminal consequences.

There is no authoritative Hero-less checkpoint, replay record, public summary, or presentation refresh. Rejected/stale plans leave the original state digest unchanged and are not recorded.

## Sacrifice, Payment, and Cleanup

Sacrifice cause is the typed Hybrid summon transaction, not combat defeat. The old Hero remains as an inactive historical unit record, has no board occupancy, and emits no Hero-defeat or game-over trace. Payment uses one stable Wand instance and existing lifecycle helpers. Public payment trace contains no paid card instance or Wand definition identity. All equipment attached to the old Hero is detached and discarded; no attachment remains orphaned.

## Hero, Board, and Terminal State

After commit, the acting player has one authoritative active Hero: the new Hybrid's new unit ID. Public board summary derives `bHeroUnit` from player `HeroUnitId`; old Hero is absent from the public board. The destination is the canon-supported vacated Hero tile. Cap legality evaluates `active owned units - sacrificed unit + replacement` and rejects a completed count over four.

Valid replacement remains nonterminal with winner/loser unset and no Hero-loss reason. The unchanged zero-HP path was exercised after replacement: later loss of the Hybrid Hero remains terminal with `hero_defeated_without_replacement`. There is no global terminal suppression.

## Replay and Privacy

The existing Summon family records the full stable action ID:

`hybrid_summon:p{player}:i{hybrid_instance}:s{hero}:w{source}:i{wand_instance}:x{x}:y{y}`

No second codec or replay format was added. Fresh replay reproduces Hero identity, discard/equipment counts, state digest, trace digest, generation, and revision. The archive remains partial/nonterminal with no completion footer. The public receipt remains eight fields.

In-editor smoke deterministic evidence:

- Fixture bundle digest: `dd007d63231ecca82f071fb2f62ebf77ceb5213af74ab003a889f91bc41594dd`.
- Old / new Hero IDs: `0` / `2`.
- Destination: `(4,8)`.
- Archive SHA-256: `a3a85aa6ae03ce3f57a5c5af40b172e4b938c582e9e1cc8ef58e47e39fe5e384`.
- Receipt SHA-256: `2c24946f17152ace66929e247d4bc435ebfddf6783c865d7bd87343aa174d85a`.
- Replay digest: `7eccfe5a90d59270fa5dc172df721bab66bdc5e627de63489c0caa5e13303145`.
- Final state / trace digests: `9abce6721fb022c2769f8207b4dd07c79811a65bb8fc25f97962feb4e09f1897` / `ce8eddad94d2c21b0465ab3d727c63e03bb4103dad4d3c416af410033fe02125`.

Packaged smoke deterministic evidence:

- Fixture bundle digest: `dd007d63231ecca82f071fb2f62ebf77ceb5213af74ab003a889f91bc41594dd`.
- Old / new Hero IDs: `0` / `2`; fresh replay reproduced Hero ID `2`.
- Repeated archive SHA-256: `e1fa69301728e8129e69866ce0a91fbeaf77cc990d08f0a33133943bc629be20`.
- Repeated receipt SHA-256: `7cdba9356c9fbb6c796aaaedfbeef7fc884ec74522a4a20231d082961cc6f156`.
- Repeated Hybrid startup SHA-256: `67804f4fd7a0a4ee4310c5981e3e6ed59a30ff7cad652644137d7e516c6ad054`.
- Replay digest: `9c73493e6931a627969a7472b49c858a623150de49da522ef963663f41e3f98e`.
- Final state / trace digests: `9abce6721fb022c2769f8207b4dd07c79811a65bb8fc25f97962feb4e09f1897` / `fdc47bcbab4c83987b4c9749cf1e905074a90a6d6fd914f4606201b8c83b7703`.
- Canonical startup remained byte-identical at SHA-256 `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.

The top-level Windows launcher inserted a space before unquoted `.json` extensions. Final validation therefore invoked the packaged inner executable with its required `WandboundUE` bootstrap argument; the game still consumed only package-relative `Data/...` fixture paths.

Opponent observation hides hand and payment candidates. Public traces expose the visible Hybrid/replacement unit but not the paid Wand identity. Receipts expose no seed, actions, hidden cards/markers, protected state/trace digests, or paths.

## Tests

`WBHybridHeroReplacementTests.cpp` adds 79 exact named tests: 58 `Wandbound.Hybrid.*`, 11 `Wandbound.Replay.Hybrid.*`, and 10 `Wandbound.Authority.Hybrid.*`. They cover schema, sacrifice/payment/destination legality, stale and altered-plan atomicity, completed cap/occupancy, replacement state, ordinary and later Hero loss, cleanup, ordered safe traces, fresh replay parity, public privacy, package smoke contract, and authority guards.

## Changed Files

Core: `WBCardDefinition.h`, `WBCardDefinitionRepository.cpp`, new `WBHybridSummon.h/.cpp`, `WBMatchCoordinator.h/.cpp`, and the already-dirty `WBPublicBoardSummary.h/.cpp`.

Generic CardDB: `WBProductionCardDatabase.h/.cpp` and `ProductionCardDB.schema.json`. These enable validated test fixtures and canonicalize validated Hybrid tokens at deterministic digest/JSON boundaries; no canonical production definition or deck changed.

Runtime/replay: `WBProductionMatchReplayRuntime.h/.cpp`, `WBRuntimePresentationEvent.h`, `WBRuntimeTracePresentationTranslator.cpp`, new `WBProductionHybridReplacementSmoke.h/.cpp`, `WBRuntimeMatchBootstrapActor.cpp`, `WandboundRuntime.Build.cs`, and the narrow terminal-smoke helper rename.

Tests/data/docs: new `WBHybridHeroReplacementTests.cpp`, `Data/Replay/HybridReplacementFixture/*`, this report, and `Atomic_Hybrid_Hero_Replacement_Audit.md/.json`.

No task change exists in `WBActionCodec`, canonical production cards/decks/spec, Config, Godot, Meshy, maps, Blueprints, models, `.uasset`, or `.umap` files.

## Git Status

No file is staged and no LFS object is staged. Task files are those listed above. Pre-existing tracked changes in Config, CardDB reports, affordability/payment, game state, resonance, runtime result, equip, and existing tests remain present and untouched except the documented public-board overlap. Pre-existing untracked maps, Meshy content, unrelated audits, `MaxHP`, `RLTotal`, and sentinel files remain untouched. Generated package/replay/test output stays under ignored `Saved/`.

## Remaining Risks

1. Non-Hero Hybrid placement remains unimplemented.
2. Sacrifice-trigger and card-specific summon-trigger timing remain unspecified/unsupported.
3. Simultaneous dual-Hero resolution remains the existing fail-closed unsupported case.
4. The top-level packaged Windows launcher mangles unquoted `.json` arguments; automated package probes should continue invoking the packaged inner executable with the required bootstrap argument or use a launcher-safe argument transport.

## Recommended Next Task

Implement the non-Hero Hybrid summon branch using explicit canonical sacrifice, payment, and adjacent-placement choices through the existing typed plan and coordinator transaction, without adding inheritance, response windows, or production cards.
