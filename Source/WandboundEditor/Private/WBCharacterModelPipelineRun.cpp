#include "WBCharacterModelPipeline.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxMeshImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Interfaces/IPluginManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "UObject/SavePackage.h"

namespace
{
using FCondensedWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

void AddDiagnostic(
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics,
	const EWBCharacterDiagnosticSeverity Severity,
	const FString& Code,
	const FString& Message,
	const FString& ManifestPath = FString(),
	const FString& FieldPath = FString(),
	const FString& SourcePath = FString(),
	const FString& Action = FString())
{
	FWBCharacterManifestDiagnostic Diagnostic;
	Diagnostic.Severity = Severity;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.ManifestPath = ManifestPath;
	Diagnostic.FieldPath = FieldPath;
	Diagnostic.SourceRelativePath = SourcePath;
	Diagnostic.RecommendedAction = Action;
	Diagnostics.Add(MoveTemp(Diagnostic));
}

bool HasError(const TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	return Diagnostics.ContainsByPredicate([](const FWBCharacterManifestDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EWBCharacterDiagnosticSeverity::Error;
	});
}

bool AtomicWrite(const FString& AbsolutePath, const FString& Contents)
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
	const FString TemporaryPath = AbsolutePath + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(
		Contents,
		*TemporaryPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}
	return IFileManager::Get().Move(
		*AbsolutePath,
		*TemporaryPath,
		true,
		true,
		false,
		true);
}

FString ModelTypeName(const EWBCharacterModelType Type)
{
	return Type == EWBCharacterModelType::Skeletal ? TEXT("skeletal") : TEXT("static");
}

FString SourceFormatName(const EWBCharacterSourceFormat Format)
{
	switch (Format)
	{
	case EWBCharacterSourceFormat::FBX: return TEXT("fbx");
	case EWBCharacterSourceFormat::GLB: return TEXT("glb");
	case EWBCharacterSourceFormat::GLTF: return TEXT("gltf");
	default: return TEXT("unknown");
	}
}

FString PipelineModeName(const EWBCharacterPipelineMode Mode)
{
	switch (Mode)
	{
	case EWBCharacterPipelineMode::Validate: return TEXT("validate");
	case EWBCharacterPipelineMode::DryRun: return TEXT("dry_run");
	case EWBCharacterPipelineMode::Import: return TEXT("import");
	case EWBCharacterPipelineMode::Reimport: return TEXT("reimport");
	default: return TEXT("unknown");
	}
}

FString SeverityName(const EWBCharacterDiagnosticSeverity Severity)
{
	return Severity == EWBCharacterDiagnosticSeverity::Error ? TEXT("error") : TEXT("warning");
}

void WriteDiagnostic(
	const TSharedRef<FCondensedWriter>& Writer,
	const FWBCharacterManifestDiagnostic& Diagnostic)
{
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("severity"), SeverityName(Diagnostic.Severity));
	Writer->WriteValue(TEXT("code"), Diagnostic.Code);
	Writer->WriteValue(TEXT("message"), Diagnostic.Message);
	Writer->WriteValue(TEXT("manifest_path"), Diagnostic.ManifestPath);
	Writer->WriteValue(TEXT("field_path"), Diagnostic.FieldPath);
	Writer->WriteValue(TEXT("source_relative_path"), Diagnostic.SourceRelativePath);
	Writer->WriteValue(TEXT("recommended_action"), Diagnostic.RecommendedAction);
	Writer->WriteObjectEnd();
}

void WriteStringArray(
	const TSharedRef<FCondensedWriter>& Writer,
	const FString& Field,
	const TArray<FString>& Values)
{
	Writer->WriteArrayStart(Field);
	for (const FString& Value : Values)
	{
		Writer->WriteValue(Value);
	}
	Writer->WriteArrayEnd();
}

void WriteStringMap(
	const TSharedRef<FCondensedWriter>& Writer,
	const FString& Field,
	const TMap<FString, FString>& Values)
{
	TArray<FString> Keys;
	Values.GetKeys(Keys);
	Keys.Sort();
	Writer->WriteObjectStart(Field);
	for (const FString& Key : Keys)
	{
		Writer->WriteValue(Key, Values[Key]);
	}
	Writer->WriteObjectEnd();
}

FString ObjectPathForPackage(const FString& Package)
{
	return Package + TEXT(".") + FPaths::GetCleanFilename(Package);
}

UObject* LoadDestinationObject(const FString& Package)
{
	return StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPathForPackage(Package));
}

bool DestinationPackagesExist(const FWBCharacterDestinationPlan& Destination)
{
	if (Destination.PrimaryMeshPackage.IsEmpty())
	{
		return false;
	}
	FString Filename;
	return FPackageName::DoesPackageExist(Destination.PrimaryMeshPackage, &Filename);
}

bool SaveObjectPackage(UObject* Object)
{
	if (Object == nullptr || Object->GetOutermost() == nullptr)
	{
		return false;
	}
	UPackage* Package = Object->GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Package->GetName(),
		FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, Object, *Filename, Args);
}

