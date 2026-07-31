#include "WBActiveFormat.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <openssl/sha.h>

namespace
{
bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& Out)
{
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
}

bool ReadInt(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	int32& Out)
{
	double Value = 0.0;
	if (!Object.IsValid()
		|| !Object->TryGetNumberField(Field, Value)
		|| !FMath::IsNearlyEqual(Value, static_cast<double>(
			static_cast<int32>(Value))))
	{
		return false;
	}
	Out = static_cast<int32>(Value);
	return true;
}

bool ReadString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	FString& Out)
{
	return Object.IsValid()
		&& Object->TryGetStringField(Field, Out)
		&& !Out.IsEmpty();
}

bool ReadBool(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	bool& Out)
{
	return Object.IsValid() && Object->TryGetBoolField(Field, Out);
}

bool ReadStringArray(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	TArray<FString>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid()
		|| !Object->TryGetArrayField(Field, Values))
	{
		return false;
	}
	Out.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString Item;
		if (!Value.IsValid() || !Value->TryGetString(Item)
			|| Item.IsEmpty())
		{
			return false;
		}
		Out.Add(Item);
	}
	return true;
}

bool ReadObject(
	const TSharedPtr<FJsonObject>& Parent,
	const TCHAR* Field,
	TSharedPtr<FJsonObject>& Out)
{
	const TSharedPtr<FJsonValue>* Value =
		Parent.IsValid() ? Parent->Values.Find(Field) : nullptr;
	if (Value == nullptr || !Value->IsValid()
		|| (*Value)->Type != EJson::Object)
	{
		Out.Reset();
		return false;
	}
	Out = (*Value)->AsObject();
	return Out.IsValid();
}

bool HasExactFields(
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& Fields)
{
	if (!Object.IsValid() || Object->Values.Num() != Fields.Num())
	{
		return false;
	}
	for (const FString& Field : Fields)
	{
		if (!Object->HasField(Field))
		{
			return false;
		}
	}
	return true;
}

FString SHA256String(const FString& Value)
{
	const FTCHARToUTF8 Utf8(*Value);
	uint8 Bytes[SHA256_DIGEST_LENGTH] = {};
	if (SHA256(
		reinterpret_cast<const uint8*>(Utf8.Get()),
		static_cast<size_t>(Utf8.Length()),
		Bytes) == nullptr)
	{
		return FString();
	}
	FString Result;
	for (const uint8 Byte : Bytes)
	{
		Result += FString::Printf(TEXT("%02x"), Byte);
	}
	return Result;
}

FWBActiveFormatValidationResult ValidationFailure(
	const FString& Reason)
{
	FWBActiveFormatValidationResult Result;
	Result.Reason = Reason;
	Result.Diagnostics.Add(Reason);
	return Result;
}

bool IsHybrid(const FWBProductionCardRecord& Record)
{
	return Record.CoreDefinition.PublicCategory.Equals(
			TEXT("Hybrid"),
			ESearchCase::IgnoreCase)
		|| Record.CoreDefinition.PublicTags.Contains(TEXT("hybrid"));
}
}

FWBActiveFormatLoadResult WBActiveFormat::Load(const FString& Path)
{
	FWBActiveFormatLoadResult Result;
	const FString Resolved =
		WBProductionCardDatabase::ResolveInputPath(Path);
	FString Json;
	if (Resolved.IsEmpty()
		|| !FFileHelper::LoadFileToString(Json, *Resolved))
	{
		Result.SourcePath = Path;
		Result.Reason = TEXT("active_format_not_found");
		return Result;
	}
	return ParseForTest(Json, Resolved);
}

