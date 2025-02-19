#pragma once

#include "GitLockProviderSettings.h"

#include "IGitLockProvider.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class UGitLockProvider : public UInterface
{
	GENERATED_BODY()
};

struct FGitFileLockOpParams
{
	FString GitBinaryPath;
	TArray<FString> CustomParams;
	bool bUseLocalCache;
	TArray<FString> FileNames;
};

class IGitLockProvider
{
	GENERATED_BODY()
public:
	virtual bool ConfigureWithSettings(const FGitLockProviderSettings& NewSettings, TArray<FString>& OutErrors) = 0;
	virtual bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot,
							   const FString& GitBinaryFallback, const TArray<FString>& InParameters,
							   const TArray<FString>& InFiles, TArray<FString>& OutResults,
							   TArray<FString>& OutErrorMessages) = 0;
	virtual bool GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
								TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) = 0;
	virtual bool LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
						   TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) = 0;
	virtual bool UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
							 TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) = 0;
	virtual bool CheckLockableExtensions(const FString& InPathToGitBinary, const FString& InRepositoryRoot,
										 const TArray<FString>& InFiles, TArray<FString>& OutLockableExtensions,
										 TArray<FString>& OutErrorMessages) = 0;
};

UCLASS(Abstract)
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

	bool GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
						TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) override
	{
		return false;
	}

	bool LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params, TArray<FString>& OutResults,
				   TArray<FString>& OutErrorMessages) override
	{
		return false;
	}

	bool UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params, TArray<FString>& OutResults,
					 TArray<FString>& OutErrorMessages) override
	{
		return false;
	}

	bool ConfigureWithSettings(const FGitLockProviderSettings& NewSettings, TArray<FString>& OutErrors) override
	{
		return true;
	}

	bool CheckLockableExtensions(const FString& InPathToGitBinary, const FString& InRepositoryRoot,
								 const TArray<FString>& InFiles, TArray<FString>& OutLockableExtensions,
								 TArray<FString>& OutErrorMessages) override
	{
		return true;
	}
};
