#include "WBRuntimePresentationAssetRegistry.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace
{
int32 CategorySpecificity(
	const EWBRuntimePresentationUnitCategory Candidate,
	const EWBRuntimePresentationUnitCategory Requested)
{
	if (Candidate == Requested)
	{
		return 300;
	}
	if (Requested == EWBRuntimePresentationUnitCategory::PlayerHero
		&& Candidate == EWBRuntimePresentationUnitCategory::PlayerUnit)
	{
		return 250;
	}
	return Candidate == EWBRuntimePresentationUnitCategory::Any ? 100 : INDEX_NONE;
}

FString EventBindingStableKey(const FWBRuntimePresentationAssetBinding& Binding)
{
	return FString::Printf(
		TEXT("%s|%03d|%s|%s|%s|%s|%s|%s|%s|%s"),
		*Binding.PublicDefinitionId,
		static_cast<int32>(Binding.UnitCategory),
		*Binding.AnimationMontage.ToSoftObjectPath().ToString(),
		*Binding.AnimationSequence.ToSoftObjectPath().ToString(),
		*Binding.NiagaraSystem.ToSoftObjectPath().ToString(),
		*Binding.Sound.ToSoftObjectPath().ToString(),
		*Binding.CameraShake.ToSoftObjectPath().ToString(),
		*Binding.SkeletalMeshOverride.ToSoftObjectPath().ToString(),
		*Binding.StaticMeshOverride.ToSoftObjectPath().ToString(),
		*Binding.MaterialOverride.ToSoftObjectPath().ToString());
}

FString UnitProfileStableKey(const FWBRuntimeUnitAssetProfile& Profile)
{
	return FString::Printf(
		TEXT("%s|%03d|%s|%s|%s"),
		*Profile.PublicDefinitionId,
		static_cast<int32>(Profile.UnitCategory),
		*Profile.SkeletalMesh.ToSoftObjectPath().ToString(),
		*Profile.StaticMesh.ToSoftObjectPath().ToString(),
		*Profile.MaterialOverride.ToSoftObjectPath().ToString());
}

bool IsDefinitionPublicForEvent(const EWBRuntimePresentationEventType Type)
{
	return Type == EWBRuntimePresentationEventType::UnitSummoned
		|| Type == EWBRuntimePresentationEventType::NPCSpawned;
}

bool IsRevealedMarkerEvent(const EWBRuntimePresentationEventType Type)
{
	return Type == EWBRuntimePresentationEventType::MarkerRevealed
		|| Type == EWBRuntimePresentationEventType::TrapTriggered;
}
}

void UWBRuntimePresentationAssetRegistry::Configure(
	UWBRuntimePresentationAssetSet* InAssetSet,
	const bool bEnableAssetLoading)
{
	if (AssetSet != InAssetSet)
	{
		LoadedAssetCache.Reset();
	}
	AssetSet = InAssetSet;
	bAssetLoadingEnabled = bEnableAssetLoading;
	LastDiagnostic.Reset();
}

void UWBRuntimePresentationAssetRegistry::BeginMatchGeneration(const int32 InMatchGeneration)
{
	if (MatchGeneration != InMatchGeneration)
	{
		LoadedAssetCache.Reset();
	}
	MatchGeneration = InMatchGeneration;
	LastDiagnostic.Reset();
}

void UWBRuntimePresentationAssetRegistry::Clear()
{
	LoadedAssetCache.Reset();
	MatchGeneration = INDEX_NONE;
	LastDiagnostic.Reset();
}

