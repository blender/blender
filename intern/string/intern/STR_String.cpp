/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * This file was formerly known as: GEN_StdString.cpp.
 * @date	April, 25, 2001
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h> 
#include <ctype.h>
#include <string.h>
#if defined(__sun__) || defined( __sun ) || defined (__sparc) || defined (__sparc__) || defined (_AIX)
#include <strings.h>
#endif
#include "STR_String.h"

/*-------------------------------------------------------------------------------------------------
	Construction / destruction
-------------------------------------------------------------------------------------------------*/



//
// Construct an empty string
//
STR_String::STR_String() :
	pData(new char [32]), 
	Len(0),
	Max(32)
{
	pData[0] = 0;
}



//
// Construct a string of one character
//
STR_String::STR_String(char c) :
	pData(new char [9]),
	Len(1),
	Max(9)
{
	pData[0] = c;
	pData[1] = 0;
}



//
// Construct a string of multiple repeating characters
//
STR_String::STR_String(char c, int len) :
	pData(new char [len+8]),
	Len(len),
	Max(len+8)
{
	assertd(pData != NULL);
	memset(pData, c, len);
	pData[len] = 0;
}



//
// Construct a string from a pointer-to-ASCIIZ-string
//
// MAART: Changed to test for null strings
STR_String::STR_String(const char *str)
{
	if (str) {
		Len = ::strlen(str);
		Max = Len + 8;
		pData = new char [Max];
		assertd(pData != NULL);
		::memcpy(pData, str, Len);
		pData[Len] = 0;
	}
	else {
		pData = 0;
		Len = 0;
		Max = 8;
	}
}



//
// Construct a string from a pointer-to-ASCII-string and a length
//
STR_String::STR_String(const char *str, int len) :
	pData(new char [len+8]),
	Len(len),
	Max(len+8)
{
	assertd(pData != NULL);
	memcpy(pData, str, len);
	pData[len] = 0;
}



//
// Construct a string from another string
//
STR_String::STR_String(rcSTR_String str) :
	pData(new char [str.Length()+8]),
	Len(str.Length()),
	Max(str.Length()+8)
{
	assertd(pData != NULL);
	assertd(str.pData != NULL);
	memcpy(pData, str.pData, str.Length());
	pData[str.Length()] = 0;
}



//
// Construct a string from the first number of characters in another string
//
STR_String::STR_String(rcSTR_String str, int len) :
	pData(new char [len+8]),
	Len(len),
	Max(len+8)
{
	assertd(pData != NULL);
	assertd(str.pData != NULL);
	memcpy(pData, str.pData, str.Length());
	pData[str.Length()] = 0;
}



//
// Create a string by concatenating two sources
//
STR_String::STR_String(const char *src1, int len1, const char *src2, int len2) :
	pData(new char [len1+len2+8]),
	Len(len1+len2),
	Max(len1+len2+8)
{
	assertd(pData != NULL);
	memcpy(pData, src1, len1);
	memcpy(pData+len1, src2, len2);
	pData[len1+len2] = 0;
}



//
// Create a string with an integer value
//
STR_String::STR_String(int val) :
	pData(new char [32]),
	Max(32)
{
	assertd(pData != NULL);
	Len=sprintf(pData, "%d", val);
}
	



//
// Create a string with a dword value
//
STR_String::STR_String(dword val) :
	pData(new char [32]),
	Max(32)
{
	assertd(pData != NULL);
	Len=sprintf(pData, "%lu", val);
}



//
// Create a string with a floating point value
//
STR_String::STR_String(float val) :
	pData(new char [32]),
	Max(32)
{
	assertd(pData != NULL);
	Len=sprintf(pData, "%g", val);
}



//
// Create a string with a double value
//
STR_String::STR_String(double val) :
	pData(new char [32]),
	Max(32)
{
	assertd(pData != NULL);
	Len=sprintf(pData, "%g", val);
}



/*-------------------------------------------------------------------------------------------------
	Buffer management
-------------------------------------------------------------------------------------------------*/



//
// Make sure that the allocated buffer is at least <len> in size
//
void STR_String::AllocBuffer(int len, bool keep_contents)
{
	// Check if we have enough space
	if (len+1 <= Max) return;

	// Reallocate string
	char *new_data = new char [len+8];
	if (keep_contents) memcpy(new_data, pData, Len);
	delete[] pData;

	// Accept new data
	Max = len+8;
	pData = new_data;
	assertd(pData != NULL);
}
	


/*-------------------------------------------------------------------------------------------------
	Basic string operations
-------------------------------------------------------------------------------------------------*/



//
// Format string (as does sprintf)
//
STR_String& STR_String::Format(const char *fmt, ...)
{
	AllocBuffer(2048, false);

	assertd(pData != NULL);
	// Expand arguments and format to string
	va_list args;
	va_start(args, fmt);
	Len = vsprintf(pData, fmt, args);
	assertd(Len <= 2048);
	va_end(args);

	return *this;
}



