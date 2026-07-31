#include "WBGameStartAddendum.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBProductionCardDatabase.h"
#include <openssl/sha.h>

namespace
{
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

bool ReadBool(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	bool& Out)
{
	return Object.IsValid() && Object->TryGetBoolField(Field, Out);
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
		return false;
	}
	Out = (*Value)->AsObject();
	return Out.IsValid();
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
	return !Out.IsEmpty();
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
}

FWBGameStartAddendumLoadResult WBGameStartAddendum::Load(
	const FString& Path)
{
	FWBGameStartAddendumLoadResult Result;
	const FString Resolved =
		WBProductionCardDatabase::ResolveInputPath(Path);
	FString Json;
	if (Resolved.IsEmpty()
		|| !FFileHelper::LoadFileToString(Json, *Resolved))
	{
		Result.SourcePath = Path;
		Result.Reason = TEXT("game_start_addendum_not_found");
		return Result;
	}
	return ParseForTest(Json, Resolved);
}

FWBGameStartAddendumLoadResult
WBGameStartAddendum::ParseForTest(
	const FString& Json,
	const FString& SourcePath)
{
	FWBGameStartAddendumLoadResult Result;
	Result.SourcePath = SourcePath;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root)
		|| !HasExactFields(
			Root,
			{
				TEXT("schema_version"),
				TEXT("addendum_id"),
				TEXT("addendum_version"),
				TEXT("authority"),
				TEXT("approval_date"),
				TEXT("addendum_digest"),
				TEXT("supersedes_rules"),
				TEXT("setup_sequence"),
				TEXT("hero_spawn"),
				TEXT("trigger_order"),
				TEXT("reaction_policy"),
				TEXT("opening_hand_timing"),
				TEXT("reserved_tiles"),
				TEXT("turn_one"),
				TEXT("provenance")
			}))
	{
		Result.Reason = TEXT("game_start_addendum_schema_invalid");
		return Result;
	}

	FWBGameStartAddendumV1& Addendum = Result.Addendum;
	TSharedPtr<FJsonObject> HeroSpawn;
	TSharedPtr<FJsonObject> TriggerOrder;
	TSharedPtr<FJsonObject> ReactionPolicy;
	TSharedPtr<FJsonObject> OpeningHandTiming;
	TSharedPtr<FJsonObject> ReservedTiles;
	TSharedPtr<FJsonObject> TurnOne;
	if (!ReadInteger(Root, TEXT("schema_version"), Addendum.SchemaVersion)
		|| Addendum.SchemaVersion != 1
		|| !ReadString(Root, TEXT("addendum_id"), Addendum.AddendumId)
		|| Addendum.AddendumId != TEXT("game_start_turn_one_v1")
		|| !ReadInteger(
			Root,
			TEXT("addendum_version"),
			Addendum.AddendumVersion)
		|| Addendum.AddendumVersion != 1
		|| !ReadString(Root, TEXT("authority"), Addendum.Authority)
		|| Addendum.Authority != TEXT("product_owner_approved")
		|| !ReadString(Root, TEXT("approval_date"), Addendum.ApprovalDate)
		|| Addendum.ApprovalDate != TEXT("2026-07-30")
		|| !ReadString(Root, TEXT("addendum_digest"), Addendum.Digest)
		|| !ReadStringArray(
			Root,
			TEXT("supersedes_rules"),
			Addendum.SupersedesRules)
		|| !ReadStringArray(
			Root,
			TEXT("setup_sequence"),
			Addendum.SetupSequence)
		|| !ReadStringArray(
			Root,
			TEXT("provenance"),
			Addendum.Provenance)
		|| !ReadObject(Root, TEXT("hero_spawn"), HeroSpawn)
		|| !ReadObject(Root, TEXT("trigger_order"), TriggerOrder)
		|| !ReadObject(Root, TEXT("reaction_policy"), ReactionPolicy)
		|| !ReadObject(
			Root,
			TEXT("opening_hand_timing"),
			OpeningHandTiming)
		|| !ReadObject(Root, TEXT("reserved_tiles"), ReservedTiles)
		|| !ReadObject(Root, TEXT("turn_one"), TurnOne)
		|| !HasExactFields(
			HeroSpawn,
			{
				TEXT("atomic"),
				TEXT("placement_counts_as_summon"),
				TEXT("collect_triggers_after_batch_commit")
			})
		|| !HasExactFields(
			TriggerOrder,
			{
				TEXT("first_player_batch_first"),
				TEXT("player_selected_within_batch"),
				TEXT("stable_replay_decision_ids")
			})
		|| !HasExactFields(
			ReactionPolicy,
			{
				TEXT("manual_reacts_allowed"),
				TEXT("priority_passing_allowed"),
				TEXT("required_choices_continue")
			})
		|| !HasExactFields(
			OpeningHandTiming,
			{
				TEXT("setup_effect_draws_before_standard_draw"),
				TEXT("standard_draw_count"),
				TEXT("setup_trigger_draw_reason"),
				TEXT("opening_hand_draw_reason")
			})
		|| !HasExactFields(
			ReservedTiles,
			{ TEXT("hero_spawn_tiles_reserved") })
		|| !HasExactFields(
			TurnOne,
			{
				TEXT("board_size"),
				TEXT("neutral_row"),
				TEXT("first_player_only"),
				TEXT("neutral_row_summons_allowed"),
				TEXT("opponent_half_summons_allowed"),
				TEXT("protected_boundary_crossing_allowed"),
				TEXT("all_relocation_operations_covered"),
				TEXT("neutral_npc_attacks_allowed"),
				TEXT("opponent_controlled_attacks_allowed")
			})
		|| !ReadBool(HeroSpawn, TEXT("atomic"), Addendum.bAtomicHeroSpawn)
		|| !ReadBool(
			HeroSpawn,
			TEXT("placement_counts_as_summon"),
			Addendum.bHeroPlacementCountsAsSummon)
		|| !ReadBool(
			HeroSpawn,
			TEXT("collect_triggers_after_batch_commit"),
			Addendum.bCollectTriggersAfterBatchCommit)
		|| !ReadBool(
			TriggerOrder,
			TEXT("first_player_batch_first"),
			Addendum.bFirstPlayerTriggerBatchFirst)
		|| !ReadBool(
			TriggerOrder,
			TEXT("player_selected_within_batch"),
			Addendum.bPlayerSelectedTriggerOrder)
		|| !ReadBool(
			TriggerOrder,
			TEXT("stable_replay_decision_ids"),
			Addendum.bStableReplayDecisionIds)
		|| !ReadBool(
			ReactionPolicy,
			TEXT("manual_reacts_allowed"),
			Addendum.bManualReactsAllowedDuringSetup)
		|| !ReadBool(
			ReactionPolicy,
			TEXT("priority_passing_allowed"),
			Addendum.bPriorityPassingAllowedDuringSetup)
		|| !ReadBool(
			ReactionPolicy,
			TEXT("required_choices_continue"),
			Addendum.bRequiredChoicesContinue)
		|| !ReadBool(
			OpeningHandTiming,
			TEXT("setup_effect_draws_before_standard_draw"),
			Addendum.bSetupEffectDrawsBeforeOpeningHand)
		|| !ReadInteger(
			OpeningHandTiming,
			TEXT("standard_draw_count"),
			Addendum.OpeningHandCount)
		|| !ReadString(
			OpeningHandTiming,
			TEXT("setup_trigger_draw_reason"),
			Addendum.SetupTriggerDrawReason)
		|| !ReadString(
			OpeningHandTiming,
			TEXT("opening_hand_draw_reason"),
			Addendum.OpeningHandDrawReason)
		|| !ReadBool(
			ReservedTiles,
			TEXT("hero_spawn_tiles_reserved"),
			Addendum.bHeroSpawnTilesReserved)
		|| !ReadInteger(TurnOne, TEXT("board_size"), Addendum.BoardSize)
		|| !ReadInteger(TurnOne, TEXT("neutral_row"), Addendum.NeutralRow)
		|| !ReadBool(
			TurnOne,
			TEXT("first_player_only"),
			Addendum.bFirstPlayerTurnOneOnly)
		|| !ReadBool(
			TurnOne,
			TEXT("neutral_row_summons_allowed"),
			Addendum.bNeutralRowSummonsAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("opponent_half_summons_allowed"),
			Addendum.bOpponentHalfSummonsAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("protected_boundary_crossing_allowed"),
			Addendum.bProtectedBoundaryCrossingAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("all_relocation_operations_covered"),
			Addendum.bAllRelocationOperationsCovered)
		|| !ReadBool(
			TurnOne,
			TEXT("neutral_npc_attacks_allowed"),
			Addendum.bNeutralNPCAttacksAllowed)
		|| !ReadBool(
			TurnOne,
			TEXT("opponent_controlled_attacks_allowed"),
			Addendum.bOpponentControlledAttacksAllowed))
	{
		Result.Reason = TEXT("game_start_addendum_schema_invalid");
		return Result;
	}

	const TArray<FString> ExpectedSequence = {
		TEXT("format_validation"),
		TEXT("first_player_selection"),
		TEXT("marker_placement"),
		TEXT("hero_atomic_spawn"),
		TEXT("hero_trigger_collection"),
		TEXT("first_player_hero_trigger_resolution"),
		TEXT("second_player_hero_trigger_resolution"),
		TEXT("opening_hand_draw"),
		TEXT("first_turn_ready")
	};
	const TArray<FString> ExpectedSupersededRules = {
		TEXT("rules_bible_v2.sequential_hero_deployment"),
		TEXT("rules_bible_v2_1.first_player_turn_one_blanket_attack_prohibition")
	};
	const bool bDigestMatches =
		ComputeDigest(Addendum) == Addendum.Digest;
	if (Addendum.SetupSequence != ExpectedSequence
		|| Addendum.SupersedesRules != ExpectedSupersededRules
		|| !Addendum.bAtomicHeroSpawn
		|| !Addendum.bHeroPlacementCountsAsSummon
		|| !Addendum.bCollectTriggersAfterBatchCommit
		|| !Addendum.bFirstPlayerTriggerBatchFirst
		|| !Addendum.bPlayerSelectedTriggerOrder
		|| !Addendum.bStableReplayDecisionIds
		|| Addendum.bManualReactsAllowedDuringSetup
		|| Addendum.bPriorityPassingAllowedDuringSetup
		|| !Addendum.bRequiredChoicesContinue
		|| !Addendum.bSetupEffectDrawsBeforeOpeningHand
		|| Addendum.OpeningHandCount != 6
		|| !Addendum.bHeroSpawnTilesReserved
		|| Addendum.BoardSize != 9
		|| Addendum.NeutralRow != 4
		|| !Addendum.bFirstPlayerTurnOneOnly
		|| !Addendum.bNeutralRowSummonsAllowed
		|| Addendum.bOpponentHalfSummonsAllowed
		|| Addendum.bProtectedBoundaryCrossingAllowed
		|| !Addendum.bAllRelocationOperationsCovered
		|| !Addendum.bNeutralNPCAttacksAllowed
		|| Addendum.bOpponentControlledAttacksAllowed
		|| Addendum.SetupTriggerDrawReason != TEXT("setup_trigger_draw")
		|| Addendum.OpeningHandDrawReason != TEXT("opening_hand_draw")
		|| !bDigestMatches)
	{
		Result.Reason = !bDigestMatches
			? FString(TEXT("game_start_addendum_digest_mismatch"))
			: FString(TEXT("game_start_addendum_schema_invalid"));
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("success");
	return Result;
}

