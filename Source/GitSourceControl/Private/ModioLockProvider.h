// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/Union.h"
#include "CoreMinimal.h"
#include "IGitLockProvider.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"

#include "ModioLockProvider.generated.h"

/**
 *
 */
UCLASS()
class UModioLockProvider : public UObject, public IGitLockProvider
{
	GENERATED_BODY()
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> GetLocksRequest();
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> LockFileRequest(const FString& Username,
																		const FString& FilePath,
																		const FString& ProjectName);
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> UnlockFileRequest(const FString& Username,
																		  const FString& FilePath,
																		  const FString& ProjectName);

	TUnion<FString, int32> PerformHttpRequest(TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> Request);

	TSharedPtr<FJsonObject> GetResponseAsJsonObject(const FString& ResponseString);
	TArray<TSharedPtr<FJsonValue>> GetResponseAsJsonArray(const FString& ResponseString);

	void YieldThread();

public:
	bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot, const FString& GitBinaryFallback,
					   const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults,
					   TArray<FString>& OutErrorMessages) override;

	bool GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
						TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) override;

	bool LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params, TArray<FString>& OutResults,
				   TArray<FString>& OutErrorMessages) override;

	bool UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params, TArray<FString>& OutResults,
					 TArray<FString>& OutErrorMessages) override;
};
