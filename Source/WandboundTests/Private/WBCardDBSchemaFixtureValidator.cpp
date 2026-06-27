#include "WBCardDBSchemaFixtureValidator.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
void AddDiagnostic(
	FWBCardDBSchemaValidationResult& Result,
	const EWBCardDBSchemaDiagnostic Code,
	const FString& Message,
	const FString& CardId,
	const FString& EffectId,
	const int32 CardIndex = -1,
	const FString& BundleCardId = FString(),
	const FString& JsonPath = FString())
{
	FWBCardDBSchemaValidationDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.CardId = CardId;
	Diagnostic.EffectId = EffectId;
	Diagnostic.CardIndex = CardIndex;
	Diagnostic.BundleCardId = BundleCardId;
	Diagnostic.JsonPath = JsonPath;
	Result.Diagnostics.Add(Diagnostic);
}

void AddDiagnostic(
	FWBCardDBBundleSchemaValidationResult& Result,
	const EWBCardDBSchemaDiagnostic Code,
	const FString& Message,
	const FString& CardId,
	const FString& EffectId,
	const int32 CardIndex = -1,
	const FString& BundleCardId = FString(),
	const FString& JsonPath = FString())
{
	FWBCardDBSchemaValidationDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.CardId = CardId;
	Diagnostic.EffectId = EffectId;
	Diagnostic.CardIndex = CardIndex;
	Diagnostic.BundleCardId = BundleCardId;
	Diagnostic.JsonPath = JsonPath;
	Result.Diagnostics.Add(Diagnostic);
}

const TArray<FString>& TopLevelAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("schema_version"),
		TEXT("carddb_version"),
		TEXT("source_version"),
		TEXT("migration_notes"),
		TEXT("metadata"),
		TEXT("card_id"),
		TEXT("public_name"),
		TEXT("public_text"),
		TEXT("kind"),
		TEXT("tags"),
		TEXT("stats"),
		TEXT("activated_effects")
	};
	return Fields;
}

const TArray<FString>& BundleTopLevelAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("bundle_schema_version"),
		TEXT("carddb_version"),
		TEXT("source_version"),
		TEXT("migration_notes"),
		TEXT("metadata"),
		TEXT("cards")
	};
	return Fields;
}

const TArray<FString>& CardMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("author"),
		TEXT("notes"),
		TEXT("source"),
		TEXT("version"),
		TEXT("test_only")
	};
	return Fields;
}

const TArray<FString>& StatsAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("hp"),
		TEXT("max_hp"),
		TEXT("atk"),
		TEXT("ar"),
		TEXT("base_rl"),
		TEXT("current_rl"),
		TEXT("rr")
	};
	return Fields;
}

const TArray<FString>& EffectAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("effect_id"),
		TEXT("public_label"),
		TEXT("target_requirement"),
		TEXT("source_gate"),
		TEXT("payloads"),
		TEXT("metadata")
	};
	return Fields;
}

const TArray<FString>& EffectMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("notes"),
		TEXT("source"),
		TEXT("test_only")
	};
	return Fields;
}

const TArray<FString>& SourceGateAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("required_zone"),
		TEXT("timing"),
		TEXT("requires_source_unit"),
		TEXT("requires_source_unit_ownership"),
		TEXT("blocked_by_stunned"),
		TEXT("blocked_by_frozen"),
		TEXT("requires_costs_satisfied_externally"),
		TEXT("once_per_turn"),
		TEXT("once_per_turn_key"),
		TEXT("requires_fixture_zone_ownership"),
		TEXT("cost_gate"),
		TEXT("metadata")
	};
	return Fields;
}

const TArray<FString>& SourceGateMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("notes"),
		TEXT("test_only")
	};
	return Fields;
}

const TArray<FString>& CostGateAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("requires_external_affordability"),
		TEXT("required_rr"),
		TEXT("cost_kind"),
		TEXT("metadata")
	};
	return Fields;
}

const TArray<FString>& CostGateMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("notes"),
		TEXT("test_only")
	};
	return Fields;
}

const TArray<FString>& PayloadAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("type"),
		TEXT("operation"),
		TEXT("amount"),
		TEXT("bypass_armor"),
		TEXT("damage_cause"),
		TEXT("status_id"),
		TEXT("turns_remaining"),
		TEXT("target"),
		TEXT("metadata")
	};
	return Fields;
}

const TArray<FString>& PayloadMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("notes"),
		TEXT("test_only")
	};
	return Fields;
}

template <typename ResultType>
void ValidateAllowedFields(
	ResultType& Result,
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& AllowedFieldNames,
	const EWBCardDBSchemaDiagnostic DiagnosticCode,
	const FWBCardDBSchemaValidationOptions& Options,
	const FString& CardId,
	const FString& EffectId,
	const FString& JsonPath = FString())
{
	if (!Options.bStrictUnknownFieldRejection || !Object.IsValid())
	{
		return;
	}

	TArray<FString> FieldNames;
	Object->Values.GetKeys(FieldNames);
	FieldNames.Sort();

	for (const FString& FieldName : FieldNames)
	{
		if (!AllowedFieldNames.Contains(FieldName))
		{
			AddDiagnostic(
				Result,
				DiagnosticCode,
				FString::Printf(TEXT("unknown field: %s"), *FieldName),
				CardId,
				EffectId,
				-1,
				FString(),
				JsonPath);
		}
	}
}

template <typename ResultType>
void ValidateMetadataAllowedFields(
	ResultType& Result,
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& AllowedFieldNames,
	const FWBCardDBSchemaValidationOptions& Options,
	const FString& CardId,
	const FString& EffectId,
	const FString& JsonPath = FString())
{
	if (!Options.bStrictUnknownFieldRejection || !Object.IsValid() || !Object->HasField(TEXT("metadata")))
	{
		return;
	}

	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (!Object->TryGetObjectField(TEXT("metadata"), MetadataObject)
		|| MetadataObject == nullptr
		|| !MetadataObject->IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::UnknownMetadataField,
			TEXT("metadata must be an object"),
			CardId,
			EffectId,
			-1,
			FString(),
			JsonPath);
		return;
	}

	ValidateAllowedFields(
		Result,
		*MetadataObject,
		AllowedFieldNames,
		EWBCardDBSchemaDiagnostic::UnknownMetadataField,
		Options,
		CardId,
		EffectId,
		JsonPath);
}

void ValidateStatsAllowedFields(
	FWBCardDBSchemaValidationResult& Result,
	const TSharedPtr<FJsonObject>& RootObject,
	const FWBCardDBSchemaValidationOptions& Options,
	const FString& CardId)
{
	if (!Options.bStrictUnknownFieldRejection || !RootObject.IsValid() || !RootObject->HasField(TEXT("stats")))
	{
		return;
	}

	const TSharedPtr<FJsonObject>* StatsObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("stats"), StatsObject)
		&& StatsObject != nullptr
		&& StatsObject->IsValid())
	{
		ValidateAllowedFields(
			Result,
			*StatsObject,
			StatsAllowedFields(),
			EWBCardDBSchemaDiagnostic::UnknownCardField,
			Options,
			CardId,
			FString());
	}
}

