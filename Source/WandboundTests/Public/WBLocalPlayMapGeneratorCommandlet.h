#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WBLocalPlayMapGeneratorCommandlet.generated.h"

struct WANDBOUNDTESTS_API WBLocalPlayMapGenerator
{
	static const TCHAR* GetMapPackagePath();
	static bool Generate(FString& OutFailureReason, bool& bOutMapChanged);
};

UCLASS()
class WANDBOUNDTESTS_API UWandboundLocalPlayMapGeneratorCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWandboundLocalPlayMapGeneratorCommandlet();
	virtual int32 Main(const FString& Params) override;
};
