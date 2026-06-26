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
	InvalidRRCost,
	InvalidStatusId,
	HiddenInfoPolicyViolation,
	PlayerFacingLabelContainsInternalTerm,
	JsonParseFailed
};

struct FWBCardDBSchemaValidationDiagnostic
{
	EWBCardDBSchemaDiagnostic Code = EWBCardDBSchemaDiagnostic::None;
	FString Message;
	FString CardId;
	FString EffectId;
};

struct FWBCardDBSchemaValidationResult
{
	bool bOk = false;
	FString SourcePath;
	TArray<FWBCardDBSchemaValidationDiagnostic> Diagnostics;

	FWBCardDefinition CardDefinition;
};

class FWBCardDBSchemaFixtureValidator
{
public:
	static FWBCardDBSchemaValidationResult ValidateFixtureFile(const FString& AbsolutePath);
	static FWBCardDBSchemaValidationResult ValidateJsonString(const FString& Json, const FString& SourcePathForDiagnostics);
	static FString DiagnosticCodeToString(EWBCardDBSchemaDiagnostic Code);
};
