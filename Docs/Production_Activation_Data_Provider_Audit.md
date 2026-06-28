# Production Activation Data Provider Audit

Date: 2026-06-28

## Existing Runtime Provider And Session Interfaces

Runtime already exposes `IWBRuntimeActivationDataProvider` through `WBRuntimeActivationDataProvider.h`.

The provider returns `FWBRuntimeActivationDataProviderResult`, which carries a `FWBRuntimeActivationDecisionSessionRefreshInput`.

The refresh input is already externally owned data:

- normal legal actions
- `FWBCardActivationLegalActionSet`
- public board summary

`WBRuntimeActivationDataProviderAdapter` passes provider output into `UWBRuntimeActivationDecisionSessionFacadeComponent`.

`UWBRuntimeActivationDecisionSessionFacadeComponent` refreshes the normal decision point and activation presentation from external data. It does not generate activation data itself.

## External-Data Ownership Model

Runtime activation sessions consume supplied action sets and summaries. Existing fixed test providers prove the session path accepts arbitrary externally provided data.

The production provider should therefore implement the existing interface and produce `FWBRuntimeActivationDecisionSessionRefreshInput`. It should not create a parallel provider contract.

## Current CardZoneState And Observation Behavior

`FWBCardZoneState` stores Deck, Hand, Discard, Equipped, board references, and marker placeholder state.

`WBCardZoneObservation` exposes:

- public zone summaries with Deck/Hand/Discard count-only
- own Hand visible only through `BuildObservationForPlayer`
- own Discard visible only through player-private observation
- Deck identity hidden for all viewers
- marker internal identity excluded from public/player observation scan text
- board card references derived from visible unit state

The production provider may consume the viewer observation for own-hand cards. It must not inspect opponent hand, deck identity, or marker internal identity.

## Current CardDefinitionRepository Behavior

`FWBCardDefinitionRepository` stores static Unreal-owned `FWBCardDefinition` values.

`WBCardDefinitionRepository` provides validation, exact lookup, deterministic card-id ordering, duplicate-id checks, and public-label internal-term guards.

The provider should only use repository lookup/validation behavior and should not import Godot CardDB.

## Board-Source Activation Derivation

Board-source activation can be derived from visible units:

1. Iterate current board units in deterministic board order.
2. Use each visible unit `CardId`.
3. Look up the matching `FWBCardDefinition`.
4. Inspect `ActivatedEffects`.
5. Include only effects whose explicit source gate requires `Board`.
6. Reuse `WBCardActivationSourceGate::Evaluate` for read-only source gate checks.
7. Emit a target-deferred activation action for presentation/session consumption.

No usage marking, cost payment, payload execution, response window, target picking, or `FWBAction` creation is needed.

## Hand-Source Activation Derivation

Hand-source activation should use `WBCardZoneObservation::BuildObservationForPlayer`.

Only `Observation.OwnHand.Cards` are considered. Opponent Hand entries are never inspected directly from `FWBCardZoneState`.

For each own hand card:

1. Use the observed own-hand `CardId` and `InstanceId`.
2. Look up the matching `FWBCardDefinition`.
3. Inspect `ActivatedEffects`.
4. Include only effects whose explicit source gate requires `Hand`.
5. Reuse `WBCardActivationSourceGate::Evaluate` for read-only source gate checks.
6. Emit a target-deferred activation action with source unit unset.

Deck identities and marker internal identities remain hidden.

## Target Options Policy

Existing `WBCardActivationCandidateGenerator` expects concrete candidate targets and builds executable commands. That is too far for this skeleton pass.

This provider emits source/effect decision entries now and leaves target enumeration deferred. Effects that require targets emit no target options and add the test-facing `target_options_deferred` diagnostic.

Future provider work can plug in read-only target enumeration once the target policy is explicit.

## Why This Pass Does Not Execute Effects

Effect execution belongs to EffectRunner and activation command execution paths. This provider is read-only decision data generation and must not mutate `FWBGameStateData`.

Executing effects here would merge provider, rules, and mutation responsibilities and would bypass replay/trace gates.

## Why This Pass Does Not Change WBRules::GenerateLegalActions

Normal legal action generation remains the existing movement/attack/turn-control `FWBAction` path.

Activation remains separate from `FWBAction` in this pass. The provider supplies activation decision data through the existing runtime activation presentation contract.

## Why This Pass Does Not Change WBActionCodec

Activation action ids are still separate strings on `FWBCardActivationLegalAction`. No `FWBAction` activation type is added, so `WBActionCodec` has no new responsibility in this pass.

## Risks And Open Questions

- Target legality and target presentation are intentionally deferred.
- Hand-source action presentation currently has no public source-unit card context because source unit is unset.
- Cost affordability, payment, usage marking, and activation resolution remain future work.
- Discard and equipped source zones remain disabled by provider config by default.
- Future provider passes must decide whether activation action ids remain target-deferred or are replaced by target-specific ids.
