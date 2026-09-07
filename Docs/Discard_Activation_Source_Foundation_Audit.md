# Discard Activation Source Foundation Audit

## Scope

This pass productionizes player-declared activated effects whose exact source card is in the declaring player's private Discard. It reuses the existing activation source gate, candidate expansion, pending-effect stack, response window, generic effect runner, replay, and public observation boundaries.

No production card was added. The validation card bundle is test-only.

## Existing Activation Architecture

| Source | Discovery | Exact instance | Source unit | Cost payer | Visibility | Pending frame |
| --- | --- | --- | --- | --- | --- | --- |
| Board | Board units | No | Board unit | Board unit | Public | Existing command |
| Hand | Ordered private Hand | Yes | None | Hero, by existing policy | Owner-private before declaration | Existing command |
| Equipped | Equipped entries | Yes | Equipped unit | Equipped unit | Existing equipped policy | Existing command |
| Discard | Ordered private Discard | Yes | None | None established | Owner-private before declaration | Existing command |

The pending frame already retains the complete activation command, including source zone and exact source-card instance, so no parallel frame or full zone snapshot was added.

## Existing Partial Discard Scaffolding

The source-zone enum, candidate source, activation command, and fixture ownership model already represented Discard. Production discovery, authoritative zone parity, parser support, runtime handoff, and expansion into `EWBCardZone::Discard` were the missing pieces.

## Production Discard Discovery

The coordinator and runtime provider enumerate only the acting/viewing player's authoritative Discard entries. Each entry resolves its CardId through the current repository and produces candidates only for effects explicitly gated to Discard. Duplicate CardIds remain separate exact instances and are sorted by established ordered-zone rules.

## Exact-Instance Authority

Discard declarations bind `SourceCardInstanceId`, CardId, owner, source zone, and effect ID. Candidate IDs include the exact source instance for Discard only. They never include a zone index. A different copy of the same CardId cannot satisfy declaration or resolution liveness.

## Source Gate And Liveness

Authoritative Discard parity validates a unique instance in `FWBCardZoneState`, exact owner, exact zone, CardId consistency, no source unit, legal timing, target, usage, non-terminal state, and supported effect definition. The same rules check runs during effect resolution. If the exact source has left Discard while pending, resolution fails closed and no duplicate copy substitutes.

The source remains in Discard while pending, after successful resolution, after negation, and after failed revalidation. Activation itself performs no card-zone mutation.

## Source Unit, Caster, And Provenance

Discard sources use `SourceUnitId = INDEX_NONE`. No Hero or other unit is fabricated as source or Caster. A player-selected activation still records `PlayerDeclared` activation provenance, and explicit targets retain player-declared target provenance.

## Timing And Negation

Normal-turn and response-window availability remains definition-driven. Discard creates no new timing or response system. Existing priority, passes, nested pending frames, LIFO resolution, and `NegatePendingEffect` apply unchanged.

## Once-Per-Turn

The default usage key for a Discard source includes the exact CardInstanceId, so duplicate copies do not collide. An explicit CardDB `once_per_turn_key` remains shared. Existing Board, Hand, and Equipped default keys remain byte-compatible.

## Cost Semantics

Costless Discard activations are supported. No canonical RR payer exists for a source with no unit, so an RR-bearing Discard effect without an already established payer fails closed. This pass does not charge the Hero or player globally.

## CardDB And Expansion

The parser accepts the stable token `discard` for explicit activated-effect source gates and still rejects production Deck activation and unknown tokens. Activation expansion maps Discard to `EWBCardZone::Discard` without changing existing Hand, Board, or Equipped mappings.

## Privacy

Discard remains private. Full Discard activation IDs are available only in the owner's viewer-scoped legal actions. Opponents receive count-only Discard observation and cannot enumerate exact instances, order, alternate candidates, or unrelated CardIds. The public receipt remains eight fields and excludes private instance IDs and protected digests.

## Replay And Determinism

