#include "WBCardDBImporterManifestForTests.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const int32 ManifestSchemaVersion = 1;

bool ContainsHiddenManifestTerm(const FString& Value)
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

FString SafeManifestIdentifier(const FString& Value)
{
	return ContainsHiddenManifestTerm(Value) ? FString() : Value;
}

FString FixtureRootPath()
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Reference"),
		TEXT("GodotCanon"),
		TEXT("CardDBSchemaFixtures"));
}

void NormalizePathForComparison(FString& Path)
{
	Path = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Path);
}

bool IsStringValueHidden(const TSharedPtr<FJsonValue>& Value)
{
	return Value.IsValid()
		&& Value->Type == EJson::String
		&& ContainsHiddenManifestTerm(Value->AsString());
}

void AddDiagnostic(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const EWBCardDBImporterManifestDiagnostic Code,
	const FString& ManifestId,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath)
{
	FWBCardDBImporterManifestDiagnosticForTest Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.ManifestId = SafeManifestIdentifier(ManifestId);
	Diagnostic.BatchName = SafeManifestIdentifier(BatchName);
	Diagnostic.BundleName = SafeManifestIdentifier(BundleName);
	Diagnostic.JsonPath = SafeManifestIdentifier(JsonPath);
	Result.Diagnostics.Add(Diagnostic);
}

void AddDiagnostic(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const EWBCardDBImporterManifestDiagnostic Code,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath)
{
	AddDiagnostic(Result, Code, Result.ManifestId, BatchName, BundleName, JsonPath);
}

const TArray<FString>& ManifestMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("notes"),
		TEXT("source"),
		TEXT("version"),
		TEXT("test_only")
	};
	return Fields;
}

const TArray<FString>& CompatibilityAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("enable"),
		TEXT("require_source_version"),
		TEXT("target_source_version"),
		TEXT("directly_supported_source_versions"),
		TEXT("supported_transitions")
	};
	return Fields;
}

const TArray<FString>& TransitionAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("from"),
		TEXT("to")
	};
	return Fields;
}

bool HasUnknownField(
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FString>& AllowedFields)
{
	if (!Object.IsValid())
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!AllowedFields.Contains(Pair.Key))
		{
			return true;
		}
	}

	return false;
}

void ValidateOptionalDescription(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath)
{
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::String)
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed, BatchName, BundleName, JsonPath);
		return;
	}

	if (ContainsHiddenManifestTerm((*Value)->AsString()))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, JsonPath);
	}
}

void ValidateOptionalMetadata(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath)
{
	if (!Object.IsValid() || !Object->HasField(TEXT("metadata")))
	{
		return;
	}

	const TSharedPtr<FJsonValue>* MetadataValue = Object->Values.Find(TEXT("metadata"));
	if (MetadataValue == nullptr || !MetadataValue->IsValid() || (*MetadataValue)->Type != EJson::Object)
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed, BatchName, BundleName, JsonPath);
		return;
	}

	const TSharedPtr<FJsonObject> MetadataObject = (*MetadataValue)->AsObject();
	if (!MetadataObject.IsValid() || HasUnknownField(MetadataObject, ManifestMetadataAllowedFields()))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed, BatchName, BundleName, JsonPath);
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : MetadataObject->Values)
	{
		const FString FieldPath = JsonPath + TEXT(".") + Pair.Key;
		if (Pair.Key == TEXT("test_only"))
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Boolean)
			{
				AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed, BatchName, BundleName, FieldPath);
			}
		}
		else if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed, BatchName, BundleName, FieldPath);
		}
		else if (ContainsHiddenManifestTerm(Pair.Value->AsString()))
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, FieldPath);
		}
	}
}

bool TryReadRequiredString(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const EWBCardDBImporterManifestDiagnostic MissingDiagnostic,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath,
	FString& OutValue)
{
	OutValue.Reset();
	const TSharedPtr<FJsonValue>* Value = Object.IsValid()
		? Object->Values.Find(FieldName)
		: nullptr;
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::String || (*Value)->AsString().IsEmpty())
	{
		AddDiagnostic(Result, MissingDiagnostic, BatchName, BundleName, JsonPath);
		return false;
	}

	OutValue = (*Value)->AsString();
	if (ContainsHiddenManifestTerm(OutValue))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, JsonPath);
	}

	return true;
}

