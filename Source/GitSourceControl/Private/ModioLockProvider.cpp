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
#include "Serialization/JsonSerializer.h"

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UModioLockProvider::GetLocksRequest()
{
	FHttpModule& HttpModule = FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
	FString RequestURL = ServerAddress + TEXT("/api/FileLock/lock");
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
	FString RequestURL = ServerAddress + TEXT("/api/FileLock/lock");
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
	FString RequestURL = ServerAddress + TEXT("/api/FileLock/lock");
	Request->SetVerb(TEXT("DELETE"));
	Request->SetURL(RequestURL);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	FString RequestContent = FString::Format(TEXT("{\"username\": \"{0}\", \"assetPath\": \"{1}\", \"projectName\": "
												  "\"{2}\" }"),
											 {*Username, *FilePath, *ProjectName});
	Request->SetContentAsString(RequestContent);
	return Request;
}

TUnion<FString, int32> UModioLockProvider::PerformHttpRequest(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request)
{
	TUnion<FString, int32> Result;
	Result.SetSubtype<int32>(-1);
	volatile bool bRequestDone = false;
	Request->OnProcessRequestComplete().BindLambda(
		[&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully) {
			if (Response->GetResponseCode() == 200)
			{
				Result.SetSubtype<FString>(Response->GetContentAsString());
			}
			else
			{
				Result.SetSubtype<int32>(Response->GetResponseCode());
			}
			bRequestDone = true;
		});
	Request->ProcessRequest();
	while (!bRequestDone)
	{
		YieldThread();
	}
	return Result;
}

TSharedPtr<FJsonObject> UModioLockProvider::GetResponseAsJsonObject(const FString& ResponseString)
{
	TSharedPtr<FJsonStringReader> TopLevelJson = FJsonStringReader::Create(ResponseString);
	TSharedPtr<FJsonObject> ParsedResponse;
	if (!FJsonSerializer::Deserialize<TCHAR>(*TopLevelJson, ParsedResponse, FJsonSerializer::EFlags::None))
	{
		return nullptr;
	}
	else
	{
		return ParsedResponse;
	}
}

TArray<TSharedPtr<FJsonValue>> UModioLockProvider::GetResponseAsJsonArray(const FString& ResponseString)
{
	TSharedPtr<FJsonStringReader> TopLevelJson = FJsonStringReader::Create(ResponseString);
	TArray<TSharedPtr<FJsonValue>> ParsedResponse;
	if (!FJsonSerializer::Deserialize<TCHAR>(*TopLevelJson, ParsedResponse, FJsonSerializer::EFlags::None))
	{
		return {};
	}
	else
	{
		return ParsedResponse;
	}
}

void UModioLockProvider::YieldThread()
{
	/*if (FTaskGraphInterface::Get().GetCurrentThread() == ENamedThreads::GameThread)
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
	}*/
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
		TArray<TSharedPtr<FJsonValue>> ResponseJSON = GetResponseAsJsonArray(Result.GetSubtype<FString>());

		for (const auto& Element : ResponseJSON)
		{
			const TSharedPtr<FJsonObject>& ElementAsObject = Element->AsObject();
			OutResults.Add(FString::Format(TEXT("{0}\t{1}\t{2}"), {ElementAsObject->GetStringField("assetPath"),
																   ElementAsObject->GetStringField("username"),
																   ElementAsObject->GetStringField("id")}));
			// deserialize here
		}
		return true;
	}
	else
	{
		OutErrorMessages.Add(FString::Format(TEXT("Request for file lock list resulted in HTTP error {0}"),
											 {Result.GetSubtype<int32>()}));
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
		OutErrorMessages.Add(FString::Format(TEXT("Request to lock file {0} resulted in HTTP error {1}"),
											 {*Params.FileNames[0], Result.GetSubtype<int32>()}));
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
		OutErrorMessages.Add(FString::Format(TEXT("Request to unlock file {0} resulted in HTTP error {1}"),
											 {*Params.FileNames[0], Result.GetSubtype<int32>()}));
		return false;
	}
}

bool UModioLockProvider::ConfigureWithSettings(const FGitLockProviderSettings& NewSettings, TArray<FString>& OutErrors)
{
	if (NewSettings.SettingValues.Contains("ServerAddress"))
	{
		if (NewSettings.SettingValues.Contains("ServerPort"))
		{
			ServerAddress =
				FString::Format(TEXT("https://{0}:{1}"), {NewSettings.SettingValues["ServerAddress"].TrimStartAndEnd(),
														  NewSettings.SettingValues["ServerPort"].TrimStartAndEnd()})
					.TrimStartAndEnd();
			return true;
		}
		else
		{
			OutErrors.Add("ServerPort setting missing");
		}
	}
	else
	{
		OutErrors.Add("ServerAddress setting missing");
	}
	return false;
}
