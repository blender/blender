/**
 * blenlib/BKE_global.h (mar-2001 nzc)
 *
 * Global settings, handles, pointers. This is the root for finding
 * any data in Blender. This block is not serialized, but built anew
 * for every fresh Blender run.
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
#ifndef BKE_GLOBAL_H
#define BKE_GLOBAL_H

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/* forwards */
struct View3D;
struct View2D;
struct SpaceIpo;
struct SpaceButs;
struct SpaceImage;
struct SpaceOops;
struct SpaceText;
struct SpaceSound;
struct SpaceAction;
struct SpaceNla;
struct Main;
struct Scene;
struct bScreen;
struct Object;
struct bSoundListener;
struct BMF_Font;
struct EditMesh;

#define UNDO_MAX	64
#define UNDO_EDIT_MAX 	64

typedef struct UndoBufferEdit {

	void *datablock;
	char name[64];

} UndoBufferEdit;


typedef struct Global {

    /* active pointers */
    struct View3D *vd;
    struct View2D *v2d;
    struct SpaceIpo *sipo;
    struct SpaceButs *buts;
    struct SpaceImage *sima;
    struct SpaceOops *soops;
    struct SpaceSound *ssound;
	struct SpaceAction *saction;	/* __NLA */
	struct SpaceNla *snla;
    struct Main *main;
    struct Scene *scene;				/* denk aan file.c */
    struct bScreen *curscreen;
    struct Object *obedit;
	
    /* fonts, allocated global data */
    struct BMF_Font *font, *fonts, *fontss;
    
    /* strings: lastsaved */
    char ima[160], sce[160], lib[160];
    
    /* totalen */
    short totobj, totlamp, totobjsel, totcurve, totmesh, totmat;
    int totvert, totface, totvertsel, totfacesel;
    
    short machine, afbreek, moving, colact, zbuf;
    short qual, background, imagewin, animspeed;
	/**
	 * The current version of Blender.
	 */
    short version;
	short simulf, fields, order, rt;
	int f;

    /* Editmode lists */
	struct EditMesh *editMesh;
    
    float textcurs[4][2];
    
    /* Frank's variables */
    int renderd;
    int real_sfra, real_efra;
    int	save_over;

	/* Reevan's __NLA variables */
	struct	Object	*obpose;		/* Current posable object */
	struct	ListBase edbo;			/* Armature Editmode bones */
 
	/* Rob's variables */
	int have_quicktime;
	int ui_international;

    /* this variable is written to / read from FileGlobal->fileflags */
    int fileflags;
    
	/* save the allowed windowstate of blender when using -W or -w */
	int windowstate;

	/* Janco's playing ground */
	struct bSoundListener* listener;

    /* Test thingy for Nzc */
    int magic;       /* toggle use of experimental render pipe       */
	int compat;      /* toggle compatibility mode for edge rendering */
	int notonlysolid;/* T-> also edge-render transparent faces       */
    int useRscale;   /* bitflag for using colour scaling             */
    int useGscale;   /* bitflag for using colour scaling             */
    int useBscale;   /* bitflag for using colour scaling             */
    float cscale[4]; /* sliders for colour scaling                   */
    int Rhisto;      /* flags for making histograms                  */
    int Ghisto;
    int Bhisto;

    /* special versions */
    short special1, special2;

    int flags;

    /* editmode undo - written by intrr, ported by Det. Thorn  */
    struct UndoBufferEdit undo_edit[UNDO_EDIT_MAX+1];/* need one more for undoing first undo */
    int undo_edit_level;			/* index of last undo buffer created */
    int undo_edit_highest;			/* index of highest undo buffer in use */
    void *undo_last_data;			/* pointer to last datablock edited */
    void (*undo_clear)(void);		/* pointer to function to free the undo data */

        
} Global;

/* **************** GLOBAL ********************* */

/* G.f */
#define G_DISABLE_OK	1
#define G_PLAYANIM		2
#define G_TEST_DUPLI	4
#define G_SIMULATION	8
#define G_BACKBUFSEL	16
#define G_PICKSEL		32
#define G_DRAWNORMALS	64
#define G_DRAWFACES		128
#define G_FACESELECT	256
#define G_DRAW_EXT		512
#define G_VERTEXPAINT	1024
#define G_ALLEDGES		2048
#define G_DEBUG			4096
#define G_SCENESCRIPT	8192
#define G_PROPORTIONAL	16384
#define G_WEIGHTPAINT	32768	/* __NLA */
#define G_TEXTUREPAINT	65536
#define G_NOFROZEN	(1 << 17) // frozen modules inactive
#define G_DRAWEDGES	(1 << 18)

/* G.fileflags */

#define G_AUTOPACK_BIT                  0
#define G_FILE_COMPRESS_BIT             1
#define G_FILE_AUTOPLAY_BIT             2
#define G_FILE_ENABLE_ALL_FRAMES_BIT    3
#define G_FILE_SHOW_DEBUG_PROPS_BIT     4
#define G_FILE_SHOW_FRAMERATE_BIT       5
#define G_FILE_SHOW_PROFILE_BIT         6
#define G_FILE_LOCK_BIT                 7
#define G_FILE_SIGN_BIT                 8
#define G_FILE_PUBLISH_BIT				9

#define G_AUTOPACK               (1 << G_AUTOPACK_BIT)
#define G_FILE_COMPRESS          (1 << G_FILE_COMPRESS_BIT)
#define G_FILE_AUTOPLAY          (1 << G_FILE_AUTOPLAY_BIT)
#define G_FILE_ENABLE_ALL_FRAMES (1 << G_FILE_ENABLE_ALL_FRAMES_BIT)
#define G_FILE_SHOW_DEBUG_PROPS  (1 << G_FILE_SHOW_DEBUG_PROPS_BIT)
#define G_FILE_SHOW_FRAMERATE    (1 << G_FILE_SHOW_FRAMERATE_BIT)
#define G_FILE_SHOW_PROFILE      (1 << G_FILE_SHOW_PROFILE_BIT)
#define G_FILE_LOCK              (1 << G_FILE_LOCK_BIT)
#define G_FILE_SIGN              (1 << G_FILE_SIGN_BIT)
#define G_FILE_PUBLISH			 (1 << G_FILE_PUBLISH_BIT)

/* G.windowstate */
#define G_WINDOWSTATE_USERDEF		0
#define G_WINDOWSTATE_BORDER		1
#define G_WINDOWSTATE_FULLSCREEN	2

/* G.simulf */
#define G_LOADFILE	2
#define G_RESTART	4
#define G_QUIT		8
#define G_SETSCENE	16

/* G.flags: double? */
#define G_FLAGS_AUTOPLAY_BIT	2 
#define G_FLAGS_AUTOPLAY		(1 << G_FLAGS_AUTOPLAY_BIT)

/* G.qual */
#define R_SHIFTKEY	1
#define L_SHIFTKEY	2
#define LR_SHIFTKEY 3
#define R_ALTKEY	4
#define L_ALTKEY	8
#define LR_ALTKEY	12
#define R_CTRLKEY	16
#define L_CTRLKEY	32
#define LR_CTRLKEY  48

/* G.order: indicates what endianness the platform where the file was
 * written had. */
#define L_ENDIAN	1
#define B_ENDIAN	0

/* G.special1 */

/* Memory is allocated where? blender.c */
extern Global G;

#ifdef __cplusplus
}
#endif
	
#endif