FWBRuntimePresentationBindingResolution
UWBRuntimePresentationAssetRegistry::ResolveEventBinding(
	const FWBRuntimePresentationBindingContext& Context) const
{
	FWBRuntimePresentationBindingResolution Result;
	if (AssetSet == nullptr)
	{
		return Result;
	}

	const FString SafeDefinitionId =
		Context.bPublicDefinitionIdAllowed ? Context.PublicDefinitionId : FString();
	const FWBRuntimePresentationAssetBinding* Best = nullptr;
	int32 BestSpecificity = INDEX_NONE;
	int32 BestPriority = MIN_int32;
	FString BestKey;
	for (const FWBRuntimePresentationAssetBinding& Candidate : AssetSet->EventBindings)
	{
		if (Candidate.EventType != Context.EventType)
		{
			continue;
		}

		int32 Specificity = INDEX_NONE;
		if (!Candidate.PublicDefinitionId.IsEmpty())
		{
			if (!SafeDefinitionId.IsEmpty()
				&& Candidate.PublicDefinitionId == SafeDefinitionId)
			{
				Specificity = 400;
			}
		}
		else
		{
			Specificity = CategorySpecificity(Candidate.UnitCategory, Context.UnitCategory);
		}
		if (Specificity == INDEX_NONE)
		{
			continue;
		}

		const FString StableKey = EventBindingStableKey(Candidate);
		const bool bBetter = Best == nullptr
			|| Specificity > BestSpecificity
			|| (Specificity == BestSpecificity && Candidate.StablePriority > BestPriority)
			|| (Specificity == BestSpecificity && Candidate.StablePriority == BestPriority
				&& StableKey < BestKey);
		if (bBetter)
		{
			Best = &Candidate;
			BestSpecificity = Specificity;
			BestPriority = Candidate.StablePriority;
			BestKey = StableKey;
		}
	}

	if (Best != nullptr)
	{
		Result.bFoundConfiguredBinding = true;
		Result.bUsePrimitiveFallback = false;
		Result.Specificity = BestSpecificity;
		Result.Reason = TEXT("presentation_asset_binding_resolved");
		Result.Binding = *Best;
	}
	return Result;
}

FWBRuntimeUnitProfileResolution UWBRuntimePresentationAssetRegistry::ResolveUnitProfile(
	const FWBRuntimeUnitPresentation& Unit) const
{
	FWBRuntimeUnitProfileResolution Result;
	if (AssetSet == nullptr)
	{
		return Result;
	}

	const EWBRuntimePresentationUnitCategory RequestedCategory = CategoryForUnit(Unit);
	const FWBRuntimeUnitAssetProfile* Best = nullptr;
	int32 BestSpecificity = INDEX_NONE;
	int32 BestPriority = MIN_int32;
	FString BestKey;
	for (const FWBRuntimeUnitAssetProfile& Candidate : AssetSet->UnitProfiles)
	{
		int32 Specificity = INDEX_NONE;
		if (!Candidate.PublicDefinitionId.IsEmpty())
		{
			if (!Unit.PublicDefinitionId.IsEmpty()
				&& Candidate.PublicDefinitionId == Unit.PublicDefinitionId)
			{
				Specificity = 400;
			}
		}
		else
		{
			Specificity = CategorySpecificity(Candidate.UnitCategory, RequestedCategory);
		}
		if (Specificity == INDEX_NONE)
		{
			continue;
		}

		const FString StableKey = UnitProfileStableKey(Candidate);
		const bool bBetter = Best == nullptr
			|| Specificity > BestSpecificity
			|| (Specificity == BestSpecificity && Candidate.StablePriority > BestPriority)
			|| (Specificity == BestSpecificity && Candidate.StablePriority == BestPriority
				&& StableKey < BestKey);
		if (bBetter)
		{
			Best = &Candidate;
			BestSpecificity = Specificity;
			BestPriority = Candidate.StablePriority;
			BestKey = StableKey;
		}
	}

	if (Best != nullptr)
	{
		Result.bFoundConfiguredProfile = true;
		Result.bUsePrimitiveFallback = false;
		Result.Specificity = BestSpecificity;
		Result.Reason = TEXT("unit_asset_profile_resolved");
		Result.Profile = *Best;
	}
	return Result;
}