bool SerializeJsonObjectToString(const TSharedPtr<FJsonObject>& Object, FString& OutJson)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
}

bool ContainsHiddenDiagnosticTerm(const FString& Value)
{
	static const TArray<FString> HiddenTerms = {
		TEXT("SECRET"),
		TEXT("hidden_hand"),
		TEXT("opponent_hand"),
		TEXT("deck_card_id"),
		TEXT("marker_identity")
	};

	for (const FString& HiddenTerm : HiddenTerms)
	{
		if (Value.Contains(HiddenTerm, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

FString SafeDiagnosticIdentifier(const FString& Value)
{
	return ContainsHiddenDiagnosticTerm(Value) ? FString() : Value;
}

FString CardJsonPath(const int32 CardIndex)
{
	return FString::Printf(TEXT("$.cards[%d]"), CardIndex);
}

bool IsKnownPayloadTypeForPath(const FString& PayloadType)
{
	return PayloadType.Equals(TEXT("damage_effect"), ESearchCase::IgnoreCase)
		|| PayloadType.Equals(TEXT("heal_effect"), ESearchCase::IgnoreCase)
		|| PayloadType.Equals(TEXT("armor_effect"), ESearchCase::IgnoreCase)
		|| PayloadType.Equals(TEXT("status_effect"), ESearchCase::IgnoreCase);
}

bool TryGetEffectArray(
	const TSharedPtr<FJsonObject>& CardObject,
	const TArray<TSharedPtr<FJsonValue>>*& OutEffectValues)
{
	OutEffectValues = nullptr;
	return CardObject.IsValid()
		&& CardObject->TryGetArrayField(TEXT("activated_effects"), OutEffectValues)
		&& OutEffectValues != nullptr;
}

int32 FindEffectIndexForDiagnostic(
	const TSharedPtr<FJsonObject>& CardObject,
	const FWBCardDBSchemaValidationDiagnostic& Diagnostic)
{
	const TArray<TSharedPtr<FJsonValue>>* EffectValues = nullptr;
	if (!TryGetEffectArray(CardObject, EffectValues))
	{
		return INDEX_NONE;
	}

	for (int32 EffectIndex = 0; EffectIndex < EffectValues->Num(); ++EffectIndex)
	{
		const TSharedPtr<FJsonObject> EffectObject =
			(*EffectValues)[EffectIndex].IsValid() ? (*EffectValues)[EffectIndex]->AsObject() : nullptr;
		if (!EffectObject.IsValid())
		{
			continue;
		}

		FString EffectId;
		EffectObject->TryGetStringField(TEXT("effect_id"), EffectId);
		if (!Diagnostic.EffectId.IsEmpty() && EffectId == Diagnostic.EffectId)
		{
			return EffectIndex;
		}

		if (Diagnostic.Code == EWBCardDBSchemaDiagnostic::EffectIdMissing && EffectId.IsEmpty())
		{
			return EffectIndex;
		}
	}

	return EffectValues->Num() > 0 ? 0 : INDEX_NONE;
}

TSharedPtr<FJsonObject> GetEffectObjectAt(const TSharedPtr<FJsonObject>& CardObject, const int32 EffectIndex)
{
	const TArray<TSharedPtr<FJsonValue>>* EffectValues = nullptr;
	if (EffectIndex == INDEX_NONE || !TryGetEffectArray(CardObject, EffectValues) || !EffectValues->IsValidIndex(EffectIndex))
	{
		return nullptr;
	}

	return (*EffectValues)[EffectIndex].IsValid() ? (*EffectValues)[EffectIndex]->AsObject() : nullptr;
}

int32 FindPayloadIndexForDiagnostic(
	const TSharedPtr<FJsonObject>& EffectObject,
	const EWBCardDBSchemaDiagnostic Code)
{
	const TArray<TSharedPtr<FJsonValue>>* PayloadValues = nullptr;
	if (!EffectObject.IsValid()
		|| !EffectObject->TryGetArrayField(TEXT("payloads"), PayloadValues)
		|| PayloadValues == nullptr)
	{
		return INDEX_NONE;
	}

	for (int32 PayloadIndex = 0; PayloadIndex < PayloadValues->Num(); ++PayloadIndex)
	{
		const TSharedPtr<FJsonObject> PayloadObject =
			(*PayloadValues)[PayloadIndex].IsValid() ? (*PayloadValues)[PayloadIndex]->AsObject() : nullptr;
		if (!PayloadObject.IsValid())
		{
			continue;
		}

		FString PayloadType;
		PayloadObject->TryGetStringField(TEXT("type"), PayloadType);
		if (Code == EWBCardDBSchemaDiagnostic::UnsupportedPayloadType
			&& (PayloadType.IsEmpty() || !IsKnownPayloadTypeForPath(PayloadType)))
		{
			return PayloadIndex;
		}

		if (Code == EWBCardDBSchemaDiagnostic::InvalidStatusId
			&& PayloadType.Equals(TEXT("status_effect"), ESearchCase::IgnoreCase))
		{
			return PayloadIndex;
		}

		if (Code == EWBCardDBSchemaDiagnostic::UnsupportedPayloadOperation
			|| Code == EWBCardDBSchemaDiagnostic::InvalidNumericField
			|| Code == EWBCardDBSchemaDiagnostic::UnknownPayloadField)
		{
			return PayloadIndex;
		}
	}

	return PayloadValues->Num() > 0 ? 0 : INDEX_NONE;
}

FString InferBundleCardDiagnosticJsonPath(
	const TSharedPtr<FJsonObject>& CardObject,
	const FWBCardDBSchemaValidationDiagnostic& Diagnostic,
	const int32 CardIndex)
{
	const FString BasePath = CardJsonPath(CardIndex);

	switch (Diagnostic.Code)
	{
	case EWBCardDBSchemaDiagnostic::SchemaVersionMissing:
		return BasePath + TEXT(".schema_version");
	case EWBCardDBSchemaDiagnostic::CardIdMissing:
		return BasePath + TEXT(".card_id");
	case EWBCardDBSchemaDiagnostic::PublicNameMissing:
		return BasePath + TEXT(".public_name");
	case EWBCardDBSchemaDiagnostic::UnsupportedCardKind:
		return BasePath + TEXT(".kind");
	case EWBCardDBSchemaDiagnostic::ActivatedEffectsMalformed:
		return BasePath + TEXT(".activated_effects");
	case EWBCardDBSchemaDiagnostic::UnknownTopLevelField:
		return BasePath;
	case EWBCardDBSchemaDiagnostic::UnknownCardField:
		return BasePath + TEXT(".stats");
	default:
		break;
	}

	const int32 EffectIndex = FindEffectIndexForDiagnostic(CardObject, Diagnostic);
	if (EffectIndex == INDEX_NONE)
	{
		return BasePath;
	}

	const FString EffectPath = BasePath + FString::Printf(TEXT(".activated_effects[%d]"), EffectIndex);
	switch (Diagnostic.Code)
	{
	case EWBCardDBSchemaDiagnostic::EffectIdMissing:
	case EWBCardDBSchemaDiagnostic::EffectIdDuplicate:
		return EffectPath + TEXT(".effect_id");
	case EWBCardDBSchemaDiagnostic::PlayerFacingLabelContainsInternalTerm:
	case EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation:
		return Diagnostic.EffectId.IsEmpty() ? BasePath : EffectPath + TEXT(".public_label");
	case EWBCardDBSchemaDiagnostic::UnsupportedTargetRequirement:
		return EffectPath + TEXT(".target_requirement");
	case EWBCardDBSchemaDiagnostic::SourceGateMalformed:
	case EWBCardDBSchemaDiagnostic::UnsupportedSourceZone:
	case EWBCardDBSchemaDiagnostic::UnsupportedTiming:
	case EWBCardDBSchemaDiagnostic::UnknownSourceGateField:
		return EffectPath + TEXT(".source_gate");
	case EWBCardDBSchemaDiagnostic::CostGateMalformed:
	case EWBCardDBSchemaDiagnostic::InvalidRRCost:
	case EWBCardDBSchemaDiagnostic::UnknownCostGateField:
		return EffectPath + TEXT(".source_gate.cost_gate");
	case EWBCardDBSchemaDiagnostic::PayloadsMissing:
	case EWBCardDBSchemaDiagnostic::PayloadsMalformed:
	case EWBCardDBSchemaDiagnostic::PayloadsEmpty:
		return EffectPath + TEXT(".payloads");
	default:
		break;
	}

	const TSharedPtr<FJsonObject> EffectObject = GetEffectObjectAt(CardObject, EffectIndex);
	const int32 PayloadIndex = FindPayloadIndexForDiagnostic(EffectObject, Diagnostic.Code);
	if (PayloadIndex != INDEX_NONE)
	{
		return EffectPath + FString::Printf(TEXT(".payloads[%d]"), PayloadIndex);
	}

	return EffectPath;
}

void AppendDiagnostics(
	FWBCardDBBundleSchemaValidationResult& Result,
	const FWBCardDBSchemaValidationResult& CardResult,
	const TSharedPtr<FJsonObject>& CardObject,
	const int32 CardIndex,
	const FString& BundleCardId)
{
	const FString SafeBundleCardId = SafeDiagnosticIdentifier(BundleCardId);
	for (FWBCardDBSchemaValidationDiagnostic Diagnostic : CardResult.Diagnostics)
	{
		Diagnostic.CardIndex = CardIndex;
		Diagnostic.BundleCardId = SafeBundleCardId;
		Diagnostic.CardId = SafeDiagnosticIdentifier(Diagnostic.CardId);
		Diagnostic.EffectId = SafeDiagnosticIdentifier(Diagnostic.EffectId);
		Diagnostic.JsonPath = InferBundleCardDiagnosticJsonPath(CardObject, Diagnostic, CardIndex);
		Result.Diagnostics.Add(Diagnostic);
	}
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
}

int32 ReadIntegerFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const int32 DefaultValue)
{
	int32 Value = DefaultValue;
	TryReadIntegerField(Object, FieldName, Value);
	return Value;
}

bool ValidateOptionalIntegerField(
	FWBCardDBSchemaValidationResult& Result,
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& CardId,
	const FString& EffectId,
	int32& OutValue)
{
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return true;
	}

	if (!TryReadIntegerField(Object, FieldName, OutValue))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::InvalidNumericField,
			FString::Printf(TEXT("%s must be numeric"), FieldName),
			CardId,
			EffectId);
		return false;
	}

	return true;
}

