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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):  gsr b3d, and a very minor edit from Robert Holcomb 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"


/* **************** NORMALIZE single channel, useful for Z buffer ******************** */
static bNodeSocketType cmp_node_normalize_in[]= {
	{   SOCK_VALUE, 1, "Value",         1.0f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{   -1, 0, ""   }
};
static bNodeSocketType cmp_node_normalize_out[]= {
	{   SOCK_VALUE, 0, "Value",         1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{   -1, 0, ""   }
};

static void do_normalize(bNode *node, float *out, float *src, float *min, float *mult)
{
	float res;
	res = (src[0] - min[0]) * mult[0];
	if (res > 1.0f) {
		out[0] = 1.0f;
	}
	else if (res < 0.0f) {
		out[0] = 0.0f;
	}
	else {
		out[0] = res;
	}
}

/* The code below assumes all data is inside range +- this, and that input buffer is single channel */
#define BLENDER_ZMAX 10000.0f

static void node_composit_exec_normalize(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: valbuf */
	/* stack order out: valbuf */
	if(out[0]->hasoutput==0) return;

	/* Input has no image buffer? Then pass the value */
	if(in[0]->data==NULL) {
		QUATCOPY(out[0]->vec, in[0]->vec);
	}
	else {
		float min = 1.0f+BLENDER_ZMAX;
		float max = -1.0f-BLENDER_ZMAX;
		float mult = 1.0f;
		float *val;
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		int tot= cbuf->x*cbuf->y;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */

		for (val = cbuf->rect; tot; tot--, val++) {
			if ((*val > max) && (*val <= BLENDER_ZMAX)) {
				max = *val;
			}
			if ((*val < min) && (*val >= -BLENDER_ZMAX)) {
				min = *val;
			}
		}
		/* In the rare case of flat buffer, which would cause a divide by 0, just pass the input to the output */
		if ((max-min) != 0.0f) {
			mult = 1.0f/(max-min);
			composit3_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, NULL, &min, NULL, &mult, do_normalize, CB_VAL, CB_VAL, CB_VAL);
		} else {
			memcpy(stackbuf->rect, cbuf->rect, sizeof(float) * cbuf->x * cbuf->y);
		}

		out[0]->data= stackbuf;
	}
}

bNodeType cmp_node_normalize= {
	/* *next, *prev*/   NULL, NULL,
	/* type code   */   CMP_NODE_NORMALIZE,
	/* name        */   "Normalize",
	/* width+range */   100, 60, 150,
	/* class+opts  */   NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */   cmp_node_normalize_in,
	/* output sock */   cmp_node_normalize_out,
	/* storage     */   "TexMapping",
	/* execfunc    */   node_composit_exec_normalize, 
	/* butfunc     */   NULL, 
	/* initfunc    */   NULL, 
	/* freestoragefunc	*/ NULL, 
	/* copystoragefunc	*/ NULL, 
	/* id		*/	NULL
};
