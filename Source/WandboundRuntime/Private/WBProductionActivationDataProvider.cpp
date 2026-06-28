#include "WBProductionActivationDataProvider.h"

#include "WBCardActivationSourceGate.h"
#include "WBCardZoneObservation.h"
#include "WBPublicBoardSummary.h"

namespace
{
enum class EWBProductionActivationSourceSortZone : uint8
{
	Board,
	Hand
};

struct FWBProductionActivationActionBuildEntry
{
	FWBCardActivationLegalAction Action;
	EWBProductionActivationSourceSortZone SortZone = EWBProductionActivationSourceSortZone::Board;
	int32 SortPlayerId = -1;
	int32 SortIndex = INDEX_NONE;
	FString SortCardId;
	FString SortEffectId;
};

void AddDiagnostic(
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics,
	const FString& Code,
	const FString& CardId = FString(),
	const FString& InstanceId = FString(),
	const int32 UnitId = -1)
{
	FWBProductionActivationDataProviderDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.CardId = CardId;
	Diagnostic.InstanceId = InstanceId;
	Diagnostic.UnitId = UnitId;
	Diagnostics.Add(Diagnostic);
}

FWBRuntimeActivationDataProviderResult MakeProviderFailure(
	const FWBRuntimeActivationDataProviderRequest& Request,
	const FString& Reason,
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics)
{
	FWBRuntimeActivationDataProviderResult Result;
	Result.Request = Request;
	Result.Reason = Reason;
	AddDiagnostic(Diagnostics, Reason);
	return Result;
}

bool IsPublicLabelAllowed(const FString& PublicLabel)
{
	FString ForbiddenTerm;
	return !WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(
		PublicLabel,
		ForbiddenTerm);
}

bool IsDefinitionPublicTextAllowed(const FWBCardDefinition& Definition)
{
	if (!IsPublicLabelAllowed(Definition.PublicName))
	{
		return false;
	}

	for (const FWBCardEffectDefinition& Effect : Definition.ActivatedEffects)
	{
		if (!IsPublicLabelAllowed(Effect.PublicLabel))
		{
			return false;
		}
	}

	return true;
}

bool IsSourceZoneSupportedByConfig(
	const EWBCardActivationSourceZone Zone,
	const FWBProductionActivationDataProviderConfig& Config)
{
	switch (Zone)
	{
	case EWBCardActivationSourceZone::Board:
		return Config.bIncludeBoardSources;
	case EWBCardActivationSourceZone::Hand:
		return Config.bIncludeOwnHandSources;
	case EWBCardActivationSourceZone::Discard:
		return Config.bIncludeDiscardSources;
	case EWBCardActivationSourceZone::Equipped:
		return Config.bIncludeEquippedSources;
	default:
		return false;
	}
}

FString MakeActivationActionId(
	const EWBProductionActivationSourceSortZone Zone,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const FString& InstanceId,
	const FString& CardId,
	const FString& EffectId)
{
	if (Zone == EWBProductionActivationSourceSortZone::Board)
	{
		return FString::Printf(
			TEXT("activate_source:p%d:zboard:u%d:c%s:e%s"),
			PlayerId,
			SourceUnitId,
			*CardId,
			*EffectId);
	}

	return FString::Printf(
		TEXT("activate_source:p%d:zhand:i%s:c%s:e%s"),
		PlayerId,
		*InstanceId,
		*CardId,
		*EffectId);
}

FWBCardActivationSourceGateContext MakeSourceGateContext(
	const int32 PlayerId,
	const int32 SourceUnitId,
	const FString& SourceCardId,
	const EWBCardActivationSourceZone SourceZone,
	const FWBCardActivationFixtureZoneContext& FixtureZoneContext)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = PlayerId;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceCardId = SourceCardId;
	Context.SourceZone = SourceZone;
	Context.FixtureZoneContext = FixtureZoneContext;
	Context.bCostsSatisfiedExternally = true;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationFixtureZoneContext MakeOwnHandFixtureZoneContext(
	const FWBCardZonePlayerObservation& Observation)
{
	FWBCardActivationFixtureZoneContext Context;
	for (const FWBObservedCardRef& Card : Observation.OwnHand.Cards)
	{
		FWBCardActivationFixtureZoneEntry Entry;
		Entry.CardId = Card.CardId;
		Entry.OwnerPlayerId = Observation.ViewerPlayerId;
		Entry.Zone = EWBCardActivationSourceZone::Hand;
		Context.Entries.Add(Entry);
	}
	return Context;
}

FString PublicLabelForEffect(const FWBCardEffectDefinition& Effect)
{
	return Effect.PublicLabel.IsEmpty() ? FString(TEXT("Activate")) : Effect.PublicLabel;
}

bool IsValidUnitTargetOptionSource(const FWBPublicUnitBoardSummary& Unit)
{
	return Unit.UnitId >= 0
		&& FWBGameStateData::IsValidPlayerId(Unit.OwnerId)
		&& Unit.X >= 0
		&& Unit.Y >= 0
		&& !Unit.CardId.IsEmpty();
}

FString PublicLabelForTargetUnit(const FWBPublicUnitBoardSummary& Unit)
{
	return FString::Printf(TEXT("Unit %d"), Unit.UnitId);
}

TArray<FWBCardActivationTargetOption> BuildUnitTargetOptions(
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	TArray<FWBCardActivationTargetOption> Options;
	Options.Reserve(PublicBoardSummary.Units.Num());

	for (const FWBPublicUnitBoardSummary& Unit : PublicBoardSummary.Units)
	{
		if (!IsValidUnitTargetOptionSource(Unit))
		{
			continue;
		}

		FWBCardActivationTargetOption Option;
		Option.Type = EWBCardActivationTargetOptionType::Unit;
		Option.TargetUnitId = Unit.UnitId;
		Option.TargetOwnerPlayerId = Unit.OwnerId;
		Option.TargetTile = FWBTile(Unit.X, Unit.Y);
		Option.TargetCardId = Unit.CardId;
		Option.PublicLabel = PublicLabelForTargetUnit(Unit);
		Options.Add(Option);
	}

	Options.Sort([](const FWBCardActivationTargetOption& A, const FWBCardActivationTargetOption& B)
	{
		if (A.TargetOwnerPlayerId != B.TargetOwnerPlayerId)
		{
			return A.TargetOwnerPlayerId < B.TargetOwnerPlayerId;
		}

		if (A.TargetTile.Y != B.TargetTile.Y)
		{
			return A.TargetTile.Y < B.TargetTile.Y;
		}

		if (A.TargetTile.X != B.TargetTile.X)
		{
			return A.TargetTile.X < B.TargetTile.X;
		}

		if (A.TargetUnitId != B.TargetUnitId)
		{
			return A.TargetUnitId < B.TargetUnitId;
		}

		return A.TargetCardId < B.TargetCardId;
	});

	return Options;
}

TArray<FWBCardActivationTargetOption> BuildTargetOptionsForEffect(
	const FWBCardEffectDefinition& Effect,
	const FWBPublicBoardSummary& PublicBoardSummary,
	const FWBCardDefinition& Definition,
	const FString& InstanceId,
	const int32 SourceUnitId,
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics)
{
	switch (Effect.TargetRequirement)
	{
	case EWBCardEffectTargetRequirement::None:
		return TArray<FWBCardActivationTargetOption>();
	case EWBCardEffectTargetRequirement::Unit:
	{
		TArray<FWBCardActivationTargetOption> Options = BuildUnitTargetOptions(PublicBoardSummary);
		if (Options.IsEmpty())
		{
			AddDiagnostic(Diagnostics, TEXT("no_unit_targets_available"), Definition.CardId, InstanceId, SourceUnitId);
		}
		return Options;
	}
	case EWBCardEffectTargetRequirement::Tile:
	case EWBCardEffectTargetRequirement::WallEdge:
		AddDiagnostic(Diagnostics, TEXT("target_options_deferred"), Definition.CardId, InstanceId, SourceUnitId);
		return TArray<FWBCardActivationTargetOption>();
	default:
		AddDiagnostic(Diagnostics, TEXT("unsupported_target_requirement"), Definition.CardId, InstanceId, SourceUnitId);
		return TArray<FWBCardActivationTargetOption>();
	}
}

FWBCardActivationLegalAction MakeTargetDeferredAction(
	const FWBCardDefinition& Definition,
	const FWBCardEffectDefinition& Effect,
	const EWBProductionActivationSourceSortZone SortZone,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const FString& InstanceId,
	const TArray<FWBCardActivationTargetOption>& TargetOptions)
{
	FWBCardActivationLegalAction Action;
	Action.ActivationActionId = MakeActivationActionId(
		SortZone,
		PlayerId,
		SourceUnitId,
		InstanceId,
		Definition.CardId,
		Effect.EffectId);
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = SourceUnitId;
	Action.PublicLabel = PublicLabelForEffect(Effect);
	Action.TargetOptions = TargetOptions;

	Action.Candidate.ActivationCandidateId = Action.ActivationActionId;
	Action.Candidate.PlayerId = PlayerId;
	Action.Candidate.SourceUnitId = SourceUnitId;
	Action.Candidate.SourceCardId = Definition.CardId;
	Action.Candidate.SourceEffectId = Effect.EffectId;
	Action.Candidate.PublicLabel = Action.PublicLabel;

	Action.Command.Source.PlayerId = PlayerId;
	Action.Command.Source.SourceUnitId = SourceUnitId;
	Action.Command.Source.SourceCardId = Definition.CardId;
	Action.Command.Source.SourceEffectId = Effect.EffectId;
	Action.Command.EffectRequest.Source.PlayerId = PlayerId;
	Action.Command.EffectRequest.Source.SourceUnitId = SourceUnitId;
	Action.Command.EffectRequest.Source.SourceCardId = Definition.CardId;
	Action.Command.EffectRequest.Source.SourceEffectId = Effect.EffectId;
	return Action;
}

void AppendActionForEffectIfAllowed(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const FWBCardEffectDefinition& Effect,
	const EWBProductionActivationSourceSortZone SortZone,
	const EWBCardActivationSourceZone RequiredZone,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const int32 SortIndex,
	const FString& InstanceId,
	const FWBCardActivationFixtureZoneContext& FixtureZoneContext,
	const FWBProductionActivationDataProviderConfig& Config,
	const FWBPublicBoardSummary& PublicBoardSummary,
	TArray<FWBProductionActivationActionBuildEntry>& OutEntries,
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics)
{
	if (!Effect.SourceGate.bHasExplicitSourceGate
		|| Effect.SourceGate.RequiredZone != RequiredZone
		|| !IsSourceZoneSupportedByConfig(Effect.SourceGate.RequiredZone, Config))
	{
		AddDiagnostic(Diagnostics, TEXT("unsupported_source_zone"), Definition.CardId, InstanceId, SourceUnitId);
		return;
	}

	if (!IsPublicLabelAllowed(PublicLabelForEffect(Effect)))
	{
		AddDiagnostic(Diagnostics, TEXT("public_label_rejected"), Definition.CardId, InstanceId, SourceUnitId);
		return;
	}

	const FWBCardActivationSourceGateContext SourceGateContext = MakeSourceGateContext(
		PlayerId,
		SourceUnitId,
		Definition.CardId,
		RequiredZone,
		FixtureZoneContext);
	const FWBCardActivationSourceGateResult SourceGateResult =
		WBCardActivationSourceGate::Evaluate(State, Effect.SourceGate, SourceGateContext);
	if (!SourceGateResult.bOk)
	{
		return;
	}

	const TArray<FWBCardActivationTargetOption> TargetOptions = BuildTargetOptionsForEffect(
		Effect,
		PublicBoardSummary,
		Definition,
		InstanceId,
		SourceUnitId,
		Diagnostics);

	FWBProductionActivationActionBuildEntry Entry;
	Entry.Action = MakeTargetDeferredAction(
		Definition,
		Effect,
		SortZone,
		PlayerId,
		SourceUnitId,
		InstanceId,
		TargetOptions);
	Entry.SortZone = SortZone;
	Entry.SortPlayerId = PlayerId;
	Entry.SortIndex = SortIndex;
	Entry.SortCardId = Definition.CardId;
	Entry.SortEffectId = Effect.EffectId;
	OutEntries.Add(Entry);
}

void AppendActionsForDefinition(
	const FWBGameStateData& State,
	const FWBCardDefinition& Definition,
	const EWBProductionActivationSourceSortZone SortZone,
	const EWBCardActivationSourceZone RequiredZone,
	const int32 PlayerId,
	const int32 SourceUnitId,
	const int32 SortIndex,
	const FString& InstanceId,
	const FWBCardActivationFixtureZoneContext& FixtureZoneContext,
	const FWBProductionActivationDataProviderConfig& Config,
	const FWBPublicBoardSummary& PublicBoardSummary,
	TArray<FWBProductionActivationActionBuildEntry>& OutEntries,
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics)
{
	if (!IsDefinitionPublicTextAllowed(Definition))
	{
		AddDiagnostic(Diagnostics, TEXT("public_label_rejected"), Definition.CardId, InstanceId, SourceUnitId);
		return;
	}

	for (const FWBCardEffectDefinition& Effect : Definition.ActivatedEffects)
	{
		AppendActionForEffectIfAllowed(
			State,
			Definition,
			Effect,
			SortZone,
			RequiredZone,
			PlayerId,
			SourceUnitId,
			SortIndex,
			InstanceId,
			FixtureZoneContext,
			Config,
			PublicBoardSummary,
			OutEntries,
			Diagnostics);
	}
}

void AppendBoardSourceActions(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId,
	const FWBProductionActivationDataProviderConfig& Config,
	const FWBPublicBoardSummary& PublicBoardSummary,
	TArray<FWBProductionActivationActionBuildEntry>& OutEntries,
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics)
{
	if (!Config.bIncludeBoardSources)
	{
		return;
	}

	for (const FWBPublicUnitBoardSummary& UnitSummary : PublicBoardSummary.Units)
	{
		if (UnitSummary.OwnerId != ViewerPlayerId)
		{
			continue;
		}

		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, UnitSummary.CardId);
		if (!Lookup.bFound)
		{
			AddDiagnostic(Diagnostics, TEXT("card_definition_not_found"), UnitSummary.CardId, FString(), UnitSummary.UnitId);
			continue;
		}

		AppendActionsForDefinition(
			State,
			Lookup.Definition,
			EWBProductionActivationSourceSortZone::Board,
			EWBCardActivationSourceZone::Board,
			ViewerPlayerId,
			UnitSummary.UnitId,
			UnitSummary.UnitId,
			FString(),
			FWBCardActivationFixtureZoneContext(),
			Config,
			PublicBoardSummary,
			OutEntries,
			Diagnostics);
	}
}

