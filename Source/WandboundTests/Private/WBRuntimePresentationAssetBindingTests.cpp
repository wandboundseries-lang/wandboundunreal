#include "Misc/AutomationTest.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimePresentationAssetPlaybackComponent.h"
#include "WBRuntimePresentationAssetRegistry.h"
#include "WBRuntimePresentationSequenceComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimePresentationAssetRegistry* MakeRegistry(
	UWBRuntimePresentationAssetSet*& OutAssetSet,
	const bool bEnableLoading = false,
	const int32 Generation = 7)
{
	OutAssetSet = NewObject<UWBRuntimePresentationAssetSet>(GetTransientPackage());
	UWBRuntimePresentationAssetRegistry* Registry =
		NewObject<UWBRuntimePresentationAssetRegistry>(GetTransientPackage());
	Registry->Configure(OutAssetSet, bEnableLoading);
	Registry->BeginMatchGeneration(Generation);
	return Registry;
}

FWBRuntimePresentationAssetBinding MakeBinding(
	const EWBRuntimePresentationEventType Type,
	const EWBRuntimePresentationUnitCategory Category,
	const FString& DefinitionId = FString(),
	const int32 Priority = 0)
{
	FWBRuntimePresentationAssetBinding Binding;
	Binding.EventType = Type;
	Binding.UnitCategory = Category;
	Binding.PublicDefinitionId = DefinitionId;
	Binding.StablePriority = Priority;
	return Binding;
}

FWBRuntimePresentationBindingContext MakeContext(
	const EWBRuntimePresentationEventType Type,
	const EWBRuntimePresentationUnitCategory Category,
	const FString& DefinitionId = FString(),
	const bool bDefinitionAllowed = true)
{
	FWBRuntimePresentationBindingContext Context;
	Context.EventType = Type;
	Context.UnitCategory = Category;
	Context.PublicDefinitionId = DefinitionId;
	Context.bPublicDefinitionIdAllowed =
		bDefinitionAllowed && !DefinitionId.IsEmpty();
	return Context;
}

FWBRuntimeUnitPresentation MakeUnit(
	const bool bHero,
	const bool bNeutral,
	const FString& DefinitionId)
{
	FWBRuntimeUnitPresentation Unit;
	Unit.UnitId = bNeutral ? 90 : bHero ? 10 : 20;
	Unit.OwnerId = bNeutral ? -1 : 0;
	Unit.bHero = bHero;
	Unit.bNeutralNPC = bNeutral;
	Unit.PublicDefinitionId = DefinitionId;
	return Unit;
}

