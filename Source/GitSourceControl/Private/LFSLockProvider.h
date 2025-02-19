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

	virtual bool GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
								TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) override;

	virtual bool LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
						   TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) override;

	virtual bool UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
							 TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) override;

	bool CheckLockableExtensions(const FString& InPathToGitBinary, const FString& InRepositoryRoot,
								 const TArray<FString>& InFiles, TArray<FString>& OutLockableExtensions,
								 TArray<FString>& OutErrorMessages) override;
};
