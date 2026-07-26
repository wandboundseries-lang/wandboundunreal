#include "WBCharacterModelImportCommandlet.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "WBCharacterModelPipeline.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBCharacterModelImport, Log, All);

UWBCharacterModelImportCommandlet::UWBCharacterModelImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UWBCharacterModelImportCommandlet::Main(const FString& Params)
{
	FString ManifestPath;
	FString ModeValue;
	if (!FParse::Value(*Params, TEXT("Manifest="), ManifestPath)
		|| ManifestPath.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogWBCharacterModelImport, Error, TEXT("Missing required -Manifest parameter."));
		return 2;
	}
	if (!FParse::Value(*Params, TEXT("Mode="), ModeValue))
	{
		ModeValue = TEXT("Validate");
	}

	FWBCharacterPipelineRunOptions Options;
	Options.ManifestRepositoryPath = WBCharacterModelPipeline::NormalizeRepositoryPath(ManifestPath);
	Options.bGeneratePreview = FParse::Param(*Params, TEXT("GeneratePreview"));
	Options.bValidateCook = FParse::Param(*Params, TEXT("ValidateCook"));
	if (ModeValue.Equals(TEXT("Validate"), ESearchCase::IgnoreCase))
	{
		Options.Mode = EWBCharacterPipelineMode::Validate;
	}
	else if (ModeValue.Equals(TEXT("DryRun"), ESearchCase::IgnoreCase))
	{
		Options.Mode = EWBCharacterPipelineMode::DryRun;
	}
	else if (ModeValue.Equals(TEXT("Import"), ESearchCase::IgnoreCase))
	{
		Options.Mode = EWBCharacterPipelineMode::Import;
	}
	else if (ModeValue.Equals(TEXT("Reimport"), ESearchCase::IgnoreCase))
	{
		Options.Mode = EWBCharacterPipelineMode::Reimport;
	}
	else
	{
		UE_LOG(LogWBCharacterModelImport, Error, TEXT("Unsupported -Mode value: %s"), *ModeValue);
		return 2;
	}
	Options.bUpdateCatalog =
		Options.Mode == EWBCharacterPipelineMode::Import
		|| Options.Mode == EWBCharacterPipelineMode::Reimport;

	const FWBCharacterPipelineRunResult Result =
		WBCharacterModelPipeline::Run(FPaths::ProjectDir(), Options);
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Result.Diagnostics)
	{
		if (Diagnostic.Severity == EWBCharacterDiagnosticSeverity::Error)
		{
			UE_LOG(
				LogWBCharacterModelImport,
				Error,
				TEXT("[%s] %s (%s)"),
				*Diagnostic.Code,
				*Diagnostic.Message,
				*Diagnostic.SourceRelativePath);
		}
		else
		{
			UE_LOG(
				LogWBCharacterModelImport,
				Warning,
				TEXT("[%s] %s (%s)"),
				*Diagnostic.Code,
				*Diagnostic.Message,
				*Diagnostic.SourceRelativePath);
		}
	}
	for (const FString& Report : Result.GeneratedReportPaths)
	{
		UE_LOG(LogWBCharacterModelImport, Display, TEXT("Generated report: %s"), *Report);
	}
	if (Result.bOk)
	{
		UE_LOG(
			LogWBCharacterModelImport,
			Display,
			TEXT("Character pipeline result: ok=true reason=%s character=%s reimport=%s"),
			*Result.Reason,
			*Result.Validation.Manifest.CharacterId,
			*WBCharacterModelPipeline::ReimportStateName(Result.ReimportState));
	}
	else
	{
		UE_LOG(
			LogWBCharacterModelImport,
			Error,
			TEXT("Character pipeline result: ok=false reason=%s character=%s reimport=%s"),
			*Result.Reason,
			*Result.Validation.Manifest.CharacterId,
			*WBCharacterModelPipeline::ReimportStateName(Result.ReimportState));
	}
	return Result.bOk ? 0 : 1;
}
