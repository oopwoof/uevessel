// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Session/VesselGuidesLoader.h"

#include "VesselLog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace VesselGuidesDetail
{
	static const TCHAR* kRejectionsHeader = TEXT("## Known Rejections");

	/**
	 * Hard cap on AGENTS.md size before we'll load it. Designers may paste
	 * large blobs into the user-edited preamble; without a cap a 100MB
	 * misclick blocks the Game Thread on first session boot.
	 */
	static constexpr int64 kMaxAgentsMdBytes = 1 * 1024 * 1024; // 1 MiB

	/** Pull the "tool=Foo · target=Bar" tokens out of a "### ..." entry header line. */
	static void ParseHeaderLine(const FString& Line, FString& OutTool, FString& OutTarget)
	{
		// Header looks like:
		//   "### 2026-04-25T... · tool=WriteDataTableRow · target=/Game/X"
		// or without target:
		//   "### 2026-04-25T... · tool=WriteDataTableRow"
		const int32 ToolIdx = Line.Find(TEXT("tool="));
		if (ToolIdx == INDEX_NONE)
		{
			return;
		}
		const FString After = Line.Mid(ToolIdx + 5); // skip "tool="
		// Extract up to next " · " or end of line.
		const int32 SepIdx = After.Find(TEXT(" · "));
		OutTool = (SepIdx == INDEX_NONE) ? After : After.Left(SepIdx);
		OutTool.TrimStartAndEndInline();

		const int32 TargetIdx = Line.Find(TEXT("target="));
		if (TargetIdx != INDEX_NONE)
		{
			OutTarget = Line.Mid(TargetIdx + 7); // skip "target="
			OutTarget.TrimStartAndEndInline();
		}
	}

	/** Pull the reason text out of a "**reason**: ..." line. */
	static FString ParseReasonLine(const FString& Line)
	{
		// "**reason**: <text>"
		const int32 Idx = Line.Find(TEXT("**reason**:"));
		if (Idx == INDEX_NONE)
		{
			return FString();
		}
		FString Text = Line.Mid(Idx + 11); // skip "**reason**:"
		Text.TrimStartAndEndInline();
		return Text;
	}
}

FString FVesselGuidesLoader::GetAgentsMdPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("AGENTS.md"));
}

FString FVesselGuidesLoader::ReadAgentsMd()
{
	const FString Path = GetAgentsMdPath();

	const int64 Size = IFileManager::Get().FileSize(*Path);
	if (Size < 0)
	{
		return FString(); // file does not exist
	}
	if (Size > VesselGuidesDetail::kMaxAgentsMdBytes)
	{
		UE_LOG(LogVesselHITL, Warning,
			TEXT("AGENTS.md exceeds %lld byte cap (%lld bytes) — skipping guides "
			     "injection. Trim the file or split history into archive JSONL."),
			VesselGuidesDetail::kMaxAgentsMdBytes, Size);
		return FString();
	}

	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *Path))
	{
		return FString();
	}
	return Contents;
}

void FVesselGuidesLoader::SplitPreambleAndRejections(
	const FString& Full,
	FString& OutPreamble,
	FString& OutRejectionsSection)
{
	OutPreamble.Reset();
	OutRejectionsSection.Reset();
	if (Full.IsEmpty())
	{
		return;
	}

	const int32 HeaderIdx = Full.Find(VesselGuidesDetail::kRejectionsHeader);
	if (HeaderIdx == INDEX_NONE)
	{
		// Whole file is preamble — no rejections recorded yet.
		OutPreamble = Full;
		return;
	}
	OutPreamble = Full.Left(HeaderIdx);
	OutRejectionsSection = Full.Mid(HeaderIdx);
	OutPreamble.TrimEndInline();
}

