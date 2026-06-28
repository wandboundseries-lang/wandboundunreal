#include "WBCardDBImporterManifestSuiteForTests.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const int32 SuiteSchemaVersion = 1;

bool ContainsHiddenSuiteTerm(const FString& Value)
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

FString SafeSuiteIdentifier(const FString& Value)
{
	return ContainsHiddenSuiteTerm(Value) ? FString() : Value;
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

void AddDiagnostic(
	FWBCardDBImporterManifestSuiteValidationResultForTest& Result,
	const EWBCardDBImporterManifestSuiteDiagnostic Code,
	const FString& SuiteId,
	const FString& ManifestName,
	const FString& JsonPath)
{
	FWBCardDBImporterManifestSuiteDiagnosticForTest Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.SuiteId = SafeSuiteIdentifier(SuiteId);
	Diagnostic.ManifestName = SafeSuiteIdentifier(ManifestName);
	Diagnostic.JsonPath = SafeSuiteIdentifier(JsonPath);
	Result.Diagnostics.Add(Diagnostic);
}

void AddDiagnostic(
	FWBCardDBImporterManifestSuiteValidationResultForTest& Result,
	const EWBCardDBImporterManifestSuiteDiagnostic Code,
	const FString& ManifestName,
	const FString& JsonPath)
{
	AddDiagnostic(Result, Code, Result.SuiteId, ManifestName, JsonPath);
}

const TArray<FString>& SuiteMetadataAllowedFields()
{
	static const TArray<FString> Fields = {
		TEXT("notes"),
		TEXT("source"),
		TEXT("version"),
		TEXT("test_only")
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
	FWBCardDBImporterManifestSuiteValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const FString& ManifestName,
	const FString& JsonPath)
{
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		return;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::String)
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteMetadataMalformed, ManifestName, JsonPath);
		return;
	}

	if (ContainsHiddenSuiteTerm((*Value)->AsString()))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteHiddenInfoPolicyViolation, ManifestName, JsonPath);
	}
}

void ValidateOptionalMetadata(
	FWBCardDBImporterManifestSuiteValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& ManifestName,
	const FString& JsonPath)
{
	if (!Object.IsValid() || !Object->HasField(TEXT("metadata")))
	{
		return;
	}

	const TSharedPtr<FJsonValue>* MetadataValue = Object->Values.Find(TEXT("metadata"));
	if (MetadataValue == nullptr || !MetadataValue->IsValid() || (*MetadataValue)->Type != EJson::Object)
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteMetadataMalformed, ManifestName, JsonPath);
		return;
	}

	const TSharedPtr<FJsonObject> MetadataObject = (*MetadataValue)->AsObject();
	if (!MetadataObject.IsValid() || HasUnknownField(MetadataObject, SuiteMetadataAllowedFields()))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteMetadataMalformed, ManifestName, JsonPath);
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : MetadataObject->Values)
	{
		const FString FieldPath = JsonPath + TEXT(".") + Pair.Key;
		if (Pair.Key == TEXT("test_only"))
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Boolean)
			{
				AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteMetadataMalformed, ManifestName, FieldPath);
			}
		}
		else if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String)
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteMetadataMalformed, ManifestName, FieldPath);
		}
		else if (ContainsHiddenSuiteTerm(Pair.Value->AsString()))
		{
			AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteHiddenInfoPolicyViolation, ManifestName, FieldPath);
		}
	}
}

bool TryReadRequiredString(
	FWBCardDBImporterManifestSuiteValidationResultForTest& Result,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const EWBCardDBImporterManifestSuiteDiagnostic MissingDiagnostic,
	const FString& ManifestName,
	const FString& JsonPath,
	FString& OutValue)
{
	OutValue.Reset();
	const TSharedPtr<FJsonValue>* Value = Object.IsValid()
		? Object->Values.Find(FieldName)
		: nullptr;
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::String || (*Value)->AsString().IsEmpty())
	{
		AddDiagnostic(Result, MissingDiagnostic, ManifestName, JsonPath);
		return false;
	}

	OutValue = (*Value)->AsString();
	if (ContainsHiddenSuiteTerm(OutValue))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteHiddenInfoPolicyViolation, ManifestName, JsonPath);
	}

	return true;
}

bool IsRelativeManifestPathSafe(const FString& RelativePath)
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

