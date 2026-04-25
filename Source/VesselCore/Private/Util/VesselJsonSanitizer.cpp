// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Util/VesselJsonSanitizer.h"
#include "Serialization/JsonSerializer.h"

FString FVesselJsonSanitizer::StripMarkdownFences(const FString& Raw)
{
	FString S = Raw;
	S.TrimStartAndEndInline();

	// Strip leading ```lang\n or ```\n
	if (S.StartsWith(TEXT("```"), ESearchCase::CaseSensitive))
	{
		int32 FirstNewline = INDEX_NONE;
		if (S.FindChar(TEXT('\n'), FirstNewline))
		{
			S.RemoveAt(0, FirstNewline + 1);
		}
		else
		{
			// No newline → whole thing was just ``` with no content; give up.
			S.RemoveAt(0, 3);
		}
	}

	// Strip trailing ```
	if (S.EndsWith(TEXT("```"), ESearchCase::CaseSensitive))
	{
		S.RemoveAt(S.Len() - 3, 3);
	}

	S.TrimStartAndEndInline();
	return S;
}

int32 FVesselJsonSanitizer::FindBalancedObjectEnd(const FString& Text, int32 StartIdx)
{
	if (!Text.IsValidIndex(StartIdx) || Text[StartIdx] != TEXT('{'))
	{
		return INDEX_NONE;
	}

	int32 Depth = 0;
	bool bInString = false;
	bool bEscape = false;

	for (int32 i = StartIdx; i < Text.Len(); ++i)
	{
		const TCHAR C = Text[i];

		if (bEscape)
		{
			bEscape = false;
			continue;
		}
		if (bInString)
		{
			if (C == TEXT('\\'))
			{
				bEscape = true;
			}
			else if (C == TEXT('"'))
			{
				bInString = false;
			}
			continue;
		}

		if (C == TEXT('"'))
		{
			bInString = true;
		}
		else if (C == TEXT('{'))
		{
			++Depth;
		}
		else if (C == TEXT('}'))
		{
			--Depth;
			if (Depth == 0)
			{
				return i;
			}
		}
	}
	return INDEX_NONE;
}

bool FVesselJsonSanitizer::ExtractFirstJsonObject(const FString& Raw, FString& OutJson)
{
	OutJson.Empty();
	const FString Stripped = StripMarkdownFences(Raw);

	const int32 Start = Stripped.Find(TEXT("{"));
	if (Start == INDEX_NONE)
	{
		return false;
	}

	const int32 End = FindBalancedObjectEnd(Stripped, Start);
	if (End == INDEX_NONE)
	{
		return false;
	}

	OutJson = Stripped.Mid(Start, End - Start + 1);
	return true;
}

FString FVesselJsonSanitizer::RepairUnescapedInnerQuotes(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len() + 8);

	bool bInString = false;
	bool bEscape   = false;

	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR C = Text[i];

		if (bEscape)
		{
			Out.AppendChar(C);
			bEscape = false;
			continue;
		}

		if (bInString)
		{
			if (C == TEXT('\\'))
			{
				Out.AppendChar(C);
				bEscape = true;
				continue;
			}
			if (C == TEXT('"'))
			{
				// Peek past whitespace at the next non-blank character.
				int32 j = i + 1;
				while (j < Text.Len())
				{
					const TCHAR P = Text[j];
					if (P == TEXT(' ')  || P == TEXT('\t') ||
					    P == TEXT('\n') || P == TEXT('\r'))
					{
						++j;
						continue;
					}
					break;
				}
				const bool bAtEnd = (j >= Text.Len());
				const TCHAR Next  = bAtEnd ? TEXT('\0') : Text[j];
				const bool bRealClose =
					bAtEnd ||
					Next == TEXT(':') || Next == TEXT(',') ||
					Next == TEXT('}') || Next == TEXT(']');

				if (bRealClose)
				{
					Out.AppendChar(C);
					bInString = false;
				}
				else
				{
					// Inner unescaped quote — emit \" and stay in string.
					Out.AppendChar(TEXT('\\'));
					Out.AppendChar(C);
				}
				continue;
			}
			Out.AppendChar(C);
			continue;
		}

		// Outside string.
		if (C == TEXT('"'))
		{
			bInString = true;
		}
		Out.AppendChar(C);
	}
	return Out;
}

bool FVesselJsonSanitizer::ParseAsObject(const FString& Raw, TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();

	FString Cleaned;
	if (!ExtractFirstJsonObject(Raw, Cleaned))
	{
		return false;
	}

	// Strict pass first.
	{
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Cleaned);
		if (FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid())
		{
			return true;
		}
	}

	// Repair pass: only retry if we actually changed something. Avoids
	// pointless re-parsing of objects that are broken in some other way
	// the heuristic can't fix.
	OutObject.Reset();
	const FString Repaired = RepairUnescapedInnerQuotes(Cleaned);
	if (Repaired.Equals(Cleaned, ESearchCase::CaseSensitive))
	{
		return false;
	}
	TSharedRef<TJsonReader<TCHAR>> RepairReader = TJsonReaderFactory<TCHAR>::Create(Repaired);
	return FJsonSerializer::Deserialize(RepairReader, OutObject) && OutObject.IsValid();
}
