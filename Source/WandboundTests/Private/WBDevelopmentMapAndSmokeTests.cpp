#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Camera/CameraActor.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "WBBoardViewActor.h"
#include "WBLocalPlayMapGeneratorCommandlet.h"
#include "WBRuntimeLocalPlayGameMode.h"
#include "WBRuntimeLocalPlaySmoke.h"
#include "WBRuntimeMatchBootstrapActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeUnitPresentationActor.h"
#include "WBRuntimePlayerController.h"

namespace
{
const TCHAR* MapPackagePath = TEXT("/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev");

UWorld* LoadDevelopmentMap()
{
	UPackage* Package = LoadPackage(nullptr, MapPackagePath, LOAD_None);
	return Package != nullptr ? UWorld::FindWorldInPackage(Package) : nullptr;
}

template <typename TActor>
int32 CountLevelActors(UWorld* World)
{
	int32 Count = 0;
	if (World == nullptr || World->PersistentLevel == nullptr) return Count;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (IsValid(Actor) && Actor->IsA<TActor>()) ++Count;
	}
	return Count;
}

struct FSmokeWorldFixture
{
	UWorld* World = nullptr;
	AWBRuntimePlayerController* Controller = nullptr;
	AWBRuntimeLocalPlayGameMode* GameMode = nullptr;
	AWBRuntimeMatchBootstrapActor* Bootstrap = nullptr;

	bool Create()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("WBDevelopmentMapSmokeTestWorld")));
		if (World == nullptr || GEngine == nullptr) return false;
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		Controller = World->SpawnActor<AWBRuntimePlayerController>();
		GameMode = World->SpawnActor<AWBRuntimeLocalPlayGameMode>();
		if (Controller == nullptr || GameMode == nullptr) return false;
		Controller->bCreateHUDOnBeginPlay = false;
		const FWBRuntimeLocalPlayResult StartResult = GameMode->StartLocalPlayForController(Controller);
		Bootstrap = GameMode->GetBootstrapActor();
		return StartResult.bOk && Bootstrap != nullptr;
	}

	void Destroy()
	{
		if (Bootstrap != nullptr) Bootstrap->ShutdownLocalPlay();
		if (World != nullptr && GEngine != nullptr) GEngine->DestroyWorldContext(World);
		if (World != nullptr) World->DestroyWorld(false);
		World = nullptr;
	}
};

bool LoadText(const FString& RelativePath, FString& OutText)
{
	return FFileHelper::LoadFileToString(OutText, *FPaths::Combine(FPaths::ProjectDir(), RelativePath));
}

