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

/** \file blender/nodes/shader/nodes/node_shader_lamp.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** LAMP INFO  ******************** */
static bNodeSocketTemplate sh_node_lamp_out[] = {
	{	SOCK_RGBA, 0, N_("Color")},
	{	SOCK_VECTOR, 0, N_("Light Vector")},
	{	SOCK_FLOAT, 0, N_("Distance")},
	{	SOCK_RGBA, 0, N_("Shadow")},
	{	SOCK_FLOAT, 0, N_("Visibility Factor")},
	{	-1, 0, ""	}
};


static void node_shader_exec_lamp(void *data, int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **UNUSED(in), bNodeStack **out)
{
	if (data) {
		Object *ob = (Object *)node->id;

		if (ob) {
			ShadeInput *shi = ((ShaderCallData *)data)->shi;

			shi->nodes = 1; /* temp hack to prevent trashadow recursion */
			out[4]->vec[0] = RE_lamp_get_data(shi, ob, out[0]->vec, out[1]->vec, out[2]->vec, out[3]->vec);
			shi->nodes = 0;
		}
	}
}

static int gpu_shader_lamp(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (node->id) {
		GPULamp *lamp = GPU_lamp_from_blender(GPU_material_scene(mat), (Object *)node->id, NULL);
		GPUNodeLink *col, *lv, *dist, *visifac, *shadow;

		visifac = GPU_lamp_get_data(mat, lamp, &col, &lv, &dist, &shadow);

		return GPU_stack_link(mat, "lamp", in, out, col, lv, dist, shadow, visifac);
	}

	return 0;
}

void register_node_type_sh_lamp(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_LAMP, "Lamp Data", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_lamp_out);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_lamp);
	node_type_gpu(&ntype, gpu_shader_lamp);

	nodeRegisterType(&ntype);
}
