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

/** \file blender/nodes/shader/nodes/node_shader_math.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"


/* **************** SCALAR MATH ******************** */ 
static bNodeSocketTemplate sh_node_math_in[] = {
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
	{ -1, 0, "" }
};

static bNodeSocketTemplate sh_node_math_out[] = {
	{ SOCK_FLOAT, 0, N_("Value")},
	{ -1, 0, "" }
};

static void node_shader_exec_math(void *UNUSED(data), int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out) 
{
	float a, b, r = 0.0f;
	
	nodestack_get_vec(&a, SOCK_FLOAT, in[0]);
	nodestack_get_vec(&b, SOCK_FLOAT, in[1]);
	
	switch (node->custom1) {
	
		case NODE_MATH_ADD:
			r = a + b;
			break;
		case NODE_MATH_SUB:
			r = a - b;
			break;
		case NODE_MATH_MUL:
			r = a * b;
			break;
		case NODE_MATH_DIVIDE:
		{
			if (b == 0) /* We don't want to divide by zero. */
				r = 0.0;
			else
				r = a / b;
			break;
		}
		case NODE_MATH_SIN:
		{
			if (in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				r = sinf(a);
			else
				r = sinf(b);
			break;
		}
		case NODE_MATH_COS:
		{
			if (in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				r = cosf(a);
			else
				r = cosf(b);
			break;
		}
		case NODE_MATH_TAN:
		{
			if (in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				r = tanf(a);
			else
				r = tanf(b);
			break;
		}
		case NODE_MATH_ASIN:
		{
			if (in[0]->hasinput || !in[1]->hasinput) { /* This one only takes one input, so we've got to choose. */
				/* Can't do the impossible... */
				if (a <= 1 && a >= -1)
					r = asinf(a);
				else
					r = 0.0;
			}
			else {
				/* Can't do the impossible... */
				if (b <= 1 && b >= -1)
					r = asinf(b);
				else
					r = 0.0;
			}
			break;
		}
		case NODE_MATH_ACOS:
		{
			if (in[0]->hasinput || !in[1]->hasinput) { /* This one only takes one input, so we've got to choose. */
				/* Can't do the impossible... */
				if (a <= 1 && a >= -1)
					r = acosf(a);
				else
					r = 0.0;
			}
			else {
				/* Can't do the impossible... */
				if (b <= 1 && b >= -1)
					r = acosf(b);
				else
					r = 0.0;
			}
			break;
		}
		case NODE_MATH_ATAN:
		{
			if (in[0]->hasinput || !in[1]->hasinput) /* This one only takes one input, so we've got to choose. */
				r = atan(a);
			else
				r = atan(b);
			break;
		}
		case NODE_MATH_POW:
		{
			/* Only raise negative numbers by full integers */
			if (a >= 0) {
				r = pow(a, b);
			}
			else {
				float y_mod_1 = fabsf(fmodf(b, 1.0f));
				
				/* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
				if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
					r = powf(a, floorf(b + 0.5f));
				}
				else {
					r = 0.0f;
				}
			}

			break;
		}
		case NODE_MATH_LOG:
		{
			/* Don't want any imaginary numbers... */
			if (a > 0  && b > 0)
				r = log(a) / log(b);
			else
				r = 0.0;
			break;
		}
		case NODE_MATH_MIN:
		{
			if (a < b)
				r = a;
			else
				r = b;
			break;
		}
		case NODE_MATH_MAX:
		{
			if (a > b)
				r = a;
			else
				r = b;
			break;
		}
		case NODE_MATH_ROUND:
		{
			if (in[0]->hasinput || !in[1]->hasinput) /* This one only takes one input, so we've got to choose. */
				r = (a < 0) ? (int)(a - 0.5f) : (int)(a + 0.5f);
			else
				r = (b < 0) ? (int)(b - 0.5f) : (int)(b + 0.5f);
			break;
		}
		case NODE_MATH_LESS:
		{
			if (a < b)
				r = 1.0f;
			else
				r = 0.0f;
			break;
		}
		case NODE_MATH_GREATER:
		{
			if (a > b)
				r = 1.0f;
			else
				r = 0.0f;
			break;
		}
		case NODE_MATH_MOD:
		{
			if (b == 0.0f)
				r = 0.0f;
			else
				r = fmod(a, b);
			break;
		}
		case NODE_MATH_ABS:
		{
			r = fabsf(a);
			break;
		}
	}
	if (node->custom2 & SHD_MATH_CLAMP) {
		CLAMP(r, 0.0f, 1.0f);
	}
	out[0]->vec[0] = r;
}

static int gpu_shader_math(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	static const char *names[] = {"math_add", "math_subtract", "math_multiply",
		                          "math_divide", "math_sine", "math_cosine", "math_tangent", "math_asin",
		                          "math_acos", "math_atan", "math_pow", "math_log", "math_min", "math_max",
                                  "math_round", "math_less_than", "math_greater_than", "math_modulo", "math_absolute"};

	switch (node->custom1) {
		case NODE_MATH_ADD:
		case NODE_MATH_SUB:
		case NODE_MATH_MUL:
		case NODE_MATH_DIVIDE:
		case NODE_MATH_POW:
		case NODE_MATH_LOG:
		case NODE_MATH_MIN:
		case NODE_MATH_MAX:
		case NODE_MATH_LESS:
		case NODE_MATH_GREATER:
		case NODE_MATH_MOD:
			GPU_stack_link(mat, names[node->custom1], in, out);
			break;
		case NODE_MATH_SIN:
		case NODE_MATH_COS:
		case NODE_MATH_TAN:
		case NODE_MATH_ASIN:
		case NODE_MATH_ACOS:
		case NODE_MATH_ATAN:
		case NODE_MATH_ROUND:
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
			return 0;
	}

	if (node->custom2 & SHD_MATH_CLAMP) {
		float min[3] = {0.0f, 0.0f, 0.0f};
		float max[3] = {1.0f, 1.0f, 1.0f};
		GPU_link(mat, "clamp_val", out[0].link, GPU_uniform(min), GPU_uniform(max), &out[0].link);
	}

	return 1;
}

void register_node_type_sh_math(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_math_in, sh_node_math_out);
	node_type_label(&ntype, node_math_label);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_math);
	node_type_gpu(&ntype, gpu_shader_math);

	nodeRegisterType(&ntype);
}