bool DoesManifestPathResolveInsideFixtureRoot(
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

bool ValidateManifestPath(
	FWBCardDBImporterManifestSuiteValidationResultForTest& Result,
	const FString& FixtureRootAbsolutePath,
	const FString& RelativePath,
	const FString& ManifestName,
	const FString& JsonPath)
{
	if (ContainsHiddenSuiteTerm(RelativePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteHiddenInfoPolicyViolation, ManifestName, JsonPath);
		return false;
	}

	if (!IsRelativeManifestPathSafe(RelativePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathInvalid, ManifestName, JsonPath);
		return false;
	}

	FString AbsolutePath;
	if (!DoesManifestPathResolveInsideFixtureRoot(FixtureRootAbsolutePath, RelativePath, AbsolutePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathInvalid, ManifestName, JsonPath);
		return false;
	}

	if (!FPaths::FileExists(AbsolutePath))
	{
		AddDiagnostic(Result, EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathNotFound, ManifestName, JsonPath);
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
		Writer->WriteValue(TEXT("reason"), SafeSuiteIdentifier(ReasonSummary.Reason));
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
		Writer->WriteValue(TEXT("diagnostic_code"), SafeSuiteIdentifier(DiagnosticSummary.DiagnosticCode));
		Writer->WriteValue(TEXT("count"), DiagnosticSummary.Count);
		Writer->WriteValue(TEXT("affected_bundle_count"), DiagnosticSummary.AffectedBundleCount);
		Writer->WriteValue(TEXT("affected_card_count"), DiagnosticSummary.AffectedCardCount);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
}
}

FWBCardDBImporterManifestSuiteValidationResultForTest FWBCardDBImporterManifestSuiteForTests::ValidateSuiteFile(
	const FString& AbsolutePathToSuite)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePathToSuite))
	{
		FWBCardDBImporterManifestSuiteValidationResultForTest Result;
		Result.SourcePath = AbsolutePathToSuite;
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteJsonParseFailed,
			FString(),
			TEXT("$"));
		Result.Reason = TEXT("suite_validation_failed");
		return Result;
	}

	return ValidateSuiteJsonString(Json, AbsolutePathToSuite, FixtureRootPath());
}

FWBCardDBImporterManifestSuiteValidationResultForTest FWBCardDBImporterManifestSuiteForTests::ValidateSuiteJsonString(
	const FString& Json,
	const FString& SourcePathForDiagnostics,
	const FString& FixtureRootAbsolutePath)
{
	FWBCardDBImporterManifestSuiteValidationResultForTest Result;
	Result.SourcePath = SourcePathForDiagnostics;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteJsonParseFailed,
			FString(),
			TEXT("$"));
		Result.Reason = TEXT("suite_validation_failed");
		return Result;
	}

	FString SuiteId;
	if (TryReadRequiredString(
		Result,
		RootObject,
		TEXT("suite_id"),
		EWBCardDBImporterManifestSuiteDiagnostic::SuiteIdMissing,
		FString(),
		TEXT("$.suite_id"),
		SuiteId))
	{
		Result.SuiteId = SafeSuiteIdentifier(SuiteId);
	}

	const TSharedPtr<FJsonValue>* SchemaVersionValue = RootObject->Values.Find(TEXT("suite_schema_version"));
	if (SchemaVersionValue == nullptr || !SchemaVersionValue->IsValid())
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteSchemaVersionMissing,
			FString(),
			TEXT("$.suite_schema_version"));
	}
	else if ((*SchemaVersionValue)->Type != EJson::Number
		|| FMath::RoundToInt((*SchemaVersionValue)->AsNumber()) != SuiteSchemaVersion)
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteSchemaVersionUnsupported,
			FString(),
			TEXT("$.suite_schema_version"));
	}

	ValidateOptionalDescription(Result, RootObject, TEXT("description"), FString(), TEXT("$.description"));
	ValidateOptionalMetadata(Result, RootObject, FString(), TEXT("$.metadata"));

	const TArray<TSharedPtr<FJsonValue>>* ManifestValues = nullptr;
	if (!RootObject->HasField(TEXT("manifests")))
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMissing,
			FString(),
			TEXT("$.manifests"));
	}
	else if (!RootObject->TryGetArrayField(TEXT("manifests"), ManifestValues) || ManifestValues == nullptr)
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMalformed,
			FString(),
			TEXT("$.manifests"));
	}
	else if (ManifestValues->Num() == 0)
	{
		AddDiagnostic(
			Result,
			EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMalformed,
			FString(),
			TEXT("$.manifests"));
	}
	else
	{
		TSet<FString> SeenManifestNames;
		for (int32 ManifestIndex = 0; ManifestIndex < ManifestValues->Num(); ++ManifestIndex)
		{
			const FString ManifestPath = FString::Printf(TEXT("$.manifests[%d]"), ManifestIndex);
			const TSharedPtr<FJsonValue>& ManifestValue = (*ManifestValues)[ManifestIndex];
			if (!ManifestValue.IsValid() || ManifestValue->Type != EJson::Object)
			{
				AddDiagnostic(
					Result,
					EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMalformed,
					FString(),
					ManifestPath);
				continue;
			}

			const TSharedPtr<FJsonObject> ManifestObject = ManifestValue->AsObject();
			FString ManifestName;
			const bool bHasManifestName = TryReadRequiredString(
				Result,
				ManifestObject,
				TEXT("name"),
				EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestNameMissing,
				FString(),
				ManifestPath + TEXT(".name"),
				ManifestName);
			const FString SafeManifestName = SafeSuiteIdentifier(ManifestName);
			if (bHasManifestName)
			{
				if (SeenManifestNames.Contains(ManifestName))
				{
					AddDiagnostic(
						Result,
						EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestNameDuplicate,
						ManifestName,
						ManifestPath + TEXT(".name"));
				}
				else
				{
					SeenManifestNames.Add(ManifestName);
				}
			}

			ValidateOptionalMetadata(Result, ManifestObject, ManifestName, ManifestPath + TEXT(".metadata"));

			FString RelativePath;
			const bool bHasRelativePath = TryReadRequiredString(
				Result,
				ManifestObject,
				TEXT("path"),
				EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathMissing,
				ManifestName,
				ManifestPath + TEXT(".path"),
				RelativePath);
			const bool bPathValid = bHasRelativePath
				&& ValidateManifestPath(
					Result,
					FixtureRootAbsolutePath,
					RelativePath,
					ManifestName,
					ManifestPath + TEXT(".path"));

			if (bHasManifestName && bPathValid)
			{
				FWBCardDBImporterManifestSuiteEntryForTest Entry;
				Entry.Name = SafeManifestName;
				Entry.RelativePath = RelativePath;
				Result.ManifestEntries.Add(Entry);
			}
		}
	}

	Result.bOk = Result.Diagnostics.Num() == 0;
	Result.Reason = Result.bOk ? FString() : FString(TEXT("suite_validation_failed"));
	if (!Result.bOk)
	{
		Result.ManifestEntries.Reset();
	}
	return Result;
}

