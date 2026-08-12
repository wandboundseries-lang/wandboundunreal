# Suspended Attack Continuation Audit

## Baseline

- Commit and `origin/main`: `51f7f8c1b372cecc2a1b8858fdcf5a90865a2f02`.
- Initial automation baseline: 2,203 Wandbound tests.
- Replay schema: version 1.
- `WBActionCodec` source hashes: header `44ef87156beb5799066c2a5ecbc98f04928d98c0`, source `8c86faf74e07aea1a72a6cc27aba4fbc7dd09783`; both match the baseline Git blobs.
- No staged files existed at audit start.
- `WBGameStateData.cpp` was baseline-dirty and was inspected but not modified by this pass.
- Unrelated Config, RL, activation, public-summary, Meshy, Content, asset, and scratch work was preserved.

## Authoritative Sources

The most authoritative tracked combat text is
`Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2_1_FINAL.txt`, supported by
the final glossary, effect-language specification, and read-only Godot simulation
files under `scripts/sim/` and `autoload/Game.gd`.

The Rules Bible establishes this sequence:

`declare -> PreHit -> modifiers/prevention -> damage or Frozen break -> PostHit -> counter`

The completed Unreal reaction architecture already establishes opponent-first
priority, alternating priority, two consecutive passes to close, React resetting
the pass count, and deterministic auto-pass. Combat reuses those rules unchanged.

## Godot Combat Findings

- Declaration consumes the ordinary attack budget and stores one pending attack.
- PreHit opens to the opponent of the current attacker.
- A cleared/prevented pending attack ends before damage and does not open PostHit.
- Removing either participant before damage prevents resolution; no retarget occurs.
- Declaration legality is not rerun before damage. Range and line of sight are not
  rerun for the original hit.
- Frozen is removed instead of ordinary damage, then PostHit opens. A Frozen break
  suppresses the later counter.
- PostHit opens after a surviving defender receives the hit, including zero HP
  damage caused by zero ATK or full Armor absorption.
- Defender death clears the pending attack and skips PostHit and counter.
- Counter is automatic after PostHit. It is not a second player action.
- Counter eligibility reacquires both units and checks that the defender can attack,
  can see the original attacker, and is in current AR range.
- Stunned, Frozen, Cannot Attack, and equivalent no-attack statuses block counter.
- Counter does not inspect or consume `AttacksLeft`.
- Counter uses the defender's current ATK through the ordinary damage pipeline.
- Counter targets only the original attacker.
- Counter opens its own PreHit and PostHit windows.
- A counter cannot create another counter.

The Godot pending attack stores declaration damage, while the Rules Bible does not
state that ATK is locked at declaration. Existing Unreal attack damage reads current
ATK at resolution, so this pass preserves Unreal behavior rather than importing an
uncertain Godot implementation detail.

Godot also checks an original-attacker `cannot_be_countered` passive. Unreal has no
equivalent canonical unit field or production definition mapping. That gate remains
unimplemented and is an explicit data-model blocker rather than an invented rule.

## Existing Unreal Flow

Before this pass, the coordinator accepted Attack, called `ApplyAttackDeclare`, and
immediately called `ApplyPendingAttackDamage`. Reaction state already supported
typed `PreHit` and `PostHit`, but production attacks did not use it.

NPC phase resolution has a separate immediate declaration/damage path. Tracked
sources do not establish player-priority ownership or replay decisions during the
NPC phase, so this pass preserves that production behavior and does not partially
open player reaction windows during NPC automation.

## Implemented Production Boundary

`FWBPendingAttackState` now carries one typed continuation with stage, original and
current participant IDs, stable declaration/continuation identity, prevention,
damage, PostHit, Frozen-break, and counter flags.

`WBMatchCoordinator` is the sole whole-flow authority. It:

1. commits the existing Attack declaration and resource cost;
2. opens or auto-processes PreHit;
3. suspends the attack while the generic pending-effect stack handles A/B/C;
4. resolves damage at most once after PreHit;
5. observes death/terminal cleanup;
6. opens or auto-processes PostHit when canonical;
7. runs an eligible counter inside the same continuation;
8. clears continuation state and restores Action.

`WBEffectRunner` still owns declaration and damage state mutation. It cannot advance
priority, close a reaction layer, start a counter, or restore Action.

## Generic Prevention

`PreventPendingAttack` is a typed generic payload. Legal-action materialization binds
it to the exact active continuation ID. Rules accept it only during that attack's
PreHit stage. Coordinator resolution marks that exact continuation prevented.

