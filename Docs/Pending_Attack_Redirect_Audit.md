# Pending Attack Redirect Audit

## Scope

This pass adds a generic Unreal rules operation for redirecting one active pending
attack. It does not implement CSN Body Double, CSN Crash-In, or any production card.
The audit used tracked canon and the read-only Godot reference. No Godot file was
changed or loaded by Unreal at runtime.

Baseline: `867cb2e0ae6c86e3076eec0462c3b937ee0c3fbf` on `main`, synchronized with
`origin/main`. The baseline had 2,230 passing Wandbound automation tests. All task
source files were clean before this pass; unrelated dirty and untracked work was
left untouched.

## Sources

- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary_v2_1_FINAL.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2_1_FINAL.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Effect_Language_Spec_v1.txt`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/directives/handlers/redirect_pending_attack_handler.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/effects_react.json`
- `Reference/GodotProject/godotcanon/tools/ci/smoke_csn_body_double.gd`

## Canonical Findings

| Question | Established behavior |
| --- | --- |
| Meaning | Redirect changes an attack's current target to a new legal target. |
| Timing | `attack_pre_hit`. Godot also recognizes a nested pending effect whose base event is `attack_pre_hit`. |
| Attack identity | The pending attack remains the same declaration and continuation. No second attack is declared or consumed. |
| Attacker | Legality uses the active pending attack's current attacker. This naturally covers an active counter continuation. |
| Current target | Mutable. Unreal uses `DefenderUnitId`; `OriginalDefenderUnitId` remains the declaration target. |
| New target | Must exist, be live, be on board, differ from the current target, and not be the attacker. |
| Geometry | New target must be within current attacker AR, orthogonally aligned, and have clear wall/unit LOS under current Unreal combat geometry. |
| Ownership/faction | Generic Redirect has no ownership or faction restriction. Friendly, Hero, CSN, and adjacency requirements belong to a card gate/selector. |
| Attack resources | Do not recheck or consume `AttacksLeft`. |
| Attack-blocking status | Do not recheck Stunned/Frozen/CannotAttack because Redirect is not a declaration. |
| Turn restrictions | Do not recheck normal-turn, Turn-1, or declaration priority rules. |
| Neutral/NPC | No generic faction restriction excludes a neutral target. Neutral-NPC attacks use the same PreHit continuation and may be redirected. |
| Counter | Counter attacks enter the same PreHit continuation and may be redirected. |
| Resolve-time changes | Target existence, board presence, range, and LOS are revalidated when the Redirect effect resolves. |
| Response lifecycle | Redirect mutation does not reopen the window, reset passes, or perform another priority transition. The React activation owns normal response progression. |

## Card-Specific Findings

Godot's `redirect_attack_to_adjacent_friendly` query contains Body Double policy:
the original/current defender must be the player's Hero, the replacement must be
a friendly qualifying unit, and it must be adjacent to that Hero. Crash-In adds
sacrifice, summon-from-hand, faction, and tile-replacement behavior. None of those
requirements were transferred into the generic Redirect primitive.

## Unreal Design

- `EWBGenericEffectOp::RedirectPendingAttack` is a typed payload operation.
- `FWBGenericEffectPayload::PendingAttackContinuationId` identifies the exact attack.
- `FWBEffectRequest::Target.TargetUnitId` identifies the replacement target.
- `WBRules::CanRedirectPendingAttack` owns redirect-specific legality and reuses only lower-level range/LOS helpers.
- `WBEffectRunner::ApplyPendingAttackRedirect` mutates `DefenderUnitId` and `DefenderTile`, preserving all declaration and authority fields.
- `WBMatchCoordinator` injects the active continuation ID into generated commands and filters illegal target candidates through rules.
- A successful mutation emits `pending_attack_redirected` with old target, new target, continuation, stage, geometry, and counter flag.
- Suspended reaction frames restore control-flow state, then combat PreHit/PostHit source and target are rebound from the authoritative pending attack.

## Fail-Closed Boundary

Tracked sources available to this pass do not establish redirect-after-prevent
ordering. `CanRedirectPendingAttack` therefore rejects an already-prevented attack
with `pending_attack_already_prevented`. Ordinary stack ordering remains supported:
a Redirect can resolve before a later Prevent, and a negated Prevent leaves the
redirected attack active.

## Preserved Boundaries

- Replay schema remains version 1.
- `WBActionCodec` is byte-identical to baseline.
- No accepted Attack action is fabricated by Redirect.
- No production card, card-specific target selector, UI, Blueprint, asset,
  networking, Godot, or Meshy change is included.

## Production Validation

The isolated five-file fixture at
`Data/Replay/PendingAttackRedirectFixture/` uses neutral fixture definitions and
production CardDB loading. Player 0 summons one attacker through a generated
Summon action, advances through ordinary turn control, and declares one attack
against Player 1's Hero. The PreHit response chain is:

```text
Prevent A -> Negate B -> Redirect C
```

Redirect C changes the active continuation target from H to X at the deepest
pending-effect level. Parent frames then restore at least three times. Each
restoration preserves the same declaration/continuation/original identities and
rebinds current combat source and target from the authoritative pending attack.
Negate B suppresses Prevent A, so ordinary attack damage resolves exactly once to
X and never to H. The attack resource is consumed once and the replay contains
one accepted Attack record; responses remain ordinary Activation records.

The packaged smoke uses production bootstrap, generated legal actions,
`WBMatchCoordinator::SubmitActionId`, public observations, replay persistence,
and fresh-coordinator replay. It does not mutate coordinator state after startup.
The exact inner-executable arguments were:

