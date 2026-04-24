// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class IFileHandle;

/**
 * JSONL append-only log for one session.
 *
 * One file per session under `<Project>/Saved/AgentSessions/<SessionId>.jsonl`.
 * Each line is a standalone JSON object with at minimum:
 *   {
 *     "type":"<TypeTag>",
 *     "ts":"<ISO 8601>",
 *     "session":"<SessionId>",
 *     ...<payload fields>
 *   }
 *
 * Durability model:
 *   - Writes go through `IPlatformFile::OpenWrite(Path, /*append=*/true, /*allowRead=*/true)`
 *     so Windows shares the handle; other processes (VSCode tail, antivirus)
 *     do not block our appends.
 *   - Each AppendRecord flushes to disk before returning. A crash therefore
 *     may drop the last in-flight record but NEVER leaves a partial JSON
 *     line — the replay path relies on that invariant.
 *
 * Thread-safety: AppendRecord is serialized by an internal critical section;
 * multiple session ticks on the game thread remain consistent, but do not
 * call AppendRecord from worker threads.
 */
class VESSELEDITOR_API FVesselSessionLog
{
public:
	FVesselSessionLog();
	~FVesselSessionLog();

	/** Allocate a handle under Saved/AgentSessions/<SessionId>.jsonl. False on IO error. */
	bool Open(const FString& SessionId);

	/** Emit a structured record. Adds `type`, `ts`, `session` automatically. */
	void AppendRecord(const FString& TypeTag, const TSharedRef<FJsonObject>& Payload);

	/** Emit a raw line. Caller is responsible for trailing newline. Low-level escape hatch. */
	void AppendRawLine(const FString& Line);

	/** Flush pending writes to disk. Normally redundant — AppendRecord always flushes. */
	void Flush();

	/** Close the file. Safe to call multiple times. */
	void Close();

	bool IsOpen() const { return FileHandle != nullptr; }
	const FString& GetFilePath() const { return FilePath; }
	const FString& GetSessionId() const { return SessionId; }

	// Non-copyable, non-movable (holds an OS file handle).
	FVesselSessionLog(const FVesselSessionLog&) = delete;
	FVesselSessionLog& operator=(const FVesselSessionLog&) = delete;
	FVesselSessionLog(FVesselSessionLog&&) = delete;
	FVesselSessionLog& operator=(FVesselSessionLog&&) = delete;

	/** Return `<Project>/Saved/AgentSessions/<SessionId>.jsonl`. */
	static FString DefaultPathForSession(const FString& SessionId);

private:
	/** Write raw bytes via the owned IFileHandle under the internal lock. */
	void WriteBytesLocked(const uint8* Data, int64 NumBytes);

	IFileHandle* FileHandle = nullptr;
	FString FilePath;
	FString SessionId;
	FCriticalSection WriteLock;
};
