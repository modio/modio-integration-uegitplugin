// Fill out your copyright notice in the Description page of Project Settings.

#include "ModioLockProvider.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GitMessageLog.h"
#include "GitSourceControlModule.h"
#include "HAL/PlatformProcess.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "ISourceControlModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

namespace ModioLockProviderConstants
{
	/** Endpoints on the ConBot FileLock controller. */
	static const TCHAR* LocksEndpoint = TEXT("/api/FileLock/lock");
	static const TCHAR* BatchLockEndpoint = TEXT("/api/FileLock/locks");
	static const TCHAR* BatchReleaseEndpoint = TEXT("/api/FileLock/locks/release");

	/** Per-asset outcomes returned by the batch endpoints. */
	static const TCHAR* StatusCreated = TEXT("Created");
	static const TCHAR* StatusAlreadyOwned = TEXT("AlreadyOwned");
	static const TCHAR* StatusConflict = TEXT("Conflict");
	static const TCHAR* StatusReleased = TEXT("Released");
	static const TCHAR* StatusNotLocked = TEXT("NotLocked");
	static const TCHAR* StatusNotOwned = TEXT("NotOwned");

	static constexpr float RequestTimeoutSeconds = 30.0f;

	/**
	 * The HTTP delegate completes on the HTTP thread, so the calling thread polls for it. One
	 * request now covers a whole batch, so this granularity costs a single sleep per operation.
	 */
	static constexpr float PollIntervalSeconds = 0.02f;

	/** Backstop in case the request never completes and never times out on its own. */
	static constexpr double WaitLimitSeconds = RequestTimeoutSeconds + 15.0;
}

namespace
{
	/**
	 * Server state is keyed by asset path, so every caller has to agree on one spelling. Unreal hands
	 * us '/'-separated relative paths in most cases but not all, and a '\' would additionally have to
	 * be escaped to survive the JSON body.
	 */
	FString NormalizeAssetPath(const FString& InPath)
	{
		FString Normalized = InPath;
		FPaths::NormalizeFilename(Normalized);
		return Normalized;
	}

	/** The project a lock belongs to. Locks are scoped per project on the server. */
	FString GetLockProjectName()
	{
		return FApp::GetProjectName();
	}