```text
WandboundUE /Game/Wandbound/Maps/Wandbound_LocalPlay_Dev
-WandboundProductionData
-WandboundAllowTestCardBundle
-WandboundCardBundle=Data/Replay/PendingAttackRedirectFixture/root_manifest.json
-WandboundMatchSpec=Data/Replay/PendingAttackRedirectFixture/match_spec.json
-WandboundProductionPendingAttackRedirectSmoke
-unattended -nop4 -NullRHI -nosplash -nosound -log
```

The PowerShell caller used stop-parsing (`--%`) with package-relative fixture
paths so the known outer-launcher `.json` quoting defect was not involved.

## Packaged Results

Both packaged runs exited 0 and produced byte-identical files:

| Artifact | Run 1 SHA-256 | Run 2 SHA-256 |
| --- | --- | --- |
| Archive | `25c1e81c458ad343192942ffe54a28510dea88c7aee5381e753293ff3682af79` | same |
| Receipt | `801a88e95569e4a435731fc83adad7e7287d07efb4aaf568af14dc3ce6234895` | same |
| Startup | `979559cf54a1deb5460b290819791f81c5cf8ca61635c9501fad37ccdd268ac5` | same |

The deterministic private results were replay digest
`0aff196f0b5aefc31f01653037b7adeec9068fbfe499cad04737083f0514543c`,
state digest
`1745bae8bdd221ce2626ce6022b3909067d341ece3a6e6dc2a8968eb431e8ef7`,
trace digest
`08bf582c5820a6d5086138294a3df67929ead289a29514828d0a96dac31842d9`,
generation 1, revision 9, and eight accepted records. Fresh replay reproduced
the records, state, trace, generation, revision, and closed pending states in
both runs.

The public receipt has exactly eight fields. Scans found no opponent-hand
identity, hidden response alternative, pending-effect frame ID, pending-attack
continuation ID, original/current private combat bookkeeping, protected digest,
or filesystem path.

## Build and Regression Results

- Focused Redirect automation: 15 succeeded, 0 failed, 0 warnings.
- Affected Attack automation: 31 succeeded, 0 failed, 0 warnings.
- Final full Wandbound automation: 2,245 succeeded, 0 failed, 0 warnings,
  0 not run.
- Final Editor non-unity build: succeeded in 26.77 seconds.
- Game non-unity build: succeeded.
- Cook, stage, package, and archive: succeeded through the established route
  after verified Editor/Game non-unity builds.
- A direct combined BuildCookRun compile attempt reproduced only the known
  pre-existing adaptive-unity anonymous-helper collisions (`FindDiscard`,
  `SubmitAndCapture`, `CountDiscardInstance`, `FindEquip`, and `HasTrace`). No
  unrelated smoke source was changed; the task-owned smoke has no unity collision.
- Canonical startup: exit 0, `production_started`, preserved SHA-256
  `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Partial replay archive/receipt/replay remained
  `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463` /
  `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9` /
  `391f0a6e836fc19439f110a5bd0a748367c00c826e73ad1d615fe53d9b492e7e`.
- Terminal replay archive remained
  `37d1256be0157bd07eab750c4b827801eb2ccfb155a73a0c8ca1683cb3a43896`.
- Hero Hybrid archive/receipt/replay remained
  `e1fa69301728e8129e69866ce0a91fbeaf77cc990d08f0a33133943bc629be20` /
  `7cdba9356c9fbb6c796aaaedfbeef7fc884ec74522a4a20231d082961cc6f156` /
  `9c73493e6931a627969a7472b49c858a623150de49da522ef963663f41e3f98e`.
- Non-Hero Hybrid archive/receipt/replay remained
  `f3b0cb64cb2bd45ae6816ddc871b0a08d89cac55272110a102e94fb666380c1d` /
  `f3dab3075d43922bd1bcd59e377370f69bb848d5268a04d8bbfc4c5b9e1aa3f1` /
  `e84384339113dc407708b223ef29532eeee595e4da3b3434c6ba97159369bac4`.
- Reaction-window, pending-effect, suspended-attack, and NPC reaction-combat
  packaged smokes all exited 0. Their archive/receipt hashes remained:
  `e08121092f5769ca50ba4061d65ea895c761c611772182cdba508eebbb92cdf9` /
  `0ff8d172f543e95ecedaf79e759bd74c114cb2d123de741a100274328c7ba5db`,
  `5a986774800be98fec924253d2bd301a00b39cb3f84d313778b689991ffd5bed` /
  `ec28863a73d64745d4f3b6794630b536b92f989525eb08f265ce6221849d0597`,
  `8a18e8d44e85d282d52756dfe26d513dd07367e8ef22880627a9d9b78a60d6aa` /
  `6b9a7828bdb9b64babb5303c38d16223696e9284b2a96ee3e731fcebf6e5083a`,
  and `7b365000b871aec7c5ef71066a74749a38682de084f89c2c594f975b068a02b9` /
  `d7501c4e7fcc68a494517599c13c241f410ea27c17b0c7aa14a708616da00c70`.
- No packaged `WandboundUE.exe` process remained after validation.

One interim canonical-startup command omitted its required bundle and match-spec
arguments and correctly failed closed with exit 13 / `production_bundle_invalid`.
The corrected canonical invocation supplied
`Data/CardDB/Production/InitialCanonical/root_manifest.json` and its canonical
match spec; it passed and restored the canonical startup artifact. This was
command evidence only and caused no source change.
