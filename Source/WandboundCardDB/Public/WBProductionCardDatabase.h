#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"

enum class EWBProductionCardDBDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

enum class EWBProductionBundleKind : uint8
{
	Unknown,
	Production,
	Test,
	Development
};

enum class EWBProductionCardType : uint8
{
	Unknown,
	Character,
	Hero,
	Wand,
	Action,
	Trap,
	NPC
};

enum class EWBProductionEffectSupport : uint8
{
	Supported,
	Unsupported,
	Missing,
	LegacyOnly,
	DevelopmentOnly
};

struct WANDBOUNDCARDDB_API FWBProductionCardDBDiagnostic
{
	EWBProductionCardDBDiagnosticSeverity Severity =
		EWBProductionCardDBDiagnosticSeverity::Error;
	FString Code;
	FString ManifestPath;
	FString DefinitionId;
	FString FieldPath;
	FString Message;
	FString RecommendedAction;
};

struct WANDBOUNDCARDDB_API FWBProductionMovementSpecification
{
	FString Pattern;
};

struct WANDBOUNDCARDDB_API FWBProductionAttackSpecification
{
	FString Pattern;
	int32 Range = 0;
};

struct WANDBOUNDCARDDB_API FWBProductionEquipSpecification
{
	FString TargetRequirement;
	int32 ResonanceRequirement = 0;
};

struct WANDBOUNDCARDDB_API FWBProductionCardRecord
{
	EWBProductionCardType Type = EWBProductionCardType::Unknown;
	FWBCardDefinition CoreDefinition;
	FWBProductionMovementSpecification Movement;
	FWBProductionAttackSpecification Attack;
	FWBProductionEquipSpecification Equip;
	bool bHeroRole = false;
	FString HeroMatchStartPlacement;
	FString SourceManifestPath;
	FString SourceBundlePath;
};

struct WANDBOUNDCARDDB_API FWBProductionCardDatabase
{
	FString SuiteId;
	FString CardDBVersion;
	FString SourceVersion;
	EWBProductionBundleKind BundleKind = EWBProductionBundleKind::Unknown;
	FString ContentDigest;
	TArray<FString> IncludedManifestPaths;
	TArray<FString> IncludedBundlePaths;
	TArray<FWBProductionCardRecord> Records;
	FWBCardDefinitionRepository CoreRepository;

	const FWBProductionCardRecord* FindRecord(const FString& DefinitionId) const;
	const FWBProductionCardRecord* FindCharacter(const FString& DefinitionId) const;
	const FWBProductionCardRecord* FindHero(const FString& DefinitionId) const;
	const FWBProductionCardRecord* FindWand(const FString& DefinitionId) const;
	TArray<FString> GetDefinitionIds() const;
};

struct WANDBOUNDCARDDB_API FWBProductionCardDatabaseLoadResult
{
	bool bOk = false;
	FString Reason;
	FString RootManifestPath;
	TSharedPtr<const FWBProductionCardDatabase> Snapshot;
	TArray<FWBProductionCardDBDiagnostic> Diagnostics;
};

class WANDBOUNDCARDDB_API WBProductionCardDatabase
{
public:
	static FWBProductionCardDatabaseLoadResult LoadManifestSuite(
		const FString& RootManifestPath);

	static FWBProductionCardDatabaseLoadResult LoadManifestSuiteFromJsonForTest(
		const FString& Json,
		const FString& RootManifestPath,
		const FString& SuiteRootDirectory);

	static FString ResolveInputPath(const FString& InputPath);
	static FString DiagnosticSeverityToString(
		EWBProductionCardDBDiagnosticSeverity Severity);
	static FString BundleKindToString(EWBProductionBundleKind Kind);
	static FString CardTypeToString(EWBProductionCardType Type);
	static FString EffectSupportToString(EWBProductionEffectSupport Support);
	static bool IsSafeDefinitionId(const FString& DefinitionId);
	static bool IsSafeRepositoryRelativePath(const FString& RelativePath);
	static bool SnapshotToCanonicalJson(
		const FWBProductionCardDatabase& Snapshot,
		FString& OutJson);
};
