#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinition.h"

enum class EWBCardDBSchemaDiagnostic : uint8
{
	None,
	SchemaVersionMissing,
	CardIdMissing,
	PublicNameMissing,
	EffectIdMissing,
	EffectIdDuplicate,
	UnsupportedCardKind,
	UnsupportedTargetRequirement,
	UnsupportedSourceZone,
	UnsupportedTiming,
	UnsupportedPayloadType,
	UnsupportedPayloadOperation,
	PayloadsMissing,
	PayloadsMalformed,
	PayloadsEmpty,
	ActivatedEffectsMalformed,
	SourceGateMalformed,
	CostGateMalformed,
	InvalidRRCost,
	InvalidStatusId,
	InvalidNumericField,
	UnknownTopLevelField,
	UnknownCardField,
	UnknownEffectField,
	UnknownSourceGateField,
	UnknownCostGateField,
	UnknownPayloadField,
	UnknownMetadataField,
	HiddenInfoPolicyViolation,
	PlayerFacingLabelContainsInternalTerm,
	JsonParseFailed,
	CardDBVersionMissing,
	CardsMissing,
	CardsMalformed,
	CardsEmpty,
	CardIdDuplicate,
	BundleSchemaVersionMissing,
	BundleSchemaVersionUnsupported
};

struct FWBCardDBSchemaValidationDiagnostic
{
	EWBCardDBSchemaDiagnostic Code = EWBCardDBSchemaDiagnostic::None;
	FString Message;
	FString CardId;
	FString EffectId;
};

struct FWBCardDBSchemaValidationOptions
{
	bool bStrictUnknownFieldRejection = false;
};

struct FWBCardDBSchemaValidationResult
{
	bool bOk = false;
	FString SourcePath;
	TArray<FWBCardDBSchemaValidationDiagnostic> Diagnostics;

	FWBCardDefinition CardDefinition;
};

struct FWBCardDBBundleSchemaValidationResult
{
	bool bOk = false;
	FString SourcePath;
	TArray<FWBCardDBSchemaValidationDiagnostic> Diagnostics;
	TArray<FWBCardDefinition> CardDefinitions;
};

class FWBCardDBSchemaFixtureValidator
{
public:
	static FWBCardDBSchemaValidationResult ValidateFixtureFile(
		const FString& AbsolutePath,
		const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions());
	static FWBCardDBSchemaValidationResult ValidateJsonString(
		const FString& Json,
		const FString& SourcePathForDiagnostics,
		const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions());
	static FWBCardDBBundleSchemaValidationResult ValidateBundleFixtureFile(
		const FString& AbsolutePath,
		const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions());
	static FWBCardDBBundleSchemaValidationResult ValidateBundleJsonString(
		const FString& Json,
		const FString& SourcePathForDiagnostics,
		const FWBCardDBSchemaValidationOptions& Options = FWBCardDBSchemaValidationOptions());
	static FString DiagnosticCodeToString(EWBCardDBSchemaDiagnostic Code);
};