//
// Format string (as does sprintf)
//
STR_String& STR_String::FormatAdd(const char *fmt, ...)
{
	AllocBuffer(2048, false);

	assertd(pData != NULL);
	// Expand arguments and format to string
	va_list args;
	va_start(args, fmt);
	Len += vsprintf(pData+Len, fmt, args);
	assertd(Len <= 2048);
	va_end(args);

	return *this;
}



/*-------------------------------------------------------------------------------------------------
	Properties
-------------------------------------------------------------------------------------------------*/



//
// Check if string is entirely in UPPERCase
//
bool STR_String::IsUpper() const
{
	for (int i=0; i<Len; i++)
		if (isLower(pData[i]))
			return false;

	return true;
}



//
// Check if string is entirely in lowerCase
//
bool STR_String::IsLower() const
{
	for (int i=0; i<Len; i++)
		if (isUpper(pData[i]))
			return false;

	return true;
}



/*-------------------------------------------------------------------------------------------------
	Search/Replace
-------------------------------------------------------------------------------------------------*/



//
// Find the first orccurence of <c> in the string
//
int STR_String::Find(char c, int pos) const
{
	assertd(pos >= 0);
	assertd(Len==0 || pos<Len);
	assertd(pData != NULL);
	char *find_pos = strchr(pData+pos, c);
	return (find_pos) ? (find_pos-pData) : -1;
}



//
// Find the first occurence of <str> in the string
//
int	STR_String::Find(const char *str, int pos) const
{
	assertd(pos >= 0);
	assertd(Len==0 || pos<Len);
	assertd(pData != NULL);
	char *find_pos = strstr(pData+pos, str);
	return (find_pos) ? (find_pos-pData) : -1;
}



//
// Find the first occurence of <str> in the string
//
int	STR_String::Find(rcSTR_String str, int pos) const
{
	assertd(pos >= 0);
	assertd(Len==0 || pos<Len);
	assertd(pData != NULL);
	char *find_pos = strstr(pData+pos, str.ReadPtr());
	return (find_pos) ? (find_pos-pData) : -1;
}



//
// Find the last occurence of <c> in the string
//
int STR_String::RFind(char c) const
{
	assertd(pData != NULL);
	char *pos = strrchr(pData, c);
	return (pos) ? (pos-pData) : -1;
}



//
// Find the first occurence of any character in character set <set> in the string
//
int STR_String::FindOneOf(const char *set, int pos) const
{
	assertd(pos >= 0);
	assertd(Len==0 || pos<Len);
	assertd(pData != NULL);
	char *find_pos = strpbrk(pData+pos, set);
	return (find_pos) ? (find_pos-pData) : -1;
}



//
// Replace a character in this string with another string
//
void STR_String::Replace(int pos, rcSTR_String str)
{
	//bounds(pos, 0, Length()-1);

	if (str.Length() < 1)
	{
		// Remove one character from the string
		memcpy(pData+pos, pData+pos+1, Len-pos);
	}
	else
	{
		// Insert zero or more characters into the string
		AllocBuffer(Len + str.Length() - 1, true);
		if (str.Length() != 1) memcpy(pData+pos+str.Length(), pData+pos+1, Length()-pos);
		memcpy(pData+pos, str.ReadPtr(), str.Length());
	}

	Len += str.Length()-1;
}



//
// Replace a substring of this string with another string
//
void STR_String::Replace(int pos, int num, rcSTR_String str)
{
	//bounds(pos, 0, Length()-1);
	//bounds(pos+num, 0, Length());
	assertd(num >= 1);

	if (str.Length() < num)
	{
		// Remove some data from the string by replacement
		memcpy(pData+pos+str.Length(), pData+pos+num, Len-pos-num+1);
		memcpy(pData+pos, str.ReadPtr(), str.Length());
	}
	else
	{
		// Insert zero or more characters into the string
		AllocBuffer(Len + str.Length() - num, true);
		if (str.Length() != num) memcpy(pData+pos+str.Length(), pData+pos+num, Length()-pos-num+1);
		memcpy(pData+pos, str.ReadPtr(), str.Length());
	}

	Len += str.Length()-num;
}



/*-------------------------------------------------------------------------------------------------
	Comparison
-------------------------------------------------------------------------------------------------*/



//
// Compare two strings and return the result, <0 if *this<rhs, >0 if *this>rhs or 0 if *this==rhs
//
int	STR_String::Compare(rcSTR_String rhs) const
{
	return strcmp(pData, rhs.pData);
}



//
// Compare two strings without respecting case and return the result, <0 if *this<rhs, >0 if *this>rhs or 0 if *this==rhs
//
int STR_String::CompareNoCase(rcSTR_String rhs) const
{
#ifdef WIN32
	return stricmp(pData, rhs.pData);
#else
	return strcasecmp(pData, rhs.pData);
#endif
}



/*-------------------------------------------------------------------------------------------------
	Formatting
-------------------------------------------------------------------------------------------------*/