bool ValidateOptionalNonNegativeIntegerField(
	FWBCardDBSchemaValidationResult& Result,
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& CardId,
	const FString& EffectId,
	int32& OutValue)
{
	if (!ValidateOptionalIntegerField(Result, Object, FieldName, CardId, EffectId, OutValue))
	{
		return false;
	}

	if (Object.IsValid() && Object->HasField(FieldName) && OutValue < 0)
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::InvalidNumericField,
			FString::Printf(TEXT("%s must be greater than or equal to zero"), FieldName),
			CardId,
			EffectId);
		return false;
	}

	return true;
}

bool ContainsAnyTerm(const FString& Value, const TArray<FString>& Terms)
{
	for (const FString& Term : Terms)
	{
		if (Value.Contains(Term, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool ValidatePlayerFacingField(
	FWBCardDBSchemaValidationResult& Result,
	const FString& Value,
	const FString& CardId,
	const FString& EffectId)
{
	bool bOk = true;

	static const TArray<FString> InternalTerms = {
		TEXT("EffectRunner"),
		TEXT("Rules"),
		TEXT("damage_effect"),
		TEXT("heal_effect"),
		TEXT("armor_effect"),
		TEXT("status_effect"),
		TEXT("effect.type"),
		TEXT("from_tile"),
		TEXT("to_tile"),
		TEXT("chosen_tile"),
		TEXT("player 0"),
		TEXT("player 1"),
		TEXT("player id"),
		TEXT("schema"),
		TEXT("hook")
	};

	static const TArray<FString> HiddenTerms = {
		TEXT("SECRET_"),
		TEXT("hidden_hand"),
		TEXT("opponent_hand"),
		TEXT("deck_card_id"),
		TEXT("marker_identity")
	};

	if (ContainsAnyTerm(Value, InternalTerms))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::PlayerFacingLabelContainsInternalTerm,
			TEXT("player-facing field contains internal terminology"),
			CardId,
			EffectId);
		bOk = false;
	}

	if (ContainsAnyTerm(Value, HiddenTerms))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation,
			TEXT("player-facing field contains hidden-information token"),
			CardId,
			EffectId);
		bOk = false;
	}

	return bOk;
}

