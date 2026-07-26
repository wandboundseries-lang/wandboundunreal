#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBStarterPresentationAssetGeneratorCommandlet.h"
#include "WBStarterPresentationAssetValidator.h"
#include "WBRuntimeMatchBootstrapActor.h"
#include "WBRuntimePresentationAssetRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimePresentationAssetSet* LoadStarterAsset()
{
	return WBStarterPresentationAssetGenerator::LoadGeneratedAsset();
}

UWBRuntimePresentationAssetSet* DuplicateStarterAsset()
{
	UWBRuntimePresentationAssetSet* Source = LoadStarterAsset();
	if (Source == nullptr)
	{
		return nullptr;
	}
	UWBRuntimePresentationAssetSet* Copy =
		NewObject<UWBRuntimePresentationAssetSet>(GetTransientPackage());
	Copy->EventBindings = Source->EventBindings;
	Copy->UnitProfiles = Source->UnitProfiles;
	return Copy;
}

bool HasCategory(
	const UWBRuntimePresentationAssetSet* AssetSet,
	const EWBRuntimePresentationUnitCategory Category)
{
	return AssetSet != nullptr
		&& AssetSet->UnitProfiles.ContainsByPredicate(
			[Category](const FWBRuntimeUnitAssetProfile& Profile)
			{
				return Profile.UnitCategory == Category;
			});
}

bool HasEvent(
	const UWBRuntimePresentationAssetSet* AssetSet,
	const EWBRuntimePresentationEventType EventType)
{
	return AssetSet != nullptr
		&& AssetSet->EventBindings.ContainsByPredicate(
			[EventType](
				const FWBRuntimePresentationAssetBinding& Binding)
			{
				return Binding.EventType == EventType;
			});
}

FWBRuntimeUnitPresentation MakeUnit(
	const bool bHero,
	const bool bNeutral)
{
	FWBRuntimeUnitPresentation Unit;
	Unit.UnitId = bNeutral ? 90 : bHero ? 10 : 20;
	Unit.OwnerId = bNeutral ? INDEX_NONE : 0;
	Unit.bHero = bHero;
	Unit.bNeutralNPC = bNeutral;
	return Unit;
}

