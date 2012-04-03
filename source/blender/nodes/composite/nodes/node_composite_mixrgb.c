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

/** \file blender/nodes/composite/nodes/node_composite_mixrgb.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** MIX RGB ******************** */
static bNodeSocketTemplate cmp_node_mix_rgb_in[]= {
	{	SOCK_FLOAT, 1, "Fac",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_mix_rgb_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void do_mix_rgb(bNode *node, float *out, float *in1, float *in2, float *fac)
{
	float col[3];
	
	copy_v3_v3(col, in1);
	if (node->custom2)
		ramp_blend(node->custom1, col, in2[3]*fac[0], in2);
	else
		ramp_blend(node->custom1, col, fac[0], in2);
	copy_v3_v3(out, col);
	out[3]= in1[3];
}

static void node_composit_exec_mix_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, Image, Image */
	/* stack order out: Image */
	float *fac= in[0]->vec;
	
	if (out[0]->hasoutput==0) return;
	
	/* input no image? then only color operation */
	if (in[1]->data==NULL && in[2]->data==NULL) {
		do_mix_rgb(node, out[0]->vec, in[1]->vec, in[2]->vec, fac);
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf= in[1]->data?in[1]->data:in[2]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		
		composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, fac, do_mix_rgb, CB_RGBA, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
		
		generate_preview(data, node, out[0]->data);
	}
}

/* custom1 = mix type */
void register_node_type_cmp_mix_rgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_mix_rgb_in, cmp_node_mix_rgb_out);
	node_type_size(&ntype, 110, 60, 120);
	node_type_label(&ntype, node_blend_label);
	node_type_exec(&ntype, node_composit_exec_mix_rgb);

	nodeRegisterType(ttype, &ntype);
}