The existing action/replay architecture records the selected exact activation action and the existing pending-effect sequence. Fresh coordinator replay verifies final state and trace digests. No RNG is introduced by Discard source discovery. Replay schema remains version 1 and `WBActionCodec` is unchanged.

## Explicit Deferrals

- Production card definitions using this source
- Summoning from Discard
- Automatic or zone-resident Discard triggers
- "When sent to Discard" transition events
- Zone movement prevention or replacement
- Deck activation
- Cross-player zone movement or ownership transfer
- An invented RR payer for Discard sources
- New effect operations, response windows, UI, networking, or AI

## Validation

- Focused automation: 5 succeeded, 0 failed, 0 warnings, with 156 focused assertions.
- Affected mature provider automation: 50 succeeded, 0 failed.
- Full Wandbound automation: 2,503 succeeded, 0 failed, 0 warnings, 0 not run. The verified baseline was 2,498, for a net increase of 5 tests.
- Editor and Game targets passed non-unity, default-unity, and forced-unity builds. The forced-unity runs used `-ForceUnity -DisableAdaptiveUnity` and therefore exercised true unity translation units.
- Clean BuildCookRun succeeded with a full cook, stage, package, and archive. AutomationTool exited with code 0.
- Production smoke covers pass-through resolution, existing effect negation, duplicate exact instances, source retention, per-instance usage, owner privacy, replay persistence, and fresh replay.
- The packaged Discard activation smoke passed twice from the staged package using package-relative fixture paths. Both runs were byte-identical: receipt `f0366395769ce0fd966b8f29d9debe8468dd393c38c44f5b93a7142024a2bcfd`, replay `2e2b2751add9662750ba35d2c34fe1425051bdf68df0fbda71c116580975730d`, and startup JSON `864ebdb8b12ed36c95287838916976017cb26034d4fda4459aa7fd51d12295e0`.
- Packaged final coordinator evidence is generation 1, revision 9, state digest `4811b2beb5f4a3fc8763d46a40244d308897cae62d1596dd90f9af53c1702fb7`, and trace digest `6bf3b239afbaaf4cbd4dc9c529380d762d7113974d41849ab1fa44ee5f5777e0`.
- The existing packaged pending-effect activation smoke also passed with exit code 0 and an eight-field public receipt, covering the mature activation regression.

## Exact Changed Files

1. `Data/Replay/DiscardActivationFixture/bundle_manifest.json`
2. `Data/Replay/DiscardActivationFixture/markers.json`
3. `Data/Replay/DiscardActivationFixture/match_spec.json`
4. `Data/Replay/DiscardActivationFixture/root_manifest.json`
5. `Data/Replay/DiscardActivationFixture/units.json`
6. `Docs/Discard_Activation_Source_Foundation_Audit.json`
7. `Docs/Discard_Activation_Source_Foundation_Audit.md`
8. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
9. `Source/WandboundCore/Private/WBCardActivationCandidateGenerator.cpp`
10. `Source/WandboundCore/Private/WBCardActivationExpansion.cpp`
11. `Source/WandboundCore/Private/WBCardActivationSourceGate.cpp`
12. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
13. `Source/WandboundCore/Private/WBRules.cpp`
14. `Source/WandboundCore/Public/WBCardActivationSourceGate.h`
15. `Source/WandboundRuntime/Private/WBProductionActivationDataProvider.cpp`
16. `Source/WandboundRuntime/Private/WBProductionActivationExecutionHandoff.cpp`
17. `Source/WandboundRuntime/Private/WBProductionDiscardActivationSmoke.cpp`
18. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
19. `Source/WandboundRuntime/Public/WBProductionActivationDataProvider.h`
20. `Source/WandboundRuntime/Public/WBProductionDiscardActivationSmoke.h`
21. `Source/WandboundRuntime/WandboundRuntime.Build.cs`
22. `Source/WandboundTests/Private/WBDiscardActivationSourceTests.cpp`
