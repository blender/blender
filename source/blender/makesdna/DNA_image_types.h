/**
 * blenlib/DNA_image_types.h (mar-2001 nzc)
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
#ifndef DNA_IMAGE_TYPES_H
#define DNA_IMAGE_TYPES_H

#include "DNA_ID.h"

struct PackedFile;
struct anim;
struct ImBuf;

typedef struct Image {
	ID id;
	
	char name[160];
	
	struct anim *anim;
	struct ImBuf *ibuf;
	struct ImBuf *mipmap[10];
	
	short ok, flag;
	short lastframe, lastquality;

	/* texture pagina */
	short tpageflag, totbind;
	short xrep, yrep;
	short twsta, twend;
	unsigned int bindcode;
	unsigned int *repbind;	/* om subregio's te kunnen repeaten */
	
	struct PackedFile * packedfile;

	float lastupdate;
	short animspeed;
	short reserved1;
} Image;

/*  in Image struct */
#define MAXMIPMAP	10

/* **************** IMAGE ********************* */

/* flag */
#define IMA_HALVE		1
#define IMA_BW			2
#define IMA_FROMANIM	4
#define IMA_USED		8
#define	IMA_REFLECT		16

/* tpageflag */
#define IMA_TILES		1
#define IMA_TWINANIM	2
#define IMA_COLCYCLE	4	/* Depreciated */

#endif
