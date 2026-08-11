# Production Reaction-Window Audit

## Baseline

- Commit: `e1412b0f1e22a8b61fe11e78894dfa052c5746c7`
- Branch synchronization: `main` and `origin/main` both point to the baseline commit.
- Initial Wandbound automation count: 2,122 passed.
- Replay schema: version 1.
- Canonical startup hash: `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Partial replay hashes: archive `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463`, receipt `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9`, replay `391f0a6e836fc19439f110a5bd0a748367c00c826e73ad1d615fe53d9b492e7e`.
- Terminal replay hashes: archive `4e30424a56b613cbbda225295a0775473ed661cda390f172b609e529450235cc`, receipt `5bcce2e1e9361e8848e4757a634cf82acdee30d2463a01f6f9f0023157e1ca76`, replay `95a9bd178298085120774097e8beea17a266b4ec6c048aa49dc9351f9c50dc6b`.
- Hybrid Hero hashes: archive `e1fa69301728e8129e69866ce0a91fbeaf77cc990d08f0a33133943bc629be20`, receipt `7cdba9356c9fbb6c796aaaedfbeef7fc884ec74522a4a20231d082961cc6f156`, replay `9c73493e6931a627969a7472b49c858a623150de49da522ef963663f41e3f98e`, state `9abce6721fb022c2769f8207b4dd07c79811a65bb8fc25f97962feb4e09f1897`, trace `fdc47bcbab4c83987b4c9749cf1e905074a90a6d6fd914f4606201b8c83b7703`.
- Non-Hero Hybrid hashes: archive `f3b0cb64cb2bd45ae6816ddc871b0a08d89cac55272110a102e94fb666380c1d`, receipt `f3dab3075d43922bd1bcd59e377370f69bb848d5268a04d8bbfc4c5b9e1aa3f1`, replay `e84384339113dc407708b223ef29532eeee595e4da3b3434c6ba97159369bac4`, state `4c30735d3f70def1c8ca7c3c63787ac71f364399a1f32da7a74d2de0605803ba`, trace `c4c3e2fed801da6de224b9ab5b76563e2e1ea37127855d17a9f72eadefc846b3`.
- `WBActionCodec.h`: source SHA-256 `c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949`, Git blob `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- `WBActionCodec.cpp`: source SHA-256 `ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99`, Git blob `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`.
- No staged files. Existing tracked and untracked working-copy changes were recorded before this pass and are outside the reaction implementation.

## Canonical Findings

Tracked canon defines exactly five windows: pre-hit, post-hit, post-move, post-summon, and post-effect. Priority alternates, two total consecutive passes close a window, and a player with no legal response auto-passes.

The read-only Godot reference establishes the details omitted by the prose rules:

- Post-summon and post-move first priority is the opponent of the originating player.
- A legal React resets the consecutive pass count to zero and transfers priority.
- Manual passes are decisions; forced passes are automatic resolution events.
- Marker resolution occurs before the post-move/post-summon checkpoint.
- Initial Hero setup suppresses manual Reacts.
- Godot has pending-event continuations and a response stack for attack/effect flows.

## Component Matrix

