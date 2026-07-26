#include "WBStarterPresentationAssetGeneratorCommandlet.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "WBStarterPresentationAssetValidator.h"
#include "WBRuntimePresentationAssetBinding.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBStarterPresentationAssetGenerator, Log, All);

namespace
{
TSoftObjectPtr<UStaticMesh> EngineMesh(const TCHAR* ObjectPath)
{
	return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ObjectPath));
}

FWBRuntimeUnitAssetProfile MakeProfile(
	const EWBRuntimePresentationUnitCategory Category,
	const int32 Priority,
	const TSoftObjectPtr<UStaticMesh>& StaticMesh,
	const FVector& Scale,
	const FVector& Offset = FVector::ZeroVector)
{
	FWBRuntimeUnitAssetProfile Profile;
	Profile.UnitCategory = Category;
	Profile.StablePriority = Priority;
	Profile.StaticMesh = StaticMesh;
	Profile.VisualScale = Scale;
	Profile.LocationOffset = Offset;
	return Profile;
}

FWBRuntimePresentationAssetBinding MakeBinding(
	const EWBRuntimePresentationEventType EventType,
	const EWBRuntimePresentationUnitCategory Category,
	const int32 Priority,
	const float Duration,
	const EWBRuntimePresentationAttachmentPolicy AttachmentPolicy =
		EWBRuntimePresentationAttachmentPolicy::TargetLocation,
	const FVector& Scale = FVector::OneVector)
{
	FWBRuntimePresentationAssetBinding Binding;
	Binding.EventType = EventType;
	Binding.UnitCategory = Category;
	Binding.StablePriority = Priority;
	Binding.PresentationDurationSeconds = Duration;
	Binding.AttachmentPolicy = AttachmentPolicy;
	Binding.Scale = Scale;
	Binding.bOptional = true;
	return Binding;
}

void BuildGeneratedProfiles(TArray<FWBRuntimeUnitAssetProfile>& OutProfiles)
{
	OutProfiles = {
		MakeProfile(
			EWBRuntimePresentationUnitCategory::PlayerHero,
			2000,
			EngineMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")),
			FVector(1.15f, 1.15f, 1.35f),
			FVector(0.0f, 0.0f, 58.0f)),
		MakeProfile(
			EWBRuntimePresentationUnitCategory::PlayerUnit,
			1990,
			EngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),
			FVector(0.78f, 0.78f, 1.0f),
			FVector(0.0f, 0.0f, 50.0f)),
		MakeProfile(
			EWBRuntimePresentationUnitCategory::NeutralNPC,
			1980,
			EngineMesh(TEXT("/Engine/BasicShapes/Cone.Cone")),
			FVector(0.85f, 0.85f, 1.05f),
			FVector(0.0f, 0.0f, 52.0f)),
		MakeProfile(
			EWBRuntimePresentationUnitCategory::ConcealedMarker,
			1970,
			EngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),
			FVector(0.42f, 0.42f, 0.18f),
			FVector(0.0f, 0.0f, 9.0f)),
		MakeProfile(
			EWBRuntimePresentationUnitCategory::RevealedTrap,
			1960,
			EngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube")),
			FVector(0.48f, 0.48f, 0.14f),
			FVector(0.0f, 0.0f, 7.0f)),
		MakeProfile(
			EWBRuntimePresentationUnitCategory::RevealedNPCMarker,
			1950,
			EngineMesh(TEXT("/Engine/BasicShapes/Cone.Cone")),
			FVector(0.42f, 0.42f, 0.55f),
			FVector(0.0f, 0.0f, 27.0f))
	};
}

