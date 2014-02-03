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
#ifndef __BKE_GLOBAL_H__
#define __BKE_GLOBAL_H__

/** \file BKE_global.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 *  \section aboutglobal Global settings
 *   Global settings, handles, pointers. This is the root for finding
 *   any data in Blender. This block is not serialized, but built anew
 *   for every fresh Blender run.
 */

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* forwards */
struct Main;
struct Object;

typedef struct Global {

	/* active pointers */
	struct Main *main;
	
	/* strings: lastsaved */
	char ima[1024], lib[1024]; /* 1024 = FILE_MAX */

	/* when set: G.main->name contains valid relative base path */
	bool relbase_valid;
	bool file_loaded;
	bool save_over;

	/* strings of recent opened files */
	struct ListBase recent_files;

	/* has escape been pressed or Ctrl+C pressed in background mode, used for render quit */
	bool is_break;

	bool background;
	bool factory_startup;

	short moving;
	short winpos, displaymode;  /* used to be in Render */

	/* to indicate render is busy, prevent renderwindow events etc */
	bool is_rendering;

	/* debug value, can be set from the UI and python, used for testing nonstandard features */
	short debug_value;

	/* saved to the blend file as FileGlobal.globalf,
	 * however this is now only used for runtime options */
	int f;

	/* debug flag, G_DEBUG, G_DEBUG_PYTHON & friends, set python or command line args */
	int debug;

	bool have_quicktime;

	/* this variable is written to / read from FileGlobal->fileflags */
	int fileflags;

	/* save the allowed windowstate of blender when using -W or -w (GHOST_TWindowState) */
	int windowstate;

	/* message to use when autoexec fails */
	char autoexec_fail[200];
} Global;

/* **************** GLOBAL ********************* */

/* G.f */
#define G_RENDER_OGL    (1 <<  0)
#define G_SWAP_EXCHANGE (1 <<  1)
/* also uses G_FILE_AUTOPLAY */
/* #define G_RENDER_SHADOW	(1 <<  3) */ /* temp flag, removed */
#define G_BACKBUFSEL    (1 <<  4)
#define G_PICKSEL       (1 <<  5)

/* #define G_FACESELECT	(1 <<  8) use (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) */

#define G_SCRIPT_AUTOEXEC (1 << 13)
#define G_SCRIPT_OVERRIDE_PREF (1 << 14) /* when this flag is set ignore the userprefs */
#define G_SCRIPT_AUTOEXEC_FAIL (1 << 15)
#define G_SCRIPT_AUTOEXEC_FAIL_QUIET (1 << 16)

/* #define G_NOFROZEN	(1 << 17) also removed */
/* #define G_GREASEPENCIL   (1 << 17)   also removed */

/* #define G_AUTOMATKEYS	(1 << 30)   also removed */

/* G.debug */
enum {
	G_DEBUG =           (1 << 0), /* general debug flag, print more info in unexpected cases */
	G_DEBUG_FFMPEG =    (1 << 1),
	G_DEBUG_PYTHON =    (1 << 2), /* extra python info */
	G_DEBUG_EVENTS =    (1 << 3), /* input/window/screen events */
	G_DEBUG_HANDLERS =  (1 << 4), /* events handling */
	G_DEBUG_WM =        (1 << 5), /* operator, undo */
	G_DEBUG_JOBS =      (1 << 6), /* jobs time profiling */
	G_DEBUG_FREESTYLE = (1 << 7), /* freestyle messages */
	G_DEBUG_DEPSGRAPH = (1 << 8), /* depsgraph messages */
};

#define G_DEBUG_ALL  (G_DEBUG | G_DEBUG_FFMPEG | G_DEBUG_PYTHON | G_DEBUG_EVENTS | G_DEBUG_WM | G_DEBUG_JOBS | \
                      G_DEBUG_FREESTYLE | G_DEBUG_DEPSGRAPH)


/* G.fileflags */

#define G_AUTOPACK               (1 << 0)
#define G_FILE_COMPRESS          (1 << 1)
#define G_FILE_AUTOPLAY          (1 << 2)
#define G_FILE_ENABLE_ALL_FRAMES (1 << 3)               /* deprecated */
#define G_FILE_SHOW_DEBUG_PROPS  (1 << 4)               /* deprecated */
#define G_FILE_SHOW_FRAMERATE    (1 << 5)               /* deprecated */
/* #define G_FILE_SHOW_PROFILE   (1 << 6) */            /* deprecated */
#define G_FILE_LOCK              (1 << 7)
#define G_FILE_SIGN              (1 << 8)
#define G_FILE_USERPREFS         (1 << 9)
#define G_FILE_NO_UI             (1 << 10)
/* #define G_FILE_GAME_TO_IPO    (1 << 11) */           /* deprecated */
#define G_FILE_GAME_MAT          (1 << 12)              /* deprecated */
/* #define G_FILE_DISPLAY_LISTS  (1 << 13) */           /* deprecated */
#define G_FILE_SHOW_PHYSICS      (1 << 14)              /* deprecated */
#define G_FILE_GAME_MAT_GLSL     (1 << 15)              /* deprecated */
/* #define G_FILE_GLSL_NO_LIGHTS     (1 << 16) */       /* deprecated */
#define G_FILE_GLSL_NO_SHADERS   (1 << 17)              /* deprecated */
#define G_FILE_GLSL_NO_SHADOWS   (1 << 18)              /* deprecated */
#define G_FILE_GLSL_NO_RAMPS     (1 << 19)              /* deprecated */
#define G_FILE_GLSL_NO_NODES     (1 << 20)              /* deprecated */
#define G_FILE_GLSL_NO_EXTRA_TEX (1 << 21)              /* deprecated */
#define G_FILE_IGNORE_DEPRECATION_WARNINGS  (1 << 22)   /* deprecated */
#define G_FILE_RECOVER           (1 << 23)
#define G_FILE_RELATIVE_REMAP    (1 << 24)
#define G_FILE_HISTORY           (1 << 25)
#define G_FILE_MESH_COMPAT       (1 << 26)              /* BMesh option to save as older mesh format */
#define G_FILE_SAVE_COPY         (1 << 27)              /* restore paths after editing them */

#define G_FILE_FLAGS_RUNTIME (G_FILE_NO_UI | G_FILE_RELATIVE_REMAP | G_FILE_MESH_COMPAT | G_FILE_SAVE_COPY)

/* ENDIAN_ORDER: indicates what endianness the platform where the file was
 * written had. */
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#  error Either __BIG_ENDIAN__ or __LITTLE_ENDIAN__ must be defined.
#endif

/* there is really no good place for this */
#if defined(FREE_WINDOWS) && ((__GNUC__ < 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ < 6)))
#  error "Mingw requires GCC 4.6 minimum"
#endif

#define L_ENDIAN    1
#define B_ENDIAN    0

#ifdef __BIG_ENDIAN__
#  define ENDIAN_ORDER B_ENDIAN
#else
#  define ENDIAN_ORDER L_ENDIAN
#endif

/* G.moving, signals drawing in (3d) window to denote transform */
#define G_TRANSFORM_OBJ         1
#define G_TRANSFORM_EDIT        2
#define G_TRANSFORM_SEQ         4
#define G_TRANSFORM_FCURVES     8

/* G.special1 */

/* Memory is allocated where? blender.c */
extern Global G;

#ifdef __cplusplus
}
#endif
	
#endif