TSoftObjectPtr<UStaticMesh> CylinderMesh()
{
	return TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetExactDefinitionTest,
	"Wandbound.Runtime.PresentationAssets.Binding.ExactDefinitionWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetExactDefinitionTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::AttackDeclared,
		EWBRuntimePresentationUnitCategory::Any));
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::AttackDeclared,
		EWBRuntimePresentationUnitCategory::PlayerHero,
		TEXT("hero_alpha")));
	const FWBRuntimePresentationBindingResolution Result =
		Registry->ResolveEventBinding(MakeContext(
			EWBRuntimePresentationEventType::AttackDeclared,
			EWBRuntimePresentationUnitCategory::PlayerHero,
			TEXT("hero_alpha")));
	TestTrue(TEXT("Configured binding resolves"), Result.bFoundConfiguredBinding);
	TestEqual(TEXT("Exact definition is selected"), Result.Binding.PublicDefinitionId, FString(TEXT("hero_alpha")));
	TestEqual(TEXT("Exact specificity"), Result.Specificity, 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetHeroCategoryTest,
	"Wandbound.Runtime.PresentationAssets.Binding.HeroFallsBackToPlayerUnitCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetHeroCategoryTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::UnitMoved,
		EWBRuntimePresentationUnitCategory::Any));
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::UnitMoved,
		EWBRuntimePresentationUnitCategory::PlayerUnit));
	const FWBRuntimePresentationBindingResolution Result =
		Registry->ResolveEventBinding(MakeContext(
			EWBRuntimePresentationEventType::UnitMoved,
			EWBRuntimePresentationUnitCategory::PlayerHero));
	TestEqual(TEXT("Player-unit category selected"), Result.Binding.UnitCategory, EWBRuntimePresentationUnitCategory::PlayerUnit);
	TestEqual(TEXT("Parent-category specificity"), Result.Specificity, 250);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetGlobalFallbackTest,
	"Wandbound.Runtime.PresentationAssets.Binding.GlobalEventFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetGlobalFallbackTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::TurnStarted,
		EWBRuntimePresentationUnitCategory::Any));
	const FWBRuntimePresentationBindingResolution Result =
		Registry->ResolveEventBinding(MakeContext(
			EWBRuntimePresentationEventType::TurnStarted,
			EWBRuntimePresentationUnitCategory::NeutralNPC));
	TestTrue(TEXT("Global binding resolves"), Result.bFoundConfiguredBinding);
	TestEqual(TEXT("Global specificity"), Result.Specificity, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetStablePriorityTest,
	"Wandbound.Runtime.PresentationAssets.Binding.StablePriorityBreaksEqualSpecificity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetStablePriorityTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::DamageApplied,
		EWBRuntimePresentationUnitCategory::PlayerUnit,
		FString(),
		2));
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::DamageApplied,
		EWBRuntimePresentationUnitCategory::PlayerUnit,
		FString(),
		9));
	const FWBRuntimePresentationBindingResolution Result =
		Registry->ResolveEventBinding(MakeContext(
			EWBRuntimePresentationEventType::DamageApplied,
			EWBRuntimePresentationUnitCategory::PlayerUnit));
	TestEqual(TEXT("Higher stable priority wins"), Result.Binding.StablePriority, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetStablePathTest,
	"Wandbound.Runtime.PresentationAssets.Binding.AssetPathBreaksCompleteTie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetStablePathTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimePresentationAssetBinding Z = MakeBinding(
		EWBRuntimePresentationEventType::GameOver,
		EWBRuntimePresentationUnitCategory::Any);
	Z.StaticMeshOverride = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	FWBRuntimePresentationAssetBinding A = Z;
	A.StaticMeshOverride = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone")));
	Set->EventBindings = { Z, A };
	const FWBRuntimePresentationBindingResolution Result =
		Registry->ResolveEventBinding(MakeContext(
			EWBRuntimePresentationEventType::GameOver,
			EWBRuntimePresentationUnitCategory::Any));
	TestEqual(
		TEXT("Lexical path wins independent of insertion"),
		Result.Binding.StaticMeshOverride.ToSoftObjectPath().ToString(),
		FString(TEXT("/Engine/BasicShapes/Cone.Cone")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetConcealedMarkerTest,
	"Wandbound.Runtime.PresentationAssets.HiddenInfo.ConcealedMarkerLookupIsTypeNeutral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetConcealedMarkerTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimePresentationEvent Event;
	Event.Type = EWBRuntimePresentationEventType::MarkerConsumed;
	Event.PublicMarkerType = FName(TEXT("Trap"));
	Event.PublicDefinitionId = TEXT("private_trap");
	const FWBRuntimePresentationBindingContext Context =
		Registry->BuildBindingContext(Event, nullptr, nullptr);
	TestEqual(TEXT("Concealed category remains generic"), Context.UnitCategory, EWBRuntimePresentationUnitCategory::ConcealedMarker);
	TestTrue(TEXT("Definition removed"), Context.PublicDefinitionId.IsEmpty());
	TestTrue(TEXT("Marker type removed"), Context.PublicMarkerType.IsNone());
	TestFalse(TEXT("Definition lookup forbidden"), Context.bPublicDefinitionIdAllowed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetPublicDefinitionGateTest,
	"Wandbound.Runtime.PresentationAssets.HiddenInfo.PrivateDefinitionCannotSelectBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetPublicDefinitionGateTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::WandEquipped,
		EWBRuntimePresentationUnitCategory::PlayerUnit,
		TEXT("private_wand")));
	Set->EventBindings.Add(MakeBinding(
		EWBRuntimePresentationEventType::WandEquipped,
		EWBRuntimePresentationUnitCategory::Any));
	const FWBRuntimePresentationBindingResolution Result =
		Registry->ResolveEventBinding(MakeContext(
			EWBRuntimePresentationEventType::WandEquipped,
			EWBRuntimePresentationUnitCategory::PlayerUnit,
			TEXT("private_wand"),
			false));
	TestTrue(TEXT("Safe global binding resolves"), Result.bFoundConfiguredBinding);
	TestTrue(TEXT("Private definition binding ignored"), Result.Binding.PublicDefinitionId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetEquivalentResolutionTest,
	"Wandbound.Runtime.PresentationAssets.Determinism.EquivalentInputResolvesIdentically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetEquivalentResolutionTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimePresentationAssetBinding Binding = MakeBinding(
		EWBRuntimePresentationEventType::AttackImpact,
		EWBRuntimePresentationUnitCategory::NeutralNPC,
		FString(),
		3);
	Binding.PresentationDurationSeconds = 0.75f;
	Set->EventBindings.Add(Binding);
	const FWBRuntimePresentationBindingContext Context = MakeContext(
		EWBRuntimePresentationEventType::AttackImpact,
		EWBRuntimePresentationUnitCategory::NeutralNPC);
	const FWBRuntimePresentationBindingResolution A = Registry->ResolveEventBinding(Context);
	const FWBRuntimePresentationBindingResolution B = Registry->ResolveEventBinding(Context);
	TestEqual(TEXT("Reason stable"), A.Reason, B.Reason);
	TestEqual(TEXT("Specificity stable"), A.Specificity, B.Specificity);
	TestEqual(TEXT("Duration stable"), A.Binding.PresentationDurationSeconds, B.Binding.PresentationDurationSeconds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetHeroProfileTest,
	"Wandbound.Runtime.PresentationAssets.UnitProfiles.HeroCategorySelected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetHeroProfileTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimeUnitAssetProfile Profile;
	Profile.UnitCategory = EWBRuntimePresentationUnitCategory::PlayerHero;
	Set->UnitProfiles.Add(Profile);
	const FWBRuntimeUnitProfileResolution Result =
		Registry->ResolveUnitProfile(MakeUnit(true, false, TEXT("hero")));
	TestTrue(TEXT("Hero profile found"), Result.bFoundConfiguredProfile);
	TestEqual(TEXT("Hero category"), Result.Profile.UnitCategory, EWBRuntimePresentationUnitCategory::PlayerHero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetPlayerProfileTest,
	"Wandbound.Runtime.PresentationAssets.UnitProfiles.PlayerUnitCategorySelected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetPlayerProfileTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimeUnitAssetProfile Profile;
	Profile.UnitCategory = EWBRuntimePresentationUnitCategory::PlayerUnit;
	Set->UnitProfiles.Add(Profile);
	const FWBRuntimeUnitProfileResolution Result =
		Registry->ResolveUnitProfile(MakeUnit(false, false, TEXT("student")));
	TestTrue(TEXT("Player unit profile found"), Result.bFoundConfiguredProfile);
	TestEqual(TEXT("Player category"), Result.Profile.UnitCategory, EWBRuntimePresentationUnitCategory::PlayerUnit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetNPCProfileTest,
	"Wandbound.Runtime.PresentationAssets.UnitProfiles.NeutralNPCProfileSelected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetNPCProfileTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimeUnitAssetProfile Profile;
	Profile.UnitCategory = EWBRuntimePresentationUnitCategory::NeutralNPC;
	Set->UnitProfiles.Add(Profile);
	const FWBRuntimeUnitProfileResolution Result =
		Registry->ResolveUnitProfile(MakeUnit(false, true, TEXT("basic_npc")));
	TestTrue(TEXT("NPC profile found"), Result.bFoundConfiguredProfile);
	TestEqual(TEXT("NPC category"), Result.Profile.UnitCategory, EWBRuntimePresentationUnitCategory::NeutralNPC);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetExactProfileTest,
	"Wandbound.Runtime.PresentationAssets.UnitProfiles.PublicDefinitionOverridesCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetExactProfileTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	FWBRuntimeUnitAssetProfile Category;
	Category.UnitCategory = EWBRuntimePresentationUnitCategory::PlayerUnit;
	FWBRuntimeUnitAssetProfile Exact = Category;
	Exact.PublicDefinitionId = TEXT("student");
	Set->UnitProfiles = { Category, Exact };
	const FWBRuntimeUnitProfileResolution Result =
		Registry->ResolveUnitProfile(MakeUnit(false, false, TEXT("student")));
	TestEqual(TEXT("Definition profile selected"), Result.Profile.PublicDefinitionId, FString(TEXT("student")));
	TestEqual(TEXT("Definition specificity"), Result.Specificity, 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetProfileFallbackTest,
	"Wandbound.Runtime.PresentationAssets.UnitProfiles.NoProfileUsesPrimitiveFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetProfileFallbackTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set);
	const FWBRuntimeUnitProfileResolution Result =
		Registry->ResolveUnitProfile(MakeUnit(false, false, TEXT("unknown")));
	TestFalse(TEXT("No configured profile"), Result.bFoundConfiguredProfile);
	TestTrue(TEXT("Primitive fallback required"), Result.bUsePrimitiveFallback);
	TestEqual(TEXT("Safe reason"), Result.Reason, FString(TEXT("unit_asset_profile_fallback")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetEngineLoadTest,
	"Wandbound.Runtime.PresentationAssets.Loading.EngineStaticMeshLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetEngineLoadTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set, true);
	UStaticMesh* Loaded = Registry->LoadStaticMesh(CylinderMesh(), 7);
	TestNotNull(TEXT("Tracked engine fallback mesh loads"), Loaded);
	TestTrue(TEXT("No diagnostic"), Registry->GetLastDiagnostic().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetWrongClassTest,
	"Wandbound.Runtime.PresentationAssets.Loading.WrongAssetClassFallsBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetWrongClassTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set, true);
	const TSoftObjectPtr<USoundBase> WrongType(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	TestNull(TEXT("Wrong asset class rejected"), Registry->LoadSound(WrongType, 7));
	TestEqual(TEXT("Safe diagnostic"), Registry->GetLastDiagnostic(), FString(TEXT("presentation_asset_load_failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetStaleGenerationTest,
	"Wandbound.Runtime.PresentationAssets.Loading.StaleGenerationRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetStaleGenerationTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set, true, 12);
	TestNull(TEXT("Stale load rejected"), Registry->LoadStaticMesh(CylinderMesh(), 11));
	TestEqual(TEXT("Generation diagnostic"), Registry->GetLastDiagnostic(), FString(TEXT("stale_asset_generation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetDisabledLoadTest,
	"Wandbound.Runtime.PresentationAssets.Loading.DisabledLoadingIsSilentFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetDisabledLoadTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set, false);
	TestNull(TEXT("Loading remains disabled"), Registry->LoadStaticMesh(CylinderMesh(), 7));
	TestTrue(TEXT("Disabled loading is not a failure"), Registry->GetLastDiagnostic().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetDurationAuthorityTest,
	"Wandbound.Runtime.PresentationAssets.Sequence.ConfiguredDurationOwnedBySequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetDurationAuthorityTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set =
		NewObject<UWBRuntimePresentationAssetSet>(GetTransientPackage());
	FWBRuntimePresentationAssetBinding Move = MakeBinding(
		EWBRuntimePresentationEventType::UnitMoved,
		EWBRuntimePresentationUnitCategory::Any);
	Move.PresentationDurationSeconds = 0.73f;
	Set->EventBindings.Add(Move);
	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	Host->ConfigurePresentationAssets(Set, nullptr, false);
	TestTrue(TEXT("Match initializes"), Host->InitializeDevelopmentMatch(0, false).bOk);
	Host->SetPresentationPlaybackSpeed(1.0f);
	const TArray<FWBRuntimeLegalActionPresentation> LegalActions =
		Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* MoveAction =
		LegalActions.FindByPredicate([](
			const FWBRuntimeLegalActionPresentation& Action)
		{
			return Action.Family == EWBRuntimeMatchActionFamily::Move;
		});
	TestNotNull(TEXT("Move available"), MoveAction);
	if (MoveAction == nullptr)
	{
		return false;
	}
	const FString ActionId = MoveAction->ActionId;
	TestTrue(TEXT("Move accepted"), Host->SubmitLegalActionById(ActionId).bOk);
	TestTrue(TEXT("Sequence remains active"), Host->IsPresentationSequenceActive());
	TestEqual(
		TEXT("Sequence event owns configured duration"),
		Host->GetPresentationSequence()->GetCurrentEvent().SuggestedDurationSeconds,
		0.73f);
	Host->SkipAllPresentationEvents();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetSkipStateTest,
	"Wandbound.Runtime.PresentationAssets.Sequence.SkipDoesNotChangeCommittedState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetSkipStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	TestTrue(TEXT("Match initializes"), Host->InitializeDevelopmentMatch(0, false).bOk);
	Host->SetPresentationPlaybackSpeed(1.0f);
	const TArray<FWBRuntimeLegalActionPresentation> LegalActions =
		Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* MoveAction =
		LegalActions.FindByPredicate([](
			const FWBRuntimeLegalActionPresentation& Action)
		{
			return Action.Family == EWBRuntimeMatchActionFamily::Move;
		});
	if (MoveAction == nullptr)
	{
		return false;
	}
	const FString ActionId = MoveAction->ActionId;
	const int32 UnitId = MoveAction->SourceUnitId;
	const FIntPoint Destination = MoveAction->TargetTile;
	TestTrue(TEXT("Move accepted"), Host->SubmitLegalActionById(ActionId).bOk);
	Host->SkipAllPresentationEvents();
	const FWBRuntimeUnitPresentation* Unit =
		Host->GetCurrentUnits().FindByPredicate([UnitId](
			const FWBRuntimeUnitPresentation& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	TestNotNull(TEXT("Unit remains"), Unit);
	if (Unit != nullptr)
	{
		TestEqual(TEXT("Committed destination retained"), Unit->Tile, Destination);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetCleanupTest,
	"Wandbound.Runtime.PresentationAssets.Playback.MissingAssetsAndRepeatedCleanupAreHarmless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetCleanupTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set = nullptr;
	UWBRuntimePresentationAssetRegistry* Registry = MakeRegistry(Set, false);
	UWBRuntimePresentationAssetPlaybackComponent* Playback =
		NewObject<UWBRuntimePresentationAssetPlaybackComponent>(GetTransientPackage());
	Playback->Configure(Registry, nullptr);
	FWBRuntimePresentationAssetBinding Binding = MakeBinding(
		EWBRuntimePresentationEventType::AttackImpact,
		EWBRuntimePresentationUnitCategory::Any);
	Binding.Sound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Wandbound/Missing/OptionalSound.OptionalSound")));
	FWBRuntimePresentationBindingResolution Resolution;
	Resolution.bFoundConfiguredBinding = true;
	Resolution.bUsePrimitiveFallback = false;
	Resolution.Binding = Binding;
	FWBRuntimePresentationEvent Event;
	Event.Type = EWBRuntimePresentationEventType::AttackImpact;
	Event.SequenceIndex = 4;
	const FWBRuntimePresentationAssetPlaybackResult Result =
		Playback->BeginEventPlayback(
			Event,
			Resolution,
			nullptr,
			nullptr,
			FVector::ZeroVector,
			FVector::ZeroVector,
			7);
	TestTrue(TEXT("Missing optional assets do not fail playback"), Result.bOk);
	Playback->StopAllPresentationAssets();
	Playback->StopAllPresentationAssets();
	TestEqual(TEXT("No audio remains"), Playback->GetActiveAudioComponentCount(), 0);
	TestEqual(TEXT("No VFX remains"), Playback->GetActiveVFXComponentCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetSmokePolicyTest,
	"Wandbound.Runtime.PresentationAssets.PackagedSmoke.UnattendedPolicyDisablesAssetLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetSmokePolicyTest::RunTest(const FString& Parameters)
{
	UWBRuntimePresentationAssetSet* Set =
		NewObject<UWBRuntimePresentationAssetSet>(GetTransientPackage());
	UWBRuntimeMatchHostComponent* Host =
		NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	Host->ConfigurePresentationAssets(Set, nullptr, true);
	TestNotNull(TEXT("Registry created"), Host->GetPresentationAssetRegistry());
	if (Host->GetPresentationAssetRegistry() != nullptr)
	{
		TestFalse(
			TEXT("Unattended automation suppresses authored loading"),
			Host->GetPresentationAssetRegistry()->IsAssetLoadingEnabled());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimePresentationAssetSourceGuardTest,
	"Wandbound.Runtime.PresentationAssets.SourceGuards.NoGameplayAuthorityOrMeshyDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimePresentationAssetSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const TArray<FString> Files = {
		Root / TEXT("Source/WandboundRuntime/Private/WBRuntimePresentationAssetRegistry.cpp"),
		Root / TEXT("Source/WandboundRuntime/Private/WBRuntimePresentationAssetPlaybackComponent.cpp")
	};
	for (const FString& File : Files)
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("Reads %s"), *File), FFileHelper::LoadFileToString(Source, *File));
		TestFalse(TEXT("No rules executor"), Source.Contains(TEXT("WBRules")));
		TestFalse(TEXT("No effect runner"), Source.Contains(TEXT("WBEffectRunner")));
		TestFalse(TEXT("No mutable game state"), Source.Contains(TEXT("FWBGameStateData")));
		TestFalse(TEXT("No private zone inspection"), Source.Contains(TEXT("CardZoneState")));
		TestFalse(TEXT("No gameplay RNG"), Source.Contains(TEXT("RandomStream")));
		TestFalse(TEXT("No sequence advancement"), Source.Contains(TEXT("AdvanceSequence")));
		TestFalse(TEXT("No Meshy content dependency"), Source.Contains(TEXT("MeshyImports")));
	}
	return true;
}

#endif
