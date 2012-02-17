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

#include "DNA_ID.h"

struct PackedFile;
struct Scene;
struct anim;
struct ImBuf;
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
	char ok, pad;

	short multi_index, layer, pass;	 /* listbase indices, for menu browsing or retrieve buffer */

	short flag;
	
	int pad2;

} ImageUser;

/* iuser->flag */
#define	IMA_ANIM_ALWAYS		1
#define IMA_ANIM_REFRESHED	2
/* #define IMA_DO_PREMUL	4 */

typedef struct Image {
	ID id;
	
	char name[1024];			/* file path, 1024 = FILE_MAX */
	
	ListBase ibufs;					/* not written in file */
	struct GPUTexture *gputexture;	/* not written in file */
	
	/* sources from: */
	struct anim *anim;
	struct RenderResult *rr;

	struct RenderResult *renders[8]; /* IMA_MAX_RENDER_SLOT */
	short render_slot, last_render_slot;
	
	short ok, flag;
	short source, type;
	int lastframe;

	/* texture page */
	short tpageflag, totbind;
	short xrep, yrep;
	short twsta, twend;
	unsigned int bindcode;	/* only for current image... */
	unsigned int *repbind;	/* for repeat of parts of images */
	
	struct PackedFile * packedfile;
	struct PreviewImage * preview;

	/* game engine tile animation */
	float lastupdate;
	int lastused;
	short animspeed;
	
	/* for generated images */
	short gen_x, gen_y;
	char gen_type, gen_flag;
	
	/* display aspect - for UV editing images resized for faster openGL display */
	float aspx, aspy;
} Image;


/* **************** IMAGE ********************* */

/* Image.flag */
#define IMA_FIELDS			1
#define IMA_STD_FIELD		2
#define IMA_DO_PREMUL		4
#define IMA_REFLECT			16
#define IMA_NOCOLLECT   	32
#define IMA_DEPRECATED		64
#define IMA_OLD_PREMUL		128
#define IMA_CM_PREDIVIDE	256
#define IMA_USED_FOR_RENDER	512

/* Image.tpageflag */
#define IMA_TILES			1
#define IMA_TWINANIM		2
#define IMA_COLCYCLE		4	/* Depreciated */
#define IMA_MIPMAP_COMPLETE 8   /* all mipmap levels in OpenGL texture set? */
#define IMA_CLAMP_U			16 
#define IMA_CLAMP_V			32
#define IMA_TPAGE_REFRESH	64

/* ima->type and ima->source moved to BKE_image.h, for API */

/* render */
#define IMA_MAX_RENDER_TEXT		512
#define IMA_MAX_RENDER_SLOT		8

/* gen_flag */
#define IMA_GEN_FLOAT		1

#endif

