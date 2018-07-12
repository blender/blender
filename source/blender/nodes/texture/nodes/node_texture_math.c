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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_math.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"


/* **************** SCALAR MATH ******************** */
static bNodeSocketTemplate inputs[] = {
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
	{ -1, 0, "" }
};

static bNodeSocketTemplate outputs[] = {
	{ SOCK_FLOAT, 0, N_("Value")},
	{ -1, 0, "" }
};

static void valuefn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float in0 = tex_input_value(in[0], p, thread);
	float in1 = tex_input_value(in[1], p, thread);

	switch (node->custom1) {

		case NODE_MATH_ADD:
			*out = in0 + in1;
			break;
		case NODE_MATH_SUB:
			*out = in0 - in1;
			break;
		case NODE_MATH_MUL:
			*out = in0 * in1;
			break;
		case NODE_MATH_DIVIDE:
		{
			if (in1 == 0) /* We don't want to divide by zero. */
				*out = 0.0;
			else
				*out = in0 / in1;
			break;
		}
		case NODE_MATH_SIN:
		{
			*out = sinf(in0);
			break;
		}
		case NODE_MATH_COS:
		{
			*out = cosf(in0);
			break;
		}
		case NODE_MATH_TAN:
		{
			*out = tanf(in0);
			break;
		}
		case NODE_MATH_ASIN:
		{
			/* Can't do the impossible... */
			if (in0 <= 1 && in0 >= -1)
				*out = asinf(in0);
			else
				*out = 0.0;
			break;
		}
		case NODE_MATH_ACOS:
		{
			/* Can't do the impossible... */
			if (in0 <= 1 && in0 >= -1)
				*out = acosf(in0);
			else
				*out = 0.0;
			break;
		}
		case NODE_MATH_ATAN:
		{
			*out = atan(in0);
			break;
		}
		case NODE_MATH_POW:
		{
			/* Only raise negative numbers by full integers */
			if (in0 >= 0) {
				out[0] = pow(in0, in1);
			}
			else {
				float y_mod_1 = fmod(in1, 1);
				if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
					*out = pow(in0, floor(in1 + 0.5f));
				}
				else {
					*out = 0.0;
				}
			}
			break;
		}
		case NODE_MATH_LOG:
		{
			/* Don't want any imaginary numbers... */
			if (in0 > 0  && in1 > 0)
				*out = log(in0) / log(in1);
			else
				*out = 0.0;
			break;
		}
		case NODE_MATH_MIN:
		{
			if (in0 < in1)
				*out = in0;
			else
				*out = in1;
			break;
		}
		case NODE_MATH_MAX:
		{
			if (in0 > in1)
				*out = in0;
			else
				*out = in1;
			break;
		}
		case NODE_MATH_ROUND:
		{
			*out = (in0 < 0) ? (int)(in0 - 0.5f) : (int)(in0 + 0.5f);
			break;
		}

		case NODE_MATH_LESS:
		{
			if (in0 < in1)
				*out = 1.0f;
			else
				*out = 0.0f;
			break;
		}

		case NODE_MATH_GREATER:
		{
			if (in0 > in1)
				*out = 1.0f;
			else
				*out = 0.0f;
			break;
		}

		case NODE_MATH_MOD:
		{
			if (in1 == 0.0f)
				*out = 0.0f;
			else
				*out = fmod(in0, in1);
			break;
		}

		case NODE_MATH_ABS:
		{
			*out = fabsf(in0);
			break;
		}

		case NODE_MATH_ATAN2:
		{
			*out = atan2(in0, in1);
			break;
		}

		case NODE_MATH_FLOOR:
		{
			*out = floorf(in0);
			break;
		}

		case NODE_MATH_CEIL:
		{
			*out = ceilf(in0);
			break;
		}

		case NODE_MATH_FRACT:
		{
			*out = in0 - floorf(in0);
			break;
		}

		case NODE_MATH_SQRT:
		{
			if (in0 > 0.0f)
				*out = sqrtf(in0);
			else
				*out = 0.0f;
			break;
		}

		default:
		{
			BLI_assert(0);
			break;
		}
	}

	if (node->custom2 & SHD_MATH_CLAMP) {
		CLAMP(*out, 0.0f, 1.0f);
	}
}

static void exec(void *data, int UNUSED(thread), bNode *node, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, execdata, in, out[0], &valuefn, data);
}

void register_node_type_tex_math(void)
{
	static bNodeType ntype;

	tex_node_type_base(&ntype, TEX_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_label(&ntype, node_math_label);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, NULL, NULL, exec);

	nodeRegisterType(&ntype);
}
