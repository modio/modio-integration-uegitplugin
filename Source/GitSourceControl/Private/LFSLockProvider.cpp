#include "LFSLockProvider.h"

bool ULFSLockProvider::RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot,
									 const FString& GitBinaryFallback, const TArray<FString>& InParameters,
									 const TArray<FString>& InFiles, TArray<FString>& OutResults,
									 TArray<FString>& OutErrorMessages)
{
	return false;
}