FString WBGameStartAddendum::ComputeDigest(
	const FWBGameStartAddendumV1& Addendum)
{
	TArray<FString> Provenance = Addendum.Provenance;
	Provenance.Sort();
	return SHA256String(FString::Printf(
		TEXT("%s|%d|%s|%s|%s|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s"),
		*Addendum.AddendumId,
		Addendum.AddendumVersion,
		*Addendum.Authority,
		*Addendum.ApprovalDate,
		*FString::Join(Addendum.SupersedesRules, TEXT(",")),
		*FString::Join(Addendum.SetupSequence, TEXT(",")),
		Addendum.bAtomicHeroSpawn ? 1 : 0,
		Addendum.bHeroPlacementCountsAsSummon ? 1 : 0,
		Addendum.bCollectTriggersAfterBatchCommit ? 1 : 0,
		Addendum.bFirstPlayerTriggerBatchFirst ? 1 : 0,
		Addendum.bPlayerSelectedTriggerOrder ? 1 : 0,
		Addendum.bStableReplayDecisionIds ? 1 : 0,
		Addendum.bManualReactsAllowedDuringSetup ? 1 : 0,
		Addendum.bPriorityPassingAllowedDuringSetup ? 1 : 0,
		Addendum.bRequiredChoicesContinue ? 1 : 0,
		Addendum.bSetupEffectDrawsBeforeOpeningHand ? 1 : 0,
		Addendum.OpeningHandCount,
		*Addendum.SetupTriggerDrawReason,
		*Addendum.OpeningHandDrawReason,
		Addendum.bHeroSpawnTilesReserved ? 1 : 0,
		Addendum.BoardSize,
		Addendum.NeutralRow,
		Addendum.bFirstPlayerTurnOneOnly ? 1 : 0,
		Addendum.bNeutralRowSummonsAllowed ? 1 : 0,
		Addendum.bOpponentHalfSummonsAllowed ? 1 : 0,
		Addendum.bProtectedBoundaryCrossingAllowed ? 1 : 0,
		Addendum.bAllRelocationOperationsCovered ? 1 : 0,
		Addendum.bNeutralNPCAttacksAllowed ? 1 : 0,
		Addendum.bOpponentControlledAttacksAllowed ? 1 : 0,
		*FString::Join(Provenance, TEXT(","))));
}
