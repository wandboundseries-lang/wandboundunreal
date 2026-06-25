#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WBCardActivationLegalAction.h"
#include "WBCardActivationTargetPresentation.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeActivationSelectionResolver.h"
#include "WBRuntimeActivationPresentationModelComponent.generated.h"

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationPresentationStatus
{
	bool bReady = false;
	bool bHasActivationActions = false;
	int32 ActivationActionCount = 0;
	int32 ActivationPresentationEntryCount = 0;
	int32 TargetPresentationEntryCount = 0;
	FString LastRefreshReason;
};

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimeActivationPresentationModelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWBRuntimeActivationPresentationModelComponent();

	void SetCurrentActivationPresentationState(
		const FWBCardActivationLegalActionSet& ActivationActionSet,
		const FWBPublicBoardSummary& PublicBoardSummary);

	void ClearCurrentActivationPresentationState();

	bool HasCurrentActivationPresentationState() const;

	const FWBCardActivationLegalActionSet& GetCurrentActivationActionSet() const;
	const FWBCardActivationLegalActionPresentationSnapshot& GetCurrentActivationPresentationSnapshot() const;
	const FWBCardActivationTargetPresentationSnapshot& GetCurrentTargetPresentationSnapshot() const;
	FWBRuntimeActivationPresentationStatus GetCurrentActivationPresentationStatus() const;

	bool TryFindActivationPresentationEntryByActionId(
		const FString& ActivationActionId,
		FWBCardActivationLegalActionPresentationEntry& OutEntry) const;

	bool TryFindTargetPresentationEntryByActionId(
		const FString& ActivationActionId,
		FWBCardActivationTargetPresentationEntry& OutEntry) const;

	FWBRuntimeActivationSelectionResolution ResolveSelectedActivationActionId(
		const FString& SelectedActivationActionId) const;

private:
	FWBCardActivationLegalActionSet CurrentActivationActionSet;
	FWBCardActivationLegalActionPresentationSnapshot CurrentActivationPresentationSnapshot;
	FWBCardActivationTargetPresentationSnapshot CurrentTargetPresentationSnapshot;
	FWBRuntimeActivationPresentationStatus CurrentStatus;
	bool bHasCurrentActivationPresentationState = false;
};
