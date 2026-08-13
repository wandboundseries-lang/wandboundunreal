#include "WBProductionCardDatabase.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBStatusEffect.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

namespace
{
constexpr int32 SupportedSchemaVersion = 1;

struct FBundleReference
{
	FString Name;
	FString RelativePath;
	FString OwnerManifestPath;
};

bool IsKnownField(
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& KnownFields,
	TArray<FString>& OutUnknownFields)
{
	OutUnknownFields.Reset();
	if (!Object.IsValid())
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!KnownFields.Contains(Pair.Key))
		{
			OutUnknownFields.Add(Pair.Key);
		}
	}
	OutUnknownFields.Sort();
	return OutUnknownFields.IsEmpty();
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

bool TryReadBool(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	bool& OutValue)
{
	const TSharedPtr<FJsonValue>* Value =
		Object.IsValid() ? Object->Values.Find(Field) : nullptr;
	if (Value == nullptr
		|| !Value->IsValid()
		|| (*Value)->Type != EJson::Boolean)
	{
		return false;
	}
	OutValue = (*Value)->AsBool();
	return true;
}

bool HasField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field)
{
	return Object.IsValid() && Object->Values.Contains(Field);
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

bool ParseJsonObject(
	const FString& Json,
	TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutObject)
		&& OutObject.IsValid();
}

FString NormalizeRelativePath(const FString& Path)
{
	FString Normalized = Path;
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Normalized.StartsWith(TEXT("./")))
	{
		Normalized.RightChopInline(2);
	}
	return Normalized;
}

FString CanonicalHybridToken(const FName Token)
{
	FString Canonical = Token.ToString();
	Canonical.ToLowerInline();
	return Canonical;
}

FString SHA256String(const FString& Value)
{
	const FTCHARToUTF8 Utf8(*Value);
	uint8 Digest[SHA256_DIGEST_LENGTH] = {};
	if (SHA256(
		reinterpret_cast<const uint8*>(Utf8.Get()),
		static_cast<size_t>(Utf8.Length()),
		Digest) == nullptr)
	{
		return FString();
	}

	FString Result;
	Result.Reserve(SHA256_DIGEST_LENGTH * 2);
	for (const uint8 Byte : Digest)
	{
		Result += FString::Printf(TEXT("%02x"), Byte);
	}
	return Result;
}

bool IsCanonicalFaction(const FString& Faction)
{
	return Faction == TEXT("black_meridian")
		|| Faction == TEXT("csn")
		|| Faction == TEXT("wandwright")
		|| Faction == TEXT("officer")
		|| Faction == TEXT("marrow_syndicate");
}

bool IsSafeTag(const FString& Tag)
{
	if (Tag.IsEmpty() || Tag.Len() > 64)
	{
		return false;
	}
	for (const TCHAR Character : Tag)
	{
		if (!(FChar::IsLower(Character)
			|| FChar::IsDigit(Character)
			|| Character == TEXT('_')))
		{
			return false;
		}
	}
	return true;
}

bool ReadStringArray(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	TArray<FString>& OutValues)
{
	OutValues.Reset();
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
		OutValues.Add(Value->AsString());
	}
	OutValues.Sort();
	for (int32 Index = OutValues.Num() - 1; Index > 0; --Index)
	{
		if (OutValues[Index] == OutValues[Index - 1])
		{
			OutValues.RemoveAt(Index);
		}
	}
	return true;
}

EWBProductionBundleKind ParseBundleKind(const FString& Value)
{
	if (Value == TEXT("production"))
	{
		return EWBProductionBundleKind::Production;
	}
	if (Value == TEXT("test"))
	{
		return EWBProductionBundleKind::Test;
	}
	if (Value == TEXT("development"))
	{
		return EWBProductionBundleKind::Development;
	}
	return EWBProductionBundleKind::Unknown;
}

EWBProductionCardType ParseCardType(const FString& Value)
{
	if (Value == TEXT("character"))
	{
		return EWBProductionCardType::Character;
	}
	if (Value == TEXT("hero"))
	{
		return EWBProductionCardType::Hero;
	}
	if (Value == TEXT("hybrid"))
	{
		return EWBProductionCardType::Hybrid;
	}
	if (Value == TEXT("wand"))
	{
		return EWBProductionCardType::Wand;
	}
	if (Value == TEXT("action"))
	{
		return EWBProductionCardType::Action;
	}
	if (Value == TEXT("trap"))
	{
		return EWBProductionCardType::Trap;
	}
	if (Value == TEXT("npc"))
	{
		return EWBProductionCardType::NPC;
	}
	return EWBProductionCardType::Unknown;
}

EWBCardDefinitionKind CoreKindFor(const EWBProductionCardType Type)
{
	switch (Type)
	{
	case EWBProductionCardType::Character:
	case EWBProductionCardType::Hero:
		return EWBCardDefinitionKind::Character;
	case EWBProductionCardType::Hybrid:
		return EWBCardDefinitionKind::Hybrid;
	case EWBProductionCardType::Wand:
		return EWBCardDefinitionKind::Wand;
	case EWBProductionCardType::Action:
		return EWBCardDefinitionKind::Action;
	case EWBProductionCardType::Trap:
		return EWBCardDefinitionKind::Trap;
	case EWBProductionCardType::NPC:
		return EWBCardDefinitionKind::NPC;
	default:
		return EWBCardDefinitionKind::Unknown;
	}
}

EWBCardEffectTargetRequirement ParseTargetRequirement(const FString& Value)
{
	if (Value == TEXT("unit"))
	{
		return EWBCardEffectTargetRequirement::Unit;
	}
	if (Value == TEXT("none"))
	{
		return EWBCardEffectTargetRequirement::None;
	}
	if (Value == TEXT("tile"))
	{
		return EWBCardEffectTargetRequirement::Tile;
	}
	if (Value == TEXT("wall_edge"))
	{
		return EWBCardEffectTargetRequirement::WallEdge;
	}
	return EWBCardEffectTargetRequirement::None;
}

EWBCardActivationSourceZone ParseSourceZone(const FString& Value)
{
	if (Value == TEXT("board"))
	{
		return EWBCardActivationSourceZone::Board;
	}
	if (Value == TEXT("equipped"))
	{
		return EWBCardActivationSourceZone::Equipped;
	}
	if (Value == TEXT("hand"))
	{
		return EWBCardActivationSourceZone::Hand;
	}
	return EWBCardActivationSourceZone::Unknown;
}

EWBArmorEffectOp ParseArmorOperation(const FString& Value)
{
	if (Value == TEXT("add_current_armor")) return EWBArmorEffectOp::AddCurrentArmor;
	if (Value == TEXT("reduce_current_armor")) return EWBArmorEffectOp::ReduceCurrentArmor;
	if (Value == TEXT("set_current_armor")) return EWBArmorEffectOp::SetCurrentArmor;
	if (Value == TEXT("add_max_armor")) return EWBArmorEffectOp::AddMaxArmor;
	if (Value == TEXT("reduce_max_armor")) return EWBArmorEffectOp::ReduceMaxArmor;
	if (Value == TEXT("set_max_armor")) return EWBArmorEffectOp::SetMaxArmor;
	if (Value == TEXT("restore_armor_to_max")) return EWBArmorEffectOp::RestoreArmorToMax;
	return EWBArmorEffectOp::Unknown;
}

EWBStatusEffectOp ParseStatusOperation(const FString& Value)
{
	if (Value == TEXT("apply_status")) return EWBStatusEffectOp::ApplyStatus;
	if (Value == TEXT("remove_status")) return EWBStatusEffectOp::RemoveStatus;
	if (Value == TEXT("set_status_duration")) return EWBStatusEffectOp::SetStatusDuration;
	if (Value == TEXT("add_status_duration")) return EWBStatusEffectOp::AddStatusDuration;
	if (Value == TEXT("reduce_status_duration")) return EWBStatusEffectOp::ReduceStatusDuration;
	if (Value == TEXT("cleanse_status")) return EWBStatusEffectOp::CleanseStatus;
	if (Value == TEXT("cleanse_all_statuses")) return EWBStatusEffectOp::CleanseAllStatuses;
	return EWBStatusEffectOp::Unknown;
}

bool IsSupportedStatus(const FName StatusId)
{
	const FName Canonical = WBStatusEffect::CanonicalizeStatusId(StatusId);
	return Canonical == FName(TEXT("Burn"))
		|| Canonical == FName(TEXT("Poison"))
		|| Canonical == FName(TEXT("Rooted"))
		|| Canonical == FName(TEXT("Stunned"))
		|| Canonical == FName(TEXT("Frozen"))
		|| Canonical == FName(TEXT("CannotAttack"));
}

FString EffectDigest(const FWBCardEffectDefinition& Effect)
{
	FString Digest = FString::Printf(
		TEXT("effect=%s|label=%s|target=%d|zone=%d|timing=%d|source=%d|owner=%d|stun=%d|frozen=%d|once=%d|key=%s|rr=%d|cost=%s"),
		*Effect.EffectId,
		*Effect.PublicLabel,
		static_cast<int32>(Effect.TargetRequirement),
		static_cast<int32>(Effect.SourceGate.RequiredZone),
		static_cast<int32>(Effect.SourceGate.Timing),
		Effect.SourceGate.bRequiresSourceUnit ? 1 : 0,
		Effect.SourceGate.bRequiresSourceUnitOwnership ? 1 : 0,
		Effect.SourceGate.bBlockedByStunned ? 1 : 0,
		Effect.SourceGate.bBlockedByFrozen ? 1 : 0,
		Effect.SourceGate.bOncePerTurn ? 1 : 0,
		*Effect.SourceGate.OncePerTurnKey,
		Effect.SourceGate.CostGate.RequiredRR,
		*Effect.SourceGate.CostGate.CostKind.ToString());
	for (const FWBGenericEffectPayload& Payload : Effect.Payloads)
	{
		Digest += FString::Printf(TEXT("|payload=%d"), static_cast<int32>(Payload.Operation));
		switch (Payload.Operation)
		{
		case EWBGenericEffectOp::DamageEffect:
			Digest += FString::Printf(
				TEXT(":%d:%d"),
				Payload.DamageEffect.Amount,
				Payload.DamageEffect.bBypassArmor ? 1 : 0);
			break;
		case EWBGenericEffectOp::HealEffect:
			Digest += FString::Printf(TEXT(":%d"), Payload.HealEffect.Amount);
			break;
		case EWBGenericEffectOp::StatusEffect:
			Digest += FString::Printf(
				TEXT(":%d:%s:%d"),
				static_cast<int32>(Payload.StatusEffect.Operation),
				*Payload.StatusEffect.StatusId.ToString(),
				Payload.StatusEffect.Duration);
			break;
		case EWBGenericEffectOp::ArmorEffect:
			Digest += FString::Printf(
				TEXT(":%d:%d"),
				static_cast<int32>(Payload.ArmorEffect.Operation),
				Payload.ArmorEffect.Amount);
			break;
		case EWBGenericEffectOp::PreventPendingAttack:
		case EWBGenericEffectOp::RedirectPendingAttack:
			break;
		default:
			break;
		}
	}
	return Digest;
}

FString SnapshotDigestSource(const FWBProductionCardDatabase& Database)
{
	FString Source = FString::Printf(
		TEXT("schema=1|kind=%s|carddb=%s|source=%s\n"),
		*WBProductionCardDatabase::BundleKindToString(Database.BundleKind),
		*Database.CardDBVersion,
		*Database.SourceVersion);
	for (const FWBProductionCardRecord& Record : Database.Records)
	{
		const FWBCardDefinition& Definition = Record.CoreDefinition;
		Source += FString::Printf(
			TEXT("id=%s|type=%s|name=%s|category=%s|rules=%s|hp=%d|atk=%d|ar=%d|rl=%d|rr=%d|trap_damage=%d|move=%s|attack=%s:%d|equip=%s:%d|hero=%d:%s"),
			*Definition.CardId,
			*WBProductionCardDatabase::CardTypeToString(Record.Type),
			*Definition.PublicName,
			*Definition.PublicCategory,
			*Definition.PublicRulesText,
			Definition.CharacterStats.HP,
			Definition.CharacterStats.ATK,
			Definition.CharacterStats.AR,
			Definition.CharacterStats.RL,
			Definition.WandStats.RR,
			Definition.TrapDamage,
			*Record.Movement.Pattern,
			*Record.Attack.Pattern,
			Record.Attack.Range,
			*Record.Equip.TargetRequirement,
			Record.Equip.ResonanceRequirement,
			Record.bHeroRole ? 1 : 0,
			*Record.HeroMatchStartPlacement);
		for (const FString& Faction : Definition.PublicFactions)
		{
			Source += TEXT("|faction=") + Faction;
		}
		for (const FString& Tag : Definition.PublicTags)
		{
			Source += TEXT("|tag=") + Tag;
		}
		if (Record.Type == EWBProductionCardType::Hybrid)
		{
			Source += FString::Printf(
				TEXT("|hybrid=%d:%s:%d:%s:%s"),
				Definition.HybridSummon.SacrificeCount,
				*CanonicalHybridToken(
					Definition.HybridSummon.SacrificeRequirement),
				Definition.HybridSummon.WandPaymentCount,
				*CanonicalHybridToken(
					Definition.HybridSummon.HeroDestination),
				*CanonicalHybridToken(
					Definition.HybridSummon.NonHeroDestination));
			for (const FName SourceName : Definition.HybridSummon.WandPaymentSources)
			{
				Source += TEXT(":") + CanonicalHybridToken(SourceName);
			}
		}
		TArray<FWBCardEffectDefinition> Effects = Definition.ActivatedEffects;
		Effects.Sort([](const FWBCardEffectDefinition& A, const FWBCardEffectDefinition& B)
		{
			return A.EffectId < B.EffectId;
		});
		for (const FWBCardEffectDefinition& Effect : Effects)
		{
			Source += TEXT("|") + EffectDigest(Effect);
		}
		TArray<EWBCombatCapability> CombatCapabilities =
			Definition.GrantedCombatCapabilitiesWhileEquipped.Array();
		CombatCapabilities.Sort([](const EWBCombatCapability A, const EWBCombatCapability B)
		{
			return static_cast<uint8>(A) < static_cast<uint8>(B);
		});
		for (const EWBCombatCapability Capability : CombatCapabilities)
		{
			Source += FString::Printf(
				TEXT("|equipped_combat_capability=%d"),
				static_cast<int32>(Capability));
		}
		Source += TEXT("\n");
	}
	return Source;
}

