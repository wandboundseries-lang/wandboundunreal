#include "WBProductionMatchSpecification.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 SupportedMatchSchemaVersion = 1;
constexpr int32 SupportedInitialDrawCount = 6;

bool ParseJsonObject(
	const FString& Json,
	TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutObject)
		&& OutObject.IsValid();
}

bool TryReadInteger(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	int32& OutValue)
{
	const TSharedPtr<FJsonValue>* Value =
		Object.IsValid() ? Object->Values.Find(Field) : nullptr;
	if (Value == nullptr
		|| !Value->IsValid()
		|| (*Value)->Type != EJson::Number)
	{
		return false;
	}
	double Number = 0.0;
	if (!(*Value)->TryGetNumber(Number)
		|| Number < static_cast<double>(TNumericLimits<int32>::Min())
		|| Number > static_cast<double>(TNumericLimits<int32>::Max()))
	{
		return false;
	}
	const int32 Integer = static_cast<int32>(Number);
	if (!FMath::IsNearlyEqual(Number, static_cast<double>(Integer)))
	{
		return false;
	}
	OutValue = Integer;
	return true;
}

bool TryReadString(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	FString& OutValue)
{
	OutValue.Reset();
	const TSharedPtr<FJsonValue>* Value =
		Object.IsValid() ? Object->Values.Find(Field) : nullptr;
	if (Value == nullptr
		|| !Value->IsValid()
		|| (*Value)->Type != EJson::String)
	{
		return false;
	}
	OutValue = (*Value)->AsString();
	return true;
}

bool TryReadArray(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	const TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
	OutArray = nullptr;
	const TSharedPtr<FJsonValue>* Value =
		Object.IsValid() ? Object->Values.Find(Field) : nullptr;
	if (Value == nullptr
		|| !Value->IsValid()
		|| (*Value)->Type != EJson::Array)
	{
		return false;
	}
	OutArray = &(*Value)->AsArray();
	return true;
}

bool TryReadObject(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();
	const TSharedPtr<FJsonValue>* Value =
		Object.IsValid() ? Object->Values.Find(Field) : nullptr;
	if (Value == nullptr
		|| !Value->IsValid()
		|| (*Value)->Type != EJson::Object)
	{
		return false;
	}
	OutObject = (*Value)->AsObject();
	return OutObject.IsValid();
}

void AddDiagnostic(
	FWBProductionMatchSpecificationLoadResult& Result,
	const FString& Code,
	const FString& DefinitionId,
	const FString& FieldPath,
	const FString& Message,
	const FString& RecommendedAction = FString())
{
	FWBProductionCardDBDiagnostic Diagnostic;
	Diagnostic.Severity = EWBProductionCardDBDiagnosticSeverity::Error;
	Diagnostic.Code = Code;
	Diagnostic.ManifestPath = Result.MatchSpecPath;
	Diagnostic.DefinitionId = DefinitionId;
	Diagnostic.FieldPath = FieldPath;
	Diagnostic.Message = Message;
	Diagnostic.RecommendedAction = RecommendedAction;
	Result.Diagnostics.Add(MoveTemp(Diagnostic));
}

void ValidateKnownFields(
	FWBProductionMatchSpecificationLoadResult& Result,
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& KnownFields,
	const FString& FieldPath)
{
	if (!Object.IsValid())
	{
		return;
	}
	TArray<FString> UnknownFields;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!KnownFields.Contains(Pair.Key))
		{
			UnknownFields.Add(Pair.Key);
		}
	}
	UnknownFields.Sort();
	for (const FString& UnknownField : UnknownFields)
	{
		AddDiagnostic(
			Result,
			TEXT("unknown_field"),
			FString(),
			FieldPath + TEXT(".") + UnknownField,
			TEXT("The production match specification is a closed schema."));
	}
}

