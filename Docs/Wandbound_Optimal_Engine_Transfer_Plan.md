# Wandbound Optimal Engine Transfer Plan

Date: 2026-06-28

## Plan Goal

The goal is to reach a playable Unreal vertical slice as efficiently as possible while preserving deterministic rules correctness and future AI-readiness.

The route stays rules-kernel-first: Rules validate legality, EffectRunner mutates state, runtime consumes externally supplied action/summary data, and UI selects only.

## What Stops Now

Pause the test-only CardDB/importer expansion branch after the current work.

Specifically pause:

- additional test-only CardDB export/reporting passes
- additional manifest/suite/reporting layers
- nonessential planning-only validation unless directly blocking production work

Do not delete existing work. The current test-only CardDB scaffolding remains valuable reference and safety coverage. The change is priority: stop extending validation-only layers while the production vertical slice still lacks zones, observation, provider implementation, and card movement.

## What Starts Now

Prioritize production-facing gameplay plumbing:

- production-safe zone state
- player-perspective observation
- minimal production CardDB fixture loader
- production activation data provider skeleton
- draw/hand/discard loop
- board/hand activation vertical slice
- summon/equip/wand foundation
- playable runtime loop

## Phase 1 - Production Zone State and Observation

Milestones:

1. Add production-safe card zone state structs.
2. Add read-only zone query APIs.
3. Add player-perspective observation model.
4. Add public/private hidden-info tests.
5. Keep runtime from owning rules state.

Scope:

- Model Deck, Hand, Discard, Equipped, Board, and a Markers placeholder.
- Store ownership and viewer scope explicitly.
- Treat hidden zones as private unless an observation policy permits exposure.
- Keep Deck identity hidden by default.
- Keep opponent Hand hidden by default.
- Keep marker identity hidden until marker state/reveal rules exist.
- Include Discard visibility as a canon decision point, using fail-closed owner scoping until policy is confirmed.
- Add no UI.
- Add no Godot CardDB import.

Success criteria:

- WandboundCore has deterministic zone structs and read-only query surfaces.
- Tests prove own/private and opponent/public observation differ correctly.
- Source/WandboundRuntime still does not own mutable rules state.

## Phase 2 - Minimal Production Card Definition Repository

Milestones:

1. Add production-safe `CardDefinitionRepository` shell.
2. Load a small Unreal-owned fixture bundle or embedded test repository.
3. Use the already validated schema shape.
4. Fail closed on unsupported payloads.
5. Do not import Godot CardDB yet.

Clarification:

- This is not full CardDB import.
- This creates enough Unreal-owned card definitions for a vertical slice.
- The repository should map into existing `FWBCardDefinition` and `FWBCardEffectDefinition` shapes where practical.
- Unsupported payloads, unsupported source zones, and unsupported timings should return explicit diagnostics.

Success criteria:

- A small production-safe card definition set can be queried by card id.
- It is not Godot runtime loading.
- It is enough to drive one or two board/hand activations.

## Phase 3 - Production Activation Data Provider Skeleton

Milestones:

1. Implement an `IWBRuntimeActivationDataProvider`-backed skeleton.
2. Consume zone state plus card definitions.
3. Produce externally owned normal legal actions, activation legal actions, and public summary.
4. Keep activation separate from `FWBAction` initially.
5. Add tests proving runtime still does not generate actions internally.

Scope:

- The provider may coordinate with authorized core systems.
- Runtime remains a consumer of provider output.
- The provider should surface deterministic diagnostics when required inputs are missing.
- `WBActionCodec` remains unchanged.
- `WBRules::GenerateLegalActions` remains unchanged.

Success criteria:

- The existing runtime activation decision-session facade can refresh from production-shaped provider output.
- Hidden zones do not leak through provider output.

## Phase 4 - Playable Board/Hand Activation Vertical Slice

Milestones:

1. Board-source activation from a visible unit.
2. Hand-source activation from own hand.
3. Unit target selection from precomputed target options.
4. Cost gate, payment, and usage marking.
5. Runtime execute plus post-action refresh.
6. Public presentation update.