| Path / symbol | Current role | Production reachable | Window / priority / pass behavior | Legal React source | State / replay / privacy | Baseline dirty / overlap | Required change |
|---|---|---:|---|---|---|---|---|
| `WBGameStateData.h` / `FWBGameStateData` | Rules state | Yes | Generic `Response` phase and priority only | N/A | State digest includes phase/priority | Clean / none | Add typed reaction state without overloading pending attack. |
| `WBGameStateData.cpp` / reaction helpers | Rules-state behavior | Yes | No typed lifecycle before this pass | N/A | Private deterministic state | Dirty / existing RL and turn-start edits are separate; no semantic overlap | Add only typed state open/reset helpers. |
| `WBMatchCoordinator.cpp` / legal enumeration and submission | Sole production action and transition authority | Yes | Accepts Response but only legacy pass is generated | Existing activation candidates | Accepted actions are replay-recorded; observations are viewer-scoped | Clean / none | Own open, pass, React, auto-pass, close, and continuation. |
| `WBAction.h` / action families | Stable rules action model | Yes | `PassResponse` already exists | Existing activation family | Replay-facing IDs remain stable | Clean / none | No change. |
| `WBActionCodec.h/.cpp` | Legacy/core action ID and JSON codec | Yes | Existing `PassResponse` codec behavior | Existing activation IDs are outside codec changes | Version/hash guarded | Clean / none | No change; source and blobs must remain byte-identical. |
| `WBRules.cpp` / `PassResponse` | Compatibility core action legality | Yes through coordinator | One pass is legal for priority | None | Stable `pass_response:pN` ID | Clean / none | Preserve ID and legality; coordinator supplies canonical two-pass lifecycle. |
| `WBEffectRunner.cpp` / `ApplyPassResponse` | Legacy raw-state compatibility mutation | Yes for legacy adapters | One pass closes immediately | None | Existing trace shape | Clean / none | Preserve compatibility behavior; production coordinator no longer delegates response lifecycle to it. |
| `WBReplayTrace.h/.cpp` / `FWBTraceEvent` | Deterministic trace model | Yes | No typed window metadata before this pass | N/A | Private replay trace | Clean / none | Add optional public-safe kind/pass metadata without schema changes. |
| `WBCardActivationSourceGate` | Effect source/timing legality | Yes | `ResponseWindow` currently fails closed | Board/equipped fixture context | Does not expose candidates itself | Clean / none | Permit exact typed Response timing for current priority only. |
| `WBCardActivationCandidateGenerator` | Expands legal effect commands | Yes | Timing delegated to source gate | Existing card definitions/source gates | Private candidates remain internal | Clean / none | No architecture change; coordinator supplies Response-filtered sources. |
| `WBCardActivationLegalActionGenerator` | Stable activation action IDs | Yes | Timing-agnostic | Existing candidate IDs | Public label only | Clean / none | Reuse unchanged. |
| `WBCardActivationCommand` / `WBEffectRunner::ApplyCardActivationCommand` | Applies effect/cost/usage atomically | Yes | Immediate effect resolution | Existing effect payloads | Trace-producing state mutation | Clean / none | Reuse for React; reset passes and transfer priority after success. |
| `WBSummonExecution` | Normal Character summon | Yes | No response checkpoint | N/A | Hidden hand identity moves to public board | Clean / none | Coordinator opens post-summon after marker/automatic terminal resolution when a legal React exists. |
| `WBHybridSummon` | Atomic Hero replacement and non-Hero Hybrid summon | Yes | No response checkpoint | N/A | Payment identities remain protected | Clean / none | Same post-summon checkpoint after atomic replacement and marker resolution. |
| `WBMarkerResolution` | Immediate marker consequences | Yes | Resolves before current automatic death handling | N/A | Hidden marker contents are not broadly exposed | Clean / none | Preserve ordering; terminal state suppresses window. |
| Move execution in coordinator | Move then marker resolution | Yes | No response checkpoint | N/A | Stable move ID/replay | Clean / none | Open post-move only when a legal React exists. |
| Attack execution in coordinator | Declare then immediately apply pending damage | Yes | No pre-hit/post-hit suspension; no counter stage | None | Stable attack ID/replay | Clean / none | Preserve current behavior. Unified attack windows are blocked on production continuation staging. |
| Activation execution in coordinator | Resolves effect immediately | Yes | No pending pre-effect continuation | Normal-turn activation | Existing cost/effect traces | Clean / none | Preserve current behavior. Do not mislabel an after-resolution window as Godot's pending-effect response. |
| `WBProductionMatchReplay` | Hash-chained action replay | Yes | Records accepted decisions and trace ranges | Coordinator legal set | Schema 1; private digests not in public receipt | Clean / none | Include open typed state in state digest and automatic lifecycle in trace digest without schema change. |
| `WBProductionMatchReplayRuntime` | Production recorder, persistence, and fresh replay | Yes | Captures coordinator commits and automatic traces | Coordinator legal set | Eight-field public receipt boundary | Clean / none | Reuse unchanged; verify reaction replay parity. |
| `WBPublicTurnSummary` / observation | Public current/priority/phase | Yes | Already reports Response and priority | Viewer sees actions only at priority | Opponent zones remain filtered | Clean / none | Typed kind may be public while open; legal candidates remain viewer-authorized. |
| Runtime decision loop / `UWBRuntimeMatchHostComponent` | Displays coordinator observations and submits IDs | Yes | No independent response state | Coordinator legal actions | No direct hidden-state access | Clean / none | Reuse unchanged; coordinator remains sole mutator. |
| Presentation translator / public action view | Translates public observations | Yes | Reflects coordinator phase and legal actions | Viewer-authorized actions only | No private candidate access | Existing unrelated dirt / no overlap | No change; UI remains selection-only. |
| Initial Hero setup | Atomic simultaneous setup and ordered triggers | Yes | Manual React suppression already explicit | None | Setup traces deterministic | Clean / none | Preserve suppression; no setup post-summon windows. |

## Supported Scope

This pass can safely implement typed state, post-summon, post-move, response-timed activation commands, canonical pass progression, deterministic auto-pass, terminal clearing, replay traces/digests, and public observation.

## Canonical Blockers

- **Attack:** Unreal currently applies damage immediately after declaration. Pre-hit/post-hit integration requires an explicit suspended continuation through damage, Frozen handling, counter declaration, and counter completion. Reusing the typed state without that continuation would duplicate or skip damage.
- **Post-effect:** Godot opens `effect_activation_pending` before resolving the effect, while the current Unreal command applies the effect immediately. A post-resolution window would change the established timing. The required pending-effect continuation is not present.
- **Nested windows:** Godot has a response stack, but the tracked Unreal architecture has no canonical stack representation. A React that would open another window must remain fail-closed; unrestricted nesting is not added.