bool ParseCardKind(const FString& Kind, EWBCardDefinitionKind& OutKind)
{
	if (Kind.Equals(TEXT("character"), ESearchCase::IgnoreCase))
	{
		OutKind = EWBCardDefinitionKind::Character;
		return true;
	}

	if (Kind.Equals(TEXT("wand"), ESearchCase::IgnoreCase))
	{
		OutKind = EWBCardDefinitionKind::Wand;
		return true;
	}

	if (Kind.Equals(TEXT("action"), ESearchCase::IgnoreCase))
	{
		OutKind = EWBCardDefinitionKind::Action;
		return true;
	}

	if (Kind.Equals(TEXT("terrain"), ESearchCase::IgnoreCase))
	{
		OutKind = EWBCardDefinitionKind::Terrain;
		return true;
	}

	if (Kind.Equals(TEXT("wall"), ESearchCase::IgnoreCase))
	{
		OutKind = EWBCardDefinitionKind::Wall;
		return true;
	}

	if (Kind.Equals(TEXT("fixture"), ESearchCase::IgnoreCase))
	{
		OutKind = EWBCardDefinitionKind::Fixture;
		return true;
	}

	OutKind = EWBCardDefinitionKind::Unknown;
	return false;
}

bool ParseTargetRequirement(const FString& TargetRequirement, EWBCardEffectTargetRequirement& OutRequirement)
{
	if (TargetRequirement.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		OutRequirement = EWBCardEffectTargetRequirement::None;
		return true;
	}

	if (TargetRequirement.Equals(TEXT("unit"), ESearchCase::IgnoreCase))
	{
		OutRequirement = EWBCardEffectTargetRequirement::Unit;
		return true;
	}

	if (TargetRequirement.Equals(TEXT("tile"), ESearchCase::IgnoreCase))
	{
		OutRequirement = EWBCardEffectTargetRequirement::Tile;
		return true;
	}

	if (TargetRequirement.Equals(TEXT("wall_edge"), ESearchCase::IgnoreCase))
	{
		OutRequirement = EWBCardEffectTargetRequirement::WallEdge;
		return true;
	}

	OutRequirement = EWBCardEffectTargetRequirement::None;
	return false;
}

bool ParseSourceZone(const FString& SourceZone, EWBCardActivationSourceZone& OutZone)
{
	if (SourceZone.Equals(TEXT("fixture"), ESearchCase::IgnoreCase))
	{
		OutZone = EWBCardActivationSourceZone::Fixture;
		return true;
	}

	if (SourceZone.Equals(TEXT("board"), ESearchCase::IgnoreCase))
	{
		OutZone = EWBCardActivationSourceZone::Board;
		return true;
	}

	if (SourceZone.Equals(TEXT("hand"), ESearchCase::IgnoreCase))
	{
		OutZone = EWBCardActivationSourceZone::Hand;
		return true;
	}

	if (SourceZone.Equals(TEXT("equipped"), ESearchCase::IgnoreCase))
	{
		OutZone = EWBCardActivationSourceZone::Equipped;
		return true;
	}

	if (SourceZone.Equals(TEXT("discard"), ESearchCase::IgnoreCase))
	{
		OutZone = EWBCardActivationSourceZone::Discard;
		return true;
	}

	if (SourceZone.Equals(TEXT("deck"), ESearchCase::IgnoreCase))
	{
		OutZone = EWBCardActivationSourceZone::Deck;
		return true;
	}

	OutZone = EWBCardActivationSourceZone::Unknown;
	return false;
}

bool ParseTiming(const FString& Timing, EWBCardActivationTimingRequirement& OutTiming)
{
	if (Timing.Equals(TEXT("any"), ESearchCase::IgnoreCase))
	{
		OutTiming = EWBCardActivationTimingRequirement::Any;
		return true;
	}

	if (Timing.Equals(TEXT("normal_turn_priority"), ESearchCase::IgnoreCase))
	{
		OutTiming = EWBCardActivationTimingRequirement::NormalTurnPriority;
		return true;
	}

	OutTiming = EWBCardActivationTimingRequirement::Unknown;
	return false;
}

