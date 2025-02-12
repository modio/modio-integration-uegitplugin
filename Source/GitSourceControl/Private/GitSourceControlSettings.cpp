// Copyright (c) 2014-2020 Sebastien Rombauts (sebastien.rombauts@gmail.com)
//
// Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
// or copy at http://opensource.org/licenses/MIT)

#include "GitSourceControlSettings.h"

#include "Misc/ConfigCacheIni.h"
#include "SourceControlHelpers.h"

namespace GitSettingsConstants
{

	/** The section of the ini file we load our settings from */
	static const FString SettingsSection = TEXT("GitSourceControl.GitSourceControlSettings");
	static const FString LockProviderSettingsSection = TEXT("GitSourceControl.LockProviderSettings");

} // namespace GitSettingsConstants

const FString& FGitSourceControlSettings::GetBinaryPath() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return BinaryPath; // Return a copy to be thread-safe
}

bool FGitSourceControlSettings::SetBinaryPath(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (BinaryPath != InString);
	if (bChanged)
	{
		BinaryPath = InString;
	}
	return bChanged;
}

/** Tell if using the Git LFS file Locking workflow */
bool FGitSourceControlSettings::IsUsingGitLfsLocking() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return bUsingGitLfsLocking;
}

/** Configure the usage of Git LFS file Locking workflow */
bool FGitSourceControlSettings::SetUsingGitLfsLocking(const bool InUsingGitLfsLocking)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (bUsingGitLfsLocking != InUsingGitLfsLocking);
	bUsingGitLfsLocking = InUsingGitLfsLocking;
	return bChanged;
}

const FString FGitSourceControlSettings::GetLfsUserName() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return LfsUserName; // Return a copy to be thread-safe
}

bool FGitSourceControlSettings::SetLfsUserName(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (LfsUserName != InString);
	if (bChanged)
	{
		LfsUserName = InString;
	}
	return bChanged;
}

const TSoftClassPtr<class UGitLockProviderBase> FGitSourceControlSettings::GetLockProviderClass() const
{
	return LockProviderClass;
}

const FGitLockProviderSettings& FGitSourceControlSettings::GetLockProviderSettings() const
{
	return CurrentLockProviderSettings;
}

bool FGitSourceControlSettings::SetLockProviderClass(TSoftClassPtr<class UGitLockProviderBase> Provider)
{
	LockProviderClass = Provider;
	return true;
}

// This is called at startup nearly before anything else in our module: BinaryPath will then be used by the provider
void FGitSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->GetString(*GitSettingsConstants::SettingsSection, TEXT("BinaryPath"), BinaryPath, IniFile);
	GConfig->GetBool(*GitSettingsConstants::SettingsSection, TEXT("UsingGitLfsLocking"), bUsingGitLfsLocking, IniFile);
	GConfig->GetString(*GitSettingsConstants::SettingsSection, TEXT("LfsUserName"), LfsUserName, IniFile);
	FString LockProviderClassPath;
	GConfig->GetString(*GitSettingsConstants::SettingsSection, TEXT("LockProviderClass"), LockProviderClassPath,
					   IniFile);
	if (LockProviderClassPath.IsEmpty())
	{
		LockProviderClassPath = TEXT("/Script/GitSourceControl.LFSLockProvider");
	}
	LockProviderClass = TSoftClassPtr<class UGitLockProviderBase> {LockProviderClassPath};
	FConfigSection* LockProviderSettings =
		GConfig->GetSectionPrivate(*GitSettingsConstants::LockProviderSettingsSection, false, true, IniFile);
	if (LockProviderSettings)
	{
		for (const auto& Value : *LockProviderSettings)
		{
			CurrentLockProviderSettings.SettingValues.Add(Value.Key.ToString(), Value.Value.GetValue());
		}
	}
}

void FGitSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->SetString(*GitSettingsConstants::SettingsSection, TEXT("BinaryPath"), *BinaryPath, IniFile);
	GConfig->SetBool(*GitSettingsConstants::SettingsSection, TEXT("UsingGitLfsLocking"), bUsingGitLfsLocking, IniFile);
	GConfig->SetString(*GitSettingsConstants::SettingsSection, TEXT("LfsUserName"), *LfsUserName, IniFile);
	GConfig->SetString(*GitSettingsConstants::SettingsSection, TEXT("LockProviderClass"), *LockProviderClass.ToString(),
					   IniFile);
	FConfigSection* LockProviderSettings =
		GConfig->GetSectionPrivate(*GitSettingsConstants::LockProviderSettingsSection, true, false, IniFile);
	{
		for (const auto& Value : CurrentLockProviderSettings.SettingValues)
		{
			LockProviderSettings->Add(FName(Value.Key), Value.Value);
		}
	}
	GConfig->Flush(false, IniFile);
}