void BuildGeneratedBindings(TArray<FWBRuntimePresentationAssetBinding>& OutBindings)
{
	int32 Priority = 1000;
	const auto Add = [&OutBindings, &Priority](
		const EWBRuntimePresentationEventType EventType,
		const EWBRuntimePresentationUnitCategory Category,
		const float Duration,
		const EWBRuntimePresentationAttachmentPolicy AttachmentPolicy =
			EWBRuntimePresentationAttachmentPolicy::TargetLocation,
		const FVector& Scale = FVector::OneVector)
	{
		OutBindings.Add(MakeBinding(
			EventType,
			Category,
			Priority--,
			Duration,
			AttachmentPolicy,
			Scale));
	};

	Add(EWBRuntimePresentationEventType::UnitMoved,
		EWBRuntimePresentationUnitCategory::PlayerUnit, 0.20f,
		EWBRuntimePresentationAttachmentPolicy::DestinationTile);
	Add(EWBRuntimePresentationEventType::NPCMoved,
		EWBRuntimePresentationUnitCategory::NeutralNPC, 0.20f,
		EWBRuntimePresentationAttachmentPolicy::DestinationTile);
	Add(EWBRuntimePresentationEventType::AttackDeclared,
		EWBRuntimePresentationUnitCategory::PlayerUnit, 0.18f,
		EWBRuntimePresentationAttachmentPolicy::AttachToSource);
	Add(EWBRuntimePresentationEventType::NPCAttacked,
		EWBRuntimePresentationUnitCategory::NeutralNPC, 0.18f,
		EWBRuntimePresentationAttachmentPolicy::AttachToSource);
	Add(EWBRuntimePresentationEventType::AttackImpact,
		EWBRuntimePresentationUnitCategory::Any, 0.12f);
	Add(EWBRuntimePresentationEventType::DamageApplied,
		EWBRuntimePresentationUnitCategory::Any, 0.12f);
	Add(EWBRuntimePresentationEventType::ArmorChanged,
		EWBRuntimePresentationUnitCategory::Any, 0.10f);
	Add(EWBRuntimePresentationEventType::UnitSummoned,
		EWBRuntimePresentationUnitCategory::PlayerUnit, 0.25f);
	Add(EWBRuntimePresentationEventType::NPCSpawned,
		EWBRuntimePresentationUnitCategory::NeutralNPC, 0.25f);
	Add(EWBRuntimePresentationEventType::WandEquipped,
		EWBRuntimePresentationUnitCategory::PlayerUnit, 0.16f,
		EWBRuntimePresentationAttachmentPolicy::AttachToSource);
	Add(EWBRuntimePresentationEventType::ActivationResolved,
		EWBRuntimePresentationUnitCategory::PlayerUnit, 0.18f,
		EWBRuntimePresentationAttachmentPolicy::AttachToSource);
	Add(EWBRuntimePresentationEventType::MarkerRevealed,
		EWBRuntimePresentationUnitCategory::RevealedTrap, 0.20f);
	Add(EWBRuntimePresentationEventType::MarkerRevealed,
		EWBRuntimePresentationUnitCategory::RevealedNPCMarker, 0.20f);
	Add(EWBRuntimePresentationEventType::MarkerConsumed,
		EWBRuntimePresentationUnitCategory::ConcealedMarker, 0.12f);
	Add(EWBRuntimePresentationEventType::TrapTriggered,
		EWBRuntimePresentationUnitCategory::RevealedTrap, 0.20f);
	Add(EWBRuntimePresentationEventType::UnitDefeated,
		EWBRuntimePresentationUnitCategory::Any, 0.25f);
	Add(EWBRuntimePresentationEventType::HeroDefeated,
		EWBRuntimePresentationUnitCategory::PlayerHero, 0.35f);
	Add(EWBRuntimePresentationEventType::TurnStarted,
		EWBRuntimePresentationUnitCategory::Any, 0.10f);
	Add(EWBRuntimePresentationEventType::TurnEnded,
		EWBRuntimePresentationUnitCategory::Any, 0.10f);
	Add(EWBRuntimePresentationEventType::GameOver,
		EWBRuntimePresentationUnitCategory::Any, 0.35f);
}

template <typename TEntry>
void PreserveManualEntries(
	const TArray<TEntry>& ExistingEntries,
	TArray<TEntry>& GeneratedEntries)
{
	for (const TEntry& Entry : ExistingEntries)
	{
		if (Entry.StablePriority >=
			WBStarterPresentationAssetGenerator::ManualOverridePriorityFloor)
		{
			GeneratedEntries.Add(Entry);
		}
	}
}
}

const TCHAR* WBStarterPresentationAssetGenerator::GetAssetPackagePath()
{
	return TEXT("/Game/Wandbound/Presentation/DA_WandboundStarterPresentation");
}

const TCHAR* WBStarterPresentationAssetGenerator::GetAssetObjectPath()
{
	return TEXT("/Game/Wandbound/Presentation/DA_WandboundStarterPresentation.DA_WandboundStarterPresentation");
}

FString WBStarterPresentationAssetGenerator::GetAssetFilename()
{
	return FPackageName::LongPackageNameToFilename(
		GetAssetPackagePath(),
		FPackageName::GetAssetPackageExtension());
}

UWBRuntimePresentationAssetSet*
WBStarterPresentationAssetGenerator::LoadGeneratedAsset()
{
	FString ExistingFilename;
	if (!FPackageName::DoesPackageExist(
		GetAssetPackagePath(),
		&ExistingFilename))
	{
		return nullptr;
	}
	return LoadObject<UWBRuntimePresentationAssetSet>(
		nullptr,
		GetAssetObjectPath());
}