FWBCardDBImporterManifestSuiteEvaluationResultForTest FWBCardDBImporterManifestSuiteForTests::EvaluateSuiteFile(
	const FString& AbsolutePathToSuite)
{
	FWBCardDBImporterManifestSuiteEvaluationResultForTest Result;
	Result.SuiteValidationResult = ValidateSuiteFile(AbsolutePathToSuite);
	if (!Result.SuiteValidationResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = TEXT("suite_validation_failed");
		return Result;
	}

	const FString FixtureRoot = FixtureRootPath();
	TArray<FWBCardDBImporterReadinessResultForTest> AggregateReadinessResults;
	Result.TotalManifests = Result.SuiteValidationResult.ManifestEntries.Num();
	for (const FWBCardDBImporterManifestSuiteEntryForTest& SuiteEntry : Result.SuiteValidationResult.ManifestEntries)
	{
		const FString ManifestPath = FPaths::Combine(FixtureRoot, SuiteEntry.RelativePath);
		FWBCardDBImporterManifestBatchEvaluationResultForTest ManifestResult =
			FWBCardDBImporterManifestForTests::EvaluateManifestFile(ManifestPath);
		Result.ManifestResults.Add(ManifestResult);

		if (ManifestResult.bOk)
		{
			++Result.ValidManifests;
		}

		Result.TotalBatches += ManifestResult.BatchResults.Num();
		for (const FWBCardDBImporterBatchReadinessResultForTest& BatchResult : ManifestResult.BatchResults)
		{
			Result.TotalBundles += BatchResult.TotalBundles;
			Result.ReadyBundles += BatchResult.ReadyBundles;
			Result.NotReadyBundles += BatchResult.NotReadyBundles;
			AggregateReadinessResults.Append(BatchResult.BundleResults);
		}
	}

	Result.InvalidManifests = Result.TotalManifests - Result.ValidManifests;
	Result.AggregateDiagnosticSummary =
		FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(AggregateReadinessResults);
	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

bool FWBCardDBImporterManifestSuiteForTests::SuiteEvaluationResultToJsonStringForTest(
	const FWBCardDBImporterManifestSuiteEvaluationResultForTest& Result,
	FString& OutJson)
{
	OutJson.Reset();

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("ok"), Result.bOk);
	Writer->WriteValue(TEXT("reason"), SafeSuiteIdentifier(Result.Reason));
	Writer->WriteValue(TEXT("suite_id"), SafeSuiteIdentifier(Result.SuiteValidationResult.SuiteId));
	Writer->WriteValue(TEXT("manifest_count"), Result.bOk ? Result.TotalManifests : 0);
	Writer->WriteValue(TEXT("valid_manifests"), Result.bOk ? Result.ValidManifests : 0);
	Writer->WriteValue(TEXT("invalid_manifests"), Result.bOk ? Result.InvalidManifests : 0);
	Writer->WriteValue(TEXT("total_batches"), Result.bOk ? Result.TotalBatches : 0);
	Writer->WriteValue(TEXT("total_bundles"), Result.bOk ? Result.TotalBundles : 0);
	Writer->WriteValue(TEXT("ready_bundles"), Result.bOk ? Result.ReadyBundles : 0);
	Writer->WriteValue(TEXT("not_ready_bundles"), Result.bOk ? Result.NotReadyBundles : 0);

	Writer->WriteArrayStart(TEXT("manifests"));
	if (Result.bOk)
	{
		for (int32 ManifestIndex = 0; ManifestIndex < Result.ManifestResults.Num(); ++ManifestIndex)
		{
			const FWBCardDBImporterManifestBatchEvaluationResultForTest& ManifestResult = Result.ManifestResults[ManifestIndex];
			const FString ManifestName = Result.SuiteValidationResult.ManifestEntries.IsValidIndex(ManifestIndex)
				? Result.SuiteValidationResult.ManifestEntries[ManifestIndex].Name
				: FString();

			int32 ManifestTotalBundles = 0;
			int32 ManifestReadyBundles = 0;
			int32 ManifestNotReadyBundles = 0;
			for (const FWBCardDBImporterBatchReadinessResultForTest& BatchResult : ManifestResult.BatchResults)
			{
				ManifestTotalBundles += BatchResult.TotalBundles;
				ManifestReadyBundles += BatchResult.ReadyBundles;
				ManifestNotReadyBundles += BatchResult.NotReadyBundles;
			}

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), SafeSuiteIdentifier(ManifestName));
			Writer->WriteValue(TEXT("ok"), ManifestResult.bOk);
			Writer->WriteValue(TEXT("batch_count"), ManifestResult.BatchResults.Num());
			Writer->WriteValue(TEXT("total_bundles"), ManifestTotalBundles);
			Writer->WriteValue(TEXT("ready_bundles"), ManifestReadyBundles);
			Writer->WriteValue(TEXT("not_ready_bundles"), ManifestNotReadyBundles);
			Writer->WriteObjectEnd();
		}
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectStart(TEXT("summary"));
	WriteReasonSummaries(Writer, Result.AggregateDiagnosticSummary.ReasonSummaries);
	WriteDiagnosticSummaries(Writer, Result.AggregateDiagnosticSummary.DiagnosticSummaries);
	Writer->WriteObjectEnd();

	Writer->WriteArrayStart(TEXT("diagnostics"));
	for (const FWBCardDBImporterManifestSuiteDiagnosticForTest& Diagnostic : Result.SuiteValidationResult.Diagnostics)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("code"), SuiteDiagnosticCodeToString(Diagnostic.Code));
		Writer->WriteValue(TEXT("suite_id"), SafeSuiteIdentifier(Diagnostic.SuiteId));
		Writer->WriteValue(TEXT("manifest_name"), SafeSuiteIdentifier(Diagnostic.ManifestName));
		Writer->WriteValue(TEXT("json_path"), SafeSuiteIdentifier(Diagnostic.JsonPath));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	return Writer->Close();
}

