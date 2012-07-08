/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file DNA_ID.h
 *  \ingroup DNA
 *  \brief ID and Library types, which are fundamental for sdna.
 */

#ifndef __DNA_ID_H__
#define __DNA_ID_H__

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
	char type, subtype;
	short flag;
	char name[64];	/* MAX_IDPROP_NAME */
	int saved; /* saved is used to indicate if this struct has been saved yet.
	            * seemed like a good idea as a pad var was needed anyway :)*/
	IDPropertyData data;	/* note, alignment for 64 bits */
	int len; /* array length, also (this is important!) string length + 1.
	          * the idea is to be able to reuse array realloc functions on strings.*/
	/* totallen is total length of allocated array/string, including a buffer.
	 * Note that the buffering is mild; the code comes from python's list implementation.*/
	int totallen; /*strings and arrays are both buffered, though the buffer isn't saved.*/
} IDProperty;

#define MAX_IDPROP_NAME	64
#define DEFAULT_ALLOC_FOR_NULL_STRINGS	64

/*->type*/
#define IDP_STRING		0
#define IDP_INT			1
#define IDP_FLOAT		2
#define IDP_ARRAY		5
#define IDP_GROUP		6
/* the ID link property type hasn't been implemented yet, this will require
 * some cleanup of blenkernel, most likely.*/
#define IDP_ID			7
#define IDP_DOUBLE		8
#define IDP_IDPARRAY	9
#define IDP_NUMTYPES	10

/*->subtype */

/* IDP_STRING */
#define IDP_STRING_SUB_UTF8  0 /* default */
#define IDP_STRING_SUB_BYTE  1 /* arbitrary byte array, _not_ null terminated */
/*->flag*/
#define IDP_FLAG_GHOST (1<<7)  /* this means the propery is set but RNA will return
                                * false when checking 'RNA_property_is_set',
                                * currently this is a runtime flag */


/* add any future new id property types here.*/

/* watch it: Sequence has identical beginning. */
/**
 * ID is the first thing included in all serializable types. It
 * provides a common handle to place all data in double-linked lists.
 * */

/* 2 characters for ID code and 64 for actual name */
#define MAX_ID_NAME	66

/* There's a nasty circular dependency here.... void* to the rescue! I
 * really wonder why this is needed. */
typedef struct ID {
	void *next, *prev;
	struct ID *newid;
	struct Library *lib;
	char name[66]; /* MAX_ID_NAME */
	short pad, us;
	/**
	 * LIB_... flags report on status of the datablock this ID belongs
	 * to.
	 */
	short flag;
	int icon_id, pad2;
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
	char name[1024];		/* path name used for reading, can be relative and edited in the outliner */
	char filepath[1024];	/* absolute filepath, this is only for convenience,
							 * 'name' is the real path used on file read but in
							 * some cases its useful to access the absolute one,
							 * This is set on file read.
							 * Use BKE_library_filepath_set() rather than
							 * setting 'name' directly and it will be kept in
							 * sync - campbell */
	struct Library *parent;	/* set for indirectly linked libs, used in the outliner and while reading */
} Library;

enum eIconSizes {
	ICON_SIZE_ICON = 0,
	ICON_SIZE_PREVIEW = 1
};
#define NUM_ICON_SIZES (ICON_SIZE_PREVIEW + 1)

typedef struct PreviewImage {
	/* All values of 2 are really NUM_ICON_SIZES */
	unsigned int w[2];
	unsigned int h[2];
	short changed[2];
	short changed_timestamp[2];
	unsigned int *rect[2];
} PreviewImage;

/**
 * Defines for working with IDs.
 *
 * The tags represent types! This is a dirty way of enabling RTTI. The
 * sig_byte end endian defines aren't really used much.
 *
 **/

#ifdef __BIG_ENDIAN__
   /* big endian */
#  define MAKE_ID2(c, d)		( (c)<<8 | (d) )
#  define MOST_SIG_BYTE			0
#  define BBIG_ENDIAN
#else
   /* little endian  */
#  define MAKE_ID2(c, d)		( (d)<<8 | (c) )
#  define MOST_SIG_BYTE			1
#  define BLITTLE_ENDIAN
#endif