bool IsCompatibilityObjectPresentAndValid(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath,
	TSharedPtr<FJsonObject>& OutCompatibilityObject)
{
	OutCompatibilityObject.Reset();
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Object)
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath);
		return false;
	}

	OutCompatibilityObject = (*Value)->AsObject();
	if (!OutCompatibilityObject.IsValid() || HasUnknownField(OutCompatibilityObject, CompatibilityAllowedFields()))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath);
	}

	return OutCompatibilityObject.IsValid();
}

void ApplyCompatibilityObject(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& CompatibilityObject,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath,
	FWBCardDBSchemaValidationOptions& Options)
{
	if (!CompatibilityObject.IsValid())
	{
		return;
	}

	FWBCardDBSourceVersionCompatibilityMatrixForTest& Matrix = Options.SourceVersionCompatibility;

	const TSharedPtr<FJsonValue>* EnableValue = CompatibilityObject->Values.Find(TEXT("enable"));
	if (EnableValue != nullptr)
	{
		if (!EnableValue->IsValid() || (*EnableValue)->Type != EJson::Boolean)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath + TEXT(".enable"));
		}
		else
		{
			Matrix.bEnableSourceVersionCompatibilityValidation = (*EnableValue)->AsBool();
		}
	}

	const TSharedPtr<FJsonValue>* RequireValue = CompatibilityObject->Values.Find(TEXT("require_source_version"));
	if (RequireValue != nullptr)
	{
		if (!RequireValue->IsValid() || (*RequireValue)->Type != EJson::Boolean)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath + TEXT(".require_source_version"));
		}
		else
		{
			Matrix.bRequireSourceVersion = (*RequireValue)->AsBool();
		}
	}

	const TSharedPtr<FJsonValue>* TargetValue = CompatibilityObject->Values.Find(TEXT("target_source_version"));
	if (TargetValue != nullptr)
	{
		if (!TargetValue->IsValid() || (*TargetValue)->Type != EJson::String)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath + TEXT(".target_source_version"));
		}
		else if (ContainsHiddenManifestTerm((*TargetValue)->AsString()))
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, JsonPath + TEXT(".target_source_version"));
		}
		else
		{
			Matrix.TargetSourceVersion = (*TargetValue)->AsString();
		}
	}

	const TSharedPtr<FJsonValue>* SupportedValue = CompatibilityObject->Values.Find(TEXT("directly_supported_source_versions"));
	if (SupportedValue != nullptr)
	{
		if (!SupportedValue->IsValid() || (*SupportedValue)->Type != EJson::Array)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath + TEXT(".directly_supported_source_versions"));
		}
		else
		{
			Matrix.DirectlySupportedSourceVersions.Reset();
			const TArray<TSharedPtr<FJsonValue>>& SupportedValues = (*SupportedValue)->AsArray();
			for (int32 Index = 0; Index < SupportedValues.Num(); ++Index)
			{
				const TSharedPtr<FJsonValue>& SourceVersionValue = SupportedValues[Index];
				const FString EntryPath = JsonPath + FString::Printf(TEXT(".directly_supported_source_versions[%d]"), Index);
				if (!SourceVersionValue.IsValid() || SourceVersionValue->Type != EJson::String)
				{
					AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, EntryPath);
				}
				else if (ContainsHiddenManifestTerm(SourceVersionValue->AsString()))
				{
					AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, EntryPath);
				}
				else
				{
					Matrix.DirectlySupportedSourceVersions.Add(SourceVersionValue->AsString());
				}
			}
		}
	}

	const TSharedPtr<FJsonValue>* TransitionsValue = CompatibilityObject->Values.Find(TEXT("supported_transitions"));
	if (TransitionsValue != nullptr)
	{
		if (!TransitionsValue->IsValid() || (*TransitionsValue)->Type != EJson::Array)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, JsonPath + TEXT(".supported_transitions"));
		}
		else
		{
			Matrix.SupportedTransitions.Reset();
			const TArray<TSharedPtr<FJsonValue>>& TransitionValues = (*TransitionsValue)->AsArray();
			for (int32 Index = 0; Index < TransitionValues.Num(); ++Index)
			{
				const TSharedPtr<FJsonValue>& TransitionValue = TransitionValues[Index];
				const FString TransitionPath = JsonPath + FString::Printf(TEXT(".supported_transitions[%d]"), Index);
				if (!TransitionValue.IsValid() || TransitionValue->Type != EJson::Object)
				{
					AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, TransitionPath);
					continue;
				}

				const TSharedPtr<FJsonObject> TransitionObject = TransitionValue->AsObject();
				if (!TransitionObject.IsValid() || HasUnknownField(TransitionObject, TransitionAllowedFields()))
				{
					AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, TransitionPath);
					continue;
				}

				const TSharedPtr<FJsonValue>* FromValue = TransitionObject->Values.Find(TEXT("from"));
				const TSharedPtr<FJsonValue>* ToValue = TransitionObject->Values.Find(TEXT("to"));
				if (FromValue == nullptr || ToValue == nullptr
					|| !FromValue->IsValid() || !ToValue->IsValid()
					|| (*FromValue)->Type != EJson::String || (*ToValue)->Type != EJson::String)
				{
					AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed, BatchName, BundleName, TransitionPath);
					continue;
				}

				if (IsStringValueHidden(*FromValue) || IsStringValueHidden(*ToValue))
				{
					AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, TransitionPath);
					continue;
				}

				FWBCardDBSourceVersionTransitionForTest Transition;
				Transition.FromSourceVersion = (*FromValue)->AsString();
				Transition.ToSourceVersion = (*ToValue)->AsString();
				Matrix.SupportedTransitions.Add(Transition);
			}
		}
	}
}

