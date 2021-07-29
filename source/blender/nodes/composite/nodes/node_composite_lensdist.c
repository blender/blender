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
 * Contributor(s): Alfredo de Greef  (eeshlo)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_lensdist.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_lensdist_in[] = {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Distort"), 	0.f, 0.f, 0.f, 0.f, -0.999f, 1.f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Dispersion"), 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_lensdist_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void node_composit_init_lensdist(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeLensDist *nld = MEM_callocN(sizeof(NodeLensDist), "node lensdist data");
	nld->jit = nld->proj = nld->fit = 0;
	node->storage = nld;
}


void register_node_type_cmp_lensdist(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_LENSDIST, "Lens Distortion", NODE_CLASS_DISTORT, 0);
	node_type_socket_templates(&ntype, cmp_node_lensdist_in, cmp_node_lensdist_out);
	node_type_init(&ntype, node_composit_init_lensdist);
	node_type_storage(&ntype, "NodeLensDist", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
