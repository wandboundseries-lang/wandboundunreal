# After-Damage Trigger Collection Audit

## Baseline

- Commit and `origin/main`: `e919f5e9736d3c5650dfa9df94038466304d44d3`.
- Baseline suite: 2,262 Wandbound tests.
- Replay schema: 1.
- `WBActionCodec` is unchanged.
- No production card received an after-damage trigger.

## Canonical Timing

The canonical glossary defines "After damage is dealt" as after damage resolves.
Production attack continuation is therefore:

`PreHit -> CalculateDamage -> SubstituteDamage -> ApplyDamage -> AfterDamage -> PostHit -> CounterEligibility -> Counter -> Complete`

`AfterDamage` is automatic. It is not a response window. Existing `PostHit`
remains the response checkpoint after the completed damage event. When no legal
PostHit response exists, the coordinator retains its existing empty-window fast
path and emits `attack_post_hit_closed` without opening a manual window.

## Historical Godot Timing

Read-only Godot `scripts/sim/combat_runner.gd` invoked
`battle_hit_resolved` before applying damage so effects survived lethal hits.
That ordering conflicts with canonical wording and is not reproduced. Unreal
captures eligible trigger sources before `ApplyDamage`, applies damage and death
cleanup, then resolves the captured instances during `AfterDamage`. This preserves
lethal-source persistence at the correct post-damage timing.

## Event Context

`FWBAfterDamageEventContext` keeps attacker, hit unit, and final damage recipient
distinct. It also records controllers, raw damage, Armor absorption, calculated
HP damage, actual clamped HP loss, substitution, prevention, Frozen break,
counterattack identity, stable attack identity, and before/after HP values.

- Normal hit: hit unit and final recipient are the same unit.
- Body Double: the Hero remains the hit unit; the substitute is only the final
  recipient.
- Redirect: the redirected unit becomes both hit unit and final recipient before
  calculation; the original target and any stale Body Double recipient are not
  attributed as hit.
- `AppliedHPDamage` is the actual clamped HP delta from
  `attack_damage_applied`, not calculated overkill.

## Requirements And Roles

Definitions support `DamageResolved` and `PositiveHPDamage`. Zero-HP outcomes
(full Armor absorption, Frozen break, prevention, or zero ATK) can satisfy only
`DamageResolved`.

Source roles are `Attacker`, `HitUnit`, `FinalDamageRecipient`, and
`BattleParticipant`. Battle participant means attacker or hit unit; a Body Double
recipient does not become a battle participant merely by receiving HP damage.

Targets are `None`, `Self`, `Attacker`, `HitUnit`, `FinalDamageRecipient`, and
`OpposingBattleUnit`. Opposing battle unit is the other attacker/hit-unit
participant and never the Body Double recipient.

## Collection And Resolution

The collector snapshots eligible unit definitions and unique equipped Wand
instances associated with involved units before damage mutation. Removed,
defeated, Stunned, Frozen, or Negated unit-origin sources are excluded. Frozen
suppresses the unit source but not a separately equipped Wand source. Captured
unit and Wand instances survive cleanup caused by that hit; they do not require
their source to remain on board during resolution. A `Self` payload still fails
naturally when its target no longer exists.

Only mandatory definitions are supported. `mandatory: false` fails closed as
`optional_after_damage_trigger_unsupported`. Resolution constructs a generic
`FWBEffectRequest` containing existing `FWBGenericEffectPayload` data; no collector
branch mutates card-specific state.

## Deterministic Ordering And Usage

Canon does not specify simultaneous cross-controller after-damage ordering. The
collector uses the existing least-novel deterministic convention: originating
controller first, then controller ID, source unit ID, source kind, equipped order,
and stable trigger ID. Stable IDs derive from attack continuation, source,
instance, and trigger identity, never pointers, paths, GUIDs, or map iteration.

`once_per_turn` keys include controller, source identity, and trigger identity.
`once_per_turn_per_opposing_unit` additionally includes the opposing battle unit,
not the final substitution recipient. Usage is marked only after successful
mandatory resolution.

## Terminal And Continuation

After-damage effects resolve before PostHit and counter eligibility. If resolution
makes the match terminal or removes a required attack participant, the coordinator
does not open stale PostHit work and does not counter. Counterattacks and neutral
NPC attacks use the same collector and context, with `bCounterAttack` set for the
counter path. No counter chain was added.

## Traces, Replay, And Privacy

Eligible work emits `after_damage_trigger_collected`, generic payload traces, then
`after_damage_trigger_resolved`. An attack with no matching definition emits no
after-damage trace churn. Fresh replay reproduces trigger order, state digest,
trace digest, generation, and revision. Replay schema remains 1.

The public receipt remains exactly eight fields. It excludes state/trace digests,
continuation IDs, private source-instance alternatives, hidden Hand identities,
and filesystem paths. Public HP and resulting status continue through existing
public observation policy.

