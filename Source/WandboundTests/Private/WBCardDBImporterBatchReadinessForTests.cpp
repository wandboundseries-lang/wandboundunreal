#include "WBCardDBImporterBatchReadinessForTests.h"

#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool ContainsHiddenBatchTerm(const FString& Value)
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

FString SafeBatchIdentifier(const FString& Value)
{
	return ContainsHiddenBatchTerm(Value) ? FString() : Value;
}

FString SafeBatchSourcePath(const FString& SourcePath)
{
	const FString CleanFilename = FPaths::GetCleanFilename(SourcePath);
	return ContainsHiddenBatchTerm(CleanFilename) ? FString(TEXT("redacted_source")) : CleanFilename;
}

void WriteStringArray(
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer,
	const FString& FieldName,
	const TArray<FString>& Values)
{
	Writer->WriteArrayStart(FieldName);
	for (const FString& Value : Values)
	{
		Writer->WriteValue(SafeBatchIdentifier(Value));
	}
	Writer->WriteArrayEnd();
}

void WriteDiagnosticCodes(
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer,
	const FWBCardDBImporterReadinessResultForTest& BundleResult)
{
	Writer->WriteArrayStart(TEXT("diagnostic_codes"));
	for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : BundleResult.BundleValidationResult.Diagnostics)
	{
		Writer->WriteValue(
			SafeBatchIdentifier(FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(Diagnostic.Code)));
	}
	Writer->WriteArrayEnd();
}

void WriteReasonSummaries(
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer,
	const TArray<FWBCardDBImporterReasonSummaryForTest>& ReasonSummaries)
{
	Writer->WriteArrayStart(TEXT("reason_summaries"));
	for (const FWBCardDBImporterReasonSummaryForTest& ReasonSummary : ReasonSummaries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("reason"), SafeBatchIdentifier(ReasonSummary.Reason));
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
		Writer->WriteValue(TEXT("diagnostic_code"), SafeBatchIdentifier(DiagnosticSummary.DiagnosticCode));
		Writer->WriteValue(TEXT("count"), DiagnosticSummary.Count);
		Writer->WriteValue(TEXT("affected_bundle_count"), DiagnosticSummary.AffectedBundleCount);
		Writer->WriteValue(TEXT("affected_card_count"), DiagnosticSummary.AffectedCardCount);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
}
}

FWBCardDBImporterBatchReadinessResultForTest FWBCardDBImporterBatchReadinessForTests::EvaluateBatch(
	const TArray<FWBCardDBImporterBatchEntryForTest>& Entries)
{
	FWBCardDBImporterBatchReadinessResultForTest Result;
	Result.TotalBundles = Entries.Num();

	for (const FWBCardDBImporterBatchEntryForTest& Entry : Entries)
	{
		if (Entry.BundlePath.IsEmpty() || !FPaths::FileExists(Entry.BundlePath))
		{
			Result.bOk = false;
			Result.Reason = TEXT("bundle_path_missing");
			return Result;
		}
	}

	for (const FWBCardDBImporterBatchEntryForTest& Entry : Entries)
	{
		Result.BundleNames.Add(SafeBatchIdentifier(Entry.Name));
		Result.BundleResults.Add(
			FWBCardDBImporterReadinessForTests::EvaluateBundleFileForImportReadiness(
				Entry.BundlePath,
				Entry.Options));
	}

	Result.DiagnosticSummary = FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(Result.BundleResults);
	Result.TotalBundles = Result.DiagnosticSummary.TotalBundles;
	Result.ReadyBundles = Result.DiagnosticSummary.ReadyBundles;
	Result.NotReadyBundles = Result.DiagnosticSummary.NotReadyBundles;
	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

bool FWBCardDBImporterBatchReadinessForTests::BatchReadinessResultToJsonStringForTest(
	const FWBCardDBImporterBatchReadinessResultForTest& Result,
	FString& OutJson)
{
	OutJson.Reset();

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("ok"), Result.bOk);
	Writer->WriteValue(TEXT("reason"), SafeBatchIdentifier(Result.Reason));
	Writer->WriteValue(TEXT("total_bundles"), Result.TotalBundles);
	Writer->WriteValue(TEXT("ready_bundles"), Result.ReadyBundles);
	Writer->WriteValue(TEXT("not_ready_bundles"), Result.NotReadyBundles);

	Writer->WriteArrayStart(TEXT("bundles"));
	for (int32 Index = 0; Index < Result.BundleResults.Num(); ++Index)
	{
		const FWBCardDBImporterReadinessResultForTest& BundleResult = Result.BundleResults[Index];
		const FString BundleName = Result.BundleNames.IsValidIndex(Index) ? Result.BundleNames[Index] : FString();

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), SafeBatchIdentifier(BundleName));
		Writer->WriteValue(TEXT("source_path"), SafeBatchSourcePath(BundleResult.SourcePath));
		Writer->WriteValue(TEXT("ready"), BundleResult.bReady);
		Writer->WriteValue(TEXT("reason"), SafeBatchIdentifier(BundleResult.Reason));
		Writer->WriteValue(TEXT("carddb_version"), SafeBatchIdentifier(BundleResult.CardDBVersion));
		Writer->WriteValue(TEXT("source_version"), SafeBatchIdentifier(BundleResult.SourceVersion));
		Writer->WriteValue(TEXT("card_count"), BundleResult.OrderedCardDefinitions.Num());
		WriteStringArray(Writer, TEXT("dependency_order_card_ids"), BundleResult.DependencyOrderCardIds);
		WriteDiagnosticCodes(Writer, BundleResult);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectStart(TEXT("summary"));
	WriteReasonSummaries(Writer, Result.DiagnosticSummary.ReasonSummaries);
	WriteDiagnosticSummaries(Writer, Result.DiagnosticSummary.DiagnosticSummaries);
	Writer->WriteObjectEnd();

	Writer->WriteObjectEnd();
	return Writer->Close();
}