/* ID from database */
#define ID_SCE		MAKE_ID2('S', 'C') /* Scene */
#define ID_LI		MAKE_ID2('L', 'I') /* Library */
#define ID_OB		MAKE_ID2('O', 'B') /* Object */
#define ID_ME		MAKE_ID2('M', 'E') /* Mesh */
#define ID_CU		MAKE_ID2('C', 'U') /* Curve */
#define ID_MB		MAKE_ID2('M', 'B') /* MetaBall */
#define ID_MA		MAKE_ID2('M', 'A') /* Material */
#define ID_TE		MAKE_ID2('T', 'E') /* Texture */
#define ID_IM		MAKE_ID2('I', 'M') /* Image */
#define ID_LT		MAKE_ID2('L', 'T') /* Lattice */
#define ID_LA		MAKE_ID2('L', 'A') /* Lamp */
#define ID_CA		MAKE_ID2('C', 'A') /* Camera */
#define ID_IP		MAKE_ID2('I', 'P') /* Ipo (depreciated, replaced by FCurves) */
#define ID_KE		MAKE_ID2('K', 'E') /* Key (shape key) */
#define ID_WO		MAKE_ID2('W', 'O') /* World */
#define ID_SCR		MAKE_ID2('S', 'R') /* Screen */
#define ID_SCRN		MAKE_ID2('S', 'N') /* (depreciated?) */
#define ID_VF		MAKE_ID2('V', 'F') /* VectorFont */
#define ID_TXT		MAKE_ID2('T', 'X') /* Text */
#define ID_SPK		MAKE_ID2('S', 'K') /* Speaker */
#define ID_SO		MAKE_ID2('S', 'O') /* Sound */
#define ID_GR		MAKE_ID2('G', 'R') /* Group */
#define ID_ID		MAKE_ID2('I', 'D') /* (internal use only) */
#define ID_AR		MAKE_ID2('A', 'R') /* Armature */
#define ID_AC		MAKE_ID2('A', 'C') /* Action */
#define ID_SCRIPT	MAKE_ID2('P', 'Y') /* Script (depreciated) */
#define ID_NT		MAKE_ID2('N', 'T') /* NodeTree */
#define ID_BR		MAKE_ID2('B', 'R') /* Brush */
#define ID_PA		MAKE_ID2('P', 'A') /* ParticleSettings */
#define ID_GD		MAKE_ID2('G', 'D') /* GreasePencil */
#define ID_WM		MAKE_ID2('W', 'M') /* WindowManager */
#define ID_MC		MAKE_ID2('M', 'C') /* MovieClip */
#define ID_MSK		MAKE_ID2('M', 'S') /* Mask */

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

#define ID_REAL_USERS(id) (((ID *)id)->us - ((((ID *)id)->flag & LIB_FAKEUSER) ? 1:0))

#define ID_CHECK_UNDO(id) ((GS((id)->name) != ID_SCR) && (GS((id)->name) != ID_WM))

#define ID_BLEND_PATH(_bmain, _id) ((_id)->lib ? (_id)->lib->filepath : (_bmain)->name)

#ifdef GS
#  undef GS
#endif
#define GS(a)	(*((short *)(a)))

#define ID_NEW(a)		if (      (a) && (a)->id.newid ) (a) = (void *)(a)->id.newid
#define ID_NEW_US(a)	if (      (a)->id.newid)       { (a) = (void *)(a)->id.newid;       (a)->id.us++; }
#define ID_NEW_US2(a)	if (((ID *)a)->newid)          { (a) = ((ID  *)a)->newid;     ((ID *)a)->us++;    }

/* id->flag: set frist 8 bits always at zero while reading */
#define LIB_LOCAL		0
#define LIB_EXTERN		1
#define LIB_INDIRECT	2
#define LIB_TEST		8
#define LIB_TESTEXT		(LIB_TEST | LIB_EXTERN)
#define LIB_TESTIND		(LIB_TEST | LIB_INDIRECT)
#define LIB_READ		16
#define LIB_NEEDLINK	32

#define LIB_NEW			256
#define LIB_FAKEUSER	512
/* free test flag */
#define LIB_DOIT		1024
/* tag existing data before linking so we know what is new */
#define LIB_PRE_EXISTING	2048
/* runtime */
#define LIB_ID_RECALC		4096
#define LIB_ID_RECALC_DATA	8192

#ifdef __cplusplus
}
#endif

#endif
