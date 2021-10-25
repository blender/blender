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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_mixRgb.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** MIX RGB ******************** */
static bNodeSocketTemplate sh_node_mix_rgb_in[] = {
	{	SOCK_FLOAT, 1, N_("Fac"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Color1"),			0.5f, 0.5f, 0.5f, 1.0f},
	{	SOCK_RGBA, 1, N_("Color2"),			0.5f, 0.5f, 0.5f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_mix_rgb_out[] = {
	{	SOCK_RGBA, 0, N_("Color")},
	{	-1, 0, ""	}
};

static void node_shader_exec_mix_rgb(void *UNUSED(data), int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, col1, col2 */
	/* stack order out: col */
	float col[3];
	float fac;
	float vec[3];

	nodestack_get_vec(&fac, SOCK_FLOAT, in[0]);
	CLAMP(fac, 0.0f, 1.0f);
	
	nodestack_get_vec(col, SOCK_VECTOR, in[1]);
	nodestack_get_vec(vec, SOCK_VECTOR, in[2]);

	ramp_blend(node->custom1, col, fac, vec);
	if (node->custom2 & SHD_MIXRGB_CLAMP) {
		CLAMP3(col, 0.0f, 1.0f);
	}
	copy_v3_v3(out[0]->vec, col);
}

static int gpu_shader_mix_rgb(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	static const char *names[] = {"mix_blend", "mix_add", "mix_mult", "mix_sub",
		                          "mix_screen", "mix_div", "mix_diff", "mix_dark", "mix_light",
		                          "mix_overlay", "mix_dodge", "mix_burn", "mix_hue", "mix_sat",
		                          "mix_val", "mix_color", "mix_soft", "mix_linear"};
	int ret = GPU_stack_link(mat, names[node->custom1], in, out);
	if (ret && node->custom2 & SHD_MIXRGB_CLAMP) {
		float min[3] = {0.0f, 0.0f, 0.0f};
		float max[3] = {1.0f, 1.0f, 1.0f};
		GPU_link(mat, "clamp_vec3", out[0].link, GPU_uniform(min), GPU_uniform(max), &out[0].link);
	}
	return ret;
}


void register_node_type_sh_mix_rgb(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_mix_rgb_in, sh_node_mix_rgb_out);
	node_type_label(&ntype, node_blend_label);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_mix_rgb);
	node_type_gpu(&ntype, gpu_shader_mix_rgb);

	nodeRegisterType(&ntype);
}
