#pragma once

#include "CoreMinimal.h"

struct FWBCardDefinitionRepository;

enum class EWBCharacterDiagnosticSeverity : uint8
{
	Warning,
	Error
};

enum class EWBCharacterModelType : uint8
{
	Static,
	Skeletal
};

enum class EWBCharacterSourceFormat : uint8
{
	FBX,
	GLB,
	GLTF
};

enum class EWBCharacterReimportState : uint8
{
	NeverImported,
	UpToDate,
	SourceChanged,
	ManifestChanged,
	SettingsChanged,
	DestinationMissing,
	DependencyMissing,
	ReimportRequired,
	ImportFailed
};

enum class EWBCharacterGitStatus : uint8
{
	TrackedGit,
	TrackedLFS,
	Untracked,
	Ignored,
	Missing,
	OutsideRepository,
	EngineAsset
};

enum class EWBCharacterPipelineMode : uint8
{
	Validate,
	DryRun,
	Import,
	Reimport
};

struct WANDBOUNDEDITOR_API FWBCharacterManifestDiagnostic
{
	EWBCharacterDiagnosticSeverity Severity = EWBCharacterDiagnosticSeverity::Error;
	FString Code;
	FString Message;
	FString ManifestPath;
	FString FieldPath;
	FString SourceRelativePath;
	FString RecommendedAction;
};

struct WANDBOUNDEDITOR_API FWBCharacterAnimationDefinition
{
	FString Role;
	FString SourcePath;
	bool bRequired = false;
};

struct WANDBOUNDEDITOR_API FWBCharacterTextureDefinition
{
	FString Role;
	FString SourcePath;
	bool bRequired = false;
};

struct WANDBOUNDEDITOR_API FWBCharacterModelSourceDefinition
{
	FString ModelPath;
	EWBCharacterSourceFormat Format = EWBCharacterSourceFormat::FBX;
	EWBCharacterModelType ModelType = EWBCharacterModelType::Static;
	FString TexturesDirectory = TEXT("textures");
	FString AnimationsDirectory = TEXT("animations");
	TArray<FWBCharacterTextureDefinition> Textures;
	TArray<FWBCharacterAnimationDefinition> Animations;
};

struct WANDBOUNDEDITOR_API FWBCharacterImportSettings
{
	bool bImportMaterials = true;
	bool bImportTextures = true;
	bool bImportAnimations = false;
	bool bCreatePhysicsAsset = false;
	bool bGenerateCollision = true;
	FString SkeletonPolicy = TEXT("none");
	FString ExistingSkeletonPackage;
	FString NormalPolicy = TEXT("compute_normals");
};

struct WANDBOUNDEDITOR_API FWBCharacterPresentationSettings
{
	FString Role = TEXT("player_unit");
	double Scale = 1.0;
	FVector Rotation = FVector::ZeroVector;
	FVector Offset = FVector::ZeroVector;
	FString FacingAxis = TEXT("positive_x");
};

struct WANDBOUNDEDITOR_API FWBCharacterModelManifest
{
	int32 SchemaVersion = 0;
	FString CharacterId;
	FString DisplayName;
	FString CardDefinitionId;
	bool bApprovedForImport = false;
	FString ApprovedBy;
	FString ApprovalNote;
	FWBCharacterModelSourceDefinition Source;
	FWBCharacterImportSettings Import;
	FWBCharacterPresentationSettings Presentation;
	TMap<FString, FString> PreviewPaths;
	TArray<FString> Tags;
	FString Notes;
	FString ManifestRepositoryPath;
	FString BundleRepositoryPath;
	FString BundleAbsolutePath;
	FString NormalizedJson;
};

struct WANDBOUNDEDITOR_API FWBCharacterManifestValidationResult
{
	bool bParsed = false;
	FWBCharacterModelManifest Manifest;
	TArray<FWBCharacterManifestDiagnostic> Diagnostics;

	bool IsValid() const;
	int32 ErrorCount() const;
	int32 WarningCount() const;
};

struct WANDBOUNDEDITOR_API FWBCharacterSourceInventoryEntry
{
	FString RelativePath;
	FString FileType;
	int64 SizeBytes = 0;
	FString SHA256;
	FString DeclaredRole;
	bool bRequired = false;
	EWBCharacterGitStatus GitStatus = EWBCharacterGitStatus::Untracked;
};

