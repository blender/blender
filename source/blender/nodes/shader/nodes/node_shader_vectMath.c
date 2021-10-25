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

/** \file blender/nodes/shader/nodes/node_shader_vectMath.c
 *  \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** VECTOR MATH ******************** */ 
static bNodeSocketTemplate sh_node_vect_math_in[] = {
	{ SOCK_VECTOR, 1, N_("Vector"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
	{ SOCK_VECTOR, 1, N_("Vector"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
	{ -1, 0, "" }
};

static bNodeSocketTemplate sh_node_vect_math_out[] = {
	{ SOCK_VECTOR, 0, N_("Vector")},
	{ SOCK_FLOAT, 0, N_("Value")},
	{ -1, 0, "" }
};

static void node_shader_exec_vect_math(void *UNUSED(data), int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out) 
{ 
	float vec1[3], vec2[3];
	
	nodestack_get_vec(vec1, SOCK_VECTOR, in[0]);
	nodestack_get_vec(vec2, SOCK_VECTOR, in[1]);
	
	if (node->custom1 == 0) {	/* Add */
		out[0]->vec[0] = vec1[0] + vec2[0];
		out[0]->vec[1] = vec1[1] + vec2[1];
		out[0]->vec[2] = vec1[2] + vec2[2];
		
		out[1]->vec[0] = (fabsf(out[0]->vec[0]) + fabsf(out[0]->vec[1]) + fabsf(out[0]->vec[2])) / 3.0f;
	}
	else if (node->custom1 == 1) {	/* Subtract */
		out[0]->vec[0] = vec1[0] - vec2[0];
		out[0]->vec[1] = vec1[1] - vec2[1];
		out[0]->vec[2] = vec1[2] - vec2[2];
		
		out[1]->vec[0] = (fabsf(out[0]->vec[0]) + fabsf(out[0]->vec[1]) + fabsf(out[0]->vec[2])) / 3.0f;
	}
	else if (node->custom1 == 2) {	/* Average */
		out[0]->vec[0] = vec1[0] + vec2[0];
		out[0]->vec[1] = vec1[1] + vec2[1];
		out[0]->vec[2] = vec1[2] + vec2[2];
		
		out[1]->vec[0] = normalize_v3(out[0]->vec);
	}
	else if (node->custom1 == 3) {	/* Dot product */
		out[1]->vec[0] = (vec1[0] * vec2[0]) + (vec1[1] * vec2[1]) + (vec1[2] * vec2[2]);
	}
	else if (node->custom1 == 4) {	/* Cross product */
		out[0]->vec[0] = (vec1[1] * vec2[2]) - (vec1[2] * vec2[1]);
		out[0]->vec[1] = (vec1[2] * vec2[0]) - (vec1[0] * vec2[2]);
		out[0]->vec[2] = (vec1[0] * vec2[1]) - (vec1[1] * vec2[0]);
		
		out[1]->vec[0] = normalize_v3(out[0]->vec);
	}
	else if (node->custom1 == 5) {	/* Normalize */
		if (in[0]->hasinput || !in[1]->hasinput) {	/* This one only takes one input, so we've got to choose. */
			out[0]->vec[0] = vec1[0];
			out[0]->vec[1] = vec1[1];
			out[0]->vec[2] = vec1[2];
		}
		else {
			out[0]->vec[0] = vec2[0];
			out[0]->vec[1] = vec2[1];
			out[0]->vec[2] = vec2[2];
		}
		
		out[1]->vec[0] = normalize_v3(out[0]->vec);
	}
	
}

static int gpu_shader_vect_math(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	static const char *names[] = {"vec_math_add", "vec_math_sub",
		"vec_math_average", "vec_math_dot", "vec_math_cross",
		"vec_math_normalize"};

	switch (node->custom1) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			GPU_stack_link(mat, names[node->custom1], in, out);
			break;
		case 5:
			if (in[0].hasinput || !in[1].hasinput) {
				/* use only first item and terminator */
				GPUNodeStack tmp_in[2];
				memcpy(&tmp_in[0], &in[0], sizeof(GPUNodeStack));
				memcpy(&tmp_in[1], &in[2], sizeof(GPUNodeStack));
				GPU_stack_link(mat, names[node->custom1], tmp_in, out);
			}
			else {
				/* use only second item and terminator */
				GPUNodeStack tmp_in[2];
				memcpy(&tmp_in[0], &in[1], sizeof(GPUNodeStack));
				memcpy(&tmp_in[1], &in[2], sizeof(GPUNodeStack));
				GPU_stack_link(mat, names[node->custom1], tmp_in, out);
			}
			break;
		default:
			return false;
	}
	
	return true;
}

void register_node_type_sh_vect_math(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_VECT_MATH, "Vector Math", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_vect_math_in, sh_node_vect_math_out);
	node_type_label(&ntype, node_vect_math_label);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_vect_math);
	node_type_gpu(&ntype, gpu_shader_vect_math);

	nodeRegisterType(&ntype);
}