bool WBStarterPresentationAssetGenerator::Generate(
	FString& OutFailureReason,
	bool& bOutAssetChanged,
	FString& OutNormalizedSignature)
{
	OutFailureReason.Reset();
	bOutAssetChanged = false;
	OutNormalizedSignature.Reset();

	UWBRuntimePresentationAssetSet* AssetSet = LoadGeneratedAsset();
	UPackage* Package = AssetSet != nullptr
		? AssetSet->GetOutermost()
		: CreatePackage(GetAssetPackagePath());
	if (Package == nullptr)
	{
		OutFailureReason = TEXT("starter_presentation_package_creation_failed");
		return false;
	}
	if (AssetSet == nullptr)
	{
		AssetSet = NewObject<UWBRuntimePresentationAssetSet>(
			Package,
			TEXT("DA_WandboundStarterPresentation"),
			RF_Public | RF_Standalone);
		if (AssetSet == nullptr)
		{
			OutFailureReason = TEXT("starter_presentation_asset_creation_failed");
			return false;
		}
		FAssetRegistryModule::AssetCreated(AssetSet);
		bOutAssetChanged = true;
	}

	TArray<FWBRuntimeUnitAssetProfile> GeneratedProfiles;
	TArray<FWBRuntimePresentationAssetBinding> GeneratedBindings;
	BuildGeneratedProfiles(GeneratedProfiles);
	BuildGeneratedBindings(GeneratedBindings);
	PreserveManualEntries(AssetSet->UnitProfiles, GeneratedProfiles);
	PreserveManualEntries(AssetSet->EventBindings, GeneratedBindings);

	UWBRuntimePresentationAssetSet* Expected =
		NewObject<UWBRuntimePresentationAssetSet>(GetTransientPackage());
	Expected->UnitProfiles = GeneratedProfiles;
	Expected->EventBindings = GeneratedBindings;
	const FString ExpectedSignature =
		WBStarterPresentationAssetValidator::BuildNormalizedSignature(Expected);
	const FString ExistingSignature =
		WBStarterPresentationAssetValidator::BuildNormalizedSignature(AssetSet);
	if (ExistingSignature != ExpectedSignature)
	{
		AssetSet->Modify();
		AssetSet->UnitProfiles = MoveTemp(GeneratedProfiles);
		AssetSet->EventBindings = MoveTemp(GeneratedBindings);
		AssetSet->MarkPackageDirty();
		bOutAssetChanged = true;
	}

	const FWBPresentationAssetValidationResult Validation =
		WBStarterPresentationAssetValidator::Validate(AssetSet, true);
	if (!Validation.IsValid())
	{
		OutFailureReason = FString::Printf(
			TEXT("starter_presentation_validation_failed:%d"),
			Validation.ErrorCount());
		return false;
	}

	if (bOutAssetChanged)
	{
		const FString Filename = GetAssetFilename();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(
			Package,
			AssetSet,
			*Filename,
			SaveArgs))
		{
			OutFailureReason = TEXT("starter_presentation_asset_save_failed");
			return false;
		}
	}

	OutNormalizedSignature =
		WBStarterPresentationAssetValidator::BuildNormalizedSignature(AssetSet);
	return true;
}

UWBStarterPresentationAssetGeneratorCommandlet::
UWBStarterPresentationAssetGeneratorCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UWBStarterPresentationAssetGeneratorCommandlet::Main(
	const FString& Params)
{
	FString FailureReason;
	FString Signature;
	bool bAssetChanged = false;
	if (!WBStarterPresentationAssetGenerator::Generate(
		FailureReason,
		bAssetChanged,
		Signature))
	{
		UE_LOG(
			LogWBStarterPresentationAssetGenerator,
			Error,
			TEXT("Starter presentation generation failed: %s"),
			*FailureReason);
		return 1;
	}

	UE_LOG(
		LogWBStarterPresentationAssetGenerator,
		Display,
		TEXT("Starter presentation asset %s: %s (signature chars=%d)"),
		bAssetChanged ? TEXT("generated") : TEXT("already current"),
		WBStarterPresentationAssetGenerator::GetAssetObjectPath(),
		Signature.Len());
	return 0;
}

#else

const TCHAR* WBStarterPresentationAssetGenerator::GetAssetPackagePath()
{
	return TEXT("/Game/Wandbound/Presentation/DA_WandboundStarterPresentation");
}

const TCHAR* WBStarterPresentationAssetGenerator::GetAssetObjectPath()
{
	return TEXT("/Game/Wandbound/Presentation/DA_WandboundStarterPresentation.DA_WandboundStarterPresentation");
}

FString WBStarterPresentationAssetGenerator::GetAssetFilename()
{
	return FString();
}

UWBRuntimePresentationAssetSet*
WBStarterPresentationAssetGenerator::LoadGeneratedAsset()
{
	return nullptr;
}

bool WBStarterPresentationAssetGenerator::Generate(
	FString& OutFailureReason,
	bool& bOutAssetChanged,
	FString& OutNormalizedSignature)
{
	OutFailureReason = TEXT("editor_only_starter_presentation_generator");
	bOutAssetChanged = false;
	OutNormalizedSignature.Reset();
	return false;
}

UWBStarterPresentationAssetGeneratorCommandlet::
UWBStarterPresentationAssetGeneratorCommandlet() = default;

int32 UWBStarterPresentationAssetGeneratorCommandlet::Main(
	const FString& Params)
{
	return 1;
}

#endif
