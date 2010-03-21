/**
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
	
	short type, flag;			/* type is copy from socket type struct */
	short limit, stack_index;	/* limit for dependency sort, stack_index for exec */
	short intern;				/* intern = tag for group nodes */
	short stack_index_ext;		/* for groups, to find the caller stack index */
	int pad1;
	
	float locx, locy;
	
	/* internal data to retrieve relations and groups */
	
	int own_index, to_index;	/* group socket identifiers, to find matching pairs after reading files */
	
	struct bNodeSocket *tosock;	/* group-node sockets point to the internal group counterpart sockets, set after read file  */
	struct bNodeLink *link;		/* a link pointer, set in nodeSolveOrder() */
	
} bNodeSocket;

/* sock->type */
#define SOCK_VALUE		0
#define SOCK_VECTOR		1
#define SOCK_RGBA		2

/* sock->flag, first bit is select */
		/* hidden is user defined, to hide unused */
#define SOCK_HIDDEN				2
		/* only used now for groups... */
#define SOCK_IN_USE				4
		/* unavailable is for dynamic sockets */
#define SOCK_UNAVAIL			8
#
#
typedef struct bNodePreview {
	unsigned char *rect;
	short xsize, ysize;
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
#define NODE_CUSTOM_NAME		1024

typedef struct bNodeLink {
	struct bNodeLink *next, *prev;
	
	bNode *fromnode, *tonode;
	bNodeSocket *fromsock, *tosock;
	
	int flag, pad;
	
} bNodeLink;

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
	struct bNodeType *owntype;		/* for groups or dynamic trees, no read/write */

	int pad2[2];
	
	/* callbacks */
	void (*timecursor)(void *, int nr);
	void (*stats_draw)(void *, char *str);
	int (*test_break)(void *);
	void *tbh, *tch, *sdh;
	
} bNodeTree;

/* ntree->type, index */
#define NTREE_SHADER	0
#define NTREE_COMPOSIT	1
#define NTREE_TEXTURE   2

/* ntree->init, flag */
#define NTREE_TYPE_INIT	1
#define NTREE_EXEC_INIT	2

/* ntree->flag */
#define NTREE_DS_EXPAND	1	/* for animation editors */

/* data structs, for node->storage */

/* this one has been replaced with ImageUser, keep it for do_versions() */
typedef struct NodeImageAnim {
	int frames, sfra, nr;
	char cyclic, movie;
	short pad;
} NodeImageAnim;

typedef struct NodeBlurData {
	short sizex, sizey;
	short samples, maxspeed, minspeed, relative;
	float fac, percentx, percenty;
	short filtertype;
	char bokeh, gamma;
	short curved;
	short pad;
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
} NodeColorBalance;

typedef struct NodeColorspill {
	short limchan, unspill;
	float limscale;
	float uspillr, uspillg, uspillb;
}NodeColorspill;

/* TEX_output */
typedef struct TexNodeOutput {
	char name[32];
} TexNodeOutput;


/* comp channel matte */
#define CMP_NODE_CHANNEL_MATTE_CS_RGB	1
#define CMP_NODE_CHANNEL_MATTE_CS_HSV	2
#define CMP_NODE_CHANNEL_MATTE_CS_YUV	3
#define CMP_NODE_CHANNEL_MATTE_CS_YCC	4

#endif
