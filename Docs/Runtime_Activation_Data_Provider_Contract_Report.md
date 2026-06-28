# Runtime Activation Data Provider Contract Report

## Purpose

This pass adds a production-safe, test-only contract suite for activation data providers. It defines the shape CardDB/zone-backed providers must satisfy.

`FWBProductionActivationDataProvider` now exercises the same interface as the first production provider skeleton. No CardDB import, production card movement, UI widgets, response windows, or activation `FWBAction` integration were added.

## Contract Shape

Successful current-decision and post-activation provider results must use request kind `CurrentDecisionPoint` or `PostActivation`.

Provider results are expected to carry externally prepared:

- normal legal `FWBAction` values
- `FWBCardActivationLegalActionSet`, including provider-supplied target options when available
- `FWBPublicBoardSummary`

Successful provider results must also include a valid player id and non-empty decision point id. Post-activation provider requests must include the selected activation action id that triggered the refresh.

Every activation legal action must have a non-empty `ActivationActionId`.

Target options are part of the externally supplied activation action data. The contract debug string includes target option ids, owners, tiles, visible card ids, and public labels so hidden-token checks and determinism checks cover that payload.

The expectations helper can require minimum counts for normal legal actions, activation actions, and public summary units. It can also allow phases with no activation actions when a test configures that explicitly.

## Determinism

`FWBRuntimeActivationDataProviderContractVerifier::VerifyDeterministicProviderOutput` calls the same provider twice with the same request and compares test-only debug strings. A deterministic provider passes, while the intentionally changing test provider fails with `provider_output_not_deterministic`.

## Hidden Information

The contract verifier checks a test-only debug string against forbidden substrings. The negative coverage proves internal or hidden tokens such as private labels, raw schema wording, and internal operation names are rejected when they appear in provider-facing output.

Provider-supplied presentation should keep using clean public labels such as `Activate`, `Choose Unit`, `Choose Tile`, and `Choose Wall`.

Provider-supplied target option labels must also avoid raw schema or implementation terms.

## Provider Failure Policy

A failing provider returns `bOk=false` and a reason. The adapter propagates that reason and does not refresh the session.

For execute-and-refresh, `WBRuntimeActivationDataProviderAdapter` asks the provider for post-activation data before executing. If the provider fails, execution is not attempted and state is unchanged.

## Adapter Behavior

Valid provider output refreshes the activation decision-session facade from external data.

Valid post-activation provider output lets the adapter execute the selected activation through the facade and then refresh from the supplied post-action data.

Invalid or failing provider output stops before refresh or execution.

## Test-Only Locations

- `Source/WandboundTests/Private/WBRuntimeActivationDataProviderContractVerifier.h`
- `Source/WandboundTests/Private/WBRuntimeActivationDataProviderContractVerifier.cpp`
- `Source/WandboundTests/Private/WBRuntimeActivationContractDataProvidersForTests.h`
- `Source/WandboundTests/Private/WBRuntimeActivationContractDataProvidersForTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeActivationDataProviderContractTests.cpp`

The verifier debug string is test-only and must not become runtime output.

## Action Boundary

Activation legal actions remain separate from `FWBAction`.

`WBActionCodec` is unchanged. `WBRules::GenerateLegalActions` is unchanged.

## Runtime Boundary

Runtime production code still does not:

- generate normal legal actions
- execute effects
- own or mutate rules state through the provider shell
- inspect hidden zones

The production provider may build read-only activation source/effect action-set data from explicit inputs, but runtime sessions still consume that data externally.

## Production Provider Contract Coverage

`WBProductionActivationDataProviderTests.cpp` and `WBProductionActivationDataProviderTargetOptionsTests.cpp` add production provider contract coverage for configuration failures, board-source activations, own-hand activations, unit target options, hidden-info exclusion, deterministic output, session facade refresh, no mutation, no effect execution, no `WBRules::GenerateLegalActions` call, and no `WBActionCodec` dependency.

## Future Provider Guidance

`Docs/Runtime_Activation_CardDB_Zone_Provider_Plan.md` now records how this contract should guide future CardDB/zone-backed provider implementations. Future providers should extend the same request/result and hidden-information expectations before adding production zones or CardDB import.

## Fixtures

No GodotCanon fixtures were added. This pass verifies C++ provider-interface behavior, transient component refresh behavior, and adapter failure policy. C++ automation is the clearer format for this contract-level surface.

## Out Of Scope

- production provider
- CardDB import
- production zones
- UI widgets
- target picking
- response windows
- effect negation
- passives
- wands
- overflow destruction
- wand destruction or redirect
- card-specific prevention or replacement
- death triggers
- counters
- random dice
- NPC phase
- visual animation
- VFX
- sound
- Blueprints, `.uasset`, or `.umap` changes

## Validation

Build succeeded:

```text
Result: Succeeded
Total execution time: 10.33 seconds
```

Wandbound automation succeeded:

```text
succeeded=756
succeededWithWarnings=0
failed=0
notRun=0
```

Added 21 `Wandbound.Runtime.ActivationDataProviderContract.*` automation tests.
