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
 * Contributor(s): Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_hueSatVal.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"


/* **************** Hue Saturation ******************** */
static bNodeSocketTemplate sh_node_hue_sat_in[] = {
	{	SOCK_FLOAT, 1, N_("Hue"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Saturation"),		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Value"),			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Fac"),			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Color"),			0.8f, 0.8f, 0.8f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_hue_sat_out[] = {
	{	SOCK_RGBA, 0, N_("Color")},
	{	-1, 0, ""	}
};

/* note: it would be possible to use CMP version for both nodes */
static void do_hue_sat_fac(bNode *UNUSED(node), float *out, float hue, float sat, float val, float in[4], float fac)
{
	if (fac != 0.0f && (hue != 0.5f || sat != 1.0f || val != 1.0f)) {
		float col[3], hsv[3], mfac = 1.0f - fac;
		
		rgb_to_hsv(in[0], in[1], in[2], hsv, hsv + 1, hsv + 2);
		hsv[0] += (hue - 0.5f);
		if (hsv[0] > 1.0f) hsv[0] -= 1.0f; else if (hsv[0] < 0.0f) hsv[0] += 1.0f;
		hsv[1] *= sat;
		hsv[2] *= val;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], col, col + 1, col + 2);

		out[0] = mfac * in[0] + fac * col[0];
		out[1] = mfac * in[1] + fac * col[1];
		out[2] = mfac * in[2] + fac * col[2];
	}
	else {
		copy_v4_v4(out, in);
	}
}

static void node_shader_exec_hue_sat(void *UNUSED(data), int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	float hue, sat, val, fac;
	float col[4];
	nodestack_get_vec(&hue, SOCK_FLOAT, in[0]);
	nodestack_get_vec(&sat, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&val, SOCK_FLOAT, in[2]);
	nodestack_get_vec(&fac, SOCK_FLOAT, in[3]);
	nodestack_get_vec(col, SOCK_RGBA, in[4]);
	do_hue_sat_fac(node, out[0]->vec, hue, sat, val, col, fac);
}


static int gpu_shader_hue_sat(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "hue_sat", in, out);
}

void register_node_type_sh_hue_sat(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_HUE_SAT, "Hue Saturation Value", NODE_CLASS_OP_COLOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_hue_sat_in, sh_node_hue_sat_out);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_hue_sat);
	node_type_gpu(&ntype, gpu_shader_hue_sat);

	nodeRegisterType(&ntype);
}
