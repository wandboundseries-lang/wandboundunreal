# Runtime Action Selection Bridge Audit

## Action ID Source

Runtime action IDs come from `WBActionCodec::MakeActionId`.

Current stable formats are:

- Move: `move:p{player}:u{unit}:{from_x},{from_y}->{to_x},{to_y}`
- Attack: `attack:p{player}:u{source}:t{target}`
- EndTurn: `end_turn:p{player}`
- Pass: `pass:p{player}`
- PassResponse: `pass_response:p{player}`

The action ID does not include runtime context, MP roll values, visual/controller state, UObject identity, or hidden information.

## Runtime Controller Facade

`UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction` accepts external mutable `FWBGameStateData&`, an external `FWBAction`, and an external `FWBRuntimeTurnResolutionContext`.

The facade builds `FWBSelectedActionVisualHarnessOptions` from its refresh booleans, delegates to `WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals`, stores the latest runtime and visual refresh results, and marks that an action has executed.

The facade does not own or cache rules state, generate legal actions, call `WBRules` legality APIs, or inspect deck/hand/discard data.

## Selected-Action Harness

`WBSelectedActionVisualHarness` applies the selected action through `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`, then optionally refreshes `UWBRuntimeVisualControllerComponent` based on success/failure refresh policy.

The harness does not choose actions. It executes the already selected `FWBAction` and refreshes visuals from the runtime result's public board summary.

## Why The Bridge Accepts A Precomputed Legal Action List

Future input/UI needs a runtime-safe way to take a selected action ID and execute the corresponding `FWBAction`. The legal action list must be produced elsewhere by the rules/action generation layer. This keeps the rule boundary intact:

- Rules validate legality.
- EffectRunner mutates state.
- UI selects only.
- Runtime visuals display public outcomes only.

The bridge scans only the provided action list, compares each action's `WBActionCodec` ID, and returns the matching action. It does not know how the list was produced and does not filter or alter legality.

## Why The Bridge Does Not Generate Or Validate Legal Actions

Generating legal actions or making legality decisions would put rule behavior into runtime selection code. This bridge is only an ID resolver and delegation helper. It must not call `WBRules::GenerateLegalActions`, `CanMoveUnit`, `CanDeclareAttack`, `CanEndTurn`, or other legality APIs.

Duplicate IDs are treated as ambiguous so the bridge never chooses arbitrarily.

## Why The Bridge Does Not Own Game State

`FWBGameStateData` remains external deterministic rules state. The bridge receives state by reference only for successful delegation to the runtime controller facade. Failed resolution or a null facade must not mutate state.

## Null Facade Policy

The bridge requires a non-null `UWBRuntimeControllerFacadeComponent` for execution. If no facade is provided, execution fails with `runtime_controller_facade_missing` and does not mutate state.

Low-level execution without a facade already exists through the runtime adapter and selected-action harness. This bridge is specifically the selected-ID-to-facade path.

## Hidden Information Boundary

The bridge compares public action ID strings against a supplied action array. It must not read deck, hand, discard, private choices, hidden marker identity, or raw hidden state. Failure reasons are stable generic strings and must not include legal action list contents or hidden data.

## Intentionally Not Implemented

- input
- UI
- tile selection
- unit selection
- action menus
- camera behavior
- animation
- VFX
- sound
- marker visuals
- hidden marker identity
- assets
- Blueprints
- UMG
- `.uasset` edits
- `.umap` edits
- gameplay ownership
- legal action generation
- legality decisions