int32 RunPowerShell(const FString& Arguments, FString& OutStdOut, FString& OutStdErr)
{
	int32 ReturnCode = INDEX_NONE;
	FPlatformProcess::ExecProcess(
		TEXT("C:\\WINDOWS\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"),
		*Arguments,
		&ReturnCode,
		&OutStdOut,
		&OutStdErr);
	return ReturnCode;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentMapStructureTest,
	"Wandbound.Runtime.DevelopmentMap.Asset.StructureAndGameMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentMapStructureTest::RunTest(const FString& Parameters)
{
	FString Filename;
	TestTrue(TEXT("Development map package exists"), FPackageName::DoesPackageExist(MapPackagePath, &Filename));
	UWorld* World = LoadDevelopmentMap();
	TestNotNull(TEXT("Development map loads"), World);
	if (World == nullptr) return false;
	TestTrue(TEXT("Package path matches"), World->GetOutermost()->GetName() == MapPackagePath);
	TestTrue(TEXT("Per-map GameMode override is local-play C++ GameMode"), World->GetWorldSettings()->DefaultGameMode == AWBRuntimeLocalPlayGameMode::StaticClass());
	TestNull(TEXT("No custom Level Blueprint"), World->PersistentLevel->GetLevelScriptBlueprint(true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentMapEnvironmentTest,
	"Wandbound.Runtime.DevelopmentMap.Asset.EnvironmentAndNoGameplayAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentMapEnvironmentTest::RunTest(const FString& Parameters)
{
	UWorld* World = LoadDevelopmentMap();
	TestNotNull(TEXT("Development map loads"), World);
	if (World == nullptr) return false;
	TestEqual(TEXT("One directional light"), CountLevelActors<ADirectionalLight>(World), 1);
	TestEqual(TEXT("One skylight"), CountLevelActors<ASkyLight>(World), 1);
	TestEqual(TEXT("One sky atmosphere"), CountLevelActors<ASkyAtmosphere>(World), 1);
	TestEqual(TEXT("No manually placed board"), CountLevelActors<AWBBoardViewActor>(World), 0);
	TestEqual(TEXT("No manually placed bootstrap"), CountLevelActors<AWBRuntimeMatchBootstrapActor>(World), 0);
	TestEqual(TEXT("No manually placed unit presentations"), CountLevelActors<AWBRuntimeUnitPresentationActor>(World), 0);
	int32 HostComponents = 0;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (IsValid(Actor) && Actor->FindComponentByClass<UWBRuntimeMatchHostComponent>() != nullptr) ++HostComponents;
	}
	TestEqual(TEXT("No manually placed match host"), HostComponents, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentMapGeneratorIdempotenceTest,
	"Wandbound.Runtime.DevelopmentMap.Generator.Idempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentMapGeneratorIdempotenceTest::RunTest(const FString& Parameters)
{
	FString FailureReason;
	bool bFirstChanged = true;
	bool bSecondChanged = true;
	TestTrue(TEXT("First reconciliation succeeds"), WBLocalPlayMapGenerator::Generate(FailureReason, bFirstChanged));
	TestFalse(TEXT("Current map needs no first-run changes"), bFirstChanged);
	TestTrue(TEXT("Second reconciliation succeeds"), WBLocalPlayMapGenerator::Generate(FailureReason, bSecondChanged));
	TestFalse(TEXT("Second run remains unchanged"), bSecondChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentMapProductionIsolationTest,
	"Wandbound.Runtime.DevelopmentMap.Isolation.ProductionDefaultsUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentMapProductionIsolationTest::RunTest(const FString& Parameters)
{
	FString DefaultEngine;
	FString DefaultGame;
	TestTrue(TEXT("DefaultEngine readable"), LoadText(TEXT("Config/DefaultEngine.ini"), DefaultEngine));
	TestTrue(TEXT("DefaultGame readable"), LoadText(TEXT("Config/DefaultGame.ini"), DefaultGame));
	TestTrue(TEXT("Production default map preserved"), DefaultEngine.Contains(TEXT("GameDefaultMap=/Engine/Maps/Templates/OpenWorld")));
	TestFalse(TEXT("Editor startup map unchanged"), DefaultEngine.Contains(TEXT("EditorStartupMap=")));
	TestFalse(TEXT("Global GameMode unchanged"), DefaultEngine.Contains(TEXT("GlobalDefaultGameMode=")) || DefaultGame.Contains(TEXT("GlobalDefaultGameMode=")));
	TestFalse(TEXT("No MapsToCook setting added"), DefaultEngine.Contains(TEXT("MapsToCook")) || DefaultGame.Contains(TEXT("MapsToCook")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentPackagingScriptContractTest,
	"Wandbound.Runtime.DevelopmentMap.Packaging.ScriptContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentPackagingScriptContractTest::RunTest(const FString& Parameters)
{
	FString PackageScript;
	FString SmokeScript;
	FString GeneratorScript;
	TestTrue(TEXT("Package script readable"), LoadText(TEXT("Scripts/Build/PackageWandboundLocalPlay.ps1"), PackageScript));
	TestTrue(TEXT("Smoke script readable"), LoadText(TEXT("Scripts/Build/RunWandboundLocalPlaySmoke.ps1"), SmokeScript));
	TestTrue(TEXT("Generator wrapper readable"), LoadText(TEXT("Scripts/Editor/GenerateWandboundLocalPlayDevMap.ps1"), GeneratorScript));
	TestTrue(TEXT("Package uses BuildCookRun"), PackageScript.Contains(TEXT("BuildCookRun")));
	TestTrue(TEXT("Package explicitly cooks only development map"), PackageScript.Contains(TEXT("-map=/Game/Wandbound/Maps/Wandbound_LocalPlay_Dev")));
	TestTrue(TEXT("Package disables unrelated Datasmith importer content for this cook"), PackageScript.Contains(TEXT("-AdditionalCookerOptions=-DisablePlugins=DatasmithFBXImporter,DatasmithContent")));
	TestTrue(TEXT("Package is Win64 Development"), PackageScript.Contains(TEXT("-targetplatform=Win64")) && PackageScript.Contains(TEXT("-clientconfig=Development")));
	TestTrue(TEXT("Package builds cooks stages packages archives"), PackageScript.Contains(TEXT("\"-build\"")) && PackageScript.Contains(TEXT("\"-cook\"")) && PackageScript.Contains(TEXT("\"-stage\"")) && PackageScript.Contains(TEXT("\"-package\"")) && PackageScript.Contains(TEXT("\"-archive\"")));
	TestTrue(TEXT("UAT failure propagates"), PackageScript.Contains(TEXT("$LASTEXITCODE")) && PackageScript.Contains(TEXT("BuildCookRun failed")));
	TestTrue(TEXT("Smoke checks process and JSON status"), SmokeScript.Contains(TEXT("$processExitCode -ne 0")) && SmokeScript.Contains(TEXT("-not $result.success")));
	TestTrue(TEXT("Smoke captures redirected process exit code reliably"), SmokeScript.Contains(TEXT("$null = $process.Handle")));
	for (const FString& Forbidden : { TEXT("Meshy"), TEXT("Reference/GodotProject"), TEXT("DefaultEngine.ini"), TEXT("DefaultGame.ini") })
	{
		TestFalse(*FString::Printf(TEXT("Scripts exclude %s"), *Forbidden), (PackageScript + SmokeScript + GeneratorScript).Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentPackagingParameterValidationTest,
	"Wandbound.Runtime.DevelopmentMap.Packaging.ParameterValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentPackagingParameterValidationTest::RunTest(const FString& Parameters)
{
	const FString Script = FPaths::Combine(FPaths::ProjectDir(), TEXT("Scripts/Build/PackageWandboundLocalPlay.ps1"));
	const FString Project = FPaths::Combine(FPaths::ProjectDir(), TEXT("WandboundUE.uproject"));
	FString StdOut;
	FString StdErr;
	const int32 ValidCode = RunPowerShell(
		FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -File \"%s\" -ValidateOnly"), *Script),
		StdOut,
		StdErr);
	TestEqual(TEXT("Valid package inputs pass"), ValidCode, 0);
	const int32 MissingProjectCode = RunPowerShell(
		FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -File \"%s\" -ProjectPath \"%s\" -ValidateOnly"), *Script, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Missing.uproject"))),
		StdOut,
		StdErr);
	TestTrue(TEXT("Missing project fails"), MissingProjectCode != 0);
	const int32 MissingEngineCode = RunPowerShell(
		FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -File \"%s\" -ProjectPath \"%s\" -EngineRoot \"C:\\MissingUE\" -ValidateOnly"), *Script, *Project),
		StdOut,
		StdErr);
	TestTrue(TEXT("Missing engine fails"), MissingEngineCode != 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentSmokeCommandLinePolicyTest,
	"Wandbound.Runtime.DevelopmentMap.Smoke.CommandLinePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentSmokeCommandLinePolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Ordinary local play does not request smoke"), UWBRuntimeLocalPlaySmokeCoordinator::IsSmokeRequested(TEXT("-unattended")));
#if UE_BUILD_SHIPPING
	TestFalse(TEXT("Shipping ignores smoke flag"), UWBRuntimeLocalPlaySmokeCoordinator::IsSmokeRequested(TEXT("-WandboundLocalPlaySmoke")));
#else
	TestTrue(TEXT("Explicit development flag requests smoke"), UWBRuntimeLocalPlaySmokeCoordinator::IsSmokeRequested(TEXT("-WandboundLocalPlaySmoke")));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentSmokeSuccessTest,
	"Wandbound.Runtime.DevelopmentMap.Smoke.SuccessActionTurnAndExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentSmokeSuccessTest::RunTest(const FString& Parameters)
{
	FSmokeWorldFixture Fixture;
	if (!Fixture.Create()) return false;
	UWBRuntimeLocalPlaySmokeCoordinator* Smoke = NewObject<UWBRuntimeLocalPlaySmokeCoordinator>(Fixture.GameMode);
	int32 ExitCount = 0;
	uint8 ExitCode = 255;
	Smoke->SetExitRequestForTesting([&ExitCount, &ExitCode](const uint8 InExitCode) { ++ExitCount; ExitCode = InExitCode; });
	const FString ResultPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AutomationReports/WandboundLocalPlaySmokeTests/success.json"));
	Smoke->SetResultPathForTesting(ResultPath);
	const int32 InitialRevision = Fixture.Bootstrap->GetMatchHost()->GetCurrentPresentation().PresentationRevision;
	TestTrue(TEXT("Smoke succeeds through runtime path"), Smoke->RunSmoke(Fixture.GameMode, Fixture.Bootstrap, Fixture.World->GetOutermost()->GetName()));
	const FWBRuntimeLocalPlaySmokeResult Result = Smoke->GetLastResult();
	TestTrue(TEXT("Stable action submitted"), Result.bActionSubmitted);
	TestTrue(TEXT("End Turn submitted"), Result.bEndTurnSubmitted);
	TestTrue(TEXT("Presentation advanced"), Result.PresentationRevision > InitialRevision);
	TestEqual(TEXT("Exit requested once"), ExitCount, 1);
	TestEqual(TEXT("Success exit code"), ExitCode, static_cast<uint8>(0));
	TestFalse(TEXT("Duplicate smoke run rejected"), Smoke->RunSmoke(Fixture.GameMode, Fixture.Bootstrap, Fixture.World->GetOutermost()->GetName()));
	TestEqual(TEXT("Duplicate does not request exit again"), ExitCount, 1);
	FString Json;
	TestTrue(TEXT("Success result written"), FFileHelper::LoadFileToString(Json, *ResultPath));
	TestTrue(TEXT("Success result declares success"), Json.Contains(TEXT("\"success\": true")));
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentSmokeFailureTest,
	"Wandbound.Runtime.DevelopmentMap.Smoke.FailureResultAndExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentSmokeFailureTest::RunTest(const FString& Parameters)
{
	FSmokeWorldFixture Fixture;
	if (!Fixture.Create()) return false;
	UWBRuntimeLocalPlaySmokeCoordinator* Smoke = NewObject<UWBRuntimeLocalPlaySmokeCoordinator>(Fixture.GameMode);
	uint8 ExitCode = 0;
	Smoke->SetExitRequestForTesting([&ExitCode](const uint8 InExitCode) { ExitCode = InExitCode; });
	const FString ResultPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AutomationReports/WandboundLocalPlaySmokeTests/failure.json"));
	Smoke->SetResultPathForTesting(ResultPath);
	AddExpectedError(TEXT("Wandbound packaged local-play smoke failed: smoke_bootstrap_not_ready"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Missing bootstrap fails"), Smoke->RunSmoke(Fixture.GameMode, nullptr, Fixture.World->GetOutermost()->GetName()));
	TestTrue(TEXT("Failure exit is nonzero"), ExitCode != 0);
	FString Json;
	TestTrue(TEXT("Failure result written"), FFileHelper::LoadFileToString(Json, *ResultPath));
	TestTrue(TEXT("Failure diagnostic serialized"), Json.Contains(TEXT("smoke_bootstrap_not_ready")));
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentSmokeResultSchemaTest,
	"Wandbound.Runtime.DevelopmentMap.Smoke.ResultSchemaHiddenInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentSmokeResultSchemaTest::RunTest(const FString& Parameters)
{
	FWBRuntimeLocalPlaySmokeResult Result;
	Result.MapName = MapPackagePath;
	Result.GameModeClass = TEXT("/Script/WandboundRuntime.WBRuntimeLocalPlayGameMode");
	const FString Json = UWBRuntimeLocalPlaySmokeCoordinator::SerializeResult(Result);
	for (const FString& Required : { TEXT("success"), TEXT("failure_reason"), TEXT("map_name"), TEXT("game_mode_class"), TEXT("bootstrap_state"), TEXT("match_generation"), TEXT("presentation_revision"), TEXT("tile_count"), TEXT("visible_unit_count"), TEXT("visible_hero_count"), TEXT("concealed_marker_count"), TEXT("own_hand_count"), TEXT("legal_action_count"), TEXT("action_submitted"), TEXT("end_turn_submitted"), TEXT("game_over"), TEXT("presentation_asset_set_configured"), TEXT("presentation_asset_loading_enabled"), TEXT("presentation_fallback_active"), TEXT("winner_player_id"), TEXT("process_exit_code") })
	{
		TestTrue(*FString::Printf(TEXT("Schema contains %s"), *Required), Json.Contains(Required));
	}
	for (const FString& Forbidden : { TEXT("opponent_hand"), TEXT("marker_type"), TEXT("definition_id"), TEXT("card_instance_id"), TEXT("authoritative_state") })
	{
		TestFalse(*FString::Printf(TEXT("Schema excludes %s"), *Forbidden), Json.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBDevelopmentSmokeSourceGuardTest,
	"Wandbound.Runtime.DevelopmentMap.Smoke.SourceGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDevelopmentSmokeSourceGuardTest::RunTest(const FString& Parameters)
{
	FString SmokeSource;
	FString GameModeSource;
	TestTrue(TEXT("Smoke source readable"), LoadText(TEXT("Source/WandboundRuntime/Private/WBRuntimeLocalPlaySmoke.cpp"), SmokeSource));
	TestTrue(TEXT("GameMode source readable"), LoadText(TEXT("Source/WandboundRuntime/Private/WBRuntimeLocalPlayGameMode.cpp"), GameModeSource));
	TestTrue(TEXT("Stable action revision boundary used"), SmokeSource.Contains(TEXT("SubmitLegalActionAtRevision")));
	TestTrue(TEXT("Normal host End Turn boundary used"), SmokeSource.Contains(TEXT("Host->EndTurn()")));
	TestTrue(TEXT("Smoke is explicit command-line only"), GameModeSource.Contains(TEXT("IsSmokeRequested(FCommandLine::Get())")));
	for (const FString& Forbidden : { TEXT("WBRules"), TEXT("WBEffectRunner"), TEXT("GetMutableStateForTest"), TEXT("Reference/GodotProject"), TEXT("Meshy") })
	{
		TestFalse(*FString::Printf(TEXT("Smoke source excludes %s"), *Forbidden), (SmokeSource + GameModeSource).Contains(Forbidden));
	}
	return true;
}

#endif