FWBActiveFormatLoadResult WBActiveFormat::ParseForTest(
	const FString& Json,
	const FString& SourcePath)
{
	FWBActiveFormatLoadResult Result;
	Result.SourcePath = SourcePath;
	TSharedPtr<FJsonObject> Root;
	if (!ParseObject(Json, Root)
		|| !HasExactFields(
			Root,
			{
				TEXT("schema_version"),
				TEXT("format_id"),
				TEXT("format_version"),
				TEXT("authority"),
				TEXT("approval_date"),
				TEXT("format_digest"),
				TEXT("source_provenance"),
				TEXT("main_deck"),
				TEXT("setup_kit"),
				TEXT("opening"),
				TEXT("randomization"),
				TEXT("turn_one"),
				TEXT("game_start_addendum")
			}))
	{
		Result.Reason = TEXT("active_format_schema_invalid");
		return Result;
	}

	TSharedPtr<FJsonObject> MainDeck;
	TSharedPtr<FJsonObject> SetupKit;
	TSharedPtr<FJsonObject> Opening;
	TSharedPtr<FJsonObject> Randomization;
	TSharedPtr<FJsonObject> TurnOne;
	TSharedPtr<FJsonObject> GameStartAddendum;
	FWBActiveFormatV1& Format = Result.Format;
	if (!ReadInt(Root, TEXT("schema_version"), Format.SchemaVersion)
		|| Format.SchemaVersion != 1
		|| !ReadString(Root, TEXT("format_id"), Format.FormatId)
		|| Format.FormatId != TEXT("active_format_v1")
		|| !ReadInt(Root, TEXT("format_version"), Format.FormatVersion)
		|| Format.FormatVersion != 1
		|| !ReadString(Root, TEXT("authority"), Format.Authority)
		|| Format.Authority != TEXT("product_owner_approved")
		|| !ReadString(Root, TEXT("approval_date"), Format.ApprovalDate)
		|| Format.ApprovalDate != TEXT("2026-07-30")
		|| !ReadString(Root, TEXT("format_digest"), Format.Digest)
		|| !ReadStringArray(
			Root,
			TEXT("source_provenance"),
			Format.SourceProvenance)
		|| Format.SourceProvenance.IsEmpty()
		|| !ReadObject(Root, TEXT("main_deck"), MainDeck)
		|| !ReadObject(Root, TEXT("setup_kit"), SetupKit)
		|| !ReadObject(Root, TEXT("opening"), Opening)
		|| !ReadObject(Root, TEXT("randomization"), Randomization)
		|| !ReadObject(Root, TEXT("turn_one"), TurnOne)
		|| !ReadObject(
			Root,
			TEXT("game_start_addendum"),
			GameStartAddendum)
		|| !HasExactFields(
			MainDeck,
			{
				TEXT("minimum_size"),
				TEXT("maximum_size"),
				TEXT("unique_definition_ids"),
				TEXT("requires_non_hybrid_character"),
				TEXT("legal_categories"),
				TEXT("excluded_categories")
			})
		|| !HasExactFields(
			SetupKit,
			{
				TEXT("trap_count"),
				TEXT("npc_count"),
				TEXT("definition_ids_may_repeat")
			})
		|| !HasExactFields(
			Opening,
			{
				TEXT("hand_count"),
				TEXT("mirrored_decks_allowed")
			})
		|| !HasExactFields(
			Randomization,
			{
				TEXT("seeded_shuffle"),
				TEXT("seeded_first_player_coin_flip"),
				TEXT("first_player_policy")
			})
		|| !HasExactFields(
			TurnOne,
			{
				TEXT("first_player_only"),
				TEXT("neutral_row_summons_allowed"),
				TEXT("opponent_half_summons_allowed"),
				TEXT("protected_region_boundary_crossing_allowed"),
				TEXT("neutral_npc_attacks_allowed"),
				TEXT("opponent_controlled_attacks_allowed")
			})
		|| !HasExactFields(
			GameStartAddendum,
			{
				TEXT("id"),
				TEXT("version")
			})
		|| !ReadInt(
			MainDeck,
			TEXT("minimum_size"),
			Format.MinimumMainDeckSize)
		|| !ReadInt(
			MainDeck,
			TEXT("maximum_size"),
			Format.MaximumMainDeckSize)
		|| !ReadBool(
			MainDeck,
			TEXT("unique_definition_ids"),
			Format.bUniqueMainDeckDefinitions)
		|| !ReadBool(
			MainDeck,
			TEXT("requires_non_hybrid_character"),
			Format.bRequiresNonHybridCharacter)
		|| !ReadStringArray(
			MainDeck,
			TEXT("legal_categories"),
			Format.LegalMainDeckCategories)
		|| !ReadStringArray(
			MainDeck,
			TEXT("excluded_categories"),
			Format.ExcludedMainDeckCategories)
		|| !ReadInt(
			SetupKit,
			TEXT("trap_count"),
			Format.SetupTrapCount)
		|| !ReadInt(
			SetupKit,
			TEXT("npc_count"),
			Format.SetupNPCCount)
		|| !ReadBool(
			SetupKit,
			TEXT("definition_ids_may_repeat"),
			Format.bSetupDefinitionsMayRepeat)
		|| !ReadInt(
			Opening,
			TEXT("hand_count"),
			Format.OpeningHandCount)
		|| !ReadBool(
			Opening,
			TEXT("mirrored_decks_allowed"),
			Format.bMirroredDecksAllowed)
		|| !ReadBool(
			Randomization,
			TEXT("seeded_shuffle"),
			Format.bSeededShuffleRequired)
		|| !ReadBool(
			Randomization,
			TEXT("seeded_first_player_coin_flip"),
			Format.bSeededFirstPlayerCoinFlipRequired)
		|| !ReadString(
			Randomization,
			TEXT("first_player_policy"),
			Format.FirstPlayerPolicy)
		|| !ReadBool(
			TurnOne,
			TEXT("first_player_only"),
			Format.bFirstPlayerTurnOneRestriction)
		|| !ReadBool(
			TurnOne,
			TEXT("neutral_row_summons_allowed"),
			Format.bNeutralRowSummonsAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("opponent_half_summons_allowed"),
			Format.bOpponentHalfSummonsAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("protected_region_boundary_crossing_allowed"),
			Format.bProtectedRegionBoundaryCrossingAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("neutral_npc_attacks_allowed"),
			Format.bNeutralNPCAttacksAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("opponent_controlled_attacks_allowed"),
			Format.bOpponentControlledAttacksAllowed)
		|| !ReadString(
			GameStartAddendum,
			TEXT("id"),
			Format.GameStartAddendumId)
		|| !ReadInt(
			GameStartAddendum,
			TEXT("version"),
			Format.GameStartAddendumVersion)
		|| Format.MinimumMainDeckSize != 1
		|| Format.MaximumMainDeckSize != 30
		|| !Format.bUniqueMainDeckDefinitions
		|| !Format.bRequiresNonHybridCharacter
		|| Format.LegalMainDeckCategories
			!= TArray<FString>({
				TEXT("character"),
				TEXT("wand"),
				TEXT("action")
			})
		|| Format.ExcludedMainDeckCategories
			!= TArray<FString>({ TEXT("trap"), TEXT("npc") })
		|| Format.SetupTrapCount != 2
		|| Format.SetupNPCCount != 2
		|| !Format.bSetupDefinitionsMayRepeat
		|| Format.OpeningHandCount != 6
		|| !Format.bMirroredDecksAllowed
		|| !Format.bSeededShuffleRequired
		|| !Format.bSeededFirstPlayerCoinFlipRequired
		|| Format.FirstPlayerPolicy
			!= TEXT("seeded_coin_flip")
		|| !Format.bFirstPlayerTurnOneRestriction
		|| !Format.bNeutralRowSummonsAllowed
		|| Format.bOpponentHalfSummonsAllowed
		|| Format.bProtectedRegionBoundaryCrossingAllowed
		|| !Format.bNeutralNPCAttacksAllowed
		|| Format.bOpponentControlledAttacksAllowed
		|| Format.GameStartAddendumId
			!= TEXT("game_start_turn_one_v1")
		|| Format.GameStartAddendumVersion != 1)
	{
		Result.Reason = TEXT("active_format_schema_invalid");
		return Result;
	}

	if (ComputeDigest(Format) != Format.Digest)
	{
		Result.Reason = TEXT("active_format_digest_mismatch");
		return Result;
	}
	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}