void ValidateImportedObjects(
	const FWBCharacterModelManifest& Manifest,
	const FWBCharacterDestinationPlan& Destination,
	const TArray<FString>& ImportedPaths,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	UObject* MeshObject = LoadDestinationObject(Destination.PrimaryMeshPackage);
	if (MeshObject == nullptr)
	{
		for (const FString& Path : ImportedPaths)
		{
			UObject* Candidate = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
			if ((Manifest.Source.ModelType == EWBCharacterModelType::Static
					&& Candidate != nullptr && Candidate->IsA<UStaticMesh>())
				|| (Manifest.Source.ModelType == EWBCharacterModelType::Skeletal
					&& Candidate != nullptr && Candidate->IsA<USkeletalMesh>()))
			{
				MeshObject = Candidate;
				break;
			}
		}
	}
	if (MeshObject == nullptr)
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("import_primary_mesh_missing"),
			TEXT("Import completed without a loadable primary mesh."),
			Manifest.ManifestRepositoryPath,
			TEXT("source.model"),
			Manifest.Source.ModelPath);
		return;
	}
	if (Manifest.Source.ModelType == EWBCharacterModelType::Static)
	{
		const UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshObject);
		if (StaticMesh == nullptr)
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_model_type_mismatch"),
				TEXT("The imported primary object is not a static mesh."),
				Manifest.ManifestRepositoryPath,
				TEXT("source.model_type"));
			return;
		}
		if (StaticMesh->GetBounds().BoxExtent.IsNearlyZero())
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_mesh_bounds_zero"),
				TEXT("The imported static mesh has zero bounds."));
		}
		if (StaticMesh->GetRenderData() == nullptr)
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_static_render_data_missing"),
				TEXT("The imported static mesh has no render data."));
		}
	}
	else
	{
		const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshObject);
		if (SkeletalMesh == nullptr)
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_model_type_mismatch"),
				TEXT("The imported primary object is not a skeletal mesh."),
				Manifest.ManifestRepositoryPath,
				TEXT("source.model_type"));
			return;
		}
		if (SkeletalMesh->GetBounds().BoxExtent.IsNearlyZero())
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_mesh_bounds_zero"),
				TEXT("The imported skeletal mesh has zero bounds."));
		}
		const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (Skeleton == nullptr
			|| SkeletalMesh->GetRefSkeleton().GetNum() <= 0
			|| SkeletalMesh->GetRefSkeleton().GetBoneName(0).IsNone())
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_skeleton_invalid"),
				TEXT("The imported skeletal mesh has no valid skeleton and root bone."));
		}
	}
}

FString BuildPreviewResultJson(
	const FWBCharacterModelManifest& Manifest,
	const bool bRequested,
	const FString& Status)
{
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schema_version"), 1);
	Writer->WriteValue(TEXT("character_id"), Manifest.CharacterId);
	Writer->WriteValue(TEXT("requested"), bRequested);
	Writer->WriteValue(TEXT("output_path"),
		FString::Printf(TEXT("Docs/AssetImports/%s/UnrealPreview.png"), *Manifest.CharacterId));
	Writer->WriteValue(TEXT("status"), Status);
	Writer->WriteValue(TEXT("camera_policy"), TEXT("whole_character_fixed_front"));
	Writer->WriteValue(TEXT("lighting_policy"), TEXT("neutral_deterministic"));
	Writer->WriteValue(TEXT("pose_policy"), TEXT("reference_pose_or_safe_idle"));
	Writer->WriteValue(TEXT("fatal_to_import"), false);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

FString GenerateUnrealPreview(
	const FWBCharacterModelManifest& Manifest,
	const FWBCharacterDestinationPlan& Destination,
	const FString& AbsoluteOutputPath,
	const bool bRequested,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	if (!bRequested)
	{
		return TEXT("not_requested");
	}
	if (!FApp::CanEverRender())
	{
		return TEXT("unsupported_under_null_rhi");
	}

	const FString ObjectPath = FString::Printf(
		TEXT("%s.%s"),
		*Destination.PrimaryMeshPackage,
		*FPaths::GetCleanFilename(Destination.PrimaryMeshPackage));
	UObject* MeshObject = LoadObject<UObject>(nullptr, *ObjectPath);
	if (MeshObject == nullptr)
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Warning,
			TEXT("preview_mesh_not_available"),
			TEXT("The Unreal preview was requested, but the imported primary mesh is not available."),
			Manifest.ManifestRepositoryPath,
			TEXT("$"),
			Destination.PrimaryMeshPackage);
		return TEXT("imported_mesh_required");
	}

	constexpr int32 PreviewSize = 512;
	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(
		MeshObject,
		PreviewSize,
		PreviewSize,
		ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush,
		nullptr,
		&Thumbnail);
	const TArray<uint8>& Raw = Thumbnail.GetUncompressedImageData();
	const int32 PixelCount = Thumbnail.GetImageWidth() * Thumbnail.GetImageHeight();
	if (Thumbnail.IsEmpty()
		|| PixelCount <= 0
		|| Raw.Num() != PixelCount * static_cast<int32>(sizeof(FColor)))
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Warning,
			TEXT("preview_render_failed"),
			TEXT("Unreal did not return a valid mesh thumbnail; import remains valid."),
			Manifest.ManifestRepositoryPath);
		return TEXT("render_failed_nonfatal");
	}

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(PixelCount);
	FMemory::Memcpy(Pixels.GetData(), Raw.GetData(), Raw.Num());
	TArray<uint8> PngBytes;
	FImageUtils::ThumbnailCompressImageArray(
		Thumbnail.GetImageWidth(),
		Thumbnail.GetImageHeight(),
		Pixels,
		PngBytes);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteOutputPath), true);
	if (PngBytes.IsEmpty()
		|| !FFileHelper::SaveArrayToFile(PngBytes, *AbsoluteOutputPath))
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Warning,
			TEXT("preview_write_failed"),
			TEXT("The Unreal preview PNG could not be written; import remains valid."),
			Manifest.ManifestRepositoryPath);
		return TEXT("write_failed_nonfatal");
	}
	return TEXT("generated");
}

FString BuildCookResultJson(const FWBCharacterCookVerificationResult& Cook)
{
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schema_version"), 1);
	Writer->WriteValue(TEXT("valid"), Cook.bValid);
	WriteStringArray(Writer, TEXT("exact_packages"), Cook.AcceptedPackages);
	Writer->WriteArrayStart(TEXT("diagnostics"));
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Cook.Diagnostics)
	{
		WriteDiagnostic(Writer, Diagnostic);
	}
	Writer->WriteArrayEnd();
	Writer->WriteValue(TEXT("cook_executed"), false);
	Writer->WriteValue(TEXT("status"), Cook.bValid ? TEXT("request_ready") : TEXT("request_invalid"));
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}
}