bool ParseArmorOperation(const FString& Operation, EWBArmorEffectOp& OutOperation)
{
	if (Operation.Equals(TEXT("add_current_armor"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::AddCurrentArmor;
		return true;
	}

	if (Operation.Equals(TEXT("reduce_current_armor"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::ReduceCurrentArmor;
		return true;
	}

	if (Operation.Equals(TEXT("set_current_armor"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::SetCurrentArmor;
		return true;
	}

	if (Operation.Equals(TEXT("add_max_armor"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::AddMaxArmor;
		return true;
	}

	if (Operation.Equals(TEXT("reduce_max_armor"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::ReduceMaxArmor;
		return true;
	}

	if (Operation.Equals(TEXT("set_max_armor"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::SetMaxArmor;
		return true;
	}

	if (Operation.Equals(TEXT("restore_armor_to_max"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBArmorEffectOp::RestoreArmorToMax;
		return true;
	}

	OutOperation = EWBArmorEffectOp::Unknown;
	return false;
}

bool ParseStatusOperation(const FString& Operation, EWBStatusEffectOp& OutOperation)
{
	if (Operation.Equals(TEXT("apply_status"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::ApplyStatus;
		return true;
	}

	if (Operation.Equals(TEXT("remove_status"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::RemoveStatus;
		return true;
	}

	if (Operation.Equals(TEXT("set_status_duration"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::SetStatusDuration;
		return true;
	}

	if (Operation.Equals(TEXT("add_status_duration"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::AddStatusDuration;
		return true;
	}

	if (Operation.Equals(TEXT("reduce_status_duration"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::ReduceStatusDuration;
		return true;
	}

	if (Operation.Equals(TEXT("cleanse_status"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::CleanseStatus;
		return true;
	}

	if (Operation.Equals(TEXT("cleanse_all_statuses"), ESearchCase::IgnoreCase))
	{
		OutOperation = EWBStatusEffectOp::CleanseAllStatuses;
		return true;
	}

	OutOperation = EWBStatusEffectOp::Unknown;
	return false;
}

bool IsKnownStatusId(const FString& StatusId)
{
	return StatusId.Equals(TEXT("Burn"), ESearchCase::IgnoreCase)
		|| StatusId.Equals(TEXT("Poison"), ESearchCase::IgnoreCase)
		|| StatusId.Equals(TEXT("Rooted"), ESearchCase::IgnoreCase)
		|| StatusId.Equals(TEXT("Stunned"), ESearchCase::IgnoreCase)
		|| StatusId.Equals(TEXT("Frozen"), ESearchCase::IgnoreCase)
		|| StatusId.Equals(TEXT("CannotAttack"), ESearchCase::IgnoreCase)
		|| StatusId.Equals(TEXT("Cannot Attack"), ESearchCase::IgnoreCase);
}

void ValidateSourceGate(
	FWBCardDBSchemaValidationResult& Result,
	const TSharedPtr<FJsonObject>& SourceGateObject,
	const FString& CardId,
	const FString& EffectId,
	const FWBCardDBSchemaValidationOptions& Options,
	FWBCardActivationSourceGateDefinition& OutSourceGate)
{
	if (!SourceGateObject.IsValid())
	{
		return;
	}

	ValidateAllowedFields(
		Result,
		SourceGateObject,
		SourceGateAllowedFields(),
		EWBCardDBSchemaDiagnostic::UnknownSourceGateField,
		Options,
		CardId,
		EffectId);
	ValidateMetadataAllowedFields(Result, SourceGateObject, SourceGateMetadataAllowedFields(), Options, CardId, EffectId);

	OutSourceGate.bHasExplicitSourceGate = true;

	FString RequiredZone;
	if (SourceGateObject->TryGetStringField(TEXT("required_zone"), RequiredZone))
	{
		if (!ParseSourceZone(RequiredZone, OutSourceGate.RequiredZone))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::UnsupportedSourceZone,
				TEXT("unsupported source zone"),
				CardId,
				EffectId);
		}
	}

	FString Timing;
	if (SourceGateObject->TryGetStringField(TEXT("timing"), Timing))
	{
		if (!ParseTiming(Timing, OutSourceGate.Timing))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::UnsupportedTiming,
				TEXT("unsupported timing"),
				CardId,
				EffectId);
		}
	}

	SourceGateObject->TryGetBoolField(TEXT("requires_fixture_zone_ownership"), OutSourceGate.bRequiresFixtureZoneOwnership);
	SourceGateObject->TryGetBoolField(TEXT("requires_source_unit"), OutSourceGate.bRequiresSourceUnit);
	SourceGateObject->TryGetBoolField(TEXT("requires_source_unit_ownership"), OutSourceGate.bRequiresSourceUnitOwnership);
	SourceGateObject->TryGetBoolField(TEXT("blocked_by_stunned"), OutSourceGate.bBlockedByStunned);
	SourceGateObject->TryGetBoolField(TEXT("blocked_by_frozen"), OutSourceGate.bBlockedByFrozen);
	SourceGateObject->TryGetBoolField(TEXT("requires_costs_satisfied_externally"), OutSourceGate.bRequiresCostsSatisfiedExternally);
	SourceGateObject->TryGetBoolField(TEXT("once_per_turn"), OutSourceGate.bOncePerTurn);
	SourceGateObject->TryGetStringField(TEXT("once_per_turn_key"), OutSourceGate.OncePerTurnKey);

	const TSharedPtr<FJsonObject>* CostGateObject = nullptr;
	if (SourceGateObject->HasField(TEXT("cost_gate")))
	{
		if (!SourceGateObject->TryGetObjectField(TEXT("cost_gate"), CostGateObject)
			|| CostGateObject == nullptr
			|| !CostGateObject->IsValid())
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::CostGateMalformed,
				TEXT("cost_gate must be an object"),
				CardId,
				EffectId);
			return;
		}

		ValidateAllowedFields(
			Result,
			*CostGateObject,
			CostGateAllowedFields(),
			EWBCardDBSchemaDiagnostic::UnknownCostGateField,
			Options,
			CardId,
			EffectId);
		ValidateMetadataAllowedFields(Result, *CostGateObject, CostGateMetadataAllowedFields(), Options, CardId, EffectId);

		(*CostGateObject)->TryGetBoolField(
			TEXT("requires_external_affordability"),
			OutSourceGate.CostGate.bRequiresExternalAffordability);
		if (!ValidateOptionalIntegerField(
			Result,
			*CostGateObject,
			TEXT("required_rr"),
			CardId,
			EffectId,
			OutSourceGate.CostGate.RequiredRR))
		{
			return;
		}

		FString CostKind;
		if ((*CostGateObject)->TryGetStringField(TEXT("cost_kind"), CostKind))
		{
			OutSourceGate.CostGate.CostKind = FName(*CostKind);
		}

		if (OutSourceGate.CostGate.RequiredRR < 0)
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::InvalidRRCost,
				TEXT("required_rr must be greater than or equal to zero"),
				CardId,
				EffectId);
		}
	}
}

void ValidatePayload(
	FWBCardDBSchemaValidationResult& Result,
	const TSharedPtr<FJsonObject>& PayloadObject,
	const FString& CardId,
	const FString& EffectId,
	const FWBCardDBSchemaValidationOptions& Options,
	FWBGenericEffectPayload& OutPayload)
{
	ValidateAllowedFields(
		Result,
		PayloadObject,
		PayloadAllowedFields(),
		EWBCardDBSchemaDiagnostic::UnknownPayloadField,
		Options,
		CardId,
		EffectId);
	ValidateMetadataAllowedFields(Result, PayloadObject, PayloadMetadataAllowedFields(), Options, CardId, EffectId);

	FString PayloadType;
	if (!PayloadObject.IsValid() || !PayloadObject->TryGetStringField(TEXT("type"), PayloadType))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::UnsupportedPayloadType,
			TEXT("missing or unsupported payload type"),
			CardId,
			EffectId);
		return;
	}

	if (PayloadType.Equals(TEXT("damage_effect"), ESearchCase::IgnoreCase))
	{
		OutPayload.Operation = EWBGenericEffectOp::DamageEffect;
		ValidateOptionalNonNegativeIntegerField(
			Result,
			PayloadObject,
			TEXT("amount"),
			CardId,
			EffectId,
			OutPayload.DamageEffect.Amount);
		PayloadObject->TryGetBoolField(TEXT("bypass_armor"), OutPayload.DamageEffect.bBypassArmor);

		FString DamageCause;
		if (PayloadObject->TryGetStringField(TEXT("damage_cause"), DamageCause))
		{
			OutPayload.DamageEffect.DamageCause = FName(*DamageCause);
		}

		return;
	}

	if (PayloadType.Equals(TEXT("heal_effect"), ESearchCase::IgnoreCase))
	{
		OutPayload.Operation = EWBGenericEffectOp::HealEffect;
		ValidateOptionalNonNegativeIntegerField(
			Result,
			PayloadObject,
			TEXT("amount"),
			CardId,
			EffectId,
			OutPayload.HealEffect.Amount);
		return;
	}

	if (PayloadType.Equals(TEXT("armor_effect"), ESearchCase::IgnoreCase))
	{
		OutPayload.Operation = EWBGenericEffectOp::ArmorEffect;

		FString Operation;
		if (!PayloadObject->TryGetStringField(TEXT("operation"), Operation)
			|| !ParseArmorOperation(Operation, OutPayload.ArmorEffect.Operation))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::UnsupportedPayloadOperation,
				TEXT("unsupported armor operation"),
				CardId,
				EffectId);
			return;
		}

		ValidateOptionalNonNegativeIntegerField(
			Result,
			PayloadObject,
			TEXT("amount"),
			CardId,
			EffectId,
			OutPayload.ArmorEffect.Amount);
		return;
	}

	if (PayloadType.Equals(TEXT("status_effect"), ESearchCase::IgnoreCase))
	{
		OutPayload.Operation = EWBGenericEffectOp::StatusEffect;

		FString Operation;
		if (!PayloadObject->TryGetStringField(TEXT("operation"), Operation)
			|| !ParseStatusOperation(Operation, OutPayload.StatusEffect.Operation))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::UnsupportedPayloadOperation,
				TEXT("unsupported status operation"),
				CardId,
				EffectId);
			return;
		}

		FString StatusId;
		if (PayloadObject->TryGetStringField(TEXT("status_id"), StatusId))
		{
			OutPayload.StatusEffect.StatusId = FName(*StatusId);
		}

		if (OutPayload.StatusEffect.Operation != EWBStatusEffectOp::CleanseAllStatuses
			&& !IsKnownStatusId(StatusId))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::InvalidStatusId,
				TEXT("unsupported status id"),
				CardId,
				EffectId);
		}

		ValidateOptionalNonNegativeIntegerField(
			Result,
			PayloadObject,
			TEXT("turns_remaining"),
			CardId,
			EffectId,
			OutPayload.StatusEffect.Duration);
		return;
	}

	AddDiagnostic(
		Result,
		EWBCardDBSchemaDiagnostic::UnsupportedPayloadType,
		TEXT("unsupported payload type"),
		CardId,
		EffectId);
}

void ValidateActivatedEffects(
	FWBCardDBSchemaValidationResult& Result,
	const TSharedPtr<FJsonObject>& RootObject,
	const FString& CardId,
	const FWBCardDBSchemaValidationOptions& Options)
{
	const TArray<TSharedPtr<FJsonValue>>* EffectValues = nullptr;
	if (!RootObject.IsValid() || !RootObject->HasField(TEXT("activated_effects")))
	{
		return;
	}

	if (!RootObject->TryGetArrayField(TEXT("activated_effects"), EffectValues) || EffectValues == nullptr)
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::ActivatedEffectsMalformed,
			TEXT("activated_effects must be an array"),
			CardId,
			FString());
		return;
	}

	TSet<FString> SeenEffectIds;
	for (const TSharedPtr<FJsonValue>& EffectValue : *EffectValues)
	{
		const TSharedPtr<FJsonObject> EffectObject = EffectValue.IsValid() ? EffectValue->AsObject() : nullptr;
		if (!EffectObject.IsValid())
		{
			continue;
		}

		FWBCardEffectDefinition Effect;
		EffectObject->TryGetStringField(TEXT("effect_id"), Effect.EffectId);

		ValidateAllowedFields(
			Result,
			EffectObject,
			EffectAllowedFields(),
			EWBCardDBSchemaDiagnostic::UnknownEffectField,
			Options,
			CardId,
			Effect.EffectId);
		ValidateMetadataAllowedFields(Result, EffectObject, EffectMetadataAllowedFields(), Options, CardId, Effect.EffectId);

		if (Effect.EffectId.IsEmpty())
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::EffectIdMissing,
				TEXT("effect_id is required"),
				CardId,
				FString());
		}
		else if (SeenEffectIds.Contains(Effect.EffectId))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::EffectIdDuplicate,
				TEXT("effect_id must be unique within the card"),
				CardId,
				Effect.EffectId);
		}
		else
		{
			SeenEffectIds.Add(Effect.EffectId);
		}

		EffectObject->TryGetStringField(TEXT("public_label"), Effect.PublicLabel);
		if (Effect.PublicLabel.IsEmpty())
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::PlayerFacingLabelContainsInternalTerm,
				TEXT("public_label is required"),
				CardId,
				Effect.EffectId);
		}
		else
		{
			ValidatePlayerFacingField(Result, Effect.PublicLabel, CardId, Effect.EffectId);
		}

		FString TargetRequirement;
		EffectObject->TryGetStringField(TEXT("target_requirement"), TargetRequirement);
		if (!ParseTargetRequirement(TargetRequirement, Effect.TargetRequirement))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::UnsupportedTargetRequirement,
				TEXT("unsupported target requirement"),
				CardId,
				Effect.EffectId);
		}

		const TSharedPtr<FJsonObject>* SourceGateObject = nullptr;
		if (EffectObject->HasField(TEXT("source_gate")))
		{
			if (!EffectObject->TryGetObjectField(TEXT("source_gate"), SourceGateObject)
				|| SourceGateObject == nullptr
				|| !SourceGateObject->IsValid())
			{
				AddDiagnostic(
					Result,
					EWBCardDBSchemaDiagnostic::SourceGateMalformed,
					TEXT("source_gate must be an object"),
					CardId,
					Effect.EffectId);
			}
			else
			{
				ValidateSourceGate(Result, *SourceGateObject, CardId, Effect.EffectId, Options, Effect.SourceGate);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* PayloadValues = nullptr;
		if (!EffectObject->HasField(TEXT("payloads")))
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::PayloadsMissing,
				TEXT("payloads is required for activated effects"),
				CardId,
				Effect.EffectId);
		}
		else if (!EffectObject->TryGetArrayField(TEXT("payloads"), PayloadValues) || PayloadValues == nullptr)
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::PayloadsMalformed,
				TEXT("payloads must be an array"),
				CardId,
				Effect.EffectId);
		}
		else if (PayloadValues->Num() == 0)
		{
			AddDiagnostic(
				Result,
				EWBCardDBSchemaDiagnostic::PayloadsEmpty,
				TEXT("payloads must not be empty for activated effects"),
				CardId,
				Effect.EffectId);
		}
		else
		{
			for (const TSharedPtr<FJsonValue>& PayloadValue : *PayloadValues)
			{
				const TSharedPtr<FJsonObject> PayloadObject = PayloadValue.IsValid() ? PayloadValue->AsObject() : nullptr;
				FWBGenericEffectPayload Payload;
				ValidatePayload(Result, PayloadObject, CardId, Effect.EffectId, Options, Payload);
				Effect.Payloads.Add(Payload);
			}
		}

		Result.CardDefinition.ActivatedEffects.Add(Effect);
	}
}
}

