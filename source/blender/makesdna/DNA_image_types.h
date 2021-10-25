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

/** \file DNA_image_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_IMAGE_TYPES_H__
#define __DNA_IMAGE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_color_types.h"  /* for color management */

struct PackedFile;
struct Scene;
struct anim;
struct MovieCache;
struct RenderResult;
struct GPUTexture;

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
typedef struct ImageUser {
	struct Scene *scene;		/* to retrieve render result */

	int framenr;				/* movies, sequences: current to display */
	int frames;					/* total amount of frames to use */
	int offset, sfra;			/* offset within movie, start frame in global time */
	char fie_ima, cycl;		/* fields/image in movie, cyclic flag */
	char ok;

	char multiview_eye;			/* multiview current eye - for internal use of drawing routines */
	short pass;
	short pad;

	short multi_index, view, layer;	 /* listbase indices, for menu browsing or retrieve buffer */
	short flag;
} ImageUser;

typedef struct ImageAnim {
	struct ImageAnim *next, *prev;
	struct anim *anim;
} ImageAnim;

typedef struct ImageView {
	struct ImageView *next, *prev;
	char name[64];			/* MAX_NAME */
	char filepath[1024];	/* 1024 = FILE_MAX */
} ImageView;

typedef struct ImagePackedFile {
	struct ImagePackedFile *next, *prev;
	struct PackedFile *packedfile;
	char filepath[1024];	/* 1024 = FILE_MAX */
} ImagePackedFile;

typedef struct RenderSlot {
	char name[64];  /* 64 = MAX_NAME */
} RenderSlot;

/* iuser->flag */
#define	IMA_ANIM_ALWAYS		1
#define IMA_ANIM_REFRESHED	2
/* #define IMA_DO_PREMUL	4 */
#define IMA_NEED_FRAME_RECALC	8
#define IMA_SHOW_STEREO		16

enum {
	TEXTARGET_TEXTURE_2D = 0,
	TEXTARGET_TEXTURE_CUBE_MAP = 1,
	TEXTARGET_COUNT = 2
};

typedef struct Image {
	ID id;
	
	char name[1024];			/* file path, 1024 = FILE_MAX */
	
	struct MovieCache *cache;	/* not written in file */
	struct GPUTexture *gputexture[2]; /* not written in file 2 = TEXTARGET_COUNT */
	
	/* sources from: */
	ListBase anims;
	struct RenderResult *rr;

	struct RenderResult *renders[8]; /* IMA_MAX_RENDER_SLOT */
	short render_slot, last_render_slot;

	int flag;
	short source, type;
	int lastframe;

	/* texture page */
	short tpageflag, totbind;
	short xrep, yrep;
	short twsta, twend;
	unsigned int bindcode[2]; /* only for current image... 2 = TEXTARGET_COUNT */
	char pad1[4];
	unsigned int *repbind;	/* for repeat of parts of images */
	
	struct PackedFile *packedfile DNA_DEPRECATED; /* deprecated */
	struct ListBase packedfiles;
	struct PreviewImage *preview;

	/* game engine tile animation */
	float lastupdate;
	int lastused;
	short animspeed;

	short ok;
	
	/* for generated images */
	int gen_x, gen_y;
	char gen_type, gen_flag;
	short gen_depth;
	float gen_color[4];
	
	/* display aspect - for UV editing images resized for faster openGL display */
	float aspx, aspy;

	/* color management */
	ColorManagedColorspaceSettings colorspace_settings;
	char alpha_mode;

	char pad[5];

	/* Multiview */
	char eye; /* for viewer node stereoscopy */
	char views_format;
	ListBase views;  /* ImageView */
	struct Stereo3dFormat *stereo3d_format;

	RenderSlot render_slots[8];  /* 8 = IMA_MAX_RENDER_SLOT */
} Image;


/* **************** IMAGE ********************* */

/* Image.flag */
enum {
	IMA_FIELDS              = (1 << 0),
	IMA_STD_FIELD           = (1 << 1),
#ifdef DNA_DEPRECATED
	IMA_DO_PREMUL           = (1 << 2),  /* deprecated, should not be used */
#endif
	IMA_REFLECT             = (1 << 4),
	IMA_NOCOLLECT           = (1 << 5),
	//IMA_DONE_TAG          = (1 << 6),  // UNUSED
	IMA_OLD_PREMUL          = (1 << 7),
	// IMA_CM_PREDIVIDE     = (1 << 8),  /* deprecated, should not be used */
	IMA_USED_FOR_RENDER     = (1 << 9),
	IMA_USER_FRAME_IN_RANGE = (1 << 10), /* for image user, but these flags are mixed */
	IMA_VIEW_AS_RENDER      = (1 << 11),
	IMA_IGNORE_ALPHA        = (1 << 12),
	IMA_DEINTERLACE         = (1 << 13),
	IMA_USE_VIEWS           = (1 << 14),
	// IMA_IS_STEREO        = (1 << 15), /* deprecated */
	// IMA_IS_MULTIVIEW     = (1 << 16), /* deprecated */
};

/* Image.tpageflag */
#define IMA_TILES			1
#define IMA_TWINANIM		2
#define IMA_COLCYCLE		4	/* Depreciated */
#define IMA_MIPMAP_COMPLETE 8   /* all mipmap levels in OpenGL texture set? */
#define IMA_CLAMP_U			16 
#define IMA_CLAMP_V			32
#define IMA_TPAGE_REFRESH	64
#define IMA_GLBIND_IS_DATA	128 /* opengl image texture bound as non-color data */

/* ima->type and ima->source moved to BKE_image.h, for API */

/* render */
#define IMA_MAX_RENDER_TEXT		512
#define IMA_MAX_RENDER_SLOT		8

/* gen_flag */
#define IMA_GEN_FLOAT		1

/* alpha_mode */
enum {
	IMA_ALPHA_STRAIGHT = 0,
	IMA_ALPHA_PREMUL = 1,
};

#endif
