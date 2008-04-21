/**
 * imbuf_patch.h
 *
 * These are some definitions to make imbuf more independent from the
 * rest of the blender code. Most of these are dirty and should not
 * really exist.
 *
 * $Id$ *
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
#ifndef IMBUF_PATCH_H
#define IMBUF_PATCH_H

/* most of imbuf uses this aloc, and it will disappear soon
 * (hopefully) (25-10-2001 nzc) */
#include "MEM_guardedalloc.h"

struct ImBuf;

/* originally, these were defines ... */
typedef unsigned char  uchar;

/* should not be used at all */
#define TRUE 1
#define FALSE 0

/* Endianness: flip the byte order. It's strange that this is needed..
 * After all, there is an internal endian.{c,h}... */
#if defined(__sgi) || defined (__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__hppa__) || defined (__BIG_ENDIAN__)
#define MAKE_ID(a,b,c,d) ( (int)(a)<<24 | (int)(b)<<16 | (c)<<8 | (d) )
#else
#define MAKE_ID(a,b,c,d) ( (int)(d)<<24 | (int)(c)<<16 | (b)<<8 | (a) )
#endif

/* These defines loop back to the internal Blender memory management
 * system, implemented in blenlib. */
#define NEW(x) (x*)MEM_mallocN(sizeof(x),# x) 
#define mallocstruct(x,y) (x*)malloc((y)* sizeof(x))
#define callocstruct(x,y) (x*)calloc((y), sizeof(x))

/* These vars are used thoughout the image buffer for conversions. */
extern float rgbyuv[4][4];
extern float yuvrgb[4][4];
extern float rgbbeta[4][4];

/* This one helps debugging. */
extern int IB_verbose;

/* These ID's are used for checking memory blocks. See blenlib for
 * more details. This set is only used in the imbuf internally. */

#define CAT  MAKE_ID('C','A','T',' ')
#define FORM MAKE_ID('F','O','R','M')
#define ILBM MAKE_ID('I','L','B','M')
#define BMHD MAKE_ID('B','M','H','D')
#define CMAP MAKE_ID('C','M','A','P')
#define CAMG MAKE_ID('C','A','M','G')
#define BODY MAKE_ID('B','O','D','Y')

#define ANIM MAKE_ID('A','N','I','M')
#define ADAT MAKE_ID('A','D','A','T')
#define CODE MAKE_ID('C','O','D','E')
#define ANHD MAKE_ID('A','N','H','D')
#define DLTA MAKE_ID('D','L','T','A')
#define BLCK MAKE_ID('B','L','C','K')

#define MAXRUN 126
#define MAXDAT 126
#define IFFNOP 128

#define camg ftype

#define LI_rect		IB_rect
#define LI_planes	IB_planes
#define LI_kcmap	IB_cmap
#define LI_cmap		IB_cmap
#define LI_freem	IB_freem
#define LI_test		IB_test

#define SI_rect		IB_rect
#define SI_planes	IB_planes
#define SI_kcmap	IB_cmap
#define SI_cmap		IB_cmap
#define SI_vert		IB_vert

#endif