FWBCardDBSchemaValidationResult FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(
	const FString& AbsolutePath,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath))
	{
		FWBCardDBSchemaValidationResult Result;
		Result.SourcePath = AbsolutePath;
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::JsonParseFailed,
			TEXT("fixture file could not be loaded"),
			FString(),
			FString());
		return Result;
	}

	return ValidateJsonString(Json, AbsolutePath, Options);
}

FWBCardDBSchemaValidationResult FWBCardDBSchemaFixtureValidator::ValidateJsonString(
	const FString& Json,
	const FString& SourcePathForDiagnostics,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FWBCardDBSchemaValidationResult Result;
	Result.SourcePath = SourcePathForDiagnostics;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::JsonParseFailed,
			TEXT("fixture json could not be parsed"),
			FString(),
			FString());
		return Result;
	}

	FString StrictDiagnosticCardId;
	RootObject->TryGetStringField(TEXT("card_id"), StrictDiagnosticCardId);
	ValidateAllowedFields(
		Result,
		RootObject,
		TopLevelAllowedFields(),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField,
		Options,
		StrictDiagnosticCardId,
		FString());
	ValidateMetadataAllowedFields(Result, RootObject, CardMetadataAllowedFields(), Options, StrictDiagnosticCardId, FString());
	ValidateStatsAllowedFields(Result, RootObject, Options, StrictDiagnosticCardId);

	int32 SchemaVersion = 0;
	if (!TryReadIntegerField(RootObject, TEXT("schema_version"), SchemaVersion) || SchemaVersion != 1)
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::SchemaVersionMissing,
			TEXT("schema_version is required and must equal 1"),
			FString(),
			FString());
	}

	RootObject->TryGetStringField(TEXT("card_id"), Result.CardDefinition.CardId);
	if (Result.CardDefinition.CardId.IsEmpty())
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::CardIdMissing,
			TEXT("card_id is required"),
			FString(),
			FString());
	}

	RootObject->TryGetStringField(TEXT("public_name"), Result.CardDefinition.PublicName);
	if (Result.CardDefinition.PublicName.IsEmpty())
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::PublicNameMissing,
			TEXT("public_name is required"),
			Result.CardDefinition.CardId,
			FString());
	}
	else
	{
		ValidatePlayerFacingField(Result, Result.CardDefinition.PublicName, Result.CardDefinition.CardId, FString());
	}

	FString PublicText;
	if (RootObject->TryGetStringField(TEXT("public_text"), PublicText))
	{
		ValidatePlayerFacingField(Result, PublicText, Result.CardDefinition.CardId, FString());
	}

	FString Kind;
	if (!RootObject->TryGetStringField(TEXT("kind"), Kind)
		|| !ParseCardKind(Kind, Result.CardDefinition.Kind))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::UnsupportedCardKind,
			TEXT("unsupported card kind"),
			Result.CardDefinition.CardId,
			FString());
	}

	ValidateActivatedEffects(Result, RootObject, Result.CardDefinition.CardId, Options);
	Result.bOk = Result.Diagnostics.Num() == 0;
	return Result;
}

