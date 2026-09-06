# Declared Summon Negation Foundation Audit

## Canonical rule change

`Negate a Summon` is a distinct mechanic: cancel a pending summon before the
unit is successfully summoned. It is not Destruction, Sacrifice, Damage, or
effect negation, and it does not move the attempted card by itself. The public
Rules Bible and glossary addenda define the new term and clarify that summoned
conditions begin only after successful placement.

## PreSummon timing

Only a player-declared normal Character summon from Hand opts into the new
typed `PreSummon` reaction window in this pass. The sequence is declaration,
provisional validation, pending state, opponent-first response priority,
existing nested response resolution, two-pass close, exact revalidation, and
either cancellation or established summon execution. The coordinator remains
the sole owner of priority, passes, reaction state, and continuation.

## Pending Summon

`FWBPendingSummonState` stores deterministic authoritative continuation facts:
active/accepting/negated flags, deterministic pending ID, player ownership and
control, exact internal card instance, CardId snapshot, source zone, tile,
typed origin, declaration provenance, condition policy, source action, resume
context, and event identity. It contains no GUID, timestamp, pointer identity,
or RNG-derived token and is cleared after resolution or terminal commit.

## Declared summon provenance

The original player action remains the declaration because that player chose
the exact Character and destination tile. Creating pending state is not a
second declaration. Automatic and effect-generated summons are not inferred to
be declared merely because a player made a target or private card choice.

## Exact instance and source-zone retention

Pending authority is `CardInstanceId`-exact. Duplicate definitions cannot
substitute for the selected copy. The card remains in its authoritative Hand
position while responses resolve. Negation leaves it there without an implicit
discard, destruction, sacrifice, reveal, or temporary zone.

## Summon-negation operation and legality

`NegatePendingSummon` is a typed generic effect operation with a typed pending
summon target. It is legal only for the responding opponent during the matching
active `PreSummon` window, against the current accepting pending summon ID. It
is not a universal action and is emitted only from an otherwise legal React
effect. Stale, missing, wrong-player, wrong-window, or already-negated targets
fail closed.

## Nested response behavior

Activating summon negation does not cancel the summon immediately. The effect
uses the existing pending-effect stack and marks the summon negated only when
that effect resolves. Existing effect negation may negate the summon-negation
React; in that case the pending summon remains unnegated. Existing LIFO order,
pass count reset on React, alternating priority, and depth limits remain intact.

## Successful summon path

An unnegated pending summon is revalidated against the exact Hand instance,
definition, owner, source zone, destination, unit cap, summon conditions, and
terminal state. Success delegates to established summon execution, consumes
that exact card, constructs the live unit, resolves established marker and
summon behavior, and reaches the existing `PostSummon` checkpoint.

## Negated summon path

A resolved negation clears pending state, leaves the card in Hand, creates no
unit, and resumes the saved action context. It emits no summon, destruction,
sacrifice, damage, marker, or PostSummon result. Failed post-response
revalidation also clears the continuation without consuming or substituting a
card and emits a deterministic failure trace.

## PostSummon distinction and trigger semantics

`PreSummon` and `PostSummon` remain separate timings. `When this unit is
summoned`, summon listeners, and summon-entry marker behavior occur only after
successful established summon execution. Declaration, pending-state creation,
window opening, and negation do not satisfy a successful-summon condition.

## Privacy

The exact card instance remains internal. Public pending/window traces use a
deterministic pending ID and public context without exposing unrelated Hand
cards, Hand order, private zone index, alternate instances, or protected
digests. Packaged receipt and startup JSON scans found none of those protected
fields. The receipt remains exactly eight fields.

## Replay and determinism

Pending summon state participates in the authoritative state digest. New trace
fields are conditional, preserving older traces. The packaged production smoke
persisted and freshly replayed all three required paths. Two repeated final
runs were byte-identical:

| Evidence | SHA-256 / digest |
| --- | --- |
| Archive | `5df7faac15f1da0b1124efb80150b33a95de45afeaafbde96407ef549c22f83d` |
| Receipt | `511587e612c2a087e0c7b6813ec9817d3ab2ad4add1a342fd88521b7651335e9` |
| Startup JSON | `210da863cb6587a48e8dd31e502e04a8f5b57b5cf765e5495353ca3b0daac360` |
| Replay | `a95c134c2ffd99dfb40c3659f099f4c4e66695fa01b44307ac833271a788b69d` |
| Final state | `f5cc3958d96315972872176c9a89736c339c5f52813e90a6b707680772454b90` |
| Final trace | `7fe7097b50d0b2c50f81646a846445a9d5c8f9756e992658d144c932960d20b2` |

Final persisted scenario: schema 1, three accepted records,
generation/revision `1/4`, nonterminal partial replay. The synthetic bundle
digest is `1749cdaa6ffc916c32ae48c97592e88249567e8a058876f70aa73b14855c4ab2`.

## Explicitly unaffected summon origins

| Summon path | Declared summon | Source | New PreSummon |
| --- | --- | --- | --- |
| Normal player Character | Yes | Hand | Yes |
| Rook continuation | No | Deck | No change |
| Patch continuation | No | Deck | No change |
| Crash-In replacement | Existing atomic semantics | Hand | No change |
| Hybrid procedure | Existing Hybrid semantics | Hand/payment | No change |
| Setup | No | Setup data | No |
| NPC/marker | No | Generated state | No |
| Other effect-generated summon | No | Effect-defined | No change |

