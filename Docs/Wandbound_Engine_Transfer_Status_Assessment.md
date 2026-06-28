# Wandbound Engine Transfer Status Assessment

Date: 2026-06-28

This assessment reflects the current Unreal migration state and the strategic pivot from additional test-only CardDB/importer reporting toward production-facing vertical-slice work.

## Current Strengths

- Deterministic rules kernel progress is substantial. Movement, turn control, status ticks, attacks, damage, armor, death cleanup, public summaries, replay traces, action IDs, replay verification, and selected-action execution already live in headless C++.
- The EffectRunner mutation boundary is well established. Rules validate legality, then EffectRunner applies state changes and emits deterministic traces.
- Legal action and activation action scaffolding exists. Normal `FWBAction` generation covers core gameplay actions, while activation candidates/legal actions remain a separate family with stable IDs, target presentation, affordability, cost, usage, and payment scaffolding.
- Runtime activation/session/provider spine exists. Runtime components consume externally supplied normal legal actions, activation legal actions, and public summaries rather than becoming the rules owner.
- Replay, trace, and automation coverage are deep. The latest reported Wandbound automation baseline is 1031 succeeded, 0 failed, 0 warnings.
- Hidden-information discipline is explicit. Reports and tests repeatedly guard against leaking source effect IDs, debug activation IDs, usage keys, hidden hand/deck/discard data, marker identity, and raw schema internals.
- CardDB schema/import readiness test coverage is broad. The test-only branch validates schema shape, bundles, dependencies, source-version compatibility, readiness, diagnostic summaries, batch readiness, manifest evaluation, and suite aggregation.

## Current Bottlenecks

- No production Deck / Hand / Discard / Equipped zone state exists.
- No player-perspective observation model exists for public/private zone views.
- No production CardDB loader exists.
- No production card definition repository exists.
- No production activation data provider exists.
- No real draw/hand/discard loop exists.
- No real summon flow exists.
- No real equip/wand/RL overflow flow exists.
- No marker state/reveal model exists.
- No response window/negation model exists.
- No actual UI target picking exists.
- No full game controller/session ownership exists for a playable match loop.
- No playable game loop exists.

## Efficiency Assessment

The prior correctness-first route was useful. It built a strong deterministic C++ base, clarified ownership boundaries, and reduced migration risk before visual or runtime work could drift into rules ownership.

Continuing more test-only CardDB report/export layers is no longer the most efficient route. The existing validators and reports have proven the intended schema/import direction well enough for now. More validation-only layers would mainly refine a future importer that still cannot feed a playable game because production zones, observation, provider ownership, and card movement are missing.

The project should now prioritize production-facing zone/card/provider and playable vertical-slice work. The next efficient step is to add production-safe card zone state and a player-perspective observation model, then connect a minimal Unreal-owned card definition repository to a production activation data provider skeleton.

## Progress Estimate

These are rough planning ranges, not precise completion claims.

| Area | Estimated Progress |
| --- | --- |
| Rules kernel foundation | 70-80% |
| Full mechanics coverage | 35-50% |
| Runtime backend/session flow | 50-65% |
| Visual/playable implementation | 10-20% |
| Production CardDB/import | 10-20% |
| Playable vertical slice | 20-30% |
| Full engine transfer | 30-45% |

The main gap is not core determinism anymore. The main gap is production-facing gameplay state and the path from that state into a playable decision loop.
