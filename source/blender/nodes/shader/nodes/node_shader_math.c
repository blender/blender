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
	
		case 0: /* Add */
			r = a + b;
			break;
		case 1: /* Subtract */
			r = a - b;
			break;
		case 2: /* Multiply */
			r = a * b;
			break;
		case 3: /* Divide */
		{
			if (b == 0) /* We don't want to divide by zero. */
				r = 0.0;
			else
				r = a / b;
			break;
		}
		case 4: /* Sine */
		{
			if (in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				r = sinf(a);
			else
				r = sinf(b);
			break;
		}
		case 5: /* Cosine */
		{
			if (in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				r = cosf(a);
			else
				r = cosf(b);
			break;
		}
		case 6: /* Tangent */
		{
			if (in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				r = tanf(a);
			else
				r = tanf(b);
			break;
		}
		case 7: /* Arc-Sine */
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
		case 8: /* Arc-Cosine */
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
		case 9: /* Arc-Tangent */
		{
			if (in[0]->hasinput || !in[1]->hasinput) /* This one only takes one input, so we've got to choose. */
				r = atan(a);
			else
				r = atan(b);
			break;
		}
		case 10: /* Power */
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
		case 11: /* Logarithm */
		{
			/* Don't want any imaginary numbers... */
			if (a > 0  && b > 0)
				r = log(a) / log(b);
			else
				r = 0.0;
			break;
		}
		case 12: /* Minimum */
		{
			if (a < b)
				r = a;
			else
				r = b;
			break;
		}
		case 13: /* Maximum */
		{
			if (a > b)
				r = a;
			else
				r = b;
			break;
		}
		case 14: /* Round */
		{
			if (in[0]->hasinput || !in[1]->hasinput) /* This one only takes one input, so we've got to choose. */
				r = (a < 0) ? (int)(a - 0.5f) : (int)(a + 0.5f);
			else
				r = (b < 0) ? (int)(b - 0.5f) : (int)(b + 0.5f);
			break;
		}
		case 15: /* Less Than */
		{
			if (a < b)
				r = 1.0f;
			else
				r = 0.0f;
			break;
		}
		case 16: /* Greater Than */
		{
			if (a > b)
				r = 1.0f;
			else
				r = 0.0f;
			break;
		}
		case 17: /* Modulo */
		{
			if (b == 0.0f)
				r = 0.0f;
			else
				r = fmod(a, b);
			break;
		}
		case 18: /* Absolute */
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
		case 0:
		case 1:
		case 2:
		case 3:
		case 10:
		case 11:
		case 12:
		case 13:
		case 15:
		case 16:
		case 17:
			GPU_stack_link(mat, names[node->custom1], in, out);
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 14:
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
