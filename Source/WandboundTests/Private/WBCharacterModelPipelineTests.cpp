#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCharacterModelPipeline.h"
#include "WBCardDefinitionRepository.h"

namespace
{
constexpr EAutomationTestFlags PipelineFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

FString StaticManifest(
	const FString& CharacterId = TEXT("synthetic_guardian"),
	const FString& ExtraRoot = FString())
{
	return FString::Printf(
		TEXT("{")
		TEXT("\"schema_version\":1,")
		TEXT("\"character_id\":\"%s\",")
		TEXT("\"display_name\":\"Synthetic Guardian\",")
		TEXT("\"card_definition_id\":\"CHAR_SYNTHETIC_GUARDIAN\",")
		TEXT("\"approval\":{\"approved_for_import\":true,\"approved_by\":\"automation\"},")
		TEXT("\"source\":{\"model\":\"model/guardian.fbx\",\"model_format\":\"fbx\",\"model_type\":\"static\",\"textures\":[],\"animations\":[]},")
		TEXT("\"presentation\":{\"role\":\"player_unit\",\"scale\":1.0,\"rotation\":[0,0,0],\"offset\":[0,0,0],\"facing_axis\":\"positive_x\"},")
		TEXT("\"import\":{\"import_materials\":true,\"import_textures\":true,\"import_animations\":false,\"create_physics_asset\":false,\"generate_collision\":true,\"skeleton_policy\":\"none\",\"normal_policy\":\"compute_normals\"},")
		TEXT("\"previews\":{},\"tags\":[\"synthetic\"],\"notes\":\"automation only\"%s")
		TEXT("}"),
		*CharacterId,
		*ExtraRoot);
}

FString SkeletalManifest(const FString& ExtraSource = FString())
{
	return FString::Printf(
		TEXT("{")
		TEXT("\"schema_version\":1,")
		TEXT("\"character_id\":\"synthetic_rig\",")
		TEXT("\"display_name\":\"Synthetic Rig\",")
		TEXT("\"card_definition_id\":\"CHAR_SYNTHETIC_RIG\",")
		TEXT("\"approval\":{\"approved_for_import\":true,\"approved_by\":\"automation\"},")
		TEXT("\"source\":{\"model\":\"model/rig.glb\",\"model_format\":\"glb\",\"model_type\":\"skeletal\",\"textures\":[],\"animations\":[]%s},")
		TEXT("\"presentation\":{\"role\":\"player_hero\",\"scale\":1.25,\"rotation\":[0,90,0],\"offset\":[1,2,3],\"facing_axis\":\"positive_y\"},")
		TEXT("\"import\":{\"import_materials\":true,\"import_textures\":true,\"import_animations\":false,\"create_physics_asset\":true,\"generate_collision\":false,\"skeleton_policy\":\"create\",\"normal_policy\":\"import_normals_and_tangents\"},")
		TEXT("\"previews\":{}")
		TEXT("}"),
		*ExtraSource);
}

FWBCharacterManifestValidationResult Parse(
	const FString& Json,
	const FString& Id = TEXT("synthetic_guardian"))
{
	return WBCharacterModelPipeline::ParseAndValidateManifestJson(
		Json,
		FString::Printf(TEXT("SourceAssets/Characters/%s/character_manifest.json"), *Id),
		FPaths::ProjectDir(),
		false);
}

bool HasCode(
	const FWBCharacterManifestValidationResult& Result,
	const FString& Code)
{
	return Result.Diagnostics.ContainsByPredicate([&Code](const FWBCharacterManifestDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == Code;
	});
}

FString TestRoot(const FString& Name)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CharacterPipelineTests"), Name);
}

bool WriteSyntheticBundle(
	const FString& Root,
	const FString& CharacterId,
	const FString& Json,
	const FString& ModelRelative = TEXT("model/guardian.fbx"))
{
	const FString Bundle = FPaths::Combine(
		Root,
		TEXT("SourceAssets"),
		TEXT("Characters"),
		CharacterId);
	IFileManager::Get().MakeDirectory(*FPaths::Combine(Bundle, TEXT("model")), true);
	IFileManager::Get().MakeDirectory(*FPaths::Combine(Bundle, TEXT("textures")), true);
	IFileManager::Get().MakeDirectory(*FPaths::Combine(Bundle, TEXT("animations")), true);
	const bool bManifestWritten = FFileHelper::SaveStringToFile(
		Json,
		*FPaths::Combine(Bundle, TEXT("character_manifest.json")));
	const bool bModelWritten = FFileHelper::SaveStringToFile(
		TEXT("synthetic test bytes; never submitted to Unreal import"),
		*FPaths::Combine(Bundle, ModelRelative));
	return bManifestWritten && bModelWritten;
}

