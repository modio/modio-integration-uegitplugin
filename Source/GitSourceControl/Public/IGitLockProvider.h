#pragma once

#include "IGitLockProvider.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class UGitLockProvider : public UInterface
{
	GENERATED_BODY()
};

class IGitLockProvider
{
	GENERATED_BODY()
public:
	virtual bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot,
							   const FString& GitBinaryFallback, const TArray<FString>& InParameters,
							   const TArray<FString>& InFiles, TArray<FString>& OutResults,
							   TArray<FString>& OutErrorMessages) = 0;
};

UCLASS()
class UGitLockProviderBase : public UObject, public IGitLockProvider
{
	GENERATED_BODY()
public:
	bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot, const FString& GitBinaryFallback,
					   const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults,
					   TArray<FString>& OutErrorMessages) override
	{
		return false;
	}
};
