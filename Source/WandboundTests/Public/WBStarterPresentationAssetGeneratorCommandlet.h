#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WBStarterPresentationAssetGeneratorCommandlet.generated.h"

class UWBRuntimePresentationAssetSet;

struct WANDBOUNDTESTS_API WBStarterPresentationAssetGenerator
{
	static constexpr int32 ManualOverridePriorityFloor = 100000;

	static const TCHAR* GetAssetPackagePath();
	static const TCHAR* GetAssetObjectPath();
	static FString GetAssetFilename();
	static UWBRuntimePresentationAssetSet* LoadGeneratedAsset();
	static bool Generate(
		FString& OutFailureReason,
		bool& bOutAssetChanged,
		FString& OutNormalizedSignature);
};

UCLASS()
class WANDBOUNDTESTS_API UWBStarterPresentationAssetGeneratorCommandlet
	: public UCommandlet
{
	GENERATED_BODY()

public:
	UWBStarterPresentationAssetGeneratorCommandlet();
	virtual int32 Main(const FString& Params) override;
};
