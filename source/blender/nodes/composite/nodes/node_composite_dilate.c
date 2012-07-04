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

/** \file blender/nodes/composite/nodes/node_composite_dilate.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** Dilate/Erode ******************** */

static bNodeSocketTemplate cmp_node_dilateerode_in[] = {
	{   SOCK_FLOAT, 1, N_("Mask"),      0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{   -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_dilateerode_out[] = {
	{   SOCK_FLOAT, 0, N_("Mask")},
	{   -1, 0, ""   }
};

static void morpho_dilate(CompBuf *cbuf)
{
	int x, y;
	float *p, *rectf = cbuf->rect;
	
	for (y = 0; y < cbuf->y; y++) {
		for (x = 0; x < cbuf->x - 1; x++) {
			p = rectf + cbuf->x * y + x;
			*p = maxf(*p, *(p + 1));
		}
	}

	for (y = 0; y < cbuf->y; y++) {
		for (x = cbuf->x - 1; x >= 1; x--) {
			p = rectf + cbuf->x * y + x;
			*p = maxf(*p, *(p - 1));
		}
	}

	for (x = 0; x < cbuf->x; x++) {
		for (y = 0; y < cbuf->y - 1; y++) {
			p = rectf + cbuf->x * y + x;
			*p = maxf(*p, *(p + cbuf->x));
		}
	}

	for (x = 0; x < cbuf->x; x++) {
		for (y = cbuf->y - 1; y >= 1; y--) {
			p = rectf + cbuf->x * y + x;
			*p = maxf(*p, *(p - cbuf->x));
		}
	}
}

static void morpho_erode(CompBuf *cbuf)
{
	int x, y;
	float *p, *rectf = cbuf->rect;
	
	for (y = 0; y < cbuf->y; y++) {
		for (x = 0; x < cbuf->x - 1; x++) {
			p = rectf + cbuf->x * y + x;
			*p = minf(*p, *(p + 1));
		}
	}

	for (y = 0; y < cbuf->y; y++) {
		for (x = cbuf->x - 1; x >= 1; x--) {
			p = rectf + cbuf->x * y + x;
			*p = minf(*p, *(p - 1));
		}
	}

	for (x = 0; x < cbuf->x; x++) {
		for (y = 0; y < cbuf->y - 1; y++) {
			p = rectf + cbuf->x * y + x;
			*p = minf(*p, *(p + cbuf->x));
		}
	}

	for (x = 0; x < cbuf->x; x++) {
		for (y = cbuf->y - 1; y >= 1; y--) {
			p = rectf + cbuf->x * y + x;
			*p = minf(*p, *(p - cbuf->x));
		}
	}
	
}

static void node_composit_exec_dilateerode(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: mask */
	/* stack order out: mask */
	if (out[0]->hasoutput == 0)
		return;
	
	/* input no image? then only color operation */
	if (in[0]->data == NULL) {
		zero_v4(out[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf = typecheck_compbuf(in[0]->data, CB_VAL);
		CompBuf *stackbuf = dupalloc_compbuf(cbuf);
		short i;
		
		if (node->custom2 > 0) { // positive, dilate
			for (i = 0; i < node->custom2; i++)
				morpho_dilate(stackbuf);
		}
		else if (node->custom2 < 0) { // negative, erode
			for (i = 0; i > node->custom2; i--)
				morpho_erode(stackbuf);
		}
		
		if (cbuf != in[0]->data)
			free_compbuf(cbuf);
		
		out[0]->data = stackbuf;
	}
}

static void node_composit_init_dilateerode(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeDilateErode *data = MEM_callocN(sizeof(NodeDilateErode), "NodeDilateErode");
	data->falloff = PROP_SMOOTH;
	node->storage = data;
}

void register_node_type_cmp_dilateerode(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, CMP_NODE_DILATEERODE, "Dilate/Erode", NODE_CLASS_OP_FILTER, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_dilateerode_in, cmp_node_dilateerode_out);
	node_type_size(&ntype, 130, 100, 320);
	node_type_init(&ntype, node_composit_init_dilateerode);
	node_type_exec(&ntype, node_composit_exec_dilateerode);
	
	node_type_storage(&ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(ttype, &ntype);
}
