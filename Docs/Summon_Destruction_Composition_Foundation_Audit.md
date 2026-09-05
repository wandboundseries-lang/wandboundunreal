# Summon and Destruction Composition Foundation

## Baseline and scope

Public HEAD and `origin/main` are
`72cfc15f5b7c89970658f5c5aec6fbfa4e33d20d`. Baseline automation is 2,473.
No files were staged. Three unrelated untracked paths were preserved. A private
legacy inventory was consulted read-only for capability coverage; its contents
are not reproduced here.

## Existing lifecycle inventory

| Path | Operation | Genuine destruction | Sacrifice | Source zone | Exact instance | Creates unit | Queues destruction | Special policy |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | --- |
| Normal Character summon | Summon | No | No | Hand | Yes | Yes | No | Normal summon conditions |
| Rook continuation | Summon after destruction | Prior event | No | Deck | Yes | Yes | No additional event | Ignore summon conditions; inheritance remains separate |
| Patch continuation | Summon after sacrifice | No | Yes | Deck | Yes | Yes | No | Sacrifice remains non-destruction; inheritance remains separate |
| Crash-In | Destroy then summon | Yes | No | Hand | Yes | Yes | Exactly one | Preserve and redirect pending attack; detached equipment for inheritance |
| Zero-HP cleanup | Destruction | Yes | No | None | N/A | No | Exactly one | Normal equipment, pending attack, and terminal cleanup |
| Direct effect destruction | Destruction | Yes | No | None | N/A | No | Exactly one | Positive HP allowed without fabricated damage |
| Initial Hero setup | Setup spawn | No | No | Setup authority | Existing policy | Yes | No | Deliberately not migrated |
| Hybrid summon | Specialized summon | Existing sacrifice policy | Yes | Existing authority | Yes | Yes | Existing behavior | Deliberately not migrated |
| NPC and marker creation | Specialized spawn | No | No | Existing authority | N/A | Specialized | No | Deliberately not migrated |

## Implemented contract

`WBCharacterConstruction` is the shared normal Character constructor and live-unit
ID allocator. It initializes explicit Owner and Controller, card identity, tile,
HP/MaxHP, ATK, AR, BaseRL/CurrentRL/RL compatibility, RLUsed, attacks, and movement
defaults from an immutable validated definition. Allocation is deterministic
`max(live unit IDs) + 1` and fails closed at integer overflow.

`WBCharacterSummon` consumes one exact Deck or Hand instance, validates the
authoritative card identity and ownership, applies a typed normal-versus-ignore
conditions policy, constructs the unit through the shared authority, preserves
remaining zone order, validates final zone state, and commits atomically. It does
not enumerate private candidates and does not contain card-specific behavior.

`WBDeathResolution::ApplyGenuineUnitDestruction` is the shared typed destruction
authority for zero-HP cleanup and direct effect destruction. It uses the existing
prevention hook, captures the established immutable snapshot, supports positive-HP
destruction without Damage or AfterDamage semantics, handles explicit equipment,
pending-attack, and terminal policies, queues one deterministic destruction event,
validates zones, and commits a working copy only on success.

`WBSummonDestructionComposition` performs required destruction and exact summon on
one working game state. It validates the final board after the target is removed,
so a one-for-one replacement is legal at unit cap and may occupy the target's
vacated tile. Optional CSN inheritance and pending-attack redirect remain delegated
to their existing authorities. Hero terminal commit preserves established ordering.
Any failure leaves the input state unchanged.

## Compatibility decisions

- Rook and Patch continue through `WBDeckSummon`, now backed by shared construction
  and exact Deck consumption. Choice, continuation, no-shuffle, and inheritance
  behavior are unchanged.
- Patch sacrifice is not routed through destruction and produces no destruction
  event or observer trigger.
- Crash-In delegates lifecycle mutation to the atomic composition while retaining
  activation-time exact Hand choice, response timing, declaration provenance, and
  combat redirect authority.
- Sable and other observers consume the same deterministic destruction snapshot
  queue; no duplicate event is introduced.
- Ordinary destruction discards equipment. Composition may explicitly detach the
  captured equipment for the established inheritance continuation.
- Ordinary destruction clears a participating pending attack. Crash-In explicitly
  preserves it until the existing redirect authority accepts the new defender.
- Initial Hero, Hybrid, NPC, marker, setup suppression, and summon-reaction policy
  are unchanged.

## Replay and privacy

Construction, allocation, snapshots, event IDs, ordering, exact-instance
consumption, traces, and transaction commits are deterministic. Public traces do
not enumerate private alternatives and include an exact selected instance only
when the existing caller contract requires private trace evidence. No new action
family was added. `WBActionCodec` is byte-identical to the baseline, replay schema
remains 1, and the public receipt remains eight fields.

## Validation

