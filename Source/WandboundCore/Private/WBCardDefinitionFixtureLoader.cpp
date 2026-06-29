#include "WBCardDefinitionFixtureLoader.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const TCHAR* FixtureLoadFailedReason = TEXT("fixture_load_failed");

void AddDiagnostic(
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& Code,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	FWBCardDefinitionFixtureLoadDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.CardId = CardId;
	Diagnostic.EffectId = EffectId;
	Diagnostic.Path = Path;
	Result.Diagnostics.Add(Diagnostic);
}

void AddDiagnostic(
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* Code,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	AddDiagnostic(Result, FString(Code), CardId, EffectId, Path);
}

void MarkFailedIfDiagnostics(FWBCardDefinitionFixtureLoadResult& Result)
{
	if (!Result.Diagnostics.IsEmpty())
	{
		Result.bOk = false;
		Result.Reason = FixtureLoadFailedReason;
		Result.Repository = FWBCardDefinitionRepository();
	}
}

FString JoinPath(const FString& ParentPath, const FString& Child)
{
	if (Child.StartsWith(TEXT("[")))
	{
		return ParentPath + Child;
	}

	return ParentPath + TEXT(".") + Child;
}

bool IsFieldKnown(const TArray<FString>& KnownFields, const FString& FieldName)
{
	for (const FString& KnownField : KnownFields)
	{
		if (KnownField == FieldName)
		{
			return true;
		}
	}
	return false;
}

void ValidateKnownFields(
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& KnownFields,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	if (!Object.IsValid())
	{
		return;
	}

	TArray<FString> FieldNames;
	Object->Values.GetKeys(FieldNames);
	FieldNames.Sort();
	for (const FString& FieldName : FieldNames)
	{
		if (!IsFieldKnown(KnownFields, FieldName))
		{
			AddDiagnostic(Result, TEXT("unknown_field"), CardId, EffectId, JoinPath(Path, FieldName));
		}
	}
}

bool TryReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* MissingCode,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path,
	FString& OutValue)
{
	OutValue.Reset();

	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(FieldName) : nullptr;
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::String)
	{
		AddDiagnostic(Result, MissingCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	OutValue = (*Value)->AsString();
	if (OutValue.IsEmpty())
	{
		AddDiagnostic(Result, MissingCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	return true;
}

bool TryReadOptionalString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* MalformedCode,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path,
	FString& OutValue)
{
	OutValue.Reset();

	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(FieldName) : nullptr;
	if (Value == nullptr)
	{
		return false;
	}

	if (!Value->IsValid() || (*Value)->Type != EJson::String)
	{
		AddDiagnostic(Result, MalformedCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	OutValue = (*Value)->AsString();
	return true;
}

bool TryReadOptionalBool(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* MalformedCode,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path,
	bool& OutValue)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(FieldName) : nullptr;
	if (Value == nullptr)
	{
		return false;
	}

	if (!Value->IsValid() || (*Value)->Type != EJson::Boolean)
	{
		AddDiagnostic(Result, MalformedCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	OutValue = (*Value)->AsBool();
	return true;
}

bool TryReadOptionalInteger(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* MalformedCode,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path,
	int32& OutValue)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(FieldName) : nullptr;
	if (Value == nullptr)
	{
		return false;
	}

	double Number = 0.0;
	if (!Value->IsValid() || (*Value)->Type != EJson::Number || !(*Value)->TryGetNumber(Number))
	{
		AddDiagnostic(Result, MalformedCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	const int32 IntegerValue = static_cast<int32>(Number);
	if (!FMath::IsNearlyEqual(Number, static_cast<double>(IntegerValue)))
	{
		AddDiagnostic(Result, MalformedCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	OutValue = IntegerValue;
	return true;
}

bool TryReadRequiredInteger(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* MalformedCode,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path,
	int32& OutValue)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(FieldName) : nullptr;
	if (Value == nullptr)
	{
		AddDiagnostic(Result, MalformedCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	return TryReadOptionalInteger(Object, FieldName, Result, MalformedCode, CardId, EffectId, Path, OutValue);
}

bool TryGetObjectField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		&& Child->IsValid())
	{
		OutObject = *Child;
		return true;
	}
	return false;
}

bool TryGetRequiredObjectField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	FWBCardDefinitionFixtureLoadResult& Result,
	const TCHAR* MalformedCode,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path,
	TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();
	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(FieldName) : nullptr;
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Object)
	{
		AddDiagnostic(Result, MalformedCode, CardId, EffectId, JoinPath(Path, FieldName));
		return false;
	}

	OutObject = (*Value)->AsObject();
	return OutObject.IsValid();
}

bool TryGetArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
	OutArray = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, OutArray) && OutArray != nullptr)
	{
		return true;
	}
	return false;
}

EWBCardDefinitionKind ParseCardKind(
	const FString& Kind,
	bool& bOutSupported)
{
	bOutSupported = true;
	if (Kind.IsEmpty() || Kind == TEXT("fixture"))
	{
		return EWBCardDefinitionKind::Fixture;
	}
	if (Kind == TEXT("character"))
	{
		return EWBCardDefinitionKind::Character;
	}
	if (Kind == TEXT("wand"))
	{
		return EWBCardDefinitionKind::Wand;
	}
	if (Kind == TEXT("action"))
	{
		return EWBCardDefinitionKind::Action;
	}
	if (Kind == TEXT("terrain"))
	{
		return EWBCardDefinitionKind::Terrain;
	}
	if (Kind == TEXT("wall"))
	{
		return EWBCardDefinitionKind::Wall;
	}

	bOutSupported = false;
	return EWBCardDefinitionKind::Unknown;
}

void ParseCharacterStats(
	const TSharedPtr<FJsonObject>& CardObject,
	FWBCardDefinition& OutDefinition,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& Path)
{
	TSharedPtr<FJsonObject> StatsObject;
	if (!TryGetRequiredObjectField(
		CardObject,
		TEXT("character_stats"),
		Result,
		TEXT("character_stats_malformed"),
		CardId,
		TEXT(""),
		Path,
		StatsObject))
	{
		return;
	}

	ValidateKnownFields(
		StatsObject,
		{ TEXT("hp"), TEXT("atk"), TEXT("ar"), TEXT("rl") },
		Result,
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("character_stats")));

	TryReadRequiredInteger(
		StatsObject,
		TEXT("hp"),
		Result,
		TEXT("character_stats_malformed"),
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("character_stats")),
		OutDefinition.CharacterStats.HP);
	TryReadRequiredInteger(
		StatsObject,
		TEXT("atk"),
		Result,
		TEXT("character_stats_malformed"),
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("character_stats")),
		OutDefinition.CharacterStats.ATK);
	TryReadRequiredInteger(
		StatsObject,
		TEXT("ar"),
		Result,
		TEXT("character_stats_malformed"),
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("character_stats")),
		OutDefinition.CharacterStats.AR);
	TryReadRequiredInteger(
		StatsObject,
		TEXT("rl"),
		Result,
		TEXT("character_stats_malformed"),
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("character_stats")),
		OutDefinition.CharacterStats.RL);
}

