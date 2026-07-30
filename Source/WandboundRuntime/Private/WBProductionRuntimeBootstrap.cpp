#include "WBProductionRuntimeBootstrap.h"

#include "WBProductionMatchSpecification.h"

FWBProductionRuntimeBootstrapResult WBProductionRuntimeBootstrap::Build(
	const FWBProductionRuntimeBootstrapRequest& Request)
{
	FWBProductionRuntimeBootstrapResult Result;
	if (Request.CardBundleManifestPath.IsEmpty())
	{
		Result.Reason = TEXT("production_card_bundle_missing");
		return Result;
	}

	const FWBProductionCardDatabaseLoadResult DatabaseResult =
		WBProductionCardDatabase::LoadManifestSuite(
			Request.CardBundleManifestPath);
	Result.Diagnostics = DatabaseResult.Diagnostics;
	if (!DatabaseResult.bOk || !DatabaseResult.Snapshot.IsValid())
	{
		Result.Reason = DatabaseResult.Reason.IsEmpty()
			? TEXT("production_card_bundle_invalid")
			: DatabaseResult.Reason;
		return Result;
	}
	if (DatabaseResult.Snapshot->BundleKind != EWBProductionBundleKind::Production
		&& !(Request.bAllowTestBundle
			&& DatabaseResult.Snapshot->BundleKind == EWBProductionBundleKind::Test))
	{
		Result.Reason = TEXT("production_card_bundle_kind_disallowed");
		return Result;
	}
	Result.Database = DatabaseResult.Snapshot;
	if (DatabaseResult.Snapshot->MatchStatus == TEXT("blocked"))
	{
		Result.Reason = DatabaseResult.Snapshot->MatchBlockedReason.IsEmpty()
			? TEXT("production_match_spec_blocked")
			: DatabaseResult.Snapshot->MatchBlockedReason;
		return Result;
	}
	if (Request.MatchSpecificationPath.IsEmpty())
	{
		Result.Reason = TEXT("production_match_spec_missing");
		return Result;
	}

	const FWBProductionMatchSpecificationLoadResult MatchResult =
		WBProductionMatchSpecification::LoadAndBuildRequest(
			Request.MatchSpecificationPath,
			*DatabaseResult.Snapshot);
	Result.Diagnostics.Append(MatchResult.Diagnostics);
	if (!MatchResult.bOk)
	{
		Result.Reason = MatchResult.Reason.IsEmpty()
			? TEXT("production_match_spec_invalid")
			: MatchResult.Reason;
		return Result;
	}

	Result.bOk = true;
	Result.Reason = TEXT("success");
	Result.InitializationRequest = MatchResult.InitializationRequest;
	return Result;
}
