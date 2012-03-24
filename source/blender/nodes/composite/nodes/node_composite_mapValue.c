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

/** \file blender/nodes/composite/nodes/node_composite_mapValue.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** MAP VALUE ******************** */
static bNodeSocketTemplate cmp_node_map_value_in[]= {
	{	SOCK_FLOAT, 1, "Value",			1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_map_value_out[]= {
	{	SOCK_FLOAT, 0, "Value"},
	{	-1, 0, ""	}
};

static void do_map_value(bNode *node, float *out, float *src)
{
	TexMapping *texmap= node->storage;
	
	out[0]= (src[0] + texmap->loc[0])*texmap->size[0];
	if (texmap->flag & TEXMAP_CLIP_MIN)
		if (out[0]<texmap->min[0])
			out[0]= texmap->min[0];
	if (texmap->flag & TEXMAP_CLIP_MAX)
		if (out[0]>texmap->max[0])
			out[0]= texmap->max[0];
}

static void node_composit_exec_map_value(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: valbuf */
	/* stack order out: valbuf */
	if (out[0]->hasoutput==0) return;
	
	/* input no image? then only value operation */
	if (in[0]->data==NULL) {
		do_map_value(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_map_value, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}


static void node_composit_init_map_value(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= add_tex_mapping();
}

void register_node_type_cmp_map_value(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MAP_VALUE, "Map Value", NODE_CLASS_OP_VECTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_map_value_in, cmp_node_map_value_out);
	node_type_size(&ntype, 100, 60, 150);
	node_type_init(&ntype, node_composit_init_map_value);
	node_type_storage(&ntype, "TexMapping", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_map_value);

	nodeRegisterType(ttype, &ntype);
}