TArray<FVesselGuidesLoader::FRejectionEntry> FVesselGuidesLoader::ParseRejections(
	const FString& RejectionsSection,
	int32 MaxEntries)
{
	TArray<FRejectionEntry> Out;
	if (RejectionsSection.IsEmpty())
	{
		return Out;
	}

	TArray<FString> Lines;
	RejectionsSection.ParseIntoArrayLines(Lines, /*CullEmpty*/ false);

	// State machine:
	//   • "### " line  → flush previous entry, parse this one's header.
	//   • "**reason**: <text>" line → seed reason, enter ReasonAccumulating.
	//   • In ReasonAccumulating:
	//       — "**xxx**:" (any other bold marker, e.g. **session**, **rejecter**)
	//         → exit accumulator, ignore this line.
	//       — "### " (next entry header) → exit accumulator, fall through.
	//       — blank or any other line → append to Current.Reason as continuation.
	//   • Outside ReasonAccumulating, non-### / non-reason lines are skipped.
	FRejectionEntry Current;
	bool bInEntry = false;
	bool bAccumulatingReason = false;

	auto Flush = [&]()
	{
		if (bInEntry && (!Current.Tool.IsEmpty() || !Current.Reason.IsEmpty()))
		{
			Current.Reason.TrimEndInline();
			Out.Add(Current);
		}
		Current = FRejectionEntry{};
		bInEntry = false;
		bAccumulatingReason = false;
	};

	auto IsBoldMarker = [](const FString& Line)
	{
		return Line.StartsWith(TEXT("**")) && Line.Contains(TEXT("**:"));
	};

	for (const FString& Raw : Lines)
	{
		FString Line = Raw;
		Line.TrimStartAndEndInline();

		if (Line.StartsWith(TEXT("### ")))
		{
			Flush();
			bInEntry = true;
			VesselGuidesDetail::ParseHeaderLine(Line, Current.Tool, Current.Target);
			continue;
		}
		if (!bInEntry)
		{
			continue;
		}

		if (Line.StartsWith(TEXT("**reason**:")))
		{
			Current.Reason = VesselGuidesDetail::ParseReasonLine(Line);
			bAccumulatingReason = true;
			continue;
		}

		if (bAccumulatingReason)
		{
			if (IsBoldMarker(Line))
			{
				// Hit **session**: / **rejecter**: / etc — reason ends here.
				bAccumulatingReason = false;
				continue;
			}
			// Continuation line — preserve newline so multi-paragraph
			// reasons read naturally in the LLM prompt.
			if (!Current.Reason.IsEmpty())
			{
				Current.Reason += TEXT("\n");
			}
			Current.Reason += Line;
		}
	}
	Flush();

	if (Out.Num() > MaxEntries)
	{
		Out.SetNum(MaxEntries);
	}
	return Out;
}

FString FVesselGuidesLoader::BuildProjectGuidesBlock(
	int32 MaxRejections, int32 MaxPreambleChars)
{
	const FString Full = ReadAgentsMd();
	if (Full.IsEmpty())
	{
		return FString();
	}

	FString Preamble, RejectionsSection;
	SplitPreambleAndRejections(Full, Preamble, RejectionsSection);

	// Trim "# AGENTS.md" header line off the preamble — it's noise to the LLM.
	if (Preamble.StartsWith(TEXT("# AGENTS.md")))
	{
		const int32 NewlineIdx = Preamble.Find(TEXT("\n"));
		if (NewlineIdx != INDEX_NONE)
		{
			Preamble = Preamble.Mid(NewlineIdx + 1).TrimStart();
		}
		else
		{
			Preamble.Reset();
		}
	}

	if (Preamble.Len() > MaxPreambleChars)
	{
		Preamble = Preamble.Left(MaxPreambleChars - 1) + TEXT("…");
	}

	// Read all rejections, then take the most recent (the file appends in
	// chronological order — last entries are newest).
	TArray<FRejectionEntry> AllRejections = ParseRejections(RejectionsSection, INT32_MAX);
	const int32 Skip = FMath::Max(0, AllRejections.Num() - MaxRejections);
	TArray<FRejectionEntry> Recent;
	Recent.Reserve(AllRejections.Num() - Skip);
	for (int32 i = Skip; i < AllRejections.Num(); ++i)
	{
		Recent.Add(AllRejections[i]);
	}

	if (Preamble.IsEmpty() && Recent.Num() == 0)
	{
		return FString();
	}

	FString Out;
	if (!Preamble.IsEmpty())
	{
		Out += TEXT("## Project guides (from AGENTS.md)\n");
		Out += Preamble;
		Out += TEXT("\n\n");
	}
	if (Recent.Num() > 0)
	{
		Out += TEXT("## Past rejections (avoid repeating)\n");
		for (const FRejectionEntry& E : Recent)
		{
			Out += TEXT("- tool=");
			Out += E.Tool;
			if (!E.Target.IsEmpty())
			{
				Out += TEXT(" on ");
				Out += E.Target;
			}
			Out += TEXT(": ");
			Out += E.Reason;
			Out += TEXT("\n");
		}
		Out += TEXT("\n");
	}
	return Out;
}
