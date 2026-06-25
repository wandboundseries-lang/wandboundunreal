#include "WBRuntimeActivationPresentationModelComponent.h"

UWBRuntimeActivationPresentationModelComponent::UWBRuntimeActivationPresentationModelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeActivationPresentationModelComponent::SetCurrentActivationPresentationState(
	const FWBCardActivationLegalActionSet& ActivationActionSet,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	CurrentActivationActionSet = ActivationActionSet;
	CurrentActivationPresentationSnapshot = WBCardActivationLegalActionPresentation::BuildSnapshot(
		CurrentActivationActionSet,
		PublicBoardSummary);
	CurrentTargetPresentationSnapshot = WBCardActivationTargetPresentation::BuildSnapshot(
		CurrentActivationActionSet,
		PublicBoardSummary);

	bHasCurrentActivationPresentationState = true;
	CurrentStatus = FWBRuntimeActivationPresentationStatus();
	CurrentStatus.bReady = true;
	CurrentStatus.bHasActivationActions = CurrentActivationActionSet.Actions.Num() > 0;
	CurrentStatus.ActivationActionCount = CurrentActivationActionSet.Actions.Num();
	CurrentStatus.ActivationPresentationEntryCount = CurrentActivationPresentationSnapshot.Entries.Num();
	CurrentStatus.TargetPresentationEntryCount = CurrentTargetPresentationSnapshot.Entries.Num();
}

void UWBRuntimeActivationPresentationModelComponent::ClearCurrentActivationPresentationState()
{
	CurrentActivationActionSet = FWBCardActivationLegalActionSet();
	CurrentActivationPresentationSnapshot = FWBCardActivationLegalActionPresentationSnapshot();
	CurrentTargetPresentationSnapshot = FWBCardActivationTargetPresentationSnapshot();
	CurrentStatus = FWBRuntimeActivationPresentationStatus();
	bHasCurrentActivationPresentationState = false;
}

bool UWBRuntimeActivationPresentationModelComponent::HasCurrentActivationPresentationState() const
{
	return bHasCurrentActivationPresentationState;
}

const FWBCardActivationLegalActionSet&
UWBRuntimeActivationPresentationModelComponent::GetCurrentActivationActionSet() const
{
	return CurrentActivationActionSet;
}

const FWBCardActivationLegalActionPresentationSnapshot&
UWBRuntimeActivationPresentationModelComponent::GetCurrentActivationPresentationSnapshot() const
{
	return CurrentActivationPresentationSnapshot;
}

const FWBCardActivationTargetPresentationSnapshot&
UWBRuntimeActivationPresentationModelComponent::GetCurrentTargetPresentationSnapshot() const
{
	return CurrentTargetPresentationSnapshot;
}

FWBRuntimeActivationPresentationStatus
UWBRuntimeActivationPresentationModelComponent::GetCurrentActivationPresentationStatus() const
{
	return CurrentStatus;
}

bool UWBRuntimeActivationPresentationModelComponent::TryFindActivationPresentationEntryByActionId(
	const FString& ActivationActionId,
	FWBCardActivationLegalActionPresentationEntry& OutEntry) const
{
	return WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId(
		CurrentActivationPresentationSnapshot,
		ActivationActionId,
		OutEntry);
}

bool UWBRuntimeActivationPresentationModelComponent::TryFindTargetPresentationEntryByActionId(
	const FString& ActivationActionId,
	FWBCardActivationTargetPresentationEntry& OutEntry) const
{
	return WBCardActivationTargetPresentation::TryFindEntryByActivationActionId(
		CurrentTargetPresentationSnapshot,
		ActivationActionId,
		OutEntry);
}

FWBRuntimeActivationSelectionResolution
UWBRuntimeActivationPresentationModelComponent::ResolveSelectedActivationActionId(
	const FString& SelectedActivationActionId) const
{
	return WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
		this,
		SelectedActivationActionId);
}