bool LoadProjectText(const TCHAR* RelativePath, FString& OutText)
{
	return FFileHelper::LoadFileToString(
		OutText,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationCommandletClassTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Generation.CommandletClassLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationCommandletClassTest::RunTest(
	const FString& Parameters)
{
	TestNotNull(
		TEXT("Starter generator commandlet class exists"),
		UWBStarterPresentationAssetGeneratorCommandlet::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationExpectedPackageTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Generation.ExpectedPackageAndClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationExpectedPackageTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* AssetSet = LoadStarterAsset();
	TestNotNull(TEXT("Starter presentation asset loads"), AssetSet);
	if (AssetSet != nullptr)
	{
		TestEqual(
			TEXT("Expected package"),
			AssetSet->GetOutermost()->GetName(),
			FString(WBStarterPresentationAssetGenerator::GetAssetPackagePath()));
		TestTrue(
			TEXT("Expected asset class"),
			AssetSet->IsA<UWBRuntimePresentationAssetSet>());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationIdempotenceTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Generation.Idempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationIdempotenceTest::RunTest(
	const FString& Parameters)
{
	FString FailureReason;
	FString FirstSignature;
	FString SecondSignature;
	bool bFirstChanged = false;
	bool bSecondChanged = false;
	TestTrue(
		TEXT("First reconciliation succeeds"),
		WBStarterPresentationAssetGenerator::Generate(
			FailureReason,
			bFirstChanged,
			FirstSignature));
	TestTrue(
		TEXT("Second reconciliation succeeds"),
		WBStarterPresentationAssetGenerator::Generate(
			FailureReason,
			bSecondChanged,
			SecondSignature));
	TestFalse(TEXT("Second reconciliation is unchanged"), bSecondChanged);
	TestEqual(
		TEXT("Normalized state is stable"),
		SecondSignature,
		FirstSignature);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationProfilesTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Generation.RequiredProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationProfilesTest::RunTest(const FString& Parameters)
{
	const UWBRuntimePresentationAssetSet* AssetSet = LoadStarterAsset();
	TestNotNull(TEXT("Starter presentation asset loads"), AssetSet);
	if (AssetSet == nullptr) return false;
	TestEqual(TEXT("Six starter profiles"), AssetSet->UnitProfiles.Num(), 6);
	for (const EWBRuntimePresentationUnitCategory Category : {
		EWBRuntimePresentationUnitCategory::PlayerHero,
		EWBRuntimePresentationUnitCategory::PlayerUnit,
		EWBRuntimePresentationUnitCategory::NeutralNPC,
		EWBRuntimePresentationUnitCategory::ConcealedMarker,
		EWBRuntimePresentationUnitCategory::RevealedTrap,
		EWBRuntimePresentationUnitCategory::RevealedNPCMarker })
	{
		TestTrue(TEXT("Required category exists"), HasCategory(AssetSet, Category));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationBindingsTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Generation.RequiredBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationBindingsTest::RunTest(const FString& Parameters)
{
	const UWBRuntimePresentationAssetSet* AssetSet = LoadStarterAsset();
	TestNotNull(TEXT("Starter presentation asset loads"), AssetSet);
	if (AssetSet == nullptr) return false;
	TestEqual(TEXT("Twenty starter bindings"), AssetSet->EventBindings.Num(), 20);
	for (const EWBRuntimePresentationEventType EventType : {
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
		TestTrue(TEXT("Required event exists"), HasEvent(AssetSet, EventType));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationHiddenDataTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Generation.HiddenDataAbsent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationHiddenDataTest::RunTest(
	const FString& Parameters)
{
	const UWBRuntimePresentationAssetSet* AssetSet = LoadStarterAsset();
	TestNotNull(TEXT("Starter presentation asset loads"), AssetSet);
	if (AssetSet == nullptr) return false;
	for (const FWBRuntimeUnitAssetProfile& Profile : AssetSet->UnitProfiles)
	{
		TestTrue(
			TEXT("Profiles remain category-only"),
			Profile.PublicDefinitionId.IsEmpty());
	}
	for (const FWBRuntimePresentationAssetBinding& Binding :
		AssetSet->EventBindings)
	{
		TestTrue(
			TEXT("Bindings remain category-only"),
			Binding.PublicDefinitionId.IsEmpty());
	}
	const int32 ConcealedCount = AssetSet->UnitProfiles.FilterByPredicate(
		[](const FWBRuntimeUnitAssetProfile& Profile)
		{
			return Profile.UnitCategory ==
				EWBRuntimePresentationUnitCategory::ConcealedMarker;
		}).Num();
	TestEqual(TEXT("One type-neutral concealed profile"), ConcealedCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationValidValidationTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.ValidAssetPasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationValidValidationTest::RunTest(
	const FString& Parameters)
{
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(
			LoadStarterAsset(),
			true);
	TestTrue(TEXT("Starter asset validates"), Result.IsValid());
	TestEqual(TEXT("No validation errors"), Result.ErrorCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationDuplicateBindingTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.DuplicateExactBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationDuplicateBindingTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Copy = DuplicateStarterAsset();
	TestNotNull(TEXT("Starter copy created"), Copy);
	if (Copy == nullptr) return false;
	const FWBRuntimePresentationAssetBinding Duplicate =
		Copy->EventBindings[0];
	Copy->EventBindings.Add(Duplicate);
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(Copy, false);
	TestTrue(
		TEXT("Duplicate exact binding reported"),
		Result.ContainsCode(TEXT("duplicate_exact_binding")));
	TestTrue(
		TEXT("Duplicate stable priority reported"),
		Result.ContainsCode(TEXT("duplicate_stable_priority")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationInvalidClassTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.InvalidAssetClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationInvalidClassTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Copy = DuplicateStarterAsset();
	TestNotNull(TEXT("Starter copy created"), Copy);
	if (Copy == nullptr) return false;
	Copy->UnitProfiles[0].StaticMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")));
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(Copy, false);
	TestTrue(
		TEXT("Wrong class reported"),
		Result.ContainsCode(TEXT("invalid_asset_class")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationMissingReferenceTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.MissingSoftReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationMissingReferenceTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Copy = DuplicateStarterAsset();
	TestNotNull(TEXT("Starter copy created"), Copy);
	if (Copy == nullptr) return false;
	Copy->UnitProfiles[0].StaticMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Engine/BasicShapes/WB_Missing.WB_Missing")));
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(Copy, false);
	TestTrue(
		TEXT("Missing reference reported"),
		Result.ContainsCode(TEXT("missing_soft_reference")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationMeshyReferenceTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.MeshyRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationMeshyReferenceTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Copy = DuplicateStarterAsset();
	TestNotNull(TEXT("Starter copy created"), Copy);
	if (Copy == nullptr) return false;
	Copy->UnitProfiles[0].StaticMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/MeshyImports/Forbidden.Forbidden")));
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(Copy, false);
	TestTrue(
		TEXT("Meshy dependency rejected"),
		Result.ContainsCode(TEXT("untracked_meshy_dependency")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationConcealedBindingTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.ConcealedTypeSpecificRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationConcealedBindingTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Copy = DuplicateStarterAsset();
	TestNotNull(TEXT("Starter copy created"), Copy);
	if (Copy == nullptr) return false;
	FWBRuntimePresentationAssetBinding Binding;
	Binding.EventType = EWBRuntimePresentationEventType::MarkerConsumed;
	Binding.UnitCategory =
		EWBRuntimePresentationUnitCategory::RevealedTrap;
	Binding.StablePriority = 5000;
	Copy->EventBindings.Add(Binding);
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(Copy, false);
	TestTrue(
		TEXT("Concealed type distinction rejected"),
		Result.ContainsCode(
			TEXT("concealed_marker_type_specific_binding")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationPrivateDefinitionTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.PrivateDefinitionRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationPrivateDefinitionTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Copy = DuplicateStarterAsset();
	TestNotNull(TEXT("Starter copy created"), Copy);
	if (Copy == nullptr) return false;
	Copy->UnitProfiles[0].PublicDefinitionId =
		TEXT("private_instance_42");
	const FWBPresentationAssetValidationResult Result =
		WBStarterPresentationAssetValidator::Validate(Copy, false);
	TestTrue(
		TEXT("Private definition rejected"),
		Result.ContainsCode(TEXT("private_definition_id_not_allowed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationGitClassificationTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Validation.GitClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationGitClassificationTest::RunTest(
	const FString& Parameters)
{
	const FString TrackedFile =
		FPaths::Combine(FPaths::ProjectDir(), TEXT("WandboundUE.uproject"));
	TestEqual(
		TEXT("Tracked project file classified"),
		WBStarterPresentationAssetValidator::ClassifyProjectFile(TrackedFile),
		EWBPresentationAssetDependencyStatus::TrackedProjectAsset);

	const FString IgnoredFile = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("AutomationReports/StarterPresentation/ignored_probe.txt"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(IgnoredFile), true);
	TestTrue(
		TEXT("Ignored probe created"),
		FFileHelper::SaveStringToFile(TEXT("probe"), *IgnoredFile));
	TestEqual(
		TEXT("Ignored file classified"),
		WBStarterPresentationAssetValidator::ClassifyProjectFile(IgnoredFile),
		EWBPresentationAssetDependencyStatus::IgnoredProjectAsset);
	IFileManager::Get().Delete(*IgnoredFile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationHeroResolutionTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Resolution.HeroProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationHeroResolutionTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetRegistry* Registry =
		NewObject<UWBRuntimePresentationAssetRegistry>(GetTransientPackage());
	Registry->Configure(LoadStarterAsset(), false);
	const FWBRuntimeUnitProfileResolution Resolution =
		Registry->ResolveUnitProfile(MakeUnit(true, false));
	TestTrue(TEXT("Hero profile resolves"), Resolution.bFoundConfiguredProfile);
	TestEqual(
		TEXT("Hero category selected"),
		Resolution.Profile.UnitCategory,
		EWBRuntimePresentationUnitCategory::PlayerHero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationPlayerUnitResolutionTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Resolution.PlayerUnitProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationPlayerUnitResolutionTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetRegistry* Registry =
		NewObject<UWBRuntimePresentationAssetRegistry>(GetTransientPackage());
	Registry->Configure(LoadStarterAsset(), false);
	const FWBRuntimeUnitProfileResolution Resolution =
		Registry->ResolveUnitProfile(MakeUnit(false, false));
	TestTrue(
		TEXT("Player unit profile resolves"),
		Resolution.bFoundConfiguredProfile);
	TestEqual(
		TEXT("Player unit category selected"),
		Resolution.Profile.UnitCategory,
		EWBRuntimePresentationUnitCategory::PlayerUnit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationNPCResolutionTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Resolution.NeutralNPCProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationNPCResolutionTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetRegistry* Registry =
		NewObject<UWBRuntimePresentationAssetRegistry>(GetTransientPackage());
	Registry->Configure(LoadStarterAsset(), false);
	const FWBRuntimeUnitProfileResolution Resolution =
		Registry->ResolveUnitProfile(MakeUnit(false, true));
	TestTrue(TEXT("NPC profile resolves"), Resolution.bFoundConfiguredProfile);
	TestEqual(
		TEXT("NPC category selected"),
		Resolution.Profile.UnitCategory,
		EWBRuntimePresentationUnitCategory::NeutralNPC);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationMarkerResolutionTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Resolution.MarkerPublicCategories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationMarkerResolutionTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetRegistry* Registry =
		NewObject<UWBRuntimePresentationAssetRegistry>(GetTransientPackage());
	Registry->Configure(LoadStarterAsset(), false);

	FWBRuntimePresentationBindingContext Concealed;
	Concealed.EventType = EWBRuntimePresentationEventType::MarkerConsumed;
	Concealed.UnitCategory =
		EWBRuntimePresentationUnitCategory::ConcealedMarker;
	const FWBRuntimePresentationBindingResolution ConcealedResult =
		Registry->ResolveEventBinding(Concealed);
	TestTrue(
		TEXT("Concealed marker binding resolves"),
		ConcealedResult.bFoundConfiguredBinding);
	TestTrue(
		TEXT("Concealed binding contains no definition"),
		ConcealedResult.Binding.PublicDefinitionId.IsEmpty());

	for (const EWBRuntimePresentationUnitCategory RevealedCategory : {
		EWBRuntimePresentationUnitCategory::RevealedTrap,
		EWBRuntimePresentationUnitCategory::RevealedNPCMarker })
	{
		FWBRuntimePresentationBindingContext Revealed;
		Revealed.EventType =
			EWBRuntimePresentationEventType::MarkerRevealed;
		Revealed.UnitCategory = RevealedCategory;
		const FWBRuntimePresentationBindingResolution Result =
			Registry->ResolveEventBinding(Revealed);
		TestTrue(TEXT("Revealed marker binding resolves"), Result.bFoundConfiguredBinding);
		TestEqual(
			TEXT("Public revealed category selected"),
			Result.Binding.UnitCategory,
			RevealedCategory);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationDeterministicSignatureTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Resolution.EquivalentStateDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationDeterministicSignatureTest::RunTest(
	const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* First = DuplicateStarterAsset();
	UWBRuntimePresentationAssetSet* Second = DuplicateStarterAsset();
	TestNotNull(TEXT("First copy created"), First);
	TestNotNull(TEXT("Second copy created"), Second);
	if (First == nullptr || Second == nullptr) return false;
	Algo::Reverse(Second->EventBindings);
	Algo::Reverse(Second->UnitProfiles);
	TestEqual(
		TEXT("Order-independent normalized signature"),
		WBStarterPresentationAssetValidator::BuildNormalizedSignature(First),
		WBStarterPresentationAssetValidator::BuildNormalizedSignature(Second));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationBootstrapDefaultTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Bootstrap.DevelopmentSoftReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationBootstrapDefaultTest::RunTest(
	const FString& Parameters)
{
	const AWBRuntimeMatchBootstrapActor* DefaultBootstrap =
		GetDefault<AWBRuntimeMatchBootstrapActor>();
	TestTrue(
		TEXT("Starter asset set is configured"),
		DefaultBootstrap->IsPresentationAssetSetConfigured());
	TestEqual(
		TEXT("Soft reference uses intended object path"),
		DefaultBootstrap->DevelopmentPresentationAssetSet.ToSoftObjectPath().ToString(),
		FString(WBStarterPresentationAssetGenerator::GetAssetObjectPath()));
	TestNull(
		TEXT("Production/manual hard reference remains unset"),
		DefaultBootstrap->PresentationAssetSet.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationPackagingGuardTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Cook.PackageScriptNarrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationPackagingGuardTest::RunTest(
	const FString& Parameters)
{
	FString Script;
	TestTrue(
		TEXT("Package script loads"),
		LoadProjectText(
			TEXT("Scripts/Build/PackageWandboundLocalPlay.ps1"),
			Script));
	TestTrue(
		TEXT("Development map remains the cook root"),
		Script.Contains(
			TEXT("-map=/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev")));
	TestTrue(
		TEXT("Starter asset is the only explicit non-map cook package"),
		Script.Contains(
			TEXT("-Package=/Game/Wandbound/Presentation/DA_WandboundStarterPresentation")));
	for (const FString& Forbidden : {
		TEXT("AdditionalAssetDirectoriesToCook"),
		TEXT("-CookDir=Content"),
		TEXT("Content/*"),
		TEXT("MeshyImports"),
		TEXT("Reference/GodotProject") })
	{
		TestFalse(
			*FString::Printf(TEXT("No broad or unsafe cook token: %s"), *Forbidden),
			Script.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBStarterPresentationSourceGuardTest,
	"Wandbound.Runtime.PresentationAssets.Starter.Guards.NoGameplayAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBStarterPresentationSourceGuardTest::RunTest(
	const FString& Parameters)
{
	FString Generator;
	FString Validator;
	FString Bootstrap;
	TestTrue(
		TEXT("Generator source loads"),
		LoadProjectText(
			TEXT("Source/WandboundTests/Private/WBStarterPresentationAssetGeneratorCommandlet.cpp"),
			Generator));
	TestTrue(
		TEXT("Validator source loads"),
		LoadProjectText(
			TEXT("Source/WandboundTests/Private/WBStarterPresentationAssetValidator.cpp"),
			Validator));
	TestTrue(
		TEXT("Bootstrap source loads"),
		LoadProjectText(
			TEXT("Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp"),
			Bootstrap));
	const FString Combined = Generator + Validator + Bootstrap;
	for (const FString& Forbidden : {
		TEXT("WBRules"),
		TEXT("WBEffectRunner"),
		TEXT("FWBGameStateData"),
		TEXT("GetMutableState"),
		TEXT("OpponentHand"),
		TEXT("Reference/GodotProject") })
	{
		TestFalse(
			*FString::Printf(TEXT("No gameplay/private dependency: %s"), *Forbidden),
			Combined.Contains(Forbidden));
	}
	TestFalse(
		TEXT("Runtime bootstrap never scans Content"),
		Bootstrap.Contains(TEXT("FindFilesRecursive")));
	return true;
}

#endif
