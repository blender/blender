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
static bNodeSocketTemplate inputs[]= { 
	{ SOCK_FLOAT, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE}, 
	{ SOCK_FLOAT, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE}, 
	{ -1, 0, "" } 
};

static bNodeSocketTemplate outputs[]= { 
	{ SOCK_FLOAT, 0, "Value"}, 
	{ -1, 0, "" } 
};

static void valuefn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float in0 = tex_input_value(in[0], p, thread);
	float in1 = tex_input_value(in[1], p, thread);
	
	switch(node->custom1){ 
	
	case 0: /* Add */
		*out= in0 + in1; 
		break; 
	case 1: /* Subtract */
		*out= in0 - in1;
		break; 
	case 2: /* Multiply */
		*out= in0 * in1; 
		break; 
	case 3: /* Divide */
		{
			if(in1==0)	/* We don't want to divide by zero. */
				*out= 0.0;
			else
				*out= in0 / in1;
			}
		break;
	case 4: /* Sine */
		{
			*out= sin(in0);
		}
		break;
	case 5: /* Cosine */
		{
			*out= cos(in0);
		}
		break;
	case 6: /* Tangent */
		{
			*out= tan(in0);
		}
		break;
	case 7: /* Arc-Sine */
		{
			/* Can't do the impossible... */
			if( in0 <= 1 && in0 >= -1 )
				*out= asin(in0);
			else
				*out= 0.0;
		}
		break;
	case 8: /* Arc-Cosine */
		{
			/* Can't do the impossible... */
			if( in0 <= 1 && in0 >= -1 )
				*out= acos(in0);
			else
				*out= 0.0;
		}
		break;
	case 9: /* Arc-Tangent */
		{
			*out= atan(in0);
		}
		break;
	case 10: /* Power */
		{
			/* Only raise negative numbers by full integers */
			if( in0 >= 0 ) {
				out[0]= pow(in0, in1);
			} else {
				float y_mod_1 = fmod(in1, 1);
				if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
					*out = pow(in0, floor(in1 + 0.5f));
				} else {
					*out = 0.0;
				}
			}
		}
		break;
	case 11: /* Logarithm */
		{
			/* Don't want any imaginary numbers... */
			if( in0 > 0  && in1 > 0 )
				*out= log(in0) / log(in1);
			else
				*out= 0.0;
		}
		break;
	case 12: /* Minimum */
		{
			if( in0 < in1 )
				*out= in0;
			else
				*out= in1;
		}
		break;
	case 13: /* Maximum */
		{
			if( in0 > in1 )
				*out= in0;
			else
				*out= in1;
		}
		break;
	case 14: /* Round */
		{
			*out= (in0<0)?(int)(in0 - 0.5f):(int)(in0 + 0.5f);
		}
		break; 
		
	case 15: /* Less Than */
		{
			if( in0 < in1 )
				*out= 1.0f;
			else
				*out= 0.0f;
		}
		break;
		
	case 16: /* Greater Than */
		{
			if( in0 > in1 )
				*out= 1.0f;
			else
				*out= 0.0f;
		}
		break;
		
	default:
		fprintf(stderr,
			"%s:%d: unhandeld value in switch statement: %d\n",
			__FILE__, __LINE__, node->custom1);
	} 
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &valuefn, data);
}

void register_node_type_tex_math(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 120, 110, 160);
	node_type_label(&ntype, node_math_label);
	node_type_storage(&ntype, "node_math", NULL, NULL);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
