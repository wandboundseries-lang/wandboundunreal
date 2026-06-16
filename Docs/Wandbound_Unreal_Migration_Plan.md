# Wandbound Unreal Migration Plan

## Current Strategy

- Godot remains the behavior reference.
- Unreal starts as a rules-kernel-first prototype.
- Do not port visuals before rules are testable.

## Phase 1 - Project Setup

- AGENTS.md
- module plan
- build/test command discovery
- Git baseline

## Phase 2 - WandboundCore Rules Kernel

- board state
- unit state
- walls
- actions
- legal action generation
- effect runner
- trace events
- deterministic replay

## Milestone 1 - Board State and Movement

- WandboundCore module added
- pure data state added
- movement legality added
- ApplyMove added
- movement trace event added
- automation tests added
- no Actor/Blueprint/UI dependency

## Milestone: Turn Flow + Legal Action Generation

- Implemented state fields:
  - `CurrentPlayer`
  - `PriorityPlayer`
  - `TurnNumber`
  - `Phase`
  - `bGameOver`
  - player `RemainingMP` storage for deterministic future turn setup
- Added basic state helpers:
  - player ID validation
  - current-player lookup
  - priority-player lookup
  - normal/response phase checks
  - `AdvanceTurnBasic`
- Legal actions currently supported:
  - `Move`
  - `EndTurn`
  - `Pass`
  - `PassResponse`
- Deterministic action ordering:
  - units are traversed in stable state order
  - movement destinations are evaluated east, west, south, north
  - `EndTurn` is appended after movement
  - explicit response phase generates `PassResponse` as the sole response action for the priority player
- Intentionally not implemented yet:
  - attack
  - summon
  - equip
  - cast/effects
  - card draw
  - MP dice roll
  - start/end turn triggers
  - full response activations

## Phase 3 - Movement

- 9x9 bounds
- orthogonal movement
- MP cost
- wall blocking
- unit blocking
- root/stun checks

## Phase 4 - Attack Declaration

- attacks_left
- AR
- orthogonal LOS
- wall blocking
- stunned/cannot attack
- trace event

## Phase 5 - Damage and Counterattacks

- damage
- HP death
- counter timing
- prevention hooks later

## Phase 6 - Turn Structure

- turn start
- draw
- MP roll
- action phase
- turn end
- status ticks
- NPC phase later

## Phase 7 - Card Data Import

- keep source of truth data-driven
- import from JSON or generated data
- validate card IDs/effects/passives

## Phase 8 - EffectRunner

- effect dispatch
- targeting
- statuses
- stat modification
- healing
- wall effects
- terrain effects

## Phase 9 - Response Windows

- pre-hit
- post-hit
- post-move
- post-summon
- post-effect
- pass priority
- negation

## Phase 10 - AI/Replay/Logs

- observations
- legal actions
- chosen actions
- resolved trace events
- semantic logs
- hidden-information firewall

## Phase 11 - 3D Runtime

- board actor
- tile interaction
- unit actors
- walls
- deck/discard slots
- camera
- UMG

## Phase 12 - Asset Migration

- card art
- slash.png
- 3D models
- materials/textures
- sounds

## First Milestone Definition of Done

- project builds
- WandboundCore module exists
- movement tests pass
- no Actor/Blueprint dependency in rules kernel
