/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup string
 *
 * Copyright (C) 2001 NaN Technologies B.V.
 * This file was formerly known as: GEN_StdString.cpp.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "STR_String.h"

/*-------------------------------------------------------------------------------------------------
  Construction / destruction
-------------------------------------------------------------------------------------------------*/

#define STR_STRING_SIZE_DEFAULT_WORD 32 /* default size for a new word */
#define STR_STRING_SIZE_DEFAULT_CHAR 9  /* default size for a new char */

//
// Construct an empty string
//
STR_String::STR_String()
    : m_data(new char[STR_STRING_SIZE_DEFAULT_WORD]), m_len(0), m_max(STR_STRING_SIZE_DEFAULT_WORD)
{
  this->m_data[0] = 0;
}

//
// Construct a string of one character
//
STR_String::STR_String(char c)
    : m_data(new char[STR_STRING_SIZE_DEFAULT_CHAR]), m_len(1), m_max(STR_STRING_SIZE_DEFAULT_CHAR)
{
  this->m_data[0] = c;
  this->m_data[1] = 0;
}

//
// Construct a string of multiple repeating characters
//
STR_String::STR_String(char c, int len) : m_data(new char[len + 8]), m_len(len), m_max(len + 8)
{
  assertd(this->m_data != NULL);
  memset(this->m_data, c, len);
  this->m_data[len] = 0;
}

//
// Construct a string from a pointer-to-ASCIIZ-string
//
// MAART: Changed to test for null strings
STR_String::STR_String(const char *str)
{
  if (str) {
    this->m_len = ::strlen(str);
    this->m_max = this->m_len + 8;
    this->m_data = new char[this->m_max];
    assertd(this->m_data != NULL);
    ::memcpy(this->m_data, str, this->m_len);
    this->m_data[this->m_len] = 0;
  }
  else {
    this->m_data = NULL;
    this->m_len = 0;
    this->m_max = 8;
  }
}

//
// Construct a string from a pointer-to-ASCII-string and a length
//
STR_String::STR_String(const char *str, int len)
    : m_data(new char[len + 8]), m_len(len), m_max(len + 8)
{
  assertd(this->m_data != NULL);
  memcpy(this->m_data, str, len);
  this->m_data[len] = 0;
}

//
// Construct a string from another string
//
STR_String::STR_String(rcSTR_String str)
    : m_data(new char[str.Length() + 8]), m_len(str.Length()), m_max(str.Length() + 8)
{
  assertd(this->m_data != NULL);
  assertd(str.this->m_data != NULL);
  memcpy(this->m_data, str.ReadPtr(), str.Length());
  this->m_data[str.Length()] = 0;
}

//
// Construct a string from the first number of characters in another string
//
STR_String::STR_String(rcSTR_String str, int len)
    : m_data(new char[len + 8]), m_len(len), m_max(len + 8)
{
  assertd(this->m_data != NULL);
  assertd(str.this->m_data != NULL);
  memcpy(this->m_data, str.ReadPtr(), str.Length());
  this->m_data[str.Length()] = 0;
}

//
// Create a string by concatenating two sources
//
STR_String::STR_String(const char *src1, int len1, const char *src2, int len2)
    : m_data(new char[len1 + len2 + 8]), m_len(len1 + len2), m_max(len1 + len2 + 8)
{
  assertd(this->m_data != NULL);
  memcpy(this->m_data, src1, len1);
  memcpy(this->m_data + len1, src2, len2);
  this->m_data[len1 + len2] = 0;
}

//
// Create a string with an integer value
//
STR_String::STR_String(int val)
    : m_data(new char[STR_STRING_SIZE_DEFAULT_WORD]), m_max(STR_STRING_SIZE_DEFAULT_WORD)
{
  assertd(this->m_data != NULL);
  this->m_len = sprintf(this->m_data, "%d", val);
}

//
// Create a string with a dword value
//
STR_String::STR_String(dword val)
    : m_data(new char[STR_STRING_SIZE_DEFAULT_WORD]), m_max(STR_STRING_SIZE_DEFAULT_WORD)
{
  assertd(this->m_data != NULL);
  this->m_len = sprintf(this->m_data, "%lu", val);
}

