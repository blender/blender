/**
 * blenlib/DNA_ID.h (mar-2001 nzc)
 *
 * ID and Library types, which are fundamental for sdna,
 *
 * $Id$
 *
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
#ifndef DNA_ID_H
#define DNA_ID_H

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Library;
struct FileData;
struct ID;

typedef struct IDPropertyData {
	void *pointer;
	ListBase group;
	int val, val2; /*note, we actually fit a double into these two ints*/
} IDPropertyData;

typedef struct IDProperty {
	struct IDProperty *next, *prev;
	char name[32];
	char type, subtype;
	short flag;
	int saved; /*saved is used to indicate if this struct has been saved yet.
				seemed like a good idea as a pad var was needed anyway :)*/
	IDPropertyData data;	/* note, alignment for 64 bits */
	int len; /* array length, also (this is important!) string length + 1.
	            the idea is to be able to reuse array realloc functions on strings.*/
	/*totallen is total length of allocated array/string, including a buffer.
	  Note that the buffering is mild; the code comes from python's list implementation.*/
	int totallen; /*strings and arrays are both buffered, though the buffer isn't
	                saved.*/
} IDProperty;

#define MAX_IDPROP_NAME	32
#define DEFAULT_ALLOC_FOR_NULL_STRINGS	64

/*->type*/
#define IDP_STRING	0
#define IDP_INT		1
#define IDP_FLOAT	2
#define IDP_ARRAY	5
#define IDP_GROUP	6
/*the ID link property type hasn't been implemented yet, this will require
  some cleanup of blenkernel, most likely.*/
#define IDP_ID		7
#define IDP_DOUBLE	8

/*add any future new id property types here.*/

/* watch it: Sequence has identical beginning. */
/**
 * ID is the first thing included in all serializable types. It
 * provides a common handle to place all data in double-linked lists.
 * */

/* There's a nasty circular dependency here.... void* to the rescue! I
 * really wonder why this is needed. */
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
	int icon_id;
	IDProperty *properties;
} ID;

/**
 * For each library file used, a Library struct is added to Main
 * WARNING: readfile.c, expand_doit() reads this struct without DNA check!
 */
typedef struct Library {
	ID id;
	ID *idblock;
	struct FileData *filedata;
	char name[240];			/* reveiled in the UI, can store relative path */
	char filename[240];		/* expanded name, not relative, used while reading */
	int tot, pad;			/* tot, idblock and filedata are only fo read and write */
	struct Library *parent;	/* for outliner, showing dependency */
} Library;

#define PREVIEW_MIPMAPS 2
#define PREVIEW_MIPMAP_ZERO 0
#define PREVIEW_MIPMAP_LARGE 1

typedef struct PreviewImage {
	unsigned int w[2];
	unsigned int h[2];	
	short changed[2];
	short pad0, pad1;
	unsigned int * rect[2];
} PreviewImage;

/**
 * Defines for working with IDs.
 *
 * The tags represent types! This is a dirty way of enabling RTTI. The
 * sig_byte end endian defines aren't really used much.
 *
 **/

#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__)  || defined (__hppa__) || defined (__BIG_ENDIAN__)
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

/* ID from database */
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
#define ID_GR		MAKE_ID2('G', 'R')
#define ID_ID		MAKE_ID2('I', 'D')
#define ID_AR		MAKE_ID2('A', 'R')
#define ID_AC		MAKE_ID2('A', 'C')
#define ID_SCRIPT	MAKE_ID2('P', 'Y')
#define ID_NT		MAKE_ID2('N', 'T')
#define ID_BR		MAKE_ID2('B', 'R')
#define ID_PA		MAKE_ID2('P', 'A')

	/* NOTE! Fake IDs, needed for g.sipo->blocktype or outliner */
#define ID_SEQ		MAKE_ID2('S', 'Q')
			/* constraint */
#define ID_CO		MAKE_ID2('C', 'O')
			/* pose (action channel, used to be ID_AC in code, so we keep code for backwards compat) */
#define ID_PO		MAKE_ID2('A', 'C')
			/* used in outliner... */
#define ID_NLA		MAKE_ID2('N', 'L')
			/* fluidsim Ipo */
#define ID_FLUIDSIM	MAKE_ID2('F', 'S')


/*#ifdef WITH_VERSE*/
#define ID_VS		MAKE_ID2('V', 'S')	/* fake id for VerseSession, needed for outliner */
#define ID_VN		MAKE_ID2('V', 'N')	/* fake id for VerseNode, needed for outliner */
#define ID_MS		MAKE_ID2('M', 'S')  /* fake id for VerseServer root entry, needed for outliner */
#define ID_SS		MAKE_ID2('S', 'S')  /* fake id for VerseServer entry, needed for ountliner */
/*#endif*/


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
/*  */
#define LIB_APPEND_TAG	2048 

#ifdef __cplusplus
}
#endif

#endif

