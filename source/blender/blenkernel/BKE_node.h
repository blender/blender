/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_NODE_H
#define BKE_NODE_H

struct bNodeTree;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct ListBase;

#define SOCK_IN		1
#define SOCK_OUT	2


/* ************** GENERIC API *************** */

void			nodeFreeNode(struct bNodeTree *ntree, struct bNode *node);
void			nodeFreeTree(struct bNodeTree *ntree);

struct bNode	*nodeAddNode(struct bNodeTree *ntree, char *name);
struct bNodeLink *nodeAddLink(struct bNodeTree *ntree, struct bNode *fromnode, struct bNodeSocket *fromsock, struct bNode *tonode, struct bNodeSocket *tosock);
struct bNode	*nodeCopyNode(struct bNodeTree *ntree, struct bNode *node);

struct bNodeSocket *nodeAddSocket(struct bNode *node, int type, int where, int limit, char *name);

struct bNodeLink *nodeFindLink(struct bNodeTree *ntree, struct bNodeSocket *from, struct bNodeSocket *to);
int				nodeCountSocketLinks(struct bNodeTree *ntree, struct bNodeSocket *sock);

void			nodeSolveOrder(struct bNodeTree *ntree);
void			nodeExecTree(struct bNodeTree *ntree);

/* ************** SHADER NODES *************** */

/* types are needed to restore callbacks */
#define SH_NODE_TEST		0
#define SH_NODE_RGB			1
#define SH_NODE_VALUE		2
#define SH_NODE_MIX_RGB		3
#define SH_NODE_SHOW_RGB	4

struct bNode	*node_shader_add(struct bNodeTree *ntree, int type);
void			node_shader_set_execfunc(struct bNode *node);

#endif