void AppendOwnHandSourceActions(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId,
	const FWBProductionActivationDataProviderConfig& Config,
	const FWBPublicBoardSummary& PublicBoardSummary,
	TArray<FWBProductionActivationActionBuildEntry>& OutEntries,
	TArray<FWBProductionActivationDataProviderDiagnostic>& Diagnostics)
{
	if (!Config.bIncludeOwnHandSources)
	{
		return;
	}

	const FWBCardZonePlayerObservation Observation =
		WBCardZoneObservation::BuildObservationForPlayer(State, ViewerPlayerId);
	const FWBCardActivationFixtureZoneContext FixtureZoneContext =
		MakeOwnHandFixtureZoneContext(Observation);

	for (int32 HandIndex = 0; HandIndex < Observation.OwnHand.Cards.Num(); ++HandIndex)
	{
		const FWBObservedCardRef& Card = Observation.OwnHand.Cards[HandIndex];
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Card.CardId);
		if (!Lookup.bFound)
		{
			AddDiagnostic(Diagnostics, TEXT("card_definition_not_found"), Card.CardId, Card.InstanceId);
			continue;
		}

		AppendActionsForDefinition(
			State,
			Lookup.Definition,
			EWBProductionActivationSourceSortZone::Hand,
			EWBCardActivationSourceZone::Hand,
			ViewerPlayerId,
			-1,
			HandIndex,
			Card.InstanceId,
			FixtureZoneContext,
			Config,
			PublicBoardSummary,
			OutEntries,
			Diagnostics);
	}
}

