#include "WBStarterPresentationAssetValidator.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformProcess.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "WBRuntimePresentationAssetBinding.h"

namespace
{
const TCHAR* ApprovedStarterAssetRoot =
	TEXT("/Game/Wandbound/Presentation/Starter/");

FString EventName(const EWBRuntimePresentationEventType Type)
{
	return StaticEnum<EWBRuntimePresentationEventType>()->GetNameStringByValue(
		static_cast<int64>(Type));
}

FString CategoryName(const EWBRuntimePresentationUnitCategory Category)
{
	return StaticEnum<EWBRuntimePresentationUnitCategory>()->GetNameStringByValue(
		static_cast<int64>(Category));
}

void AddDiagnostic(
	FWBPresentationAssetValidationResult& Result,
	const EWBPresentationAssetValidationSeverity Severity,
	const FString& Code,
	const FString& AssetSetPackage,
	const int32 EntryIndex,
	const FString& EntryKind,
	const FString& PublicEventOrCategory,
	const FString& ReferencedPackagePath,
	const FString& RecommendedCorrection)
{
	FWBPresentationAssetValidationDiagnostic Diagnostic;
	Diagnostic.Severity = Severity;
	Diagnostic.Code = Code;
	Diagnostic.AssetSetPackage = AssetSetPackage;
	Diagnostic.EntryIndex = EntryIndex;
	Diagnostic.EntryKind = EntryKind;
	Diagnostic.PublicEventOrCategory = PublicEventOrCategory;
	Diagnostic.ReferencedPackagePath = ReferencedPackagePath;
	Diagnostic.RecommendedCorrection = RecommendedCorrection;
	Result.Diagnostics.Add(MoveTemp(Diagnostic));
}

bool RunGit(const FString& Arguments)
{
	int32 ReturnCode = INDEX_NONE;
	FString Output;
	FString Error;
	FPlatformProcess::ExecProcess(
		TEXT("git.exe"),
		*Arguments,
		&ReturnCode,
		&Output,
		&Error);
	return ReturnCode == 0;
}

bool IsPrivateDefinitionId(const FString& DefinitionId)
{
	const FString Lower = DefinitionId.ToLower();
	return Lower.Contains(TEXT("private"))
		|| Lower.Contains(TEXT("instance"))
		|| Lower.Contains(TEXT("hidden"));
}

EWBPresentationAssetDependencyStatus ValidateReference(
	FWBPresentationAssetValidationResult& Result,
	const FString& AssetSetPackage,
	const FSoftObjectPath& Path,
	UClass* ExpectedClass,
	const int32 EntryIndex,
	const FString& EntryKind,
	const FString& PublicEventOrCategory,
	const bool bValidateGitTracking)
{
	if (!Path.IsValid())
	{
		return EWBPresentationAssetDependencyStatus::MissingAsset;
	}

	const FString PackagePath = Path.GetLongPackageName();
	if (PackagePath.StartsWith(TEXT("/Game/MeshyImports"))
		|| PackagePath.StartsWith(TEXT("/Game/Plugins/meshy"))
		|| PackagePath.Contains(TEXT("Meshy")))
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("untracked_meshy_dependency"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Remove the Meshy reference or commit an approved replacement first."));
		return EWBPresentationAssetDependencyStatus::UntrackedProjectAsset;
	}

	if (PackagePath.StartsWith(TEXT("/Engine/")))
	{
		FString EngineFilename;
		if (!FPackageName::DoesPackageExist(
			PackagePath,
			&EngineFilename))
		{
			AddDiagnostic(
				Result,
				EWBPresentationAssetValidationSeverity::Error,
				TEXT("missing_soft_reference"),
				AssetSetPackage,
				EntryIndex,
				EntryKind,
				PublicEventOrCategory,
				PackagePath,
				TEXT("Select an existing cookable engine asset or clear the optional hook."));
			return EWBPresentationAssetDependencyStatus::MissingAsset;
		}
		UObject* Loaded = Path.TryLoad();
		if (Loaded == nullptr)
		{
			AddDiagnostic(
				Result,
				EWBPresentationAssetValidationSeverity::Error,
				TEXT("missing_soft_reference"),
				AssetSetPackage,
				EntryIndex,
				EntryKind,
				PublicEventOrCategory,
				PackagePath,
				TEXT("Select an existing cookable engine asset or clear the optional hook."));
			return EWBPresentationAssetDependencyStatus::MissingAsset;
		}
		if (!Loaded->IsA(ExpectedClass))
		{
			AddDiagnostic(
				Result,
				EWBPresentationAssetValidationSeverity::Error,
				TEXT("invalid_asset_class"),
				AssetSetPackage,
				EntryIndex,
				EntryKind,
				PublicEventOrCategory,
				PackagePath,
				TEXT("Assign an asset of the declared binding type."));
			return EWBPresentationAssetDependencyStatus::InvalidAssetClass;
		}
		if (Loaded->IsEditorOnly())
		{
			AddDiagnostic(
				Result,
				EWBPresentationAssetValidationSeverity::Error,
				TEXT("editor_only_asset_reference"),
				AssetSetPackage,
				EntryIndex,
				EntryKind,
				PublicEventOrCategory,
				PackagePath,
				TEXT("Use a runtime-cookable asset."));
			return EWBPresentationAssetDependencyStatus::EditorOnlyAsset;
		}
		return EWBPresentationAssetDependencyStatus::EngineProvided;
	}

	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("asset_outside_approved_roots"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Use an engine asset or a tracked starter asset under the approved project path."));
		return EWBPresentationAssetDependencyStatus::MissingAsset;
	}

	FString Filename;
	if (!FPackageName::DoesPackageExist(PackagePath, &Filename))
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("missing_soft_reference"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Create and track the referenced project asset or clear the optional hook."));
		return EWBPresentationAssetDependencyStatus::MissingAsset;
	}

	if (!PackagePath.StartsWith(ApprovedStarterAssetRoot))
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("asset_outside_approved_directories"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Move task-generated dependencies under /Game/Wandbound/Presentation/Starter/."));
	}

	if (bValidateGitTracking)
	{
		const EWBPresentationAssetDependencyStatus GitStatus =
			WBStarterPresentationAssetValidator::ClassifyProjectFile(Filename);
		if (GitStatus == EWBPresentationAssetDependencyStatus::IgnoredProjectAsset)
		{
			AddDiagnostic(
				Result,
				EWBPresentationAssetValidationSeverity::Error,
				TEXT("ignored_project_dependency"),
				AssetSetPackage,
				EntryIndex,
				EntryKind,
				PublicEventOrCategory,
				PackagePath,
				TEXT("Use a tracked dependency that is not ignored."));
			return GitStatus;
		}
		if (GitStatus == EWBPresentationAssetDependencyStatus::UntrackedProjectAsset)
		{
			AddDiagnostic(
				Result,
				EWBPresentationAssetValidationSeverity::Error,
				TEXT("untracked_project_dependency"),
				AssetSetPackage,
				EntryIndex,
				EntryKind,
				PublicEventOrCategory,
				PackagePath,
				TEXT("Track the approved dependency through the project binary-asset policy."));
			return GitStatus;
		}
	}

	UObject* Loaded = Path.TryLoad();
	if (Loaded == nullptr)
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("uncookable_reference"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Repair or replace the project asset."));
		return EWBPresentationAssetDependencyStatus::MissingAsset;
	}
	if (!Loaded->IsA(ExpectedClass))
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("invalid_asset_class"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Assign an asset of the declared binding type."));
		return EWBPresentationAssetDependencyStatus::InvalidAssetClass;
	}
	if (Loaded->IsEditorOnly())
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("editor_only_asset_reference"),
			AssetSetPackage,
			EntryIndex,
			EntryKind,
			PublicEventOrCategory,
			PackagePath,
			TEXT("Use a runtime-cookable asset."));
		return EWBPresentationAssetDependencyStatus::EditorOnlyAsset;
	}
	return EWBPresentationAssetDependencyStatus::TrackedProjectAsset;
}