An independent packaged Rook production smoke exited 0 after the final package,
confirming its exact Deck choice, continuation, and effect-generated summon path
remain operational without a new PreSummon checkpoint.

## Validation

- Baseline: public HEAD and origin/main were
  `4f353c7948ea2576a6ba8b17ca040e0012c4de83` before changes.
- Focused SummonNegation: 9 succeeded, 0 failed/warnings/not-run, with at least
  82 assertions across pending state, exact instances, responses, revalidation,
  privacy, terminal cleanup, and replay.
- Existing Reaction regression: 37 succeeded, 0 failed/warnings/not-run.
- Full Wandbound automation: 2,498 succeeded, 0 failed, 0 warnings, 0 not-run;
  baseline 2,489, delta +9.
- Editor non-unity: succeeded; final source compiled in 216.24 seconds and the
  post-staging check succeeded in 76.78 seconds.
- Game non-unity: succeeded; final source compiled in 202.83 seconds and the
  post-staging check succeeded in 173.69 seconds.
- Editor default: succeeded; final source compiled in 95.73 seconds and final
  metadata regenerated in 21.59 seconds.
- Game default: succeeded; final source compiled in 89.74 seconds and final
  metadata regenerated in 7.60 seconds.
- Editor forced unity: final source compiled in 61.69 seconds; final staging-only
  revision remained up to date and passed in 3.64 seconds.
- Game forced unity: final source compiled in 72.67 seconds; final staging-only
  revision remained up to date and passed in 3.49 seconds.
- Final clean BuildCookRun: 417 build actions, 532 cooked packages, 459 package
  store packages, five staged fixture files, exit 0 in 934.18 seconds.
- Packaged summon-negation smoke: two runs, both exit 0, all three scenarios and
  fresh replay verified in-process; archive, receipt, and startup bytes matched.
- Packaged Rook regression smoke: exit 0.
- `WBActionCodec` unchanged: cpp blob
  `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`, header blob
  `44ef87156beb5799066c2a5ecbc98f04928d98c0`.
- Replay schema remains 1; public receipt remains eight fields.

The first package attempt exposed that the new fixture lacked runtime staging.
It failed closed with `production_card_bundle_not_found`; only its verified
packaged process was stopped. Adding the fixture to the existing runtime
dependency list resolved the packaging gap. The corrected clean package and all
final smokes passed.

## Exact changed paths

All task paths were clean or absent at baseline and contain no unrelated hunks.

1. `Data/Replay/SummonNegationFixture/bundle_manifest.json`
2. `Data/Replay/SummonNegationFixture/markers.json`
3. `Data/Replay/SummonNegationFixture/match_spec.json`
4. `Data/Replay/SummonNegationFixture/root_manifest.json`
5. `Data/Replay/SummonNegationFixture/units.json`
6. `Docs/Summon_Negation_Foundation_Audit.json`
7. `Docs/Summon_Negation_Foundation_Audit.md`
8. `Docs/Wandbound_Canonical_Glossary_Summon_Negation_Addendum_v1.md`
9. `Docs/Wandbound_Rules_Bible_Summon_Negation_Addendum_v1.md`
10. `Reference/GodotCanon/README.md`
11. `Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp`
12. `Source/WandboundCore/Private/WBCardActivationExpansion.cpp`
13. `Source/WandboundCore/Private/WBEffectRunner.cpp`
14. `Source/WandboundCore/Private/WBGameStateData.cpp`
15. `Source/WandboundCore/Private/WBMatchCoordinator.cpp`
16. `Source/WandboundCore/Private/WBProductionMatchReplay.cpp`
17. `Source/WandboundCore/Private/WBPublicTurnSummary.cpp`
18. `Source/WandboundCore/Private/WBReplayTrace.cpp`
19. `Source/WandboundCore/Private/WBRules.cpp`
20. `Source/WandboundCore/Public/WBCharacterSummon.h`
21. `Source/WandboundCore/Public/WBEffectRequest.h`
22. `Source/WandboundCore/Public/WBGameStateData.h`
23. `Source/WandboundCore/Public/WBMatchCoordinator.h`
24. `Source/WandboundCore/Public/WBReplayTrace.h`
25. `Source/WandboundRuntime/Private/WBProductionSummonNegationSmoke.cpp`
26. `Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp`
27. `Source/WandboundRuntime/Public/WBProductionSummonNegationSmoke.h`
28. `Source/WandboundRuntime/WandboundRuntime.Build.cs`
29. `Source/WandboundTests/Private/WBProductionReactionWindowTests.cpp`
30. `Source/WandboundTests/Private/WBSummonNegationTests.cpp`

The Markdown and JSON manifests contain the same 30 paths. No file is staged.

## Deferred behavior

PreSummon remains deliberately disabled for Rook, Patch, Crash-In, Hybrid,
setup, NPC/marker, and arbitrary effect-generated summons. Also deferred are
implicit discard/destruction of a negated card, cross-zone effects, a production
summon-negation card, choice-system expansion, inheritance/RL transfer,
temporary modifiers, status ownership changes, AI, and telemetry.

## Remaining ambiguity

Future origins that pay sacrifice, Wand, replacement, or effect costs require
an owner ruling on retained payment before opting into PreSummon. This pass does
not infer those semantics. The normal Hand Character path has no unresolved
timing ambiguity under the approved addendum.
