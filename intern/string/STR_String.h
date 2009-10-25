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
 * This file was formerly known as: GEN_StdString.h.
 * @date	April, 25, 2001
 */

#ifndef _STR_String_H_
#define _STR_String_H_

#ifndef STR_NO_ASSERTD
#undef  assertd
#define	assertd(exp)			((void)NULL)
#endif

#include <vector>
#include <limits.h>

#include <cstring>
#include <cstdlib>

using namespace std;

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#ifdef _WIN32
#define stricmp _stricmp
#endif

class STR_String;

typedef  unsigned long dword;
typedef const STR_String& rcSTR_String;
typedef unsigned char byte;

/**
 *	Smart String Value class. Is used by parser when an expression tree is build containing string.
*/

class STR_String
{
public:
	// Initialization
	STR_String();
	STR_String(char c);
	STR_String(char c, int len);
	STR_String(const char *str);
	STR_String(const char *str, int len);
	STR_String(const STR_String &str);
	STR_String(const STR_String & str, int len);
	STR_String(const char *src1, int src1_len, const char *src2, int src2_len);
	explicit STR_String(int val);
	explicit STR_String(dword val);
	explicit STR_String(float val);
	explicit STR_String(double val);
	inline ~STR_String()												{ delete[] pData; }

	// Operations
	STR_String&			Format(const char *fmt, ...);				// Set formatted text to string
	STR_String&			FormatAdd(const char *fmt, ...);			// Add formatted text to string
	inline void			Clear()										{ Len = pData[0] = 0; }
	inline const STR_String	& Reverse()									
        { 
            for (int i1=0, i2=Len-1; i1<i2; i1++, i2--) 
                swap(pData[i1], pData[i2]); return *this; 
        }

	// Properties
	bool				IsUpper() const;
	bool				IsLower() const;
	inline bool			IsEmpty() const								{ return Len==0; }
	inline int			Length() const								{ return Len; }

	// Data access
	inline STR_String&	SetLength(int len)							{ AllocBuffer(len, true); Len=len; pData[len]=0; return *this; }
	inline char			GetAt(int pos) const						{ assertd(pos<Len); return pData[pos]; }
	inline void			SetAt(int pos, char c)						{ assertd(pos<Len); pData[pos]=c; }
	inline void			SetAt(int pos, rcSTR_String str);
	inline void			SetAt(int pos, int num, rcSTR_String str);
	void				Replace(int pos, rcSTR_String str);
	void				Replace(int pos, int num, rcSTR_String str);

	// Substrings
	inline STR_String	Left(int num) const								{ num = (num < Len ? num:Len ); return STR_String(pData, num); }
	inline STR_String	Right(int num) const							{ num = (num < Len ? num:Len ); return STR_String(pData+Len-num, num); }
	inline STR_String	Mid(int pos, int num = INT_MAX) const			{ pos = (pos < Len ? pos:Len ); num = (num < (Len - pos) ? num : (Len - pos)); return STR_String(pData+pos, num); }

	// Comparison
	int				Compare(rcSTR_String rhs) const;
	int				CompareNoCase(rcSTR_String rhs) const;
	inline bool		IsEqual(rcSTR_String rhs) const					{ return (Compare(rhs)==0); }
	inline bool		IsEqualNoCase(rcSTR_String rhs) const				{ return (CompareNoCase(rhs)==0); }

	// Search/replace
	int				Find(char c, int pos = 0) const;
	int				Find(const char *str, int pos = 0) const;
	int				Find(rcSTR_String str, int pos = 0) const;
	int				RFind(char c) const;
	int				FindOneOf(const char *set, int pos = 0) const;
	int				RFindOneOf(const char *set, int pos = 0) const;

	vector<STR_String>	Explode(char c) const;

	// Formatting
	STR_String&			Upper();
	STR_String&			Lower();
	STR_String&			Capitalize();
	STR_String&			TrimLeft();
	STR_String&			TrimLeft(char *set);
	STR_String&			TrimRight();
	STR_String&			TrimRight(char *set);
	STR_String&			Trim();
	STR_String&			Trim(char *set);
	STR_String&			TrimQuotes();

	// Conversions
//	inline operator char*()												{ return pData; }
	inline operator const char *() const								{ return pData; }
	inline char *Ptr()													{ return pData; }
	inline const char *ReadPtr() const									{ return pData; }
	inline float	ToFloat() const										{ float x=(float)(atof(pData)); return x; }
	inline int		ToInt() const										{ return atoi(pData); }

	// Operators
	inline rcSTR_String	operator=(const byte *rhs)						{ return Copy((const char *)rhs, strlen((const char *)rhs)); }
	inline rcSTR_String	operator=(rcSTR_String rhs)						{ return Copy(rhs.ReadPtr(), rhs.Length()); }
	inline rcSTR_String	operator=(char rhs)								{ return Copy(&rhs, 1); }
	inline rcSTR_String	operator=(const char *rhs)						{ return Copy(rhs, strlen(rhs)); }