class FProductionCardDBLoader
{
public:
	FProductionCardDBLoader(
		const FString& InRootManifestPath,
		const FString& InSuiteRootDirectory)
		: RootManifestPath(InRootManifestPath)
		, SuiteRootDirectory(FPaths::ConvertRelativePathToFull(InSuiteRootDirectory))
	{
		FPaths::NormalizeDirectoryName(SuiteRootDirectory);
		Result.RootManifestPath = InRootManifestPath;
	}

	FWBProductionCardDatabaseLoadResult Load(const FString& SuiteJson)
	{
		ParseSuite(SuiteJson);
		if (!HasErrors())
		{
			BuildSnapshot();
		}
		SortDiagnostics();
		if (HasErrors())
		{
			Result.bOk = false;
			Result.Reason = TEXT("production_card_database_invalid");
			Result.Snapshot.Reset();
		}
		return Result;
	}

private:
	FString RootManifestPath;
	FString SuiteRootDirectory;
	FWBProductionCardDatabaseLoadResult Result;
	FWBProductionCardDatabase WorkingDatabase;
	TSet<FString> VisitingManifests;
	TSet<FString> VisitedManifests;
	TSet<FString> ManifestIds;
	TMap<FString, FString> BundleOwners;
	TArray<FBundleReference> BundleReferences;
	TArray<FWBProductionCardRecord> ParsedRecords;
	FString ExpectedCardDBVersion;
	FString ExpectedSourceVersion;
	FString BundleLockRelativePath;
	FString MatchStatusRelativePath;

	void AddDiagnostic(
		const EWBProductionCardDBDiagnosticSeverity Severity,
		const FString& Code,
		const FString& ManifestPath,
		const FString& DefinitionId,
		const FString& FieldPath,
		const FString& Message,
		const FString& RecommendedAction)
	{
		FWBProductionCardDBDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.ManifestPath = ManifestPath;
		Diagnostic.DefinitionId = DefinitionId;
		Diagnostic.FieldPath = FieldPath;
		Diagnostic.Message = Message;
		Diagnostic.RecommendedAction = RecommendedAction;
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
	}

	void AddError(
		const FString& Code,
		const FString& ManifestPath,
		const FString& DefinitionId,
		const FString& FieldPath,
		const FString& Message,
		const FString& RecommendedAction = FString())
	{
		AddDiagnostic(
			EWBProductionCardDBDiagnosticSeverity::Error,
			Code,
			ManifestPath,
			DefinitionId,
			FieldPath,
			Message,
			RecommendedAction);
	}

	bool HasErrors() const
	{
		for (const FWBProductionCardDBDiagnostic& Diagnostic : Result.Diagnostics)
		{
			if (Diagnostic.Severity == EWBProductionCardDBDiagnosticSeverity::Error)
			{
				return true;
			}
		}
		return false;
	}

	void ValidateKnownFields(
		const TSharedPtr<FJsonObject>& Object,
		const TArray<FString>& KnownFields,
		const FString& ManifestPath,
		const FString& DefinitionId,
		const FString& FieldPath)
	{
		TArray<FString> UnknownFields;
		IsKnownField(Object, KnownFields, UnknownFields);
		for (const FString& UnknownField : UnknownFields)
		{
			AddError(
				TEXT("unknown_field"),
				ManifestPath,
				DefinitionId,
				FieldPath + TEXT(".") + UnknownField,
				TEXT("The production schema is closed and does not accept this field."),
				TEXT("Remove the field or add it to a versioned schema revision."));
		}
	}

	void ValidateMetadataField(
		const TSharedPtr<FJsonObject>& Parent,
		const FString& ManifestPath,
		const FString& FieldPath)
	{
		if (!HasField(Parent, TEXT("metadata")))
		{
			return;
		}
		TSharedPtr<FJsonObject> Metadata;
		if (!TryReadObject(Parent, TEXT("metadata"), Metadata))
		{
			AddError(
				TEXT("metadata_malformed"),
				ManifestPath,
				FString(),
				FieldPath,
				TEXT("Production metadata must be an object."));
			return;
		}
		ValidateKnownFields(
			Metadata,
			{
				TEXT("author"),
				TEXT("notes"),
				TEXT("source"),
				TEXT("version"),
				TEXT("test_only")
			},
			ManifestPath,
			FString(),
			FieldPath);
	}

	bool ReadControlFilePath(
		const TSharedPtr<FJsonObject>& Suite,
		const FString& Field,
		FString& OutPath)
	{
		if (!HasField(Suite, Field))
		{
			return true;
		}

		TSharedPtr<FJsonObject> Entry;
		if (!TryReadObject(Suite, Field, Entry))
		{
			AddError(
				TEXT("suite_control_file_malformed"),
				RootManifestPath,
				FString(),
				TEXT("$.") + Field,
				TEXT("Suite control-file entries must be objects."));
			return false;
		}
		ValidateKnownFields(
			Entry,
			{ TEXT("path") },
			RootManifestPath,
			FString(),
			TEXT("$.") + Field);

		FString Path;
		if (!TryReadString(Entry, TEXT("path"), Path)
			|| !WBProductionCardDatabase::IsSafeRepositoryRelativePath(Path))
		{
			AddError(
				TEXT("suite_control_file_path_invalid"),
				RootManifestPath,
				FString(),
				TEXT("$.") + Field + TEXT(".path"),
				TEXT("Suite control-file paths must remain inside the suite root."));
			return false;
		}
		OutPath = NormalizeRelativePath(Path);
		return true;
	}