	/**
	 * Shared between the calling thread and the HTTP thread. Held by shared pointer so that a
	 * response arriving after we have given up writes into live memory rather than a dead stack frame.
	 */
	struct FModioLockHttpResult
	{
		/** Release-ordered by bComplete: only read after bComplete observes true. */
		TUnion<FString, int32> Value;
		std::atomic<bool> bComplete {false};
	};
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UModioLockProvider::GetLocksRequest()
{
	FHttpModule& HttpModule = FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
	FString RequestURL = ServerAddress + ModioLockProviderConstants::LocksEndpoint;
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(RequestURL);
	Request->SetTimeout(ModioLockProviderConstants::RequestTimeoutSeconds);
	return Request;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UModioLockProvider::BatchLockRequest(const FString& Endpoint,
																				   const FString& Username,
																				   const FString& ProjectName,
																				   const TArray<FString>& AssetPaths,
																				   bool bCreateProject)
{
	FHttpModule& HttpModule = FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(ServerAddress + Endpoint);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetTimeout(ModioLockProviderConstants::RequestTimeoutSeconds);

	// Serialized rather than formatted into a string: asset paths and usernames are not guaranteed
	// to be free of characters that need escaping, and a malformed body is a silent 400.
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("username"), Username);
	Body->SetStringField(TEXT("projectName"), ProjectName);
	Body->SetBoolField(TEXT("bCreateProject"), bCreateProject);

	TArray<TSharedPtr<FJsonValue>> PathValues;
	PathValues.Reserve(AssetPaths.Num());
	for (const FString& AssetPath : AssetPaths)
	{
		PathValues.Add(MakeShared<FJsonValueString>(AssetPath));
	}
	Body->SetArrayField(TEXT("assetPaths"), PathValues);

	FString RequestContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
	FJsonSerializer::Serialize(Body, Writer);
	Request->SetContentAsString(RequestContent);

	return Request;
}

TUnion<FString, int32> UModioLockProvider::PerformHttpRequest(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request)
{
	TSharedRef<FModioLockHttpResult, ESPMode::ThreadSafe> State = MakeShared<FModioLockHttpResult, ESPMode::ThreadSafe>();
	State->Value.SetSubtype<int32>(-1);

	Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
	Request->OnProcessRequestComplete().BindLambda(
		[State](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully) {
			// need to check validity of request and response here
			if (!Request.IsValid() || !Response.IsValid())
			{
				State->Value.SetSubtype<int32>(404);
				State->bComplete.store(true, std::memory_order_release);
				return;
			}
			FTSMessageLog("SourceControl")
				.Info(FText::FromString(
					FString::Format(TEXT("Request {0} {1} received response code {2}"),
									{Request->GetVerb(), *Request->GetURL(), Response->GetResponseCode()})));
			if (Response->GetResponseCode() == 200)
			{
				State->Value.SetSubtype<FString>(Response->GetContentAsString());
			}
			else
			{
				State->Value.SetSubtype<int32>(Response->GetResponseCode());
			}
			// The release store pairs with the acquire load below
			State->bComplete.store(true, std::memory_order_release);
		});

	if (!Request->ProcessRequest())
	{
		// The request was rejected before it started, so the completion delegate will never fire.
		TUnion<FString, int32> Failed;
		Failed.SetSubtype<int32>(-1);
		return Failed;
	}

	const double Deadline = FPlatformTime::Seconds() + ModioLockProviderConstants::WaitLimitSeconds;
	while (!State->bComplete.load(std::memory_order_acquire))
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Request->CancelRequest();
			TUnion<FString, int32> TimedOut;
			TimedOut.SetSubtype<int32>(408);
			// State is shared, so a late completion writes somewhere still alive.
			return TimedOut;
		}
		FPlatformProcess::Sleep(ModioLockProviderConstants::PollIntervalSeconds);
	}

	return State->Value;
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

bool UModioLockProvider::RunBatchLockOperation(const FString& Endpoint, bool bLocking,
											   const FGitFileLockOpParams& Params, TArray<FString>& OutSucceededFiles,
											   TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	if (Params.FileNames.Num() == 0)
	{
		// Nothing to do, and the endpoints reject an empty batch.
		return true;
	}

	// The server echoes the path it was sent, so keep a way back to the caller's spelling: callers
	// use the returned entries to key their own caches and must get their own strings back.
	TMap<FString, FString> RequestedByAssetPath;
	TArray<FString> AssetPaths;
	RequestedByAssetPath.Reserve(Params.FileNames.Num());
	AssetPaths.Reserve(Params.FileNames.Num());
	for (const FString& FileName : Params.FileNames)
	{
		const FString AssetPath = NormalizeAssetPath(FileName);
		if (!RequestedByAssetPath.Contains(AssetPath))
		{
			RequestedByAssetPath.Add(AssetPath, FileName);
			AssetPaths.Add(AssetPath);
		}
	}

	const FString ProjectName = GetLockProjectName();
	const FString Username = FGitSourceControlModule::Get().GetProvider().GetLockUser();

	auto Result = PerformHttpRequest(
		BatchLockRequest(Endpoint, Username, ProjectName, AssetPaths, /*bCreateProject=*/bLocking));

	if (Result.GetCurrentSubtypeIndex() != 0)
	{
		OutErrorMessages.Add(FString::Format(TEXT("Request to {0} {1} file(s) resulted in HTTP error {2}"),
											 {bLocking ? TEXT("lock") : TEXT("unlock"), AssetPaths.Num(),
											  Result.GetSubtype<int32>()}));
		return false;
	}

	return ApplyBatchResponse(Result.GetSubtype<FString>(), RequestedByAssetPath, bLocking, OutSucceededFiles,
							  OutResults, OutErrorMessages);
}

