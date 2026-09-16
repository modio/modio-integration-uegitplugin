#pragma once

class IGitLockInfo
{
	virtual FString GetLockOwner() = 0;
	virtual FString GetLockPath() = 0;
};
