/**
 * blenlib/BKE_global.h (mar-2001 nzc)
 *
 * Global settings, handles, pointers. This is the root for finding
 * any data in Blender. This block is not serialized, but built anew
 * for every fresh Blender run.
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
#ifndef BKE_GLOBAL_H
#define BKE_GLOBAL_H

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* forwards */
struct Main;
struct Object;
struct BME_Glob;

typedef struct Global {

	/* active pointers */
	struct Main *main;
	
	/* strings: lastsaved */
	char ima[256], sce[256], lib[256];

	/* flag: if != 0 G.sce contains valid relative base path */
	int relbase_valid;

	/* strings of recent opend files */
	struct ListBase recent_files;
        
	short afbreek, moving, file_loaded;
	short background;
	short winpos, displaymode;	/* used to be in Render */
	short rendering;			/* to indicate render is busy, prevent renderwindow events etc */

	short rt;
	int f;

	/* Used for BMesh transformations */
	struct BME_Glob *editBMesh;
    
	/* Frank's variables */
	int	save_over;

	/* Rob's variables (keep here for WM recode) */
	int have_quicktime;
	int ui_international;
	int charstart;
	int charmin;
	int charmax;
	struct VFont *selfont;
	struct ListBase ttfdata;

	/* libtiff flag used to determine if shared library loaded for libtiff*/
	int have_libtiff;

	/* this variable is written to / read from FileGlobal->fileflags */
	int fileflags;
    
	/* save the allowed windowstate of blender when using -W or -w */
	int windowstate;

	/* ndof device found ? */
	int ndofdevice;
	
	/* confusing... G.f and G.flags */
	int flags;

} Global;

/* **************** GLOBAL ********************* */

/* G.f */
#define G_RENDER_OGL	(1 <<  0)
#define G_SWAP_EXCHANGE	(1 <<  1)
/* also uses G_FILE_AUTOPLAY */
#define G_RENDER_SHADOW	(1 <<  3)
#define G_BACKBUFSEL	(1 <<  4)
#define G_PICKSEL		(1 <<  5)

/* #define G_FACESELECT	(1 <<  8) use (mesh->editflag & ME_EDIT_PAINT_MASK) */

#define G_DEBUG			(1 << 12)
#define G_DOSCRIPTLINKS (1 << 13)

/* #define G_NOFROZEN	(1 << 17) also removed */
#define G_GREASEPENCIL 	(1 << 17)

/* #define G_AUTOMATKEYS	(1 << 30)   also removed */

/* G.fileflags */

#define G_AUTOPACK               (1 << 0)
#define G_FILE_COMPRESS          (1 << 1)
#define G_FILE_AUTOPLAY          (1 << 2)
#define G_FILE_ENABLE_ALL_FRAMES (1 << 3)				/* deprecated */
#define G_FILE_SHOW_DEBUG_PROPS  (1 << 4)				/* deprecated */
#define G_FILE_SHOW_FRAMERATE    (1 << 5)				/* deprecated */
#define G_FILE_SHOW_PROFILE      (1 << 6)				/* deprecated */
#define G_FILE_LOCK              (1 << 7)
#define G_FILE_SIGN              (1 << 8)
#define G_FIle_PUBLISH			 (1 << 9)
#define G_FILE_NO_UI			 (1 << 10)
#define G_FILE_GAME_TO_IPO		 (1 << 11)				/* deprecated */
#define G_FILE_GAME_MAT			 (1 << 12)				/* deprecated */
#define G_FILE_DISPLAY_LISTS	 (1 << 13)				/* deprecated */
#define G_FILE_SHOW_PHYSICS		 (1 << 14)				/* deprecated */
#define G_FILE_GAME_MAT_GLSL	 (1 << 15)				/* deprecated */
#define G_FILE_GLSL_NO_LIGHTS	 (1 << 16)				/* deprecated */
#define G_FILE_GLSL_NO_SHADERS	 (1 << 17)				/* deprecated */
#define G_FILE_GLSL_NO_SHADOWS	 (1 << 18)				/* deprecated */
#define G_FILE_GLSL_NO_RAMPS	 (1 << 19)				/* deprecated */
#define G_FILE_GLSL_NO_NODES	 (1 << 20)				/* deprecated */
#define G_FILE_GLSL_NO_EXTRA_TEX (1 << 21)				/* deprecated */
#define G_FILE_IGNORE_DEPRECATION_WARNINGS	(1 << 22)	/* deprecated */
#define G_FILE_RECOVER			 (1 << 23)
#define G_FILE_RELATIVE_REMAP	 (1 << 24)

/* G.windowstate */
#define G_WINDOWSTATE_USERDEF		0
#define G_WINDOWSTATE_BORDER		1
#define G_WINDOWSTATE_FULLSCREEN	2

/* ENDIAN_ORDER: indicates what endianness the platform where the file was
 * written had. */
#define L_ENDIAN	1
#define B_ENDIAN	0
extern short ENDIAN_ORDER;

/* G.moving, signals drawing in (3d) window to denote transform */
#define G_TRANSFORM_OBJ			1
#define G_TRANSFORM_EDIT		2
#define G_TRANSFORM_MANIP		4
#define G_TRANSFORM_PARTICLE	8

/* G.special1 */

/* Memory is allocated where? blender.c */
extern Global G;

#ifdef __cplusplus
}
#endif
	
#endif


