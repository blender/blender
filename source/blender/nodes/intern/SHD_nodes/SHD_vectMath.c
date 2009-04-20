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


/* **************** VECTOR MATH ******************** */ 
static bNodeSocketType sh_node_vect_math_in[]= { 
	{ SOCK_VECTOR, 1, "Vector", 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f}, 
	{ SOCK_VECTOR, 1, "Vector", 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType sh_node_vect_math_out[]= {
	{ SOCK_VECTOR, 0, "Vector", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ SOCK_VALUE, 0, "Value", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ -1, 0, "" } 
};

static void node_shader_exec_vect_math(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{ 
	float vec1[3], vec2[3];
	
	nodestack_get_vec(vec1, SOCK_VECTOR, in[0]);
	nodestack_get_vec(vec2, SOCK_VECTOR, in[1]);
	
	if(node->custom1 == 0) {	/* Add */
		out[0]->vec[0]= vec1[0] + vec2[0];
		out[0]->vec[1]= vec1[1] + vec2[1];
		out[0]->vec[2]= vec1[2] + vec2[2];
		
		out[1]->vec[0]= (fabs(out[0]->vec[0]) + fabs(out[0]->vec[0]) + fabs(out[0]->vec[0])) / 3;
	}
	else if(node->custom1 == 1) {	/* Subtract */
		out[0]->vec[0]= vec1[0] - vec2[0];
		out[0]->vec[1]= vec1[1] - vec2[1];
		out[0]->vec[2]= vec1[2] - vec2[2];
		
		out[1]->vec[0]= (fabs(out[0]->vec[0]) + fabs(out[0]->vec[0]) + fabs(out[0]->vec[0])) / 3;
	}
	else if(node->custom1 == 2) {	/* Average */
		out[0]->vec[0]= vec1[0] + vec2[0];
		out[0]->vec[1]= vec1[1] + vec2[1];
		out[0]->vec[2]= vec1[2] + vec2[2];
		
		out[1]->vec[0] = Normalize( out[0]->vec );
	}
	else if(node->custom1 == 3) {	/* Dot product */
		out[1]->vec[0]= (vec1[0] * vec2[0]) + (vec1[1] * vec2[1]) + (vec1[2] * vec2[2]);
	}
	else if(node->custom1 == 4) {	/* Cross product */
		out[0]->vec[0]= (vec1[1] * vec2[2]) - (vec1[2] * vec2[1]);
		out[0]->vec[1]= (vec1[2] * vec2[0]) - (vec1[0] * vec2[2]);
		out[0]->vec[2]= (vec1[0] * vec2[1]) - (vec1[1] * vec2[0]);
		
		out[1]->vec[0] = Normalize( out[0]->vec );
	}
	else if(node->custom1 == 5) {	/* Normalize */
		if(in[0]->hasinput || !in[1]->hasinput) {	/* This one only takes one input, so we've got to choose. */
			out[0]->vec[0]= vec1[0];
			out[0]->vec[1]= vec1[1];
			out[0]->vec[2]= vec1[2];
		}
		else {
			out[0]->vec[0]= vec2[0];
			out[0]->vec[1]= vec2[1];
			out[0]->vec[2]= vec2[2];
		}
		
		out[1]->vec[0] = Normalize( out[0]->vec );
	}
	
}

static int gpu_shader_vect_math(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	static char *names[] = {"vec_math_add", "vec_math_subtract",
		"vec_math_average", "vec_math_dot", "vec_math_cross",
		"vec_math_normalize"};

	switch (node->custom1) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			GPU_stack_link(mat, names[node->custom1], NULL, out,
				GPU_socket(&in[0]), GPU_socket(&in[1]));
			break;
		case 5:
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

bNodeType sh_node_vect_math= { 
	/* *next,*prev */	NULL, NULL,
	/* type code   */ SH_NODE_VECT_MATH, 
	/* name        */ "Vector Math", 
	/* width+range */ 80, 75, 140, 
	/* class+opts  */ NODE_CLASS_CONVERTOR, NODE_OPTIONS, 
	/* input sock  */ sh_node_vect_math_in, 
	/* output sock */ sh_node_vect_math_out, 
	/* storage     */ "node_vect_math", 
	/* execfunc    */ node_shader_exec_vect_math,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_vect_math
};