FString WBCharacterModelPipeline::BuildImportReportJson(
	const FWBCharacterPipelineRunResult& Result)
{
	const FWBCharacterModelManifest& Manifest = Result.Validation.Manifest;
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("report_schema_version"), 1);
	Writer->WriteValue(TEXT("ok"), Result.bOk);
	Writer->WriteValue(TEXT("reason"), Result.Reason);
	Writer->WriteObjectStart(TEXT("identity"));
	Writer->WriteValue(TEXT("character_id"), Manifest.CharacterId);
	Writer->WriteValue(TEXT("display_name"), Manifest.DisplayName);
	Writer->WriteValue(TEXT("card_definition_id"), Manifest.CardDefinitionId);
	Writer->WriteValue(TEXT("approved_for_import"), Manifest.bApprovedForImport);
	Writer->WriteValue(TEXT("approved_by"), Manifest.ApprovedBy);
	Writer->WriteObjectEnd();
	Writer->WriteObjectStart(TEXT("source"));
	Writer->WriteValue(TEXT("manifest_path"), Manifest.ManifestRepositoryPath);
	Writer->WriteValue(TEXT("model_path"), Manifest.Source.ModelPath);
	Writer->WriteValue(TEXT("model_format"), SourceFormatName(Manifest.Source.Format));
	Writer->WriteValue(TEXT("model_type"), ModelTypeName(Manifest.Source.ModelType));
	Writer->WriteValue(TEXT("inventory_hash"), Result.Inventory.InventoryHash);
	Writer->WriteObjectEnd();
	Writer->WriteObjectStart(TEXT("destination"));
	Writer->WriteValue(TEXT("root"), Result.Destination.DestinationRoot);
	Writer->WriteValue(TEXT("primary_mesh_package"), Result.Destination.PrimaryMeshPackage);
	Writer->WriteValue(TEXT("skeleton_package"), Result.Destination.SkeletonPackage);
	Writer->WriteValue(TEXT("physics_package"), Result.Destination.PhysicsPackage);
	WriteStringMap(Writer, TEXT("texture_packages"), Result.Destination.TexturePackages);
	WriteStringMap(Writer, TEXT("animation_packages"), Result.Destination.AnimationPackages);
	Writer->WriteObjectEnd();
	Writer->WriteObjectStart(TEXT("transform"));
	Writer->WriteValue(TEXT("scale"), Manifest.Presentation.Scale);
	Writer->WriteArrayStart(TEXT("rotation"));
	Writer->WriteValue(Manifest.Presentation.Rotation.X);
	Writer->WriteValue(Manifest.Presentation.Rotation.Y);
	Writer->WriteValue(Manifest.Presentation.Rotation.Z);
	Writer->WriteArrayEnd();
	Writer->WriteArrayStart(TEXT("offset"));
	Writer->WriteValue(Manifest.Presentation.Offset.X);
	Writer->WriteValue(Manifest.Presentation.Offset.Y);
	Writer->WriteValue(Manifest.Presentation.Offset.Z);
	Writer->WriteArrayEnd();
	Writer->WriteValue(TEXT("facing_axis"), Manifest.Presentation.FacingAxis);
	Writer->WriteObjectEnd();
	Writer->WriteValue(TEXT("reimport_status"), ReimportStateName(Result.ReimportState));
	Writer->WriteValue(TEXT("validation_status"),
		Result.Validation.IsValid() && !HasError(Result.Diagnostics)
			? TEXT("valid")
			: TEXT("invalid"));
	Writer->WriteValue(TEXT("cook_status"),
		Result.CookVerification.AcceptedPackages.Num() > 0
			? Result.CookVerification.bValid ? TEXT("request_ready") : TEXT("invalid")
			: TEXT("not_requested"));
	Writer->WriteValue(TEXT("presentation_status"),
		Result.PresentationCandidate.ValidationStatus);
	WriteStringArray(Writer, TEXT("imported_object_paths"), Result.ImportedObjectPaths);
	Writer->WriteArrayStart(TEXT("diagnostics"));
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Result.Diagnostics)
	{
		WriteDiagnostic(Writer, Diagnostic);
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectStart(TEXT("git_lfs"));
	Writer->WriteValue(
		TEXT("source_model_recommendation"),
		FString::Printf(
			TEXT("SourceAssets/Characters/%s/model/* filter=lfs diff=lfs merge=lfs -text"),
			*Manifest.CharacterId));
	Writer->WriteValue(
		TEXT("source_texture_recommendation"),
		FString::Printf(
			TEXT("SourceAssets/Characters/%s/textures/* filter=lfs diff=lfs merge=lfs -text"),
			*Manifest.CharacterId));
	Writer->WriteValue(
		TEXT("source_animation_recommendation"),
		FString::Printf(
			TEXT("SourceAssets/Characters/%s/animations/* filter=lfs diff=lfs merge=lfs -text"),
			*Manifest.CharacterId));
	Writer->WriteValue(
		TEXT("source_preview_recommendation"),
		FString::Printf(
			TEXT("SourceAssets/Characters/%s/previews/* filter=lfs diff=lfs merge=lfs -text"),
			*Manifest.CharacterId));
	Writer->WriteValue(
		TEXT("generated_asset_recommendation"),
		FString::Printf(
			TEXT("Content/Wandbound/Characters/%s/**/*.uasset filter=lfs diff=lfs merge=lfs -text"),
			*Manifest.CharacterId));
	Writer->WriteValue(TEXT("gitattributes_modified"), false);
	Writer->WriteObjectEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

FString WBCharacterModelPipeline::BuildImportReportMarkdown(
	const FWBCharacterPipelineRunResult& Result)
{
	const FWBCharacterModelManifest& Manifest = Result.Validation.Manifest;
	FString Output;
	Output += TEXT("# Wandbound Character Import Report\n\n");
	Output += FString::Printf(TEXT("- Character: `%s` (%s)\n"), *Manifest.CharacterId, *Manifest.DisplayName);
	Output += FString::Printf(TEXT("- Public definition: `%s`\n"), *Manifest.CardDefinitionId);
	Output += FString::Printf(TEXT("- Approved: `%s` by `%s`\n"), Manifest.bApprovedForImport ? TEXT("yes") : TEXT("no"), *Manifest.ApprovedBy);
	Output += FString::Printf(TEXT("- Result: `%s` (`%s`)\n"), Result.bOk ? TEXT("success") : TEXT("failed"), *Result.Reason);
	Output += FString::Printf(TEXT("- Source: `%s` (%s, %s)\n"), *Manifest.Source.ModelPath, *SourceFormatName(Manifest.Source.Format), *ModelTypeName(Manifest.Source.ModelType));
	Output += FString::Printf(TEXT("- Inventory hash: `%s`\n"), *Result.Inventory.InventoryHash);
	Output += FString::Printf(TEXT("- Reimport state: `%s`\n"), *ReimportStateName(Result.ReimportState));
	Output += FString::Printf(TEXT("- Destination: `%s`\n"), *Result.Destination.DestinationRoot);
	Output += FString::Printf(TEXT("- Primary mesh: `%s`\n"), *Result.Destination.PrimaryMeshPackage);
	Output += FString::Printf(TEXT("- Skeleton: `%s`\n"), *Result.Destination.SkeletonPackage);
	Output += FString::Printf(TEXT("- Physics asset: `%s`\n"), *Result.Destination.PhysicsPackage);
	Output += FString::Printf(TEXT("- Transform: scale %.6g, rotation (%g, %g, %g), offset (%g, %g, %g)\n"),
		Manifest.Presentation.Scale,
		Manifest.Presentation.Rotation.X,
		Manifest.Presentation.Rotation.Y,
		Manifest.Presentation.Rotation.Z,
		Manifest.Presentation.Offset.X,
		Manifest.Presentation.Offset.Y,
		Manifest.Presentation.Offset.Z);
	Output += TEXT("- Preview: optional; NullRHI produces a structured request instead of failing import\n");
	Output += TEXT("- Presentation: candidate only; the starter asset is not modified automatically\n");
	Output += TEXT("- Fallback: missing animation roles retain Wandbound transform/pulse fallbacks\n\n");
	Output += TEXT("## Source Inventory\n\n");
	Output += TEXT("| Path | Role | Bytes | SHA-256 | Git/LFS |\n| --- | --- | ---: | --- | --- |\n");
	for (const FWBCharacterSourceInventoryEntry& Entry : Result.Inventory.Entries)
	{
		Output += FString::Printf(
			TEXT("| `%s` | `%s` | %lld | `%s` | `%s` |\n"),
			*Entry.RelativePath,
			*Entry.DeclaredRole,
			Entry.SizeBytes,
			*Entry.SHA256,
			*GitStatusName(Entry.GitStatus));
	}
	Output += TEXT("\n## Diagnostics\n\n");
	if (Result.Diagnostics.IsEmpty())
	{
		Output += TEXT("No diagnostics.\n");
	}
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Result.Diagnostics)
	{
		Output += FString::Printf(
			TEXT("- **%s** `%s`: %s"),
			*SeverityName(Diagnostic.Severity),
			*Diagnostic.Code,
			*Diagnostic.Message);
		if (!Diagnostic.RecommendedAction.IsEmpty())
		{
			Output += TEXT(" ") + Diagnostic.RecommendedAction;
		}
		Output += TEXT("\n");
	}
	Output += TEXT("\n## Git LFS Recommendations\n\n");
	Output += FString::Printf(
		TEXT("`SourceAssets/Characters/%s/model/* filter=lfs diff=lfs merge=lfs -text`\n\n"),
		*Manifest.CharacterId);
	Output += FString::Printf(
		TEXT("`SourceAssets/Characters/%s/textures/* filter=lfs diff=lfs merge=lfs -text`\n\n"),
		*Manifest.CharacterId);
	Output += FString::Printf(
		TEXT("`SourceAssets/Characters/%s/animations/* filter=lfs diff=lfs merge=lfs -text`\n\n"),
		*Manifest.CharacterId);
	Output += FString::Printf(
		TEXT("`SourceAssets/Characters/%s/previews/* filter=lfs diff=lfs merge=lfs -text`\n\n"),
		*Manifest.CharacterId);
	Output += FString::Printf(
		TEXT("`Content/Wandbound/Characters/%s/**/*.uasset filter=lfs diff=lfs merge=lfs -text`\n"),
		*Manifest.CharacterId);
	return Output;
}

