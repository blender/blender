/**
 * blenkernel/DNA_userdef_types.h (mar-2001 nzc)
 *
 *	$Id$
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

#ifndef DNA_USERDEF_TYPES_H
#define DNA_USERDEF_TYPES_H

typedef struct UserDef {
	short flag, dupflag;
	int savetime;
	char tempdir[64];
	char fontdir[64];
	char renderdir[64];
	char textudir[64];
	char plugtexdir[64];
	char plugseqdir[64];
	char pythondir[64];
	char sounddir[64];
	short versions, vrmlflag;	// tmp for export, will be replaced by strubi
	int gameflags;
	int wheellinescroll;
	short uiflag, pad2;
} UserDef;

extern UserDef U; /* from usiblender.c !!!! */

/* ***************** USERDEF ****************** */

/* flag */
#define AUTOSAVE		1
#define AUTOGRABGRID	2
#define AUTOROTGRID		4
#define AUTOSIZEGRID	8
#define SCENEGLOBAL		16
#define TRACKBALL		32
#define DUPLILINK		64
#define FSCOLLUM		128
#define MAT_ON_OB		256
#define NO_CAPSLOCK		512
#define VIEWMOVE		1024
#define TOOLTIPS		2048
#define TWOBUTTONMOUSE	4096
#define NONUMPAD		8192

/* uiflag */

#define	KEYINSERTACT	1
#define	KEYINSERTOBJ	2
#define WHEELZOOMDIR	4
#define FILTERFILEEXTS	8

/* dupflag */
#define DUPMESH			1
#define DUPCURVE		2
#define DUPSURF			4
#define DUPFONT			8
#define DUPMBALL		16
#define DUPLAMP			32
#define DUPIPO			64
#define DUPMAT			128
#define DUPTEX			256
#define	DUPARM			512
#define	DUPACT			1024

/* gameflags */
#define USERDEF_VERTEX_ARRAYS_BIT        0
#define USERDEF_DISABLE_SOUND_BIT        1
#define USERDEF_DISABLE_MIPMAP_BIT       2


#define USERDEF_VERTEX_ARRAYS        (1 << USERDEF_VERTEX_ARRAYS_BIT)
#define USERDEF_DISABLE_SOUND        (1 << USERDEF_DISABLE_SOUND_BIT)
#define USERDEF_DISABLE_MIPMAP       (1 << USERDEF_DISABLE_MIPMAP_BIT)


/* vrml flag */

#define USERDEF_VRML_LAYERS		1
#define USERDEF_VRML_AUTOSCALE	2
#define USERDEF_VRML_TWOSIDED	4

#endif

