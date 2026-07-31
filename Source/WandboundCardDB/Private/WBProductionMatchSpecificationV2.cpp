#include "WBProductionMatchSpecification.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FWBProductionMatchSpecificationLoadResult Failure(
	const FString& Path,
	const FString& Reason)
{
	FWBProductionMatchSpecificationLoadResult Result;
	Result.MatchSpecPath = Path;
	Result.Reason = Reason;
	return Result;
}

bool ReadInteger(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	int32& Out)
{
	double Value = 0.0;
	if (!Object.IsValid()
		|| !Object->TryGetNumberField(Field, Value)
		|| !FMath::IsNearlyEqual(
			Value,
			static_cast<double>(static_cast<int32>(Value))))
	{
		return false;
	}
	Out = static_cast<int32>(Value);
	return true;
}

bool ReadTile(
	const TSharedPtr<FJsonObject>& Parent,
	const TCHAR* Field,
	FWBTile& Out)
{
	const TSharedPtr<FJsonValue>* Value =
		Parent.IsValid() ? Parent->Values.Find(Field) : nullptr;
	if (Value == nullptr || !Value->IsValid()
		|| (*Value)->Type != EJson::Object)
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Tile = (*Value)->AsObject();
	return Tile.IsValid() && Tile->Values.Num() == 2
		&& ReadInteger(Tile, TEXT("x"), Out.X)
		&& ReadInteger(Tile, TEXT("y"), Out.Y);
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

bool ReadStrings(
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

FWBCardInstanceRef CardInstance(
	const int32 PlayerId,
	const int32 Index,
	const FString& DefinitionId,
	const bool bHero)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = bHero
		? FString::Printf(
			TEXT("p%d_hero_%s"),
			PlayerId,
			*DefinitionId)
		: FString::Printf(
			TEXT("p%d_card_%03d_%s"),
			PlayerId,
			Index,
			*DefinitionId);
	Card.CardId = DefinitionId;
	Card.OwnerPlayerId = PlayerId;
	return Card;
}

int32 SeededFirstPlayer(const int32 Seed)
{
	uint32 State = static_cast<uint32>(Seed);
	if (State == 0)
	{
		State = 0x6d2b79f5u;
	}
	State = State * 1664525u + 1013904223u;
	return static_cast<int32>(State % 2u);
}
}

FWBProductionMatchSpecificationLoadResult
WBProductionMatchSpecification::LoadAndBuildRequestV2(
	const FString& MatchSpecPath,
	const FWBProductionCardDatabase& Database,
	const FWBActiveFormatV1& Format,
	const FWBGameStartAddendumV1& Addendum)
{
	const FString Resolved =
		WBProductionCardDatabase::ResolveInputPath(MatchSpecPath);
	FString Json;
	if (Resolved.IsEmpty()
		|| !FFileHelper::LoadFileToString(Json, *Resolved))
	{
		return Failure(
			MatchSpecPath,
			TEXT("production_match_spec_not_found"));
	}
	return ParseAndBuildRequestV2ForTest(
		Json,
		Resolved,
		Database,
		Format,
		Addendum);
}