void SortActionEntries(TArray<FWBProductionActivationActionBuildEntry>& Entries)
{
	Entries.Sort([](const FWBProductionActivationActionBuildEntry& A, const FWBProductionActivationActionBuildEntry& B)
	{
		if (A.SortZone != B.SortZone)
		{
			return static_cast<uint8>(A.SortZone) < static_cast<uint8>(B.SortZone);
		}

		if (A.SortPlayerId != B.SortPlayerId)
		{
			return A.SortPlayerId < B.SortPlayerId;
		}

		if (A.SortIndex != B.SortIndex)
		{
			return A.SortIndex < B.SortIndex;
		}

		if (A.SortCardId != B.SortCardId)
		{
			return A.SortCardId < B.SortCardId;
		}

		return A.SortEffectId < B.SortEffectId;
	});
}

FWBCardActivationLegalActionSet BuildActionSetFromEntries(
	TArray<FWBProductionActivationActionBuildEntry>& Entries)
{
	SortActionEntries(Entries);

	FWBCardActivationLegalActionSet ActionSet;
	ActionSet.Actions.Reserve(Entries.Num());
	for (const FWBProductionActivationActionBuildEntry& Entry : Entries)
	{
		ActionSet.Actions.Add(Entry.Action);
	}
	return ActionSet;
}
}

