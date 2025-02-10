#include "LFSLockProvider.h"
#include "GitSourceControlUtils.h"

bool ULFSLockProvider::RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot,
									 const FString& GitBinaryFallback, const TArray<FString>& InParameters,
									 const TArray<FString>& InFiles, TArray<FString>& OutResults,
									 TArray<FString>& OutErrorMessages)
{
	FString Command = InCommand;
#if GIT_USE_CUSTOM_LFS
	FString BaseDir = IPluginManager::Get().FindPlugin("GitSourceControl")->GetBaseDir();
	#if PLATFORM_WINDOWS
	FString LFSLockBinary = FString::Printf(TEXT("%s/git-lfs.exe"), *BaseDir);
	#elif PLATFORM_MAC
		#if ENGINE_MAJOR_VERSION >= 5
			#if PLATFORM_MAC_ARM64
	FString LFSLockBinary = FString::Printf(TEXT("%s/git-lfs-mac-arm64"), *BaseDir);
			#else
	FString LFSLockBinary = FString::Printf(TEXT("%s/git-lfs-mac-amd64"), *BaseDir);
			#endif
		#else
	FString LFSLockBinary = FString::Printf(TEXT("%s/git-lfs-mac-amd64"), *BaseDir);
		#endif
	#elif PLATFORM_LINUX
	FString LFSLockBinary = FString::Printf(TEXT("%s/git-lfs"), *BaseDir);
	#else
	ensureMsgf(false, TEXT("Unhandled platform for LFS binary!"));
	const FString& LFSLockBinary = GitBinaryFallback;
	Command = TEXT("lfs ") + Command;
	#endif
#else
	const FString& LFSLockBinary = GitBinaryFallback;
	Command = TEXT("lfs ") + Command;
#endif

	return GitSourceControlUtils::RunCommand(Command, LFSLockBinary, InRepositoryRoot, InParameters, InFiles,
											 OutResults, OutErrorMessages);
}

bool ULFSLockProvider::GetLockedFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
									  TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	return RunLFSCommand(TEXT("locks"), InRepositoryRoot, Params.GitBinaryPath, Params.CustomParams, Params.FileNames,
						 OutResults, OutErrorMessages);
}

bool ULFSLockProvider::LockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
								 TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	return RunLFSCommand(TEXT("lock"), InRepositoryRoot, Params.GitBinaryPath, Params.CustomParams, Params.FileNames,
						 OutResults, OutErrorMessages);
}

bool ULFSLockProvider::UnlockFiles(const FString& InRepositoryRoot, const FGitFileLockOpParams& Params,
								   TArray<FString>& OutResults, TArray<FString>& OutErrorMessages)
{
	return RunLFSCommand(TEXT("unlock"), InRepositoryRoot, Params.GitBinaryPath, Params.CustomParams, Params.FileNames,
						 OutResults, OutErrorMessages);
}
