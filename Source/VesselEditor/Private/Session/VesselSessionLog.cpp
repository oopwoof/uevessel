// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselSessionLog.h"

#include "VesselLog.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

FVesselSessionLog::FVesselSessionLog() = default;

FVesselSessionLog::~FVesselSessionLog()
{
	Close();
}

FString FVesselSessionLog::DefaultPathForSession(const FString& InSessionId)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("AgentSessions");
	return FPaths::ConvertRelativePathToFull(Dir / (InSessionId + TEXT(".jsonl")));
}

bool FVesselSessionLog::Open(const FString& InSessionId)
{
	Close();

	if (InSessionId.IsEmpty())
	{
		UE_LOG(LogVesselSession, Warning, TEXT("SessionLog: cannot open with empty session id"));
		return false;
	}

	SessionId = InSessionId;
	FilePath = DefaultPathForSession(SessionId);

	// Ensure directory exists (CreateDirectoryTree is idempotent).
	const FString Dir = FPaths::GetPath(FilePath);
	IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Append mode + allow read so VSCode / tail / antivirus can observe without blocking appends.
	// Gemini 3 Pro Preview flagged this exact hazard during review — see SESSION_MACHINE.md §8.5.
	FileHandle = PlatformFile.OpenWrite(
		*FilePath,
		/*bAppend=*/    true,
		/*bAllowRead=*/ true);

	if (!FileHandle)
	{
		UE_LOG(LogVesselSession, Error,
			TEXT("SessionLog: failed to open '%s' for append"), *FilePath);
		return false;
	}

	UE_LOG(LogVesselSession, Log,
		TEXT("SessionLog opened: %s (session=%s)"), *FilePath, *SessionId);
	return true;
}

void FVesselSessionLog::WriteBytesLocked(const uint8* Data, int64 NumBytes)
{
	// Caller must hold WriteLock.
	if (!FileHandle || NumBytes <= 0)
	{
		return;
	}
	FileHandle->Write(Data, NumBytes);
	FileHandle->Flush();
}

void FVesselSessionLog::AppendRecord(const FString& TypeTag, const TSharedRef<FJsonObject>& Payload)
{
	// Clone payload and stamp header fields.
	TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
	Record->SetStringField(TEXT("type"), TypeTag);
	Record->SetStringField(TEXT("ts"), FDateTime::UtcNow().ToIso8601());
	Record->SetStringField(TEXT("session"), SessionId);
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Payload->Values)
	{
		Record->SetField(Pair.Key, Pair.Value);
	}

	FString Line;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
	FJsonSerializer::Serialize(Record, Writer);
	Line.AppendChar(TEXT('\n'));

	AppendRawLine(Line);
}

void FVesselSessionLog::AppendRawLine(const FString& Line)
{
	FScopeLock Guard(&WriteLock);
	if (!FileHandle)
	{
		return;
	}

	FTCHARToUTF8 Converter(*Line);
	WriteBytesLocked(
		reinterpret_cast<const uint8*>(Converter.Get()),
		static_cast<int64>(Converter.Length()));
}

void FVesselSessionLog::Flush()
{
	FScopeLock Guard(&WriteLock);
	if (FileHandle)
	{
		FileHandle->Flush();
	}
}

void FVesselSessionLog::Close()
{
	FScopeLock Guard(&WriteLock);
	if (FileHandle)
	{
		FileHandle->Flush();
		delete FileHandle;
		FileHandle = nullptr;
		UE_LOG(LogVesselSession, Log, TEXT("SessionLog closed: %s"), *FilePath);
	}
}
