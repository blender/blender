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

#include "DNA_ID.h"
#include "DNA_vec_types.h"
#include "DNA_listBase.h"


struct SpaceNode;
struct bNodeLink;
struct bNodeType;
struct bNodeGroup;
struct uiBlock;

#define NODE_MAXSTR 32


typedef struct bNodeStack {
	float vec[4];
	float min, max;			/* min/max for values (UI writes it, execute might use it) */
	void *data;
	short hasinput;			/* when input has link, tagged before executing */
	short hasoutput;		/* when output is linked, tagged before executing */
	short datatype;			/* type of data pointer */
	short pad;
} bNodeStack;

/* ns->datatype, shadetree only */
#define NS_OSA_VECTORS		1
#define NS_OSA_VALUES		2

typedef struct bNodeSocket {
	struct bNodeSocket *next, *prev;
	
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
#define SOCK_IMAGE		3

/* sock->flag, first bit is select */
#define SOCK_HIDDEN				2
		/* only used now for groups... */
#define SOCK_IN_USE				4

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
	short stack_index;		/* for groupnode, offset in global caller stack */
	short nr;				/* number of this node in list, used for UI exec events */
	
	ListBase inputs, outputs;
	struct ID *id;			/* optional link to libdata */
	void *storage;			/* custom data, must be struct, for storage in file */
	struct uiBlock *block;	/* each node has own block */
	
	float locx, locy;		/* root offset for drawing */
	float width, miniwidth;			
	short custom1, custom2;	/* to be abused for buttons */
	
	short need_exec, exec;	/* need_exec is set as UI execution event, exec is flag during exec */
	
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
#define NODE_GROUP_EDIT		128

typedef struct bNodeLink {
	struct bNodeLink *next, *prev;
	
	bNode *fromnode, *tonode;
	bNodeSocket *fromsock, *tosock;
	
} bNodeLink;

/* the basis for a Node tree, all links and nodes reside internal here */
/* only re-usable node trees are in the library though, materials allocate own tree struct */
typedef struct bNodeTree {
	ID id;
	
	ListBase nodes, links;
	
	bNodeStack *stack;				/* stack is only while executing, no read/write in file */
	bNodeStack *stack1;				/* for other thread, easy to expand though... */
	
	int type, init;					/* set init on fileread */
	int stacksize;					/* amount of elements in stack */
	int cur_index;					/* sockets in groups have unique identifiers, adding new sockets always will increase this counter */
	struct bNodeType **alltypes;	/* type definitions, set on fileread, no read/write */
	struct bNodeType *owntype;		/* for groups or dynamic trees, no read/write */
	
	/* callbacks */
	void (*timecursor)(int nr);
	
} bNodeTree;

/* ntree->type, index */
#define NTREE_SHADER	0
#define NTREE_COMPOSIT	1

/* ntree->init, flag */
#define NTREE_TYPE_INIT	1
#define NTREE_EXEC_INIT	2

/* data structs, for node->storage */

typedef struct NodeImageAnim {
	short frames, sfra, nr;
	char cyclic, movie;
} NodeImageAnim;

typedef struct NodeBlurData {
	short sizex, sizey;
	short filtertype;
	char bokeh, gamma;
} NodeBlurData;

#endif