bool UModioLockProvider::ApplyBatchResponse(const FString& ResponseString,
											const TMap<FString, FString>& RequestedByAssetPath, bool bLocking,
											TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
											TArray<FString>& OutErrorMessages)
{
	TSharedPtr<FJsonObject> ResponseObject = GetResponseAsJsonObject(ResponseString);
	if (!ResponseObject.IsValid())
	{
		OutErrorMessages.Add(TEXT("Could not parse the response from the lock server"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
	if (!ResponseObject->TryGetArrayField(TEXT("results"), Results) || Results == nullptr)
	{
		OutErrorMessages.Add(TEXT("Lock server response did not contain any per-asset results"));
		return false;
	}

	bool bAllSucceeded = true;
	for (const TSharedPtr<FJsonValue>& Entry : *Results)
	{
		const TSharedPtr<FJsonObject> EntryObject = Entry.IsValid() ? Entry->AsObject() : nullptr;
		if (!EntryObject.IsValid())
		{
			continue;
		}

		FString AssetPath;
		FString Status;
		EntryObject->TryGetStringField(TEXT("assetPath"), AssetPath);
		EntryObject->TryGetStringField(TEXT("status"), Status);

		FString LockedBy;
		EntryObject->TryGetStringField(TEXT("lockedBy"), LockedBy);

		// Hand back the caller's own string; an unrecognised path means the server echoed something
		// we did not send, so fall back to what it told us rather than dropping the entry.
		const FString* Requested = RequestedByAssetPath.Find(NormalizeAssetPath(AssetPath));
		const FString ReportedFile = Requested != nullptr ? *Requested : AssetPath;

		const bool bSucceeded =
			bLocking ? (Status == ModioLockProviderConstants::StatusCreated ||
						Status == ModioLockProviderConstants::StatusAlreadyOwned)
					 : (Status == ModioLockProviderConstants::StatusReleased ||
						Status == ModioLockProviderConstants::StatusNotLocked);

		if (bSucceeded)
		{
			OutSucceededFiles.Add(ReportedFile);
			OutResults.Add(FString::Format(TEXT("{0}\t{1}"), {*ReportedFile, *Status}));
			continue;
		}

		bAllSucceeded = false;
		if (Status == ModioLockProviderConstants::StatusConflict ||
			Status == ModioLockProviderConstants::StatusNotOwned)
		{
			OutErrorMessages.Add(FString::Format(TEXT("{0} is locked by {1}"),
												 {*ReportedFile, LockedBy.IsEmpty() ? TEXT("another user") : *LockedBy}));
		}
		else
		{
			OutErrorMessages.Add(FString::Format(TEXT("Could not {0} {1}: the lock server reported '{2}'"),
												 {bLocking ? TEXT("lock") : TEXT("unlock"), *ReportedFile, *Status}));
		}
	}

	return bAllSucceeded;
}

bool UModioLockProvider::LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
								   TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
								   TArray<FString>& OutErrorMessages)
{
	return RunBatchLockOperation(ModioLockProviderConstants::BatchLockEndpoint, /*bLocking=*/true, Params,
								 OutSucceededFiles, OutResults, OutErrorMessages);
}

bool UModioLockProvider::UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
									 TArray<FString>& OutSucceededFiles, TArray<FString>& OutResults,
									 TArray<FString>& OutErrorMessages)
{
	return RunBatchLockOperation(ModioLockProviderConstants::BatchReleaseEndpoint, /*bLocking=*/false, Params,
								 OutSucceededFiles, OutResults, OutErrorMessages);
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

bool UModioLockProvider::CheckLockableExtensions(const FString& InPathToGitBinary, const FString& InRepositoryRoot,
												 const TArray<FString>& InFiles, TArray<FString>& OutLockableExtensions,
												 TArray<FString>& OutErrorMessages)
{
	for (const FString& Extension : InFiles)
	{
		if (Extension.StartsWith(TEXT("*")))
		{
			OutLockableExtensions.Add(Extension.Mid(1));
		}
	}
	return true;
}