//
// Create a string with a floating point value
//
STR_String::STR_String(float val)
    : m_data(new char[STR_STRING_SIZE_DEFAULT_WORD]), m_max(STR_STRING_SIZE_DEFAULT_WORD)
{
  assertd(this->m_data != NULL);
  this->m_len = sprintf(this->m_data, "%g", val);
}

//
// Create a string with a double value
//
STR_String::STR_String(double val)
    : m_data(new char[STR_STRING_SIZE_DEFAULT_WORD]), m_max(STR_STRING_SIZE_DEFAULT_WORD)
{
  assertd(this->m_data != NULL);
  this->m_len = sprintf(this->m_data, "%g", val);
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
  if (len + 1 <= this->m_max)
    return;

  // Reallocate string
  char *new_data = new char[len + 8];
  if (keep_contents) {
    memcpy(new_data, this->m_data, this->m_len);
  }
  delete[] this->m_data;

  // Accept new data
  this->m_max = len + 8;
  this->m_data = new_data;
  assertd(this->m_data != NULL);
}

/*-------------------------------------------------------------------------------------------------
  Basic string operations
-------------------------------------------------------------------------------------------------*/

//
// Format string (as does sprintf)
//
STR_String &STR_String::Format(const char *fmt, ...)
{
  AllocBuffer(2048, false);

  assertd(this->m_data != NULL);
  // Expand arguments and format to string
  va_list args;
  va_start(args, fmt);
  this->m_len = vsprintf(this->m_data, fmt, args);
  assertd(this->m_len <= 2048);
  va_end(args);

  return *this;
}

//
// Format string (as does sprintf)
//
STR_String &STR_String::FormatAdd(const char *fmt, ...)
{
  AllocBuffer(2048, false);

  assertd(this->m_data != NULL);
  // Expand arguments and format to string
  va_list args;
  va_start(args, fmt);
  this->m_len += vsprintf(this->m_data + this->m_len, fmt, args);
  assertd(this->m_len <= 2048);
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
  for (int i = 0; i < this->m_len; i++)
    if (isLower(this->m_data[i]))
      return false;

  return true;
}

//
// Check if string is entirely in lowerCase
//
bool STR_String::IsLower() const
{
  for (int i = 0; i < this->m_len; i++)
    if (isUpper(this->m_data[i]))
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
  assertd(this->m_len == 0 || pos < this->m_len);
  assertd(this->m_data != NULL);
  char *find_pos = strchr(this->m_data + pos, c);
  return (find_pos) ? (find_pos - this->m_data) : -1;
}

//
// Find the first occurrence of <str> in the string
//
int STR_String::Find(const char *str, int pos) const
{
  assertd(pos >= 0);
  assertd(this->m_len == 0 || pos < this->m_len);
  assertd(this->m_data != NULL);
  char *find_pos = strstr(this->m_data + pos, str);
  return (find_pos) ? (find_pos - this->m_data) : -1;
}

//
// Find the first occurrence of <str> in the string
//
int STR_String::Find(rcSTR_String str, int pos) const
{
  assertd(pos >= 0);
  assertd(this->m_len == 0 || pos < this->m_len);
  assertd(this->m_data != NULL);
  char *find_pos = strstr(this->m_data + pos, str.ReadPtr());
  return (find_pos) ? (find_pos - this->m_data) : -1;
}

//
// Find the last occurrence of <c> in the string
//
int STR_String::RFind(char c) const
{
  assertd(this->m_data != NULL);
  char *pos = strrchr(this->m_data, c);
  return (pos) ? (pos - this->m_data) : -1;
}

//
// Find the first occurrence of any character in character set <set> in the string
//
int STR_String::FindOneOf(const char *set, int pos) const
{
  assertd(pos >= 0);
  assertd(this->m_len == 0 || pos < this->m_len);
  assertd(this->m_data != NULL);
  char *find_pos = strpbrk(this->m_data + pos, set);
  return (find_pos) ? (find_pos - this->m_data) : -1;
}