void ValidateOptionalReference(
	FWBPresentationAssetValidationResult& Result,
	const FString& AssetSetPackage,
	const FSoftObjectPath& Path,
	UClass* ExpectedClass,
	const int32 EntryIndex,
	const FString& EntryKind,
	const FString& PublicEventOrCategory,
	const bool bValidateGitTracking)
{
	if (Path.IsNull())
	{
		return;
	}
	ValidateReference(
		Result,
		AssetSetPackage,
		Path,
		ExpectedClass,
		EntryIndex,
		EntryKind,
		PublicEventOrCategory,
		bValidateGitTracking);
}

FString ProfileSignature(const FWBRuntimeUnitAssetProfile& Profile)
{
	return FString::Printf(
		TEXT("P|%03d|%s|%d|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s"),
		static_cast<int32>(Profile.UnitCategory),
		*Profile.PublicDefinitionId,
		Profile.StablePriority,
		*Profile.SkeletalMesh.ToSoftObjectPath().ToString(),
		*Profile.StaticMesh.ToSoftObjectPath().ToString(),
		*Profile.MaterialOverride.ToSoftObjectPath().ToString(),
		*Profile.IdleAnimation.ToSoftObjectPath().ToString(),
		*Profile.MoveAnimation.ToSoftObjectPath().ToString(),
		*Profile.AttackAnimation.ToSoftObjectPath().ToString(),
		*Profile.HitAnimation.ToSoftObjectPath().ToString(),
		*Profile.DeathAnimation.ToSoftObjectPath().ToString(),
		*Profile.SummonAnimation.ToSoftObjectPath().ToString(),
		*Profile.VisualScale.ToString(),
		*Profile.LocationOffset.ToString());
}

