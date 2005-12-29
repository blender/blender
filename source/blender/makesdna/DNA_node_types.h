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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_NODE_TYPES_H
#define DNA_NODE_TYPES_H

#include "DNA_vec_types.h"
#include "DNA_listBase.h"

struct ID;
struct SpaceNode;
struct bNodeLink;
struct bNodeType;
struct uiBlock;

#define NODE_MAXSTR 32


typedef struct bNodeStack {
	float vec[4];
	float min, max;			/* min/max for values (UI writes it, execute might use it) */
	void *data;
	short hasinput, pad;	/* hasinput tagged for executing */
	int pad1;
} bNodeStack;

typedef struct bNodeSocket {
	struct bNodeSocket *next, *prev;
	
	char name[32];
	bNodeStack ns;				/* custom data for inputs, only UI writes in this */
	short type, flag, limit, stack_index;
	float locx, locy;
	
	struct bNodeLink *link;		/* input link to parent, max one! set in nodeSolveOrder() */
	
} bNodeSocket;

/* sock->type */
#define SOCK_VALUE		0
#define SOCK_VECTOR		1
#define SOCK_RGBA		2
#define SOCK_IMAGE		3

/* sock->flag, first bit is select */
#
#
typedef struct bNodePreview {
	float *rect;
	short xsize, ysize;
} bNodePreview;


/* limit data in bNode to what we want to see saved? */
typedef struct bNode {
	struct bNode *next, *prev, *new;
	
	char name[32];
	short type, flag;
	short done, level;		/* both for dependency and sorting */
	short lasty, menunr;	/* lasty: check preview render status, menunr: browse ID blocks */
	short pad1, pad2;
	
	ListBase inputs, outputs;
	struct ID *id;			/* optional link to libdata */
	void *storage;			/* custom data, must be struct, for storage in file */
	struct uiBlock *block;	/* each node has own block */
	
	float locx, locy;		/* root offset for drawing */
	float width, miniwidth;			
	short custom1, custom2;	/* to be abused for buttons */
	int pad3;
	
	rctf totr;				/* entire boundbox */
	rctf butr;				/* optional buttons area */
	rctf prvr;				/* optional preview area */
	bNodePreview *preview;	/* optional preview image */
	
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

typedef struct bNodeLink {
	struct bNodeLink *next, *prev;
	
	bNode *fromnode, *tonode;
	bNodeSocket *fromsock, *tosock;
	
} bNodeLink;

/* the basis for a Node tree, all links and nodes reside internal here */
typedef struct bNodeTree {
	ListBase nodes, links;
	
	void *data;					/* custom data, set by execute caller, no read/write handling */
	bNodeStack *stack;			/* stack is only while executing */
	
	int type, init;				/* set init on fileread */
	struct bNodeType **alltypes;		/* type definitions, set on fileread */
	
} bNodeTree;

/* ntree->type, index */
#define NTREE_SHADER	0
#define NTREE_COMPOSIT	1

/* ntree->init, flag */
#define NTREE_TYPE_INIT	1
#define NTREE_EXEC_INIT	2

#endif