//
// Replace a character in this string with another string
//
void STR_String::Replace(int pos, rcSTR_String str)
{
  // bounds(pos, 0, Length()-1);

  if (str.Length() < 1) {
    // Remove one character from the string
    memcpy(this->m_data + pos, this->m_data + pos + 1, this->m_len - pos);
  }
  else {
    // Insert zero or more characters into the string
    AllocBuffer(this->m_len + str.Length() - 1, true);
    if (str.Length() != 1)
      memcpy(this->m_data + pos + str.Length(), this->m_data + pos + 1, Length() - pos);
    memcpy(this->m_data + pos, str.ReadPtr(), str.Length());
  }

  this->m_len += str.Length() - 1;
}

//
// Replace a substring of this string with another string
//
void STR_String::Replace(int pos, int num, rcSTR_String str)
{
  // bounds(pos, 0, Length()-1);
  // bounds(pos+num, 0, Length());
  assertd(num >= 1);

  if (str.Length() < num) {
    // Remove some data from the string by replacement
    memcpy(
        this->m_data + pos + str.Length(), this->m_data + pos + num, this->m_len - pos - num + 1);
    memcpy(this->m_data + pos, str.ReadPtr(), str.Length());
  }
  else {
    // Insert zero or more characters into the string
    AllocBuffer(this->m_len + str.Length() - num, true);
    if (str.Length() != num)
      memcpy(
          this->m_data + pos + str.Length(), this->m_data + pos + num, Length() - pos - num + 1);
    memcpy(this->m_data + pos, str.ReadPtr(), str.Length());
  }

  this->m_len += str.Length() - num;
}

/*-------------------------------------------------------------------------------------------------
  Comparison
-------------------------------------------------------------------------------------------------*/

//
// Compare two strings and return the result,
// <0 if *this<rhs, >0 if *this>rhs or 0 if *this==rhs
//
int STR_String::Compare(rcSTR_String rhs) const
{
  return strcmp(this->ReadPtr(), rhs.ReadPtr());
}

//
// Compare two strings without respecting case and return the result,
// <0 if *this<rhs, >0 if *this>rhs or 0 if *this==rhs
//
int STR_String::CompareNoCase(rcSTR_String rhs) const
{
#ifdef WIN32
  return stricmp(this->ReadPtr(), rhs.ReadPtr());
#else
  return strcasecmp(this->ReadPtr(), rhs.ReadPtr());
#endif
}

/*-------------------------------------------------------------------------------------------------
  Formatting
-------------------------------------------------------------------------------------------------*/

//
// Capitalize string, "heLLo" -> "HELLO"
//
STR_String &STR_String::Upper()
{
  assertd(this->m_data != NULL);
#ifdef WIN32
  _strupr(this->m_data);
#else
  for (int i = 0; i < this->m_len; i++)
    this->m_data[i] = (this->m_data[i] >= 'a' && this->m_data[i] <= 'z') ?
                          this->m_data[i] + 'A' - 'a' :
                          this->m_data[i];
#endif
  return *this;
}

//
// Lower string, "heLLo" -> "hello"
//
STR_String &STR_String::Lower()
{
  assertd(this->m_data != NULL);
#ifdef WIN32
  _strlwr(this->m_data);
#else
  for (int i = 0; i < this->m_len; i++)
    this->m_data[i] = (this->m_data[i] >= 'A' && this->m_data[i] <= 'Z') ?
                          this->m_data[i] + 'a' - 'A' :
                          this->m_data[i];
#endif
  return *this;
}

//
// Capitalize string, "heLLo" -> "Hello"
//
STR_String &STR_String::Capitalize()
{
  assertd(this->m_data != NULL);
#ifdef WIN32
  if (this->m_len > 0)
    this->m_data[0] = toupper(this->m_data[0]);
  if (this->m_len > 1)
    _strlwr(this->m_data + 1);
#else
  if (this->m_len > 0)
    this->m_data[0] = (this->m_data[0] >= 'a' && this->m_data[0] <= 'z') ?
                          this->m_data[0] + 'A' - 'a' :
                          this->m_data[0];
  for (int i = 1; i < this->m_len; i++)
    this->m_data[i] = (this->m_data[i] >= 'A' && this->m_data[i] <= 'Z') ?
                          this->m_data[i] + 'a' - 'A' :
                          this->m_data[i];
#endif
  return *this;
}