FWBCardDBSchemaValidationOptions ResolveCompatibilityOptions(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath,
	const FWBCardDBSchemaValidationOptions& InheritedOptions)
{
	FWBCardDBSchemaValidationOptions Options = InheritedOptions;
	TSharedPtr<FJsonObject> CompatibilityObject;
	if (IsCompatibilityObjectPresentAndValid(Result, Object, FieldName, BatchName, BundleName, JsonPath, CompatibilityObject))
	{
		ApplyCompatibilityObject(Result, CompatibilityObject, BatchName, BundleName, JsonPath, Options);
	}
	return Options;
}

bool IsRelativeBundlePathSafe(const FString& RelativePath)
{
	if (RelativePath.IsEmpty()
		|| RelativePath.Contains(TEXT(".."))
		|| RelativePath.Contains(TEXT(":"))
		|| RelativePath.StartsWith(TEXT("/"))
		|| RelativePath.StartsWith(TEXT("\\"))
		|| !FPaths::IsRelative(RelativePath)
		|| RelativePath.Contains(TEXT("Reference/GodotProject"), ESearchCase::IgnoreCase)
		|| RelativePath.Contains(TEXT("Reference\\GodotProject"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	return true;
}

bool DoesBundlePathResolveInsideFixtureRoot(
	const FString& FixtureRootAbsolutePath,
	const FString& RelativePath,
	FString& OutAbsolutePath)
{
	FString FixtureRoot = FixtureRootAbsolutePath;
	NormalizePathForComparison(FixtureRoot);

	OutAbsolutePath = FPaths::Combine(FixtureRoot, RelativePath);
	NormalizePathForComparison(OutAbsolutePath);

	const FString FixtureRootWithSlash = FixtureRoot.EndsWith(TEXT("/"))
		? FixtureRoot
		: FixtureRoot + TEXT("/");
	return OutAbsolutePath.StartsWith(FixtureRootWithSlash, ESearchCase::IgnoreCase);
}

bool ValidateBundlePath(
	FWBCardDBImporterManifestValidationResultForTest& Result,
	const FString& FixtureRootAbsolutePath,
	const FString& RelativePath,
	const FString& BatchName,
	const FString& BundleName,
	const FString& JsonPath)
{
	if (ContainsHiddenManifestTerm(RelativePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation, BatchName, BundleName, JsonPath);
		return false;
	}

	if (!IsRelativeBundlePathSafe(RelativePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestBundlePathInvalid, BatchName, BundleName, JsonPath);
		return false;
	}

	FString AbsolutePath;
	if (!DoesBundlePathResolveInsideFixtureRoot(FixtureRootAbsolutePath, RelativePath, AbsolutePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestBundlePathInvalid, BatchName, BundleName, JsonPath);
		return false;
	}

	if (!FPaths::FileExists(AbsolutePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestDiagnostic::ManifestBundlePathNotFound, BatchName, BundleName, JsonPath);
		return false;
	}

	return true;
}

void WriteReasonSummaries(
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer,
	const TArray<FWBCardDBImporterReasonSummaryForTest>& ReasonSummaries)
{
	Writer->WriteArrayStart(TEXT("reason_summaries"));
	for (const FWBCardDBImporterReasonSummaryForTest& ReasonSummary : ReasonSummaries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("reason"), SafeManifestIdentifier(ReasonSummary.Reason));
		Writer->WriteValue(TEXT("count"), ReasonSummary.Count);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
}

void WriteDiagnosticSummaries(
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer,
	const TArray<FWBCardDBImporterDiagnosticCodeSummaryForTest>& DiagnosticSummaries)
{
	Writer->WriteArrayStart(TEXT("diagnostic_summaries"));
	for (const FWBCardDBImporterDiagnosticCodeSummaryForTest& DiagnosticSummary : DiagnosticSummaries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("diagnostic_code"), SafeManifestIdentifier(DiagnosticSummary.DiagnosticCode));
		Writer->WriteValue(TEXT("count"), DiagnosticSummary.Count);
		Writer->WriteValue(TEXT("affected_bundle_count"), DiagnosticSummary.AffectedBundleCount);
		Writer->WriteValue(TEXT("affected_card_count"), DiagnosticSummary.AffectedCardCount);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
}
}

FWBCardDBImporterManifestValidationResultForTest FWBCardDBImporterManifestForTests::ValidateManifestFile(
	const FString& AbsolutePathToManifest)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePathToManifest))
	{
		FWBCardDBImporterManifestValidationResultForTest Result;
		Result.SourcePath = AbsolutePathToManifest;
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestJsonParseFailed,
			FString(),
			FString(),
			TEXT("$"));
		Result.Reason = TEXT("manifest_validation_failed");
		return Result;
	}

	return ValidateManifestJsonString(Json, AbsolutePathToManifest, FixtureRootPath());
}