void FWBProductionActivationDataProvider::Configure(
	const FWBProductionActivationDataProviderInput& InInput,
	const FWBProductionActivationDataProviderConfig& InConfig)
{
	Input = InInput;
	Config = InConfig;
	bConfigured = true;
	LastDiagnostics.Reset();
}

bool FWBProductionActivationDataProvider::IsConfigured() const
{
	return bConfigured;
}

TArray<FWBProductionActivationDataProviderDiagnostic>
FWBProductionActivationDataProvider::GetDiagnosticsForTest() const
{
	return LastDiagnostics;
}

FWBRuntimeActivationDataProviderResult FWBProductionActivationDataProvider::GetActivationDecisionData(
	const FWBRuntimeActivationDataProviderRequest& Request) const
{
	LastDiagnostics.Reset();

	if (!bConfigured)
	{
		return MakeProviderFailure(Request, TEXT("provider_not_configured"), LastDiagnostics);
	}

	if (Input.GameState == nullptr)
	{
		return MakeProviderFailure(Request, TEXT("game_state_missing"), LastDiagnostics);
	}

	if (Input.Repository == nullptr)
	{
		return MakeProviderFailure(Request, TEXT("repository_missing"), LastDiagnostics);
	}

	if (!FWBGameStateData::IsValidPlayerId(Input.ViewerPlayerId)
		|| Input.GameState->GetPlayerById(Input.ViewerPlayerId) == nullptr)
	{
		return MakeProviderFailure(Request, TEXT("invalid_viewer_player"), LastDiagnostics);
	}

	FWBRuntimeActivationDataProviderResult Result;
	Result.bOk = true;
	Result.Request = Request;
	Result.RefreshInput.PublicBoardSummary = WBPublicBoardSummary::Build(*Input.GameState);

	TArray<FWBProductionActivationActionBuildEntry> ActionEntries;
	AppendBoardSourceActions(
		*Input.GameState,
		*Input.Repository,
		Input.ViewerPlayerId,
		Config,
		Result.RefreshInput.PublicBoardSummary,
		ActionEntries,
		LastDiagnostics);
	AppendOwnHandSourceActions(
		*Input.GameState,
		*Input.Repository,
		Input.ViewerPlayerId,
		Config,
		Result.RefreshInput.PublicBoardSummary,
		ActionEntries,
		LastDiagnostics);

	Result.RefreshInput.ActivationActionSet = BuildActionSetFromEntries(ActionEntries);
	return Result;
}
