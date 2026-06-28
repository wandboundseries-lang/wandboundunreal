#include "WBCardDBImporterReadinessForTests.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool ContainsHiddenReadinessTerm(const FString& Value)
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

FString SafeReadinessIdentifier(const FString& Value)
{
	return ContainsHiddenReadinessTerm(Value) ? FString() : Value;
}

void PopulateSafeBundleVersionFields(
	FWBCardDBImporterReadinessResultForTest& Result,
	const FString& Json)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return;
	}

	FString CardDBVersion;
	if (RootObject->TryGetStringField(TEXT("carddb_version"), CardDBVersion))
	{
		Result.CardDBVersion = SafeReadinessIdentifier(CardDBVersion);
	}

	FString SourceVersion;
	if (RootObject->TryGetStringField(TEXT("source_version"), SourceVersion))
	{
		Result.SourceVersion = SafeReadinessIdentifier(SourceVersion);
	}
}

bool PopulateOrderedCardDefinitions(
	FWBCardDBImporterReadinessResultForTest& Result)
{
	TMap<FString, const FWBCardDefinition*> CardDefinitionById;
	for (const FWBCardDefinition& CardDefinition : Result.BundleValidationResult.CardDefinitions)
	{
		CardDefinitionById.Add(CardDefinition.CardId, &CardDefinition);
	}

	for (const FString& CardId : Result.DependencyOrderCardIds)
	{
		const FWBCardDefinition* const* CardDefinition = CardDefinitionById.Find(CardId);
		if (CardDefinition == nullptr || *CardDefinition == nullptr)
		{
			Result.bReady = false;
			Result.Reason = TEXT("ordered_card_missing");
			Result.OrderedCardDefinitions.Reset();
			return false;
		}

		Result.OrderedCardDefinitions.Add(**CardDefinition);
	}

	return true;
}

void WriteDiagnostics(
	const FWBCardDBBundleSchemaValidationResult& BundleResult,
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer)
{
	Writer->WriteArrayStart(TEXT("diagnostics"));
	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : BundleResult.Diagnostics)
	{
		const FString ExportedCardId = !Diagnostic.BundleCardId.IsEmpty()
			? Diagnostic.BundleCardId
			: Diagnostic.CardId;

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("code"), FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(Diagnostic.Code));
		Writer->WriteValue(TEXT("card_index"), Diagnostic.CardIndex);
		Writer->WriteValue(TEXT("card_id"), SafeReadinessIdentifier(ExportedCardId));
		Writer->WriteValue(TEXT("effect_id"), SafeReadinessIdentifier(Diagnostic.EffectId));
		Writer->WriteValue(TEXT("json_path"), SafeReadinessIdentifier(Diagnostic.JsonPath));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
}
}

FWBCardDBImporterReadinessResultForTest FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
	const FString& AbsolutePath,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath))
	{
		FWBCardDBImporterReadinessResultForTest Result;
		Result.SourcePath = AbsolutePath;
		Result.Reason = TEXT("bundle_validation_failed");
		Result.BundleValidationResult =
			FWBCardDBSchemaFixtureValidator::ValidateBundleFixtureFile(AbsolutePath, Options);
		return Result;
	}

	return EvaluateBundleJsonForImportReadiness(Json, AbsolutePath, Options);
}

FWBCardDBImporterReadinessResultForTest FWBCardDBImporterReadinessForTests::EvaluateBundleJsonForImportReadiness(
	const FString& Json,
	const FString& SourcePathForDiagnostics,
	const FWBCardDBSchemaValidationOptions& Options)
{
	FWBCardDBImporterReadinessResultForTest Result;
	Result.SourcePath = SourcePathForDiagnostics;
	PopulateSafeBundleVersionFields(Result, Json);

	Result.BundleValidationResult =
		FWBCardDBSchemaFixtureValidator::ValidateBundleJsonString(Json, SourcePathForDiagnostics, Options);
	Result.DependencyOrderCardIds = Result.BundleValidationResult.DependencyOrderCardIds;

	if (!Result.BundleValidationResult.bOk)
	{
		Result.bReady = false;
		Result.Reason = TEXT("bundle_validation_failed");
		return Result;
	}

	if (Result.BundleValidationResult.BundleCardCount > 0 && Result.DependencyOrderCardIds.Num() == 0)
	{
		Result.bReady = false;
		Result.Reason = TEXT("dependency_order_missing");
		return Result;
	}

	if (!PopulateOrderedCardDefinitions(Result))
	{
		return Result;
	}

	Result.bReady = true;
	Result.Reason.Reset();
	return Result;
}

bool FWBCardDBImporterReadinessForTests::ImporterReadinessResultToJsonStringForTest(
	const FWBCardDBImporterReadinessResultForTest& Result,
	FString& OutJson)
{
	OutJson.Reset();

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("ready"), Result.bReady);
	Writer->WriteValue(TEXT("reason"), SafeReadinessIdentifier(Result.Reason));
	Writer->WriteValue(TEXT("source_path"), SafeReadinessIdentifier(FPaths::GetCleanFilename(Result.SourcePath)));
	Writer->WriteValue(TEXT("carddb_version"), SafeReadinessIdentifier(Result.CardDBVersion));
	Writer->WriteValue(TEXT("source_version"), SafeReadinessIdentifier(Result.SourceVersion));
	Writer->WriteValue(TEXT("card_count"), Result.OrderedCardDefinitions.Num());

	Writer->WriteArrayStart(TEXT("dependency_order_card_ids"));
	for (const FString& CardId : Result.DependencyOrderCardIds)
	{
		Writer->WriteValue(SafeReadinessIdentifier(CardId));
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("ordered_card_ids"));
	for (const FWBCardDefinition& CardDefinition : Result.OrderedCardDefinitions)
	{
		Writer->WriteValue(SafeReadinessIdentifier(CardDefinition.CardId));
	}
	Writer->WriteArrayEnd();

	WriteDiagnostics(Result.BundleValidationResult, Writer);

	Writer->WriteObjectEnd();
	return Writer->Close();
}
