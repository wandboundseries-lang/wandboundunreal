#pragma once

#include "Commandlets/Commandlet.h"
#include "WBCharacterModelImportCommandlet.generated.h"

UCLASS()
class WANDBOUNDEDITOR_API UWBCharacterModelImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWBCharacterModelImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
