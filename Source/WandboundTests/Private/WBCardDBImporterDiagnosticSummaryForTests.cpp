#include "WBCardDBImporterDiagnosticSummaryForTests.h"

#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool ContainsHiddenSummaryTerm(const FString& Value)
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

FString SafeSummaryIdentifier(const FString& Value)
{
	return ContainsHiddenSummaryTerm(Value) ? FString() : Value;
}

FString SafeSummarySourcePath(const FString& SourcePath)
{
	const FString CleanFilename = FPaths::GetCleanFilename(SourcePath);
	return ContainsHiddenSummaryTerm(CleanFilename) ? FString(TEXT("redacted_source")) : CleanFilename;
}

FString StableReason(const FString& Reason)
{
	return Reason.IsEmpty() ? FString(TEXT("unknown")) : SafeSummaryIdentifier(Reason);
}

struct FDiagnosticAccumulator
{
	int32 Count = 0;
	TSet<FString> AffectedBundleKeys;
	TSet<FString> AffectedCardKeys;
};

void IncrementReasonSummary(
	TMap<FString, int32>& ReasonCounts,
	const FString& Reason)
{
	const FString Key = StableReason(Reason);
	int32& Count = ReasonCounts.FindOrAdd(Key);
	++Count;
}

void IncrementDiagnosticSummary(
	TMap<FString, FDiagnosticAccumulator>& DiagnosticSummaries,
	const FWBCardDBImporterReadinessResultForTest& Result,
	const FWBCardDBSchemaValidationDiagnostic& Diagnostic)
{
	const FString DiagnosticCode =
		FWBCardDBSchemaFixtureValidator::DiagnosticCodeToString(Diagnostic.Code);
	FDiagnosticAccumulator& Accumulator = DiagnosticSummaries.FindOrAdd(SafeSummaryIdentifier(DiagnosticCode));
	++Accumulator.Count;

	const FString SourcePath = SafeSummarySourcePath(Result.SourcePath);
	Accumulator.AffectedBundleKeys.Add(SourcePath);
	if (Diagnostic.CardIndex >= 0)
	{
		Accumulator.AffectedCardKeys.Add(
			FString::Printf(TEXT("%s:%d"), *SourcePath, Diagnostic.CardIndex));
	}
}

void WriteStringArray(
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& Writer,
	const FString& FieldName,
	const TArray<FString>& Values)
{
	Writer->WriteArrayStart(FieldName);
	for (const FString& Value : Values)
	{
		Writer->WriteValue(SafeSummaryIdentifier(Value));
	}
	Writer->WriteArrayEnd();
}
}

FWBCardDBImporterDiagnosticSummaryForTest FWBCardDBImporterDiagnosticSummaryForTests::BuildSummary(
	const TArray<FWBCardDBImporterReadinessResultForTest>& Results)
{
	FWBCardDBImporterDiagnosticSummaryForTest Summary;
	Summary.TotalBundles = Results.Num();

	TMap<FString, int32> ReasonCounts;
	TMap<FString, FDiagnosticAccumulator> DiagnosticAccumulators;

	for (const FWBCardDBImporterReadinessResultForTest& Result : Results)
	{
		const FString SourcePath = SafeSummarySourcePath(Result.SourcePath);
		if (Result.bReady)
		{
			++Summary.ReadyBundles;
			Summary.ReadyBundleSourcePaths.Add(SourcePath);
		}
		else
		{
			++Summary.NotReadyBundles;
			Summary.NotReadyBundleSourcePaths.Add(SourcePath);
			IncrementReasonSummary(ReasonCounts, Result.Reason);
		}

		for (const FWBCardDBSchemaValidationDiagnostic& Diagnostic : Result.BundleValidationResult.Diagnostics)
		{
			IncrementDiagnosticSummary(DiagnosticAccumulators, Result, Diagnostic);
		}
	}

	ReasonCounts.KeySort([](const FString& A, const FString& B)
	{
		return A < B;
	});
	for (const TPair<FString, int32>& Pair : ReasonCounts)
	{
		FWBCardDBImporterReasonSummaryForTest ReasonSummary;
		ReasonSummary.Reason = Pair.Key;
		ReasonSummary.Count = Pair.Value;
		Summary.ReasonSummaries.Add(ReasonSummary);
	}

	DiagnosticAccumulators.KeySort([](const FString& A, const FString& B)
	{
		return A < B;
	});
	for (const TPair<FString, FDiagnosticAccumulator>& Pair : DiagnosticAccumulators)
	{
		FWBCardDBImporterDiagnosticCodeSummaryForTest DiagnosticSummary;
		DiagnosticSummary.DiagnosticCode = Pair.Key;
		DiagnosticSummary.Count = Pair.Value.Count;
		DiagnosticSummary.AffectedBundleCount = Pair.Value.AffectedBundleKeys.Num();
		DiagnosticSummary.AffectedCardCount = Pair.Value.AffectedCardKeys.Num();
		Summary.DiagnosticSummaries.Add(DiagnosticSummary);
	}

	Summary.ReadyBundleSourcePaths.Sort();
	Summary.NotReadyBundleSourcePaths.Sort();
	return Summary;
}

bool FWBCardDBImporterDiagnosticSummaryForTests::SummaryToJsonStringForTest(
	const FWBCardDBImporterDiagnosticSummaryForTest& Summary,
	FString& OutJson)
{
	OutJson.Reset();

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("total_bundles"), Summary.TotalBundles);
	Writer->WriteValue(TEXT("ready_bundles"), Summary.ReadyBundles);
	Writer->WriteValue(TEXT("not_ready_bundles"), Summary.NotReadyBundles);

	Writer->WriteArrayStart(TEXT("reason_summaries"));
	for (const FWBCardDBImporterReasonSummaryForTest& ReasonSummary : Summary.ReasonSummaries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("reason"), SafeSummaryIdentifier(ReasonSummary.Reason));
		Writer->WriteValue(TEXT("count"), ReasonSummary.Count);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("diagnostic_summaries"));
	for (const FWBCardDBImporterDiagnosticCodeSummaryForTest& DiagnosticSummary : Summary.DiagnosticSummaries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("diagnostic_code"), SafeSummaryIdentifier(DiagnosticSummary.DiagnosticCode));
		Writer->WriteValue(TEXT("count"), DiagnosticSummary.Count);
		Writer->WriteValue(TEXT("affected_bundle_count"), DiagnosticSummary.AffectedBundleCount);
		Writer->WriteValue(TEXT("affected_card_count"), DiagnosticSummary.AffectedCardCount);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	WriteStringArray(Writer, TEXT("ready_bundle_source_paths"), Summary.ReadyBundleSourcePaths);
	WriteStringArray(Writer, TEXT("not_ready_bundle_source_paths"), Summary.NotReadyBundleSourcePaths);

	Writer->WriteObjectEnd();
	return Writer->Close();
}
