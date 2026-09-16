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
class UModioLockProvider : public UGitLockProviderBase
{
	GENERATED_BODY()
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> GetLocksRequest();

	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> BatchLockRequest(const FString& Endpoint,
																		 const FString& Username,
																		 const FString& ProjectName,
																		 const TArray<FString>& AssetPaths,
																		 bool bCreateProject);

	TUnion<FString, int32> PerformHttpRequest(TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> Request);

	TSharedPtr<FJsonObject> GetResponseAsJsonObject(const FString& ResponseString);
	TArray<TSharedPtr<FJsonValue>> GetResponseAsJsonArray(const FString& ResponseString);

	bool RunBatchLockOperation(const FString& Endpoint, bool bLocking, const FGitFileLockOpParams& Params,
							   TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
							   TArray<FString>& OutErrorMessages);

	bool ApplyBatchResponse(const FString& ResponseString, const TMap<FString, FString>& RequestedByAssetPath,
							bool bLocking, TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
							TArray<FString>& OutErrorMessages);

	void YieldThread();
	FString ServerAddress;

public:
	bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot, const FString& GitBinaryFallback,
					   const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults,
					   TArray<FString>& OutErrorMessages) override;

	bool GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
						TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) override;

	bool LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
				   TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
				   TArray<FString>& OutErrorMessages) override;

	bool UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
					 TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
					 TArray<FString>& OutErrorMessages) override;

	bool ConfigureWithSettings(const FGitLockProviderSettings& NewSettings, TArray<FString>& OutErrors) override;

	bool CheckLockableExtensions(const FString& InPathToGitBinary, const FString& InRepositoryRoot,
								 const TArray<FString>& InFiles, TArray<FString>& OutLockableExtensions,
								 TArray<FString>& OutErrorMessages) override;
};