bool ReadDefinitionIdArray(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	TArray<FString>& OutIds)
{
	OutIds.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!TryReadArray(Object, Field, Values))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			return false;
		}
		OutIds.Add(Value->AsString());
	}
	return true;
}

FWBCardInstanceRef MakeCardInstance(
	const int32 PlayerId,
	const int32 DeckIndex,
	const FString& DefinitionId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = FString::Printf(
		TEXT("p%d_card_%03d_%s"),
		PlayerId,
		DeckIndex,
		*DefinitionId);
	Card.CardId = DefinitionId;
	Card.OwnerPlayerId = PlayerId;
	return Card;
}

FWBCardInstanceRef MakeHeroInstance(
	const int32 PlayerId,
	const FString& DefinitionId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = FString::Printf(
		TEXT("p%d_hero_%s"),
		PlayerId,
		*DefinitionId);
	Card.CardId = DefinitionId;
	Card.OwnerPlayerId = PlayerId;
	return Card;
}

void SortDiagnostics(FWBProductionMatchSpecificationLoadResult& Result)
{
	Result.Diagnostics.Sort([](
		const FWBProductionCardDBDiagnostic& A,
		const FWBProductionCardDBDiagnostic& B)
	{
		if (A.DefinitionId != B.DefinitionId)
		{
			return A.DefinitionId < B.DefinitionId;
		}
		if (A.FieldPath != B.FieldPath)
		{
			return A.FieldPath < B.FieldPath;
		}
		return A.Code < B.Code;
	});
}
}

FWBProductionMatchSpecificationLoadResult
WBProductionMatchSpecification::LoadAndBuildRequest(
	const FString& MatchSpecPath,
	const FWBProductionCardDatabase& Database)
{
	const FString ResolvedPath =
		WBProductionCardDatabase::ResolveInputPath(MatchSpecPath);
	FString Json;
	if (ResolvedPath.IsEmpty()
		|| !FFileHelper::LoadFileToString(Json, *ResolvedPath))
	{
		FWBProductionMatchSpecificationLoadResult Result;
		Result.MatchSpecPath = MatchSpecPath;
		Result.Reason = TEXT("production_match_spec_not_found");
		AddDiagnostic(
			Result,
			TEXT("match_spec_not_found"),
			FString(),
			TEXT("$"),
			TEXT("The explicit production match specification could not be loaded."),
			TEXT("Provide an existing -WandboundMatchSpec path."));
		return Result;
	}
	return ParseAndBuildRequestForTest(
		Json,
		ResolvedPath,
		Database);
}

