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

/* themes; defines in BIF_resource.h */

// global, button colors

typedef struct ThemeUI {
	char neutral[4];
	char action[4];
	char setting[4];
	char setting1[4];
	char setting2[4];
	char num[4];
	char textfield[4];
	char popup[4];
	char text[4];
	char text_hi[4];
	char menu_back[4];
	char menu_item[4];
	char menu_hilite[4];
	char menu_text[4];
	char menu_text_hi[4];

	char but_drawtype, pad;
	short pad1;

} ThemeUI;

// try to put them all in one, if needed a spacial struct can be created as well
// for example later on, when we introduce wire colors for ob types or so...
typedef struct ThemeSpace {
	char back[4];
	char text[4];	
	char text_hi[4];
	char header[4];
	char panel[4];
	
	char shade1[4];
	char shade2[4];
	
	char hilite[4];
	char grid[4]; 
	
	char wire[4], select[4];
	char active[4], transform[4];
	char vertex[4], vertex_select[4];
	char edge[4], edge_select[4];
	char face[4], face_select[4];
	
	char vertex_size, pad;
	short pad1;
	
} ThemeSpace;


typedef struct bTheme {
	struct bTheme *next, *prev;
	char name[32];
	
	ThemeUI tui;
	
	ThemeSpace tbuts;	
	ThemeSpace tv3d;
	ThemeSpace tfile;
	ThemeSpace tipo;
	ThemeSpace tinfo;	
	ThemeSpace tsnd;
	ThemeSpace tact;
	ThemeSpace tnla;
	ThemeSpace tseq;
	ThemeSpace tima;
	ThemeSpace timasel;
	ThemeSpace text;
	ThemeSpace toops;
	
} bTheme;

typedef struct SolidLight {
	int flag, pad;
	float col[4], spec[4], vec[4];
} SolidLight;

typedef struct UserDef {
	short flag, dupflag;
	int savetime;
	char tempdir[160];	// FILE_MAXDIR length
	char fontdir[160];
	char renderdir[160];
	char textudir[160];
	char plugtexdir[160];
	char plugseqdir[160];
	char pythondir[160];
	char sounddir[160];
	short versions, vrmlflag;	// tmp for export, will be replaced by strubi
	int gameflags;
	int wheellinescroll;
	short uiflag, language;
	int userpref;
	short console_buffer;	//console vars here for tuhopuu compat, --phase
	short console_out;
	int mixbufsize;
	int fontsize;
	short encoding;
	short transopts;
	short menuthreshold1, menuthreshold2;
	char fontname[256];		// FILE_MAXDIR+FILE length
	struct ListBase themes;
	short undosteps, pad0;
	short tb_leftmouse, tb_rightmouse;
	struct SolidLight light[3];
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
#define DRAWVIEWINFO	16
#define EVTTOCONSOLE	32		// print ghost events, here for tuhopuu compat. --phase
								// old flag for hide pulldown was here 
#define FLIPFULLSCREEN	128
#define ALLWINCODECS	256
#define MENUOPENAUTO	512

/* transopts */

#define	TR_TOOLTIPS		1
#define	TR_BUTTONS		2
#define TR_MENUS		4
#define TR_FILESELECT	8
#define TR_TEXTEDIT		16
#define TR_ALL			32

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

