#include "WBProductionCardDataProvider.h"

namespace
{
FWBProductionCardDataLookupResult MakeFailure(const TCHAR* Reason)
{
	FWBProductionCardDataLookupResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBProductionCardDataLookupResult MakeLookup(
	const TSharedPtr<const FWBProductionCardDatabase>& Snapshot,
	const FString& DefinitionId,
	const EWBProductionCardType RequiredType,
	const bool bRequireEffects)
{
	if (!Snapshot.IsValid())
	{
		return MakeFailure(TEXT("card_database_not_configured"));
	}

	const FWBProductionCardRecord* Record = Snapshot->FindRecord(DefinitionId);
	if (Record == nullptr)
	{
		return MakeFailure(TEXT("card_definition_not_found"));
	}

	if (RequiredType != EWBProductionCardType::Unknown
		&& Record->Type != RequiredType)
	{
		return MakeFailure(TEXT("card_definition_type_mismatch"));
	}

	if (bRequireEffects && Record->CoreDefinition.ActivatedEffects.IsEmpty())
	{
		return MakeFailure(TEXT("activation_definition_not_found"));
	}

	FWBProductionCardDataLookupResult Result;
	Result.bOk = true;
	Result.Reason = TEXT("success");
	Result.Record = *Record;
	return Result;
}
}

void FWBProductionCardDataProvider::Configure(
	TSharedPtr<const FWBProductionCardDatabase> InSnapshot)
{
	Snapshot = MoveTemp(InSnapshot);
}

bool FWBProductionCardDataProvider::IsConfigured() const
{
	return Snapshot.IsValid();
}

FString FWBProductionCardDataProvider::GetContentDigest() const
{
	return Snapshot.IsValid() ? Snapshot->ContentDigest : FString();
}

TSharedPtr<const FWBProductionCardDatabase>
FWBProductionCardDataProvider::GetSnapshot() const
{
	return Snapshot;
}

FWBProductionCardDataLookupResult
FWBProductionCardDataProvider::GetCharacterDefinition(
	const FString& DefinitionId) const
{
	return MakeLookup(
		Snapshot,
		DefinitionId,
		EWBProductionCardType::Character,
		false);
}

FWBProductionCardDataLookupResult FWBProductionCardDataProvider::GetHeroDefinition(
	const FString& DefinitionId) const
{
	return MakeLookup(
		Snapshot,
		DefinitionId,
		EWBProductionCardType::Hero,
		false);
}

FWBProductionCardDataLookupResult FWBProductionCardDataProvider::GetWandDefinition(
	const FString& DefinitionId) const
{
	return MakeLookup(
		Snapshot,
		DefinitionId,
		EWBProductionCardType::Wand,
		false);
}

FWBProductionCardDataLookupResult FWBProductionCardDataProvider::GetSummonData(
	const FString& DefinitionId) const
{
	return GetCharacterDefinition(DefinitionId);
}

FWBProductionCardDataLookupResult FWBProductionCardDataProvider::GetEquipData(
	const FString& DefinitionId) const
{
	return GetWandDefinition(DefinitionId);
}

FWBProductionCardDataLookupResult FWBProductionCardDataProvider::GetActivationData(
	const FString& DefinitionId) const
{
	return MakeLookup(
		Snapshot,
		DefinitionId,
		EWBProductionCardType::Unknown,
		true);
}

bool FWBProductionCardDataProvider::GetPublicPresentationData(
	const FString& DefinitionId,
	FWBProductionPublicCardData& OutData,
	FString& OutReason) const
{
	OutData = FWBProductionPublicCardData();
	const FWBProductionCardDataLookupResult Lookup =
		MakeLookup(
			Snapshot,
			DefinitionId,
			EWBProductionCardType::Unknown,
			false);
	if (!Lookup.bOk)
	{
		OutReason = Lookup.Reason;
		return false;
	}

	OutData.DefinitionId = Lookup.Record.CoreDefinition.CardId;
	OutData.DisplayName = Lookup.Record.CoreDefinition.PublicName;
	OutData.CardType = WBProductionCardDatabase::CardTypeToString(Lookup.Record.Type);
	OutData.PublicCategory = Lookup.Record.CoreDefinition.PublicCategory;
	OutData.Factions = Lookup.Record.CoreDefinition.PublicFactions;
	OutData.Tags = Lookup.Record.CoreDefinition.PublicTags;
	OutData.PublicRulesText = Lookup.Record.CoreDefinition.PublicRulesText;
	OutReason = TEXT("success");
	return true;
}