//
// Capitalize string, "heLLo" -> "HELLO"
//
STR_String&	STR_String::Upper()
{
	assertd(pData != NULL);
#ifdef WIN32
	_strupr(pData);
#else
	for (int i=0;i<Len;i++)
		pData[i] = (pData[i] >= 'a' && pData[i] <= 'z')?pData[i]+'A'-'a':pData[i];
#endif
	return *this;
}



//
// Lower string, "heLLo" -> "hello"
//
STR_String&	STR_String::Lower()
{
	assertd(pData != NULL);
#ifdef WIN32
	_strlwr(pData);
#else
	for (int i=0;i<Len;i++)
		pData[i] = (pData[i] >= 'A' && pData[i] <= 'Z')?pData[i]+'a'-'A':pData[i];
#endif
	return *this;
}



//
// Capitalize string, "heLLo" -> "Hello"
//
STR_String&	STR_String::Capitalize()
{
	assertd(pData != NULL);
#ifdef WIN32
	if (Len>0) pData[0] = toupper(pData[0]);
	if (Len>1) _strlwr(pData+1);
#else
	if (Len > 0)
		pData[0] = (pData[0] >= 'A' && pData[0] <= 'A')?pData[0]+'a'-'A':pData[0];
	for (int i=1;i<Len;i++)
		pData[i] = (pData[i] >= 'a' && pData[i] <= 'z')?pData[i]+'A'-'a':pData[i];
#endif
	return *this;
}



//
// Trim whitespace from the left side of the string
//
STR_String&	STR_String::TrimLeft()
{
	int skip;
	assertd(pData != NULL);
	for (skip=0; isSpace(pData[skip]); skip++, Len--)
		{};
	memmove(pData, pData+skip, Len+1);
	return *this;
}



//
// Trim whitespaces from the right side of the string
//
STR_String&	STR_String::TrimRight()
{
	assertd(pData != NULL);
	while (Len && isSpace(pData[Len-1])) Len--;
	pData[Len]=0;
	return *this;
}



//
// Trim spaces from both sides of the character set
//
STR_String&	STR_String::Trim()
{
	TrimRight();
	TrimLeft();
	return *this;
}



//
// Trim characters from the character set <set> from the left side of the string
//
STR_String&	STR_String::TrimLeft(char *set)
{
	int skip;
	assertd(pData != NULL);
	for (skip=0; Len && strchr(set, pData[skip]); skip++, Len--)
		{};
	memmove(pData, pData+skip, Len+1);
	return *this;
}



//
// Trim characters from the character set <set> from the right side of the string
//
STR_String&	STR_String::TrimRight(char *set)
{
	assertd(pData != NULL);
	while (Len && strchr(set, pData[Len-1])) Len--;
	pData[Len]=0;
	return *this;
}



//
// Trim characters from the character set <set> from both sides of the character set
//
STR_String&	STR_String::Trim(char *set)
{
	TrimRight(set);
	TrimLeft(set);
	return *this;
}



//
// Trim quotes from both sides of the string
//
STR_String&	STR_String::TrimQuotes()
{
	// Trim quotes if they are on both sides of the string
	assertd(pData != NULL);
	if ((Len >= 2) && (pData[0] == '\"') && (pData[Len-1] == '\"'))
	{
		memmove(pData, pData+1, Len-2+1);
		Len-=2;
	}
	return *this;
}



/*-------------------------------------------------------------------------------------------------
	Assignment/Concatenation
-------------------------------------------------------------------------------------------------*/



//
// Set the string's conents to a copy of <src> with length <len>
//
rcSTR_String STR_String::Copy(const char *src, int len)
{
	assertd(len>=0);
	assertd(src);
	assertd(pData != NULL);

	AllocBuffer(len, false);
	Len = len;
	memcpy(pData, src, len);
	pData[Len] = 0;

	return *this;
}



//
// Concate a number of bytes to the current string
//
rcSTR_String STR_String::Concat(const char *data, int len)
{
	assertd(Len>=0);
	assertd(len>=0);
	assertd(data);
	assertd(pData != NULL);

	AllocBuffer(Len+len, true);
	memcpy(pData+Len, data, len);
	Len+=len;
	pData[Len] = 0;

	return *this;
}





vector<STR_String>	STR_String::Explode(char c) const
{
	STR_String				lcv	= *this;
	vector<STR_String>		uc;

	while (lcv.Length())
	{
		int pos = lcv.Find(c);
		if (pos < 0)
		{
			uc.push_back(lcv);
			lcv.Clear();
		} else
		{
			uc.push_back(lcv.Left(pos));
			lcv = lcv.Mid(pos+1);
		}
	}

	//uc. -= STR_String("");

	return uc;
}


/*

int		STR_String::Serialize(pCStream stream)
{
	if (stream->GetAccess() == CStream::Access_Read)
	{
		int ln;
		stream->Read(&ln, sizeof(ln));
		AllocBuffer(ln, false);
		stream->Read(pData, ln); 
		pData[ln]	= '\0';
		Len			= ln;
	} else
	{
		stream->Write(&Len, sizeof(Len));
		stream->Write(pData, Len);
	}

	return Len + sizeof(Len);
}
*/