FWBRuntimePresentationBindingContext
UWBRuntimePresentationAssetRegistry::BuildBindingContext(
	const FWBRuntimePresentationEvent& Event,
	const FWBRuntimeUnitPresentation* SourceUnit,
	const FWBRuntimeUnitPresentation* TargetUnit) const
{
	FWBRuntimePresentationBindingContext Context;
	Context.EventType = Event.Type;

	const FWBRuntimeUnitPresentation* RelevantUnit = SourceUnit;
	if (Event.Type == EWBRuntimePresentationEventType::AttackImpact
		|| Event.Type == EWBRuntimePresentationEventType::DamageApplied
		|| Event.Type == EWBRuntimePresentationEventType::HPChanged
		|| Event.Type == EWBRuntimePresentationEventType::ArmorChanged
		|| Event.Type == EWBRuntimePresentationEventType::TrapTriggered
		|| Event.Type == EWBRuntimePresentationEventType::UnitDefeated
		|| Event.Type == EWBRuntimePresentationEventType::HeroDefeated)
	{
		RelevantUnit = TargetUnit;
	}

	if (RelevantUnit != nullptr)
	{
		Context.UnitCategory = CategoryForUnit(*RelevantUnit);
		Context.PublicDefinitionId = RelevantUnit->PublicDefinitionId;
		Context.bPublicDefinitionIdAllowed = !Context.PublicDefinitionId.IsEmpty();
	}
	else if (Event.Type == EWBRuntimePresentationEventType::NPCSpawned
		|| Event.Type == EWBRuntimePresentationEventType::NPCAttacked
		|| Event.Type == EWBRuntimePresentationEventType::NPCMoved)
	{
		Context.UnitCategory = EWBRuntimePresentationUnitCategory::NeutralNPC;
	}
	else if (Event.Type == EWBRuntimePresentationEventType::UnitSummoned)
	{
		Context.UnitCategory = EWBRuntimePresentationUnitCategory::PlayerUnit;
	}

	if (IsDefinitionPublicForEvent(Event.Type) && !Event.PublicDefinitionId.IsEmpty())
	{
		Context.PublicDefinitionId = Event.PublicDefinitionId;
		Context.bPublicDefinitionIdAllowed = true;
	}

	if (Event.Type == EWBRuntimePresentationEventType::MarkerConsumed)
	{
		Context.UnitCategory = EWBRuntimePresentationUnitCategory::ConcealedMarker;
		Context.PublicDefinitionId.Reset();
		Context.bPublicDefinitionIdAllowed = false;
	}
	else if (IsRevealedMarkerEvent(Event.Type))
	{
		Context.PublicMarkerType = Event.PublicMarkerType;
		Context.PublicDefinitionId.Reset();
		Context.bPublicDefinitionIdAllowed = false;
		Context.UnitCategory = Event.PublicMarkerType == FName(TEXT("Trap"))
			? EWBRuntimePresentationUnitCategory::RevealedTrap
			: EWBRuntimePresentationUnitCategory::RevealedNPCMarker;
	}

	return Context;
}

UAnimMontage* UWBRuntimePresentationAssetRegistry::LoadMontage(
	const TSoftObjectPtr<UAnimMontage>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<UAnimMontage>(LoadObject(
		Asset.ToSoftObjectPath(),
		UAnimMontage::StaticClass(),
		ExpectedGeneration));
}

UAnimSequenceBase* UWBRuntimePresentationAssetRegistry::LoadAnimation(
	const TSoftObjectPtr<UAnimSequenceBase>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<UAnimSequenceBase>(LoadObject(
		Asset.ToSoftObjectPath(),
		UAnimSequenceBase::StaticClass(),
		ExpectedGeneration));
}

