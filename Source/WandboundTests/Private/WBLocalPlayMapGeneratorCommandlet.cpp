#include "WBLocalPlayMapGeneratorCommandlet.h"

#if WITH_EDITOR

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "WBRuntimeLocalPlayGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBLocalPlayMapGenerator, Log, All);

namespace
{
const FName GeneratedTag(TEXT("WandboundLocalPlayGenerated"));
const FName DirectionalLightName(TEXT("WBDev_DirectionalLight"));
const FName SkyLightName(TEXT("WBDev_SkyLight"));
const FName SkyAtmosphereName(TEXT("WBDev_SkyAtmosphere"));

template <typename TActor>
TActor* EnsureGeneratedActor(UWorld* World, const FName ActorName, bool& bOutChanged)
{
	TActor* Result = nullptr;
	for (TActorIterator<TActor> It(World); It; ++It)
	{
		TActor* Candidate = *It;
		if (!Candidate->Tags.Contains(GeneratedTag)) continue;
		if (Result == nullptr && Candidate->GetFName() == ActorName)
		{
			Result = Candidate;
			continue;
		}
		World->EditorDestroyActor(Candidate, true);
		bOutChanged = true;
	}
	if (Result == nullptr)
	{
		FActorSpawnParameters Params;
		Params.Name = ActorName;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Result = World->SpawnActor<TActor>(TActor::StaticClass(), FTransform::Identity, Params);
		if (Result != nullptr)
		{
			Result->Tags.Add(GeneratedTag);
			Result->SetActorLabel(ActorName.ToString());
			bOutChanged = true;
		}
	}
	return Result;
}

bool SetTransformIfDifferent(AActor* Actor, const FTransform& Transform)
{
	if (Actor == nullptr || Actor->GetActorTransform().Equals(Transform)) return false;
	Actor->SetActorTransform(Transform);
	return true;
}
}

const TCHAR* WBLocalPlayMapGenerator::GetMapPackagePath()
{
	return TEXT("/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev");
}

bool WBLocalPlayMapGenerator::Generate(FString& OutFailureReason, bool& bOutMapChanged)
{
	OutFailureReason.Reset();
	bOutMapChanged = false;
	FString ExistingFilename;
	const bool bMapExists = FPackageName::DoesPackageExist(GetMapPackagePath(), &ExistingFilename);
	UWorld* World = bMapExists
		? UEditorLoadingAndSavingUtils::LoadMap(ExistingFilename)
		: UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (World == nullptr)
	{
		OutFailureReason = bMapExists ? TEXT("development_map_load_failed") : TEXT("blank_map_creation_failed");
		return false;
	}
	bOutMapChanged = !bMapExists;

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (WorldSettings == nullptr)
	{
		OutFailureReason = TEXT("development_map_world_settings_missing");
		return false;
	}
	if (WorldSettings->DefaultGameMode != AWBRuntimeLocalPlayGameMode::StaticClass())
	{
		WorldSettings->Modify();
		WorldSettings->DefaultGameMode = AWBRuntimeLocalPlayGameMode::StaticClass();
		bOutMapChanged = true;
	}

	ADirectionalLight* DirectionalLight = EnsureGeneratedActor<ADirectionalLight>(World, DirectionalLightName, bOutMapChanged);
	ASkyLight* SkyLight = EnsureGeneratedActor<ASkyLight>(World, SkyLightName, bOutMapChanged);
	ASkyAtmosphere* SkyAtmosphere = EnsureGeneratedActor<ASkyAtmosphere>(World, SkyAtmosphereName, bOutMapChanged);
	if (DirectionalLight == nullptr || SkyLight == nullptr || SkyAtmosphere == nullptr)
	{
		OutFailureReason = TEXT("development_environment_spawn_failed");
		return false;
	}

	bOutMapChanged |= SetTransformIfDifferent(
		DirectionalLight,
		FTransform(FRotator(-45.0f, -35.0f, 0.0f), FVector(0.0f, 0.0f, 800.0f)));
	if (!FMath::IsNearlyEqual(DirectionalLight->GetLightComponent()->Intensity, 5.0f))
	{
		DirectionalLight->GetLightComponent()->SetIntensity(5.0f);
		bOutMapChanged = true;
	}
	if (DirectionalLight->GetLightComponent()->GetMobility() != EComponentMobility::Movable)
	{
		DirectionalLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		bOutMapChanged = true;
	}
	if (!FMath::IsNearlyEqual(SkyLight->GetLightComponent()->Intensity, 1.0f))
	{
		SkyLight->GetLightComponent()->SetIntensity(1.0f);
		bOutMapChanged = true;
	}
	if (SkyLight->GetLightComponent()->GetMobility() != EComponentMobility::Movable)
	{
		SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		bOutMapChanged = true;
	}

	if (bOutMapChanged && !UEditorLoadingAndSavingUtils::SaveMap(World, GetMapPackagePath()))
	{
		OutFailureReason = TEXT("development_map_save_failed");
		return false;
	}
	return true;
}

UWandboundLocalPlayMapGeneratorCommandlet::UWandboundLocalPlayMapGeneratorCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UWandboundLocalPlayMapGeneratorCommandlet::Main(const FString& Params)
{
	FString FailureReason;
	bool bMapChanged = false;
	if (!WBLocalPlayMapGenerator::Generate(FailureReason, bMapChanged))
	{
		UE_LOG(LogWBLocalPlayMapGenerator, Error, TEXT("Wandbound local-play map generation failed: %s"), *FailureReason);
		return 1;
	}
	UE_LOG(
		LogWBLocalPlayMapGenerator,
		Display,
		TEXT("Wandbound local-play development map %s: %s"),
		bMapChanged ? TEXT("generated") : TEXT("already current"),
		WBLocalPlayMapGenerator::GetMapPackagePath());
	return 0;
}

#else

const TCHAR* WBLocalPlayMapGenerator::GetMapPackagePath()
{
	return TEXT("/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev");
}

bool WBLocalPlayMapGenerator::Generate(FString& OutFailureReason, bool& bOutMapChanged)
{
	OutFailureReason = TEXT("editor_only_map_generator");
	bOutMapChanged = false;
	return false;
}

UWandboundLocalPlayMapGeneratorCommandlet::UWandboundLocalPlayMapGeneratorCommandlet() = default;

int32 UWandboundLocalPlayMapGeneratorCommandlet::Main(const FString& Params)
{
	return 1;
}

#endif
