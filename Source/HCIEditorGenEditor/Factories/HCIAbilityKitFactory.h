
#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "HCIAbilityKitFactory.generated.h"

UCLASS()
class UHCIAbilityKitFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UHCIAbilityKitFactory();

	// UFactory
	virtual bool FactoryCanImport(const FString& Filename) override;

	virtual UObject* FactoryCreateFile(
			UClass* InClass,
			UObject* InParent,
			FName InName,
			EObjectFlags Flags,
			const FString& Filename,
			const TCHAR* Parms,
			FFeedbackContext* Warn,
			bool& bOutOperationCanceled) override;

	// FReimportHandler
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override { return 0; }

private:
	struct FParsedKit
	{
		int32 SchemaVersion = 1;
		FString Id;
		FString DisplayName;
		float Damage = 0.0f;
		FString SourceFileToStore;
	};

	static FString MakeSourcePathToStore(const FString& FullFilename);
	static FString ResolveSourcePath(const FString& StoredPath);
	static bool TryParseKitFile(const FString& FullFilename, FParsedKit& OutParsed, FString& OutError);
	static void ApplyParsedToAsset(class UHCIAbilityKitAsset* Asset, const FParsedKit& Parsed);
};