#pragma once

#include "IGitLockProvider.h"

#include "LFSLockProvider.generated.h"

UCLASS()
class ULFSLockProvider : public UGitLockProviderBase
{
	GENERATED_BODY()
public:
	virtual bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot,
							   const FString& GitBinaryFallback, const TArray<FString>& InParameters,
							   const TArray<FString>& InFiles, TArray<FString>& OutResults,
							   TArray<FString>& OutErrorMessages) override;
};