FWBProductionMatchSpecificationLoadResult
WBProductionMatchSpecification::ParseAndBuildRequestV2ForTest(
	const FString& Json,
	const FString& MatchSpecPath,
	const FWBProductionCardDatabase& Database,
	const FWBActiveFormatV1& Format,
	const FWBGameStartAddendumV1& Addendum)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root)
		|| !Root.IsValid())
	{
		return Failure(
			MatchSpecPath,
			TEXT("production_match_spec_invalid"));
	}

	FWBProductionMatchSpecificationLoadResult Result;
	Result.MatchSpecPath = MatchSpecPath;
	FWBProductionMatchSpecification& Specification =
		Result.Specification;
	TSharedPtr<FJsonObject> FormatPin;
	TSharedPtr<FJsonObject> AddendumPin;
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Markers = nullptr;
	if (Root->Values.Num() != 10
		|| !ReadInteger(
			Root,
			TEXT("schema_version"),
			Specification.SchemaVersion)
		|| Specification.SchemaVersion != 2
		|| !Root->TryGetStringField(
			TEXT("match_id"),
			Specification.MatchId)
		|| !WBProductionCardDatabase::IsSafeDefinitionId(
			Specification.MatchId)
		|| !ReadInteger(Root, TEXT("seed"), Specification.Seed)
		|| Specification.Seed <= 0
		|| !ReadInteger(
			Root,
			TEXT("expected_first_player"),
			Specification.FirstPlayerId)
		|| Specification.FirstPlayerId
			!= SeededFirstPlayer(Specification.Seed)
		|| !ReadInteger(
			Root,
			TEXT("opening_hand_count"),
			Specification.InitialDrawCount)
		|| Specification.InitialDrawCount
			!= Format.OpeningHandCount
		|| !Root->TryGetStringField(
			TEXT("definition_bundle_digest"),
			Specification.DefinitionBundleDigest)
		|| Specification.DefinitionBundleDigest
			!= Database.ContentDigest
		|| !ReadObject(
			Root,
			TEXT("active_format"),
			FormatPin)
		|| !ReadObject(
			Root,
			TEXT("game_start_addendum"),
			AddendumPin)
		|| !Root->TryGetArrayField(TEXT("players"), Players)
		|| Players->Num() != 2
		|| !Root->TryGetArrayField(TEXT("markers"), Markers)
		|| Markers->Num() != 8)
	{
		return Failure(
			MatchSpecPath,
			TEXT("production_match_spec_invalid"));
	}

	if (!FormatPin.IsValid()
		|| FormatPin->Values.Num() != 3
		|| !FormatPin->TryGetStringField(
			TEXT("id"),
			Specification.ActiveFormatId)
		|| !ReadInteger(
			FormatPin,
			TEXT("version"),
			Specification.ActiveFormatVersion)
		|| !FormatPin->TryGetStringField(
			TEXT("digest"),
			Specification.ActiveFormatDigest)
		|| Specification.ActiveFormatId != Format.FormatId
		|| Specification.ActiveFormatVersion
			!= Format.FormatVersion
		|| Specification.ActiveFormatDigest != Format.Digest
		|| !AddendumPin.IsValid()
		|| AddendumPin->Values.Num() != 3
		|| !AddendumPin->TryGetStringField(
			TEXT("id"),
			Specification.GameStartAddendumId)
		|| !ReadInteger(
			AddendumPin,
			TEXT("version"),
			Specification.GameStartAddendumVersion)
		|| !AddendumPin->TryGetStringField(
			TEXT("digest"),
			Specification.GameStartAddendumDigest)
		|| Specification.GameStartAddendumId
			!= Addendum.AddendumId
		|| Specification.GameStartAddendumVersion
			!= Addendum.AddendumVersion
		|| Specification.GameStartAddendumDigest
			!= Addendum.Digest)
	{
		return Failure(
			MatchSpecPath,
			TEXT("production_match_rule_digest_mismatch"));
	}

	TArray<FWBActiveFormatPlayerInput> FormatPlayers;
	for (const TSharedPtr<FJsonValue>& Value : *Players)
	{
		const TSharedPtr<FJsonObject> Player =
			Value.IsValid() ? Value->AsObject() : nullptr;
		FWBProductionPlayerMatchSpecification Parsed;
		TSharedPtr<FJsonObject> SetupKit;
		if (!Player.IsValid()
			|| Player->Values.Num() != 5
			|| !ReadInteger(
				Player,
				TEXT("player_id"),
				Parsed.PlayerId)
			|| !FWBGameStateData::IsValidPlayerId(Parsed.PlayerId)
			|| !Player->TryGetStringField(
				TEXT("hero_definition_id"),
				Parsed.HeroDefinitionId)
			|| !ReadTile(
				Player,
				TEXT("hero_spawn"),
				Parsed.HeroSpawnTile)
			|| !ReadStrings(
				Player,
				TEXT("ordered_main_deck"),
				Parsed.OrderedDeckDefinitionIds)
			|| !ReadObject(
				Player,
				TEXT("setup_kit"),
				SetupKit)
			|| !SetupKit.IsValid()
			|| SetupKit->Values.Num() != 2
			|| !ReadStrings(
				SetupKit,
				TEXT("traps"),
				Parsed.SetupTrapDefinitionIds)
			|| !ReadStrings(
				SetupKit,
				TEXT("npcs"),
				Parsed.SetupNPCDefinitionIds))
		{
			return Failure(
				MatchSpecPath,
				TEXT("production_match_spec_invalid"));
		}

		const FWBTile ExpectedSpawn = Parsed.PlayerId == 0
			? FWBTile(4, 8)
			: FWBTile(4, 0);
		if (Parsed.HeroSpawnTile != ExpectedSpawn)
		{
			return Failure(
				MatchSpecPath,
				TEXT("hero_spawn_tile_invalid"));
		}

		FWBActiveFormatPlayerInput FormatPlayer;
		FormatPlayer.PlayerId = Parsed.PlayerId;
		FormatPlayer.MainDeckDefinitionIds =
			Parsed.OrderedDeckDefinitionIds;
		FormatPlayer.HeroDefinitionId =
			Parsed.HeroDefinitionId;
		FormatPlayer.SetupTrapDefinitionIds =
			Parsed.SetupTrapDefinitionIds;
		FormatPlayer.SetupNPCDefinitionIds =
			Parsed.SetupNPCDefinitionIds;
		FormatPlayers.Add(MoveTemp(FormatPlayer));
		Specification.Players.Add(MoveTemp(Parsed));
	}
	Specification.Players.Sort([](
		const FWBProductionPlayerMatchSpecification& A,
		const FWBProductionPlayerMatchSpecification& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	if (Specification.Players[0].PlayerId != 0
		|| Specification.Players[1].PlayerId != 1)
	{
		return Failure(
			MatchSpecPath,
			TEXT("production_match_spec_invalid"));
	}

	const FWBActiveFormatValidationResult FormatResult =
		WBActiveFormat::ValidateMatchForLaunch(
			Format,
			Database,
			FormatPlayers);
	if (!FormatResult.bOk)
	{
		return Failure(MatchSpecPath, FormatResult.Reason);
	}

	for (int32 Index = 0; Index < Markers->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Marker =
			(*Markers)[Index].IsValid()
				? (*Markers)[Index]->AsObject()
				: nullptr;
		FWBSetupMarkerPlacement Placement;
		FString Type;
		if (!Marker.IsValid()
			|| Marker->Values.Num() != 5
			|| !ReadInteger(
				Marker,
				TEXT("player_id"),
				Placement.PlayerId)
			|| !Marker->TryGetStringField(TEXT("type"), Type)
			|| !ReadTile(Marker, TEXT("tile"), Placement.Tile)
			|| !Marker->TryGetStringField(
				TEXT("definition_id"),
				Placement.DefinitionId)
			|| !ReadInteger(
				Marker,
				TEXT("placement_order"),
				Placement.PlacementOrder)
			|| Placement.PlacementOrder != Index
			|| (Type != TEXT("trap") && Type != TEXT("npc")))
		{
			return Failure(
				MatchSpecPath,
				TEXT("production_match_spec_invalid"));
		}
		Placement.Type = Type == TEXT("trap")
			? EWBMarkerType::Trap
			: EWBMarkerType::NPC;
		const int32 ExpectedOwner = Index < 4
			? Specification.FirstPlayerId
			: 1 - Specification.FirstPlayerId;
		if (Placement.PlayerId != ExpectedOwner)
		{
			return Failure(
				MatchSpecPath,
				TEXT("marker_setup_order_invalid"));
		}
		const FWBProductionPlayerMatchSpecification& Owner =
			Specification.Players[Placement.PlayerId];
		const TArray<FString>& Allowed =
			Placement.Type == EWBMarkerType::Trap
				? Owner.SetupTrapDefinitionIds
				: Owner.SetupNPCDefinitionIds;
		if (!Allowed.Contains(Placement.DefinitionId))
		{
			return Failure(
				MatchSpecPath,
				TEXT("marker_setup_definition_invalid"));
		}
		Specification.MarkerPlacements.Add(Placement);
	}

	FWBMatchInitializationRequest& Request =
		Result.InitializationRequest;
	Request.Seed = Specification.Seed;
	Request.FirstPlayerId = INDEX_NONE;
	Request.ExpectedFirstPlayerId =
		Specification.FirstPlayerId;
	Request.bDeriveFirstPlayerFromSeed = true;
	Request.bShuffleDecksAtMatchStart = true;
	Request.Repository = Database.CoreRepository;
	Request.MarkerPlacements = Specification.MarkerPlacements;
	for (const FWBProductionPlayerMatchSpecification& Player :
		Specification.Players)
	{
		FWBMatchPlayerSetup Setup;
		Setup.PlayerId = Player.PlayerId;
		Setup.HeroCardId = Player.HeroDefinitionId;
		Setup.HeroSpawnTile = Player.HeroSpawnTile;
		for (int32 Index = 0;
			Index < Player.OrderedDeckDefinitionIds.Num();
			++Index)
		{
			const FString& DefinitionId =
				Player.OrderedDeckDefinitionIds[Index];
			const bool bHero =
				DefinitionId == Player.HeroDefinitionId;
			const FWBCardInstanceRef Card = CardInstance(
				Player.PlayerId,
				Index,
				DefinitionId,
				bHero);
			if (bHero)
			{
				Setup.HeroInstanceId = Card.InstanceId;
			}
			Setup.OrderedDeck.Add(Card);
		}
		Request.Players.Add(MoveTemp(Setup));
	}

	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}