FString BindingSignature(const FWBRuntimePresentationAssetBinding& Binding)
{
	return FString::Printf(
		TEXT("B|%03d|%03d|%s|%d|%s|%s|%s|%s|%s|%s|%s|%.4f|%.4f|%s|%s"),
		static_cast<int32>(Binding.EventType),
		static_cast<int32>(Binding.UnitCategory),
		*Binding.PublicDefinitionId,
		Binding.StablePriority,
		*Binding.AnimationMontage.ToSoftObjectPath().ToString(),
		*Binding.AnimationSequence.ToSoftObjectPath().ToString(),
		*Binding.NiagaraSystem.ToSoftObjectPath().ToString(),
		*Binding.Sound.ToSoftObjectPath().ToString(),
		*Binding.CameraShake.ToSoftObjectPath().ToString(),
		*Binding.SkeletalMeshOverride.ToSoftObjectPath().ToString(),
		*Binding.StaticMeshOverride.ToSoftObjectPath().ToString(),
		Binding.PresentationDurationSeconds,
		Binding.PlaybackRate,
		*Binding.Scale.ToString(),
		*Binding.LocationOffset.ToString());
}
}

bool FWBPresentationAssetValidationResult::IsValid() const
{
	return ErrorCount() == 0;
}

bool FWBPresentationAssetValidationResult::ContainsCode(const FString& Code) const
{
	return Diagnostics.ContainsByPredicate([&Code](
		const FWBPresentationAssetValidationDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == Code;
	});
}

int32 FWBPresentationAssetValidationResult::ErrorCount() const
{
	return Diagnostics.FilterByPredicate([](
		const FWBPresentationAssetValidationDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EWBPresentationAssetValidationSeverity::Error;
	}).Num();
}