It cannot target an older/later attack, negate an effect frame, retarget combat, or
clear unrelated reaction state. No production card ID appears in combat authority.

## Replay and Privacy

The original Attack remains one accepted player action. Automatic stage progression,
auto-passes, prevention, damage, PostHit, counter, cancellation, and completion are
trace events. Counters never fabricate an accepted Attack record.

Active continuation state contributes to the private state digest. New fields are
omitted when no attack is active, preserving unrelated baseline state hashes. Replay
schema remains version 1 and the public receipt remains exactly eight fields.

Public observations expose only the current priority viewer's authorized response
actions. Opponent hands, hidden candidates, continuation IDs, pending frame IDs,
and protected state/trace digests remain excluded from public receipts.

## Deliberately Deferred

- `cannot_be_countered` passive mapping, because Unreal has no canonical field/data mapping.
- NPC PreHit/PostHit player windows, because NPC reaction priority/continuation ownership is not established.
- Attack retargeting and Crash-In semantics.
- Card-specific Oddsman, Sealplate, Null Sigil, Null Thread, Claimshifter, Sever Thread, Shatter Parry, and Oathchain behavior.
- An independent "After damage is dealt" trigger collector; PostHit timing exists, but card-trigger collection semantics are not defined by this pass.

## Verification

- Focused attack-continuation suite: 16 passed, 0 failed.
- Affected attack/reaction/pending-effect/replay groups: 40 passed, 0 failed.
- Full Wandbound suite: 2,220 passed, 0 failed, 0 skipped.
- Editor non-unity build: succeeded.
- Game non-unity build: succeeded.
- Clean unity BuildCookRun exposed pre-existing symbol collisions in untouched runtime smoke files. The established non-unity build policy succeeded, followed by a successful cook/stage/package/archive using the verified binaries.
- Packaged suspended-attack smoke passed twice byte-identically. Fresh replay verification, privacy checks, eight-field receipt validation, and state/trace digest checks all passed.
- Canonical packaged startup remained byte-identical at `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Production fixture bundle digest: `62bda788d6e603b03428d2f4b27a538e94c44b847f87e343d7054adfa85133a9`.

The existing terminal replay necessarily changed because attacks now emit the typed
continuation lifecycle and eligible defenders perform the canonical counter. The
first difference is attack record 3: its gameplay `after_state_digest` remains
`fe099fd4fcc86031551c078bf91ffca7cc3ddc755bed78c0318986c66c218dac`,
while its trace span grows from `75..80` to `75..86`. The terminal archive changed
from `4e30424a56b613cbbda225295a0775473ed661cda390f172b609e529450235cc`
to `37d1256be0157bd07eab750c4b827801eb2ccfb155a73a0c8ca1683cb3a43896`.

### Partial Replay Validation Closure

The previously waiting partial-replay invocation was a map/fixture invocation
error, not a gameplay or core failure. It omitted
`/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev`, so the configured `OpenWorld`
loaded without `WBRuntimeLocalPlayGameMode` or its production bootstrap actor. It
also used the canonical startup match specification instead of the established
partial-replay smoke specification.

Validation was rerun twice with the inner packaged executable, explicit local-play
map, and package-relative paths:

`WandboundUE.exe /Game/Wandbound/Maps/Wandbound_LocalPlay_Dev -WandboundProductionData -WandboundCardBundle=Data/CardDB/Production/InitialCanonical/root_manifest.json -WandboundMatchSpec=Data/Replay/production_replay_smoke_match_spec.json -WandboundProductionReplaySmoke -unattended -nop4 -NullRHI -nosplash -nosound -log`

- Run 1 exit code: `0`.
- Run 2 exit code: `0`.
- Archive both runs: `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463`.
- Receipt both runs: `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9`.
- Replay digest both runs: `391f0a6e836fc19439f110a5bd0a748367c00c826e73ad1d615fe53d9b492e7e`.
- Final state digest both runs: `cdf2a709f551eb6e678dba97e191fb014b1b943017eaf71bc84c5f8b58dd034a`.
- Final trace digest both runs: `ff30cac96c8b8b286f13faff9593a968585f712cc25ae08665a3669ab075fe2f`.
- Final generation/revision both runs: `1` / `4`.
- Archive, receipt, and startup JSON were byte-identical between runs and equal to the established baselines.
- The smoke's fresh-coordinator replay verification succeeded on both runs.
- Receipt remained exactly eight fields and contained no protected state, trace, action, hand, marker, continuation, or pending-frame data.
- No `WandboundUE.exe` process remained after either run.
- Source code and Config were unchanged by this validation closure.
