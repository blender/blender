/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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



/* **************** SCALAR MATH ******************** */ 
static bNodeSocketType sh_node_math_in[]= { 
	{ SOCK_VALUE, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f}, 
	{ SOCK_VALUE, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType sh_node_math_out[]= { 
	{ SOCK_VALUE, 0, "Value", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void node_shader_exec_math(void *data, bNode *node, bNodeStack **in, 
bNodeStack **out) 
{
	switch(node->custom1){ 
	
	case 0: /* Add */
		out[0]->vec[0]= in[0]->vec[0] + in[1]->vec[0]; 
		break; 
	case 1: /* Subtract */
		out[0]->vec[0]= in[0]->vec[0] - in[1]->vec[0];
		break; 
	case 2: /* Multiply */
		out[0]->vec[0]= in[0]->vec[0] * in[1]->vec[0]; 
		break; 
	case 3: /* Divide */
		{
			if(in[1]->vec[0]==0)	/* We don't want to divide by zero. */
				out[0]->vec[0]= 0.0;
			else
				out[0]->vec[0]= in[0]->vec[0] / in[1]->vec[0];
			}
		break;
	case 4: /* Sine */
		{
			if(in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */
				out[0]->vec[0]= sin(in[0]->vec[0]);
			else
				out[0]->vec[0]= sin(in[1]->vec[0]);
		}
		break;
	case 5: /* Cosine */
		{
			if(in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */	
				out[0]->vec[0]= cos(in[0]->vec[0]);
			else
				out[0]->vec[0]= cos(in[1]->vec[0]);
		}
		break;
	case 6: /* Tangent */
		{
			if(in[0]->hasinput || !in[1]->hasinput)  /* This one only takes one input, so we've got to choose. */	
				out[0]->vec[0]= tan(in[0]->vec[0]);
			else
				out[0]->vec[0]= tan(in[1]->vec[0]);
		}
		break;
	case 7: /* Arc-Sine */
		{
			if(in[0]->hasinput || !in[1]->hasinput) { /* This one only takes one input, so we've got to choose. */
				/* Can't do the impossible... */
				if( in[0]->vec[0] <= 1 && in[0]->vec[0] >= -1 )
					out[0]->vec[0]= asin(in[0]->vec[0]);
				else
					out[0]->vec[0]= 0.0;
			}
			else {
				/* Can't do the impossible... */
				if( in[1]->vec[0] <= 1 && in[1]->vec[0] >= -1 )
					out[0]->vec[0]= asin(in[1]->vec[0]);
				else
					out[0]->vec[0]= 0.0;
			}
		}
		break;
	case 8: /* Arc-Cosine */
		{
			if(in[0]->hasinput || !in[1]->hasinput) { /* This one only takes one input, so we've got to choose. */
				/* Can't do the impossible... */
				if( in[0]->vec[0] <= 1 && in[0]->vec[0] >= -1 )
					out[0]->vec[0]= acos(in[0]->vec[0]);
				else
					out[0]->vec[0]= 0.0;
			}
			else {
				/* Can't do the impossible... */
				if( in[1]->vec[0] <= 1 && in[1]->vec[0] >= -1 )
					out[0]->vec[0]= acos(in[1]->vec[0]);
				else
					out[0]->vec[0]= 0.0;
			}
		}
		break;
	case 9: /* Arc-Tangent */
		{
			if(in[0]->hasinput || !in[1]->hasinput) /* This one only takes one input, so we've got to choose. */
				out[0]->vec[0]= atan(in[0]->vec[0]);
			else
				out[0]->vec[0]= atan(in[1]->vec[0]);
		}
		break;
	case 10: /* Power */
		{
			/* Don't want any imaginary numbers... */
			if( in[0]->vec[0] >= 0 )
				out[0]->vec[0]= pow(in[0]->vec[0], in[1]->vec[0]);
			else
				out[0]->vec[0]= 0.0;
		}
		break;
	case 11: /* Logarithm */
		{
			/* Don't want any imaginary numbers... */
			if( in[0]->vec[0] > 0  && in[1]->vec[0] > 0 )
				out[0]->vec[0]= log(in[0]->vec[0]) / log(in[1]->vec[0]);
			else
				out[0]->vec[0]= 0.0;
		}
		break;
	case 12: /* Minimum */
		{
			if( in[0]->vec[0] < in[1]->vec[0] )
				out[0]->vec[0]= in[0]->vec[0];
			else
				out[0]->vec[0]= in[1]->vec[0];
		}
		break;
	case 13: /* Maximum */
		{
			if( in[0]->vec[0] > in[1]->vec[0] )
				out[0]->vec[0]= in[0]->vec[0];
			else
				out[0]->vec[0]= in[1]->vec[0];
		}
		break;
	case 14: /* Round */
		{
			if(in[0]->hasinput || !in[1]->hasinput) /* This one only takes one input, so we've got to choose. */
				out[0]->vec[0]= (int)(in[0]->vec[0] + 0.5f);
			else
				out[0]->vec[0]= (int)(in[1]->vec[0] + 0.5f);
		}
		break;
	case 15: /* Less Than */
		{
			if( in[0]->vec[0] < in[1]->vec[0] )
				out[0]->vec[0]= 1.0f;
			else
				out[0]->vec[0]= 0.0f;
		}
		break;
	case 16: /* Greater Than */
		{
			if( in[0]->vec[0] > in[1]->vec[0] )
				out[0]->vec[0]= 1.0f;
			else
				out[0]->vec[0]= 0.0f;
		}
		break;
	} 
}

static int gpu_shader_math(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	static char *names[] = {"math_add", "math_subtract", "math_multiply",
		"math_divide", "math_sine", "math_cosine", "math_tangnet", "math_asin",
		"math_acos", "math_atan", "math_pow", "math_log", "math_min", "math_max",
		"math_round", "math_less_than", "math_greater_than"};

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
			GPU_stack_link(mat, names[node->custom1], NULL, out,
				GPU_socket(&in[0]), GPU_socket(&in[1]));
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 14:
			if(in[0].hasinput || !in[1].hasinput)
				GPU_stack_link(mat, names[node->custom1], NULL, out, GPU_socket(&in[0]));
			else
				GPU_stack_link(mat, names[node->custom1], NULL, out, GPU_socket(&in[1]));
			break;
		default:
			return 0;
	}
	
	return 1;
}

bNodeType sh_node_math= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_MATH, 
	/* name        */	"Math", 
	/* width+range */	120, 110, 160, 
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS, 
	/* input sock  */	sh_node_math_in, 
	/* output sock */	sh_node_math_out, 
	/* storage     */	"node_math", 
	/* execfunc    */	node_shader_exec_math,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_math
};