void ParseWandStats(
	const TSharedPtr<FJsonObject>& CardObject,
	FWBCardDefinition& OutDefinition,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& Path)
{
	TSharedPtr<FJsonObject> StatsObject;
	if (!TryGetRequiredObjectField(
		CardObject,
		TEXT("wand_stats"),
		Result,
		TEXT("wand_stats_malformed"),
		CardId,
		TEXT(""),
		Path,
		StatsObject))
	{
		return;
	}

	ValidateKnownFields(
		StatsObject,
		{ TEXT("rr") },
		Result,
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("wand_stats")));

	TryReadRequiredInteger(
		StatsObject,
		TEXT("rr"),
		Result,
		TEXT("wand_stats_malformed"),
		CardId,
		TEXT(""),
		JoinPath(Path, TEXT("wand_stats")),
		OutDefinition.WandStats.RR);
}

EWBCardEffectTargetRequirement ParseTargetRequirement(
	const FString& Requirement,
	bool& bOutSupported)
{
	bOutSupported = true;
	if (Requirement == TEXT("none"))
	{
		return EWBCardEffectTargetRequirement::None;
	}
	if (Requirement == TEXT("unit"))
	{
		return EWBCardEffectTargetRequirement::Unit;
	}
	if (Requirement == TEXT("tile"))
	{
		return EWBCardEffectTargetRequirement::Tile;
	}
	if (Requirement == TEXT("wall_edge"))
	{
		return EWBCardEffectTargetRequirement::WallEdge;
	}

	bOutSupported = false;
	return EWBCardEffectTargetRequirement::None;
}

EWBCardActivationSourceZone ParseSourceZone(
	const FString& Zone,
	bool& bOutSupported)
{
	bOutSupported = true;
	if (Zone.IsEmpty() || Zone == TEXT("fixture"))
	{
		return EWBCardActivationSourceZone::Fixture;
	}
	if (Zone == TEXT("board"))
	{
		return EWBCardActivationSourceZone::Board;
	}
	if (Zone == TEXT("hand"))
	{
		return EWBCardActivationSourceZone::Hand;
	}
	if (Zone == TEXT("discard"))
	{
		return EWBCardActivationSourceZone::Discard;
	}
	if (Zone == TEXT("equipped"))
	{
		return EWBCardActivationSourceZone::Equipped;
	}

	bOutSupported = false;
	return EWBCardActivationSourceZone::Unknown;
}

EWBCardActivationTimingRequirement ParseTiming(
	const FString& Timing,
	bool& bOutSupported)
{
	bOutSupported = true;
	if (Timing.IsEmpty() || Timing == TEXT("any"))
	{
		return EWBCardActivationTimingRequirement::Any;
	}
	if (Timing == TEXT("normal_turn_priority"))
	{
		return EWBCardActivationTimingRequirement::NormalTurnPriority;
	}

	bOutSupported = false;
	return EWBCardActivationTimingRequirement::Unknown;
}

EWBStatusEffectOp ParseStatusOperation(
	const FString& Operation,
	bool& bOutSupported)
{
	bOutSupported = true;
	if (Operation == TEXT("apply_status"))
	{
		return EWBStatusEffectOp::ApplyStatus;
	}
	if (Operation == TEXT("remove_status"))
	{
		return EWBStatusEffectOp::RemoveStatus;
	}
	if (Operation == TEXT("set_status_duration"))
	{
		return EWBStatusEffectOp::SetStatusDuration;
	}
	if (Operation == TEXT("add_status_duration"))
	{
		return EWBStatusEffectOp::AddStatusDuration;
	}
	if (Operation == TEXT("reduce_status_duration"))
	{
		return EWBStatusEffectOp::ReduceStatusDuration;
	}
	if (Operation == TEXT("cleanse_status"))
	{
		return EWBStatusEffectOp::CleanseStatus;
	}
	if (Operation == TEXT("cleanse_all_statuses"))
	{
		return EWBStatusEffectOp::CleanseAllStatuses;
	}

	bOutSupported = false;
	return EWBStatusEffectOp::Unknown;
}

