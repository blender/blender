/*
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Bob Holcomb, Xavier Thomas
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_NODE_TYPES_H
#define DNA_NODE_TYPES_H

/** \file DNA_node_types.h
 *  \ingroup DNA
 */

#include "DNA_ID.h"
#include "DNA_vec_types.h"
#include "DNA_listBase.h"

struct ListBase;
struct SpaceNode;
struct bNodeLink;
struct bNodeType;
struct bNodeGroup;
struct AnimData;
struct bGPdata;
struct uiBlock;
struct Image;

#define NODE_MAXSTR 32


typedef struct bNodeStack {
	float vec[4];
	float min, max;			/* min/max for values (UI writes it, execute might use it) */
	void *data;
	short hasinput;			/* when input has link, tagged before executing */
	short hasoutput;		/* when output is linked, tagged before executing */
	short datatype;			/* type of data pointer */
	short sockettype;		/* type of socket stack comes from, to remap linking different sockets */
} bNodeStack;

/* ns->datatype, shadetree only */
#define NS_OSA_VECTORS		1
#define NS_OSA_VALUES		2

typedef struct bNodeSocket {
	struct bNodeSocket *next, *prev, *new_sock;
	
	char name[32];
	bNodeStack ns;				/* custom data for inputs, only UI writes in this */
	
	short type, flag;
	short limit;				/* max. number of links */
	
	/* stack data info (only during execution!) */
	short stack_type;			/* type of stack reference */
	/* XXX only one of stack_ptr or stack_index is used (depending on stack_type).
	 * could store the index in the pointer with SET_INT_IN_POINTER (a bit ugly).
	 * (union won't work here, not supported by DNA)
	 */
	struct bNodeStack *stack_ptr;	/* constant input value */
	short stack_index;			/* local stack index or external input number */
	short pad1;
	
	float locx, locy;
	
	/* internal data to retrieve relations and groups */
	
	int own_index;				/* group socket identifiers, to find matching pairs after reading files */
	struct bNodeSocket *groupsock;
	int to_index;				/* XXX deprecated, only used for restoring old group node links */
	int pad2;
	
	struct bNodeLink *link;		/* a link pointer, set in nodeSolveOrder() */
} bNodeSocket;

/* sock->type */
#define SOCK_VALUE		0
#define SOCK_VECTOR		1
#define SOCK_RGBA		2
#define	SOCK_SHADER		3

/* sock->flag, first bit is select */
		/* hidden is user defined, to hide unused */
#define SOCK_HIDDEN				2
		/* only used now for groups... */
#define SOCK_IN_USE				4
		/* unavailable is for dynamic sockets */
#define SOCK_UNAVAIL			8
		/* socket collapsed in UI */
#define SOCK_COLLAPSED			16

/* sock->stack_type */
#define SOCK_STACK_LOCAL		1	/* part of the local tree stack */
#define SOCK_STACK_EXTERN		2	/* use input stack pointer */
#define SOCK_STACK_CONST		3	/* use pointer to constant input value */

typedef struct bNodePreview {
	unsigned char *rect;
	short xsize, ysize;
	int pad;
} bNodePreview;


/* limit data in bNode to what we want to see saved? */
typedef struct bNode {
	struct bNode *next, *prev, *new_node;
	
	char name[32];
	short type, flag;
	short done, level;		/* both for dependency and sorting */
	short lasty, menunr;	/* lasty: check preview render status, menunr: browse ID blocks */
	short stack_index;		/* for groupnode, offset in global caller stack */
	short nr;				/* number of this node in list, used for UI exec events */
	
	ListBase inputs, outputs;
	struct ID *id;			/* optional link to libdata */
	void *storage;			/* custom data, must be struct, for storage in file */
	
	float locx, locy;		/* root offset for drawing */
	float width, miniwidth;
	char label[32];			/* custom user-defined label */
	short custom1, custom2;	/* to be abused for buttons */
	float custom3, custom4;
	
	short need_exec, exec;	/* need_exec is set as UI execution event, exec is flag during exec */
	void *threaddata;		/* optional extra storage for use in thread (read only then!) */
	
	rctf totr;				/* entire boundbox */
	rctf butr;				/* optional buttons area */
	rctf prvr;				/* optional preview area */
	bNodePreview *preview;	/* optional preview image */
	struct uiBlock *block;	/* runtime during drawing */
	
	struct bNodeType *typeinfo;	/* lookup of callbacks and defaults */
	
} bNode;

/* node->flag */
#define NODE_SELECT			1
#define NODE_OPTIONS		2
#define NODE_PREVIEW		4
#define NODE_HIDDEN			8
#define NODE_ACTIVE			16
#define NODE_ACTIVE_ID		32
#define NODE_DO_OUTPUT		64
#define NODE_GROUP_EDIT		128
		/* free test flag, undefined */
#define NODE_TEST			256
		/* composite: don't do node but pass on buffer(s) */
#define NODE_MUTED			512
#define NODE_CUSTOM_NAME	1024	/* deprecated! */
#define NODE_ACTIVE_TEXTURE	2048

