// Fill out your copyright notice in the Description page of Project Settings.

#include "ModioLockProvider.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "GitSourceControlModule.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "ISourceControlModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Optional.h"

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UModioLockProvider::GetLocksRequest()
{
	FHttpModule& HttpModule = FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
	FString RequestURL = FString("ServerIP") + TEXT("/api/FileLock/lock");
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(RequestURL);
	return Request;
}

TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> UModioLockProvider::LockFileRequest(const FString& Username,
																						const FString& FilePath,
																						const FString& ProjectName)
{
	FHttpModule& HttpModule = FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
	FString RequestURL = FString("ServerIP") + TEXT("/api/FileLock/lock");
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(RequestURL);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	FString RequestContent = FString::Format(TEXT("{\"username\": \"{0}\", \"assetPath\": \"{1}\", \"projectName\": "
												  "\"{2}\", \"bCreateProject\": \"true\" }"),
											 {*Username, *FilePath, *ProjectName});
	Request->SetContentAsString(RequestContent);
	return Request;
}

TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> UModioLockProvider::UnlockFileRequest(const FString& Username,
																						  const FString& FilePath,
																						  const FString& ProjectName)
{
	FHttpModule& HttpModule = FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
	FString RequestURL = FString("ServerIP") + TEXT("/api/FileLock/lock");
	Request->SetVerb(TEXT("DELETE"));
	Request->SetURL(RequestURL);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	FString RequestContent = FString::Format(TEXT("{\"username\": \"{0}\", \"assetPath\": \"{1}\", \"projectName\": "
												  "\"{2}\" }"),
											 {*Username, *FilePath, *ProjectName});
	Request->SetContentAsString(RequestContent);
	return Request;
}

TUnion<FString, EHttpRequestStatus::Type> UModioLockProvider::PerformHttpRequest(
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request)
{
	TUnion<FString, EHttpRequestStatus::Type> Result;
	bool bRequestDone = false;
	Request->OnProcessRequestComplete().BindLambda(
		[&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully) {
			if (bConnectedSuccessfully)
			{
				Result.SetSubtype<FString>(Response->GetContentAsString());
			}
			else
			{
				Result.SetSubtype<EHttpRequestStatus::Type>(Request->GetStatus());
			}
		});
	Request->ProcessRequest();
	while (!bRequestDone)
	{
		YieldThread();
	}
	return Result;
}

void UModioLockProvider::YieldThread()
{
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
#if UE_VERSION_OLDER_THAN(5, 3, 0)
	FTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
#else
	FTSTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
#endif
	FSlateApplication::Get().PumpMessages();
	FSlateApplication::Get().Tick();
	FPlatformProcess::Sleep(0);
}

bool UModioLockProvider::RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot,
									   const FString& GitBinaryFallback, const TArray<FString>& InParameters,
									   const TArray<FString>& InFiles, TArray<FString>& OutResults,
									   TArray<FString>& OutErrorMessages)
{
	UE_LOG(LogSourceControl, Display, TEXT("Raw LFS command invoked on provider which does not support it"));
	return false;
}

bool UModioLockProvider::GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
										TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	auto Result = PerformHttpRequest(GetLocksRequest());
	if (Result.GetCurrentSubtypeIndex() == 0)
	{
		// deserialize here
		return true;
	}
	else
	{
		return false;
	}
}

bool UModioLockProvider::LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
								   TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	auto Result = PerformHttpRequest(LockFileRequest(FGitSourceControlModule::Get().GetProvider().GetLockUser(),
													 Params.FileNames[0], FApp::GetProjectName()));
	if (Result.GetCurrentSubtypeIndex() == 0)
	{
		// deserialize here
		return true;
	}
	else
	{
		return false;
	}
}

bool UModioLockProvider::UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
									 TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	auto Result = PerformHttpRequest(UnlockFileRequest(FGitSourceControlModule::Get().GetProvider().GetLockUser(),
													   Params.FileNames[0], FApp::GetProjectName()));
	if (Result.GetCurrentSubtypeIndex() == 0)
	{
		// deserialize here
		return true;
	}
	else
	{
		return false;
	}
}