EWBArmorEffectOp ParseArmorOperation(
	const FString& Operation,
	bool& bOutSupported)
{
	bOutSupported = true;
	if (Operation == TEXT("add_current_armor"))
	{
		return EWBArmorEffectOp::AddCurrentArmor;
	}
	if (Operation == TEXT("reduce_current_armor"))
	{
		return EWBArmorEffectOp::ReduceCurrentArmor;
	}
	if (Operation == TEXT("set_current_armor"))
	{
		return EWBArmorEffectOp::SetCurrentArmor;
	}
	if (Operation == TEXT("add_max_armor"))
	{
		return EWBArmorEffectOp::AddMaxArmor;
	}
	if (Operation == TEXT("reduce_max_armor"))
	{
		return EWBArmorEffectOp::ReduceMaxArmor;
	}
	if (Operation == TEXT("set_max_armor"))
	{
		return EWBArmorEffectOp::SetMaxArmor;
	}
	if (Operation == TEXT("restore_armor_to_max"))
	{
		return EWBArmorEffectOp::RestoreArmorToMax;
	}

	bOutSupported = false;
	return EWBArmorEffectOp::Unknown;
}

bool IsSupportedStatusId(const FString& StatusId)
{
	return StatusId == TEXT("Burn")
		|| StatusId == TEXT("Poison")
		|| StatusId == TEXT("Rooted")
		|| StatusId == TEXT("Stunned")
		|| StatusId == TEXT("Frozen")
		|| StatusId == TEXT("CannotAttack")
		|| StatusId == TEXT("Cannot Attack")
		|| StatusId == TEXT("no_attack");
}

bool ValidateOptionalSelectedTarget(
	const TSharedPtr<FJsonObject>& Object,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	const TSharedPtr<FJsonValue>* Value = Object.IsValid() ? Object->Values.Find(TEXT("target")) : nullptr;
	if (Value == nullptr)
	{
		return true;
	}

	FString Target;
	if (!TryReadOptionalString(Object, TEXT("target"), Result, TEXT("unsupported_payload_operation"), CardId, EffectId, Path, Target))
	{
		return false;
	}

	if (!Target.IsEmpty() && Target != TEXT("selected"))
	{
		AddDiagnostic(Result, TEXT("unsupported_payload_operation"), CardId, EffectId, JoinPath(Path, TEXT("target")));
		return false;
	}

	return true;
}

