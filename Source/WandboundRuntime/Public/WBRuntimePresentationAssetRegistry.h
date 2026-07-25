#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WBRuntimeMatchPresentation.h"
#include "WBRuntimePresentationAssetBinding.h"
#include "WBRuntimePresentationAssetRegistry.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;

UCLASS()
class WANDBOUNDRUNTIME_API UWBRuntimePresentationAssetRegistry : public UObject
{
	GENERATED_BODY()

public:
	void Configure(UWBRuntimePresentationAssetSet* InAssetSet, bool bEnableAssetLoading);
	void BeginMatchGeneration(int32 MatchGeneration);
	void Clear();

	FWBRuntimePresentationBindingResolution ResolveEventBinding(
		const FWBRuntimePresentationBindingContext& Context) const;
	FWBRuntimeUnitProfileResolution ResolveUnitProfile(
		const FWBRuntimeUnitPresentation& Unit) const;

	FWBRuntimePresentationBindingContext BuildBindingContext(
		const FWBRuntimePresentationEvent& Event,
		const FWBRuntimeUnitPresentation* SourceUnit,
		const FWBRuntimeUnitPresentation* TargetUnit) const;

	UAnimMontage* LoadMontage(
		const TSoftObjectPtr<UAnimMontage>& Asset,
		int32 ExpectedGeneration);
	UAnimSequenceBase* LoadAnimation(
		const TSoftObjectPtr<UAnimSequenceBase>& Asset,
		int32 ExpectedGeneration);
	UNiagaraSystem* LoadNiagaraSystem(
		const TSoftObjectPtr<UNiagaraSystem>& Asset,
		int32 ExpectedGeneration);
	USoundBase* LoadSound(
		const TSoftObjectPtr<USoundBase>& Asset,
		int32 ExpectedGeneration);
	TSubclassOf<UCameraShakeBase> LoadCameraShake(
		const TSoftClassPtr<UCameraShakeBase>& Asset,
		int32 ExpectedGeneration);
	USkeletalMesh* LoadSkeletalMesh(
		const TSoftObjectPtr<USkeletalMesh>& Asset,
		int32 ExpectedGeneration);
	UStaticMesh* LoadStaticMesh(
		const TSoftObjectPtr<UStaticMesh>& Asset,
		int32 ExpectedGeneration);
	UMaterialInterface* LoadMaterial(
		const TSoftObjectPtr<UMaterialInterface>& Asset,
		int32 ExpectedGeneration);

	bool IsAssetLoadingEnabled() const;
	int32 GetMatchGeneration() const;
	FString GetLastDiagnostic() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimePresentationAssetSet> AssetSet;

	UPROPERTY(Transient)
	TMap<FSoftObjectPath, TObjectPtr<UObject>> LoadedAssetCache;

	int32 MatchGeneration = INDEX_NONE;
	bool bAssetLoadingEnabled = false;
	FString LastDiagnostic;

	UObject* LoadObject(
		const FSoftObjectPath& Path,
		UClass* ExpectedClass,
		int32 ExpectedGeneration);
	static EWBRuntimePresentationUnitCategory CategoryForUnit(
		const FWBRuntimeUnitPresentation& Unit);
};