FWBProductionMatchSpecificationLoadResult
WBProductionMatchSpecification::ParseAndBuildRequestForTest(
	const FString& Json,
	const FString& MatchSpecPath,
	const FWBProductionCardDatabase& Database)
{
	FWBProductionMatchSpecificationLoadResult Result;
	Result.MatchSpecPath = MatchSpecPath;

	TSharedPtr<FJsonObject> Root;
	if (!ParseJsonObject(Json, Root))
	{
		Result.Reason = TEXT("production_match_spec_invalid");
		AddDiagnostic(
			Result,
			TEXT("match_spec_json_parse_failed"),
			FString(),
			TEXT("$"),
			TEXT("The production match specification is not valid JSON."));
		return Result;
	}

	ValidateKnownFields(
		Result,
		Root,
		{
			TEXT("schema_version"),
			TEXT("match_id"),
			TEXT("seed"),
			TEXT("first_player"),
			TEXT("initial_draw_count"),
			TEXT("definition_bundle_digest"),
			TEXT("players"),
			TEXT("markers")
		},
		TEXT("$"));

	FWBProductionMatchSpecification& Specification = Result.Specification;
	if (!TryReadInteger(Root, TEXT("schema_version"), Specification.SchemaVersion)
		|| Specification.SchemaVersion != SupportedMatchSchemaVersion)
	{
		AddDiagnostic(
			Result,
			TEXT("match_schema_version_unsupported"),
			FString(),
			TEXT("$.schema_version"),
			TEXT("The production match schema version is missing or unsupported."));
	}
	if (!TryReadString(Root, TEXT("match_id"), Specification.MatchId)
		|| !WBProductionCardDatabase::IsSafeDefinitionId(Specification.MatchId))
	{
		AddDiagnostic(
			Result,
			TEXT("match_id_invalid"),
			FString(),
			TEXT("$.match_id"),
			TEXT("The match id must use the canonical lowercase identifier policy."));
	}
	if (!TryReadInteger(Root, TEXT("seed"), Specification.Seed)
		|| Specification.Seed <= 0)
	{
		AddDiagnostic(
			Result,
			TEXT("match_seed_invalid"),
			FString(),
			TEXT("$.seed"),
			TEXT("Production match seed must be a positive integer."));
	}
	if (!TryReadInteger(Root, TEXT("first_player"), Specification.FirstPlayerId)
		|| !FWBGameStateData::IsValidPlayerId(Specification.FirstPlayerId))
	{
		AddDiagnostic(
			Result,
			TEXT("first_player_invalid"),
			FString(),
			TEXT("$.first_player"),
			TEXT("The explicit first player must be Player 0 or Player 1."));
	}
	if (!TryReadInteger(
		Root,
		TEXT("initial_draw_count"),
		Specification.InitialDrawCount)
		|| Specification.InitialDrawCount != SupportedInitialDrawCount)
	{
		AddDiagnostic(
			Result,
			TEXT("initial_draw_policy_unsupported"),
			FString(),
			TEXT("$.initial_draw_count"),
			TEXT("The current canonical runtime supports an opening draw of exactly 6."));
	}
	if (!TryReadString(
		Root,
		TEXT("definition_bundle_digest"),
		Specification.DefinitionBundleDigest)
		|| Specification.DefinitionBundleDigest != Database.ContentDigest)
	{
		AddDiagnostic(
			Result,
			TEXT("definition_bundle_digest_mismatch"),
			FString(),
			TEXT("$.definition_bundle_digest"),
			TEXT("The match specification does not identify the loaded immutable CardDB snapshot."),
			TEXT("Regenerate the match spec against the reported bundle digest."));
	}

	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (!TryReadArray(Root, TEXT("players"), Players)
		|| Players->Num() != 2)
	{
		AddDiagnostic(
			Result,
			TEXT("match_players_invalid"),
			FString(),
			TEXT("$.players"),
			TEXT("A production match requires exactly two player specifications."));
	}
	else
	{
		for (int32 Index = 0; Index < Players->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& PlayerValue = (*Players)[Index];
			const FString PlayerPath =
				FString::Printf(TEXT("$.players[%d]"), Index);
			if (!PlayerValue.IsValid()
				|| PlayerValue->Type != EJson::Object)
			{
				AddDiagnostic(
					Result,
					TEXT("match_player_malformed"),
					FString(),
					PlayerPath,
					TEXT("Player specifications must be objects."));
				continue;
			}

			const TSharedPtr<FJsonObject> PlayerObject = PlayerValue->AsObject();
			ValidateKnownFields(
				Result,
				PlayerObject,
				{
					TEXT("player_id"),
					TEXT("hero_definition_id"),
					TEXT("ordered_deck")
				},
				PlayerPath);

			FWBProductionPlayerMatchSpecification Player;
			if (!TryReadInteger(
				PlayerObject,
				TEXT("player_id"),
				Player.PlayerId)
				|| !FWBGameStateData::IsValidPlayerId(Player.PlayerId))
			{
				AddDiagnostic(
					Result,
					TEXT("player_id_invalid"),
					FString(),
					PlayerPath + TEXT(".player_id"),
					TEXT("Player id must be 0 or 1."));
			}
			if (!TryReadString(
				PlayerObject,
				TEXT("hero_definition_id"),
				Player.HeroDefinitionId)
				|| Database.FindHero(Player.HeroDefinitionId) == nullptr)
			{
				AddDiagnostic(
					Result,
					TEXT("hero_definition_invalid"),
					Player.HeroDefinitionId,
					PlayerPath + TEXT(".hero_definition_id"),
					TEXT("The selected Hero must resolve to a production Hero definition."));
			}
			if (!ReadDefinitionIdArray(
				PlayerObject,
				TEXT("ordered_deck"),
				Player.OrderedDeckDefinitionIds)
				|| Player.OrderedDeckDefinitionIds.Num()
					< SupportedInitialDrawCount)
			{
				AddDiagnostic(
					Result,
					TEXT("deck_size_invalid"),
					FString(),
					PlayerPath + TEXT(".ordered_deck"),
					TEXT("The ordered deck must contain enough non-Hero cards for the opening draw."));
			}
			else
			{
				for (int32 DeckIndex = 0;
					DeckIndex < Player.OrderedDeckDefinitionIds.Num();
					++DeckIndex)
				{
					const FString& DefinitionId =
						Player.OrderedDeckDefinitionIds[DeckIndex];
					const FWBProductionCardRecord* Record =
						Database.FindRecord(DefinitionId);
					if (Record == nullptr)
					{
						AddDiagnostic(
							Result,
							TEXT("deck_definition_missing"),
							DefinitionId,
							FString::Printf(
								TEXT("%s.ordered_deck[%d]"),
								*PlayerPath,
								DeckIndex),
							TEXT("The ordered deck references a missing definition."));
					}
					else if (Record->Type == EWBProductionCardType::Hero)
					{
						AddDiagnostic(
							Result,
							TEXT("deck_definition_wrong_type"),
							DefinitionId,
							FString::Printf(
								TEXT("%s.ordered_deck[%d]"),
								*PlayerPath,
								DeckIndex),
							TEXT("Hero definitions are selected separately and cannot remain in the ordered deck."));
					}
				}
			}
			Specification.Players.Add(MoveTemp(Player));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Markers = nullptr;
	if (!TryReadArray(Root, TEXT("markers"), Markers)
		|| Markers->Num() != 8)
	{
		AddDiagnostic(
			Result,
			TEXT("marker_setup_invalid"),
			FString(),
			TEXT("$.markers"),
			TEXT("A production match requires exactly eight explicitly ordered marker placements."));
	}
	else
	{
		for (int32 Index = 0; Index < Markers->Num(); ++Index)
		{
			const FString MarkerPath =
				FString::Printf(TEXT("$.markers[%d]"), Index);
			const TSharedPtr<FJsonValue>& MarkerValue = (*Markers)[Index];
			if (!MarkerValue.IsValid() || MarkerValue->Type != EJson::Object)
			{
				AddDiagnostic(
					Result,
					TEXT("marker_setup_entry_malformed"),
					FString(),
					MarkerPath,
					TEXT("Marker placements must be objects."));
				continue;
			}

			const TSharedPtr<FJsonObject> MarkerObject = MarkerValue->AsObject();
			ValidateKnownFields(
				Result,
				MarkerObject,
				{
					TEXT("player_id"),
					TEXT("type"),
					TEXT("tile"),
					TEXT("definition_id"),
					TEXT("placement_order")
				},
				MarkerPath);

			FWBSetupMarkerPlacement Placement;
			FString MarkerType;
			TSharedPtr<FJsonObject> Tile;
			const bool bPlayerValid =
				TryReadInteger(MarkerObject, TEXT("player_id"), Placement.PlayerId)
				&& FWBGameStateData::IsValidPlayerId(Placement.PlayerId);
			const bool bOrderValid =
				TryReadInteger(
					MarkerObject,
					TEXT("placement_order"),
					Placement.PlacementOrder)
				&& Placement.PlacementOrder == Index;
			const bool bTypeValid =
				TryReadString(MarkerObject, TEXT("type"), MarkerType)
				&& (MarkerType == TEXT("trap") || MarkerType == TEXT("npc"));
			if (bTypeValid)
			{
				Placement.Type = MarkerType == TEXT("trap")
					? EWBMarkerType::Trap
					: EWBMarkerType::NPC;
			}
			const bool bTileValid =
				TryReadObject(MarkerObject, TEXT("tile"), Tile)
				&& TryReadInteger(Tile, TEXT("x"), Placement.Tile.X)
				&& TryReadInteger(Tile, TEXT("y"), Placement.Tile.Y);
			if (Tile.IsValid())
			{
				ValidateKnownFields(
					Result,
					Tile,
					{ TEXT("x"), TEXT("y") },
					MarkerPath + TEXT(".tile"));
			}
			const bool bDefinitionPresent = TryReadString(
				MarkerObject,
				TEXT("definition_id"),
				Placement.DefinitionId);
			const FWBProductionCardRecord* Record =
				bDefinitionPresent
					? Database.FindRecord(Placement.DefinitionId)
					: nullptr;
			const bool bDefinitionValid =
				Record != nullptr
				&& ((Placement.Type == EWBMarkerType::Trap
						&& Record->Type == EWBProductionCardType::Trap)
					|| (Placement.Type == EWBMarkerType::NPC
						&& Record->Type == EWBProductionCardType::NPC));

			if (!bPlayerValid || !bOrderValid || !bTypeValid
				|| !bTileValid || !bDefinitionValid)
			{
				AddDiagnostic(
					Result,
					TEXT("marker_setup_entry_invalid"),
					Placement.DefinitionId,
					MarkerPath,
					TEXT("Marker owner, type, tile, definition, and placement order must be explicit and compatible."));
			}
			Specification.MarkerPlacements.Add(MoveTemp(Placement));
		}
	}

	Specification.Players.Sort([](
		const FWBProductionPlayerMatchSpecification& A,
		const FWBProductionPlayerMatchSpecification& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	if (Specification.Players.Num() == 2
		&& (Specification.Players[0].PlayerId != 0
			|| Specification.Players[1].PlayerId != 1))
	{
		AddDiagnostic(
			Result,
			TEXT("match_players_invalid"),
			FString(),
			TEXT("$.players"),
			TEXT("Player specifications must contain one entry for Player 0 and one for Player 1."));
	}

	SortDiagnostics(Result);
	if (!Result.Diagnostics.IsEmpty())
	{
		Result.Reason = TEXT("production_match_spec_invalid");
		Result.InitializationRequest = FWBMatchInitializationRequest();
		return Result;
	}

	Result.InitializationRequest.Seed = Specification.Seed;
	Result.InitializationRequest.FirstPlayerId = Specification.FirstPlayerId;
	Result.InitializationRequest.Repository = Database.CoreRepository;
	for (const FWBProductionPlayerMatchSpecification& Player : Specification.Players)
	{
		FWBMatchPlayerSetup Setup;
		Setup.PlayerId = Player.PlayerId;
		Setup.HeroCardId = Player.HeroDefinitionId;
		const FWBCardInstanceRef Hero =
			MakeHeroInstance(Player.PlayerId, Player.HeroDefinitionId);
		Setup.HeroInstanceId = Hero.InstanceId;
		Setup.OrderedDeck.Add(Hero);
		for (int32 Index = 0; Index < Player.OrderedDeckDefinitionIds.Num(); ++Index)
		{
			Setup.OrderedDeck.Add(
				MakeCardInstance(
					Player.PlayerId,
					Index,
					Player.OrderedDeckDefinitionIds[Index]));
		}
		Result.InitializationRequest.Players.Add(MoveTemp(Setup));
	}
	Result.InitializationRequest.MarkerPlacements =
		Specification.MarkerPlacements;

	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}