struct WANDBOUNDEDITOR_API FWBCharacterSourceInventory
{
	TArray<FWBCharacterSourceInventoryEntry> Entries;
	FString InventoryHash;
	TArray<FWBCharacterManifestDiagnostic> Diagnostics;
};

struct WANDBOUNDEDITOR_API FWBCharacterDestinationPlan
{
	FString CharacterId;
	FString DestinationRoot;
	FString PrimaryMeshPackage;
	FString SkeletonPackage;
	FString PhysicsPackage;
	TMap<FString, FString> TexturePackages;
	TMap<FString, FString> AnimationPackages;
	TArray<FString> IntendedPackages;
	TArray<FWBCharacterManifestDiagnostic> Diagnostics;

	bool IsValid() const;
};

struct WANDBOUNDEDITOR_API FWBCharacterImportTaskSpec
{
	FString SourceAbsolutePath;
	FString DestinationPath;
	FString DestinationName;
	FString SemanticRole;
	FString ExpectedClass;
	bool bReplaceExisting = false;
	bool bImportMaterials = false;
	bool bImportTextures = false;
	bool bImportAnimations = false;
	bool bCreatePhysicsAsset = false;
	bool bGenerateCollision = false;
};

struct WANDBOUNDEDITOR_API FWBCharacterImportReceipt
{
	int32 ReceiptSchemaVersion = 1;
	int32 ManifestSchemaVersion = 1;
	FString CharacterId;
	FString ManifestHash;
	FString SourceInventoryHash;
	FString ImportSettingsDigest;
	FString ImporterVersion = TEXT("wandbound_character_importer_v1");
	FString EngineVersion;
	TArray<FWBCharacterSourceInventoryEntry> SourceEntries;
	TArray<FString> DestinationPackages;
	FString LastResult;
	FString AuditTimestampUtc;
};

struct WANDBOUNDEDITOR_API FWBCharacterPresentationCandidate
{
	FString CharacterId;
	FString CardDefinitionId;
	FString ModelType;
	FString PrimaryMeshPackage;
	FString SkeletonPackage;
	double Scale = 1.0;
	FVector Rotation = FVector::ZeroVector;
	FVector Offset = FVector::ZeroVector;
	TMap<FString, FString> AnimationPackages;
	TArray<FString> MaterialPackages;
	FString ValidationStatus;
	TArray<FString> FallbackRequirements;
};

struct WANDBOUNDEDITOR_API FWBCharacterCookVerificationRequest
{
	TArray<FString> ExactPackages;
};

struct WANDBOUNDEDITOR_API FWBCharacterCookVerificationResult
{
	bool bValid = true;
	TArray<FString> AcceptedPackages;
	TArray<FWBCharacterManifestDiagnostic> Diagnostics;
};

struct WANDBOUNDEDITOR_API FWBCharacterPipelineRunOptions
{
	EWBCharacterPipelineMode Mode = EWBCharacterPipelineMode::Validate;
	FString ManifestRepositoryPath;
	bool bGeneratePreview = false;
	bool bValidateCook = false;
	bool bWriteReports = true;
	bool bUpdateCatalog = true;
	const FWBCardDefinitionRepository* CardDefinitionRepository = nullptr;
	bool bRequireCardDefinition = false;
};

struct WANDBOUNDEDITOR_API FWBCharacterPipelineRunResult
{
	bool bOk = false;
	FString Reason;
	FWBCharacterManifestValidationResult Validation;
	FWBCharacterSourceInventory Inventory;
	FWBCharacterDestinationPlan Destination;
	FWBCharacterImportReceipt Receipt;
	FWBCharacterPresentationCandidate PresentationCandidate;
	FWBCharacterCookVerificationResult CookVerification;
	EWBCharacterReimportState ReimportState = EWBCharacterReimportState::NeverImported;
	TArray<FString> ImportedObjectPaths;
	TArray<FString> GeneratedReportPaths;
	TArray<FWBCharacterManifestDiagnostic> Diagnostics;
};

namespace WBCharacterModelPipeline
{
	WANDBOUNDEDITOR_API const TCHAR* GetManifestSchemaRepositoryPath();
	WANDBOUNDEDITOR_API const TCHAR* GetCatalogRepositoryPath();
	WANDBOUNDEDITOR_API const TCHAR* GetCatalogSchemaRepositoryPath();