The focused suite contains 8 automation cases and 135 assertions. It passes 8/8.
The consolidated affected-system regression passes 459 tests with zero failures,
warnings, or not-run tests. Full automation passes 2,481 tests with zero failures,
warnings, or not-run tests, an increase of 8 from the 2,473 baseline.

All six requested build modes pass against final source: Editor non-unity (132
actions, 217.31 seconds), Game non-unity (146 actions, 294.80 seconds), Editor
default (5 actions, 13.64 seconds), Game default (6 actions, 97.41 seconds), Editor
forced-unity (6 actions, 22.24 seconds), and Game forced-unity (4 actions, 43.91
seconds). The first forced-unity compile exposed anonymous-namespace helper name
collisions; the new lifecycle helpers were given authority-specific names and both
forced-unity targets then passed without changing behavior.

The final clean BuildCookRun passes build, cook, stage, package, and archive with
exit code 0: 405 compile actions, 459 cooked packages, 732.78 seconds for
BuildCookRun, and 12 minutes 14 seconds total AutomationTool time. The packaged
Crash-In and Sable production smokes each pass twice with exit code 0. Each smoke
loads its persisted archive into a fresh coordinator and verifies record count,
state digest, trace digest, generation, and revision before succeeding.

Repeated packaged artifacts are byte-identical. Crash-In hashes are archive
`687aacb3c0faf57e0bce3e8bbdcb580b957d48d066ad1b71f7ef33ed6c2915f4`,
receipt `76a8ff0025fa4137a35262f52f43a89d24e1579871cccb87fcd5b51ce3c28701`,
and startup `c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c`.
Its final state, trace, and replay digests are respectively
`a274b44a9894faf29609a5945c747a27d3ef663ad59055a31926125819b52a0b`,
`a3ceda9dce70c0798d737f412ff3c1b08025e3fa3d36d09a12599a0f5992b8f4`,
and `2c94f9e99b0ad17bde1463e1f4d6e62cccaad19081bd143abd31c51f7bb8671c`.
Sable hashes are archive
`c85c0086cfc6902e5919d81bf45cbcc3b9a11fabbc5282f988555ef12cc2775b`,
receipt `580e3c5577c002225c752b358986b0cfe4ef844bd5e8e6208bbbccd0d2c1b642`,
and the same startup hash. Its final state, trace, and replay digests are
`64e55c1bce7e969938bc4f3e64561604b7d54a38f4cad55bf810b2a8ca4d0b39`,
`1a86686d391a7fa20cc003671461478bf7a5e4357eae772e573d9f9b1b589f52`,
and `d9ffbcdba36a5740b72873310159d9eb5fedf55a489d56913d894174f30e6661`.
Both receipts contain exactly eight fields, and public receipt/startup privacy scans
contain no opponent Hand, Deck order, candidate alternatives, exact private
instance lists, or protected state/trace digests.

An initial focused run exposed two fixture assumptions: the synthetic attacker
position was inconsistent with its pending-attack tile, and the destruction
snapshot correctly captured all eligible passive sources rather than only the one
with the fixture observer trigger. The fixtures were corrected; production behavior
was unchanged, and the rerun passed.

## Exact changed files

All task paths were clean or absent at baseline and contain no unrelated overlap.
The Markdown and JSON manifests are identical.

- `Docs/Summon_Destruction_Composition_Foundation_Audit.md`
- `Docs/Summon_Destruction_Composition_Foundation_Audit.json`
- `Source/WandboundCore/Private/WBCardLifecycle.cpp`
- `Source/WandboundCore/Private/WBCharacterConstruction.cpp`
- `Source/WandboundCore/Private/WBCharacterSummon.cpp`
- `Source/WandboundCore/Private/WBDeathResolution.cpp`
- `Source/WandboundCore/Private/WBDeckSummon.cpp`
- `Source/WandboundCore/Private/WBSummonDestructionComposition.cpp`
- `Source/WandboundCore/Private/WBSummonExecution.cpp`
- `Source/WandboundCore/Private/WBUnitReplacementEffect.cpp`
- `Source/WandboundCore/Public/WBCardLifecycle.h`
- `Source/WandboundCore/Public/WBCharacterConstruction.h`
- `Source/WandboundCore/Public/WBCharacterSummon.h`
- `Source/WandboundCore/Public/WBDeathResolution.h`
- `Source/WandboundCore/Public/WBSummonDestructionComposition.h`
- `Source/WandboundTests/Private/WBSummonDestructionCompositionTests.cpp`

## Deferred

Broad zone mutation, arbitrary source zones, inheritance generalization, RL
transfer, continuous/temporary stats, choose-N, optional private-choice passing,
status ownership, new cards, AI, and telemetry remain deferred. Existing global
status tick ownership is unchanged.

## Readiness

The foundation is ready: focused and full automation, all build modes, clean
packaging, production smokes, fresh replay, deterministic artifact comparison,
privacy checks, and source-control hygiene all pass.
