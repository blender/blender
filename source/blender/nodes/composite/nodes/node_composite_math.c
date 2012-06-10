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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_math.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** SCALAR MATH ******************** */ 
static bNodeSocketTemplate cmp_node_math_in[]= { 
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE}, 
	{ SOCK_FLOAT, 1, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE}, 
	{ -1, 0, "" } 
};

static bNodeSocketTemplate cmp_node_math_out[]= { 
	{ SOCK_FLOAT, 0, N_("Value")}, 
	{ -1, 0, "" } 
};

static void do_math(bNode *node, float *out, float *in, float *in2)
{
	switch (node->custom1) {
	case 0: /* Add */
		out[0]= in[0] + in2[0]; 
		break; 
	case 1: /* Subtract */
		out[0]= in[0] - in2[0];
		break; 
	case 2: /* Multiply */
		out[0]= in[0] * in2[0]; 
		break; 
	case 3: /* Divide */
		{
			if (in2[0]==0)	/* We don't want to divide by zero. */
				out[0]= 0.0;
			else
				out[0]= in[0] / in2[0];
			}
		break;
	case 4: /* Sine */
		out[0]= sin(in[0]);
		break;
	case 5: /* Cosine */
		out[0]= cos(in[0]);
		break;
	case 6: /* Tangent */
		out[0]= tan(in[0]);
		break;
	case 7: /* Arc-Sine */
		{
			/* Can't do the impossible... */
			if (in[0] <= 1 && in[0] >= -1 )
				out[0]= asin(in[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 8: /* Arc-Cosine */
		{
			/* Can't do the impossible... */
			if ( in[0] <= 1 && in[0] >= -1 )
				out[0]= acos(in[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 9: /* Arc-Tangent */
		out[0]= atan(in[0]);
		break;
	case 10: /* Power */
		{
			/* Only raise negative numbers by full integers */
			if ( in[0] >= 0 ) {
				out[0]= pow(in[0], in2[0]);
			}
			else {
				float y_mod_1 = fmod(in2[0], 1);
				/* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
				if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
					out[0]= powf(in[0], floorf(in2[0] + 0.5f));
				}
				else {
					out[0] = 0.0f;
				}
			}
		}
		break;
	case 11: /* Logarithm */
		{
			/* Don't want any imaginary numbers... */
			if ( in[0] > 0  && in2[0] > 0 )
				out[0]= log(in[0]) / log(in2[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 12: /* Minimum */
		{
			if ( in[0] < in2[0] )
				out[0]= in[0];
			else
				out[0]= in2[0];
		}
		break;
	case 13: /* Maximum */
		{
			if ( in[0] > in2[0] )
				out[0]= in[0];
			else
				out[0]= in2[0];
		}
		break;
	case 14: /* Round */
		{
			/* round by the second value */
			if ( in2[0] != 0.0f )
				out[0]= floorf(in[0] / in2[0] + 0.5f) * in2[0];
			else
				out[0]= floorf(in[0] + 0.5f);
		}
		break;
	case 15: /* Less Than */
		{
			if ( in[0] < in2[0] )
				out[0]= 1.0f;
			else
				out[0]= 0.0f;
		}
		break;
	case 16: /* Greater Than */
		{
			if ( in[0] > in2[0] )
				out[0]= 1.0f;
			else
				out[0]= 0.0f;
		}
		break;
	}
}

static void node_composit_exec_math(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf=in[0]->data;
	CompBuf *cbuf2=in[1]->data;
	CompBuf *stackbuf; 

	/* check for inputs and outputs for early out*/
	if (out[0]->hasoutput==0) return;

	/* no image-color operation */
	if (in[0]->data==NULL && in[1]->data==NULL) {
		do_math(node, out[0]->vec, in[0]->vec, in[1]->vec);
		return;
	}

	/* create output based on first input */
	if (cbuf) {
		stackbuf=alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
	}
	/* and if it doesn't exist use the second input since we 
	 * know that one of them must exist at this point*/
	else {
		stackbuf=alloc_compbuf(cbuf2->x, cbuf2->y, CB_VAL, 1);
	}

	/* operate in case there's valid size */
	composit2_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_math, CB_VAL, CB_VAL);
	out[0]->data= stackbuf;
}

void register_node_type_cmp_math(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_math_in, cmp_node_math_out);
	node_type_size(&ntype, 120, 110, 160);
	node_type_label(&ntype, node_math_label);
	node_type_exec(&ntype, node_composit_exec_math);

	nodeRegisterType(ttype, &ntype);
}