bool WBCharacterModelPipeline::UpdateCatalogAtomic(
	const FString& CatalogAbsolutePath,
	const FWBCharacterPipelineRunResult& Result,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	TArray<TSharedPtr<FJsonValue>> Entries;
	if (IFileManager::Get().FileExists(*CatalogAbsolutePath))
	{
		FString Existing;
		TSharedPtr<FJsonObject> ExistingRoot;
		const TArray<TSharedPtr<FJsonValue>>* ExistingEntries = nullptr;
		if (!FFileHelper::LoadFileToString(Existing, *CatalogAbsolutePath)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Existing), ExistingRoot)
			|| !ExistingRoot.IsValid()
			|| !ExistingRoot->TryGetArrayField(TEXT("characters"), ExistingEntries))
		{
			OutFailureReason = TEXT("character_catalog_corrupt");
			return false;
		}
		Entries = *ExistingEntries;
	}

	const FString CharacterId = Result.Validation.Manifest.CharacterId;
	Entries.RemoveAll([&CharacterId](const TSharedPtr<FJsonValue>& Entry)
	{
		FString ExistingId;
		return Entry.IsValid()
			&& Entry->Type == EJson::Object
			&& Entry->AsObject()->TryGetStringField(TEXT("character_id"), ExistingId)
			&& ExistingId == CharacterId;
	});

	for (const TSharedPtr<FJsonValue>& Entry : Entries)
	{
		FString Mesh;
		if (Entry.IsValid()
			&& Entry->Type == EJson::Object
			&& Entry->AsObject()->TryGetStringField(TEXT("primary_mesh_package"), Mesh)
			&& !Mesh.IsEmpty()
			&& Mesh == Result.Destination.PrimaryMeshPackage)
		{
			OutFailureReason = TEXT("character_catalog_duplicate_primary_mesh");
			return false;
		}
	}

	const FWBCharacterModelManifest& Manifest = Result.Validation.Manifest;
	const TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("character_id"), Manifest.CharacterId);
	Entry->SetStringField(TEXT("display_name"), Manifest.DisplayName);
	Entry->SetStringField(TEXT("card_definition_id"), Manifest.CardDefinitionId);
	Entry->SetBoolField(TEXT("approved_for_import"), Manifest.bApprovedForImport);
	Entry->SetStringField(TEXT("approval_status"), Manifest.bApprovedForImport ? TEXT("approved") : TEXT("not_approved"));
	Entry->SetStringField(TEXT("manifest_path"), Manifest.ManifestRepositoryPath);
	Entry->SetStringField(TEXT("source_model_path"), Manifest.Source.ModelPath);
	const FWBCharacterSourceInventoryEntry* ModelEntry = Result.Inventory.Entries.FindByPredicate(
		[&Manifest](const FWBCharacterSourceInventoryEntry& Candidate)
		{
			return Candidate.RelativePath == Manifest.Source.ModelPath;
		});
	Entry->SetStringField(TEXT("source_model_hash"), ModelEntry != nullptr ? ModelEntry->SHA256 : FString());
	Entry->SetStringField(TEXT("source_inventory_hash"), Result.Inventory.InventoryHash);
	Entry->SetStringField(TEXT("source_format"), SourceFormatName(Manifest.Source.Format));
	Entry->SetStringField(TEXT("model_type"), ModelTypeName(Manifest.Source.ModelType));
	Entry->SetStringField(TEXT("destination_root"), Result.Destination.DestinationRoot);
	Entry->SetStringField(TEXT("primary_mesh_package"), Result.Destination.PrimaryMeshPackage);
	Entry->SetStringField(TEXT("skeleton_package"), Result.Destination.SkeletonPackage);
	Entry->SetStringField(TEXT("physics_package"), Result.Destination.PhysicsPackage);
	TArray<TSharedPtr<FJsonValue>> DestinationPackages;
	for (const FString& Package : Result.Destination.IntendedPackages)
	{
		DestinationPackages.Add(MakeShared<FJsonValueString>(Package));
	}
	Entry->SetArrayField(TEXT("destination_packages"), DestinationPackages);
	const TSharedPtr<FJsonObject> TexturePackages = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Result.Destination.TexturePackages)
	{
		TexturePackages->SetStringField(Pair.Key, Pair.Value);
	}
	Entry->SetObjectField(TEXT("texture_packages"), TexturePackages);
	const TSharedPtr<FJsonObject> AnimationPackages = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Result.Destination.AnimationPackages)
	{
		AnimationPackages->SetStringField(Pair.Key, Pair.Value);
	}
	Entry->SetObjectField(TEXT("animation_packages"), AnimationPackages);
	TArray<TSharedPtr<FJsonValue>> MaterialPackages;
	for (const FString& Package : Result.PresentationCandidate.MaterialPackages)
	{
		MaterialPackages.Add(MakeShared<FJsonValueString>(Package));
	}
	Entry->SetArrayField(TEXT("material_packages"), MaterialPackages);
	const TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
	Transform->SetNumberField(TEXT("scale"), Manifest.Presentation.Scale);
	Transform->SetArrayField(TEXT("rotation"), {
		MakeShared<FJsonValueNumber>(Manifest.Presentation.Rotation.X),
		MakeShared<FJsonValueNumber>(Manifest.Presentation.Rotation.Y),
		MakeShared<FJsonValueNumber>(Manifest.Presentation.Rotation.Z)
	});
	Transform->SetArrayField(TEXT("offset"), {
		MakeShared<FJsonValueNumber>(Manifest.Presentation.Offset.X),
		MakeShared<FJsonValueNumber>(Manifest.Presentation.Offset.Y),
		MakeShared<FJsonValueNumber>(Manifest.Presentation.Offset.Z)
	});
	Transform->SetStringField(TEXT("facing_axis"), Manifest.Presentation.FacingAxis);
	Entry->SetObjectField(TEXT("transform"), Transform);
	const TSharedPtr<FJsonObject> PreviewPaths = MakeShared<FJsonObject>();
	TArray<FString> PreviewRoles;
	Manifest.PreviewPaths.GetKeys(PreviewRoles);
	PreviewRoles.Sort();
	for (const FString& Role : PreviewRoles)
	{
		PreviewPaths->SetStringField(Role, Manifest.PreviewPaths[Role]);
	}
	PreviewPaths->SetStringField(
		TEXT("unreal"),
		FString::Printf(TEXT("Docs/AssetImports/%s/UnrealPreview.png"), *Manifest.CharacterId));
	Entry->SetObjectField(TEXT("preview_paths"), PreviewPaths);
	Entry->SetStringField(TEXT("import_status"), Result.ImportedObjectPaths.IsEmpty() ? TEXT("not_imported") : TEXT("imported"));
	Entry->SetStringField(TEXT("reimport_status"), ReimportStateName(Result.ReimportState));
	Entry->SetStringField(TEXT("validation_status"), Result.Validation.IsValid() && !HasError(Result.Diagnostics) ? TEXT("valid") : TEXT("failed"));
	Entry->SetStringField(TEXT("cook_status"), Result.CookVerification.AcceptedPackages.IsEmpty() ? TEXT("not_requested") : TEXT("request_ready"));
	Entry->SetStringField(TEXT("presentation_status"), Result.PresentationCandidate.ValidationStatus);
	bool bRequiredBinaryNeedsLFS = false;
	for (const FWBCharacterSourceInventoryEntry& SourceEntry : Result.Inventory.Entries)
	{
		if (SourceEntry.bRequired
			&& SourceEntry.FileType != TEXT("json")
			&& SourceEntry.GitStatus != EWBCharacterGitStatus::TrackedLFS)
		{
			bRequiredBinaryNeedsLFS = true;
			break;
		}
	}
	Entry->SetStringField(
		TEXT("lfs_status"),
		bRequiredBinaryNeedsLFS ? TEXT("required_binary_not_lfs") : TEXT("ready"));
	Entry->SetStringField(TEXT("last_receipt_path"),
		FString::Printf(TEXT("Docs/AssetImports/%s/ImportReceipt.json"), *Manifest.CharacterId));
	Entry->SetStringField(TEXT("last_report_path"),
		FString::Printf(TEXT("Docs/AssetImports/%s/ImportReport.json"), *Manifest.CharacterId));
	Entries.Add(MakeShared<FJsonValueObject>(Entry));

	Entries.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		FString AId;
		FString BId;
		A->AsObject()->TryGetStringField(TEXT("character_id"), AId);
		B->AsObject()->TryGetStringField(TEXT("character_id"), BId);
		return AId < BId;
	});

	const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetArrayField(TEXT("characters"), Entries);
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		OutFailureReason = TEXT("character_catalog_serialization_failed");
		return false;
	}
	return AtomicWrite(CatalogAbsolutePath, Output);
}