FWBActiveFormatValidationResult
WBActiveFormat::ValidateStoredMainDeck(
	const FWBActiveFormatV1& Format,
	const FWBProductionCardDatabase& Database,
	const TArray<FString>& MainDeckDefinitionIds)
{
	if (MainDeckDefinitionIds.Num() < Format.MinimumMainDeckSize)
	{
		return ValidationFailure(
			TEXT("active_format_main_deck_too_small"));
	}
	if (MainDeckDefinitionIds.Num() > Format.MaximumMainDeckSize)
	{
		return ValidationFailure(
			TEXT("active_format_main_deck_too_large"));
	}

	TSet<FString> Seen;
	bool bHasEligibleCharacter = false;
	for (const FString& DefinitionId : MainDeckDefinitionIds)
	{
		if (Seen.Contains(DefinitionId))
		{
			return ValidationFailure(
				TEXT("active_format_duplicate_main_deck_definition"));
		}
		Seen.Add(DefinitionId);
		const FWBProductionCardRecord* Record =
			Database.FindRecord(DefinitionId);
		if (Record == nullptr)
		{
			return ValidationFailure(
				TEXT("active_format_unknown_main_deck_definition"));
		}
		if (!WBCardDefinitionRepository::ContainsCardId(
			Database.CoreRepository,
			DefinitionId))
		{
			return ValidationFailure(
				TEXT("active_format_unsupported_main_deck_definition"));
		}
		if (Record->Type == EWBProductionCardType::Trap)
		{
			return ValidationFailure(
				TEXT("active_format_trap_in_main_deck"));
		}
		if (Record->Type == EWBProductionCardType::NPC)
		{
			return ValidationFailure(
				TEXT("active_format_npc_in_main_deck"));
		}
		if (Record->Type == EWBProductionCardType::Unknown
			|| Record->Type == EWBProductionCardType::Hero)
		{
			return ValidationFailure(
				TEXT("active_format_illegal_main_deck_category"));
		}
		if (Record->Type == EWBProductionCardType::Character
			&& !IsHybrid(*Record))
		{
			bHasEligibleCharacter = true;
		}
	}
	if (!bHasEligibleCharacter)
	{
		return ValidationFailure(
			TEXT("active_format_missing_non_hybrid_character"));
	}

	FWBActiveFormatValidationResult Result;
	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}

