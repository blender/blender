/**
 * blenlib/DNA_scene_types.h (mar-2001 nzc)
 *
 * Renderrecipe and scene decription. The fact that there is a
 * hierarchy here is a bit strange, and not desirable.
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
#ifndef DNA_SCENE_TYPES_H
#define DNA_SCENE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_vec_types.h"
#include "DNA_listBase.h"
#include "DNA_scriptlink_types.h"
#include "DNA_ID.h"
#include "DNA_scriptlink_types.h"

struct FreeCamera;
struct Radio;
struct Object;
struct World;
struct Scene;
struct Image;
struct Group;

typedef struct Base {
	struct Base *next, *prev;
	unsigned int lay, selcol;
	int flag;
	short sx, sy;
	struct Object *object;
} Base;

typedef struct AviCodecData {
	void			*lpFormat;			/* save format */
	void			*lpParms;			/* compressor options */
	unsigned int	cbFormat;			/* size of lpFormat buffer */
	unsigned int	cbParms;			/* size of lpParms buffer */

	unsigned int	fccType;            /* stream type, for consistency */
	unsigned int	fccHandler;         /* compressor */
	unsigned int	dwKeyFrameEvery;    /* keyframe rate */
	unsigned int	dwQuality;          /* compress quality 0-10,000 */
	unsigned int	dwBytesPerSecond;   /* bytes per second */
	unsigned int	dwFlags;            /* flags... see below */
	unsigned int	dwInterleaveEvery;  /* for non-video streams only */
	unsigned int	pad;
} AviCodecData;

typedef struct RenderData {
	struct AviCodecData *avicodecdata;
	
	/* hoe gaat tijd gedefinieerd worden? */
	short cfra, sfra, efra;	/* plaatjes */
	short images, framapto, flag;
	float ctime;			/* hiermee rekenen? */
	float framelen, blurfac;

	/** For UR edge rendering: give the edges this colour */
	float edgeR, edgeG, edgeB;
	
	short fullscreen, xplay, yplay, freqplay;	/* standalone player */
	short depth, attrib, rt1, rt2;				/* standalone player */

	short stereomode;					/* standalone player stereo settings */
	short pad[3];

	short size, maximsize;	/* size in %, max in Kb */
	/* uit buttons: */
	/**
	 * The desired number of pixels in the x direction
	 */
	short xsch;
	/**
	 * The desired number of pixels in the y direction
	 */
	short ysch;
	/**
	 * Adjustment factors for the aspect ratio in the x direction
	 */
	short xasp;
	/**
	 * Adjustment factors for the aspect ratio in the x direction
	 */
	short yasp;
	/**
	 * The number of part to use in the x direction
	 */
	short xparts;
	/**
	 * The number of part to use in the y direction
	 */
	short yparts;
	/* should rewrite this I think... */
	rctf safety, border;
        
	short winpos, planes, imtype;
	/** Mode bits:                                                           */
	/* 0: Enable backbuffering for images                                    */
	short bufflag;
 	short quality;
	/**
	 * Flags for render settings. Use bit-masking to access the settings.
	 * 0: enable sequence output rendering                                   
	 * 1: render daemon                                                      
	 * 4: add extensions to filenames
	 */
	short scemode;

	/**
	 * Flags for render settings. Use bit-masking to access the settings.
	 * The bits have these meanings:
	 * 0: do oversampling                                                    
	 * 1: do shadows                                                         
	 * 2: do gamma correction                                                
	 * 3: ortho (not used?)                                                  
	 * 4: trace (not used?)                                                  
	 * 5: edge shading                                                       
	 * 6: field rendering                                                    
	 * 7: Disables time difference in field calculations                     
	 * 8: Gauss ? is this for sampling?                                      
	 * 9: borders                                                            
	 * 10: panorama                                                          
	 * 11: crop                                                              
	 * 12: save SGI movies with Cosmo hardware (????)                        
	 * 13: odd field first rendering                                         
	 * 14: motion blur                                                       
	 * 15: use unified renderer for this pic!                                
	 */
	short mode;

	/**
	 * What to do with the sky/background. Picks sky/premul/key
	 * blending for the background
	 */
	short alphamode;
	/**
	 * Toggles whether to apply a gamma correction for subpixel to
	 * pixel blending
	 */
	short dogamma;
	/**
	 * The number of samples to use per pixel.
	 */
	short osa;
	short frs_sec, edgeint;

	/** For unified renderer: reduce intensity on boundaries with
	 * identical materials with this number.*/
	short same_mat_redux, pad_3[3];
	
	/**
	 * The gamma for the normal rendering. Used when doing
	 * oversampling, to correctly blend subpixels to pixels.  */
	float gamma;
	/** post-production settings. Don't really belong here */
	float postmul, postgamma, postadd, postigamma;
	
	char backbuf[160], pic[160], ftype[160];
	
} RenderData;