	bool LoadJsonFile(
		const FString& RelativePath,
		FString& OutJson)
	{
		if (!WBProductionCardDatabase::IsSafeRepositoryRelativePath(RelativePath))
		{
			return false;
		}
		const FString AbsolutePath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(SuiteRootDirectory, RelativePath));
		return FFileHelper::LoadFileToString(OutJson, *AbsolutePath);
	}

	void ParseSuite(const FString& Json)
	{
		TSharedPtr<FJsonObject> Suite;
		if (!ParseJsonObject(Json, Suite))
		{
			AddError(
				TEXT("suite_json_parse_failed"),
				RootManifestPath,
				FString(),
				TEXT("$"),
				TEXT("The root CardDB manifest suite is not valid JSON."));
			return;
		}

		ValidateKnownFields(
			Suite,
			{
				TEXT("suite_schema_version"),
				TEXT("suite_id"),
				TEXT("bundle_kind"),
				TEXT("manifests"),
				TEXT("bundle_lock"),
				TEXT("match_status"),
				TEXT("metadata")
			},
			RootManifestPath,
			FString(),
			TEXT("$"));
		ValidateMetadataField(
			Suite,
			RootManifestPath,
			TEXT("$.metadata"));

		int32 SchemaVersion = 0;
		if (!TryReadInteger(Suite, TEXT("suite_schema_version"), SchemaVersion)
			|| SchemaVersion != SupportedSchemaVersion)
		{
			AddError(
				TEXT("suite_schema_version_unsupported"),
				RootManifestPath,
				FString(),
				TEXT("$.suite_schema_version"),
				TEXT("The suite schema version is missing or unsupported."));
		}

		if (!TryReadString(Suite, TEXT("suite_id"), WorkingDatabase.SuiteId)
			|| !WBProductionCardDatabase::IsSafeDefinitionId(WorkingDatabase.SuiteId))
		{
			AddError(
				TEXT("suite_id_invalid"),
				RootManifestPath,
				FString(),
				TEXT("$.suite_id"),
				TEXT("The suite id must use the canonical lowercase identifier policy."));
		}

		FString BundleKind;
		if (!TryReadString(Suite, TEXT("bundle_kind"), BundleKind)
			|| (WorkingDatabase.BundleKind = ParseBundleKind(BundleKind))
				== EWBProductionBundleKind::Unknown)
		{
			AddError(
				TEXT("bundle_kind_invalid"),
				RootManifestPath,
				FString(),
				TEXT("$.bundle_kind"),
				TEXT("The suite must be classified as production, test, or development."));
		}
		ReadControlFilePath(
			Suite,
			TEXT("bundle_lock"),
			BundleLockRelativePath);
		ReadControlFilePath(
			Suite,
			TEXT("match_status"),
			MatchStatusRelativePath);

		const TArray<TSharedPtr<FJsonValue>>* Manifests = nullptr;
		if (!TryReadArray(Suite, TEXT("manifests"), Manifests)
			|| Manifests->IsEmpty())
		{
			AddError(
				TEXT("suite_manifests_malformed"),
				RootManifestPath,
				FString(),
				TEXT("$.manifests"),
				TEXT("The suite must explicitly list at least one manifest."));
			return;
		}

		TArray<FString> ManifestPaths;
		TSet<FString> ManifestNames;
		for (int32 Index = 0; Index < Manifests->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Manifests)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(
					TEXT("suite_manifest_entry_malformed"),
					RootManifestPath,
					FString(),
					FString::Printf(TEXT("$.manifests[%d]"), Index),
					TEXT("Manifest entries must be objects."));
				continue;
			}

			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			ValidateKnownFields(
				Entry,
				{ TEXT("name"), TEXT("path") },
				RootManifestPath,
				FString(),
				FString::Printf(TEXT("$.manifests[%d]"), Index));

			FString Name;
			FString Path;
			if (!TryReadString(Entry, TEXT("name"), Name) || Name.IsEmpty())
			{
				AddError(
					TEXT("suite_manifest_name_missing"),
					RootManifestPath,
					FString(),
					FString::Printf(TEXT("$.manifests[%d].name"), Index),
					TEXT("Every manifest entry needs a stable name."));
			}
			else if (ManifestNames.Contains(Name))
			{
				AddError(
					TEXT("suite_manifest_name_duplicate"),
					RootManifestPath,
					FString(),
					FString::Printf(TEXT("$.manifests[%d].name"), Index),
					TEXT("Manifest names must be unique."));
			}
			else
			{
				ManifestNames.Add(Name);
			}

			if (!TryReadString(Entry, TEXT("path"), Path)
				|| !WBProductionCardDatabase::IsSafeRepositoryRelativePath(Path))
			{
				AddError(
					TEXT("manifest_path_invalid"),
					RootManifestPath,
					FString(),
					FString::Printf(TEXT("$.manifests[%d].path"), Index),
					TEXT("Manifest paths must remain repository-relative and inside the suite root."));
				continue;
			}
			ManifestPaths.Add(NormalizeRelativePath(Path));
		}

		ManifestPaths.Sort();
		for (const FString& ManifestPath : ManifestPaths)
		{
			ParseManifestRecursive(ManifestPath);
		}

		BundleReferences.Sort([](const FBundleReference& A, const FBundleReference& B)
		{
			if (A.RelativePath != B.RelativePath)
			{
				return A.RelativePath < B.RelativePath;
			}
			return A.OwnerManifestPath < B.OwnerManifestPath;
		});
		for (const FBundleReference& Bundle : BundleReferences)
		{
			ParseBundle(Bundle);
		}
	}

	void ParseManifestRecursive(const FString& RelativePath)
	{
		if (VisitedManifests.Contains(RelativePath))
		{
			return;
		}
		if (VisitingManifests.Contains(RelativePath))
		{
			AddError(
				TEXT("manifest_include_cycle"),
				RelativePath,
				FString(),
				TEXT("$.includes"),
				TEXT("A cyclic manifest include was detected."),
				TEXT("Remove the cycle and keep one explicit ownership path."));
			return;
		}

		VisitingManifests.Add(RelativePath);
		FString Json;
		if (!LoadJsonFile(RelativePath, Json))
		{
			AddError(
				TEXT("manifest_not_found"),
				RelativePath,
				FString(),
				TEXT("$"),
				TEXT("An explicitly included manifest could not be loaded."));
			VisitingManifests.Remove(RelativePath);
			return;
		}

		TSharedPtr<FJsonObject> Manifest;
		if (!ParseJsonObject(Json, Manifest))
		{
			AddError(
				TEXT("manifest_json_parse_failed"),
				RelativePath,
				FString(),
				TEXT("$"),
				TEXT("The manifest is not valid JSON."));
			VisitingManifests.Remove(RelativePath);
			return;
		}

		ValidateKnownFields(
			Manifest,
			{
				TEXT("manifest_schema_version"),
				TEXT("manifest_id"),
				TEXT("bundle_kind"),
				TEXT("includes"),
				TEXT("batches"),
				TEXT("metadata")
			},
			RelativePath,
			FString(),
			TEXT("$"));
		ValidateMetadataField(
			Manifest,
			RelativePath,
			TEXT("$.metadata"));

		int32 SchemaVersion = 0;
		if (!TryReadInteger(Manifest, TEXT("manifest_schema_version"), SchemaVersion)
			|| SchemaVersion != SupportedSchemaVersion)
		{
			AddError(
				TEXT("manifest_schema_version_unsupported"),
				RelativePath,
				FString(),
				TEXT("$.manifest_schema_version"),
				TEXT("The manifest schema version is missing or unsupported."));
		}

		FString ManifestId;
		if (!TryReadString(Manifest, TEXT("manifest_id"), ManifestId)
			|| !WBProductionCardDatabase::IsSafeDefinitionId(ManifestId))
		{
			AddError(
				TEXT("manifest_id_invalid"),
				RelativePath,
				FString(),
				TEXT("$.manifest_id"),
				TEXT("The manifest id is missing or malformed."));
		}
		else if (ManifestIds.Contains(ManifestId))
		{
			AddError(
				TEXT("manifest_id_duplicate"),
				RelativePath,
				FString(),
				TEXT("$.manifest_id"),
				TEXT("Manifest ids must be unique across the suite."));
		}
		else
		{
			ManifestIds.Add(ManifestId);
		}

		FString BundleKindValue;
		const EWBProductionBundleKind ManifestKind =
			TryReadString(Manifest, TEXT("bundle_kind"), BundleKindValue)
				? ParseBundleKind(BundleKindValue)
				: EWBProductionBundleKind::Unknown;
		if (ManifestKind != WorkingDatabase.BundleKind)
		{
			AddError(
				TEXT("bundle_kind_mismatch"),
				RelativePath,
				FString(),
				TEXT("$.bundle_kind"),
				TEXT("Manifest classification must match the root suite."));
		}

		TArray<FString> IncludedPaths;
		if (HasField(Manifest, TEXT("includes")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Includes = nullptr;
			if (!TryReadArray(Manifest, TEXT("includes"), Includes))
			{
				AddError(
					TEXT("manifest_includes_malformed"),
					RelativePath,
					FString(),
					TEXT("$.includes"),
					TEXT("Manifest includes must be an array."));
			}
			else
			{
				TSet<FString> IncludeNames;
				for (int32 Index = 0; Index < Includes->Num(); ++Index)
				{
					const TSharedPtr<FJsonValue>& Value = (*Includes)[Index];
					if (!Value.IsValid() || Value->Type != EJson::Object)
					{
						AddError(
							TEXT("manifest_include_malformed"),
							RelativePath,
							FString(),
							FString::Printf(TEXT("$.includes[%d]"), Index),
							TEXT("Manifest include entries must be objects."));
						continue;
					}
					const TSharedPtr<FJsonObject> Include = Value->AsObject();
					ValidateKnownFields(
						Include,
						{ TEXT("name"), TEXT("path") },
						RelativePath,
						FString(),
						FString::Printf(TEXT("$.includes[%d]"), Index));
					FString Name;
					FString Path;
					if (!TryReadString(Include, TEXT("name"), Name)
						|| Name.IsEmpty()
						|| IncludeNames.Contains(Name))
					{
						AddError(
							TEXT("manifest_include_name_invalid"),
							RelativePath,
							FString(),
							FString::Printf(TEXT("$.includes[%d].name"), Index),
							TEXT("Include names must be non-empty and unique within a manifest."));
					}
					else
					{
						IncludeNames.Add(Name);
					}
					if (!TryReadString(Include, TEXT("path"), Path)
						|| !WBProductionCardDatabase::IsSafeRepositoryRelativePath(Path))
					{
						AddError(
							TEXT("manifest_path_invalid"),
							RelativePath,
							FString(),
							FString::Printf(TEXT("$.includes[%d].path"), Index),
							TEXT("Included manifest paths must remain inside the suite root."));
						continue;
					}
					IncludedPaths.Add(NormalizeRelativePath(Path));
				}
			}
		}

		ParseManifestBatches(Manifest, RelativePath);
		WorkingDatabase.IncludedManifestPaths.Add(RelativePath);

		IncludedPaths.Sort();
		for (const FString& IncludedPath : IncludedPaths)
		{
			ParseManifestRecursive(IncludedPath);
		}

		VisitingManifests.Remove(RelativePath);
		VisitedManifests.Add(RelativePath);
	}

	void ParseManifestBatches(
		const TSharedPtr<FJsonObject>& Manifest,
		const FString& ManifestPath)
	{
		const TArray<TSharedPtr<FJsonValue>>* Batches = nullptr;
		if (!TryReadArray(Manifest, TEXT("batches"), Batches)
			|| Batches->IsEmpty())
		{
			AddError(
				TEXT("manifest_batches_malformed"),
				ManifestPath,
				FString(),
				TEXT("$.batches"),
				TEXT("A production manifest must contain at least one explicit batch."));
			return;
		}

		TSet<FString> BatchNames;
		TSet<FString> BundleNames;
		for (int32 BatchIndex = 0; BatchIndex < Batches->Num(); ++BatchIndex)
		{
			const TSharedPtr<FJsonValue>& BatchValue = (*Batches)[BatchIndex];
			if (!BatchValue.IsValid() || BatchValue->Type != EJson::Object)
			{
				AddError(
					TEXT("manifest_batch_malformed"),
					ManifestPath,
					FString(),
					FString::Printf(TEXT("$.batches[%d]"), BatchIndex),
					TEXT("Manifest batches must be objects."));
				continue;
			}
			const TSharedPtr<FJsonObject> Batch = BatchValue->AsObject();
			ValidateKnownFields(
				Batch,
				{ TEXT("batch_name"), TEXT("bundles") },
				ManifestPath,
				FString(),
				FString::Printf(TEXT("$.batches[%d]"), BatchIndex));

			FString BatchName;
			if (!TryReadString(Batch, TEXT("batch_name"), BatchName)
				|| BatchName.IsEmpty()
				|| BatchNames.Contains(BatchName))
			{
				AddError(
					TEXT("manifest_batch_name_invalid"),
					ManifestPath,
					FString(),
					FString::Printf(TEXT("$.batches[%d].batch_name"), BatchIndex),
					TEXT("Batch names must be non-empty and unique."));
			}
			else
			{
				BatchNames.Add(BatchName);
			}

			const TArray<TSharedPtr<FJsonValue>>* Bundles = nullptr;
			if (!TryReadArray(Batch, TEXT("bundles"), Bundles)
				|| Bundles->IsEmpty())
			{
				AddError(
					TEXT("manifest_bundles_malformed"),
					ManifestPath,
					FString(),
					FString::Printf(TEXT("$.batches[%d].bundles"), BatchIndex),
					TEXT("Every batch must explicitly list at least one bundle."));
				continue;
			}

			for (int32 BundleIndex = 0; BundleIndex < Bundles->Num(); ++BundleIndex)
			{
				const TSharedPtr<FJsonValue>& BundleValue = (*Bundles)[BundleIndex];
				if (!BundleValue.IsValid() || BundleValue->Type != EJson::Object)
				{
					AddError(
						TEXT("manifest_bundle_malformed"),
						ManifestPath,
						FString(),
						FString::Printf(
							TEXT("$.batches[%d].bundles[%d]"),
							BatchIndex,
							BundleIndex),
						TEXT("Bundle entries must be objects."));
					continue;
				}
				const TSharedPtr<FJsonObject> Bundle = BundleValue->AsObject();
				ValidateKnownFields(
					Bundle,
					{ TEXT("name"), TEXT("path") },
					ManifestPath,
					FString(),
					FString::Printf(
						TEXT("$.batches[%d].bundles[%d]"),
						BatchIndex,
						BundleIndex));

				FString Name;
				FString Path;
				if (!TryReadString(Bundle, TEXT("name"), Name)
					|| Name.IsEmpty()
					|| BundleNames.Contains(Name))
				{
					AddError(
						TEXT("manifest_bundle_name_invalid"),
						ManifestPath,
						FString(),
						FString::Printf(
							TEXT("$.batches[%d].bundles[%d].name"),
							BatchIndex,
							BundleIndex),
						TEXT("Bundle names must be non-empty and unique within a manifest."));
				}
				else
				{
					BundleNames.Add(Name);
				}

				if (!TryReadString(Bundle, TEXT("path"), Path)
					|| !WBProductionCardDatabase::IsSafeRepositoryRelativePath(Path))
				{
					AddError(
						TEXT("bundle_path_invalid"),
						ManifestPath,
						FString(),
						FString::Printf(
							TEXT("$.batches[%d].bundles[%d].path"),
							BatchIndex,
							BundleIndex),
						TEXT("Bundle paths must remain repository-relative and inside the suite root."));
					continue;
				}
				Path = NormalizeRelativePath(Path);
				if (const FString* ExistingOwner = BundleOwners.Find(Path))
				{
					AddError(
						TEXT("duplicate_manifest_ownership"),
						ManifestPath,
						FString(),
						FString::Printf(
							TEXT("$.batches[%d].bundles[%d].path"),
							BatchIndex,
							BundleIndex),
						FString::Printf(
							TEXT("The bundle is already owned by manifest '%s'."),
							**ExistingOwner),
						TEXT("Give each bundle exactly one owning manifest."));
					continue;
				}
				BundleOwners.Add(Path, ManifestPath);
				FBundleReference Reference;
				Reference.Name = Name;
				Reference.RelativePath = Path;
				Reference.OwnerManifestPath = ManifestPath;
				BundleReferences.Add(MoveTemp(Reference));
			}
		}
	}

	void ParseBundle(const FBundleReference& Reference)
	{
		FString Json;
		if (!LoadJsonFile(Reference.RelativePath, Json))
		{
			AddError(
				TEXT("bundle_not_found"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath,
				TEXT("An explicitly owned bundle could not be loaded."));
			return;
		}

		TSharedPtr<FJsonObject> Bundle;
		if (!ParseJsonObject(Json, Bundle))
		{
			AddError(
				TEXT("bundle_json_parse_failed"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath,
				TEXT("The bundle is not valid JSON."));
			return;
		}

		ValidateKnownFields(
			Bundle,
			{
				TEXT("bundle_schema_version"),
				TEXT("carddb_version"),
				TEXT("source_version"),
				TEXT("bundle_kind"),
				TEXT("metadata"),
				TEXT("cards")
			},
			Reference.OwnerManifestPath,
			FString(),
			Reference.RelativePath);

		int32 SchemaVersion = 0;
		if (!TryReadInteger(Bundle, TEXT("bundle_schema_version"), SchemaVersion)
			|| SchemaVersion != SupportedSchemaVersion)
		{
			AddError(
				TEXT("bundle_schema_version_unsupported"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath + TEXT(".bundle_schema_version"),
				TEXT("The bundle schema version is missing or unsupported."));
		}

		FString CardDBVersion;
		FString SourceVersion;
		if (!TryReadString(Bundle, TEXT("carddb_version"), CardDBVersion)
			|| CardDBVersion.IsEmpty())
		{
			AddError(
				TEXT("carddb_version_missing"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath + TEXT(".carddb_version"),
				TEXT("Production bundles require a CardDB version."));
		}
		if (!TryReadString(Bundle, TEXT("source_version"), SourceVersion)
			|| SourceVersion.IsEmpty())
		{
			AddError(
				TEXT("source_version_missing"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath + TEXT(".source_version"),
				TEXT("Production bundles require source provenance versioning."));
		}

		if (ExpectedCardDBVersion.IsEmpty())
		{
			ExpectedCardDBVersion = CardDBVersion;
			ExpectedSourceVersion = SourceVersion;
		}
		else if (CardDBVersion != ExpectedCardDBVersion
			|| SourceVersion != ExpectedSourceVersion)
		{
			AddError(
				TEXT("bundle_version_mismatch"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath,
				TEXT("All bundles in one immutable snapshot must share CardDB and source versions."));
		}

		FString BundleKindValue;
		const EWBProductionBundleKind BundleKind =
			TryReadString(Bundle, TEXT("bundle_kind"), BundleKindValue)
				? ParseBundleKind(BundleKindValue)
				: EWBProductionBundleKind::Unknown;
		if (BundleKind != WorkingDatabase.BundleKind)
		{
			AddError(
				TEXT("bundle_kind_mismatch"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath + TEXT(".bundle_kind"),
				TEXT("Bundle classification must match its manifest suite."));
		}

		ValidateMetadataField(
			Bundle,
			Reference.OwnerManifestPath,
			Reference.RelativePath + TEXT(".metadata"));

		const TArray<TSharedPtr<FJsonValue>>* Cards = nullptr;
		if (!TryReadArray(Bundle, TEXT("cards"), Cards)
			|| Cards->IsEmpty())
		{
			AddError(
				TEXT("cards_malformed"),
				Reference.OwnerManifestPath,
				FString(),
				Reference.RelativePath + TEXT(".cards"),
				TEXT("A production bundle must contain a non-empty cards array."));
			return;
		}

		for (int32 Index = 0; Index < Cards->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Card = (*Cards)[Index];
			if (!Card.IsValid() || Card->Type != EJson::Object)
			{
				AddError(
					TEXT("card_malformed"),
					Reference.OwnerManifestPath,
					FString(),
					FString::Printf(
						TEXT("%s.cards[%d]"),
						*Reference.RelativePath,
						Index),
					TEXT("Card definitions must be objects."));
				continue;
			}
			ParseCard(
				Card->AsObject(),
				Reference,
				FString::Printf(TEXT("%s.cards[%d]"), *Reference.RelativePath, Index));
		}
		WorkingDatabase.IncludedBundlePaths.Add(Reference.RelativePath);
	}

	void ParseCard(
		const TSharedPtr<FJsonObject>& Card,
		const FBundleReference& Reference,
		const FString& CardPath)
	{
		FString CardId;
		TryReadString(Card, TEXT("card_id"), CardId);
		ValidateKnownFields(
			Card,
			{
				TEXT("schema_version"),
				TEXT("card_id"),
				TEXT("public_name"),
				TEXT("kind"),
				TEXT("public_text"),
				TEXT("public_category"),
				TEXT("factions"),
				TEXT("tags"),
				TEXT("source_manifest"),
				TEXT("stats"),
				TEXT("movement"),
				TEXT("attack"),
				TEXT("hero"),
				TEXT("hybrid_summon"),
				TEXT("trap"),
				TEXT("equip"),
				TEXT("activated_effects")
			},
			Reference.OwnerManifestPath,
			CardId,
			CardPath);

		int32 SchemaVersion = 0;
		if (!TryReadInteger(Card, TEXT("schema_version"), SchemaVersion)
			|| SchemaVersion != SupportedSchemaVersion)
		{
			AddError(
				TEXT("card_schema_version_unsupported"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".schema_version"),
				TEXT("The card schema version is missing or unsupported."));
		}

		if (!WBProductionCardDatabase::IsSafeDefinitionId(CardId))
		{
			AddError(
				TEXT("definition_id_invalid"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".card_id"),
				TEXT("Card ids must be lowercase canonical identifiers."));
		}

		FWBProductionCardRecord Record;
		Record.SourceManifestPath = Reference.OwnerManifestPath;
		Record.SourceBundlePath = Reference.RelativePath;
		Record.CoreDefinition.CardId = CardId;

		if (!TryReadString(Card, TEXT("public_name"), Record.CoreDefinition.PublicName)
			|| Record.CoreDefinition.PublicName.TrimStartAndEnd().IsEmpty())
		{
			AddError(
				TEXT("public_name_missing"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".public_name"),
				TEXT("Every production definition requires a public display name."));
		}
		else
		{
			FString ForbiddenTerm;
			if (WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(
				Record.CoreDefinition.PublicName,
				ForbiddenTerm))
			{
				AddError(
					TEXT("public_label_contains_internal_term"),
					Reference.OwnerManifestPath,
					CardId,
					CardPath + TEXT(".public_name"),
					TEXT("Public labels cannot contain internal engine terminology."));
			}
		}

		FString CardType;
		Record.Type = TryReadString(Card, TEXT("kind"), CardType)
			? ParseCardType(CardType)
			: EWBProductionCardType::Unknown;
		if (Record.Type == EWBProductionCardType::Unknown)
		{
			AddError(
				TEXT("unsupported_card_type"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".kind"),
				TEXT("This card type is not supported by the production runtime snapshot."),
				TEXT("Classify it as deferred until its runtime behavior is implemented."));
		}
		Record.CoreDefinition.Kind = CoreKindFor(Record.Type);

		if (!TryReadString(
			Card,
			TEXT("public_category"),
			Record.CoreDefinition.PublicCategory)
			|| Record.CoreDefinition.PublicCategory.IsEmpty())
		{
			AddError(
				TEXT("public_category_missing"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".public_category"),
				TEXT("Every production definition requires a public category."));
		}

		if (HasField(Card, TEXT("public_text"))
			&& !TryReadString(
				Card,
				TEXT("public_text"),
				Record.CoreDefinition.PublicRulesText))
		{
			AddError(
				TEXT("public_text_malformed"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".public_text"),
				TEXT("Public rules text must be a string."));
		}

		if (HasField(Card, TEXT("factions")))
		{
			if (!ReadStringArray(
				Card,
				TEXT("factions"),
				Record.CoreDefinition.PublicFactions))
			{
				AddError(
					TEXT("factions_malformed"),
					Reference.OwnerManifestPath,
					CardId,
					CardPath + TEXT(".factions"),
					TEXT("Factions must be a deterministic string array."));
			}
			else
			{
				for (const FString& Faction : Record.CoreDefinition.PublicFactions)
				{
					if (!IsCanonicalFaction(Faction))
					{
						AddError(
							TEXT("faction_id_invalid"),
							Reference.OwnerManifestPath,
							CardId,
							CardPath + TEXT(".factions"),
							TEXT("The faction id is not canonical."));
					}
				}
			}
		}

		if (HasField(Card, TEXT("tags")))
		{
			if (!ReadStringArray(Card, TEXT("tags"), Record.CoreDefinition.PublicTags))
			{
				AddError(
					TEXT("tags_malformed"),
					Reference.OwnerManifestPath,
					CardId,
					CardPath + TEXT(".tags"),
					TEXT("Tags must be a deterministic string array."));
			}
			else
			{
				for (const FString& Tag : Record.CoreDefinition.PublicTags)
				{
					if (!IsSafeTag(Tag))
					{
						AddError(
							TEXT("tag_invalid"),
							Reference.OwnerManifestPath,
							CardId,
							CardPath + TEXT(".tags"),
							TEXT("Tags must use lowercase canonical identifiers."));
					}
				}
			}
		}

		FString SourceManifest;
		if (!TryReadString(Card, TEXT("source_manifest"), SourceManifest)
			|| NormalizeRelativePath(SourceManifest) != Reference.OwnerManifestPath)
		{
			AddError(
				TEXT("source_manifest_mismatch"),
				Reference.OwnerManifestPath,
				CardId,
				CardPath + TEXT(".source_manifest"),
				TEXT("Definition provenance must name its one owning manifest."));
		}

		ParseStats(Card, CardPath, Record);
		ParseMovementAndAttack(Card, CardPath, Record);
		ParseHero(Card, CardPath, Record);
		ParseHybrid(Card, CardPath, Record);
		ParseTrap(Card, CardPath, Record);
		ParseEquip(Card, CardPath, Record);
		ParseEffects(Card, CardPath, Record);

		ParsedRecords.Add(MoveTemp(Record));
	}

	void ParseStats(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		TSharedPtr<FJsonObject> Stats;
		if (!TryReadObject(Card, TEXT("stats"), Stats))
		{
			AddError(
				TEXT("stats_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".stats"),
				TEXT("Supported production card types require a stats object."));
			return;
		}

		const bool bUnit = Record.Type == EWBProductionCardType::Character
			|| Record.Type == EWBProductionCardType::Hero
			|| Record.Type == EWBProductionCardType::Hybrid
			|| Record.Type == EWBProductionCardType::NPC;
		const bool bWand = Record.Type == EWBProductionCardType::Wand;
		ValidateKnownFields(
			Stats,
			bUnit
				? TArray<FString>{
					TEXT("hp"),
					TEXT("atk"),
					TEXT("ar"),
					TEXT("rl") }
				: bWand
					? TArray<FString>{ TEXT("rr") }
					: TArray<FString>(),
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			CardPath + TEXT(".stats"));

		if (bUnit)
		{
			const bool bValid =
				TryReadInteger(Stats, TEXT("hp"), Record.CoreDefinition.CharacterStats.HP)
				&& TryReadInteger(Stats, TEXT("atk"), Record.CoreDefinition.CharacterStats.ATK)
				&& TryReadInteger(Stats, TEXT("ar"), Record.CoreDefinition.CharacterStats.AR)
				&& TryReadInteger(Stats, TEXT("rl"), Record.CoreDefinition.CharacterStats.RL)
				&& Record.CoreDefinition.CharacterStats.HP > 0
				&& Record.CoreDefinition.CharacterStats.HP <= 999
				&& Record.CoreDefinition.CharacterStats.ATK >= 0
				&& Record.CoreDefinition.CharacterStats.ATK <= 99
				&& Record.CoreDefinition.CharacterStats.AR >= 0
				&& Record.CoreDefinition.CharacterStats.AR <= 99
				&& Record.CoreDefinition.CharacterStats.RL >= 0
				&& Record.CoreDefinition.CharacterStats.RL <= 99;
			if (!bValid)
			{
				AddError(
					TEXT("invalid_numeric_range"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".stats"),
					TEXT("Unit HP, ATK, AR, and RL values are missing or outside supported ranges."));
			}
		}
		else if (bWand)
		{
			if (!TryReadInteger(Stats, TEXT("rr"), Record.CoreDefinition.WandStats.RR)
				|| Record.CoreDefinition.WandStats.RR < 0
				|| Record.CoreDefinition.WandStats.RR > 99)
			{
				AddError(
					TEXT("invalid_numeric_range"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".stats.rr"),
					TEXT("Wand RR must be an integer from 0 through 99."));
			}
		}
		else if (!Stats->Values.IsEmpty())
		{
			AddError(
				TEXT("contradictory_card_stats"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".stats"),
				TEXT("This card type cannot declare unit or wand stats."));
		}
	}

	void ParseMovementAndAttack(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		const bool bUnit = Record.Type == EWBProductionCardType::Character
			|| Record.Type == EWBProductionCardType::Hero
			|| Record.Type == EWBProductionCardType::Hybrid
			|| Record.Type == EWBProductionCardType::NPC;
		if (!bUnit)
		{
			if (HasField(Card, TEXT("movement")) || HasField(Card, TEXT("attack")))
			{
				AddError(
					TEXT("contradictory_unit_data"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath,
					TEXT("Only unit definitions may declare movement and attack specifications."));
			}
			return;
		}

		TSharedPtr<FJsonObject> Movement;
		if (!TryReadObject(Card, TEXT("movement"), Movement))
		{
			AddError(
				TEXT("movement_pattern_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".movement"),
				TEXT("Unit definitions require an explicit movement specification."));
		}
		else
		{
			ValidateKnownFields(
				Movement,
				{ TEXT("pattern") },
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".movement"));
			if (!TryReadString(Movement, TEXT("pattern"), Record.Movement.Pattern)
				|| Record.Movement.Pattern != TEXT("orthogonal_adjacent"))
			{
				AddError(
					TEXT("movement_pattern_unsupported"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".movement.pattern"),
					TEXT("The current deterministic runtime supports orthogonal adjacent movement only."));
			}
		}

		TSharedPtr<FJsonObject> Attack;
		if (!TryReadObject(Card, TEXT("attack"), Attack))
		{
			AddError(
				TEXT("attack_pattern_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".attack"),
				TEXT("Unit definitions require an explicit attack specification."));
		}
		else
		{
			ValidateKnownFields(
				Attack,
				{ TEXT("pattern"), TEXT("range") },
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".attack"));
			if (!TryReadString(Attack, TEXT("pattern"), Record.Attack.Pattern)
				|| Record.Attack.Pattern != TEXT("orthogonal_line")
				|| !TryReadInteger(Attack, TEXT("range"), Record.Attack.Range)
				|| Record.Attack.Range != Record.CoreDefinition.CharacterStats.AR)
			{
				AddError(
					TEXT("attack_pattern_unsupported"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".attack"),
					TEXT("Attack data must use orthogonal_line with range equal to canonical AR."));
			}
		}
	}

	void ParseHero(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		if (Record.Type != EWBProductionCardType::Hero)
		{
			if (HasField(Card, TEXT("hero")))
			{
				AddError(
					TEXT("invalid_hero_reference"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".hero"),
					TEXT("Only Hero definitions may declare Hero setup data."));
			}
			return;
		}

		TSharedPtr<FJsonObject> Hero;
		FString Role;
		if (!TryReadObject(Card, TEXT("hero"), Hero))
		{
			AddError(
				TEXT("invalid_hero_reference"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".hero"),
				TEXT("Hero definitions require explicit role and match-start placement."));
			return;
		}
		ValidateKnownFields(
			Hero,
			{ TEXT("role"), TEXT("match_start_placement") },
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			CardPath + TEXT(".hero"));
		if (!TryReadString(Hero, TEXT("role"), Role)
			|| Role != TEXT("hero")
			|| !TryReadString(
				Hero,
				TEXT("match_start_placement"),
				Record.HeroMatchStartPlacement)
			|| Record.HeroMatchStartPlacement != TEXT("canonical_hero_spawn"))
		{
			AddError(
				TEXT("invalid_hero_reference"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".hero"),
				TEXT("Hero role and canonical match-start placement are required."));
		}
		else
		{
			Record.bHeroRole = true;
		}
	}

	void ParseHybrid(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		if (Record.Type != EWBProductionCardType::Hybrid)
		{
			if (HasField(Card, TEXT("hybrid_summon")))
			{
				AddError(
					TEXT("hybrid_definition_invalid"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".hybrid_summon"),
					TEXT("Only Hybrid definitions may declare Hybrid summon rules."));
			}
			return;
		}

		TSharedPtr<FJsonObject> Hybrid;
		if (!TryReadObject(Card, TEXT("hybrid_summon"), Hybrid))
		{
			AddError(
				TEXT("hybrid_definition_invalid"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".hybrid_summon"),
				TEXT("Hybrid definitions require explicit sacrifice, payment, and destination rules."));
			return;
		}
		ValidateKnownFields(
			Hybrid,
			{
				TEXT("sacrifice_count"),
				TEXT("sacrifice_requirement"),
				TEXT("wand_payment_count"),
				TEXT("wand_payment_sources"),
				TEXT("hero_destination"),
				TEXT("non_hero_destination")
			},
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			CardPath + TEXT(".hybrid_summon"));

		FString SacrificeRequirement;
		FString HeroDestination;
		FString NonHeroDestination;
		TArray<FString> PaymentSources;
		FWBCardHybridSummonDefinition& Definition =
			Record.CoreDefinition.HybridSummon;
		const bool bValid =
			TryReadInteger(Hybrid, TEXT("sacrifice_count"), Definition.SacrificeCount)
			&& Definition.SacrificeCount == 1
			&& TryReadString(Hybrid, TEXT("sacrifice_requirement"), SacrificeRequirement)
			&& SacrificeRequirement == TEXT("controlled_character")
			&& TryReadInteger(Hybrid, TEXT("wand_payment_count"), Definition.WandPaymentCount)
			&& Definition.WandPaymentCount == 1
			&& ReadStringArray(Hybrid, TEXT("wand_payment_sources"), PaymentSources)
			&& PaymentSources.Num() == 2
			&& PaymentSources.Contains(TEXT("hand"))
			&& PaymentSources.Contains(TEXT("sacrificed_unit"))
			&& TryReadString(Hybrid, TEXT("hero_destination"), HeroDestination)
			&& HeroDestination == TEXT("sacrificed_hero_tile")
			&& TryReadString(Hybrid, TEXT("non_hero_destination"), NonHeroDestination)
			&& NonHeroDestination == TEXT("adjacent_to_hero");
		Definition.SacrificeRequirement = FName(*SacrificeRequirement);
		Definition.HeroDestination = FName(*HeroDestination);
		Definition.NonHeroDestination = FName(*NonHeroDestination);
		Definition.WandPaymentSources.Reset();
		for (const FString& PaymentSource : PaymentSources)
		{
			Definition.WandPaymentSources.Add(FName(*PaymentSource));
		}
		Definition.WandPaymentSources.Sort([](const FName A, const FName B)
		{
			return CanonicalHybridToken(A) < CanonicalHybridToken(B);
		});
		if (!bValid)
		{
			AddError(
				TEXT("hybrid_definition_invalid"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".hybrid_summon"),
				TEXT("The current Hybrid foundation supports the canonical one-Character, one-wand replacement contract only."));
		}
	}

	void ParseEquip(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		if (Record.Type != EWBProductionCardType::Wand)
		{
			if (HasField(Card, TEXT("equip")))
			{
				AddError(
					TEXT("contradictory_equip_data"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".equip"),
					TEXT("Only Wand definitions may declare equip data."));
			}
			return;
		}

		TSharedPtr<FJsonObject> Equip;
		if (!TryReadObject(Card, TEXT("equip"), Equip))
		{
			AddError(
				TEXT("contradictory_equip_data"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".equip"),
				TEXT("Wand definitions require explicit equip eligibility and RR."));
			return;
		}
		ValidateKnownFields(
			Equip,
			{
				TEXT("target_requirement"),
				TEXT("resonance_requirement"),
				TEXT("combat_capabilities")
			},
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			CardPath + TEXT(".equip"));
		if (!TryReadString(
				Equip,
				TEXT("target_requirement"),
				Record.Equip.TargetRequirement)
			|| Record.Equip.TargetRequirement != TEXT("owned_unit")
			|| !TryReadInteger(
				Equip,
				TEXT("resonance_requirement"),
				Record.Equip.ResonanceRequirement)
			|| Record.Equip.ResonanceRequirement
				!= Record.CoreDefinition.WandStats.RR)
		{
			AddError(
				TEXT("contradictory_equip_data"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".equip"),
				TEXT("Equip data must target an owned unit and match the Wand RR."));
		}
		if (HasField(Equip, TEXT("combat_capabilities")))
		{
			TArray<FString> CapabilityNames;
			if (!ReadStringArray(Equip, TEXT("combat_capabilities"), CapabilityNames))
			{
				AddError(
					TEXT("combat_capabilities_malformed"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".equip.combat_capabilities"),
					TEXT("Equip combat capabilities must be a deterministic string array."));
			}
			else
			{
				for (const FString& CapabilityName : CapabilityNames)
				{
					if (CapabilityName == TEXT("attacks_cannot_be_countered"))
					{
						Record.CoreDefinition.GrantedCombatCapabilitiesWhileEquipped.Add(
							EWBCombatCapability::AttacksCannotBeCountered);
					}
					else
					{
						AddError(
							TEXT("combat_capability_unsupported"),
							Record.SourceManifestPath,
							Record.CoreDefinition.CardId,
							CardPath + TEXT(".equip.combat_capabilities"),
							TEXT("The equip combat capability is not supported by this schema version."));
					}
				}
			}
		}
	}

	void ParseTrap(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		if (Record.Type != EWBProductionCardType::Trap)
		{
			if (HasField(Card, TEXT("trap")))
			{
				AddError(
					TEXT("contradictory_trap_data"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					CardPath + TEXT(".trap"),
					TEXT("Only Trap definitions may declare Trap behavior."));
			}
			return;
		}

		TSharedPtr<FJsonObject> Trap;
		if (!TryReadObject(Card, TEXT("trap"), Trap))
		{
			AddError(
				TEXT("trap_behavior_missing"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".trap"),
				TEXT("Production Traps require explicit supported behavior."));
			return;
		}
		ValidateKnownFields(
			Trap,
			{ TEXT("damage") },
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			CardPath + TEXT(".trap"));
		if (!TryReadInteger(
			Trap,
			TEXT("damage"),
			Record.CoreDefinition.TrapDamage)
			|| Record.CoreDefinition.TrapDamage <= 0
			|| Record.CoreDefinition.TrapDamage > 99)
		{
			AddError(
				TEXT("trap_behavior_unsupported"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".trap.damage"),
				TEXT("Trap damage must be a supported positive integer."));
		}
	}

	void ParseEffects(
		const TSharedPtr<FJsonObject>& Card,
		const FString& CardPath,
		FWBProductionCardRecord& Record)
	{
		if (!HasField(Card, TEXT("activated_effects")))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Effects = nullptr;
		if (!TryReadArray(Card, TEXT("activated_effects"), Effects))
		{
			AddError(
				TEXT("activated_effects_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				CardPath + TEXT(".activated_effects"),
				TEXT("Activated effects must be an array."));
			return;
		}

		TSet<FString> EffectIds;
		for (int32 Index = 0; Index < Effects->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Effects)[Index];
			const FString EffectPath = FString::Printf(
				TEXT("%s.activated_effects[%d]"),
				*CardPath,
				Index);
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(
					TEXT("effect_malformed"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath,
					TEXT("Activated effect entries must be objects."));
				continue;
			}
			const TSharedPtr<FJsonObject> EffectObject = Value->AsObject();
			ValidateKnownFields(
				EffectObject,
				{
					TEXT("effect_id"),
					TEXT("public_label"),
					TEXT("target_requirement"),
					TEXT("source_gate"),
					TEXT("payloads")
				},
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath);

			FWBCardEffectDefinition Effect;
			if (!TryReadString(EffectObject, TEXT("effect_id"), Effect.EffectId)
				|| !WBProductionCardDatabase::IsSafeDefinitionId(Effect.EffectId))
			{
				AddError(
					TEXT("effect_id_invalid"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".effect_id"),
					TEXT("Effect ids must use canonical lowercase identifiers."));
			}
			else if (EffectIds.Contains(Effect.EffectId))
			{
				AddError(
					TEXT("effect_id_duplicate"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".effect_id"),
					TEXT("Effect ids must be unique within a definition."));
			}
			else
			{
				EffectIds.Add(Effect.EffectId);
			}

			if (!TryReadString(EffectObject, TEXT("public_label"), Effect.PublicLabel)
				|| Effect.PublicLabel.IsEmpty())
			{
				AddError(
					TEXT("public_label_missing"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".public_label"),
					TEXT("Every activation requires a public label."));
			}
			else
			{
				FString Forbidden;
				if (WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(
					Effect.PublicLabel,
					Forbidden))
				{
					AddError(
						TEXT("public_label_contains_internal_term"),
						Record.SourceManifestPath,
						Record.CoreDefinition.CardId,
						EffectPath + TEXT(".public_label"),
						TEXT("Public labels cannot expose internal engine terminology."));
				}
			}

			FString TargetRequirement;
			if (!TryReadString(
				EffectObject,
				TEXT("target_requirement"),
				TargetRequirement)
				|| (TargetRequirement != TEXT("unit")
					&& TargetRequirement != TEXT("none")))
			{
				AddError(
					TEXT("invalid_target_requirement"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".target_requirement"),
					TEXT("Production activations support explicit unit targets and control payloads with no public target."));
			}
			Effect.TargetRequirement = ParseTargetRequirement(TargetRequirement);

			ParseSourceGate(EffectObject, EffectPath, Record, Effect);
			ParsePayloads(EffectObject, EffectPath, Record, Effect);
			ValidateEffectCompatibility(EffectPath, Record, Effect);
			Record.CoreDefinition.ActivatedEffects.Add(MoveTemp(Effect));
		}

		Record.CoreDefinition.ActivatedEffects.Sort(
			[](const FWBCardEffectDefinition& A, const FWBCardEffectDefinition& B)
			{
				return A.EffectId < B.EffectId;
			});
	}

	void ParseSourceGate(
		const TSharedPtr<FJsonObject>& EffectObject,
		const FString& EffectPath,
		const FWBProductionCardRecord& Record,
		FWBCardEffectDefinition& Effect)
	{
		TSharedPtr<FJsonObject> GateObject;
		if (!TryReadObject(EffectObject, TEXT("source_gate"), GateObject))
		{
			AddError(
				TEXT("source_gate_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate"),
				TEXT("Every activation requires an explicit source gate."));
			return;
		}

		ValidateKnownFields(
			GateObject,
			{
				TEXT("required_zone"),
				TEXT("timing"),
				TEXT("requires_source_unit"),
				TEXT("requires_source_unit_ownership"),
				TEXT("blocked_by_stunned"),
				TEXT("blocked_by_frozen"),
				TEXT("once_per_turn"),
				TEXT("once_per_turn_key"),
				TEXT("cost_gate")
			},
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			EffectPath + TEXT(".source_gate"));

		Effect.SourceGate.bHasExplicitSourceGate = true;
		Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
		Effect.SourceGate.bRequiresCostsSatisfiedExternally = true;
		Effect.SourceGate.bRequiresSourceUnitOwnership = true;
		Effect.SourceGate.bBlockedByStunned = true;

		FString Zone;
		Effect.SourceGate.RequiredZone =
			TryReadString(GateObject, TEXT("required_zone"), Zone)
				? ParseSourceZone(Zone)
				: EWBCardActivationSourceZone::Unknown;
		if (Effect.SourceGate.RequiredZone != EWBCardActivationSourceZone::Board
			&& Effect.SourceGate.RequiredZone != EWBCardActivationSourceZone::Equipped
			&& Effect.SourceGate.RequiredZone != EWBCardActivationSourceZone::Hand)
		{
			AddError(
				TEXT("unsupported_source_zone"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.required_zone"),
				TEXT("Production match activation supports Board, Equipped, and Hand sources."));
		}

		FString Timing;
		if (!TryReadString(GateObject, TEXT("timing"), Timing))
		{
			AddError(
				TEXT("unsupported_timing"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.timing"),
				TEXT("Activation timing must be normal_turn_priority or response_window."));
			Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::Unknown;
		}
		else if (Timing == TEXT("normal_turn_priority"))
		{
			Effect.SourceGate.Timing =
				EWBCardActivationTimingRequirement::NormalTurnPriority;
		}
		else if (Timing == TEXT("response_window"))
		{
			Effect.SourceGate.Timing =
				EWBCardActivationTimingRequirement::ResponseWindow;
		}
		else
		{
			AddError(
				TEXT("unsupported_timing"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.timing"),
				TEXT("Activation timing must be normal_turn_priority or response_window."));
			Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::Unknown;
		}

		bool BoolValue = false;
		if (!TryReadBool(GateObject, TEXT("requires_source_unit"), BoolValue)
			|| (Effect.SourceGate.RequiredZone == EWBCardActivationSourceZone::Hand
				? BoolValue
				: !BoolValue))
		{
			AddError(
				TEXT("source_gate_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.requires_source_unit"),
				TEXT("Board and Equipped activations require a source unit; Hand activations must not."));
		}
		Effect.SourceGate.bRequiresSourceUnit = BoolValue;

		if (HasField(GateObject, TEXT("requires_source_unit_ownership")))
		{
			if (!TryReadBool(
				GateObject,
				TEXT("requires_source_unit_ownership"),
				Effect.SourceGate.bRequiresSourceUnitOwnership)
				|| !Effect.SourceGate.bRequiresSourceUnitOwnership)
			{
				AddError(
					TEXT("source_gate_malformed"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".source_gate.requires_source_unit_ownership"),
					TEXT("Production activation sources must be owned by the acting player."));
			}
		}
		if (HasField(GateObject, TEXT("blocked_by_stunned"))
			&& !TryReadBool(
				GateObject,
				TEXT("blocked_by_stunned"),
				Effect.SourceGate.bBlockedByStunned))
		{
			AddError(
				TEXT("source_gate_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.blocked_by_stunned"),
				TEXT("blocked_by_stunned must be boolean."));
		}
		if (HasField(GateObject, TEXT("blocked_by_frozen"))
			&& !TryReadBool(
				GateObject,
				TEXT("blocked_by_frozen"),
				Effect.SourceGate.bBlockedByFrozen))
		{
			AddError(
				TEXT("source_gate_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.blocked_by_frozen"),
				TEXT("blocked_by_frozen must be boolean."));
		}
		if (HasField(GateObject, TEXT("once_per_turn")))
		{
			if (!TryReadBool(
				GateObject,
				TEXT("once_per_turn"),
				Effect.SourceGate.bOncePerTurn))
			{
				AddError(
					TEXT("source_gate_malformed"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".source_gate.once_per_turn"),
					TEXT("once_per_turn must be boolean."));
			}
		}
		if (Effect.SourceGate.bOncePerTurn)
		{
			if (!TryReadString(
				GateObject,
				TEXT("once_per_turn_key"),
				Effect.SourceGate.OncePerTurnKey)
				|| !WBProductionCardDatabase::IsSafeDefinitionId(
					Effect.SourceGate.OncePerTurnKey))
			{
				AddError(
					TEXT("usage_key_invalid"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".source_gate.once_per_turn_key"),
					TEXT("Once-per-turn activations require a stable canonical usage key."));
			}
		}
		else if (HasField(GateObject, TEXT("once_per_turn_key")))
		{
			AddError(
				TEXT("contradictory_usage_gate"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.once_per_turn_key"),
				TEXT("A usage key cannot be declared when once_per_turn is false."));
		}

		if (HasField(GateObject, TEXT("cost_gate")))
		{
			TSharedPtr<FJsonObject> CostGate;
			if (!TryReadObject(GateObject, TEXT("cost_gate"), CostGate))
			{
				AddError(
					TEXT("illegal_cost_definition"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".source_gate.cost_gate"),
					TEXT("Cost gates must be objects."));
			}
			else
			{
				ValidateKnownFields(
					CostGate,
					{ TEXT("required_rr"), TEXT("cost_kind") },
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".source_gate.cost_gate"));
				FString CostKind;
				if (!TryReadInteger(
					CostGate,
					TEXT("required_rr"),
					Effect.SourceGate.CostGate.RequiredRR)
					|| Effect.SourceGate.CostGate.RequiredRR < 0
					|| Effect.SourceGate.CostGate.RequiredRR > 99
					|| !TryReadString(CostGate, TEXT("cost_kind"), CostKind)
					|| CostKind != TEXT("RR"))
				{
					AddError(
						TEXT("illegal_cost_definition"),
						Record.SourceManifestPath,
						Record.CoreDefinition.CardId,
						EffectPath + TEXT(".source_gate.cost_gate"),
						TEXT("Activation costs must be a supported non-negative RR cost."));
				}
				Effect.SourceGate.CostGate.CostKind = FName(*CostKind);
				Effect.SourceGate.CostGate.bRequiresExternalAffordability =
					Effect.SourceGate.CostGate.RequiredRR > 0;
			}
		}
	}

	void ParsePayloads(
		const TSharedPtr<FJsonObject>& EffectObject,
		const FString& EffectPath,
		const FWBProductionCardRecord& Record,
		FWBCardEffectDefinition& Effect)
	{
		const TArray<TSharedPtr<FJsonValue>>* Payloads = nullptr;
		if (!TryReadArray(EffectObject, TEXT("payloads"), Payloads)
			|| Payloads->IsEmpty())
		{
			AddError(
				TEXT("missing_effect_reference"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".payloads"),
				TEXT("Activated effects must resolve to at least one supported payload."));
			return;
		}

		for (int32 Index = 0; Index < Payloads->Num(); ++Index)
		{
			const FString PayloadPath = FString::Printf(
				TEXT("%s.payloads[%d]"),
				*EffectPath,
				Index);
			const TSharedPtr<FJsonValue>& Value = (*Payloads)[Index];
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				AddError(
					TEXT("payload_malformed"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					PayloadPath,
					TEXT("Effect payloads must be objects."));
				continue;
			}
			const TSharedPtr<FJsonObject> PayloadObject = Value->AsObject();
			FString Type;
			if (!TryReadString(PayloadObject, TEXT("type"), Type))
			{
				AddError(
					TEXT("unsupported_effect"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					PayloadPath + TEXT(".type"),
					TEXT("Effect payload type is missing."));
				continue;
			}

			FWBGenericEffectPayload Payload;
			if (Type == TEXT("damage_effect"))
			{
				ParseDamagePayload(PayloadObject, PayloadPath, Record, Payload);
			}
			else if (Type == TEXT("heal_effect"))
			{
				ParseHealPayload(PayloadObject, PayloadPath, Record, Payload);
			}
			else if (Type == TEXT("status_effect"))
			{
				ParseStatusPayload(PayloadObject, PayloadPath, Record, Payload);
			}
			else if (Type == TEXT("armor_effect"))
			{
				ParseArmorPayload(PayloadObject, PayloadPath, Record, Payload);
			}
			else if (Type == TEXT("negate_pending_effect"))
			{
				ValidateKnownFields(
					PayloadObject,
					{ TEXT("type") },
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					PayloadPath);
				Payload.Operation = EWBGenericEffectOp::NegatePendingEffect;
			}
			else if (Type == TEXT("prevent_pending_attack"))
			{
				ValidateKnownFields(
					PayloadObject,
					{ TEXT("type") },
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					PayloadPath);
				Payload.Operation = EWBGenericEffectOp::PreventPendingAttack;
			}
			else if (Type == TEXT("redirect_pending_attack"))
			{
				ValidateKnownFields(
					PayloadObject,
					{ TEXT("type"), TEXT("target") },
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					PayloadPath);
				ValidateSelectedTarget(
					PayloadObject, PayloadPath, Record);
				Payload.Operation = EWBGenericEffectOp::RedirectPendingAttack;
			}
			else
			{
				AddError(
					TEXT("unsupported_effect"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					PayloadPath + TEXT(".type"),
					TEXT("The payload does not map to a supported EffectRunner operation."),
					TEXT("Keep this definition out of production bundles until the effect is implemented."));
			}
			Effect.Payloads.Add(MoveTemp(Payload));
		}
	}

	bool ValidateSelectedTarget(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Path,
		const FWBProductionCardRecord& Record)
	{
		FString Target;
		if (!TryReadString(Object, TEXT("target"), Target)
			|| Target != TEXT("selected"))
		{
			AddError(
				TEXT("invalid_target_requirement"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".target"),
				TEXT("Supported payloads must use the selected public unit target."));
			return false;
		}
		return true;
	}

	void ParseDamagePayload(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Path,
		const FWBProductionCardRecord& Record,
		FWBGenericEffectPayload& OutPayload)
	{
		ValidateKnownFields(
			Object,
			{
				TEXT("type"),
				TEXT("amount"),
				TEXT("bypass_armor"),
				TEXT("damage_cause"),
				TEXT("target")
			},
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			Path);
		OutPayload.Operation = EWBGenericEffectOp::DamageEffect;
		if (!TryReadInteger(Object, TEXT("amount"), OutPayload.DamageEffect.Amount)
			|| OutPayload.DamageEffect.Amount <= 0)
		{
			AddError(
				TEXT("invalid_numeric_range"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".amount"),
				TEXT("Damage amount must be a positive integer."));
		}
		if (HasField(Object, TEXT("bypass_armor"))
			&& !TryReadBool(
				Object,
				TEXT("bypass_armor"),
				OutPayload.DamageEffect.bBypassArmor))
		{
			AddError(
				TEXT("payload_malformed"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".bypass_armor"),
				TEXT("bypass_armor must be boolean."));
		}
		FString Cause = TEXT("Effect");
		if (HasField(Object, TEXT("damage_cause"))
			&& (!TryReadString(Object, TEXT("damage_cause"), Cause)
				|| Cause != TEXT("Effect")))
		{
			AddError(
				TEXT("unsupported_effect_operation"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".damage_cause"),
				TEXT("Production card damage must use the Effect cause."));
		}
		OutPayload.DamageEffect.DamageCause = FName(*Cause);
		OutPayload.DamageEffect.SourceReason = FName(TEXT("production_carddb"));
		ValidateSelectedTarget(Object, Path, Record);
	}

	void ParseHealPayload(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Path,
		const FWBProductionCardRecord& Record,
		FWBGenericEffectPayload& OutPayload)
	{
		ValidateKnownFields(
			Object,
			{ TEXT("type"), TEXT("amount"), TEXT("target") },
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			Path);
		OutPayload.Operation = EWBGenericEffectOp::HealEffect;
		if (!TryReadInteger(Object, TEXT("amount"), OutPayload.HealEffect.Amount)
			|| OutPayload.HealEffect.Amount <= 0)
		{
			AddError(
				TEXT("invalid_numeric_range"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".amount"),
				TEXT("Heal amount must be a positive integer."));
		}
		OutPayload.HealEffect.SourceReason = FName(TEXT("production_carddb"));
		ValidateSelectedTarget(Object, Path, Record);
	}

	void ParseStatusPayload(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Path,
		const FWBProductionCardRecord& Record,
		FWBGenericEffectPayload& OutPayload)
	{
		ValidateKnownFields(
			Object,
			{
				TEXT("type"),
				TEXT("operation"),
				TEXT("status_id"),
				TEXT("turns_remaining"),
				TEXT("target")
			},
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			Path);
		OutPayload.Operation = EWBGenericEffectOp::StatusEffect;
		FString Operation;
		FString StatusId;
		if (!TryReadString(Object, TEXT("operation"), Operation)
			|| (OutPayload.StatusEffect.Operation = ParseStatusOperation(Operation))
				== EWBStatusEffectOp::Unknown)
		{
			AddError(
				TEXT("unsupported_effect_operation"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".operation"),
				TEXT("The status operation is not supported."));
		}
		if (!TryReadString(Object, TEXT("status_id"), StatusId)
			|| !IsSupportedStatus(FName(*StatusId)))
		{
			AddError(
				TEXT("status_id_invalid"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".status_id"),
				TEXT("The status id is not supported by the canonical status model."));
		}
		OutPayload.StatusEffect.StatusId =
			WBStatusEffect::CanonicalizeStatusId(FName(*StatusId));
		if (!TryReadInteger(
			Object,
			TEXT("turns_remaining"),
			OutPayload.StatusEffect.Duration)
			|| OutPayload.StatusEffect.Duration < 0
			|| OutPayload.StatusEffect.Duration > 99)
		{
			AddError(
				TEXT("invalid_numeric_range"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".turns_remaining"),
				TEXT("Status duration must be an integer from 0 through 99."));
		}
		OutPayload.StatusEffect.SourceReason = FName(TEXT("production_carddb"));
		ValidateSelectedTarget(Object, Path, Record);
	}

	void ParseArmorPayload(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Path,
		const FWBProductionCardRecord& Record,
		FWBGenericEffectPayload& OutPayload)
	{
		ValidateKnownFields(
			Object,
			{ TEXT("type"), TEXT("operation"), TEXT("amount"), TEXT("target") },
			Record.SourceManifestPath,
			Record.CoreDefinition.CardId,
			Path);
		OutPayload.Operation = EWBGenericEffectOp::ArmorEffect;
		FString Operation;
		if (!TryReadString(Object, TEXT("operation"), Operation)
			|| (OutPayload.ArmorEffect.Operation = ParseArmorOperation(Operation))
				== EWBArmorEffectOp::Unknown)
		{
			AddError(
				TEXT("unsupported_effect_operation"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".operation"),
				TEXT("The armor operation is not supported."));
		}
		if (HasField(Object, TEXT("amount"))
			&& (!TryReadInteger(Object, TEXT("amount"), OutPayload.ArmorEffect.Amount)
				|| OutPayload.ArmorEffect.Amount < 0))
		{
			AddError(
				TEXT("invalid_numeric_range"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				Path + TEXT(".amount"),
				TEXT("Armor amount must be a non-negative integer."));
		}
		OutPayload.ArmorEffect.SourceReason = FName(TEXT("production_carddb"));
		ValidateSelectedTarget(Object, Path, Record);
	}

	void ValidateEffectCompatibility(
		const FString& EffectPath,
		const FWBProductionCardRecord& Record,
		const FWBCardEffectDefinition& Effect)
	{
		const bool bUnit = Record.Type == EWBProductionCardType::Character
			|| Record.Type == EWBProductionCardType::Hero
			|| Record.Type == EWBProductionCardType::Hybrid
			|| Record.Type == EWBProductionCardType::NPC;
		if (bUnit
			&& Effect.SourceGate.RequiredZone
				!= EWBCardActivationSourceZone::Board)
		{
			AddError(
				TEXT("effect_card_type_incompatible"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.required_zone"),
				TEXT("Character and Hero activations must originate from the Board."));
		}
		else if (Record.Type == EWBProductionCardType::Wand
			&& Effect.SourceGate.RequiredZone
				!= EWBCardActivationSourceZone::Equipped)
		{
			AddError(
				TEXT("effect_card_type_incompatible"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath + TEXT(".source_gate.required_zone"),
				TEXT("Wand activations must originate from Equipped."));
		}
		else if (Record.Type == EWBProductionCardType::Action
			&& Effect.SourceGate.RequiredZone
				!= EWBCardActivationSourceZone::Hand)
		{
			AddError(
				TEXT("effect_card_type_incompatible"),
				Record.SourceManifestPath,
				Record.CoreDefinition.CardId,
				EffectPath,
				TEXT("Action activations must originate from Hand."));
		}
		if (Effect.TargetRequirement == EWBCardEffectTargetRequirement::None)
		{
			const bool bOnlyPendingControl = !Effect.Payloads.IsEmpty()
				&& !Effect.Payloads.ContainsByPredicate(
					[](const FWBGenericEffectPayload& Payload)
					{
						return Payload.Operation
							!= EWBGenericEffectOp::NegatePendingEffect
							&& Payload.Operation
								!= EWBGenericEffectOp::PreventPendingAttack;
					});
			if (!bOnlyPendingControl)
			{
				AddError(
					TEXT("invalid_target_requirement"),
					Record.SourceManifestPath,
					Record.CoreDefinition.CardId,
					EffectPath + TEXT(".target_requirement"),
					TEXT("Only typed pending-effect or pending-attack control may omit a public target."));
			}
		}
	}

	void BuildSnapshot()
	{
		ParsedRecords.Sort([](
			const FWBProductionCardRecord& A,
			const FWBProductionCardRecord& B)
		{
			return A.CoreDefinition.CardId < B.CoreDefinition.CardId;
		});

		for (int32 Index = 1; Index < ParsedRecords.Num(); ++Index)
		{
			if (ParsedRecords[Index - 1].CoreDefinition.CardId
				.Equals(
					ParsedRecords[Index].CoreDefinition.CardId,
					ESearchCase::IgnoreCase))
			{
				AddError(
					TEXT("duplicate_definition_id"),
					ParsedRecords[Index].SourceManifestPath,
					ParsedRecords[Index].CoreDefinition.CardId,
					TEXT("$.card_id"),
					TEXT("Definition ids must be globally unique under the lowercase case policy."));
			}
		}
		if (ParsedRecords.IsEmpty())
		{
			AddError(
				TEXT("snapshot_empty"),
				RootManifestPath,
				FString(),
				TEXT("$"),
				TEXT("The manifest suite did not produce any definitions."));
			return;
		}
		if (HasErrors())
		{
			return;
		}

		WorkingDatabase.CardDBVersion = ExpectedCardDBVersion;
		WorkingDatabase.SourceVersion = ExpectedSourceVersion;
		WorkingDatabase.Records = ParsedRecords;
		WorkingDatabase.IncludedManifestPaths.Sort();
		WorkingDatabase.IncludedBundlePaths.Sort();

		TArray<FWBCardDefinition> CoreDefinitions;
		CoreDefinitions.Reserve(WorkingDatabase.Records.Num());
		for (const FWBProductionCardRecord& Record : WorkingDatabase.Records)
		{
			CoreDefinitions.Add(Record.CoreDefinition);
		}
		const FWBCardDefinitionRepositoryValidationResult RepositoryResult =
			WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
				WorkingDatabase.SuiteId,
				WorkingDatabase.SourceVersion,
				CoreDefinitions,
				WorkingDatabase.CoreRepository);
		if (!RepositoryResult.bOk)
		{
			AddError(
				TEXT("core_repository_invalid"),
				RootManifestPath,
				FString(),
				TEXT("$"),
				FString::Printf(
					TEXT("The normalized Core repository was rejected: %s."),
					*RepositoryResult.Reason));
			return;
		}

		WorkingDatabase.ContentDigest =
			SHA256String(SnapshotDigestSource(WorkingDatabase));
		if (WorkingDatabase.ContentDigest.IsEmpty())
		{
			AddError(
				TEXT("content_digest_failed"),
				RootManifestPath,
				FString(),
				TEXT("$"),
				TEXT("The immutable snapshot digest could not be calculated."));
			return;
		}
		ValidateBundleLock();
		ValidateMatchStatus();
		if (HasErrors())
		{
			return;
		}

		Result.bOk = true;
		Result.Reason = TEXT("success");
		Result.Snapshot =
			MakeShared<const FWBProductionCardDatabase>(MoveTemp(WorkingDatabase));
	}

	bool IsStrictlySortedUnique(const TArray<FString>& Values) const
	{
		for (int32 Index = 1; Index < Values.Num(); ++Index)
		{
			if (Values[Index - 1] >= Values[Index])
			{
				return false;
			}
		}
		return true;
	}

	bool IsSHA256(const FString& Value) const
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsDigit(Character)
				&& !(Character >= TEXT('a') && Character <= TEXT('f')))
			{
				return false;
			}
		}
		return true;
	}

	void AddControlFileError(
		const FString& Code,
		const FString& RelativePath,
		const FString& FieldPath,
		const FString& Message)
	{
		AddError(
			Code,
			RelativePath,
			FString(),
			FieldPath,
			Message);
	}

	void ValidateBundleLock()
	{
		if (BundleLockRelativePath.IsEmpty())
		{
			return;
		}

		FString Json;
		TSharedPtr<FJsonObject> Lock;
		if (!LoadJsonFile(BundleLockRelativePath, Json)
			|| !ParseJsonObject(Json, Lock))
		{
			AddControlFileError(
				TEXT("bundle_lock_invalid"),
				BundleLockRelativePath,
				TEXT("$"),
				TEXT("The explicit production bundle lock could not be loaded."));
			return;
		}
		ValidateKnownFields(
			Lock,
			{
				TEXT("schema_version"),
				TEXT("root_manifest"),
				TEXT("included_manifests"),
				TEXT("definition_ids"),
				TEXT("definition_count"),
				TEXT("semantic_digest"),
				TEXT("source_provenance"),
				TEXT("transfer_report_digest")
			},
			BundleLockRelativePath,
			FString(),
			TEXT("$"));

		int32 SchemaVersion = 0;
		int32 DefinitionCount = -1;
		FString RootManifest;
		FString SemanticDigest;
		FString TransferReportDigest;
		TArray<FString> IncludedManifests;
		TArray<FString> DefinitionIds;
		TArray<FString> SourceProvenance;
		const bool bFieldsValid =
			TryReadInteger(Lock, TEXT("schema_version"), SchemaVersion)
			&& SchemaVersion == SupportedSchemaVersion
			&& TryReadString(Lock, TEXT("root_manifest"), RootManifest)
			&& ReadStringArray(
				Lock,
				TEXT("included_manifests"),
				IncludedManifests)
			&& ReadStringArray(Lock, TEXT("definition_ids"), DefinitionIds)
			&& TryReadInteger(
				Lock,
				TEXT("definition_count"),
				DefinitionCount)
			&& TryReadString(
				Lock,
				TEXT("semantic_digest"),
				SemanticDigest)
			&& ReadStringArray(
				Lock,
				TEXT("source_provenance"),
				SourceProvenance)
			&& TryReadString(
				Lock,
				TEXT("transfer_report_digest"),
				TransferReportDigest);
		if (!bFieldsValid
			|| RootManifest != FPaths::GetCleanFilename(RootManifestPath)
			|| !IsStrictlySortedUnique(IncludedManifests)
			|| IncludedManifests != WorkingDatabase.IncludedManifestPaths
			|| !IsStrictlySortedUnique(DefinitionIds)
			|| DefinitionIds != WorkingDatabase.GetDefinitionIds()
			|| DefinitionCount != WorkingDatabase.Records.Num()
			|| SemanticDigest != WorkingDatabase.ContentDigest
			|| SourceProvenance.IsEmpty()
			|| !IsStrictlySortedUnique(SourceProvenance)
			|| !IsSHA256(TransferReportDigest))
		{
			AddControlFileError(
				TEXT("bundle_lock_mismatch"),
				BundleLockRelativePath,
				TEXT("$"),
				FString::Printf(
					TEXT("The bundle lock does not match the immutable production snapshot (expected digest %s, actual digest %s)."),
					*SemanticDigest,
					*WorkingDatabase.ContentDigest));
			return;
		}

		WorkingDatabase.BundleLockPath = BundleLockRelativePath;
		WorkingDatabase.LockedTransferReportDigest = TransferReportDigest;
	}

	void ValidateMatchStatus()
	{
		if (MatchStatusRelativePath.IsEmpty())
		{
			return;
		}

		FString Json;
		TSharedPtr<FJsonObject> Status;
		if (!LoadJsonFile(MatchStatusRelativePath, Json)
			|| !ParseJsonObject(Json, Status))
		{
			AddControlFileError(
				TEXT("match_status_invalid"),
				MatchStatusRelativePath,
				TEXT("$"),
				TEXT("The explicit production match status could not be loaded."));
			return;
		}
		ValidateKnownFields(
			Status,
			{
				TEXT("schema_version"),
				TEXT("status"),
				TEXT("reason"),
				TEXT("hero_candidate_ids"),
				TEXT("missing_requirements"),
				TEXT("definition_bundle_digest")
			},
			MatchStatusRelativePath,
			FString(),
			TEXT("$"));

		int32 SchemaVersion = 0;
		FString StatusValue;
		FString Reason;
		FString DefinitionDigest;
		TArray<FString> HeroCandidateIds;
		TArray<FString> MissingRequirements;
		const bool bFieldsValid =
			TryReadInteger(Status, TEXT("schema_version"), SchemaVersion)
			&& SchemaVersion == SupportedSchemaVersion
			&& TryReadString(Status, TEXT("status"), StatusValue)
			&& (StatusValue == TEXT("blocked")
				|| StatusValue == TEXT("ready"))
			&& TryReadString(Status, TEXT("reason"), Reason)
			&& WBProductionCardDatabase::IsSafeDefinitionId(Reason)
			&& ReadStringArray(
				Status,
				TEXT("hero_candidate_ids"),
				HeroCandidateIds)
			&& ReadStringArray(
				Status,
				TEXT("missing_requirements"),
				MissingRequirements)
			&& TryReadString(
				Status,
				TEXT("definition_bundle_digest"),
				DefinitionDigest);
		if (!bFieldsValid
			|| !IsStrictlySortedUnique(HeroCandidateIds)
			|| (StatusValue == TEXT("blocked")
				&& MissingRequirements.IsEmpty())
			|| (StatusValue == TEXT("ready")
				&& !MissingRequirements.IsEmpty())
			|| DefinitionDigest != WorkingDatabase.ContentDigest)
		{
			AddControlFileError(
				TEXT("match_status_mismatch"),
				MatchStatusRelativePath,
				TEXT("$"),
				FString::Printf(
					TEXT("The blocked match status does not match the production snapshot (expected digest %s, actual digest %s)."),
					*DefinitionDigest,
					*WorkingDatabase.ContentDigest));
			return;
		}
		for (const FString& HeroCandidateId : HeroCandidateIds)
		{
			const FWBProductionCardRecord* Record =
				WorkingDatabase.FindCharacter(HeroCandidateId);
			if (Record == nullptr)
			{
				AddControlFileError(
					TEXT("match_status_hero_invalid"),
					MatchStatusRelativePath,
					TEXT("$.hero_candidate_ids"),
					TEXT("Every Hero candidate must be a transferred Character definition."));
				return;
			}
		}

		WorkingDatabase.MatchStatusPath = MatchStatusRelativePath;
		WorkingDatabase.MatchStatus = StatusValue;
		WorkingDatabase.MatchBlockedReason =
			StatusValue == TEXT("blocked") ? Reason : FString();
		WorkingDatabase.HeroCandidateDefinitionIds = MoveTemp(HeroCandidateIds);
	}

	void SortDiagnostics()
	{
		Result.Diagnostics.Sort([](
			const FWBProductionCardDBDiagnostic& A,
			const FWBProductionCardDBDiagnostic& B)
		{
			if (A.Severity != B.Severity)
			{
				return static_cast<uint8>(A.Severity)
					> static_cast<uint8>(B.Severity);
			}
			if (A.ManifestPath != B.ManifestPath)
			{
				return A.ManifestPath < B.ManifestPath;
			}
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
};

TArray<TSharedPtr<FJsonValue>> StringArrayToJson(
	const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	for (const FString& Value : Values)
	{
		JsonValues.Add(MakeShared<FJsonValueString>(Value));
	}
	return JsonValues;
}
}

const FWBProductionCardRecord* FWBProductionCardDatabase::FindRecord(
	const FString& DefinitionId) const
{
	const int32 Index = Algo::LowerBoundBy(
		Records,
		DefinitionId,
		[](const FWBProductionCardRecord& Record)
		{
			return Record.CoreDefinition.CardId;
		});
	return Records.IsValidIndex(Index)
		&& Records[Index].CoreDefinition.CardId == DefinitionId
			? &Records[Index]
			: nullptr;
}

const FWBProductionCardRecord* FWBProductionCardDatabase::FindCharacter(
	const FString& DefinitionId) const
{
	const FWBProductionCardRecord* Record = FindRecord(DefinitionId);
	return Record != nullptr && Record->Type == EWBProductionCardType::Character
		? Record
		: nullptr;
}

const FWBProductionCardRecord* FWBProductionCardDatabase::FindHero(
	const FString& DefinitionId) const
{
	const FWBProductionCardRecord* Record = FindRecord(DefinitionId);
	return Record != nullptr && Record->Type == EWBProductionCardType::Hero
		? Record
		: nullptr;
}

const FWBProductionCardRecord* FWBProductionCardDatabase::FindWand(
	const FString& DefinitionId) const
{
	const FWBProductionCardRecord* Record = FindRecord(DefinitionId);
	return Record != nullptr && Record->Type == EWBProductionCardType::Wand
		? Record
		: nullptr;
}

TArray<FString> FWBProductionCardDatabase::GetDefinitionIds() const
{
	TArray<FString> Ids;
	Ids.Reserve(Records.Num());
	for (const FWBProductionCardRecord& Record : Records)
	{
		Ids.Add(Record.CoreDefinition.CardId);
	}
	return Ids;
}

FWBProductionCardDatabaseLoadResult WBProductionCardDatabase::LoadManifestSuite(
	const FString& RootManifestPath)
{
	const FString ResolvedPath = ResolveInputPath(RootManifestPath);
	FString Json;
	if (ResolvedPath.IsEmpty()
		|| !FFileHelper::LoadFileToString(Json, *ResolvedPath))
	{
		FWBProductionCardDatabaseLoadResult Result;
		Result.Reason = TEXT("production_card_bundle_not_found");
		Result.RootManifestPath = RootManifestPath;
		FWBProductionCardDBDiagnostic Diagnostic;
		Diagnostic.Severity = EWBProductionCardDBDiagnosticSeverity::Error;
		Diagnostic.Code = TEXT("suite_not_found");
		Diagnostic.ManifestPath = RootManifestPath;
		Diagnostic.FieldPath = TEXT("$");
		Diagnostic.Message = TEXT("The explicit CardDB manifest suite could not be loaded.");
		Diagnostic.RecommendedAction =
			TEXT("Provide an existing -WandboundCardBundle path.");
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
		return Result;
	}

	return LoadManifestSuiteFromJsonForTest(
		Json,
		ResolvedPath,
		FPaths::GetPath(ResolvedPath));
}

FWBProductionCardDatabaseLoadResult
WBProductionCardDatabase::LoadManifestSuiteFromJsonForTest(
	const FString& Json,
	const FString& RootManifestPath,
	const FString& SuiteRootDirectory)
{
	FProductionCardDBLoader Loader(RootManifestPath, SuiteRootDirectory);
	return Loader.Load(Json);
}

FString WBProductionCardDatabase::ResolveInputPath(const FString& InputPath)
{
	if (InputPath.IsEmpty())
	{
		return FString();
	}

	FString Candidate = InputPath;
	if (FPaths::IsRelative(Candidate))
	{
		Candidate = FPaths::Combine(FPaths::ProjectDir(), Candidate);
	}
	Candidate = FPaths::ConvertRelativePathToFull(Candidate);
	FPaths::NormalizeFilename(Candidate);
	if (FPaths::FileExists(Candidate))
	{
		return Candidate;
	}

	if (FPaths::IsRelative(InputPath))
	{
		Candidate = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("../../.."), InputPath));
		FPaths::NormalizeFilename(Candidate);
		if (FPaths::FileExists(Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

FString WBProductionCardDatabase::DiagnosticSeverityToString(
	const EWBProductionCardDBDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EWBProductionCardDBDiagnosticSeverity::Info:
		return TEXT("info");
	case EWBProductionCardDBDiagnosticSeverity::Warning:
		return TEXT("warning");
	case EWBProductionCardDBDiagnosticSeverity::Error:
	default:
		return TEXT("error");
	}
}

FString WBProductionCardDatabase::BundleKindToString(
	const EWBProductionBundleKind Kind)
{
	switch (Kind)
	{
	case EWBProductionBundleKind::Production:
		return TEXT("production");
	case EWBProductionBundleKind::Test:
		return TEXT("test");
	case EWBProductionBundleKind::Development:
		return TEXT("development");
	default:
		return TEXT("unknown");
	}
}

FString WBProductionCardDatabase::CardTypeToString(
	const EWBProductionCardType Type)
{
	switch (Type)
	{
	case EWBProductionCardType::Character:
		return TEXT("Character");
	case EWBProductionCardType::Hero:
		return TEXT("Hero");
	case EWBProductionCardType::Hybrid:
		return TEXT("Hybrid");
	case EWBProductionCardType::Wand:
		return TEXT("Wand");
	case EWBProductionCardType::Action:
		return TEXT("Action");
	case EWBProductionCardType::Trap:
		return TEXT("Trap");
	case EWBProductionCardType::NPC:
		return TEXT("NPC");
	default:
		return TEXT("Unknown");
	}
}

FString WBProductionCardDatabase::EffectSupportToString(
	const EWBProductionEffectSupport Support)
{
	switch (Support)
	{
	case EWBProductionEffectSupport::Supported:
		return TEXT("Supported");
	case EWBProductionEffectSupport::Unsupported:
		return TEXT("Unsupported");
	case EWBProductionEffectSupport::Missing:
		return TEXT("Missing");
	case EWBProductionEffectSupport::LegacyOnly:
		return TEXT("LegacyOnly");
	case EWBProductionEffectSupport::DevelopmentOnly:
		return TEXT("DevelopmentOnly");
	default:
		return TEXT("Unsupported");
	}
}

bool WBProductionCardDatabase::IsSafeDefinitionId(const FString& DefinitionId)
{
	if (DefinitionId.Len() < 3 || DefinitionId.Len() > 64)
	{
		return false;
	}
	if (!FChar::IsLower(DefinitionId[0]))
	{
		return false;
	}
	for (const TCHAR Character : DefinitionId)
	{
		if (!(FChar::IsLower(Character)
			|| FChar::IsDigit(Character)
			|| Character == TEXT('_')))
		{
			return false;
		}
	}
	return true;
}

bool WBProductionCardDatabase::IsSafeRepositoryRelativePath(
	const FString& RelativePath)
{
	if (RelativePath.IsEmpty()
		|| FPaths::IsRelative(RelativePath) == false
		|| RelativePath.Contains(TEXT(":")))
	{
		return false;
	}

	const FString Normalized = NormalizeRelativePath(RelativePath);
	TArray<FString> Segments;
	Normalized.ParseIntoArray(Segments, TEXT("/"), false);
	if (Segments.IsEmpty())
	{
		return false;
	}
	for (const FString& Segment : Segments)
	{
		if (Segment.IsEmpty()
			|| Segment == TEXT(".")
			|| Segment == TEXT(".."))
		{
			return false;
		}
	}
	return true;
}

bool WBProductionCardDatabase::SnapshotToCanonicalJson(
	const FWBProductionCardDatabase& Snapshot,
	FString& OutJson)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), SupportedSchemaVersion);
	Root->SetStringField(TEXT("suite_id"), Snapshot.SuiteId);
	Root->SetStringField(TEXT("carddb_version"), Snapshot.CardDBVersion);
	Root->SetStringField(TEXT("source_version"), Snapshot.SourceVersion);
	Root->SetStringField(
		TEXT("bundle_kind"),
		BundleKindToString(Snapshot.BundleKind));
	Root->SetStringField(TEXT("content_digest"), Snapshot.ContentDigest);
	Root->SetArrayField(
		TEXT("included_manifests"),
		StringArrayToJson(Snapshot.IncludedManifestPaths));
	Root->SetArrayField(
		TEXT("included_bundles"),
		StringArrayToJson(Snapshot.IncludedBundlePaths));

	TArray<TSharedPtr<FJsonValue>> Definitions;
	for (const FWBProductionCardRecord& Record : Snapshot.Records)
	{
		const FWBCardDefinition& Definition = Record.CoreDefinition;
		TSharedRef<FJsonObject> Card = MakeShared<FJsonObject>();
		Card->SetStringField(TEXT("card_id"), Definition.CardId);
		Card->SetStringField(TEXT("public_name"), Definition.PublicName);
		Card->SetStringField(TEXT("kind"), CardTypeToString(Record.Type));
		Card->SetStringField(TEXT("public_category"), Definition.PublicCategory);
		Card->SetArrayField(
			TEXT("factions"),
			StringArrayToJson(Definition.PublicFactions));
		Card->SetArrayField(
			TEXT("tags"),
			StringArrayToJson(Definition.PublicTags));
		Card->SetNumberField(TEXT("hp"), Definition.CharacterStats.HP);
		Card->SetNumberField(TEXT("atk"), Definition.CharacterStats.ATK);
		Card->SetNumberField(TEXT("ar"), Definition.CharacterStats.AR);
		Card->SetNumberField(TEXT("rl"), Definition.CharacterStats.RL);
		Card->SetNumberField(TEXT("rr"), Definition.WandStats.RR);
		if (!Definition.GrantedCombatCapabilitiesWhileEquipped.IsEmpty())
		{
			TArray<FString> CapabilityNames;
			if (Definition.GrantedCombatCapabilitiesWhileEquipped.Contains(
				EWBCombatCapability::AttacksCannotBeCountered))
			{
				CapabilityNames.Add(TEXT("attacks_cannot_be_countered"));
			}
			Card->SetArrayField(
				TEXT("equipped_combat_capabilities"),
				StringArrayToJson(CapabilityNames));
		}
		if (Record.Type == EWBProductionCardType::Trap)
		{
			TSharedRef<FJsonObject> Trap = MakeShared<FJsonObject>();
			Trap->SetNumberField(
				TEXT("damage"),
				Definition.TrapDamage);
			Card->SetObjectField(TEXT("trap"), Trap);
		}
		if (Record.Type == EWBProductionCardType::Hybrid)
		{
			TSharedRef<FJsonObject> Hybrid = MakeShared<FJsonObject>();
			Hybrid->SetNumberField(
				TEXT("sacrifice_count"),
				Definition.HybridSummon.SacrificeCount);
			Hybrid->SetStringField(
				TEXT("sacrifice_requirement"),
				CanonicalHybridToken(
					Definition.HybridSummon.SacrificeRequirement));
			Hybrid->SetNumberField(
				TEXT("wand_payment_count"),
				Definition.HybridSummon.WandPaymentCount);
			TArray<FString> PaymentSources;
			for (const FName Source : Definition.HybridSummon.WandPaymentSources)
			{
				PaymentSources.Add(CanonicalHybridToken(Source));
			}
			PaymentSources.Sort();
			Hybrid->SetArrayField(
				TEXT("wand_payment_sources"),
				StringArrayToJson(PaymentSources));
			Hybrid->SetStringField(
				TEXT("hero_destination"),
				CanonicalHybridToken(
					Definition.HybridSummon.HeroDestination));
			Hybrid->SetStringField(
				TEXT("non_hero_destination"),
				CanonicalHybridToken(
					Definition.HybridSummon.NonHeroDestination));
			Card->SetObjectField(TEXT("hybrid_summon"), Hybrid);
		}
		Card->SetStringField(TEXT("movement_pattern"), Record.Movement.Pattern);
		Card->SetStringField(TEXT("attack_pattern"), Record.Attack.Pattern);
		Card->SetNumberField(TEXT("attack_range"), Record.Attack.Range);
		Card->SetStringField(
			TEXT("source_manifest"),
			Record.SourceManifestPath);
		Card->SetStringField(TEXT("source_bundle"), Record.SourceBundlePath);
		Definitions.Add(MakeShared<FJsonValueObject>(Card));
	}
	Root->SetArrayField(TEXT("definitions"), Definitions);

	OutJson.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer);
}
