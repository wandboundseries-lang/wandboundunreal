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
	BundleSchemaVersionUnsupported,
	MissingCardReference,
	MissingEffectReference,
	ReferenceMalformed,
	UnknownReferenceField,
	DependencyCycleDetected,
	DependencySelfReference
};

struct FWBCardDBSchemaValidationDiagnostic
{
	EWBCardDBSchemaDiagnostic Code = EWBCardDBSchemaDiagnostic::None;
	FString Message;
	FString CardId;
	FString EffectId;
	int32 CardIndex = -1;
	FString BundleCardId;
	FString JsonPath;
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
	int32 BundleCardCount = 0;
	TArray<FString> DependencyOrderCardIds;
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
	static bool BundleValidationResultToJsonStringForTest(
		const FWBCardDBBundleSchemaValidationResult& Result,
		FString& OutJson);
	static FString DiagnosticContextToStringForTest(const FWBCardDBSchemaValidationDiagnostic& Diagnostic);
	static bool ContainsDiagnosticWithContext(
		const TArray<FWBCardDBSchemaValidationDiagnostic>& Diagnostics,
		EWBCardDBSchemaDiagnostic Code,
		int32 ExpectedCardIndex,
		const FString& ExpectedCardId,
		const FString& ExpectedEffectId,
		const FString& ExpectedJsonPath);
	static FString DiagnosticCodeToString(EWBCardDBSchemaDiagnostic Code);
};