typedef struct GameFraming {
	float col[3];
	char type, pad1, pad2, pad3;
} GameFraming;

#define SCE_GAMEFRAMING_BARS   0
#define SCE_GAMEFRAMING_EXTEND 1
#define SCE_GAMEFRAMING_SCALE  2

typedef struct Scene {
	ID id;
	struct Object *camera;
	struct World *world;
	
	struct Scene *set;
	struct Image *ima;
	
	ListBase base;
	struct Base *basact;
	struct Group *group;
	
	float cursor[3];
	unsigned int lay;

	/* enkele realtime vars */
	struct FreeCamera *fcam;
	
	void *ed;
	struct Radio *radio;
	void *sumohandle;
	
	struct GameFraming framing;

	/* migrate or replace? depends on some internal things... */
	/* no, is on the right place (ton) */
	struct RenderData r;
	
	ScriptLink scriptlink;
} Scene;


/* **************** RENDERDATA ********************* */

/* bufflag */
#define R_BACKBUF		1
#define R_BACKBUFANIM	2
#define R_FRONTBUF		4
#define R_FRONTBUFANIM	8

/* mode */
#define R_OSA			0x0001	
#define R_SHADOW		0x0002	
#define R_GAMMA			0x0004
#define R_ORTHO			0x0008
#define R_TRACE			0x0010
#define R_EDGE			0x0020
#define R_FIELDS		0x0040
#define R_FIELDSTILL	0x0080
#define R_GAUSS			0x0100
#define R_BORDER		0x0200
#define R_PANORAMA		0x0400
#define R_MOVIECROP		0x0800
#define R_COSMO			0x1000
/* deze verschillen tussen IrisGL en OpenGL!!! */
#define R_ODDFIELD		0x2000
#define R_MBLUR			0x4000
#define R_UNIFIED       0x8000

/* scemode */
#define R_DOSEQ			0x0001
#define R_BG_RENDER		0x0002

#define R_EXTENSION		0x0010
#define R_OGL			0x0020

/* alphamode */
#define R_ADDSKY		0
#define R_ALPHAPREMUL	1
#define R_ALPHAKEY		2

/* planes */
#define R_PLANES24		24
#define R_PLANES32		32
#define R_PLANESBW		8

/* imtype */
#define R_TARGA		0
#define R_IRIS		1
#define R_HAMX		2
#define R_FTYPE		3
#define R_JPEG90	4
#define R_MOVIE		5
#define R_IRIZ		7
#define R_RAWTGA	14
#define R_AVIRAW	15
#define R_AVIJPEG	16
#define R_PNG		17
#define R_AVICODEC	18


/* **************** RENDER ********************* */
/* mode flag is same as for renderdata */
/* flag */
#define R_ZTRA			1
#define R_HALO			2
#define R_SEC_FIELD		4
#define R_LAMPHALO		8
#define R_RENDERING		16
#define R_ANIMRENDER	32

/* vlakren->flag */
#define R_SMOOTH		1
#define R_VISIBLE		2
#define R_NOPUNOFLIP	8
#define R_CMAPCODE		16
#define R_FACE_SPLIT	32

/* vertren->texofs (texcoordinaten offset vanaf vertren->orco */
#define R_UVOFS3	1

/* **************** SCENE ********************* */
#define RAD_PHASE_PATCHES	1
#define RAD_PHASE_FACES		2

/* base->flag en ob->flag */
#define BA_WASSEL			2
#define BA_PARSEL			4
#define BA_WHERE_UPDATE		8
#define BA_DISP_UPDATE		16
#define BA_DO_IPO			32
#define BA_FROMSET			128
#define OB_DO_IMAT			256
#define OB_FROMDUPLI		512
#define OB_DONE				1024
#define OB_RADIO			2048
#define OB_FROMGROUP		4096

/* sce->flag */
#define SCE_ADDSCENAME		1

/* return flag next_object function */
#define F_START			0
#define F_SCENE			1
#define F_SET			2
#define F_DUPLI			3

#ifdef __cplusplus
}
#endif

#endif