FWBPresentationAssetValidationResult WBStarterPresentationAssetValidator::Validate(
	const UWBRuntimePresentationAssetSet* AssetSet,
	const bool bValidateGitTracking)
{
	FWBPresentationAssetValidationResult Result;
	if (AssetSet == nullptr)
	{
		AddDiagnostic(
			Result,
			EWBPresentationAssetValidationSeverity::Error,
			TEXT("presentation_asset_set_missing"),
			FString(),
			INDEX_NONE,
			TEXT("asset_set"),
			FString(),
			FString(),
			TEXT("Create or load a UWBRuntimePresentationAssetSet."));
		return Result;
	}

	const FString AssetSetPackage = AssetSet->GetOutermost()->GetName();
	TSet<FString> BindingKeys;
	TSet<int32> BindingPriorities;
	for (int32 Index = 0; Index < AssetSet->EventBindings.Num(); ++Index)
	{
		const FWBRuntimePresentationAssetBinding& Binding =
			AssetSet->EventBindings[Index];
		const FString PublicEvent = EventName(Binding.EventType);
		if (!StaticEnum<EWBRuntimePresentationEventType>()->IsValidEnumValue(
			static_cast<int64>(Binding.EventType)))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("invalid_event_type"), AssetSetPackage, Index,
				TEXT("binding"), PublicEvent, FString(),
				TEXT("Select a valid presentation event type."));
		}
		const FString Key = FString::Printf(
			TEXT("%d|%d|%s"),
			static_cast<int32>(Binding.EventType),
			static_cast<int32>(Binding.UnitCategory),
			*Binding.PublicDefinitionId);
		if (BindingKeys.Contains(Key))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("duplicate_exact_binding"), AssetSetPackage, Index,
				TEXT("binding"), PublicEvent, FString(),
				TEXT("Keep one binding for each event/category/definition tuple."));
		}
		BindingKeys.Add(Key);
		if (BindingPriorities.Contains(Binding.StablePriority))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("duplicate_stable_priority"), AssetSetPackage, Index,
				TEXT("binding"), PublicEvent, FString(),
				TEXT("Assign a unique normalized stable priority."));
		}
		BindingPriorities.Add(Binding.StablePriority);
		if (!Binding.PublicDefinitionId.IsEmpty())
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				IsPrivateDefinitionId(Binding.PublicDefinitionId)
					? TEXT("private_definition_id_not_allowed")
					: TEXT("definition_specific_binding_not_starter_safe"),
				AssetSetPackage, Index, TEXT("binding"), PublicEvent,
				FString(), TEXT("Use a public category-level starter binding."));
		}
		if (Binding.EventType == EWBRuntimePresentationEventType::MarkerConsumed
			&& Binding.UnitCategory != EWBRuntimePresentationUnitCategory::ConcealedMarker
			&& Binding.UnitCategory != EWBRuntimePresentationUnitCategory::Any)
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("concealed_marker_type_specific_binding"), AssetSetPackage,
				Index, TEXT("binding"), PublicEvent, FString(),
				TEXT("Use the shared ConcealedMarker category before reveal."));
		}

		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.AnimationMontage.ToSoftObjectPath(),
			UAnimMontage::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.AnimationSequence.ToSoftObjectPath(),
			UAnimSequenceBase::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.NiagaraSystem.ToSoftObjectPath(),
			UNiagaraSystem::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.Sound.ToSoftObjectPath(),
			USoundBase::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.SkeletalMeshOverride.ToSoftObjectPath(),
			USkeletalMesh::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.StaticMeshOverride.ToSoftObjectPath(),
			UStaticMesh::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Binding.MaterialOverride.ToSoftObjectPath(),
			UMaterialInterface::StaticClass(), Index, TEXT("binding"), PublicEvent,
			bValidateGitTracking);
	}

	TSet<EWBRuntimePresentationUnitCategory> ProfileCategories;
	TSet<int32> ProfilePriorities;
	for (int32 Index = 0; Index < AssetSet->UnitProfiles.Num(); ++Index)
	{
		const FWBRuntimeUnitAssetProfile& Profile = AssetSet->UnitProfiles[Index];
		const FString PublicCategory = CategoryName(Profile.UnitCategory);
		if (Profile.UnitCategory == EWBRuntimePresentationUnitCategory::Any)
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("malformed_profile_category"), AssetSetPackage, Index,
				TEXT("profile"), PublicCategory, FString(),
				TEXT("Use an explicit public presentation category."));
		}
		if (ProfileCategories.Contains(Profile.UnitCategory))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("duplicate_profile_category"), AssetSetPackage, Index,
				TEXT("profile"), PublicCategory, FString(),
				TEXT("Keep one generated profile per starter category."));
		}
		ProfileCategories.Add(Profile.UnitCategory);
		if (ProfilePriorities.Contains(Profile.StablePriority))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("duplicate_profile_priority"), AssetSetPackage, Index,
				TEXT("profile"), PublicCategory, FString(),
				TEXT("Assign a unique normalized stable priority."));
		}
		ProfilePriorities.Add(Profile.StablePriority);
		if (!Profile.PublicDefinitionId.IsEmpty())
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				IsPrivateDefinitionId(Profile.PublicDefinitionId)
					? TEXT("private_definition_id_not_allowed")
					: TEXT("definition_specific_profile_not_starter_safe"),
				AssetSetPackage, Index, TEXT("profile"), PublicCategory,
				FString(), TEXT("Use a public category-level starter profile."));
		}
		if (Profile.SkeletalMesh.IsNull() && Profile.StaticMesh.IsNull())
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("required_profile_mesh_missing"), AssetSetPackage, Index,
				TEXT("profile"), PublicCategory, FString(),
				TEXT("Assign a cookable skeletal or static starter mesh."));
		}
		ValidateOptionalReference(Result, AssetSetPackage,
			Profile.SkeletalMesh.ToSoftObjectPath(), USkeletalMesh::StaticClass(),
			Index, TEXT("profile"), PublicCategory, bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Profile.StaticMesh.ToSoftObjectPath(), UStaticMesh::StaticClass(),
			Index, TEXT("profile"), PublicCategory, bValidateGitTracking);
		ValidateOptionalReference(Result, AssetSetPackage,
			Profile.MaterialOverride.ToSoftObjectPath(),
			UMaterialInterface::StaticClass(), Index, TEXT("profile"),
			PublicCategory, bValidateGitTracking);

		USkeletalMesh* SkeletalMesh = Profile.SkeletalMesh.IsNull()
			? nullptr
			: Profile.SkeletalMesh.LoadSynchronous();
		for (const TSoftObjectPtr<UAnimSequenceBase>* Animation : {
			&Profile.IdleAnimation,
			&Profile.MoveAnimation,
			&Profile.AttackAnimation,
			&Profile.HitAnimation,
			&Profile.DeathAnimation,
			&Profile.SummonAnimation })
		{
			ValidateOptionalReference(Result, AssetSetPackage,
				Animation->ToSoftObjectPath(), UAnimSequenceBase::StaticClass(),
				Index, TEXT("profile"), PublicCategory, bValidateGitTracking);
			if (SkeletalMesh != nullptr && !Animation->IsNull())
			{
				if (UAnimSequenceBase* LoadedAnimation =
					Animation->LoadSynchronous())
				{
					if (LoadedAnimation->GetSkeleton()
						!= SkeletalMesh->GetSkeleton())
					{
						AddDiagnostic(Result,
							EWBPresentationAssetValidationSeverity::Error,
							TEXT("animation_skeleton_incompatible"),
							AssetSetPackage, Index, TEXT("profile"),
							PublicCategory,
							Animation->ToSoftObjectPath().GetLongPackageName(),
							TEXT("Retarget the animation or preserve the transform fallback."));
					}
				}
			}
		}
	}

	for (const EWBRuntimePresentationUnitCategory RequiredCategory : {
		EWBRuntimePresentationUnitCategory::PlayerHero,
		EWBRuntimePresentationUnitCategory::PlayerUnit,
		EWBRuntimePresentationUnitCategory::NeutralNPC,
		EWBRuntimePresentationUnitCategory::ConcealedMarker,
		EWBRuntimePresentationUnitCategory::RevealedTrap,
		EWBRuntimePresentationUnitCategory::RevealedNPCMarker })
	{
		if (!ProfileCategories.Contains(RequiredCategory))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("required_profile_category_missing"), AssetSetPackage,
				INDEX_NONE, TEXT("profile"), CategoryName(RequiredCategory),
				FString(), TEXT("Generate the required starter category profile."));
		}
	}

	for (const EWBRuntimePresentationEventType RequiredEvent : {
		EWBRuntimePresentationEventType::UnitMoved,
		EWBRuntimePresentationEventType::NPCMoved,
		EWBRuntimePresentationEventType::AttackDeclared,
		EWBRuntimePresentationEventType::NPCAttacked,
		EWBRuntimePresentationEventType::AttackImpact,
		EWBRuntimePresentationEventType::DamageApplied,
		EWBRuntimePresentationEventType::ArmorChanged,
		EWBRuntimePresentationEventType::UnitSummoned,
		EWBRuntimePresentationEventType::NPCSpawned,
		EWBRuntimePresentationEventType::WandEquipped,
		EWBRuntimePresentationEventType::ActivationResolved,
		EWBRuntimePresentationEventType::MarkerRevealed,
		EWBRuntimePresentationEventType::MarkerConsumed,
		EWBRuntimePresentationEventType::TrapTriggered,
		EWBRuntimePresentationEventType::UnitDefeated,
		EWBRuntimePresentationEventType::HeroDefeated,
		EWBRuntimePresentationEventType::TurnStarted,
		EWBRuntimePresentationEventType::TurnEnded,
		EWBRuntimePresentationEventType::GameOver })
	{
		if (!AssetSet->EventBindings.ContainsByPredicate([RequiredEvent](
			const FWBRuntimePresentationAssetBinding& Binding)
		{
			return Binding.EventType == RequiredEvent;
		}))
		{
			AddDiagnostic(Result, EWBPresentationAssetValidationSeverity::Error,
				TEXT("required_event_binding_missing"), AssetSetPackage,
				INDEX_NONE, TEXT("binding"), EventName(RequiredEvent),
				FString(), TEXT("Generate the required starter event binding."));
		}
	}

	return Result;
}

