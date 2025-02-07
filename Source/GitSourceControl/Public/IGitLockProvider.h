#pragma once


UINTERFACE(NotImplementableInBlueprint)
class UGitLockProvider
{
	GENERATED_BODY()
};

class IGitLockProvider
{
	public:
	virtual bool RunLFSCommand(const FString& InCommand, const FString& InRepositoryRoot, const FString& GitBinaryFallback, const TArray<FString>& InParameters, const TArray<FString>& InFiles, TArray<FString>& OutResults, TArray<FString>& OutErrorMessages) = 0;

};

UCLASS()
class UGitLockProviderBase : public UObject, public IGitLockProvider
{
	GENERATED_BODY()
};
