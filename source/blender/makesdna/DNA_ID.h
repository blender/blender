/**
 * blenlib/DNA_ID.h (mar-2001 nzc)
 *
 * ID and Library types, which are fundamental for sdna, 
 *
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef DNA_ID_H
#define DNA_ID_H

#ifdef __cplusplus
extern "C" {
#endif

/* There's a nasty circular dependency here.... void* to the rescue! I
 * really wonder why this is needed. */

struct Library;
struct FileData;

/* watch it: Sequence has identical beginning. */
/**
 * ID is the first thing included in all serializable types. It
 * provides a common handle to place all data in double-linked lists.
 * */
typedef struct ID {
	void *next, *prev;
	struct ID *newid;
	struct Library *lib;
	char name[24];
	short us;
	/**
	 * LIB_... flags report on status of the datablock this ID belongs
	 * to.
	 */
	short flag;
	int pad;
} ID;

/**
 * For each library file used, a Library struct is added to Main
 */
typedef struct Library {
	ID id;
	ID *idblock;
	struct FileData *filedata;
	char name[160];
	int tot, pad;		/* tot, idblock and filedata are only fo read and write */
} Library;

/**
 * Defines for working with IDs.
 *
 * The tags represent types! This is a dirty way of enabling RTTI. The
 * sig_byte end endian defines aren't really used much.
 *
 **/

#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__BIG_ENDIAN__)
/* big endian */
#define MAKE_ID2(c, d)		( (c)<<8 | (d) )
#define MOST_SIG_BYTE				0
#define BBIG_ENDIAN
#else
/* little endian  */
#define MAKE_ID2(c, d)		( (d)<<8 | (c) )
#define MOST_SIG_BYTE				1
#define BLITTLE_ENDIAN
#endif

/* ID */
#define ID_SCE		MAKE_ID2('S', 'C')
#define ID_LI		MAKE_ID2('L', 'I')
#define ID_OB		MAKE_ID2('O', 'B')
#define ID_ME		MAKE_ID2('M', 'E')
#define ID_CU		MAKE_ID2('C', 'U')
#define ID_MB		MAKE_ID2('M', 'B')
#define ID_MA		MAKE_ID2('M', 'A')
#define ID_TE		MAKE_ID2('T', 'E')
#define ID_IM		MAKE_ID2('I', 'M')
#define ID_IK		MAKE_ID2('I', 'K')
#define ID_WV		MAKE_ID2('W', 'V')
#define ID_LT		MAKE_ID2('L', 'T')
#define ID_SE		MAKE_ID2('S', 'E')
#define ID_LF		MAKE_ID2('L', 'F')
#define ID_LA		MAKE_ID2('L', 'A')
#define ID_CA		MAKE_ID2('C', 'A')
#define ID_IP		MAKE_ID2('I', 'P')
#define ID_KE		MAKE_ID2('K', 'E')
#define ID_WO		MAKE_ID2('W', 'O')
#define ID_SCR		MAKE_ID2('S', 'R')
#define ID_VF		MAKE_ID2('V', 'F')
#define ID_TXT		MAKE_ID2('T', 'X')
#define ID_SO		MAKE_ID2('S', 'O')
#define ID_SAMPLE	MAKE_ID2('S', 'A')
#define ID_GR		MAKE_ID2('G', 'R')
#define ID_ID		MAKE_ID2('I', 'D')
#define ID_SEQ		MAKE_ID2('S', 'Q')
#define ID_AR		MAKE_ID2('A', 'R')
#define ID_AC		MAKE_ID2('A', 'C')

#define IPO_CO		MAKE_ID2('C', 'O')	/* NOTE! This is not an ID, but is needed for g.sipo->blocktype */

/* id->flag: set frist 8 bits always at zero while reading */
#define LIB_LOCAL		0
#define LIB_EXTERN		1
#define LIB_INDIRECT	2
#define LIB_TEST		8
#define LIB_TESTEXT		9
#define LIB_TESTIND		10
#define LIB_READ		16
#define LIB_NEEDLINK	32

#define LIB_NEW			256
#define LIB_FAKEUSER	512
/* free test flag */
#define LIB_DOIT		1024

#ifdef __cplusplus
}
#endif

#endif

