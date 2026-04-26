#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "LargeDepthMapManifestFactory.generated.h"

UCLASS()
class LARGEDEPTHMAPEDITOR_API ULargeDepthMapManifestFactory : public UFactory
{
	GENERATED_BODY()

public:
	ULargeDepthMapManifestFactory();

	virtual UObject* FactoryCreateFile(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		const FString& Filename,
		const TCHAR* Parms,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled) override;
};