void ParseCostGate(
	const TSharedPtr<FJsonObject>& Object,
	FWBCardActivationSourceGateDefinition& OutGate,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	ValidateKnownFields(
		Object,
		{ TEXT("requires_external_affordability"), TEXT("required_rr"), TEXT("cost_kind") },
		Result,
		CardId,
		EffectId,
		Path);

	bool bRequiresExternalAffordability = false;
	if (TryReadOptionalBool(Object, TEXT("requires_external_affordability"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, bRequiresExternalAffordability))
	{
		OutGate.CostGate.bRequiresExternalAffordability = bRequiresExternalAffordability;
	}

	int32 RequiredRR = 0;
	if (TryReadOptionalInteger(Object, TEXT("required_rr"), Result, TEXT("invalid_rr_cost"), CardId, EffectId, Path, RequiredRR))
	{
		if (RequiredRR < 0)
		{
			AddDiagnostic(Result, TEXT("invalid_rr_cost"), CardId, EffectId, JoinPath(Path, TEXT("required_rr")));
		}
		else
		{
			OutGate.CostGate.RequiredRR = RequiredRR;
		}
	}

	FString CostKind;
	if (TryReadOptionalString(Object, TEXT("cost_kind"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, CostKind))
	{
		OutGate.CostGate.CostKind = FName(*CostKind);
	}
}

void ParseSourceGate(
	const TSharedPtr<FJsonObject>& Object,
	FWBCardEffectDefinition& OutEffect,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.bHasExplicitSourceGate = true;

	ValidateKnownFields(
		Object,
		{
			TEXT("required_zone"),
			TEXT("timing"),
			TEXT("requires_source_unit"),
			TEXT("requires_source_unit_ownership"),
			TEXT("blocked_by_stunned"),
			TEXT("blocked_by_frozen"),
			TEXT("once_per_turn"),
			TEXT("once_per_turn_key"),
			TEXT("requires_fixture_zone_ownership"),
			TEXT("requires_costs_satisfied_externally"),
			TEXT("cost_gate")
		},
		Result,
		CardId,
		EffectId,
		Path);

	FString Zone;
	if (TryReadOptionalString(Object, TEXT("required_zone"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Zone))
	{
		bool bSupported = false;
		Gate.RequiredZone = ParseSourceZone(Zone, bSupported);
		if (!bSupported)
		{
			AddDiagnostic(Result, TEXT("unsupported_source_zone"), CardId, EffectId, JoinPath(Path, TEXT("required_zone")));
		}
	}

	FString Timing;
	if (TryReadOptionalString(Object, TEXT("timing"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Timing))
	{
		bool bSupported = false;
		Gate.Timing = ParseTiming(Timing, bSupported);
		if (!bSupported)
		{
			AddDiagnostic(Result, TEXT("unsupported_timing"), CardId, EffectId, JoinPath(Path, TEXT("timing")));
		}
	}

	TryReadOptionalBool(Object, TEXT("requires_source_unit"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bRequiresSourceUnit);
	TryReadOptionalBool(Object, TEXT("requires_source_unit_ownership"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bRequiresSourceUnitOwnership);
	TryReadOptionalBool(Object, TEXT("blocked_by_stunned"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bBlockedByStunned);
	TryReadOptionalBool(Object, TEXT("blocked_by_frozen"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bBlockedByFrozen);
	TryReadOptionalBool(Object, TEXT("once_per_turn"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bOncePerTurn);
	TryReadOptionalBool(Object, TEXT("requires_fixture_zone_ownership"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bRequiresFixtureZoneOwnership);
	TryReadOptionalBool(Object, TEXT("requires_costs_satisfied_externally"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, Gate.bRequiresCostsSatisfiedExternally);

	FString OncePerTurnKey;
	if (TryReadOptionalString(Object, TEXT("once_per_turn_key"), Result, TEXT("source_gate_malformed"), CardId, EffectId, Path, OncePerTurnKey))
	{
		Gate.OncePerTurnKey = OncePerTurnKey;
	}

	TSharedPtr<FJsonObject> CostGateObject;
	const TSharedPtr<FJsonValue>* CostGateValue = Object.IsValid() ? Object->Values.Find(TEXT("cost_gate")) : nullptr;
	if (CostGateValue != nullptr)
	{
		if (!TryGetObjectField(Object, TEXT("cost_gate"), CostGateObject))
		{
			AddDiagnostic(Result, TEXT("source_gate_malformed"), CardId, EffectId, JoinPath(Path, TEXT("cost_gate")));
		}
		else
		{
			ParseCostGate(CostGateObject, Gate, Result, CardId, EffectId, JoinPath(Path, TEXT("cost_gate")));
		}
	}

	OutEffect.SourceGate = Gate;
}

void ParseDamagePayload(
	const TSharedPtr<FJsonObject>& Object,
	FWBGenericEffectPayload& OutPayload,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	ValidateKnownFields(Object, { TEXT("type"), TEXT("amount"), TEXT("bypass_armor"), TEXT("damage_cause"), TEXT("target") }, Result, CardId, EffectId, Path);

	int32 Amount = 0;
	if (!TryReadRequiredInteger(Object, TEXT("amount"), Result, TEXT("invalid_numeric_field"), CardId, EffectId, Path, Amount) || Amount < 0)
	{
		if (Amount < 0)
		{
			AddDiagnostic(Result, TEXT("invalid_numeric_field"), CardId, EffectId, JoinPath(Path, TEXT("amount")));
		}
		return;
	}

	bool bBypassArmor = false;
	TryReadOptionalBool(Object, TEXT("bypass_armor"), Result, TEXT("unsupported_payload_operation"), CardId, EffectId, Path, bBypassArmor);
	FString DamageCause = TEXT("Effect");
	TryReadOptionalString(Object, TEXT("damage_cause"), Result, TEXT("unsupported_payload_operation"), CardId, EffectId, Path, DamageCause);
	ValidateOptionalSelectedTarget(Object, Result, CardId, EffectId, Path);

	OutPayload.Operation = EWBGenericEffectOp::DamageEffect;
	OutPayload.DamageEffect.Amount = Amount;
	OutPayload.DamageEffect.bBypassArmor = bBypassArmor;
	OutPayload.DamageEffect.DamageCause = FName(*DamageCause);
	OutPayload.DamageEffect.SourceReason = FName(TEXT("fixture_loader"));
}

void ParseHealPayload(
	const TSharedPtr<FJsonObject>& Object,
	FWBGenericEffectPayload& OutPayload,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	ValidateKnownFields(Object, { TEXT("type"), TEXT("amount"), TEXT("target") }, Result, CardId, EffectId, Path);

	int32 Amount = 0;
	if (!TryReadRequiredInteger(Object, TEXT("amount"), Result, TEXT("invalid_numeric_field"), CardId, EffectId, Path, Amount) || Amount < 0)
	{
		if (Amount < 0)
		{
			AddDiagnostic(Result, TEXT("invalid_numeric_field"), CardId, EffectId, JoinPath(Path, TEXT("amount")));
		}
		return;
	}

	ValidateOptionalSelectedTarget(Object, Result, CardId, EffectId, Path);

	OutPayload.Operation = EWBGenericEffectOp::HealEffect;
	OutPayload.HealEffect.Amount = Amount;
	OutPayload.HealEffect.SourceReason = FName(TEXT("fixture_loader"));
}

void ParseStatusPayload(
	const TSharedPtr<FJsonObject>& Object,
	FWBGenericEffectPayload& OutPayload,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	ValidateKnownFields(Object, { TEXT("type"), TEXT("operation"), TEXT("status_id"), TEXT("turns_remaining"), TEXT("target") }, Result, CardId, EffectId, Path);

	FString OperationString;
	if (!TryReadRequiredString(Object, TEXT("operation"), Result, TEXT("unsupported_payload_operation"), CardId, EffectId, Path, OperationString))
	{
		return;
	}

	bool bSupportedOperation = false;
	const EWBStatusEffectOp Operation = ParseStatusOperation(OperationString, bSupportedOperation);
	if (!bSupportedOperation)
	{
		AddDiagnostic(Result, TEXT("unsupported_payload_operation"), CardId, EffectId, JoinPath(Path, TEXT("operation")));
		return;
	}

	FString StatusId;
	if (Operation != EWBStatusEffectOp::CleanseAllStatuses)
	{
		if (!TryReadRequiredString(Object, TEXT("status_id"), Result, TEXT("invalid_status_id"), CardId, EffectId, Path, StatusId))
		{
			return;
		}

		if (!IsSupportedStatusId(StatusId))
		{
			AddDiagnostic(Result, TEXT("invalid_status_id"), CardId, EffectId, JoinPath(Path, TEXT("status_id")));
			return;
		}
	}

	int32 TurnsRemaining = 0;
	if (Operation == EWBStatusEffectOp::ApplyStatus
		|| Operation == EWBStatusEffectOp::SetStatusDuration
		|| Operation == EWBStatusEffectOp::AddStatusDuration
		|| Operation == EWBStatusEffectOp::ReduceStatusDuration)
	{
		if (!TryReadRequiredInteger(Object, TEXT("turns_remaining"), Result, TEXT("invalid_numeric_field"), CardId, EffectId, Path, TurnsRemaining) || TurnsRemaining < 0)
		{
			if (TurnsRemaining < 0)
			{
				AddDiagnostic(Result, TEXT("invalid_numeric_field"), CardId, EffectId, JoinPath(Path, TEXT("turns_remaining")));
			}
			return;
		}
	}
	else
	{
		TryReadOptionalInteger(Object, TEXT("turns_remaining"), Result, TEXT("invalid_numeric_field"), CardId, EffectId, Path, TurnsRemaining);
	}

	ValidateOptionalSelectedTarget(Object, Result, CardId, EffectId, Path);

	OutPayload.Operation = EWBGenericEffectOp::StatusEffect;
	OutPayload.StatusEffect.Operation = Operation;
	OutPayload.StatusEffect.StatusId = FName(*StatusId);
	OutPayload.StatusEffect.Duration = TurnsRemaining;
	OutPayload.StatusEffect.SourceReason = FName(TEXT("fixture_loader"));
}

void ParseArmorPayload(
	const TSharedPtr<FJsonObject>& Object,
	FWBGenericEffectPayload& OutPayload,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	ValidateKnownFields(Object, { TEXT("type"), TEXT("operation"), TEXT("amount"), TEXT("target") }, Result, CardId, EffectId, Path);

	FString OperationString;
	if (!TryReadRequiredString(Object, TEXT("operation"), Result, TEXT("unsupported_payload_operation"), CardId, EffectId, Path, OperationString))
	{
		return;
	}

	bool bSupportedOperation = false;
	const EWBArmorEffectOp Operation = ParseArmorOperation(OperationString, bSupportedOperation);
	if (!bSupportedOperation)
	{
		AddDiagnostic(Result, TEXT("unsupported_payload_operation"), CardId, EffectId, JoinPath(Path, TEXT("operation")));
		return;
	}

	int32 Amount = 0;
	if (Operation != EWBArmorEffectOp::RestoreArmorToMax)
	{
		if (!TryReadRequiredInteger(Object, TEXT("amount"), Result, TEXT("invalid_numeric_field"), CardId, EffectId, Path, Amount) || Amount < 0)
		{
			if (Amount < 0)
			{
				AddDiagnostic(Result, TEXT("invalid_numeric_field"), CardId, EffectId, JoinPath(Path, TEXT("amount")));
			}
			return;
		}
	}
	else
	{
		TryReadOptionalInteger(Object, TEXT("amount"), Result, TEXT("invalid_numeric_field"), CardId, EffectId, Path, Amount);
	}

	ValidateOptionalSelectedTarget(Object, Result, CardId, EffectId, Path);

	OutPayload.Operation = EWBGenericEffectOp::ArmorEffect;
	OutPayload.ArmorEffect.Operation = Operation;
	OutPayload.ArmorEffect.Amount = Amount;
	OutPayload.ArmorEffect.SourceReason = FName(TEXT("fixture_loader"));
}

void ParsePayload(
	const TSharedPtr<FJsonObject>& Object,
	FWBGenericEffectPayload& OutPayload,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& EffectId,
	const FString& Path)
{
	if (!Object.IsValid())
	{
		AddDiagnostic(Result, TEXT("payloads_malformed"), CardId, EffectId, Path);
		return;
	}

	FString Type;
	if (!TryReadRequiredString(Object, TEXT("type"), Result, TEXT("unsupported_payload_type"), CardId, EffectId, Path, Type))
	{
		return;
	}

	if (Type == TEXT("damage_effect"))
	{
		ParseDamagePayload(Object, OutPayload, Result, CardId, EffectId, Path);
		return;
	}
	if (Type == TEXT("heal_effect"))
	{
		ParseHealPayload(Object, OutPayload, Result, CardId, EffectId, Path);
		return;
	}
	if (Type == TEXT("status_effect"))
	{
		ParseStatusPayload(Object, OutPayload, Result, CardId, EffectId, Path);
		return;
	}
	if (Type == TEXT("armor_effect"))
	{
		ParseArmorPayload(Object, OutPayload, Result, CardId, EffectId, Path);
		return;
	}

	AddDiagnostic(Result, TEXT("unsupported_payload_type"), CardId, EffectId, Path);
}

void ParseEffect(
	const TSharedPtr<FJsonObject>& Object,
	FWBCardEffectDefinition& OutEffect,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& CardId,
	const FString& Path)
{
	if (!Object.IsValid())
	{
		AddDiagnostic(Result, TEXT("activated_effects_malformed"), CardId, TEXT(""), Path);
		return;
	}

	FString EffectId;
	TryReadRequiredString(Object, TEXT("effect_id"), Result, TEXT("effect_id_missing"), CardId, TEXT(""), Path, EffectId);
	OutEffect.EffectId = EffectId;

	ValidateKnownFields(
		Object,
		{ TEXT("effect_id"), TEXT("public_label"), TEXT("target_requirement"), TEXT("source_gate"), TEXT("payloads") },
		Result,
		CardId,
		EffectId,
		Path);

	TryReadRequiredString(Object, TEXT("public_label"), Result, TEXT("public_label_missing"), CardId, EffectId, Path, OutEffect.PublicLabel);

	FString TargetRequirement;
	if (TryReadRequiredString(Object, TEXT("target_requirement"), Result, TEXT("unsupported_target_requirement"), CardId, EffectId, Path, TargetRequirement))
	{
		bool bSupportedTargetRequirement = false;
		OutEffect.TargetRequirement = ParseTargetRequirement(TargetRequirement, bSupportedTargetRequirement);
		if (!bSupportedTargetRequirement)
		{
			AddDiagnostic(Result, TEXT("unsupported_target_requirement"), CardId, EffectId, JoinPath(Path, TEXT("target_requirement")));
		}
	}

	TSharedPtr<FJsonObject> SourceGateObject;
	const TSharedPtr<FJsonValue>* SourceGateValue = Object->Values.Find(TEXT("source_gate"));
	if (SourceGateValue != nullptr)
	{
		if (!TryGetObjectField(Object, TEXT("source_gate"), SourceGateObject))
		{
			AddDiagnostic(Result, TEXT("source_gate_malformed"), CardId, EffectId, JoinPath(Path, TEXT("source_gate")));
		}
		else
		{
			ParseSourceGate(SourceGateObject, OutEffect, Result, CardId, EffectId, JoinPath(Path, TEXT("source_gate")));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* PayloadValues = nullptr;
	const TSharedPtr<FJsonValue>* PayloadsValue = Object->Values.Find(TEXT("payloads"));
	if (PayloadsValue == nullptr)
	{
		AddDiagnostic(Result, TEXT("payloads_missing"), CardId, EffectId, JoinPath(Path, TEXT("payloads")));
		return;
	}

	if (!TryGetArrayField(Object, TEXT("payloads"), PayloadValues))
	{
		AddDiagnostic(Result, TEXT("payloads_malformed"), CardId, EffectId, JoinPath(Path, TEXT("payloads")));
		return;
	}

	if (PayloadValues->IsEmpty())
	{
		AddDiagnostic(Result, TEXT("payloads_empty"), CardId, EffectId, JoinPath(Path, TEXT("payloads")));
		return;
	}

	for (int32 PayloadIndex = 0; PayloadIndex < PayloadValues->Num(); ++PayloadIndex)
	{
		const FString PayloadPath = JoinPath(JoinPath(Path, TEXT("payloads")), FString::Printf(TEXT("[%d]"), PayloadIndex));
		const TSharedPtr<FJsonObject> PayloadObject = (*PayloadValues)[PayloadIndex].IsValid()
			? (*PayloadValues)[PayloadIndex]->AsObject()
			: nullptr;

		FWBGenericEffectPayload Payload;
		ParsePayload(PayloadObject, Payload, Result, CardId, EffectId, PayloadPath);
		OutEffect.Payloads.Add(Payload);
	}
}

void ParseCard(
	const TSharedPtr<FJsonObject>& Object,
	FWBCardDefinition& OutDefinition,
	FWBCardDefinitionFixtureLoadResult& Result,
	const FString& Path)
{
	if (!Object.IsValid())
	{
		AddDiagnostic(Result, TEXT("cards_malformed"), TEXT(""), TEXT(""), Path);
		return;
	}

	FString CardId;
	TryReadRequiredString(Object, TEXT("card_id"), Result, TEXT("card_id_missing"), TEXT(""), TEXT(""), Path, CardId);
	OutDefinition.CardId = CardId;

	ValidateKnownFields(
		Object,
		{
			TEXT("card_id"),
			TEXT("public_name"),
			TEXT("kind"),
			TEXT("character_stats"),
			TEXT("wand_stats"),
			TEXT("activated_effects")
		},
		Result,
		CardId,
		TEXT(""),
		Path);

	TryReadRequiredString(Object, TEXT("public_name"), Result, TEXT("public_name_missing"), CardId, TEXT(""), Path, OutDefinition.PublicName);

	FString Kind;
	if (TryReadOptionalString(Object, TEXT("kind"), Result, TEXT("unsupported_card_kind"), CardId, TEXT(""), Path, Kind))
	{
		bool bSupportedKind = false;
		OutDefinition.Kind = ParseCardKind(Kind, bSupportedKind);
		if (!bSupportedKind)
		{
			AddDiagnostic(Result, TEXT("unsupported_card_kind"), CardId, TEXT(""), JoinPath(Path, TEXT("kind")));
		}
	}
	else if (Kind.IsEmpty())
	{
		OutDefinition.Kind = EWBCardDefinitionKind::Fixture;
	}

	if (OutDefinition.Kind == EWBCardDefinitionKind::Character)
	{
		if (Object->Values.Contains(TEXT("character_stats")))
		{
			ParseCharacterStats(Object, OutDefinition, Result, CardId, Path);
		}
		else if (Object->Values.Contains(TEXT("activated_effects")))
		{
			OutDefinition.Kind = EWBCardDefinitionKind::Fixture;
		}
		else
		{
			ParseCharacterStats(Object, OutDefinition, Result, CardId, Path);
		}
	}
	else if (OutDefinition.Kind == EWBCardDefinitionKind::Wand)
	{
		if (Object->Values.Contains(TEXT("wand_stats")))
		{
			ParseWandStats(Object, OutDefinition, Result, CardId, Path);
		}
		else if (Object->Values.Contains(TEXT("activated_effects")))
		{
			OutDefinition.Kind = EWBCardDefinitionKind::Fixture;
		}
		else
		{
			ParseWandStats(Object, OutDefinition, Result, CardId, Path);
		}
	}

	const TSharedPtr<FJsonValue>* EffectsValue = Object->Values.Find(TEXT("activated_effects"));
	if (EffectsValue == nullptr)
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* EffectValues = nullptr;
	if (!TryGetArrayField(Object, TEXT("activated_effects"), EffectValues))
	{
		AddDiagnostic(Result, TEXT("activated_effects_malformed"), CardId, TEXT(""), JoinPath(Path, TEXT("activated_effects")));
		return;
	}

	for (int32 EffectIndex = 0; EffectIndex < EffectValues->Num(); ++EffectIndex)
	{
		const FString EffectPath = JoinPath(JoinPath(Path, TEXT("activated_effects")), FString::Printf(TEXT("[%d]"), EffectIndex));
		const TSharedPtr<FJsonObject> EffectObject = (*EffectValues)[EffectIndex].IsValid()
			? (*EffectValues)[EffectIndex]->AsObject()
			: nullptr;

		FWBCardEffectDefinition Effect;
		ParseEffect(EffectObject, Effect, Result, CardId, EffectPath);
		OutDefinition.ActivatedEffects.Add(Effect);
	}
}

void AddRepositoryValidationDiagnostic(
	const FWBCardDefinitionRepository& Repository,
	const FWBCardDefinitionRepositoryValidationResult& ValidationResult,
	FWBCardDefinitionFixtureLoadResult& Result)
{
	if (ValidationResult.bOk || ValidationResult.Reason.IsEmpty())
	{
		return;
	}

	FString CardId;
	FString EffectId;
	FString Path = TEXT("$.cards");

	if (ValidationResult.Reason == TEXT("duplicate_card_id"))
	{
		WBCardDefinitionRepository::HasDuplicateCardIds(Repository, CardId);
	}
	else
	{
		for (int32 CardIndex = 0; CardIndex < Repository.Definitions.Num(); ++CardIndex)
		{
			const FWBCardDefinition& Definition = Repository.Definitions[CardIndex];
			const FString CardPath = FString::Printf(TEXT("$.cards[%d]"), CardIndex);
			if (ValidationResult.Reason == TEXT("card_id_missing") && Definition.CardId.IsEmpty())
			{
				Path = JoinPath(CardPath, TEXT("card_id"));
				break;
			}
			if (ValidationResult.Reason == TEXT("public_name_missing") && Definition.PublicName.IsEmpty())
			{
				CardId = Definition.CardId;
				Path = JoinPath(CardPath, TEXT("public_name"));
				break;
			}

			FString ForbiddenTerm;
			if (ValidationResult.Reason == TEXT("public_label_contains_internal_term")
				&& WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(Definition.PublicName, ForbiddenTerm))
			{
				CardId = Definition.CardId;
				Path = JoinPath(CardPath, TEXT("public_name"));
				break;
			}
			if (ValidationResult.Reason == TEXT("invalid_character_stats")
				&& Definition.Kind == EWBCardDefinitionKind::Character)
			{
				CardId = Definition.CardId;
				Path = JoinPath(CardPath, TEXT("character_stats"));
				break;
			}
			if (ValidationResult.Reason == TEXT("invalid_wand_stats")
				&& Definition.Kind == EWBCardDefinitionKind::Wand)
			{
				CardId = Definition.CardId;
				Path = JoinPath(CardPath, TEXT("wand_stats"));
				break;
			}

			TSet<FString> SeenEffectIds;
			for (int32 EffectIndex = 0; EffectIndex < Definition.ActivatedEffects.Num(); ++EffectIndex)
			{
				const FWBCardEffectDefinition& Effect = Definition.ActivatedEffects[EffectIndex];
				const FString EffectPath = JoinPath(JoinPath(CardPath, TEXT("activated_effects")), FString::Printf(TEXT("[%d]"), EffectIndex));
				if (ValidationResult.Reason == TEXT("effect_id_missing") && Effect.EffectId.IsEmpty())
				{
					CardId = Definition.CardId;
					Path = JoinPath(EffectPath, TEXT("effect_id"));
					break;
				}
				if (ValidationResult.Reason == TEXT("duplicate_effect_id") && SeenEffectIds.Contains(Effect.EffectId))
				{
					CardId = Definition.CardId;
					EffectId = Effect.EffectId;
					Path = JoinPath(EffectPath, TEXT("effect_id"));
					break;
				}
				if (ValidationResult.Reason == TEXT("public_label_contains_internal_term")
					&& WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(Effect.PublicLabel, ForbiddenTerm))
				{
					CardId = Definition.CardId;
					EffectId = Effect.EffectId;
					Path = JoinPath(EffectPath, TEXT("public_label"));
					break;
				}
				if (!Effect.EffectId.IsEmpty())
				{
					SeenEffectIds.Add(Effect.EffectId);
				}
			}

			if (Path != TEXT("$.cards"))
			{
				break;
			}
		}
	}

	AddDiagnostic(Result, ValidationResult.Reason, CardId, EffectId, Path);
	AddDiagnostic(Result, TEXT("repository_validation_failed"), CardId, EffectId, TEXT("$.repository"));
}

TSharedRef<FJsonObject> DiagnosticToJsonObject(const FWBCardDefinitionFixtureLoadDiagnostic& Diagnostic)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("code"), Diagnostic.Code);
	Object->SetStringField(TEXT("card_id"), Diagnostic.CardId);
	Object->SetStringField(TEXT("effect_id"), Diagnostic.EffectId);
	Object->SetStringField(TEXT("path"), Diagnostic.Path);
	return Object;
}
}

FWBCardDefinitionFixtureLoadResult WBCardDefinitionFixtureLoader::LoadRepositoryFromJsonString(
	const FString& Json,
	const FString& SourcePathForDiagnostics)
{
	FWBCardDefinitionFixtureLoadResult Result;
	Result.SourcePath = SourcePathForDiagnostics;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddDiagnostic(Result, TEXT("json_parse_failed"), TEXT(""), TEXT(""), TEXT("$"));
		MarkFailedIfDiagnostics(Result);
		return Result;
	}

	ValidateKnownFields(
		RootObject,
		{ TEXT("repository_id"), TEXT("source_version"), TEXT("cards") },
		Result,
		TEXT(""),
		TEXT(""),
		TEXT("$"));

	FString RepositoryId;
	TryReadRequiredString(RootObject, TEXT("repository_id"), Result, TEXT("repository_id_missing"), TEXT(""), TEXT(""), TEXT("$"), RepositoryId);

	FString SourceVersion;
	TryReadOptionalString(RootObject, TEXT("source_version"), Result, TEXT("source_gate_malformed"), TEXT(""), TEXT(""), TEXT("$"), SourceVersion);

	const TSharedPtr<FJsonValue>* CardsValue = RootObject->Values.Find(TEXT("cards"));
	if (CardsValue == nullptr)
	{
		AddDiagnostic(Result, TEXT("cards_missing"), TEXT(""), TEXT(""), TEXT("$.cards"));
		MarkFailedIfDiagnostics(Result);
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* CardValues = nullptr;
	if (!TryGetArrayField(RootObject, TEXT("cards"), CardValues))
	{
		AddDiagnostic(Result, TEXT("cards_malformed"), TEXT(""), TEXT(""), TEXT("$.cards"));
		MarkFailedIfDiagnostics(Result);
		return Result;
	}

	if (CardValues->IsEmpty())
	{
		AddDiagnostic(Result, TEXT("cards_empty"), TEXT(""), TEXT(""), TEXT("$.cards"));
		MarkFailedIfDiagnostics(Result);
		return Result;
	}

	TArray<FWBCardDefinition> Definitions;
	for (int32 CardIndex = 0; CardIndex < CardValues->Num(); ++CardIndex)
	{
		const FString CardPath = FString::Printf(TEXT("$.cards[%d]"), CardIndex);
		const TSharedPtr<FJsonObject> CardObject = (*CardValues)[CardIndex].IsValid()
			? (*CardValues)[CardIndex]->AsObject()
			: nullptr;

		FWBCardDefinition Definition;
		ParseCard(CardObject, Definition, Result, CardPath);
		Definitions.Add(Definition);
	}

	FWBCardDefinitionRepository CandidateRepository;
	CandidateRepository.RepositoryId = RepositoryId;
	CandidateRepository.SourceVersion = SourceVersion;
	CandidateRepository.Definitions = Definitions;

	if (Result.Diagnostics.IsEmpty())
	{
		const FWBCardDefinitionRepositoryValidationResult ValidationResult =
			WBCardDefinitionRepository::ValidateRepository(CandidateRepository);
		if (!ValidationResult.bOk)
		{
			AddRepositoryValidationDiagnostic(CandidateRepository, ValidationResult, Result);
		}
	}

	if (!Result.Diagnostics.IsEmpty())
	{
		MarkFailedIfDiagnostics(Result);
		return Result;
	}

	Result.bOk = true;
	Result.Repository = CandidateRepository;
	return Result;
}

FWBCardDefinitionFixtureLoadResult WBCardDefinitionFixtureLoader::LoadRepositoryFromFile(
	const FString& AbsolutePath)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath))
	{
		FWBCardDefinitionFixtureLoadResult Result;
		Result.SourcePath = AbsolutePath;
		AddDiagnostic(Result, TEXT("file_load_failed"), TEXT(""), TEXT(""), TEXT("$"));
		MarkFailedIfDiagnostics(Result);
		return Result;
	}

	return LoadRepositoryFromJsonString(Json, AbsolutePath);
}

bool WBCardDefinitionFixtureLoader::FixtureLoadResultToJsonStringForTest(
	const FWBCardDefinitionFixtureLoadResult& Result,
	FString& OutJson)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetBoolField(TEXT("ok"), Result.bOk);
	RootObject->SetStringField(TEXT("reason"), Result.Reason);
	RootObject->SetStringField(TEXT("source_path"), FPaths::GetCleanFilename(Result.SourcePath));
	RootObject->SetStringField(TEXT("repository_id"), Result.bOk ? Result.Repository.RepositoryId : FString());
	RootObject->SetStringField(TEXT("source_version"), Result.bOk ? Result.Repository.SourceVersion : FString());
	RootObject->SetNumberField(TEXT("card_count"), Result.bOk ? Result.Repository.Definitions.Num() : 0);

	TArray<TSharedPtr<FJsonValue>> CardIdValues;
	if (Result.bOk)
	{
		for (const FString& CardId : WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(Result.Repository))
		{
			CardIdValues.Add(MakeShared<FJsonValueString>(CardId));
		}
	}
	RootObject->SetArrayField(TEXT("card_ids"), CardIdValues);

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FWBCardDefinitionFixtureLoadDiagnostic& Diagnostic : Result.Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(DiagnosticToJsonObject(Diagnostic)));
	}
	RootObject->SetArrayField(TEXT("diagnostics"), DiagnosticValues);

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	return FJsonSerializer::Serialize(RootObject, Writer);
}