//
// Trim whitespace from the left side of the string
//
STR_String &STR_String::TrimLeft()
{
  int skip;
  assertd(this->m_data != NULL);
  for (skip = 0; isSpace(this->m_data[skip]); skip++, this->m_len--) {
    /* pass */
  }
  memmove(this->m_data, this->m_data + skip, this->m_len + 1);
  return *this;
}

//
// Trim whitespaces from the right side of the string
//
STR_String &STR_String::TrimRight()
{
  assertd(this->m_data != NULL);
  while (this->m_len && isSpace(this->m_data[this->m_len - 1]))
    this->m_len--;
  this->m_data[this->m_len] = 0;
  return *this;
}

//
// Trim spaces from both sides of the character set
//
STR_String &STR_String::Trim()
{
  TrimRight();
  TrimLeft();
  return *this;
}

//
// Trim characters from the character set <set> from the left side of the string
//
STR_String &STR_String::TrimLeft(char *set)
{
  int skip;
  assertd(this->m_data != NULL);
  for (skip = 0; this->m_len && strchr(set, this->m_data[skip]); skip++, this->m_len--) {
    /* pass */
  }
  memmove(this->m_data, this->m_data + skip, this->m_len + 1);
  return *this;
}

//
// Trim characters from the character set <set> from the right side of the string
//
STR_String &STR_String::TrimRight(char *set)
{
  assertd(this->m_data != NULL);
  while (this->m_len && strchr(set, this->m_data[this->m_len - 1]))
    this->m_len--;
  this->m_data[this->m_len] = 0;
  return *this;
}

//
// Trim characters from the character set <set> from both sides of the character set
//
STR_String &STR_String::Trim(char *set)
{
  TrimRight(set);
  TrimLeft(set);
  return *this;
}

//
// Trim quotes from both sides of the string
//
STR_String &STR_String::TrimQuotes()
{
  // Trim quotes if they are on both sides of the string
  assertd(this->m_data != NULL);
  if ((this->m_len >= 2) && (this->m_data[0] == '\"') && (this->m_data[this->m_len - 1] == '\"')) {
    memmove(this->m_data, this->m_data + 1, this->m_len - 2 + 1);
    this->m_len -= 2;
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
  assertd(len >= 0);
  assertd(src);
  assertd(this->m_data != NULL);

  AllocBuffer(len, false);
  this->m_len = len;
  memcpy(this->m_data, src, len);
  this->m_data[this->m_len] = 0;

  return *this;
}

//
// Concate a number of bytes to the current string
//
rcSTR_String STR_String::Concat(const char *data, int len)
{
  assertd(this->m_len >= 0);
  assertd(len >= 0);
  assertd(data);
  assertd(this->m_data != NULL);

  AllocBuffer(this->m_len + len, true);
  memcpy(this->m_data + this->m_len, data, len);
  this->m_len += len;
  this->m_data[this->m_len] = 0;

  return *this;
}

std::vector<STR_String> STR_String::Explode(char c) const
{
  STR_String lcv = *this;
  std::vector<STR_String> uc;

  while (lcv.Length()) {
    int pos = lcv.Find(c);
    if (pos < 0) {
      uc.push_back(lcv);
      lcv.Clear();
    }
    else {
      uc.push_back(lcv.Left(pos));
      lcv = lcv.Mid(pos + 1);
    }
  }

  // uc. -= STR_String("");

  return uc;
}

#if 0

int STR_String::Serialize(pCStream stream)
{
  if (stream->GetAccess() == CStream::Access_Read) {
    int ln;
    stream->Read(&ln, sizeof(ln));
    AllocBuffer(ln, false);
    stream->Read(this->m_data, ln);
    this->m_data[ln] = '\0';
    this->m_len = ln;
  }
  else {
    stream->Write(&this->m_len, sizeof(this->m_len));
    stream->Write(this->m_data, this->m_len);
  }

  return this->m_len + sizeof(this->m_len);
}
#endif
