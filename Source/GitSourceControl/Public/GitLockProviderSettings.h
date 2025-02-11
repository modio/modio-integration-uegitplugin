// Copyright (c) 2014-2020 Sebastien Rombauts (sebastien.rombauts@gmail.com)
//
// Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
// or copy at http://opensource.org/licenses/MIT)

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"

#include "GitLockProviderSettings.generated.h"


USTRUCT()
struct FGitLockProviderSettings
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FString> SettingValues;
};
