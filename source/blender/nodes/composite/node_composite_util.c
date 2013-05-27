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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/node_composite_util.c
 *  \ingroup nodes
 */

#include "node_composite_util.h"


int cmp_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
	return STREQ(ntree->idname, "CompositorNodeTree");
}

void cmp_node_update_default(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *sock;
	for (sock = node->outputs.first; sock; sock = sock->next) {
		if (sock->cache) {
			//free_compbuf(sock->cache);
			//sock->cache= NULL;
		}
	}
	node->need_exec = 1;
}

void cmp_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
	node_type_base(ntype, type, name, nclass, flag);
	
	ntype->poll = cmp_node_poll_default;
	ntype->updatefunc = cmp_node_update_default;
	ntype->update_internal_links = node_update_internal_links_default;
}