typedef struct bNodeLink {
	struct bNodeLink *next, *prev;
	
	bNode *fromnode, *tonode;
	bNodeSocket *fromsock, *tosock;
	
	int flag, pad;
	
} bNodeLink;


/* link->flag */
#define NODE_LINKFLAG_HILITE	1

/* the basis for a Node tree, all links and nodes reside internal here */
/* only re-usable node trees are in the library though, materials and textures allocate own tree struct */
typedef struct bNodeTree {
	ID id;
	struct AnimData *adt;		/* animation data (must be immediately after id for utilities to use it) */ 
	
	struct bGPdata *gpd;		/* grease pencil data */
	
	ListBase nodes, links;
	
	bNodeStack *stack;				/* stack is only while executing, no read/write in file */
	struct ListBase *threadstack;	/* same as above */
	
	int type, init;					/* set init on fileread */
	int stacksize;					/* amount of elements in stack */
	int cur_index;					/* sockets in groups have unique identifiers, adding new sockets always 
									   will increase this counter */
	int flag, pad;					
	
	ListBase alltypes;				/* type definitions */
	ListBase inputs, outputs;		/* external sockets for group nodes */

	int pad2[2];
	
	/* callbacks */
	void (*progress)(void *, float progress);
	void (*stats_draw)(void *, char *str);
	int (*test_break)(void *);
	void *tbh, *prh, *sdh;
	
} bNodeTree;

/* ntree->type, index */
#define NTREE_SHADER	0
#define NTREE_COMPOSIT	1
#define NTREE_TEXTURE   2

/* ntree->init, flag */
#define NTREE_TYPE_INIT	1
#define NTREE_EXEC_INIT	2

/* ntree->flag */
#define NTREE_DS_EXPAND		1	/* for animation editors */
/* XXX not nice, but needed as a temporary flag
 * for group updates after library linking.
 */
#define NTREE_DO_VERSIONS	1024

/* data structs, for node->storage */

/* this one has been replaced with ImageUser, keep it for do_versions() */
typedef struct NodeImageAnim {
	int frames, sfra, nr;
	char cyclic, movie;
	short pad;
} NodeImageAnim;

typedef struct NodeBlurData {
	short sizex, sizey;
	short samples, maxspeed, minspeed, relative, aspect;
	short curved;
	float fac, percentx, percenty;
	short filtertype;
	char bokeh, gamma;
	int image_in_width, image_in_height; /* needed for absolute/relative conversions */
} NodeBlurData;

typedef struct NodeDBlurData {
	float center_x, center_y, distance, angle, spin, zoom;
	short iter;
	char wrap, pad;
} NodeDBlurData;

typedef struct NodeBilateralBlurData {
	float sigma_color, sigma_space;
	short iter, pad;
} NodeBilateralBlurData;

typedef struct NodeHueSat {
	float hue, sat, val;
} NodeHueSat;

typedef struct NodeImageFile {
	char name[256];
	short imtype, subimtype, quality, codec;
	int sfra, efra;
} NodeImageFile;

typedef struct NodeChroma {
	float t1,t2,t3;
	float fsize,fstrength,falpha;
	float key[4];
   short algorithm, channel;
} NodeChroma;

typedef struct NodeTwoXYs {
	short x1, x2, y1, y2;
   float fac_x1, fac_x2, fac_y1, fac_y2;
} NodeTwoXYs;

typedef struct NodeTwoFloats {
	float x, y;
} NodeTwoFloats;

typedef struct NodeGeometry {
	char uvname[32];
	char colname[32];
} NodeGeometry;

typedef struct NodeVertexCol {
	char name[32];
} NodeVertexCol;

/* qdn: Defocus blur node */
typedef struct NodeDefocus {
	char bktype, rotation, preview, gamco;
	short samples, no_zbuf;
	float fstop, maxblur, bthresh, scale;
} NodeDefocus;

typedef struct NodeScriptDict {
	void *dict; /* for PyObject *dict */
	void *node; /* for BPy_Node *node */
} NodeScriptDict;

/* qdn: glare node */
typedef struct NodeGlare {
	char quality, type, iter;
	char angle, angle_ofs, size, pad[2];
	float colmod, mix, threshold, fade;
} NodeGlare;

/* qdn: tonemap node */
typedef struct NodeTonemap {
	float key, offset, gamma;
	float f, m, a, c;
	int type;
} NodeTonemap;

/* qdn: lens distortion node */
typedef struct NodeLensDist {
	short jit, proj, fit, pad;
} NodeLensDist;

typedef struct NodeColorBalance {
	/* for processing */
	float slope[3];
	float offset[3];
	float power[3];
	
	/* for ui representation */
	float lift[3];
	float gamma[3];
	float gain[3];

	/* temp storage for inverted lift */
	float lift_lgg[3];
	float gamma_inv[3];
} NodeColorBalance;

typedef struct NodeColorspill {
	short limchan, unspill;
	float limscale;
	float uspillr, uspillg, uspillb;
}NodeColorspill;

typedef struct NodeTexSky {
	float sun_direction[3];
	float turbidity;
} NodeTexSky;