FWBActiveFormatValidationResult
WBActiveFormat::ValidatePlayerForLaunch(
	const FWBActiveFormatV1& Format,
	const FWBProductionCardDatabase& Database,
	const FWBActiveFormatPlayerInput& Player)
{
	FWBActiveFormatValidationResult Result =
		ValidateStoredMainDeck(
			Format,
			Database,
			Player.MainDeckDefinitionIds);
	if (!Result.bOk)
	{
		return Result;
	}

	int32 HeroCount = 0;
	for (const FString& DefinitionId :
		Player.MainDeckDefinitionIds)
	{
		HeroCount += DefinitionId == Player.HeroDefinitionId ? 1 : 0;
	}
	if (HeroCount != 1)
	{
		return ValidationFailure(
			TEXT("active_format_hero_not_in_deck"));
	}
	const FWBProductionCardRecord* Hero =
		Database.FindRecord(Player.HeroDefinitionId);
	if (Hero == nullptr
		|| Hero->Type != EWBProductionCardType::Character)
	{
		return ValidationFailure(
			TEXT("active_format_hero_not_character"));
	}
	if (IsHybrid(*Hero))
	{
		return ValidationFailure(
			TEXT("active_format_hybrid_cannot_be_initial_hero"));
	}
	if (!Database.HeroCandidateDefinitionIds.Contains(
		Player.HeroDefinitionId))
	{
		return ValidationFailure(
			TEXT("active_format_hero_evidence_missing"));
	}
	if (!WBCardDefinitionRepository::ContainsCardId(
		Database.CoreRepository,
		Player.HeroDefinitionId))
	{
		return ValidationFailure(
			TEXT("active_format_hero_behavior_unsupported"));
	}
	if (Player.MainDeckDefinitionIds.Num() - 1
		< Format.OpeningHandCount)
	{
		return ValidationFailure(
			TEXT("active_format_insufficient_opening_hand_capacity"));
	}

	if (Player.SetupTrapDefinitionIds.Num()
		!= Format.SetupTrapCount)
	{
		return ValidationFailure(
			TEXT("active_format_setup_trap_count_invalid"));
	}
	if (Player.SetupNPCDefinitionIds.Num()
		!= Format.SetupNPCCount)
	{
		return ValidationFailure(
			TEXT("active_format_setup_npc_count_invalid"));
	}
	for (const FString& DefinitionId :
		Player.SetupTrapDefinitionIds)
	{
		const FWBProductionCardRecord* Record =
			Database.FindRecord(DefinitionId);
		if (Record == nullptr
			|| Record->Type != EWBProductionCardType::Trap)
		{
			return ValidationFailure(
				TEXT("active_format_setup_trap_definition_invalid"));
		}
		if (!WBCardDefinitionRepository::ContainsCardId(
			Database.CoreRepository,
			DefinitionId))
		{
			return ValidationFailure(
				TEXT("active_format_setup_definition_unsupported"));
		}
	}
	for (const FString& DefinitionId :
		Player.SetupNPCDefinitionIds)
	{
		const FWBProductionCardRecord* Record =
			Database.FindRecord(DefinitionId);
		if (Record == nullptr
			|| Record->Type != EWBProductionCardType::NPC)
		{
			return ValidationFailure(
				TEXT("active_format_setup_npc_definition_invalid"));
		}
		if (!WBCardDefinitionRepository::ContainsCardId(
			Database.CoreRepository,
			DefinitionId))
		{
			return ValidationFailure(
				TEXT("active_format_setup_definition_unsupported"));
		}
	}

	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}