	inline rcSTR_String	operator+=(const char *rhs)						{ return Concat(rhs, strlen(rhs)); }
	inline rcSTR_String	operator+=(rcSTR_String rhs)						{ return Concat(rhs.ReadPtr(), rhs.Length()); }
	inline rcSTR_String	operator+=(char rhs)							{ return Concat(&rhs, 1); }

	
	inline friend bool operator<(rcSTR_String      lhs, rcSTR_String		rhs)	{ return (strcmp(lhs, rhs)<0); }
	inline friend bool operator<(rcSTR_String      lhs, const char		*rhs)	{ return (strcmp(lhs, rhs)<0); };
	inline friend bool operator<(const char		*lhs, rcSTR_String     rhs)	{ return (strcmp(lhs, rhs)<0); }
	inline friend bool operator>(rcSTR_String      lhs, rcSTR_String		rhs)	{ return (strcmp(lhs, rhs)>0); }
	inline friend bool operator>(rcSTR_String      lhs, const char		*rhs)	{ return (strcmp(lhs, rhs)>0); }
	inline friend bool operator>(const char		*lhs, rcSTR_String     rhs)	{ return (strcmp(lhs, rhs)>0); }
	inline friend bool operator<=(rcSTR_String     lhs, rcSTR_String		rhs)	{ return (strcmp(lhs, rhs)<=0); }
	inline friend bool operator<=(rcSTR_String     lhs, const char		*rhs)	{ return (strcmp(lhs, rhs)<=0); }
	inline friend bool operator<=(const char	*lhs, rcSTR_String     rhs)	{ return (strcmp(lhs, rhs)<=0); }
	inline friend bool operator>=(rcSTR_String     lhs, rcSTR_String		rhs)	{ return (strcmp(lhs, rhs)>=0); }
	inline friend bool operator>=(rcSTR_String     lhs, const char		*rhs)	{ return (strcmp(lhs, rhs)>=0); }
	inline friend bool operator>=(const char	*lhs, rcSTR_String     rhs)	{ return (strcmp(lhs, rhs)>=0); }
	inline friend bool operator==(rcSTR_String     lhs, rcSTR_String		rhs)	{ return ((lhs.Length() == rhs.Length()) && (memcmp(lhs, rhs, lhs.Length())==0)); }
	inline friend bool operator==(rcSTR_String     lhs, const char		*rhs)	{ return (memcmp(lhs, rhs, lhs.Length()+1)==0); }
	inline friend bool operator==(const char	*lhs, rcSTR_String     rhs)	{ return (memcmp(lhs, rhs, rhs.Length()+1)==0); }
	inline friend bool operator!=(rcSTR_String     lhs, rcSTR_String		rhs)	{ return ((lhs.Length() != rhs.Length()) || (memcmp(lhs, rhs, lhs.Length())!=0)); }
	inline friend bool operator!=(rcSTR_String     lhs, const char		*rhs)	{ return (memcmp(lhs, rhs, lhs.Length()+1)!=0); }
	inline friend bool operator!=(const char	*lhs, rcSTR_String     rhs)	{ return (memcmp(lhs, rhs, rhs.Length()+1)!=0); }

	// serializing
	//int			Serialize(pCStream stream);

protected:
	// Implementation
	void	AllocBuffer(int len, bool keep_contents);
	rcSTR_String Copy(const char *src, int len);
	rcSTR_String Concat(const char *data, int len);

	static bool		isLower(char c)									{ return !isUpper(c); }
	static bool		isUpper(char c)									{ return (c>='A') && (c <= 'Z'); }
	static bool		isSpace(char c)									{ return (c==' ') || (c=='\t'); }

	char   *pData;													// -> STR_String data
	int	   Len;														// Data length
	int	   Max;														// Space in data buffer


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "CXX:STR_String"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

inline  STR_String operator+(rcSTR_String    lhs, rcSTR_String   rhs)	{ return STR_String(lhs.ReadPtr(), lhs.Length(), rhs.ReadPtr(), rhs.Length()); }
inline  STR_String operator+(rcSTR_String    lhs, char        rhs)	{ return STR_String(lhs.ReadPtr(), lhs.Length(), &rhs, 1); }
inline  STR_String operator+(char         lhs, rcSTR_String   rhs)	{ return STR_String(&lhs, 1, rhs.ReadPtr(), rhs.Length()); }
inline  STR_String operator+(rcSTR_String    lhs, const char *rhs)	{ return STR_String(lhs.ReadPtr(), lhs.Length(), rhs, strlen(rhs)); }
inline  STR_String operator+(const char  *lhs, rcSTR_String   rhs)	{ return STR_String(lhs, strlen(lhs), rhs.ReadPtr(), rhs.Length()); }


#endif //_STR_String_H_