## Fixture

`Data/Replay/AfterDamageTriggerFixture/` contains only synthetic test IDs. Its
definition bundle digest is
`0f4daf68ba16b91e6431458b87b0d24202631ff77f7e1da17df57205f49b4b4c`.
Two packaged runs produced identical artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Archive | `b4eb9718d14e14bf3b928762ccc7aa0631d1f1a27d0cb9e42fe6ad3fddb4cce6` |
| Receipt | `7cf62ba01e3db687d7ad937d3a7f22ef3fd17299bcc236b6c943932b2bc254eb` |
| Startup | `d741233fc8d65646aa6862237dc197bae60164d926c61469ee716eb4b4f73616` |
| Replay digest | `f11920277b8383e64548dd47315fe17a8765a606b44eecf5ecc31f7313615dc8` |
| Final state digest | `1624292e34a0060235c62233a328b94788f695780f8989cce84d50ed78eb5b84` |
| Final trace digest | `2dc1481b1cd6e3949575f76e31d840187520065adbb1e3101781b6764c3cd338` |

The fixture completes three accepted actions at generation 1, revision 4. Its
defender survives 3 applied HP damage, the attacker receives Rooted, the ordinary
counter applies 1 HP damage, persisted bytes match the in-memory archive, and a
fresh coordinator replay matches.

## Protected Packaged Regressions

Every protected smoke exited 0 twice. Archive, receipt, and startup bytes matched
between its two current packaged runs. Canonical startup remains
`cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.

| Path | Archive SHA-256 | Receipt SHA-256 |
| --- | --- | --- |
| Partial replay | `d30304a936fd3b5c2163209546b9063a64ed7a223a65b217092cb64ef6495463` | `881ffb586544d5ed78156635754b5eeddf555c7ef000c472f79ac607cc4d2dd9` |
| Terminal | `64dbec678c44a1a369c392c82bcfc8c646eca4f308c62b1f5c610b57f663f444` | `9a1ba2c618c25c1fb58471f1a99fd280a9a5d1be284f4ebfec098503b57825ea` |
| Hybrid Hero | `e1fa69301728e8129e69866ce0a91fbeaf77cc990d08f0a33133943bc629be20` | `7cdba9356c9fbb6c796aaaedfbeef7fc884ec74522a4a20231d082961cc6f156` |
| Hybrid non-Hero | `f3b0cb64cb2bd45ae6816ddc871b0a08d89cac55272110a102e94fb666380c1d` | `f3dab3075d43922bd1bcd59e377370f69bb848d5268a04d8bbfc4c5b9e1aa3f1` |
| Reaction | `e08121092f5769ca50ba4061d65ea895c761c611772182cdba508eebbb92cdf9` | `0ff8d172f543e95ecedaf79e759bd74c114cb2d123de741a100274328c7ba5db` |
| Pending effect | `5a986774800be98fec924253d2bd301a00b39cb3f84d313778b689991ffd5bed` | `ec28863a73d64745d4f3b6794630b536b92f989525eb08f265ce6221849d0597` |
| Suspended attack | `f36c42e0da6a2c65dc1f1423ccd0c3c35ddb1398ab6107a29df6cc059cc2ded9` | `0820ce1344b7a528b58bcb31693ddf446104e935d3490633c3681d7ea77b92a4` |
| NPC reaction | `40e108f2e0a359fba58e649f283054e38091fb382224dfc445a68103c4f65fab` | `3cdb8bebd571471f4d3ec6b6f76326a96567cfee1da970bd440ee5af79c9e68b` |
| Redirect | `1eaa2f658ae9b28eeeb0cbe0ded473489331d1abfdfa34ec51151d2a53a44c36` | `5daf5a28a1a16de4b70f48505e14c841906400375858347fff290436f909111d` |
| Body Double | `98a5ecc6a78991ab0e85568cf4134a5684b8ebfecefbcca313b4cb5ab22b6c0d` | `9cb1375c62799ff6a87958f174e11a957873a8a0ba365664ab2b23ec6e251528` |

Terminal, suspended-attack, NPC, and Redirect values differ from documentation
predating committed baseline `e919f5e` because that baseline introduced the
explicit damage-calculation stages and their trace semantics. This pass does not
add after-damage traces to those zero-definition fixtures. Body Double and the
non-combat protected hashes remain at their established values.

## Validation

- Focused AfterDamage: 35 succeeded, 0 failed, 0 warnings.
- Full Wandbound: 2,297 succeeded, 0 failed, 0 warnings, 0 not run.
- Editor non-unity build: succeeded.
- Game non-unity build: succeeded.
- Clean BuildCookRun: succeeded in 789.06 seconds.
- Packaged AfterDamage smoke: exit 0 twice.
- Protected packaged smokes: all exit 0 twice and repeat byte-identically.