FWBActiveFormatValidationResult
WBActiveFormat::ValidateMatchForLaunch(
	const FWBActiveFormatV1& Format,
	const FWBProductionCardDatabase& Database,
	const TArray<FWBActiveFormatPlayerInput>& Players)
{
	if (Players.Num() != 2)
	{
		return ValidationFailure(TEXT("active_format_players_invalid"));
	}
	TSet<int32> PlayerIds;
	for (const FWBActiveFormatPlayerInput& Player : Players)
	{
		if (!FWBGameStateData::IsValidPlayerId(Player.PlayerId)
			|| PlayerIds.Contains(Player.PlayerId))
		{
			return ValidationFailure(
				TEXT("active_format_players_invalid"));
		}
		PlayerIds.Add(Player.PlayerId);
		FWBActiveFormatValidationResult Result =
			ValidatePlayerForLaunch(Format, Database, Player);
		if (!Result.bOk)
		{
			return Result;
		}
	}
	FWBActiveFormatValidationResult Result;
	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}

FString WBActiveFormat::ComputeDigest(
	const FWBActiveFormatV1& Format)
{
	TArray<FString> Provenance = Format.SourceProvenance;
	Provenance.Sort();
	const FString LegalCategories =
		FString::Join(Format.LegalMainDeckCategories, TEXT(","));
	const FString ExcludedCategories =
		FString::Join(Format.ExcludedMainDeckCategories, TEXT(","));
	const FString Source = FString::Printf(
		TEXT("%s|%d|%s|%s|%d|%d|%d|%d|%s|%s|%d|%d|%d|%d|%d|%d|%d|%s|%d|%d|%d|%d|%d|%d|%s|%d|%s"),
		*Format.FormatId,
		Format.FormatVersion,
		*Format.Authority,
		*Format.ApprovalDate,
		Format.MinimumMainDeckSize,
		Format.MaximumMainDeckSize,
		Format.bUniqueMainDeckDefinitions ? 1 : 0,
		Format.bRequiresNonHybridCharacter ? 1 : 0,
		*LegalCategories,
		*ExcludedCategories,
		Format.SetupTrapCount,
		Format.SetupNPCCount,
		Format.bSetupDefinitionsMayRepeat ? 1 : 0,
		Format.OpeningHandCount,
		Format.bMirroredDecksAllowed ? 1 : 0,
		Format.bSeededShuffleRequired ? 1 : 0,
		Format.bSeededFirstPlayerCoinFlipRequired ? 1 : 0,
		*Format.FirstPlayerPolicy,
		Format.bFirstPlayerTurnOneRestriction ? 1 : 0,
		Format.bNeutralRowSummonsAllowed ? 1 : 0,
		Format.bOpponentHalfSummonsAllowed ? 1 : 0,
		Format.bProtectedRegionBoundaryCrossingAllowed ? 1 : 0,
		Format.bNeutralNPCAttacksAllowed ? 1 : 0,
		Format.bOpponentControlledAttacksAllowed ? 1 : 0,
		*Format.GameStartAddendumId,
		Format.GameStartAddendumVersion,
		*FString::Join(Provenance, TEXT(",")));
	return SHA256String(Source);
}