FString FWBCardDBImporterManifestSuiteForTests::SuiteDiagnosticCodeToString(
	const EWBCardDBImporterManifestSuiteDiagnostic Code)
{
	switch (Code)
	{
	case EWBCardDBImporterManifestSuiteDiagnostic::None:
		return TEXT("none");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteJsonParseFailed:
		return TEXT("suite_json_parse_failed");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteSchemaVersionMissing:
		return TEXT("suite_schema_version_missing");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteSchemaVersionUnsupported:
		return TEXT("suite_schema_version_unsupported");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteIdMissing:
		return TEXT("suite_id_missing");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMissing:
		return TEXT("suite_manifests_missing");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestsMalformed:
		return TEXT("suite_manifests_malformed");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestNameMissing:
		return TEXT("suite_manifest_name_missing");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestNameDuplicate:
		return TEXT("suite_manifest_name_duplicate");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathMissing:
		return TEXT("suite_manifest_path_missing");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathInvalid:
		return TEXT("suite_manifest_path_invalid");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteManifestPathNotFound:
		return TEXT("suite_manifest_path_not_found");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteMetadataMalformed:
		return TEXT("suite_metadata_malformed");
	case EWBCardDBImporterManifestSuiteDiagnostic::SuiteHiddenInfoPolicyViolation:
		return TEXT("suite_hidden_info_policy_violation");
	default:
		return TEXT("unknown");
	}
}