FWBCharacterPipelineRunResult MakeSyntheticRunResult(
	const FString& CharacterId = TEXT("synthetic_guardian"))
{
	FWBCharacterPipelineRunResult Result;
	Result.Validation = Parse(StaticManifest(CharacterId), CharacterId);
	Result.Destination = WBCharacterModelPipeline::BuildDestinationPlan(Result.Validation.Manifest);
	Result.Inventory.InventoryHash = TEXT("inventory_hash");
	Result.PresentationCandidate = WBCharacterModelPipeline::BuildPresentationCandidate(
		Result.Validation.Manifest,
		Result.Destination);
	Result.ReimportState = EWBCharacterReimportState::NeverImported;
	Result.bOk = true;
	Result.Reason = TEXT("dry_run_complete");
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestValidStaticTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.ValidMinimalStatic",
	PipelineFlags)
bool FWBCharacterManifestValidStaticTest::RunTest(const FString&)
{
	const FWBCharacterManifestValidationResult Result = Parse(StaticManifest());
	TestTrue(TEXT("Minimal static manifest is valid"), Result.IsValid());
	TestEqual(TEXT("Static type"), Result.Manifest.Source.ModelType, EWBCharacterModelType::Static);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestValidSkeletalTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.ValidMinimalSkeletal",
	PipelineFlags)
bool FWBCharacterManifestValidSkeletalTest::RunTest(const FString&)
{
	const FWBCharacterManifestValidationResult Result = Parse(SkeletalManifest(), TEXT("synthetic_rig"));
	TestTrue(TEXT("Minimal skeletal manifest is valid"), Result.IsValid());
	TestEqual(TEXT("GLB format"), Result.Manifest.Source.Format, EWBCharacterSourceFormat::GLB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestSchemaVersionTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.UnsupportedSchemaRejected",
	PipelineFlags)
bool FWBCharacterManifestSchemaVersionTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("\"schema_version\":1"), TEXT("\"schema_version\":2"));
	const FWBCharacterManifestValidationResult Result = Parse(Json);
	TestTrue(TEXT("Version diagnostic"), HasCode(Result, TEXT("manifest_schema_version_unsupported")));
	TestFalse(TEXT("Manifest invalid"), Result.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestApprovalTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.MissingApprovalRejected",
	PipelineFlags)
bool FWBCharacterManifestApprovalTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("\"approved_for_import\":true"), TEXT("\"approved_for_import\":false"));
	const FWBCharacterManifestValidationResult Result = Parse(Json);
	TestTrue(TEXT("Approval diagnostic"), HasCode(Result, TEXT("manifest_import_approval_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestIdTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.InvalidCharacterIdRejected",
	PipelineFlags)
bool FWBCharacterManifestIdTest::RunTest(const FString&)
{
	const FWBCharacterManifestValidationResult Result = Parse(
		StaticManifest(TEXT("Bad ID")),
		TEXT("Bad ID"));
	TestTrue(TEXT("ID diagnostic"), HasCode(Result, TEXT("manifest_character_id_invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestTraversalTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.PathTraversalRejected",
	PipelineFlags)
bool FWBCharacterManifestTraversalTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("model/guardian.fbx"), TEXT("../guardian.fbx"));
	TestTrue(
		TEXT("Traversal diagnostic"),
		HasCode(Parse(Json), TEXT("manifest_source_path_unsafe")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestAbsolutePathTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.AbsolutePathRejected",
	PipelineFlags)
bool FWBCharacterManifestAbsolutePathTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("model/guardian.fbx"), TEXT("C:/models/guardian.fbx"));
	TestTrue(
		TEXT("Absolute path diagnostic"),
		HasCode(Parse(Json), TEXT("manifest_source_path_unsafe")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestMissingModelTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.MissingModelRejected",
	PipelineFlags)
bool FWBCharacterManifestMissingModelTest::RunTest(const FString&)
{
	const FString Root = TestRoot(TEXT("MissingModel"));
	const FString Json = StaticManifest();
	const FString ManifestPath = TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json");
	const FString AbsoluteManifest = FPaths::Combine(Root, ManifestPath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteManifest), true);
	FFileHelper::SaveStringToFile(Json, *AbsoluteManifest);
	const FWBCharacterManifestValidationResult Result =
		WBCharacterModelPipeline::LoadAndValidateManifest(Root, ManifestPath);
	TestTrue(TEXT("Missing model diagnostic"), HasCode(Result, TEXT("manifest_source_model_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestExtensionTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.UnsupportedExtensionRejected",
	PipelineFlags)
bool FWBCharacterManifestExtensionTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("model/guardian.fbx"), TEXT("model/guardian.exe"));
	TestTrue(
		TEXT("Extension mismatch diagnostic"),
		HasCode(Parse(Json), TEXT("manifest_model_extension_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestScaleTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.InvalidScaleRejected",
	PipelineFlags)
bool FWBCharacterManifestScaleTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("\"scale\":1.0"), TEXT("\"scale\":0.0"));
	TestTrue(TEXT("Scale diagnostic"), HasCode(Parse(Json), TEXT("manifest_scale_invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestStaticContradictionTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.StaticAnimationContradictionRejected",
	PipelineFlags)
bool FWBCharacterManifestStaticContradictionTest::RunTest(const FString&)
{
	FString Json = StaticManifest();
	Json.ReplaceInline(TEXT("\"import_animations\":false"), TEXT("\"import_animations\":true"));
	TestTrue(
		TEXT("Static animation diagnostic"),
		HasCode(Parse(Json), TEXT("manifest_static_animation_contradiction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestDuplicateAnimationTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.DuplicateAnimationRoleRejected",
	PipelineFlags)
bool FWBCharacterManifestDuplicateAnimationTest::RunTest(const FString&)
{
	FString Json = SkeletalManifest();
	Json.ReplaceInline(
		TEXT("\"animations\":[]"),
		TEXT("\"animations\":[{\"role\":\"idle\",\"path\":\"animations/a.fbx\"},{\"role\":\"idle\",\"path\":\"animations/b.fbx\"}]"));
	Json.ReplaceInline(TEXT("\"import_animations\":false"), TEXT("\"import_animations\":true"));
	TestTrue(
		TEXT("Duplicate animation diagnostic"),
		HasCode(Parse(Json, TEXT("synthetic_rig")), TEXT("manifest_duplicate_animation_role")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestPrivateDataTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.PrivateGameplayFieldRejected",
	PipelineFlags)
bool FWBCharacterManifestPrivateDataTest::RunTest(const FString&)
{
	const FWBCharacterManifestValidationResult Result = Parse(
		StaticManifest(TEXT("synthetic_guardian"), TEXT(",\"private_hand\":[\"secret\"]")));
	TestTrue(TEXT("Private data diagnostic"), HasCode(Result, TEXT("manifest_private_data_forbidden")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestUnknownFieldTest,
	"Wandbound.Editor.CharacterModelPipeline.Manifest.UnknownFieldRejected",
	PipelineFlags)
bool FWBCharacterManifestUnknownFieldTest::RunTest(const FString&)
{
	const FWBCharacterManifestValidationResult Result = Parse(
		StaticManifest(TEXT("synthetic_guardian"), TEXT(",\"mystery_setting\":true")));
	TestTrue(TEXT("Unknown field diagnostic"), HasCode(Result, TEXT("manifest_unknown_field")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterSHA256KnownValueTest,
	"Wandbound.Editor.CharacterModelPipeline.Inventory.SHA256KnownValue",
	PipelineFlags)
bool FWBCharacterSHA256KnownValueTest::RunTest(const FString&)
{
	TestEqual(
		TEXT("SHA-256 of abc"),
		WBCharacterModelPipeline::SHA256String(TEXT("abc")),
		FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterInventoryHashTest,
	"Wandbound.Editor.CharacterModelPipeline.Inventory.HashAndStableOrder",
	PipelineFlags)
bool FWBCharacterInventoryHashTest::RunTest(const FString&)
{
	const FString Root = TestRoot(TEXT("Inventory"));
	const FString Json = StaticManifest();
	TestTrue(TEXT("Synthetic bundle written"), WriteSyntheticBundle(Root, TEXT("synthetic_guardian"), Json));
	const FWBCharacterManifestValidationResult Validation =
		WBCharacterModelPipeline::LoadAndValidateManifest(
			Root,
			TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json"));
	const FWBCharacterSourceInventory A =
		WBCharacterModelPipeline::BuildSourceInventory(Root, Validation.Manifest);
	const FWBCharacterSourceInventory B =
		WBCharacterModelPipeline::BuildSourceInventory(Root, Validation.Manifest);
	TestTrue(TEXT("Inventory has manifest and model"), A.Entries.Num() >= 2);
	TestEqual(TEXT("Inventory hash stable"), A.InventoryHash, B.InventoryHash);
	const FWBCharacterSourceInventoryEntry* ModelEntry = A.Entries.FindByPredicate(
		[](const FWBCharacterSourceInventoryEntry& Entry)
		{
			return Entry.RelativePath == TEXT("model/guardian.fbx");
		});
	TestNotNull(TEXT("Primary model path is bundle-relative"), ModelEntry);
	if (ModelEntry != nullptr)
	{
		TestEqual(TEXT("Primary role recognized"), ModelEntry->DeclaredRole, FString(TEXT("primary_model")));
		TestTrue(TEXT("Primary model required"), ModelEntry->bRequired);
	}
	const FString InventoryJson = WBCharacterModelPipeline::InventoryToJson(A);
	TestTrue(TEXT("Git status serialized"), InventoryJson.Contains(TEXT("\"git_status\"")));
	TestTrue(TEXT("LFS status serialized separately"), InventoryJson.Contains(TEXT("\"lfs_status\"")));
	for (int32 Index = 1; Index < A.Entries.Num(); ++Index)
	{
		TestTrue(TEXT("Paths sorted"), A.Entries[Index - 1].RelativePath < A.Entries[Index].RelativePath);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterOptionalDependencyTest,
	"Wandbound.Editor.CharacterModelPipeline.Inventory.OptionalDependenciesWarnOnly",
	PipelineFlags)
bool FWBCharacterOptionalDependencyTest::RunTest(const FString&)
{
	const FString Root = TestRoot(TEXT("Optional"));
	FString Json = StaticManifest();
	Json.ReplaceInline(
		TEXT("\"textures\":[]"),
		TEXT("\"textures\":[{\"role\":\"normal\",\"path\":\"textures/missing.png\",\"required\":false}]"));
	Json.ReplaceInline(TEXT("\"previews\":{}"), TEXT("\"previews\":{\"front\":\"previews/missing.png\"}"));
	TestTrue(TEXT("Synthetic bundle written"), WriteSyntheticBundle(Root, TEXT("synthetic_guardian"), Json));
	const FWBCharacterManifestValidationResult Result =
		WBCharacterModelPipeline::LoadAndValidateManifest(
			Root,
			TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json"));
	TestTrue(TEXT("Optional texture warning"), HasCode(Result, TEXT("manifest_optional_texture_missing")));
	TestTrue(TEXT("Optional preview warning"), HasCode(Result, TEXT("manifest_optional_preview_missing")));
	TestTrue(TEXT("Warnings do not invalidate"), Result.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterDestinationStaticTest,
	"Wandbound.Editor.CharacterModelPipeline.Destination.StaticMappingDeterministic",
	PipelineFlags)
bool FWBCharacterDestinationStaticTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const FWBCharacterDestinationPlan A = WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	const FWBCharacterDestinationPlan B = WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	TestEqual(
		TEXT("Static mesh package"),
		A.PrimaryMeshPackage,
		FString(TEXT("/Game/Wandbound/Characters/synthetic_guardian/Meshes/SM_synthetic_guardian")));
	TestEqual(TEXT("Repeated plan"), A.IntendedPackages, B.IntendedPackages);
	TestTrue(TEXT("No skeleton"), A.SkeletonPackage.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterDestinationSkeletalTest,
	"Wandbound.Editor.CharacterModelPipeline.Destination.SkeletalMappingDeterministic",
	PipelineFlags)
bool FWBCharacterDestinationSkeletalTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(SkeletalManifest(), TEXT("synthetic_rig")).Manifest;
	const FWBCharacterDestinationPlan Plan = WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	TestEqual(
		TEXT("Skeletal mesh package"),
		Plan.PrimaryMeshPackage,
		FString(TEXT("/Game/Wandbound/Characters/synthetic_rig/Meshes/SK_synthetic_rig")));
	TestFalse(TEXT("Skeleton planned"), Plan.SkeletonPackage.IsEmpty());
	TestFalse(TEXT("Physics planned"), Plan.PhysicsPackage.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterImportStaticSpecTest,
	"Wandbound.Editor.CharacterModelPipeline.Import.StaticTaskConfiguration",
	PipelineFlags)
bool FWBCharacterImportStaticSpecTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const TArray<FWBCharacterImportTaskSpec> Specs =
		WBCharacterModelPipeline::BuildImportTaskSpecs(
			Manifest,
			WBCharacterModelPipeline::BuildDestinationPlan(Manifest),
			false);
	TestEqual(TEXT("One primary task"), Specs.Num(), 1);
	TestEqual(TEXT("Expected class"), Specs[0].ExpectedClass, FString(TEXT("StaticMesh")));
	TestTrue(TEXT("Collision policy"), Specs[0].bGenerateCollision);
	TestFalse(TEXT("No animation"), Specs[0].bImportAnimations);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterImportSkeletalSpecTest,
	"Wandbound.Editor.CharacterModelPipeline.Import.SkeletalTaskConfiguration",
	PipelineFlags)
bool FWBCharacterImportSkeletalSpecTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(SkeletalManifest(), TEXT("synthetic_rig")).Manifest;
	const TArray<FWBCharacterImportTaskSpec> Specs =
		WBCharacterModelPipeline::BuildImportTaskSpecs(
			Manifest,
			WBCharacterModelPipeline::BuildDestinationPlan(Manifest),
			true);
	TestEqual(TEXT("Expected class"), Specs[0].ExpectedClass, FString(TEXT("SkeletalMesh")));
	TestTrue(TEXT("Physics policy"), Specs[0].bCreatePhysicsAsset);
	TestTrue(TEXT("Replace on reimport"), Specs[0].bReplaceExisting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterReimportUpToDateTest,
	"Wandbound.Editor.CharacterModelPipeline.Reimport.UnchangedIsUpToDate",
	PipelineFlags)
bool FWBCharacterReimportUpToDateTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const FWBCharacterDestinationPlan Destination =
		WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	FWBCharacterSourceInventory Inventory;
	Inventory.InventoryHash = TEXT("same");
	FWBCharacterImportReceipt Receipt;
	Receipt.CharacterId = Manifest.CharacterId;
	Receipt.ManifestSchemaVersion = Manifest.SchemaVersion;
	Receipt.SourceInventoryHash = Inventory.InventoryHash;
	Receipt.ImportSettingsDigest = WBCharacterModelPipeline::BuildImportSettingsDigest(Manifest);
	Receipt.DestinationPackages = Destination.IntendedPackages;
	Receipt.LastResult = TEXT("success");
	TestEqual(
		TEXT("Up to date"),
		WBCharacterModelPipeline::DetermineReimportState(
			Manifest, Inventory, Destination, &Receipt, true),
		EWBCharacterReimportState::UpToDate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterReimportSourceTest,
	"Wandbound.Editor.CharacterModelPipeline.Reimport.SourceChangeDetected",
	PipelineFlags)
bool FWBCharacterReimportSourceTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const FWBCharacterDestinationPlan Destination =
		WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	FWBCharacterSourceInventory Inventory;
	Inventory.InventoryHash = TEXT("new");
	FWBCharacterSourceInventoryEntry Current;
	Current.RelativePath = Manifest.Source.ModelPath;
	Current.SHA256 = TEXT("new_hash");
	Inventory.Entries.Add(Current);
	FWBCharacterImportReceipt Receipt;
	Receipt.CharacterId = Manifest.CharacterId;
	Receipt.ManifestSchemaVersion = Manifest.SchemaVersion;
	Receipt.SourceInventoryHash = TEXT("old");
	Receipt.ImportSettingsDigest = WBCharacterModelPipeline::BuildImportSettingsDigest(Manifest);
	Receipt.DestinationPackages = Destination.IntendedPackages;
	Receipt.LastResult = TEXT("success");
	FWBCharacterSourceInventoryEntry Previous = Current;
	Previous.SHA256 = TEXT("old_hash");
	Receipt.SourceEntries.Add(Previous);
	TestEqual(
		TEXT("Source changed"),
		WBCharacterModelPipeline::DetermineReimportState(
			Manifest, Inventory, Destination, &Receipt, true),
		EWBCharacterReimportState::SourceChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterReimportSettingsTest,
	"Wandbound.Editor.CharacterModelPipeline.Reimport.SettingsChangeDetected",
	PipelineFlags)
bool FWBCharacterReimportSettingsTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const FWBCharacterDestinationPlan Destination =
		WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	FWBCharacterSourceInventory Inventory;
	Inventory.InventoryHash = TEXT("same");
	FWBCharacterImportReceipt Receipt;
	Receipt.CharacterId = Manifest.CharacterId;
	Receipt.ManifestSchemaVersion = Manifest.SchemaVersion;
	Receipt.SourceInventoryHash = TEXT("same");
	Receipt.ImportSettingsDigest = TEXT("different");
	Receipt.DestinationPackages = Destination.IntendedPackages;
	Receipt.LastResult = TEXT("success");
	TestEqual(
		TEXT("Settings changed"),
		WBCharacterModelPipeline::DetermineReimportState(
			Manifest, Inventory, Destination, &Receipt, true),
		EWBCharacterReimportState::SettingsChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterReimportDestinationTest,
	"Wandbound.Editor.CharacterModelPipeline.Reimport.MissingDestinationDetected",
	PipelineFlags)
bool FWBCharacterReimportDestinationTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const FWBCharacterDestinationPlan Destination =
		WBCharacterModelPipeline::BuildDestinationPlan(Manifest);
	FWBCharacterSourceInventory Inventory;
	FWBCharacterImportReceipt Receipt;
	Receipt.CharacterId = Manifest.CharacterId;
	Receipt.LastResult = TEXT("success");
	TestEqual(
		TEXT("Destination missing"),
		WBCharacterModelPipeline::DetermineReimportState(
			Manifest, Inventory, Destination, &Receipt, false),
		EWBCharacterReimportState::DestinationMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterCatalogAtomicTest,
	"Wandbound.Editor.CharacterModelPipeline.Catalog.AtomicStableOrdering",
	PipelineFlags)
bool FWBCharacterCatalogAtomicTest::RunTest(const FString&)
{
	const FString CatalogPath = FPaths::Combine(TestRoot(TEXT("Catalog")), TEXT("catalog.json"));
	FString Failure;
	TestTrue(
		TEXT("Add second"),
		WBCharacterModelPipeline::UpdateCatalogAtomic(
			CatalogPath, MakeSyntheticRunResult(TEXT("zeta_unit")), Failure));
	TestTrue(
		TEXT("Add first"),
		WBCharacterModelPipeline::UpdateCatalogAtomic(
			CatalogPath, MakeSyntheticRunResult(TEXT("alpha_unit")), Failure));
	FString Json;
	FFileHelper::LoadFileToString(Json, *CatalogPath);
	TestTrue(TEXT("Stable order"), Json.Find(TEXT("alpha_unit")) < Json.Find(TEXT("zeta_unit")));
	TestFalse(TEXT("No private field"), Json.Contains(TEXT("private_hand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterCatalogCorruptTest,
	"Wandbound.Editor.CharacterModelPipeline.Catalog.CorruptionFailsSafely",
	PipelineFlags)
bool FWBCharacterCatalogCorruptTest::RunTest(const FString&)
{
	const FString CatalogPath = FPaths::Combine(TestRoot(TEXT("CatalogCorrupt")), TEXT("catalog.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CatalogPath), true);
	FFileHelper::SaveStringToFile(TEXT("not json"), *CatalogPath);
	FString Failure;
	TestFalse(
		TEXT("Corrupt catalog rejected"),
		WBCharacterModelPipeline::UpdateCatalogAtomic(
			CatalogPath, MakeSyntheticRunResult(), Failure));
	TestEqual(TEXT("Clear reason"), Failure, FString(TEXT("character_catalog_corrupt")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterReportTest,
	"Wandbound.Editor.CharacterModelPipeline.Reports.JsonAndMarkdownContracts",
	PipelineFlags)
bool FWBCharacterReportTest::RunTest(const FString&)
{
	const FWBCharacterPipelineRunResult Result = MakeSyntheticRunResult();
	const FString Json = WBCharacterModelPipeline::BuildImportReportJson(Result);
	const FString Markdown = WBCharacterModelPipeline::BuildImportReportMarkdown(Result);
	TestTrue(TEXT("JSON schema"), Json.Contains(TEXT("\"report_schema_version\":1")));
	TestTrue(TEXT("JSON hash"), Json.Contains(TEXT("\"inventory_hash\":\"inventory_hash\"")));
	TestTrue(TEXT("Markdown readable"), Markdown.Contains(TEXT("# Wandbound Character Import Report")));
	TestFalse(TEXT("No absolute project path"), Json.Contains(FPaths::ProjectDir()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterPresentationStaticTest,
	"Wandbound.Editor.CharacterModelPipeline.Presentation.StaticCandidateUsesFallbacks",
	PipelineFlags)
bool FWBCharacterPresentationStaticTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(StaticManifest()).Manifest;
	const FWBCharacterPresentationCandidate Candidate =
		WBCharacterModelPipeline::BuildPresentationCandidate(
			Manifest,
			WBCharacterModelPipeline::BuildDestinationPlan(Manifest));
	TestEqual(TEXT("Static candidate"), Candidate.ModelType, FString(TEXT("static")));
	TestTrue(TEXT("Idle fallback"), Candidate.FallbackRequirements.Contains(TEXT("idle")));
	TestTrue(TEXT("Attack fallback"), Candidate.FallbackRequirements.Contains(TEXT("attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterPresentationSkeletalTest,
	"Wandbound.Editor.CharacterModelPipeline.Presentation.SkeletalCandidateStable",
	PipelineFlags)
bool FWBCharacterPresentationSkeletalTest::RunTest(const FString&)
{
	const FWBCharacterModelManifest Manifest = Parse(SkeletalManifest(), TEXT("synthetic_rig")).Manifest;
	const FWBCharacterPresentationCandidate Candidate =
		WBCharacterModelPipeline::BuildPresentationCandidate(
			Manifest,
			WBCharacterModelPipeline::BuildDestinationPlan(Manifest));
	TestEqual(TEXT("Skeletal candidate"), Candidate.ModelType, FString(TEXT("skeletal")));
	TestEqual(TEXT("Public definition"), Candidate.CardDefinitionId, Manifest.CardDefinitionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterCookExactTest,
	"Wandbound.Editor.CharacterModelPipeline.Cook.ExactPackagesAccepted",
	PipelineFlags)
bool FWBCharacterCookExactTest::RunTest(const FString&)
{
	FWBCharacterCookVerificationRequest Request;
	Request.ExactPackages = {
		TEXT("/Game/Wandbound/Characters/synthetic_guardian/Meshes/SM_synthetic_guardian"),
		TEXT("/Game/Wandbound/Characters/synthetic_guardian/Textures/T_synthetic_guardian_base_color")
	};
	const FWBCharacterCookVerificationResult Result =
		WBCharacterModelPipeline::ValidateCookPackageList(Request);
	TestTrue(TEXT("Exact packages valid"), Result.bValid);
	TestEqual(TEXT("Both accepted"), Result.AcceptedPackages.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterCookGuardTest,
	"Wandbound.Editor.CharacterModelPipeline.Cook.SourceGodotAndMeshyRejected",
	PipelineFlags)
bool FWBCharacterCookGuardTest::RunTest(const FString&)
{
	FWBCharacterCookVerificationRequest Request;
	Request.ExactPackages = {
		TEXT("/Game/SourceAssets/Characters/raw"),
		TEXT("/Game/Meshy/OldCharacter"),
		TEXT("/Game/Wandbound/Godot/reference")
	};
	const FWBCharacterCookVerificationResult Result =
		WBCharacterModelPipeline::ValidateCookPackageList(Request);
	TestFalse(TEXT("Forbidden packages invalid"), Result.bValid);
	TestEqual(TEXT("Nothing accepted"), Result.AcceptedPackages.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterGitClassificationTest,
	"Wandbound.Editor.CharacterModelPipeline.Git.ReadOnlyClassification",
	PipelineFlags)
bool FWBCharacterGitClassificationTest::RunTest(const FString&)
{
	TestEqual(
		TEXT("Tracked text"),
		WBCharacterModelPipeline::ClassifyRepositoryFile(
			FPaths::ProjectDir(), TEXT("WandboundUE.uproject")),
		EWBCharacterGitStatus::TrackedGit);
	TestEqual(
		TEXT("Tracked starter asset uses LFS"),
		WBCharacterModelPipeline::ClassifyRepositoryFile(
			FPaths::ProjectDir(),
			TEXT("Content/Wandbound/Presentation/DA_WandboundStarterPresentation.uasset")),
		EWBCharacterGitStatus::TrackedLFS);
	TestEqual(
		TEXT("Missing"),
		WBCharacterModelPipeline::ClassifyRepositoryFile(
			FPaths::ProjectDir(), TEXT("SourceAssets/Characters/no_such_file.fbx")),
		EWBCharacterGitStatus::Missing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterScriptsContractTest,
	"Wandbound.Editor.CharacterModelPipeline.PowerShell.SafeWrapperContract",
	PipelineFlags)
bool FWBCharacterScriptsContractTest::RunTest(const FString&)
{
	FString ImportScript;
	FString NewScript;
	TestTrue(
		TEXT("Import wrapper readable"),
		FFileHelper::LoadFileToString(
			ImportScript,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts/Assets/ImportWandboundCharacter.ps1"))));
	TestTrue(
		TEXT("Template wrapper readable"),
		FFileHelper::LoadFileToString(
			NewScript,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts/Assets/NewWandboundCharacterBundle.ps1"))));
	const FString Both = ImportScript + NewScript;
	TestTrue(TEXT("Validate default"), ImportScript.Contains(TEXT("[string]$Mode = 'Validate'")));
	TestTrue(TEXT("Modes supported"), ImportScript.Contains(TEXT("'Validate', 'DryRun', 'Import', 'Reimport'")));
	TestTrue(TEXT("Paths passed as array"), ImportScript.Contains(TEXT("@commandletArguments")));
	TestTrue(TEXT("Existing bundle rejected"), NewScript.Contains(TEXT("will not be overwritten")));
	TestFalse(TEXT("No git add"), Both.Contains(TEXT("git add"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("No git commit"), Both.Contains(TEXT("git commit"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("No git push"), Both.Contains(TEXT("git push"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("No source deletion"), Both.Contains(TEXT("Remove-Item"), ESearchCase::IgnoreCase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterSchemaFilesTest,
	"Wandbound.Editor.CharacterModelPipeline.Schema.VersionedFilesPresent",
	PipelineFlags)
bool FWBCharacterSchemaFilesTest::RunTest(const FString&)
{
	FString ManifestSchema;
	FString CatalogSchema;
	TestTrue(
		TEXT("Manifest schema readable"),
		FFileHelper::LoadFileToString(
			ManifestSchema,
			*FPaths::Combine(
				FPaths::ProjectDir(),
				WBCharacterModelPipeline::GetManifestSchemaRepositoryPath())));
	TestTrue(
		TEXT("Catalog schema readable"),
		FFileHelper::LoadFileToString(
			CatalogSchema,
			*FPaths::Combine(
				FPaths::ProjectDir(),
				WBCharacterModelPipeline::GetCatalogSchemaRepositoryPath())));
	TestTrue(TEXT("Manifest schema version"), ManifestSchema.Contains(TEXT("\"const\": 1")));
	TestTrue(TEXT("Catalog has characters"), CatalogSchema.Contains(TEXT("\"characters\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterAuthorityGuardTest,
	"Wandbound.Editor.CharacterModelPipeline.Authority.EditorOnlyAndNoGameplayMutation",
	PipelineFlags)
bool FWBCharacterAuthorityGuardTest::RunTest(const FString&)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(
		Files,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundEditor")),
		TEXT("*.cpp"),
		true,
		false);
	FString Combined;
	for (const FString& File : Files)
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *File);
		Combined += Contents;
	}
	TestFalse(TEXT("No game state mutation"), Combined.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("No effect runner"), Combined.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("No action codec"), Combined.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No Meshy dependency"), Combined.Contains(TEXT("Plugins/meshy"), ESearchCase::IgnoreCase));

	FString RuntimeBuild;
	FFileHelper::LoadFileToString(
		RuntimeBuild,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/WandboundRuntime.Build.cs")));
	TestFalse(TEXT("Runtime does not depend on editor module"), RuntimeBuild.Contains(TEXT("WandboundEditor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterDryRunTest,
	"Wandbound.Editor.CharacterModelPipeline.Run.SyntheticDryRunWritesTextOnly",
	PipelineFlags)
bool FWBCharacterDryRunTest::RunTest(const FString&)
{
	const FString Root = TestRoot(TEXT("DryRun"));
	TestTrue(
		TEXT("Synthetic bundle written"),
		WriteSyntheticBundle(Root, TEXT("synthetic_guardian"), StaticManifest()));
	FWBCharacterPipelineRunOptions Options;
	Options.Mode = EWBCharacterPipelineMode::DryRun;
	Options.ManifestRepositoryPath =
		TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json");
	Options.bGeneratePreview = true;
	Options.bValidateCook = true;
	const FWBCharacterPipelineRunResult Result =
		WBCharacterModelPipeline::Run(Root, Options);
	TestTrue(TEXT("Dry run succeeds"), Result.bOk);
	TestTrue(TEXT("No imported objects"), Result.ImportedObjectPaths.IsEmpty());
	TestTrue(
		TEXT("Markdown report written"),
		IFileManager::Get().FileExists(
			*FPaths::Combine(
				Root,
				TEXT("Docs/AssetImports/synthetic_guardian/ImportReport.md"))));
	FString PreviewResult;
	TestTrue(
		TEXT("Preview result written"),
		FFileHelper::LoadFileToString(
			PreviewResult,
			*FPaths::Combine(
				Root,
				TEXT("Docs/AssetImports/synthetic_guardian/PreviewResult.json"))));
	TestTrue(
		TEXT("NullRHI preview is explicitly unsupported"),
		PreviewResult.Contains(TEXT("\"status\":\"unsupported_under_null_rhi\"")));
	TestTrue(
		TEXT("Preview output path stable"),
		PreviewResult.Contains(
			TEXT("Docs/AssetImports/synthetic_guardian/UnrealPreview.png")));
	TestFalse(
		TEXT("No generated Unreal character content"),
		IFileManager::Get().DirectoryExists(
			*FPaths::Combine(Root, TEXT("Content/Wandbound/Characters/synthetic_guardian"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestCardDBMappingAcceptedTest,
	"Wandbound.Editor.CharacterModelPipeline.CardDB.ValidCharacterMapping",
	PipelineFlags)
bool FWBCharacterManifestCardDBMappingAcceptedTest::RunTest(const FString&)
{
	FWBCardDefinition Character;
	Character.CardId = TEXT("fixture_character");
	Character.PublicName = TEXT("Fixture Character");
	Character.Kind = EWBCardDefinitionKind::Character;
	Character.CharacterStats.HP = 1;
	FWBCardDefinitionRepository Repository;
	TestTrue(
		TEXT("Repository builds"),
		WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
			TEXT("character_mapping"),
			TEXT("test"),
			{ Character },
			Repository).bOk);
	const FString Json = StaticManifest().Replace(
		TEXT("CHAR_SYNTHETIC_GUARDIAN"),
		TEXT("fixture_character"));
	const FWBCharacterManifestValidationResult Result =
		WBCharacterModelPipeline::ParseAndValidateManifestJson(
			Json,
			TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json"),
			FPaths::ProjectDir(),
			false,
			&Repository,
			true);
	TestTrue(TEXT("Character mapping is valid"), Result.IsValid());
	TestFalse(
		TEXT("Repository unavailable warning removed"),
		HasCode(Result, TEXT("card_definition_repository_unavailable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestCardDBMissingMappingTest,
	"Wandbound.Editor.CharacterModelPipeline.CardDB.MissingDefinition",
	PipelineFlags)
bool FWBCharacterManifestCardDBMissingMappingTest::RunTest(const FString&)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("empty_mapping");
	Repository.SourceVersion = TEXT("test");
	const FWBCharacterManifestValidationResult WarningResult =
		WBCharacterModelPipeline::ParseAndValidateManifestJson(
			StaticManifest(),
			TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json"),
			FPaths::ProjectDir(),
			false,
			&Repository,
			false);
	TestTrue(TEXT("Optional mapping remains valid"), WarningResult.IsValid());
	TestTrue(
		TEXT("Missing mapping warning"),
		HasCode(WarningResult, TEXT("card_definition_not_found")));

	const FWBCharacterManifestValidationResult ErrorResult =
		WBCharacterModelPipeline::ParseAndValidateManifestJson(
			StaticManifest(),
			TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json"),
			FPaths::ProjectDir(),
			false,
			&Repository,
			true);
	TestFalse(TEXT("Required mapping fails"), ErrorResult.IsValid());
	TestTrue(
		TEXT("Missing mapping diagnosed"),
		HasCode(ErrorResult, TEXT("card_definition_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCharacterManifestCardDBWrongTypeTest,
	"Wandbound.Editor.CharacterModelPipeline.CardDB.WrongDefinitionType",
	PipelineFlags)
bool FWBCharacterManifestCardDBWrongTypeTest::RunTest(const FString&)
{
	FWBCardDefinition Wand;
	Wand.CardId = TEXT("fixture_wand");
	Wand.PublicName = TEXT("Fixture Wand");
	Wand.Kind = EWBCardDefinitionKind::Wand;
	FWBCardDefinitionRepository Repository;
	TestTrue(
		TEXT("Repository builds"),
		WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
			TEXT("wand_mapping"),
			TEXT("test"),
			{ Wand },
			Repository).bOk);
	const FString Json = StaticManifest().Replace(
		TEXT("CHAR_SYNTHETIC_GUARDIAN"),
		TEXT("fixture_wand"));
	const FWBCharacterManifestValidationResult Result =
		WBCharacterModelPipeline::ParseAndValidateManifestJson(
			Json,
			TEXT("SourceAssets/Characters/synthetic_guardian/character_manifest.json"),
			FPaths::ProjectDir(),
			false,
			&Repository,
			true);
	TestFalse(TEXT("Wand mapping fails"), Result.IsValid());
	TestTrue(
		TEXT("Wrong kind diagnosed"),
		HasCode(Result, TEXT("card_definition_kind_mismatch")));
	return true;
}

#endif
