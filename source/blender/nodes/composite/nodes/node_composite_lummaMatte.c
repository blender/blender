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
 * Contributor(s): Bob Holcomb .
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_lummaMatte.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* ******************* Luma Matte Node ********************************* */
static bNodeSocketTemplate cmp_node_luma_matte_in[]={
	{SOCK_RGBA, 1, "Image", 1.0f, 1.0f, 1.0f, 1.0f},
	{-1, 0, ""}
};

static bNodeSocketTemplate cmp_node_luma_matte_out[]={
	{SOCK_RGBA, 0, "Image"},
	{SOCK_FLOAT, 0, "Matte"},
	{-1, 0, ""}
};

static void do_luma_matte(bNode *node, float *out, float *in)
{
	NodeChroma *c=(NodeChroma *)node->storage;
	float alpha;

	/* test range*/
	if (in[0]>c->t1) {
		alpha=1.0;
	}
	else if (in[0]<c->t2) {
		alpha=0.0;
	}
	else {/*blend */
		alpha=(in[0]-c->t2)/(c->t1-c->t2);
	}
	
	/* don't make something that was more transparent less transparent */
	if (alpha<in[3]) {
		out[3]=alpha;
	}
	else {
		out[3]=in[3];
	}

}

static void node_composit_exec_luma_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf;
	CompBuf *outbuf;
	
	if (in[0]->hasinput==0)  return;
	if (in[0]->data==NULL) return;
	if (out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	
	cbuf=typecheck_compbuf(in[0]->data, CB_RGBA);
	
	outbuf=dupalloc_compbuf(cbuf);

	composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_rgba_to_yuva, CB_RGBA);
	composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_luma_matte, CB_RGBA);
	composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_yuva_to_rgba, CB_RGBA);
	
	generate_preview(data, node, outbuf);
	out[0]->data=outbuf;
	if (out[1]->hasoutput)
		out[1]->data=valbuf_from_rgbabuf(outbuf, CHAN_A);
	if (cbuf!=in[0]->data)
		free_compbuf(cbuf);
}

static void node_composit_init_luma_matte(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
	node->storage=c;
	c->t1= 1.0f;
	c->t2= 0.0f;
}

void register_node_type_cmp_luma_matte(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_LUMA_MATTE, "Luminance Key", NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_luma_matte_in, cmp_node_luma_matte_out);
	node_type_size(&ntype, 200, 80, 250);
	node_type_init(&ntype, node_composit_init_luma_matte);
	node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_luma_matte);

	nodeRegisterType(ttype, &ntype);
}