FWBCardDBImporterManifestValidationResultForTest FWBCardDBImporterManifestForTests::ValidateManifestJsonString(
	const FString& Json,
	const FString& SourcePathForDiagnostics,
	const FString& FixtureRootAbsolutePath)
{
	FWBCardDBImporterManifestValidationResultForTest Result;
	Result.SourcePath = SourcePathForDiagnostics;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestJsonParseFailed,
			FString(),
			FString(),
			TEXT("$"));
		Result.Reason = TEXT("manifest_validation_failed");
		return Result;
	}

	FString ManifestId;
	if (TryReadRequiredString(
		Result,
		RootObject,
		TEXT("manifest_id"),
		EWBCardDBImporterManifestDiagnostic::ManifestIdMissing,
		FString(),
		FString(),
		TEXT("$.manifest_id"),
		ManifestId))
	{
		Result.ManifestId = SafeManifestIdentifier(ManifestId);
	}

	const TSharedPtr<FJsonValue>* SchemaVersionValue = RootObject->Values.Find(TEXT("manifest_schema_version"));
	if (SchemaVersionValue == nullptr || !SchemaVersionValue->IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestSchemaVersionMissing,
			FString(),
			FString(),
			TEXT("$.manifest_schema_version"));
	}
	else if ((*SchemaVersionValue)->Type != EJson::Number
		|| FMath::RoundToInt((*SchemaVersionValue)->AsNumber()) != ManifestSchemaVersion)
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestSchemaVersionUnsupported,
			FString(),
			FString(),
			TEXT("$.manifest_schema_version"));
	}

	ValidateOptionalDescription(Result, RootObject, TEXT("description"), FString(), FString(), TEXT("$.description"));
	ValidateOptionalMetadata(Result, RootObject, FString(), FString(), TEXT("$.metadata"));

	FWBCardDBSchemaValidationOptions DefaultOptions;
	DefaultOptions = ResolveCompatibilityOptions(
		Result,
		RootObject,
		TEXT("default_compatibility"),
		FString(),
		FString(),
		TEXT("$.default_compatibility"),
		DefaultOptions);

	const TArray<TSharedPtr<FJsonValue>>* BatchValues = nullptr;
	if (!RootObject->HasField(TEXT("batches")))
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestBatchesMissing,
			FString(),
			FString(),
			TEXT("$.batches"));
	}
	else if (!RootObject->TryGetArrayField(TEXT("batches"), BatchValues) || BatchValues == nullptr)
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestBatchesMalformed,
			FString(),
			FString(),
			TEXT("$.batches"));
	}
	else if (BatchValues->Num() == 0)
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestDiagnostic::ManifestBatchesMalformed,
			FString(),
			FString(),
			TEXT("$.batches"));
	}
	else
	{
		TSet<FString> SeenBatchNames;
		for (int32 BatchIndex = 0; BatchIndex < BatchValues->Num(); ++BatchIndex)
		{
			const FString BatchPath = FString::Printf(TEXT("$.batches[%d]"), BatchIndex);
			const TSharedPtr<FJsonValue>& BatchValue = (*BatchValues)[BatchIndex];
			if (!BatchValue.IsValid() || BatchValue->Type != EJson::Object)
			{
				AddDiagnostic(
					Result,
					EWBCardDBImporterManifestDiagnostic::ManifestBatchesMalformed,
					FString(),
					FString(),
					BatchPath);
				continue;
			}

			const TSharedPtr<FJsonObject> BatchObject = BatchValue->AsObject();
			FString BatchName;
			const bool bHasBatchName = TryReadRequiredString(
				Result,
				BatchObject,
				TEXT("batch_name"),
				EWBCardDBImporterManifestDiagnostic::ManifestBatchNameMissing,
				FString(),
				FString(),
				BatchPath + TEXT(".batch_name"),
				BatchName);
			const FString SafeBatchName = SafeManifestIdentifier(BatchName);
			if (bHasBatchName)
			{
				if (SeenBatchNames.Contains(BatchName))
				{
					AddDiagnostic(
						Result,
						EWBCardDBImporterManifestDiagnostic::ManifestBatchNameDuplicate,
						BatchName,
						FString(),
						BatchPath + TEXT(".batch_name"));
				}
				else
				{
					SeenBatchNames.Add(BatchName);
				}
			}

			ValidateOptionalDescription(Result, BatchObject, TEXT("description"), BatchName, FString(), BatchPath + TEXT(".description"));
			ValidateOptionalMetadata(Result, BatchObject, BatchName, FString(), BatchPath + TEXT(".metadata"));

			const FWBCardDBSchemaValidationOptions BatchOptions = ResolveCompatibilityOptions(
				Result,
				BatchObject,
				TEXT("compatibility"),
				BatchName,
				FString(),
				BatchPath + TEXT(".compatibility"),
				DefaultOptions);

			FWBCardDBImporterManifestBatchForTest Batch;
			Batch.BatchName = SafeBatchName;

			const TArray<TSharedPtr<FJsonValue>>* BundleValues = nullptr;
			if (!BatchObject->HasField(TEXT("bundles")))
			{
				AddDiagnostic(
					Result,
					EWBCardDBImporterManifestDiagnostic::ManifestBundlesMissing,
					BatchName,
					FString(),
					BatchPath + TEXT(".bundles"));
			}
			else if (!BatchObject->TryGetArrayField(TEXT("bundles"), BundleValues) || BundleValues == nullptr)
			{
				AddDiagnostic(
					Result,
					EWBCardDBImporterManifestDiagnostic::ManifestBundlesMalformed,
					BatchName,
					FString(),
					BatchPath + TEXT(".bundles"));
			}
			else if (BundleValues->Num() == 0)
			{
				AddDiagnostic(
					Result,
					EWBCardDBImporterManifestDiagnostic::ManifestBundlesMalformed,
					BatchName,
					FString(),
					BatchPath + TEXT(".bundles"));
			}
			else
			{
				TSet<FString> SeenBundleNames;
				for (int32 BundleIndex = 0; BundleIndex < BundleValues->Num(); ++BundleIndex)
				{
					const FString BundlePath = BatchPath + FString::Printf(TEXT(".bundles[%d]"), BundleIndex);
					const TSharedPtr<FJsonValue>& BundleValue = (*BundleValues)[BundleIndex];
					if (!BundleValue.IsValid() || BundleValue->Type != EJson::Object)
					{
						AddDiagnostic(
							Result,
							EWBCardDBImporterManifestDiagnostic::ManifestBundlesMalformed,
							BatchName,
							FString(),
							BundlePath);
						continue;
					}

					const TSharedPtr<FJsonObject> BundleObject = BundleValue->AsObject();
					FString BundleName;
					const bool bHasBundleName = TryReadRequiredString(
						Result,
						BundleObject,
						TEXT("name"),
						EWBCardDBImporterManifestDiagnostic::ManifestBundleNameMissing,
						BatchName,
						FString(),
						BundlePath + TEXT(".name"),
						BundleName);
					const FString SafeBundleName = SafeManifestIdentifier(BundleName);
					if (bHasBundleName)
					{
						if (SeenBundleNames.Contains(BundleName))
						{
							AddDiagnostic(
								Result,
								EWBCardDBImporterManifestDiagnostic::ManifestBundleNameDuplicate,
								BatchName,
								BundleName,
								BundlePath + TEXT(".name"));
						}
						else
						{
							SeenBundleNames.Add(BundleName);
						}
					}

					ValidateOptionalMetadata(Result, BundleObject, BatchName, BundleName, BundlePath + TEXT(".metadata"));

					FString RelativePath;
					const bool bHasRelativePath = TryReadRequiredString(
						Result,
						BundleObject,
						TEXT("path"),
						EWBCardDBImporterManifestDiagnostic::ManifestBundlePathMissing,
						BatchName,
						BundleName,
						BundlePath + TEXT(".path"),
						RelativePath);
					const bool bPathValid = bHasRelativePath
						&& ValidateBundlePath(
							Result,
							FixtureRootAbsolutePath,
							RelativePath,
							BatchName,
							BundleName,
							BundlePath + TEXT(".path"));

					const FWBCardDBSchemaValidationOptions BundleOptions = ResolveCompatibilityOptions(
						Result,
						BundleObject,
						TEXT("compatibility"),
						BatchName,
						BundleName,
						BundlePath + TEXT(".compatibility"),
						BatchOptions);

					if (bHasBundleName && bPathValid)
					{
						FWBCardDBImporterManifestBundleEntryForTest Bundle;
						Bundle.Name = SafeBundleName;
						Bundle.RelativePath = RelativePath;
						Bundle.Options = BundleOptions;
						Batch.Bundles.Add(Bundle);
					}
				}
			}

			Result.Batches.Add(Batch);
		}
	}

	Result.bOk = Result.Diagnostics.Num() == 0;
	Result.Reason = Result.bOk ? FString() : FString(TEXT("manifest_validation_failed"));
	if (!Result.bOk)
	{
		Result.Batches.Reset();
	}
	return Result;
}

