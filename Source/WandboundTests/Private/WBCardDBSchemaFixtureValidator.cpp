#include "WBCardDBSchemaFixtureValidator.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
void AddDiagnostic(
	FWBCardDBSchemaValidationResult& Result,
	const EWBCardDBSchemaDiagnostic Code,
	const FString& Message,
	const FString& CardId,
	const FString& EffectId)
{
	FWBCardDBSchemaValidationDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.CardId = CardId;
	Diagnostic.EffectId = EffectId;
	Result.Diagnostics.Add(Diagnostic);
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
	FWBCardActivationSourceGateDefinition& OutSourceGate)
{
	if (!SourceGateObject.IsValid())
	{
		return;
	}

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
	if (SourceGateObject->TryGetObjectField(TEXT("cost_gate"), CostGateObject)
		&& CostGateObject != nullptr
		&& CostGateObject->IsValid())
	{
		(*CostGateObject)->TryGetBoolField(
			TEXT("requires_external_affordability"),
			OutSourceGate.CostGate.bRequiresExternalAffordability);
		OutSourceGate.CostGate.RequiredRR = ReadIntegerFieldOrDefault(*CostGateObject, TEXT("required_rr"), 0);

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
	FWBGenericEffectPayload& OutPayload)
{
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
		OutPayload.DamageEffect.Amount = ReadIntegerFieldOrDefault(PayloadObject, TEXT("amount"), 0);
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
		OutPayload.HealEffect.Amount = ReadIntegerFieldOrDefault(PayloadObject, TEXT("amount"), 0);
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

		OutPayload.ArmorEffect.Amount = ReadIntegerFieldOrDefault(PayloadObject, TEXT("amount"), 0);
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

		OutPayload.StatusEffect.Duration = ReadIntegerFieldOrDefault(PayloadObject, TEXT("turns_remaining"), 0);
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
	const FString& CardId)
{
	const TArray<TSharedPtr<FJsonValue>>* EffectValues = nullptr;
	if (!RootObject.IsValid()
		|| !RootObject->TryGetArrayField(TEXT("activated_effects"), EffectValues)
		|| EffectValues == nullptr)
	{
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
		if (EffectObject->TryGetObjectField(TEXT("source_gate"), SourceGateObject)
			&& SourceGateObject != nullptr
			&& SourceGateObject->IsValid())
		{
			ValidateSourceGate(Result, *SourceGateObject, CardId, Effect.EffectId, Effect.SourceGate);
		}

		const TArray<TSharedPtr<FJsonValue>>* PayloadValues = nullptr;
		if (EffectObject->TryGetArrayField(TEXT("payloads"), PayloadValues) && PayloadValues != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& PayloadValue : *PayloadValues)
			{
				const TSharedPtr<FJsonObject> PayloadObject = PayloadValue.IsValid() ? PayloadValue->AsObject() : nullptr;
				FWBGenericEffectPayload Payload;
				ValidatePayload(Result, PayloadObject, CardId, Effect.EffectId, Payload);
				Effect.Payloads.Add(Payload);
			}
		}

		Result.CardDefinition.ActivatedEffects.Add(Effect);
	}
}
}

FWBCardDBSchemaValidationResult FWBCardDBSchemaFixtureValidator::ValidateFixtureFile(const FString& AbsolutePath)
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

	return ValidateJsonString(Json, AbsolutePath);
}

FWBCardDBSchemaValidationResult FWBCardDBSchemaFixtureValidator::ValidateJsonString(
	const FString& Json,
	const FString& SourcePathForDiagnostics)
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

	ValidateActivatedEffects(Result, RootObject, Result.CardDefinition.CardId);
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
	case EWBCardDBSchemaDiagnostic::InvalidRRCost:
		return TEXT("invalid_rr_cost");
	case EWBCardDBSchemaDiagnostic::InvalidStatusId:
		return TEXT("invalid_status_id");
	case EWBCardDBSchemaDiagnostic::HiddenInfoPolicyViolation:
		return TEXT("hidden_info_policy_violation");
	case EWBCardDBSchemaDiagnostic::PlayerFacingLabelContainsInternalTerm:
		return TEXT("player_facing_label_contains_internal_term");
	case EWBCardDBSchemaDiagnostic::JsonParseFailed:
		return TEXT("json_parse_failed");
	default:
		return TEXT("unknown");
	}
}