Scope:

- No UI widgets unless explicitly requested.
- Drive this through C++ harness/components first.
- Use existing activation command/effect scaffolding.
- Keep activation separate from normal `FWBAction` until explicitly revisited.

Success criteria:

- A C++ vertical-slice harness can present legal board/hand activations, select one, execute it, mutate state, and refresh presentation from post-action provider data.

## Phase 5 - Draw / Hand / Discard Loop

Milestones:

1. Turn-start draw.
2. First-player first-turn draw exception.
3. Hand ownership.
4. Card movement after activation when appropriate.
5. Discard visibility policy.
6. Replay/trace coverage.

Scope:

- Start with deterministic explicit deck order for tests.
- Do not add random shuffling until RNG ownership is defined.
- Do not expose opponent hidden hand.
- Do not implement response-window discard timing yet.

Success criteria:

- Turn start can move cards from Deck to Hand.
- Played cards can leave Hand and enter the correct post-resolution zone when supported.
- Public/private summaries remain deterministic.

## Phase 6 - Summon / Equip / Wand / RL Overflow Foundation

Milestones:

1. Summon legality adjacent to Hero.
2. Unit cap enforcement with real zones.
3. Equip source zone and equipped attachment state.
4. RL Used / Available RL integration.
5. Overflow policy scaffolding.
6. Wand destruction deferred or minimally scaffolded according to canon.

Scope:

- Use Rules Bible v2 and Godot reference for behavior.
- Add only the minimum production state needed for the vertical slice.
- Keep death-trigger, discard-trigger, and response side effects out unless required by canon for the chosen slice.

Success criteria:

- The project has a production-shaped path for summoning and equipping that can later support real card data.

## Phase 7 - Markers and NPC Phase

Milestones:

1. Production marker state.
2. Player-perspective marker identity.
3. Trap trigger.
4. NPC reveal/spawn scheduling.
5. NPC phase baseline.

Scope:

- Marker identity must remain hidden unless viewer policy allows it.
- NPC phase should be deterministic and traceable before visual presentation.
- Avoid AI behavior beyond a simple deterministic baseline unless explicitly requested.

Success criteria:

- Marker and NPC state can exist in the same authoritative model as board and card zones without hidden-info leaks.

## Phase 8 - Response Windows / Negation / Pass Priority

Milestones:

1. Activation declaration vs resolution split.
2. Pending activation state.
3. Response window priority loop.
4. Pass/pass-response semantics.
5. Negation placeholder.
6. Replay trace coverage.

Scope:

- Do not retrofit response logic into existing immediate-resolution paths without explicit trace and replay tests.
- Keep response priority deterministic.
- Confirm how activation action IDs relate to `FWBAction` before changing public action contracts.

Success criteria:

- A pending activation can open a response window, accept pass/pass-response, and resolve or be negated deterministically.

## Phase 9 - Playable UI and Visual Integration

Milestones:

1. Real C++ UI-facing view models.
2. Board click/target picking.
3. Hand display.
4. Activation target selection UI.
5. Execute selected action.
6. Post-action refresh.
7. Camera/input/VFX later.

Scope:

- UI selects only.
- UI consumes public/player-scoped view models.
- UI does not inspect hidden zones directly.
- Blueprints and assets remain out of scope unless explicitly requested.

Success criteria:

- A player can make the vertical-slice decisions through presentation surfaces while rules remain in C++.

## Phase 10 - AI-Readiness and Solo-Mode Foundation

Milestones:

1. Observation/action/trace API.
2. Replay dataset schema.
3. Baseline random/heuristic bot.
4. No-hidden-info leakage tests.
5. Deterministic simulation.
6. Later ML/search integration.

Scope:

- Build AI readiness on the same player-perspective observation and legal-action APIs used by runtime/UI.
- Avoid privileged hidden-state access except in explicitly marked omniscient debug tooling.

Success criteria:

- The vertical slice can be driven by a deterministic bot or replay runner using the same public contracts future solo mode and AI systems need.
