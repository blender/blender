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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_vectTransform.c
 *  \ingroup shdnodes
 */
 
#include "../node_shader_util.h"

/* **************** Vector Transform ******************** */ 
static bNodeSocketTemplate sh_node_vect_transform_in[] = {
	{ SOCK_VECTOR, 1, N_("Vector"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
	{ -1, 0, "" }
};

static bNodeSocketTemplate sh_node_vect_transform_out[] = {
	{ SOCK_VECTOR, 0, N_("Vector")},
	{ -1, 0, "" }
};

static void node_shader_init_vect_transform(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeShaderVectTransform *vect = MEM_callocN(sizeof(NodeShaderVectTransform), "NodeShaderVectTransform");
	
	/* Convert World into Object Space per default */
	vect->convert_to = 1;
	
	node->storage = vect;
}

void register_node_type_sh_vect_transform(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_VECT_TRANSFORM, "Vector Transform", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_init(&ntype, node_shader_init_vect_transform);
	node_type_socket_templates(&ntype, sh_node_vect_transform_in, sh_node_vect_transform_out);
	node_type_storage(&ntype, "NodeShaderVectTransform", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