FWBCardDBImporterManifestBatchEvaluationResultForTest FWBCardDBImporterManifestForTests::EvaluateManifestFile(
	const FString& AbsolutePathToManifest)
{
	FWBCardDBImporterManifestBatchEvaluationResultForTest Result;
	Result.ManifestValidationResult = ValidateManifestFile(AbsolutePathToManifest);
	if (!Result.ManifestValidationResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = TEXT("manifest_validation_failed");
		return Result;
	}

	const FString FixtureRoot = FixtureRootPath();
	for (const FWBCardDBImporterManifestBatchForTest& ManifestBatch : Result.ManifestValidationResult.Batches)
	{
		TArray<FWBCardDBImporterBatchEntryForTest> BatchEntries;
		for (const FWBCardDBImporterManifestBundleEntryForTest& ManifestBundle : ManifestBatch.Bundles)
		{
			FWBCardDBImporterBatchEntryForTest BatchEntry;
			BatchEntry.Name = ManifestBundle.Name;
			BatchEntry.BundlePath = FPaths::Combine(FixtureRoot, ManifestBundle.RelativePath);
			BatchEntry.Options = ManifestBundle.Options;
			BatchEntries.Add(BatchEntry);
		}

		FWBCardDBImporterBatchReadinessResultForTest BatchResult =
			FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(BatchEntries);
		if (!BatchResult.bOk)
		{
			Result.bOk = false;
			Result.Reason = TEXT("batch_entry_failed");
			Result.BatchResults.Reset();
			return Result;
		}
		Result.BatchResults.Add(BatchResult);
	}

	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

bool FWBCardDBImporterManifestForTests::ManifestEvaluationResultToJsonStringForTest(
	const FWBCardDBImporterManifestBatchEvaluationResultForTest& Result,
	FString& OutJson)
{
	OutJson.Reset();

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("ok"), Result.bOk);
	Writer->WriteValue(TEXT("reason"), SafeManifestIdentifier(Result.Reason));
	Writer->WriteValue(TEXT("manifest_id"), SafeManifestIdentifier(Result.ManifestValidationResult.ManifestId));
	Writer->WriteValue(TEXT("batch_count"), Result.bOk ? Result.BatchResults.Num() : 0);

	Writer->WriteArrayStart(TEXT("batches"));
	if (Result.bOk)
	{
		for (int32 BatchIndex = 0; BatchIndex < Result.BatchResults.Num(); ++BatchIndex)
		{
			const FWBCardDBImporterBatchReadinessResultForTest& BatchResult = Result.BatchResults[BatchIndex];
			const FString BatchName = Result.ManifestValidationResult.Batches.IsValidIndex(BatchIndex)
				? Result.ManifestValidationResult.Batches[BatchIndex].BatchName
				: FString();

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("batch_name"), SafeManifestIdentifier(BatchName));
			Writer->WriteValue(TEXT("ok"), BatchResult.bOk);
			Writer->WriteValue(TEXT("total_bundles"), BatchResult.TotalBundles);
			Writer->WriteValue(TEXT("ready_bundles"), BatchResult.ReadyBundles);
			Writer->WriteValue(TEXT("not_ready_bundles"), BatchResult.NotReadyBundles);
			Writer->WriteArrayStart(TEXT("bundle_names"));
			for (const FString& BundleName : BatchResult.BundleNames)
			{
				Writer->WriteValue(SafeManifestIdentifier(BundleName));
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectStart(TEXT("summary"));
			WriteReasonSummaries(Writer, BatchResult.DiagnosticSummary.ReasonSummaries);
			WriteDiagnosticSummaries(Writer, BatchResult.DiagnosticSummary.DiagnosticSummaries);
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();
		}
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("diagnostics"));
	for (const FWBCardDBImporterManifestDiagnosticForTest& Diagnostic : Result.ManifestValidationResult.Diagnostics)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("code"), ManifestDiagnosticCodeToString(Diagnostic.Code));
		Writer->WriteValue(TEXT("manifest_id"), SafeManifestIdentifier(Diagnostic.ManifestId));
		Writer->WriteValue(TEXT("batch_name"), SafeManifestIdentifier(Diagnostic.BatchName));
		Writer->WriteValue(TEXT("bundle_name"), SafeManifestIdentifier(Diagnostic.BundleName));
		Writer->WriteValue(TEXT("json_path"), SafeManifestIdentifier(Diagnostic.JsonPath));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	return Writer->Close();
}

