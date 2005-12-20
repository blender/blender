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

#define NODE_MAXSTR 32

typedef struct bNodeSocket {
	struct bNodeSocket *next, *prev;
	
	char name[32];
	short type, flag, limit, pad;
	float locx, locy;
	
	ListBase links;		/* now only used temporal for sorting */
	
} bNodeSocket;

/* sock->type */
#define SOCK_VALUE		0
#define SOCK_VECTOR		1
#define SOCK_RGBA		2
#define SOCK_IMAGE		3

/* sock->flag, first bit is select */


/* limit data in bNode to what we want to see saved? */
typedef struct bNode {
	struct bNode *next, *prev, *new;
	
	char name[32];
	short type, flag, done, level;
	
	ListBase inputs, outputs;
	struct ID *id;		/* optional link to libdata */
	void *data;			/* custom data */
	float vec[4];		/* builtin custom data */
	
	float locx, locy;	/* root offset for drawing */
	float width, prv_h;	
	rctf tot;			/* entire boundbox */
	rctf prv;			/* optional preview area */
	
	int (*drawfunc)(struct SpaceNode *, struct bNode *);
	int (*execfunc)(struct bNode *);
	
} bNode;

/* node->flag */
#define NODE_SELECT		1

typedef struct bNodeLink {
	struct bNodeLink *next, *prev;
	
	bNode *fromnode, *tonode;
	bNodeSocket *fromsock, *tosock;
	
} bNodeLink;

/* the basis for a Node tree, all links and nodes reside internal here */
typedef struct bNodeTree {
	ListBase nodes, links;
	
	ListBase inputs, outputs;	/* default inputs and outputs, for solving tree */
	
} bNodeTree;


#endif

