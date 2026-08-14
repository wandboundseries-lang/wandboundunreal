# Repository Canon Reconciliation Audit

Date: 2026-08-14

## Authority Order

For an explicitly registered conflict, a newer product-owner-approved repository
addendum wins over the read-only Rules Bible. Current validated production tests
and implementation preserve that decision. Historical Godot behavior remains
evidence, not runtime authority. Unresolved conflicts still fail closed and require
the product owner.

## Reconciled Contradictions

| Topic | Older wording | Current authority | Resolution | Proof |
| --- | --- | --- | --- | --- |
| First-player Turn 1 | Rules Bible v2.1 section 6.7 bans every attack. | `Wandbound_Game_Start_and_Turn_One_Addendum_v1.md` explicitly permits neutral NPC attacks while barring opponent-controlled targets. | The addendum wins for this named conflict. No engine change was made. | `WBActiveFormatGameStartTests.cpp`; `Wandbound.TurnStart.*` and game-start tests. |
| Hero setup | The older Bible/Godot path describes sequential deployment. | The same approved addendum requires atomic dual-Hero commit before trigger collection. | The addendum wins. No setup behavior changed. | `WBInitialHeroSetup.cpp`; Active Format game-start tests. |
| Turn start | Rules Bible v2.1 section 8.2 lists status work before draw/MP/resource refresh. | The later validated production sequence is draw, MP, resource reset, statuses, then trigger/effect collection. | Preserve the validated coordinator-owned sequence; do not regress code to the older list. | `WBTurnStartSequence.cpp`; `Wandbound.TurnStart.Order.*`. |
| RL language | Historical implementation and compatibility JSON use `RLTotal`/`rl_total`. | Canonical glossary sections 8.8-8.11 and 9 require Base RL, Current RL, RL Used, and Available RL in player-facing language. | Rules/public summaries use canonical fields; `RLTotal` remains a documented compatibility mirror only. | Resonance recalculation/load and public-summary tests. |
| Body Double | Historical Godot Redirect required an adjacent replacement and recalculated against that replacement. | Product-owner-directed Body Double text and commit `c9ad403` transfer already-calculated HP damage to any other controlled CSN unit. | Current docs already distinguish historical Redirect from canonical substitution. No engine change was made. | `WBCSNBodyDoubleDamageSubstitutionTests.cpp`; `CSN_Body_Double_Transfer_Audit.md`. |
| Damage stages | Committed baseline is PreHit, CalculateDamage, SubstituteDamage, ApplyDamage, PostHit, CounterEligibility, Counter, Complete. | The protected uncommitted AfterDamage feature inserts AfterDamage after ApplyDamage. | Baseline docs remain baseline-accurate; the protected feature audit alone describes the uncommitted stage. | Explicit damage-pipeline tests and AfterDamage tests. |

## Documentation Changes

- `Reference/GodotCanon/README.md` now records explicit addendum precedence.
- Public board/runtime reports use canonical RL terminology and label `rl_total`
  as compatibility-only.
- Historical Redirect and Rules Bible quotations remain where clearly labeled as
  historical evidence; they are not rewritten as current behavior.

## Unresolved Canon

No new unresolved timing rule was required by this cleanup pass. This audit does
not promote the protected AfterDamage work to committed baseline canon.