UNiagaraSystem* UWBRuntimePresentationAssetRegistry::LoadNiagaraSystem(
	const TSoftObjectPtr<UNiagaraSystem>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<UNiagaraSystem>(LoadObject(
		Asset.ToSoftObjectPath(),
		UNiagaraSystem::StaticClass(),
		ExpectedGeneration));
}

USoundBase* UWBRuntimePresentationAssetRegistry::LoadSound(
	const TSoftObjectPtr<USoundBase>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<USoundBase>(LoadObject(
		Asset.ToSoftObjectPath(),
		USoundBase::StaticClass(),
		ExpectedGeneration));
}

TSubclassOf<UCameraShakeBase> UWBRuntimePresentationAssetRegistry::LoadCameraShake(
	const TSoftClassPtr<UCameraShakeBase>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<UClass>(LoadObject(
		Asset.ToSoftObjectPath(),
		UClass::StaticClass(),
		ExpectedGeneration));
}

USkeletalMesh* UWBRuntimePresentationAssetRegistry::LoadSkeletalMesh(
	const TSoftObjectPtr<USkeletalMesh>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<USkeletalMesh>(LoadObject(
		Asset.ToSoftObjectPath(),
		USkeletalMesh::StaticClass(),
		ExpectedGeneration));
}

UStaticMesh* UWBRuntimePresentationAssetRegistry::LoadStaticMesh(
	const TSoftObjectPtr<UStaticMesh>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<UStaticMesh>(LoadObject(
		Asset.ToSoftObjectPath(),
		UStaticMesh::StaticClass(),
		ExpectedGeneration));
}

UMaterialInterface* UWBRuntimePresentationAssetRegistry::LoadMaterial(
	const TSoftObjectPtr<UMaterialInterface>& Asset,
	const int32 ExpectedGeneration)
{
	return Cast<UMaterialInterface>(LoadObject(
		Asset.ToSoftObjectPath(),
		UMaterialInterface::StaticClass(),
		ExpectedGeneration));
}

bool UWBRuntimePresentationAssetRegistry::IsAssetLoadingEnabled() const
{
	return bAssetLoadingEnabled;
}

int32 UWBRuntimePresentationAssetRegistry::GetMatchGeneration() const
{
	return MatchGeneration;
}

FString UWBRuntimePresentationAssetRegistry::GetLastDiagnostic() const
{
	return LastDiagnostic;
}

UObject* UWBRuntimePresentationAssetRegistry::LoadObject(
	const FSoftObjectPath& Path,
	UClass* ExpectedClass,
	const int32 ExpectedGeneration)
{
	LastDiagnostic.Reset();
	if (!bAssetLoadingEnabled || !Path.IsValid())
	{
		return nullptr;
	}
	if (ExpectedGeneration != MatchGeneration)
	{
		LastDiagnostic = TEXT("stale_asset_generation");
		return nullptr;
	}
	if (TObjectPtr<UObject>* Cached = LoadedAssetCache.Find(Path))
	{
		return Cached->Get() != nullptr && Cached->Get()->IsA(ExpectedClass)
			? Cached->Get()
			: nullptr;
	}

	UObject* Loaded = Path.ResolveObject();
	if (Loaded == nullptr)
	{
		Loaded = Path.TryLoad();
	}
	if (Loaded == nullptr || !Loaded->IsA(ExpectedClass))
	{
		LastDiagnostic = TEXT("presentation_asset_load_failed");
		return nullptr;
	}
	LoadedAssetCache.Add(Path, Loaded);
	return Loaded;
}

EWBRuntimePresentationUnitCategory
UWBRuntimePresentationAssetRegistry::CategoryForUnit(
	const FWBRuntimeUnitPresentation& Unit)
{
	if (Unit.bNeutralNPC)
	{
		return EWBRuntimePresentationUnitCategory::NeutralNPC;
	}
	return Unit.bHero
		? EWBRuntimePresentationUnitCategory::PlayerHero
		: EWBRuntimePresentationUnitCategory::PlayerUnit;
}
