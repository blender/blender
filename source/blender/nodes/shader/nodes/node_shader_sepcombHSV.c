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

/** \file blender/nodes/shader/nodes/node_shader_sepcombHSV.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** SEPARATE HSV ******************** */
static bNodeSocketTemplate sh_node_sephsv_in[] = {
	{	SOCK_RGBA, 1, N_("Color"),			0.8f, 0.8f, 0.8f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_sephsv_out[] = {
	{	SOCK_FLOAT, 0, N_("H")},
	{	SOCK_FLOAT, 0, N_("S")},
	{	SOCK_FLOAT, 0, N_("V")},
	{	-1, 0, ""	}
};

static void node_shader_exec_sephsv(void *UNUSED(data), int UNUSED(thread), bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	float col[3];
	nodestack_get_vec(col, SOCK_VECTOR, in[0]);

	rgb_to_hsv(col[0], col[1], col[2],
	           &out[0]->vec[0], &out[1]->vec[0], &out[2]->vec[0]);
}

static int gpu_shader_sephsv(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, node, "separate_hsv", in, out);
}

void register_node_type_sh_sephsv(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_SEPHSV, "Separate HSV", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, sh_node_sephsv_in, sh_node_sephsv_out);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_sephsv);
	node_type_gpu(&ntype, gpu_shader_sephsv);

	nodeRegisterType(&ntype);
}


/* **************** COMBINE HSV ******************** */
static bNodeSocketTemplate sh_node_combhsv_in[] = {
	{	SOCK_FLOAT, 1, N_("H"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	SOCK_FLOAT, 1, N_("S"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	SOCK_FLOAT, 1, N_("V"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_combhsv_out[] = {
	{	SOCK_RGBA, 0, N_("Color")},
	{	-1, 0, ""	}
};

static void node_shader_exec_combhsv(void *UNUSED(data), int UNUSED(thread), bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	float h, s, v;
	nodestack_get_vec(&h, SOCK_FLOAT, in[0]);
	nodestack_get_vec(&s, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&v, SOCK_FLOAT, in[2]);

	hsv_to_rgb(h, s, v, &out[0]->vec[0], &out[0]->vec[1], &out[0]->vec[2]);
}

static int gpu_shader_combhsv(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, node, "combine_hsv", in, out);
}

void register_node_type_sh_combhsv(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_COMBHSV, "Combine HSV", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, sh_node_combhsv_in, sh_node_combhsv_out);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_combhsv);
	node_type_gpu(&ntype, gpu_shader_combhsv);

	nodeRegisterType(&ntype);
}
