# Pending Effect Activation Audit

## Baseline

- Commit: `4b87cfe09e60f65476076b2b2100b66284bee1a6`
- `HEAD` and `origin/main`: synchronized at audit start.
- Initial Wandbound automation baseline: 2,175 tests.
- Replay schema: version 1.
- Canonical startup hash: `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- `WBActionCodec` was not modified by this pass.
- No staged files existed at audit start.

The working tree already contained unrelated tracked and untracked work. The only
required overlap was `Source/WandboundCore/Private/WBGameStateData.cpp`; this pass
did not modify that file. No baseline-dirty file was overwritten or reverted.

## Canonical Findings

Tracked canon and the read-only Godot reference agree on the established response
convention:

- the opponent of the activating player receives first priority;
- a legal React resets the pass streak and priority alternates;
- two consecutive total passes close the current window;
- a player with no legal React is automatically passed by rules authority;
- resolving a nested activation restores the suspended parent response context.

The Unreal implementation preserves these semantics through the existing typed
`PostEffect` reaction window. It does not add a sixth response timing.

## Godot Behavior Audit

The Godot reference contains a useful generic shape but also several card-specific
shortcuts that are not suitable for Unreal production architecture:

- `pending_effect_activation` stores the current effect activation.
- `pending_nested_response_activation_stack` stores suspended parent contexts.
- accepted nested hand responses are discarded before their effect resolves.
- ordinary accepted activation usage is marked before nested resolution.
- `_begin_nested_hand_response_activation_impl` pushes the parent context, opens
  `effect_activation_pending`, and gives first priority to the opponent.
- `_restore_nested_response_activation_context_impl` restores the parent event,
  effect, attack, priority, and pass state.

Card-specific Godot branches found during the audit:

- `char_marrow_oddsman` and `char_marrow_claimshifter` are handled in the pending
  hand-hook finalization sequence.
- `wand_equip_ww_sealplate` is singled out while queuing and consuming effect
  activation hooks.
- `effect_react_csn_crash_in` and `wand_react_sever_thread` are explicitly excluded
  from the ordinary nested effect-window path.
- Null Sigil/Null Thread encode negation against `pending_effect_activation`.
- Shatter Parry uses the ordinary response category while Sever Thread takes a
  bespoke non-nested path despite similar prevention intent.

Unreal does not reproduce any of those exact-ID branches. Oddsman, Sealplate,
Null Sigil, Claimshifter, Crash-In, Sever Thread, and Shatter Parry remain future
data/content mappings over the generic pending-frame lifecycle.

## Existing Unreal Behavior

Before this pass, `WBMatchCoordinator` generated canonical Response actions and
owned priority/pass transitions, but `Activation` submitted directly to
`WBEffectRunner::ApplyCardActivationCommand`. The effect mutated immediately and
`AdvanceReactionAfterReact` only advanced the surrounding response window.

That immediate path could not represent A -> B -> C, exact-frame negation, or
restoration of a suspended parent activation. The coordinator already owned the
correct response authority and therefore remained the extension point.

## Implemented Boundary

The coordinator now owns a typed LIFO stack of immutable pending activation frames.
Each frame records only deterministic private authority data:

- stable frame and parent frame IDs;
- accepted activation action ID and player;
- immutable activation command and selected targets;
- suspended parent reaction state and priority;
- negated state.

Runtime and UI receive public observations and legal actions only. They cannot push,
pop, negate, restore, or resolve frames.

Hand, Board, and Equipped sources use the same declaration and resolution path.
Hand commands carry an exact source instance ID and are discarded on acceptance.
All existing public activation action IDs retain their prior format. The exact Hand
instance remains private in the accepted command and pending frame only.

## Cost and Usage Semantics

- legality and affordability are checked when the action is accepted;
- once-per-turn usage is reserved on acceptance, matching the Godot pending model
  and preventing recursive reuse before resolution;
- a rejected action reserves nothing;
- RR remains paid only by the existing successful EffectRunner transaction;
- a negated frame consumes its accepted once-per-turn usage, pays no RR, and applies
  no payload;
- a failed resolution is traced, popped once, and restores its parent instead of
  leaving the match stuck in Response.

This pass does not change equip RL reservation or general resonance calculations.

## Unsupported and Deferred

- Suspended attack continuation is intentionally not implemented in this pass.
- Optional hook activation is supported only when represented as an ordinary legal
  activation definition; no separate optional-hook state machine was added.
- Card-specific coin flips, copy semantics, prevention semantics, and trigger text
  for the audited Godot cards remain out of scope.
- A pending activation cannot target an arbitrary older frame: the legal generic
  negation command is bound to the exact current top parent frame.
- Public observations do not expose private frame IDs, hidden candidate cards, or
  protected state/trace digests.

## Replay and Privacy

Accepted activation and pass action IDs remain normal replay decisions. Automatic
passes, frame declaration, negation, close, resolution, skip, and parent restoration
are deterministic trace events. Pending-frame state contributes to the coordinator
state digest without changing replay schema version 1.

The replay receipt remains exactly eight fields. It contains no frame IDs, state
digest, trace digest, opponent hand identity, or hidden candidate identity.

## Required Follow-up

Add coordinator-owned suspended attack continuation for canonical pre-hit -> damage
-> post-hit -> counter sequencing using the generic pending-effect activation
lifecycle.
