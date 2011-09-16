/**
 * $Id: SHD_blend_weight.c 32517 2010-10-16 14:32:17Z campbellbarton $
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** BlendWeight ******************** */
static bNodeSocketType sh_node_blend_weight_in[]= {
	{	SOCK_VALUE, 1, "Blend",		0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_blend_weight_out[]= {
	{	SOCK_VALUE, 0, "Fresnel",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Facing",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_blend_weight(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **UNUSED(in), bNodeStack **UNUSED(out))
{
}

static int node_shader_gpu_blend_weight(GPUMaterial *UNUSED(mat), bNode *UNUSED(node), GPUNodeStack *UNUSED(in), GPUNodeStack *UNUSED(out))
{
	return 0;
}

/* node type definition */
void register_node_type_sh_blend_weight(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_BLEND_WEIGHT, "Blend Weight", NODE_CLASS_INPUT, 0,
		sh_node_blend_weight_in, sh_node_blend_weight_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, node_shader_exec_blend_weight);
	node_type_gpu(&ntype, node_shader_gpu_blend_weight);

	nodeRegisterType(lb, &ntype);
};