FWBCardDBBundleSchemaValidationResult FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(
	const FString& AbsolutePath,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath))
	{
		FWBCardDBBundleSchemaValidationResult Result;
		Result.SourcePath = AbsolutePath;
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::JsonParseFailed,
			TEXT("bundle fixture file could not be loaded"),
			FString(),
			FString());
		return Result;
	}

	return ValidateBundleJsonString(Json, AbsolutePath, Options);
}

FWBCardDBBundleSchemaValidationResult FWBCardDBSchemaFixtureValidator::ValidateBundleJsonString(
	const FString& Json,
	const FString& SourcePathForDiagnostics,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FWBCardDBBundleSchemaValidationResult Result;
	Result.SourcePath = SourcePathForDiagnostics;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::JsonParseFailed,
			TEXT("bundle json could not be parsed"),
			FString(),
			FString());
		return Result;
	}

	ValidateAllowedFields(
		Result,
		RootObject,
		BundleTopLevelAllowedFields(),
		EWBCardDBSchemaDiagnostic::UnknownTopLevelField,
		Options,
		FString(),
		FString(),
		TEXT("$"));
	ValidateMetadataAllowedFields(Result, RootObject, CardMetadataAllowedFields(), Options, FString(), FString(), TEXT("$.metadata"));

	int32 BundleSchemaVersion = 0;
	if (!TryReadIntegerField(RootObject, TEXT("bundle_schema_version"), BundleSchemaVersion))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::BundleSchemaVersionMissing,
			TEXT("bundle_schema_version is required and must equal 1"),
			FString(),
			FString(),
			-1,
			FString(),
			TEXT("$.bundle_schema_version"));
	}
	else if (BundleSchemaVersion != 1)
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::BundleSchemaVersionUnsupported,
			TEXT("bundle_schema_version must equal 1"),
			FString(),
			FString(),
			-1,
			FString(),
			TEXT("$.bundle_schema_version"));
	}

	FString CardDBVersion;
	if (!RootObject->TryGetStringField(TEXT("carddb_version"), CardDBVersion) || CardDBVersion.IsEmpty())
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::CardDBVersionMissing,
			TEXT("carddb_version is required"),
			FString(),
			FString(),
			-1,
			FString(),
			TEXT("$.carddb_version"));
	}

	const TArray<TSharedPtr<FJsonValue>>* CardValues = nullptr;
	if (!RootObject->HasField(TEXT("cards")))
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::CardsMissing,
			TEXT("cards is required"),
			FString(),
			FString(),
			-1,
			FString(),
			TEXT("$.cards"));
	}
	else if (!RootObject->TryGetArrayField(TEXT("cards"), CardValues) || CardValues == nullptr)
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::CardsMalformed,
			TEXT("cards must be an array"),
			FString(),
			FString(),
			-1,
			FString(),
			TEXT("$.cards"));
	}
	else if (CardValues->Num() == 0)
	{
		AddDiagnostic(
			Result,
			EWBCardDBSchemaDiagnostic::CardsEmpty,
			TEXT("cards must not be empty"),
			FString(),
			FString(),
			-1,
			FString(),
			TEXT("$.cards"));
	}
	else
	{
		TSet<FString> SeenCardIds;
		for (int32 CardIndex = 0; CardIndex < CardValues->Num(); ++CardIndex)
		{
			const TSharedPtr<FJsonValue>& CardValue = (*CardValues)[CardIndex];
			const TSharedPtr<FJsonObject> CardObject = CardValue.IsValid() ? CardValue->AsObject() : nullptr;
			if (!CardObject.IsValid())
			{
				AddDiagnostic(
					Result,
					EWBCardDBSchemaDiagnostic::CardsMalformed,
					TEXT("cards entries must be objects"),
					FString(),
					FString(),
					CardIndex,
					FString(),
					CardJsonPath(CardIndex));
				continue;
			}

			FString CardId;
			CardObject->TryGetStringField(TEXT("card_id"), CardId);
			const FString SafeCardId = SafeDiagnosticIdentifier(CardId);
			if (!CardId.IsEmpty())
			{
				if (SeenCardIds.Contains(CardId))
				{
					AddDiagnostic(
						Result,
						EWBCardDBSchemaDiagnostic::CardIdDuplicate,
						TEXT("card_id must be unique within the bundle"),
						SafeCardId,
						FString(),
						CardIndex,
						SafeCardId,
						TEXT("$.cards"));
				}
				else
				{
					SeenCardIds.Add(CardId);
				}
			}

			FString CardJson;
			if (!SerializeJsonObjectToString(CardObject, CardJson))
			{
				AddDiagnostic(
					Result,
					EWBCardDBSchemaDiagnostic::JsonParseFailed,
					TEXT("card object could not be serialized for validation"),
					SafeCardId,
					FString(),
					CardIndex,
					SafeCardId,
					CardJsonPath(CardIndex));
				continue;
			}

			const FWBCardDBSchemaValidationResult CardResult =
				ValidateJsonString(CardJson, SourcePathForDiagnostics, Options);
			AppendDiagnostics(Result, CardResult, CardObject, CardIndex, CardId);
			if (CardResult.bOk)
			{
				Result.CardDefinitions.Add(CardResult.CardDefinition);
			}
		}
	}

	Result.bOk = Result.Diagnostics.Num() == 0;
	return Result;
}