typedef struct NodeTexImage {
	int color_space;
} NodeTexImage;

typedef struct NodeTexEnvironment {
	int color_space;
} NodeTexEnvironment;

typedef struct NodeTexBlend {
	int progression;
	int axis;
} NodeTexBlend;

typedef struct NodeTexClouds {
	int hard;
	int depth;
	int basis;
	int pad;
} NodeTexClouds;

typedef struct NodeTexVoronoi {
	int distance_metric;
	int coloring;
} NodeTexVoronoi;

typedef struct NodeTexMusgrave {
	int type;
	int basis;
} NodeTexMusgrave;

typedef struct NodeTexMarble {
	int type;
	int wave;
	int basis;
	int hard;
	int depth;
	int pad;
} NodeTexMarble;

typedef struct NodeTexMagic {
	int depth;
	int pad;
} NodeTexMagic;

typedef struct NodeTexStucci {
	int type;
	int basis;
	int hard;
	int pad;
} NodeTexStucci;

typedef struct NodeTexDistortedNoise {
	int basis;
	int distortion_basis;
} NodeTexDistortedNoise;

typedef struct NodeTexWood {
	int type;
	int wave;
	int basis;
	int hard;
} NodeTexWood;

/* TEX_output */
typedef struct TexNodeOutput {
	char name[32];
} TexNodeOutput;

/* comp channel matte */
#define CMP_NODE_CHANNEL_MATTE_CS_RGB	1
#define CMP_NODE_CHANNEL_MATTE_CS_HSV	2
#define CMP_NODE_CHANNEL_MATTE_CS_YUV	3
#define CMP_NODE_CHANNEL_MATTE_CS_YCC	4

/* glossy distributions */
#define SHD_GLOSSY_BECKMANN	0
#define SHD_GLOSSY_SHARP	1
#define SHD_GLOSSY_GGX		2

/* blend texture */
#define SHD_BLEND_LINEAR			0
#define SHD_BLEND_QUADRATIC			1
#define SHD_BLEND_EASING			2
#define SHD_BLEND_DIAGONAL			3
#define SHD_BLEND_RADIAL			4
#define SHD_BLEND_QUADRATIC_SPHERE	5
#define SHD_BLEND_SPHERICAL			6

#define SHD_BLEND_HORIZONTAL		0
#define SHD_BLEND_VERTICAL			1

/* noise basis for textures */
#define SHD_NOISE_PERLIN			0
#define SHD_NOISE_VORONOI_F1		1
#define SHD_NOISE_VORONOI_F2		2
#define SHD_NOISE_VORONOI_F3		3
#define SHD_NOISE_VORONOI_F4		4
#define SHD_NOISE_VORONOI_F2_F1		5
#define SHD_NOISE_VORONOI_CRACKLE	6
#define SHD_NOISE_CELL_NOISE		7

#define SHD_NOISE_SOFT	0
#define SHD_NOISE_HARD	1

/* voronoi texture */
#define SHD_VORONOI_DISTANCE_SQUARED	0
#define SHD_VORONOI_ACTUAL_DISTANCE		1
#define SHD_VORONOI_MANHATTAN			2
#define SHD_VORONOI_CHEBYCHEV			3
#define SHD_VORONOI_MINKOVSKY_H			4
#define SHD_VORONOI_MINKOVSKY_4			5
#define SHD_VORONOI_MINKOVSKY			6

#define SHD_VORONOI_INTENSITY					0
#define SHD_VORONOI_POSITION					1
#define SHD_VORONOI_POSITION_OUTLINE			2
#define SHD_VORONOI_POSITION_OUTLINE_INTENSITY	3

/* musgrave texture */
#define SHD_MUSGRAVE_MULTIFRACTAL			0
#define SHD_MUSGRAVE_FBM					1
#define SHD_MUSGRAVE_HYBRID_MULTIFRACTAL	2
#define SHD_MUSGRAVE_RIDGED_MULTIFRACTAL	3
#define SHD_MUSGRAVE_HETERO_TERRAIN			4

/* marble texture */
#define SHD_MARBLE_SOFT		0
#define SHD_MARBLE_SHARP	1
#define SHD_MARBLE_SHARPER	2

#define SHD_WAVE_SINE	0
#define SHD_WAVE_SAW	1
#define SHD_WAVE_TRI	2

/* stucci texture */
#define SHD_STUCCI_PLASTIC	0
#define SHD_STUCCI_WALL_IN	1
#define SHD_STUCCI_WALL_OUT	2

/* wood texture */
#define SHD_WOOD_BANDS		0
#define SHD_WOOD_RINGS		1
#define SHD_WOOD_BAND_NOISE	2
#define SHD_WOOD_RING_NOISE	3

/* image/environment texture */
#define SHD_COLORSPACE_LINEAR	0
#define SHD_COLORSPACE_SRGB		1

/* blur node */
#define CMP_NODE_BLUR_ASPECT_NONE		0
#define CMP_NODE_BLUR_ASPECT_Y			1
#define CMP_NODE_BLUR_ASPECT_X			2

#endif