	WANDBOUNDEDITOR_API bool IsSafeCharacterId(const FString& Value);
	WANDBOUNDEDITOR_API bool IsSafeCardDefinitionId(const FString& Value);
	WANDBOUNDEDITOR_API bool IsSafeBundleRelativePath(const FString& Value);
	WANDBOUNDEDITOR_API FString NormalizeRepositoryPath(const FString& Value);
	WANDBOUNDEDITOR_API FString SHA256File(const FString& AbsolutePath);
	WANDBOUNDEDITOR_API FString SHA256String(const FString& Value);

	WANDBOUNDEDITOR_API FWBCharacterManifestValidationResult ParseAndValidateManifestJson(
		const FString& Json,
		const FString& ManifestRepositoryPath,
		const FString& ProjectRoot,
		bool bRequireFiles,
		const FWBCardDefinitionRepository* CardDefinitionRepository = nullptr,
		bool bRequireCardDefinition = false);
	WANDBOUNDEDITOR_API FWBCharacterManifestValidationResult LoadAndValidateManifest(
		const FString& ProjectRoot,
		const FString& ManifestRepositoryPath,
		const FWBCardDefinitionRepository* CardDefinitionRepository = nullptr,
		bool bRequireCardDefinition = false);
	WANDBOUNDEDITOR_API FWBCharacterSourceInventory BuildSourceInventory(
		const FString& ProjectRoot,
		const FWBCharacterModelManifest& Manifest);
	WANDBOUNDEDITOR_API FWBCharacterDestinationPlan BuildDestinationPlan(
		const FWBCharacterModelManifest& Manifest);
	WANDBOUNDEDITOR_API TArray<FWBCharacterImportTaskSpec> BuildImportTaskSpecs(
		const FWBCharacterModelManifest& Manifest,
		const FWBCharacterDestinationPlan& Destination,
		bool bReplaceExisting);
	WANDBOUNDEDITOR_API FString BuildImportSettingsDigest(
		const FWBCharacterModelManifest& Manifest);
	WANDBOUNDEDITOR_API EWBCharacterReimportState DetermineReimportState(
		const FWBCharacterModelManifest& Manifest,
		const FWBCharacterSourceInventory& Inventory,
		const FWBCharacterDestinationPlan& Destination,
		const FWBCharacterImportReceipt* PreviousReceipt,
		bool bDestinationsExist);
	WANDBOUNDEDITOR_API FWBCharacterPresentationCandidate BuildPresentationCandidate(
		const FWBCharacterModelManifest& Manifest,
		const FWBCharacterDestinationPlan& Destination);
	WANDBOUNDEDITOR_API FWBCharacterCookVerificationResult ValidateCookPackageList(
		const FWBCharacterCookVerificationRequest& Request);
	WANDBOUNDEDITOR_API EWBCharacterGitStatus ClassifyRepositoryFile(
		const FString& ProjectRoot,
		const FString& RepositoryPath);
	WANDBOUNDEDITOR_API FString GitStatusName(EWBCharacterGitStatus Status);
	WANDBOUNDEDITOR_API FString ReimportStateName(EWBCharacterReimportState State);

	WANDBOUNDEDITOR_API bool LoadReceipt(
		const FString& AbsolutePath,
		FWBCharacterImportReceipt& OutReceipt);
	WANDBOUNDEDITOR_API FString ReceiptToJson(
		const FWBCharacterImportReceipt& Receipt);
	WANDBOUNDEDITOR_API FString InventoryToJson(
		const FWBCharacterSourceInventory& Inventory);
	WANDBOUNDEDITOR_API FString PresentationCandidateToJson(
		const FWBCharacterPresentationCandidate& Candidate);
	WANDBOUNDEDITOR_API FString BuildImportReportJson(
		const FWBCharacterPipelineRunResult& Result);
	WANDBOUNDEDITOR_API FString BuildImportReportMarkdown(
		const FWBCharacterPipelineRunResult& Result);

	WANDBOUNDEDITOR_API bool UpdateCatalogAtomic(
		const FString& CatalogAbsolutePath,
		const FWBCharacterPipelineRunResult& Result,
		FString& OutFailureReason);
	WANDBOUNDEDITOR_API bool ExecuteImportTasks(
		const FWBCharacterModelManifest& Manifest,
		const TArray<FWBCharacterImportTaskSpec>& Specs,
		TArray<FString>& OutImportedObjectPaths,
		TArray<FWBCharacterManifestDiagnostic>& OutDiagnostics);
	WANDBOUNDEDITOR_API FWBCharacterPipelineRunResult Run(
		const FString& ProjectRoot,
		const FWBCharacterPipelineRunOptions& Options);
}
