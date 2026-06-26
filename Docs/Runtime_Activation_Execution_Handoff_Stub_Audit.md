# Runtime Activation Execution Handoff Stub Audit

## Context

This pass starts after the runtime activation presentation handoff and selected activation id resolver. Runtime code can already receive externally supplied activation legal actions and a public board summary, build public activation/target presentation snapshots, and resolve a selected activation action id back to the matching internal activation action.

## Current Selection Resolver Behavior

`WBRuntimeActivationSelectionResolver` accepts a `UWBRuntimeActivationPresentationModelComponent` and a selected activation action id. It fails clearly when the model is missing, the model has no current activation presentation state, the id is missing, or the id is ambiguous. On success it returns:

- the selected activation action id,
- the matching internal `FWBCardActivationLegalAction`,
- the public activation presentation entry when present,
- the public target presentation entry when present.

The resolver does not execute card activations, does not inspect `FWBGameStateData`, does not call `WBRules`, and does not call `WBEffectRunner`.

## Current Presentation Model Behavior

`UWBRuntimeActivationPresentationModelComponent` stores externally supplied `FWBCardActivationLegalActionSet` data and builds public activation and target presentation snapshots from a `FWBPublicBoardSummary`. It can look up public presentation rows by activation action id and delegates selected id resolution to `WBRuntimeActivationSelectionResolver`.

The model is a presentation bridge. It does not own rules truth, generate activation candidates, generate activation legal actions, execute activation commands, mutate state, or load production card data.

## Current Coordinator And Owner Delegates

`UWBRuntimeDecisionPointCoordinatorComponent` can refresh activation presentation separately from normal `FWBAction` decision points, then delegate selected activation id resolution to the activation presentation model.

`UWBRuntimeDecisionPointOwnerComponent` can delegate activation presentation refresh and selected activation id resolution through the coordinator. Missing coordinator and missing activation model cases report explicit failure reasons.

## Internal Command Data Available After Resolution

The resolved `FWBCardActivationLegalAction` retains internal activation command data, including the future execution command, source effect id, usage commit, cost payment commit, and debug activation id. That internal data is needed for a later explicit execution integration pass, but it is not public presentation data.

## Why Add A Handoff Stub

Runtime now needs a narrow bridge between "selection resolved" and "execution exists." The bridge should preserve all data future execution wiring will need while making it impossible to accidentally imply that runtime activation execution is implemented.

This pass therefore adds a structured handoff result that accepts an `FWBRuntimeActivationSelectionResolution`, verifies it is resolved, copies the internal action and public presentation entries, and returns `activation_execution_not_implemented` for successful selections.

## Why The Stub Does Not Execute

Activation execution remains deferred because the production runtime still lacks explicit zone ownership, CardDB loading, real cost payment wiring, card usage marking, target picking UI, response windows, negation, passives, and wand behavior. Executing now would couple a presentation bridge to incomplete rules and zone systems.

Execution must remain a later explicit pass that chooses the correct rules owner, state mutation path, trace behavior, and hidden-information boundaries deliberately.

## EffectRunner Boundary

The stub must not call `WBEffectRunner` or `WBEffectRunner::ApplyCardActivationCommand`. In the architecture, `EffectRunner` mutates rules state. This runtime handoff is only a structured result creator and cannot inspect or mutate `FWBGameStateData`.

## Activation Remains Separate From FWBAction

Activation legal actions remain a separate action family from normal `FWBAction` decisions. This pass must not add activation to `EWBActionType`, must not modify `WBActionCodec`, and must not modify `WBRules::GenerateLegalActions`.

## Hidden-Information Boundary

The handoff result may retain internal `ActivationAction.Command` metadata for future execution wiring. Public presentation entries and reason strings must not expose hidden metadata such as `SourceEffectId`, `UsageKey`, `DebugActivationId`, `CostKind`, fixture zone context, hidden hand/deck/discard data, rules hooks, or schema internals.

## Deferred Work

- activation execution
- production card zones
- Godot CardDB import
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
