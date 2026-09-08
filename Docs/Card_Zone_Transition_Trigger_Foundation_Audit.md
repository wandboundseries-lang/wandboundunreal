# Card Zone Transition Trigger Foundation Audit

## Baseline

- Public baseline: `bd25b4e07d99480cfa6046d65281dfc977a995e3`
- Branch: `main`
- Local and fetched public heads matched before implementation.
- No production card definition was added or changed.

## Architecture

The existing `CardZoneTransition` event remains the sole historical event fact. `WBCardZoneMutation` is structural only, and `WBCardLifecycle` remains responsible for committed snapshots. `WBCardZoneTransitionTrigger` is the single semantic consumer for this trigger family.

`FWBCardDefinition` now owns a dedicated `CardZoneTransitionTriggers` array. Each definition has a typed source scope, explicit optional source-zone, destination-zone, and cause filters, nonnegative `DrawCount`, and a mandatory flag. `Unknown` is never used as a wildcard when a filter-enable flag is set. An explicit zero count resolves without drawing; a failed positive draw is never converted to zero.

## Source Semantics

- `MovedCardSelf` captures the exact transitioned card. It remains eligible from its immutable historical snapshot even after leaving Discard.
- `ResidentDiscardObserver` enumerates the transition owner's authoritative Discard after the transition commits. Exact sources are ordered by `ZoneIndex`, then instance ID, then trigger ID.
- A newly entered Discard card may observe its own transition when it has a separate resident-observer definition. Self and resident definitions remain separate trigger instances.
- Opponent, Hand-resident, Deck-resident, Board-resident, and cross-owner observation are unsupported.

Sources capture exact instance ID, CardId, owner, source scope, TriggerId, snapshot eligibility, and resident collection position where applicable. They never fabricate a UnitId, Hero, or Caster.

A card absent from a partial repository has no discoverable trigger definition and is inert for this family. Other known resident sources still observe its transition. Malformed definitions in the repository remain rejected. This preserves existing legacy fixture compatibility without fabricating any missing trigger.

## Resolution

Mandatory `DrawCount` outcomes use `WBCardLifecycle::DrawOneCard` with `Effect` cause and deterministic continuation identity. Nested Deck-to-Hand transitions enter a FIFO work queue. The queue and trigger count share a deterministic guard of 256 operations. Guard, parser, optional-trigger, and impossible-draw failures fail closed.

Production semantic transactions consume snapshots for normal Discard, Hand activation cost, turn-start draw, turn-start-trigger draw, and CSN Inheritance draw. A failed outer operation commits neither trigger state nor trigger traces. Setup snapshots remain suppressed. ExtractExact, summon/Board, and Equipped transitions remain excluded.

## Privacy And Replay

Public trigger traces disclose only owner, turn, count, ordering, and successful automatic resolution. They omit moved/source exact IDs, CardIds, Discard ordering, alternatives, and stable private trigger identity. Automatic provenance is resolution-only. No action, reaction window, priority mutation, or player choice is created.

Replay schema remains 1. Public receipts remain exactly eight fields. `WBActionCodec` is unchanged. Stable internal trigger identity uses the root event, source scope, exact instance, TriggerId, and collection order; it uses no RNG, GUID, clock, or pointer.

## Canon And Reference Audit

Tracked Unreal canon establishes private ordered zones, exact transition snapshots, setup suppression, and the six supported ordered-zone pairs. The read-only Godot reference confirms direct Deck/Hand/Discard mutation and historical discard flows but does not provide a reusable generic exact-instance observer pipeline. The owner-specified semantics in this pass therefore define the synthetic foundation; no Godot file was modified or loaded at runtime.

## Validation

- Focused suite: 6 succeeded, 0 failed, 0 not run; 138 direct assertion sites plus looped checks. All six also passed in the final affected and full reports.
- Early Editor non-unity builds: succeeded.
- Production parser fixture and in-editor production smoke: succeeded.
- Required affected families: 887 succeeded, 0 failed, 0 warnings, 0 not run.
- Full automation: 2,515 succeeded, 0 failed, 0 warnings, 0 not run; +6 over 2,509.
- Final Editor/Game non-unity: succeeded, 19.74 / 48.48 seconds, using -DisableUnity -DisableAdaptiveUnity.
- Final Editor/Game default unity: succeeded, 45.29 / 62.77 seconds.
- Final Editor/Game forced unity: succeeded, 41.93 / 58.46 seconds, using -ForceUnity -DisableAdaptiveUnity.
- All six final-source builds used MaxParallelActions=2. Logs are under Saved/Logs/ZoneTriggerFinal-*.
- Final reports: Saved/AutomationReports/ZoneTriggerFinalAffected/index.json and ZoneTriggerFinalFull/index.json.
- Final clean BuildCookRun: succeeded, 417 actions, 558.62 seconds, exit 0; zero build/cook error or warning lines.
- Packaged trigger smoke: two successful byte-identical receipts, six resolved triggers and nine processed transitions per run. Covers self-enter, self-leave, resident duplicate sources, nested draws, redaction, and ExtractExact exclusion.
- Packaged transition, Discard activation, and summon-negation regressions: two successful runs each, with byte-identical receipt pairs. Discard and summon regressions persist archives and verify fresh coordinator replay; their archive and receipt hashes match the committed transition-event baseline.
- Each packaged run reported zero errors and the same 21 runtime warnings: ten missing Datasmith material references reported twice, and one SpawnActor call with no class. No asset/plugin change was made for these shared warnings.
- Staged fixture bytes match all four source files. All packaged smoke processes exited.