FString FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(const EWBCardDBSchemaDiagnostic Code)
{
	switch (Code)
	{
	case EWBCardDBSchemaDiagnostic::None:
		return TEXT("none");
	case EWBCardDBSchemaDiagnostic::SchemaVersionMissing:
		return TEXT("schema_version_missing");
	case EWBCardDBSchemaDiagnostic::CardIdMissing:
		return TEXT("card_id_missing");
	case EWBCardDBSchemaDiagnostic::PublicNameMissing:
		return TEXT("public_name_missing");
	case EWBCardDBSchemaDiagnostic::EffectIdMissing:
		return TEXT("effect_id_missing");
	case EWBCardDBSchemaDiagnostic::EffectIdDuplicate:
		return TEXT("effect_id_duplicate");
	case EWBCardDBSchemaDiagnostic::UnsupportedCardKind:
		return TEXT("unsupported_card_kind");
	case EWBCardDBSchemaDiagnostic::UnsupportedTargetRequirement:
		return TEXT("unsupported_target_requirement");
	case EWBCardDBSchemaDiagnostic::UnsupportedSourceZone:
		return TEXT("unsupported_source_zone");
	case EWBCardDBSchemaDiagnostic::UnsupportedTiming:
		return TEXT("unsupported_timing");
	case EWBCardDBSchemaDiagnostic::UnsupportedPayloadType:
		return TEXT("unsupported_payload_type");
	case EWBCardDBSchemaDiagnostic::UnsupportedPayloadOperation:
		return TEXT("unsupported_payload_operation");
	case EWBCardDBSchemaDiagnostic::PayloadsMissing:
		return TEXT("payloads_missing");
	case EWBCardDBSchemaDiagnostic::PayloadsMalformed:
		return TEXT("payloads_malformed");
	case EWBCardDBSchemaDiagnostic::PayloadsEmpty:
		return TEXT("payloads_empty");
	case EWBCardDBSchemaDiagnostic::ActivatedEffectsMalformed:
		return TEXT("activated_effects_malformed");
	case EWBCardDBSchemaDiagnostic::SourceGateMalformed:
		return TEXT("source_gate_malformed");
	case EWBCardDBSchemaDiagnostic::CostGateMalformed:
		return TEXT("cost_gate_malformed");
	case EWBCardDBSchemaDiagnostic::InvalidRRCost:
		return TEXT("invalid_rr_cost");
	case EWBCardDBSchemaDiagnostic::InvalidStatusId:
		return TEXT("invalid_status_id");
	case EWBCardDBSchemaDiagnostic::InvalidNumericField:
		return TEXT("invalid_numeric_field");
	case EWBCardDBSchemaDiagnostic::UnknownTopLevelField:
		return TEXT("unknown_top_level_field");
	case EWBCardDBSchemaDiagnostic::UnknownCardField:
		return TEXT("unknown_card_field");
	case EWBCardDBSchemaDiagnostic::UnknownEffectField:
		return TEXT("unknown_effect_field");
	case EWBCardDBSchemaDiagnostic::UnknownSourceGateField:
		return TEXT("unknown_source_gate_field");
	case EWBCardDBSchemaDiagnostic::UnknownCostGateField:
		return TEXT("unknown_cost_gate_field");
	case EWBCardDBSchemaDiagnostic::UnknownPayloadField:
		return TEXT("unknown_payload_field");
	case EWBCardDBSchemaDiagnostic::UnknownMetadataField:
		return TEXT("unknown_metadata_field");
	case EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation:
		return TEXT("hidden_info_policy_violation");
	case EWBCardDBSchemaDiagnostic::PlayerFacingLabelContainsInternalTerm:
		return TEXT("player_facing_label_contains_internal_term");
	case EWBCardDBSchemaDiagnostic::JsonParseFailed:
		return TEXT("json_parse_failed");
	case EWBCardDBSchemaDiagnostic::CardDBVersionMissing:
		return TEXT("carddb_version_missing");
	case EWBCardDBSchemaDiagnostic::CardsMissing:
		return TEXT("cards_missing");
	case EWBCardDBSchemaDiagnostic::CardsMalformed:
		return TEXT("cards_malformed");
	case EWBCardDBSchemaDiagnostic::CardsEmpty:
		return TEXT("cards_empty");
	case EWBCardDBSchemaDiagnostic::CardIdDuplicate:
		return TEXT("card_id_duplicate");
	case EWBCardDBSchemaDiagnostic::BundleSchemaVersionMissing:
		return TEXT("bundle_schema_version_missing");
	case EWBCardDBSchemaDiagnostic::BundleSchemaVersionUnsupported:
		return TEXT("bundle_schema_version_unsupported");
	default:
		return TEXT("unknown");
	}
}

FString FWBCardDBSchemaFixtureValidator::DiagnosticContextToStringForTest(
	const FWBCardDBSchemaValidationDiagnostic& Diagnostic)
{
	return FString::Printf(
		TEXT("code=%s;card_index=%d;bundle_card_id=%s;card_id=%s;effect_id=%s;json_path=%s"),
		*DiagnosticCodeToString(Diagnostic.Code),
		Diagnostic.CardIndex,
		*Diagnostic.BundleCardId,
		*Diagnostic.CardId,
		*Diagnostic.EffectId,
		*Diagnostic.JsonPath);
}

bool FWBCardDBSchemaFixtureValidator::ContainsDiagnosticWithContext(
	const TArray<FWBCardDBSchemaValidationDiagnostic>& Diagnostics,
	const EWBCardDBSchemaDiagnostic Code,
	const int32 ExpectedCardIndex,
	const FString& ExpectedCardId,
	const FString& ExpectedEffectId,
	const FString& ExpectedJsonPath)
{
	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Code == Code
			&& Diagnostic.CardIndex == ExpectedCardIndex
			&& Diagnostic.BundleCardId == ExpectedCardId
			&& Diagnostic.EffectId == ExpectedEffectId
			&& Diagnostic.JsonPath == ExpectedJsonPath)
		{
			return true;
		}
	}

	return false;
}