FString FWBCardDBImporterManifestForTests::ManifestDiagnosticCodeToString(
	const EWBCardDBImporterManifestDiagnostic Code)
{
	switch (Code)
	{
	case EWBCardDBImporterManifestDiagnostic::None:
		return TEXT("none");
	case EWBCardDBImporterManifestDiagnostic::ManifestJsonParseFailed:
		return TEXT("manifest_json_parse_failed");
	case EWBCardDBImporterManifestDiagnostic::ManifestSchemaVersionMissing:
		return TEXT("manifest_schema_version_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestSchemaVersionUnsupported:
		return TEXT("manifest_schema_version_unsupported");
	case EWBCardDBImporterManifestDiagnostic::ManifestIdMissing:
		return TEXT("manifest_id_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestBatchesMissing:
		return TEXT("manifest_batches_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestBatchesMalformed:
		return TEXT("manifest_batches_malformed");
	case EWBCardDBImporterManifestDiagnostic::ManifestBatchNameMissing:
		return TEXT("manifest_batch_name_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestBatchNameDuplicate:
		return TEXT("manifest_batch_name_duplicate");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundlesMissing:
		return TEXT("manifest_bundles_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundlesMalformed:
		return TEXT("manifest_bundles_malformed");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundleNameMissing:
		return TEXT("manifest_bundle_name_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundleNameDuplicate:
		return TEXT("manifest_bundle_name_duplicate");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundlePathMissing:
		return TEXT("manifest_bundle_path_missing");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundlePathInvalid:
		return TEXT("manifest_bundle_path_invalid");
	case EWBCardDBImporterManifestDiagnostic::ManifestBundlePathNotFound:
		return TEXT("manifest_bundle_path_not_found");
	case EWBCardDBImporterManifestDiagnostic::ManifestCompatibilityMalformed:
		return TEXT("manifest_compatibility_malformed");
	case EWBCardDBImporterManifestDiagnostic::ManifestMetadataMalformed:
		return TEXT("manifest_metadata_malformed");
	case EWBCardDBImporterManifestDiagnostic::ManifestHiddenInfoPolicyViolation:
		return TEXT("manifest_hidden_info_policy_violation");
	default:
		return TEXT("unknown");
	}
}
