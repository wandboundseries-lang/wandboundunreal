# Production Match Replay Log Audit

Date: 2026-08-01

## Baseline

- Commit: `b0b6a6965103d1c0987af69d2addc7b695af5ab7` (`Centralize turn-transition authority`).
- Branch: `main`, synchronized with `origin/main` at audit time.
- Staged files: none. LFS-staged files: none.
- Initial Wandbound automation: 1,816 succeeded, 0 failed, 0 warnings, 0 not run.
- Production startup SHA-256: `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Production bundle semantic digest: `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- Canonical match-spec raw SHA-256: `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.
- Active Format semantic digest: `258c5925ae1af7a20663c96d225f21de97b339a523c4b968cc8c6d3a024529af`.
- Game-start addendum semantic digest: `bdcd0d2fc19e853f874fc80eb58cb7967062faa3b8fe91f6c85a3289972d1e67`.

The baseline dirty-file inventory is preserved in the implementation report. No baseline dirty file was reverted, staged, or claimed as replay work.

## Existing Replay Components

| Classification | Component | Current role | Decision |
|---|---|---|---|
| CoordinatorAuthority | `WBMatchCoordinator` | Production legal decisions, acceptance, mutation, RNG, pending decisions, turn transitions, traces, terminal state | Extended with an immutable accepted-action record stream. |
| LowLevelVerifier | `WBReplayVerifier` | Replays `FWBAction` decisions from a caller-provided `FWBGameStateData` | Preserved. It is not a production-match orchestrator. |
| ActionCodec | `WBActionCodec` | Stable IDs and JSON for core `FWBAction` | Consumed unchanged; no replay branch added. |
| DeterministicPrimitive | `WBReplayTrace` | Canonical trace-event JSON | Reused for protected trace digests. |
| DeterministicPrimitive | `WBProductionRuntimeBootstrap` | Production bundle/match-spec validation and coordinator initialization input | Reused by the fresh replay runner. |
| PublicSerializer | public board/turn/zone summaries and startup result | Perspective-safe runtime output | Kept separate from the protected archive. |
| PersistenceAdapter | `WBProductionMatchReplayPersistence` | Synchronous atomic archive writes and loads under `Saved` | New runtime boundary; owns no gameplay state. |
| RuntimeConsumer | `UWBRuntimeMatchHostComponent` | Runtime coordinator host and provider refresh | Integrates recorder lifecycle after successful coordinator submissions. |

`WBReplayVerifier` is insufficient for production-match replay because it starts from a supplied state, generates only core rules actions, decodes through `WBActionCodec`, and applies actions through `WBEffectRunner`. It does not load the immutable production CardDB snapshot or match specification, perform normal production setup, use `WBMatchCoordinator`, verify generations/revisions/pending decisions/terminal state, cover summon/equip/activation/discard/turn-start choices, persist a private archive, or enforce a hash chain and public receipt boundary. It remains useful for focused rules-kernel fixtures and was not converted.

## Action-Family Inventory

| Action family | Production reachable | Stable ID | Coordinator | Private data | Durable choice | Replay test | Packaged | Status |
|---|---:|---:|---:|---:|---:|---|---:|---|
| Move | Yes | Yes | Yes | No | No | classifier, capture, runner | representative | Covered |
| Attack | Yes | Yes | Yes | No | No | classifier, coordinator capture boundary | guard | Covered |
| Summon | Yes | Yes | Yes | Yes | No | classifier, coordinator capture boundary | guard | Covered |
| Equip | Yes | Yes | Yes | Yes | No | classifier, coordinator capture boundary | guard | Covered |
| Activate | Yes | Yes | Yes | Potentially | target encoded in legal ID | classifier, coordinator capture boundary | guard | Covered |
| Discard | Yes | Yes | Yes | Yes | No | full archive round trip | Yes | Covered |
| End Turn | Yes | Yes | Yes | No | No | full archive round trip | Yes | Covered |
| Turn-start trigger order/target | Yes when multiple surviving triggers exist | Yes | Yes | Potentially | Yes | paused archive and fresh replay | guard | Covered |
| Pass React | Architecture-supported; no current production path opens a response decision | Yes | Yes when exposed | No | No | classifier/unknown guard | No | Guarded |
| Pass | Core rules-supported; current production coordinator phases do not expose it | Yes | Yes when exposed | No | No | classifier/unknown guard | No | Guarded |
| Place/Move/Remove Wall | Not coordinator-submitted today | N/A | No | No | No | unknown-family guard | No | Not reachable |
| Required unit/tile/wall/mode choice | No standalone family today; encoded by the owning legal action where supported | Owning action | Owning action | Varies | Yes where exposed | legal-set/decision digest | No | Guarded |
| Setup-trigger order | Setup request data, not a post-setup coordinator action | N/A | No | Potentially | Yes | initial state/trace checkpoint | No | Automatic setup |
| Resonance overflow allocation | Automatic deterministic resolution today | N/A | No | Yes | No | state/trace digest | No | Automatic |
| Marker player choice | No coordinator-submitted choice today | N/A | No | Yes | No | unknown-family/privacy guard | No | Not reachable |

Status ticks, death cleanup, NPC activity, MP generation, draws, trigger collection, and other automatic work are not fabricated as action records. Their results are covered by protected state and trace digests.

## Architecture Decision

- `WBMatchCoordinator` captures the family and before checkpoint while the selected legal action exists, then appends exactly one record only after successful acceptance and mutation.
- `WBProductionMatchReplay` owns typed header/record/footer data, canonical UTF-8 JSON, SHA-256 digests, hash-chain rebuilding, strict parsing, and public-safe receipt serialization.
- `WBProductionMatchReplayPersistence` writes a sibling `.tmp` and replaces the destination only after the temporary write succeeds.
- `FWBProductionMatchReplayRecorder` drains only new coordinator records and disables itself on persistence failure without changing gameplay.
- `FWBProductionMatchReplayRunner` reconstructs a fresh production bootstrap, verifies current legal IDs and decision context, and calls only `WBMatchCoordinator::SubmitActionId` for recorded decisions.
- The public receipt is deliberately separate from archive bytes and is not added to production startup JSON or presentation events.

## Codec Preservation

| File | Baseline SHA-256 | Baseline Git blob |
|---|---|---|
| `Source/WandboundCore/Public/WBActionCodec.h` | `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949` | `44ef87156beb5799066c2a5ecbc98f04928d98c0` |
| `Source/WandboundCore/Private/WBActionCodec.cpp` | `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99` | `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783` |

The replay layer consumes the stable IDs already exposed by current legal actions. It does not generate, normalize, decode, or version an alternate ID format.

## Privacy Boundary

The server-private archive contains the seed, chosen stable IDs, acting seat, source digests, coordinator checkpoints, protected state/trace/legal-set digests, and integrity hashes. It is stored only below `Saved/Wandbound/Replays` by default.

The public receipt contains only `available`, `schema_version`, `opaque_match_id`, `entry_count`, `complete`, `terminal`, `final_replay_digest`, and `failure_code`. It excludes seed, actions, legal lists, card zones, concealed identities, protected digests/traces, RNG state, and filesystem paths. No encryption or remote storage is claimed.
