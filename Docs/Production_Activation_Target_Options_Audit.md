# Production Activation Target Options Audit

Date: 2026-06-28

## Current Provider Decision Data

`FWBProductionActivationDataProvider` implements `IWBRuntimeActivationDataProvider` and returns `FWBRuntimeActivationDataProviderResult`.

Successful results fill `FWBRuntimeActivationDecisionSessionRefreshInput` with:

- externally supplied normal legal actions, currently empty for the production provider
- `FWBCardActivationLegalActionSet`
- `FWBPublicBoardSummary`

The existing runtime session, owner, coordinator, and activation presentation model already consume `FWBCardActivationLegalActionSet` as external data. They do not compute activation sources or targets internally.

## Current Target Deferred Behavior

Provider-emitted activation actions currently keep `FWBEffectTargetRef` unset. Any effect whose target requirement is not `None` records the non-fatal diagnostic `target_options_deferred`.

That behavior is correct for unsupported target categories, but unit targets can now be enumerated read-only from the public board summary.

## Existing Target Requirement Shape

`EWBCardEffectTargetRequirement` supports:

- `None`
- `Unit`
- `Tile`
- `WallEdge`

There are no stricter friendly/enemy/self unit target requirements in the current enum, so generic `Unit` means all visible valid board units are target options.

## Public Board Unit Visibility Policy

Visible board unit data is already public through `FWBPublicBoardSummary`.

Allowed target-option fields from visible board units:

- `UnitId`
- `OwnerId`
- tile
- visible board `CardId`

Hidden zone identities remain forbidden, including opponent hand cards, deck cards, and hidden marker `InternalMarkerCardId`.

## Chosen Target Option Data Shape

The existing runtime path already carries `FWBCardActivationLegalActionSet` end to end, so the smallest non-circular shape is a Core activation decision-data extension:

- `EWBCardActivationTargetOptionType`
- `FWBCardActivationTargetOption`
- `FWBCardActivationLegalAction::TargetOptions`

This avoids adding a runtime-owned target map that would need parallel coordinator/owner/model plumbing, and it avoids making Core depend on runtime provider-owned structs.

The production provider owns the enumeration decision and populates these options. Runtime remains a consumer.

## Supported Unit Target Behavior

For `Unit` target requirements, the provider will enumerate all visible valid board units from the public board summary:

- friendly units
- enemy units
- the source unit itself

Removed or defeated units are excluded by the existing public board summary policy and an additional defensive validity check.

Target options are deterministic by:

1. target owner player id
2. tile Y
3. tile X
4. unit id
5. card id

## Unsupported Target-Type Behavior

`None` produces no target options and no deferred diagnostic.

`Tile` and `WallEdge` remain deferred with `target_options_deferred`.

Unknown enum values fail closed with `unsupported_target_requirement`.

If a `Unit` target requirement has no valid visible board targets, the action remains emitted with an empty option list and diagnostic `no_unit_targets_available`.

## Hidden-Info Policy

Provider output and diagnostics must not contain:

- opponent hand `InstanceId`
- opponent hand `CardId`
- deck `InstanceId`
- deck `CardId`
- hidden marker `InternalMarkerCardId`
- raw schema labels or dev terms
- debug activation ids
- usage keys

Visible board `CardId` may appear because visible board unit identity is public under the current board-reference policy.

## Boundary Rationale

This pass does not execute effects. Target options are choices for future selection, not resolution.

This pass does not create `FWBAction`. Activation decision data remains separate from normal action IDs.

This pass does not change `WBRules::GenerateLegalActions`. Normal legal action generation remains unchanged and activation data remains externally supplied.

This pass does not change `WBActionCodec`. No activation action IDs are added to the normal action codec.

## Risks And Open Questions

- Future target filters may need friendly/enemy/self/range/line-of-sight requirements once canon is explicit.
- Tile and wall target enumeration remain deferred.
- Target-option display labels are minimal and must stay free of raw schema or implementation terms.
- Activation selection/execution still needs a later pass to bind a selected target option into an executable activation command.