bool WBCharacterModelPipeline::ExecuteImportTasks(
	const FWBCharacterModelManifest& Manifest,
	const TArray<FWBCharacterImportTaskSpec>& Specs,
	TArray<FString>& OutImportedObjectPaths,
	TArray<FWBCharacterManifestDiagnostic>& OutDiagnostics)
{
	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	USkeleton* ImportedSkeleton = nullptr;

	for (const FWBCharacterImportTaskSpec& Spec : Specs)
	{
		if (!IFileManager::Get().FileExists(*Spec.SourceAbsolutePath))
		{
			AddDiagnostic(
				OutDiagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_source_file_missing"),
				TEXT("An import task source file is missing."),
				Manifest.ManifestRepositoryPath,
				TEXT("$"),
				Spec.SourceAbsolutePath);
			continue;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = Spec.SourceAbsolutePath;
		Task->DestinationPath = Spec.DestinationPath;
		Task->DestinationName = Spec.DestinationName;
		Task->bReplaceExisting = Spec.bReplaceExisting;
		Task->bReplaceExistingSettings = Spec.bReplaceExisting;
		Task->bAutomated = true;
		Task->bSave = true;
		Task->bAsync = false;

		if (FPaths::GetExtension(Spec.SourceAbsolutePath).Equals(TEXT("fbx"), ESearchCase::IgnoreCase))
		{
			UFbxFactory* Factory = NewObject<UFbxFactory>();
			UFbxImportUI* ImportUI = NewObject<UFbxImportUI>(Factory);
			Factory->ImportUI = ImportUI;
			Factory->SetDetectImportTypeOnImport(false);
			ImportUI->bAutomatedImportShouldDetectType = false;
			ImportUI->bImportMaterials = Spec.bImportMaterials;
			ImportUI->bImportTextures = Spec.bImportTextures;
			ImportUI->bImportAnimations = Spec.bImportAnimations;
			ImportUI->bCreatePhysicsAsset = Spec.bCreatePhysicsAsset;
			ImportUI->bOverrideFullName = true;
			if (Spec.ExpectedClass == TEXT("StaticMesh"))
			{
				ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
				ImportUI->OriginalImportType = FBXIT_StaticMesh;
				ImportUI->bImportAsSkeletal = false;
				ImportUI->bImportMesh = true;
				if (ImportUI->StaticMeshImportData != nullptr)
				{
					ImportUI->StaticMeshImportData->bAutoGenerateCollision = Spec.bGenerateCollision;
					ImportUI->StaticMeshImportData->NormalImportMethod =
						Manifest.Import.NormalPolicy == TEXT("import_normals_and_tangents")
							? FBXNIM_ImportNormalsAndTangents
							: Manifest.Import.NormalPolicy == TEXT("import_normals")
								? FBXNIM_ImportNormals
								: FBXNIM_ComputeNormals;
					ImportUI->StaticMeshImportData->NormalGenerationMethod =
						EFBXNormalGenerationMethod::MikkTSpace;
				}
			}
			else
			{
				ImportUI->MeshTypeToImport =
					Spec.ExpectedClass == TEXT("AnimSequence")
						? FBXIT_Animation
						: FBXIT_SkeletalMesh;
				ImportUI->OriginalImportType = ImportUI->MeshTypeToImport;
				ImportUI->bImportAsSkeletal = true;
				ImportUI->bImportMesh = Spec.ExpectedClass != TEXT("AnimSequence");
				if (Manifest.Import.SkeletonPolicy == TEXT("reuse"))
				{
					ImportedSkeleton = LoadObject<USkeleton>(
						nullptr,
						*Manifest.Import.ExistingSkeletonPackage);
				}
				ImportUI->Skeleton = ImportedSkeleton;
			}
			Task->Factory = Factory;
			Task->Options = ImportUI;
		}
		else
		{
			AddDiagnostic(
				OutDiagnostics,
				EWBCharacterDiagnosticSeverity::Warning,
				TEXT("interchange_model_type_auto_detection"),
				TEXT("GLB/glTF uses the installed Interchange translator and validates the resulting class after import."),
				Manifest.ManifestRepositoryPath,
				TEXT("source.model_type"),
				Spec.SourceAbsolutePath);
		}

		TArray<UAssetImportTask*> Tasks;
		Tasks.Add(Task);
		AssetTools.ImportAssetTasks(Tasks);
		if (Task->ImportedObjectPaths.IsEmpty())
		{
			AddDiagnostic(
				OutDiagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("import_task_produced_no_objects"),
				TEXT("Unreal reported no imported objects for an import task."),
				Manifest.ManifestRepositoryPath,
				TEXT("$"),
				Spec.SourceAbsolutePath);
			continue;
		}
		OutImportedObjectPaths.Append(Task->ImportedObjectPaths);
		for (UObject* Object : Task->GetObjects())
		{
			if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
			{
				ImportedSkeleton = SkeletalMesh->GetSkeleton();
			}
		}
	}
	OutImportedObjectPaths.Sort();
	for (int32 Index = OutImportedObjectPaths.Num() - 1; Index > 0; --Index)
	{
		if (OutImportedObjectPaths[Index] == OutImportedObjectPaths[Index - 1])
		{
			OutImportedObjectPaths.RemoveAt(Index);
		}
	}
	return !HasError(OutDiagnostics);
}

FWBCharacterPipelineRunResult WBCharacterModelPipeline::Run(
	const FString& ProjectRoot,
	const FWBCharacterPipelineRunOptions& Options)
{
	FWBCharacterPipelineRunResult Result;
	Result.Reason = TEXT("pipeline_not_started");
	const FString AbsoluteProjectRoot = FPaths::ConvertRelativePathToFull(ProjectRoot);
	Result.Validation = LoadAndValidateManifest(
		AbsoluteProjectRoot,
		Options.ManifestRepositoryPath,
		Options.CardDefinitionRepository,
		Options.bRequireCardDefinition);
	Result.Diagnostics.Append(Result.Validation.Diagnostics);
	if (!Result.Validation.IsValid())
	{
		Result.Reason = TEXT("manifest_validation_failed");
		return Result;
	}

	const FWBCharacterModelManifest& Manifest = Result.Validation.Manifest;
	Result.Inventory = BuildSourceInventory(AbsoluteProjectRoot, Manifest);
	Result.Diagnostics.Append(Result.Inventory.Diagnostics);
	Result.Destination = BuildDestinationPlan(Manifest);
	Result.Diagnostics.Append(Result.Destination.Diagnostics);
	Result.PresentationCandidate = BuildPresentationCandidate(Manifest, Result.Destination);
	if (HasError(Result.Diagnostics) || !Result.Destination.IsValid())
	{
		Result.Reason = TEXT("pipeline_preflight_failed");
		return Result;
	}

	const FString ReportDirectory = FPaths::Combine(
		AbsoluteProjectRoot,
		TEXT("Docs/AssetImports"),
		Manifest.CharacterId);
	const FString ReceiptPath = FPaths::Combine(ReportDirectory, TEXT("ImportReceipt.json"));
	FWBCharacterImportReceipt PreviousReceipt;
	const bool bHasPreviousReceipt = LoadReceipt(ReceiptPath, PreviousReceipt);
	Result.ReimportState = DetermineReimportState(
		Manifest,
		Result.Inventory,
		Result.Destination,
		bHasPreviousReceipt ? &PreviousReceipt : nullptr,
		DestinationPackagesExist(Result.Destination));

	if (Options.Mode == EWBCharacterPipelineMode::Import
		&& Result.ReimportState != EWBCharacterReimportState::NeverImported)
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("import_existing_character_requires_reimport"),
			TEXT("The destination or receipt already exists; use Reimport mode."),
			Manifest.ManifestRepositoryPath);
		Result.Reason = TEXT("import_requires_reimport_mode");
		return Result;
	}
	if (Options.Mode == EWBCharacterPipelineMode::Reimport
		&& Result.ReimportState == EWBCharacterReimportState::NeverImported)
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("reimport_receipt_missing"),
			TEXT("Reimport requires an existing import receipt."),
			Manifest.ManifestRepositoryPath);
		Result.Reason = TEXT("reimport_receipt_missing");
		return Result;
	}

	for (const FWBCharacterSourceInventoryEntry& Entry : Result.Inventory.Entries)
	{
		if (Entry.bRequired
			&& Entry.FileType != TEXT("json")
			&& Entry.FileType != TEXT("text")
			&& Entry.GitStatus != EWBCharacterGitStatus::TrackedLFS)
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Warning,
				TEXT("production_ready_blocked_binary_not_lfs"),
				TEXT("A required binary source file is not tracked through Git LFS."),
				Manifest.ManifestRepositoryPath,
				TEXT("$"),
				Entry.RelativePath,
				TEXT("Apply the narrow reported LFS rule before production approval."));
		}
	}

	if (Options.Mode == EWBCharacterPipelineMode::Import
		|| Options.Mode == EWBCharacterPipelineMode::Reimport)
	{
		const bool bReplaceExisting = Options.Mode == EWBCharacterPipelineMode::Reimport;
		if (!(Options.Mode == EWBCharacterPipelineMode::Reimport
			&& Result.ReimportState == EWBCharacterReimportState::UpToDate))
		{
			const TArray<FWBCharacterImportTaskSpec> Specs =
				BuildImportTaskSpecs(Manifest, Result.Destination, bReplaceExisting);
			ExecuteImportTasks(
				Manifest,
				Specs,
				Result.ImportedObjectPaths,
				Result.Diagnostics);
			ValidateImportedObjects(
				Manifest,
				Result.Destination,
				Result.ImportedObjectPaths,
				Result.Diagnostics);
			for (const FString& ObjectPath : Result.ImportedObjectPaths)
			{
				if (const UMaterialInterface* Material =
					Cast<UMaterialInterface>(LoadObject<UObject>(nullptr, *ObjectPath)))
				{
					Result.PresentationCandidate.MaterialPackages.AddUnique(
						Material->GetOutermost()->GetName());
				}
			}
			Result.PresentationCandidate.MaterialPackages.Sort();
		}
	}

	if (Options.bValidateCook)
	{
		FWBCharacterCookVerificationRequest CookRequest;
		CookRequest.ExactPackages = Result.Destination.IntendedPackages;
		Result.CookVerification = ValidateCookPackageList(CookRequest);
		Result.Diagnostics.Append(Result.CookVerification.Diagnostics);
	}

	const FString PreviewPath = FPaths::Combine(ReportDirectory, TEXT("UnrealPreview.png"));
	const FString PreviewStatus = GenerateUnrealPreview(
		Manifest,
		Result.Destination,
		PreviewPath,
		Options.bGeneratePreview,
		Result.Diagnostics);

	Result.Receipt.ManifestSchemaVersion = Manifest.SchemaVersion;
	Result.Receipt.CharacterId = Manifest.CharacterId;
	Result.Receipt.ManifestHash = SHA256String(Manifest.NormalizedJson);
	Result.Receipt.SourceInventoryHash = Result.Inventory.InventoryHash;
	Result.Receipt.ImportSettingsDigest = BuildImportSettingsDigest(Manifest);
	Result.Receipt.EngineVersion = FEngineVersion::Current().ToString();
	Result.Receipt.SourceEntries = Result.Inventory.Entries;
	Result.Receipt.DestinationPackages = Result.Destination.IntendedPackages;
	Result.Receipt.LastResult = HasError(Result.Diagnostics) ? TEXT("failed") : TEXT("success");
	Result.Receipt.AuditTimestampUtc = FDateTime::UtcNow().ToIso8601();

	Result.bOk = !HasError(Result.Diagnostics);
	Result.Reason = Result.bOk
		? Options.Mode == EWBCharacterPipelineMode::DryRun
			? TEXT("dry_run_succeeded")
			: Options.Mode == EWBCharacterPipelineMode::Validate
				? TEXT("validation_succeeded")
				: TEXT("import_succeeded")
		: TEXT("pipeline_failed");

	if (Options.bWriteReports)
	{
		const TArray<TPair<FString, FString>> Reports = {
			{ TEXT("SourceInventory.json"), InventoryToJson(Result.Inventory) },
			{ TEXT("PresentationProfileCandidate.json"), PresentationCandidateToJson(Result.PresentationCandidate) },
			{ TEXT("PreviewResult.json"), BuildPreviewResultJson(Manifest, Options.bGeneratePreview, PreviewStatus) },
			{ TEXT("ImportReport.json"), BuildImportReportJson(Result) },
			{ TEXT("ImportReport.md"), BuildImportReportMarkdown(Result) }
		};
		for (const TPair<FString, FString>& Report : Reports)
		{
			const FString AbsolutePath = FPaths::Combine(ReportDirectory, Report.Key);
			if (AtomicWrite(AbsolutePath, Report.Value))
			{
				Result.GeneratedReportPaths.Add(FString::Printf(
					TEXT("Docs/AssetImports/%s/%s"),
					*Manifest.CharacterId,
					*Report.Key));
			}
			else
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("report_write_failed"),
					TEXT("A pipeline report could not be written."),
					Manifest.ManifestRepositoryPath,
					TEXT("$"),
					AbsolutePath);
				Result.bOk = false;
				Result.Reason = TEXT("report_write_failed");
			}
		}
		if (PreviewStatus == TEXT("generated"))
		{
			Result.GeneratedReportPaths.Add(FString::Printf(
				TEXT("Docs/AssetImports/%s/UnrealPreview.png"),
				*Manifest.CharacterId));
		}
		if (Options.bValidateCook)
		{
			const FString CookPath = FPaths::Combine(ReportDirectory, TEXT("CookVerification.json"));
			if (AtomicWrite(CookPath, BuildCookResultJson(Result.CookVerification)))
			{
				Result.GeneratedReportPaths.Add(FString::Printf(
					TEXT("Docs/AssetImports/%s/CookVerification.json"),
					*Manifest.CharacterId));
			}
		}
		if (Options.Mode == EWBCharacterPipelineMode::Import
			|| Options.Mode == EWBCharacterPipelineMode::Reimport)
		{
			if (AtomicWrite(ReceiptPath, ReceiptToJson(Result.Receipt)))
			{
				Result.GeneratedReportPaths.Add(FString::Printf(
					TEXT("Docs/AssetImports/%s/ImportReceipt.json"),
					*Manifest.CharacterId));
			}
		}
	}

	if (Options.bUpdateCatalog)
	{
		FString CatalogFailure;
		const FString CatalogPath = FPaths::Combine(
			AbsoluteProjectRoot,
			GetCatalogRepositoryPath());
		if (!UpdateCatalogAtomic(CatalogPath, Result, CatalogFailure))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				CatalogFailure,
				TEXT("The character model catalog could not be updated safely."),
				Manifest.ManifestRepositoryPath);
			Result.bOk = false;
			Result.Reason = CatalogFailure;
		}
	}

	Result.Diagnostics.Sort([](
		const FWBCharacterManifestDiagnostic& A,
		const FWBCharacterManifestDiagnostic& B)
	{
		if (A.Severity != B.Severity) return A.Severity > B.Severity;
		if (A.Code != B.Code) return A.Code < B.Code;
		return A.SourceRelativePath < B.SourceRelativePath;
	});
	return Result;
}