EWBPresentationAssetDependencyStatus
WBStarterPresentationAssetValidator::ClassifyProjectFile(
	const FString& AbsoluteFilename)
{
	if (!FPaths::FileExists(AbsoluteFilename))
	{
		return EWBPresentationAssetDependencyStatus::MissingAsset;
	}
	FString Relative = AbsoluteFilename;
	FPaths::MakePathRelativeTo(Relative, *FPaths::ProjectDir());
	Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir());
	const FString BaseArgs = FString::Printf(
		TEXT("-C \"%s\" "),
		*ProjectDir);
	if (RunGit(BaseArgs + FString::Printf(
		TEXT("ls-files --error-unmatch -- \"%s\""),
		*Relative)))
	{
		return EWBPresentationAssetDependencyStatus::TrackedProjectAsset;
	}
	if (RunGit(BaseArgs + FString::Printf(
		TEXT("check-ignore -q -- \"%s\""),
		*Relative)))
	{
		return EWBPresentationAssetDependencyStatus::IgnoredProjectAsset;
	}
	return EWBPresentationAssetDependencyStatus::UntrackedProjectAsset;
}

FString WBStarterPresentationAssetValidator::BuildNormalizedSignature(
	const UWBRuntimePresentationAssetSet* AssetSet)
{
	if (AssetSet == nullptr)
	{
		return TEXT("missing");
	}
	TArray<FString> Entries;
	for (const FWBRuntimeUnitAssetProfile& Profile : AssetSet->UnitProfiles)
	{
		Entries.Add(ProfileSignature(Profile));
	}
	for (const FWBRuntimePresentationAssetBinding& Binding :
		AssetSet->EventBindings)
	{
		Entries.Add(BindingSignature(Binding));
	}
	Entries.Sort();
	return FString::Join(Entries, TEXT("\n"));
}

FString WBStarterPresentationAssetValidator::DependencyStatusName(
	const EWBPresentationAssetDependencyStatus Status)
{
	switch (Status)
	{
	case EWBPresentationAssetDependencyStatus::EngineProvided:
		return TEXT("engine");
	case EWBPresentationAssetDependencyStatus::TrackedProjectAsset:
		return TEXT("tracked_project");
	case EWBPresentationAssetDependencyStatus::UntrackedProjectAsset:
		return TEXT("untracked_project");
	case EWBPresentationAssetDependencyStatus::IgnoredProjectAsset:
		return TEXT("ignored_project");
	case EWBPresentationAssetDependencyStatus::MissingAsset:
		return TEXT("missing");
	case EWBPresentationAssetDependencyStatus::EditorOnlyAsset:
		return TEXT("editor_only");
	case EWBPresentationAssetDependencyStatus::InvalidAssetClass:
		return TEXT("invalid_class");
	default:
		return TEXT("unknown");
	}
}
