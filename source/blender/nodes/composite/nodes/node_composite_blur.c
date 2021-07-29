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
 * Contributor(s): Campbell Barton, Alfredo de Greef, David Millan Escriva,
 * Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_blur.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** BLUR ******************** */
static bNodeSocketTemplate cmp_node_blur_in[] = {
	{   SOCK_RGBA, 1, N_("Image"),          1.0f, 1.0f, 1.0f, 1.0f},
	{   SOCK_FLOAT, 1, N_("Size"),          1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE},
	{   -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_blur_out[] = {
	{   SOCK_RGBA, 0, N_("Image")},
	{   -1, 0, ""   }
};

static void node_composit_init_blur(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeBlurData *data = MEM_callocN(sizeof(NodeBlurData), "node blur data");
	data->filtertype = R_FILTER_GAUSS;
	node->storage = data;
}

void register_node_type_cmp_blur(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_BLUR, "Blur", NODE_CLASS_OP_FILTER, NODE_PREVIEW);
	node_type_socket_templates(&ntype, cmp_node_blur_in, cmp_node_blur_out);
	node_type_init(&ntype, node_composit_init_blur);
	node_type_storage(&ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