Trigger receipt SHA-256: `9b4a8c016857a0b1134f460654dfe20129e94c48b2cf8997b4b311de56ddd856`.
Trigger combined replay digest: `ed89ba26461b91dd7574b10582eccf925b52786e3134c411c1e23893a63d414e`.
Transition receipt baseline preserved: `fd5bf90940652eb1801c59ff30d6de0d68723fe621175e2966dc98aa27c1b674`.
Discard archive baseline preserved: `4da54005ac0291c51d505893fc0ae60eae06524440c56ce817f23d4d0c9f3abb`.
Discard receipt baseline preserved: `a3149eea66a733adaab235e817c4a1b1098a24112b27b4ccc6125dc61ec5c4bb`.
Summon archive baseline preserved: `5df7faac15f1da0b1124efb80150b33a95de45afeaafbde96407ef549c22f83d`.
Summon receipt baseline preserved: `511587e612c2a087e0c7b6813ec9817d3ab2ad4add1a342fd88521b7651335e9`.
Summon startup hash preserved: `210da863cb6587a48e8dd31e502e04a8f5b57b5cf765e5495353ca3b0daac360`.

The initial full suite caught a legacy partial-repository lookup failure and a command-line smoke source-guard overlap. Both were corrected; targeted retries and the full suite passed. The first packaged run found a double-prefixed relative fixture path, corrected by passing Data/... directly to the production loader. The initial commands labeled non-unity only disabled adaptive unity; definitive non-unity evidence uses -DisableUnity -DisableAdaptiveUnity.

## Exclusions And Deferrals

No production trigger cards, optional triggers, owner-selected ordering, opponent observers, alternate resident zones, selected targets, private choices, trigger negation, reaction windows, Board/Equipped transitions, or new action IDs were added. Empty-Deck behavior remains fail-closed because canon does not establish partial mandatory draw resolution.

## Exact Changed Paths

Both audits list the same 24 task paths: 13 tracked modifications and 11 new files. The original 23-path list expanded by one existing test file for the narrow runtime source-guard correction. No staged paths. Three pre-existing unrelated untracked paths remain preserved. Protected history remains non-ancestor of public HEAD and unreachable from fetched remotes; the audits contain no protected identifiers.

1. `Data/CardDB/ProductionCardDB.schema.json`
2. `Data/Replay/CardZoneTransitionTriggerFixture/bundle_manifest.json`
3. `Data/Replay/CardZoneTransitionTriggerFixture/markers.json`
4. `Data/Replay/CardZoneTransitionTriggerFixture/root_manifest.json`
5. `Data/Replay/CardZoneTransitionTriggerFixture/units.json`
6. `Docs/Card_Zone_Transition_Trigger_Foundation_Audit.json`
7. `Docs/Card_Zone_Transition_Trigger_Foundation_Audit.md`
8. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
9. `Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp`
10. `Source/WandboundCore/Private/WBCardDefinitionFixtureLoader.cpp`
11. `Source/WandboundCore/Private/WBCardDefinitionRepository.cpp`
12. `Source/WandboundCore/Private/WBCardZoneTransitionTrigger.cpp`
13. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
14. `Source/WandboundCore/Private/WBTurnStartSequence.cpp`
15. `Source/WandboundCore/Public/WBCardDefinition.h`
16. `Source/WandboundCore/Public/WBCardZoneTransitionTrigger.h`
17. `Source/WandboundRuntime/Private/WBProductionCardZoneTransitionTriggerSmoke.cpp`
18. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
19. `Source/WandboundRuntime/Public/WBProductionCardZoneTransitionTriggerSmoke.h`
20. `Source/WandboundRuntime/WandboundRuntime.Build.cs`
21. `Source/WandboundTests/Private/WBCardZoneObservationTests.cpp`
22. `Source/WandboundTests/Private/WBCardZoneStateTests.cpp`
23. `Source/WandboundTests/Private/WBCardZoneTransitionTriggerTests.cpp`
24. `Source/WandboundTests/Private/WBCardDefinitionRepositoryTests.cpp`
