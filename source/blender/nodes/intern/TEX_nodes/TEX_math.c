/**
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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../TEX_util.h"



/* **************** SCALAR MATH ******************** */ 
static bNodeSocketType inputs[]= { 
	{ SOCK_VALUE, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f}, 
	{ SOCK_VALUE, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType outputs[]= { 
	{ SOCK_VALUE, 0, "Value", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
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
			/* Don't want any imaginary numbers... */
			if( in0 >= 0 )
				*out= pow(in0, in1);
			else
				*out= 0.0;
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
			*out= (int)(in0 + 0.5f);
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

bNodeType tex_node_math= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_MATH, 
	/* name        */	"Math", 
	/* width+range */	120, 110, 160, 
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS, 
	/* input sock  */	inputs, 
	/* output sock */	outputs, 
	/* storage     */	"node_math", 
	/* execfunc    */	exec,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};

