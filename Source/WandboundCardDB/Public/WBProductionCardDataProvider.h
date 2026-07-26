#pragma once

#include "CoreMinimal.h"
#include "WBProductionCardDatabase.h"

struct WANDBOUNDCARDDB_API FWBProductionPublicCardData
{
	FString DefinitionId;
	FString DisplayName;
	FString CardType;
	FString PublicCategory;
	TArray<FString> Factions;
	TArray<FString> Tags;
	FString PublicRulesText;
};

struct WANDBOUNDCARDDB_API FWBProductionCardDataLookupResult
{
	bool bOk = false;
	FString Reason;
	FWBProductionCardRecord Record;
};

class WANDBOUNDCARDDB_API FWBProductionCardDataProvider
{
public:
	void Configure(TSharedPtr<const FWBProductionCardDatabase> InSnapshot);
	bool IsConfigured() const;
	FString GetContentDigest() const;
	TSharedPtr<const FWBProductionCardDatabase> GetSnapshot() const;

	FWBProductionCardDataLookupResult GetCharacterDefinition(
		const FString& DefinitionId) const;
	FWBProductionCardDataLookupResult GetHeroDefinition(
		const FString& DefinitionId) const;
	FWBProductionCardDataLookupResult GetWandDefinition(
		const FString& DefinitionId) const;
	FWBProductionCardDataLookupResult GetSummonData(
		const FString& DefinitionId) const;
	FWBProductionCardDataLookupResult GetEquipData(
		const FString& DefinitionId) const;
	FWBProductionCardDataLookupResult GetActivationData(
		const FString& DefinitionId) const;
	bool GetPublicPresentationData(
		const FString& DefinitionId,
		FWBProductionPublicCardData& OutData,
		FString& OutReason) const;

private:
	TSharedPtr<const FWBProductionCardDatabase> Snapshot;
};
