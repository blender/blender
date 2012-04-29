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
 * Contributor(s): Bob Holcomb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_diffMatte.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* ******************* channel Difference Matte ********************************* */
static bNodeSocketTemplate cmp_node_diff_matte_in[]={
	{SOCK_RGBA, 1, "Image 1", 1.0f, 1.0f, 1.0f, 1.0f},
	{SOCK_RGBA, 1, "Image 2", 1.0f, 1.0f, 1.0f, 1.0f},
	{-1, 0, ""}
};

static bNodeSocketTemplate cmp_node_diff_matte_out[]={
	{SOCK_RGBA, 0, "Image"},
	{SOCK_FLOAT, 0, "Matte"},
	{-1, 0, ""}
};

static void do_diff_matte(bNode *node, float *outColor, float *inColor1, float *inColor2)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float tolerence=c->t1;
	float falloff=c->t2;
	float difference;
	float alpha;

	difference= fabs(inColor2[0]-inColor1[0]) +
	        fabs(inColor2[1]-inColor1[1]) +
	        fabs(inColor2[2]-inColor1[2]);

	/*average together the distances*/
	difference=difference/3.0f;

	copy_v3_v3(outColor, inColor1);

	/*make 100% transparent*/
	if (difference < tolerence) {
		outColor[3]=0.0;
	}
	/*in the falloff region, make partially transparent */
	else if (difference < falloff+tolerence) {
		difference=difference-tolerence;
		alpha=difference/falloff;
		/*only change if more transparent than before */
		if (alpha < inColor1[3]) {
			outColor[3]=alpha;
		}
		else { /* leave as before */
			outColor[3]=inColor1[3];
		}
	}
	else {
		/*foreground object*/
		outColor[3]= inColor1[3];
	}
}

static void node_composit_exec_diff_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *outbuf= NULL;
	CompBuf *imbuf1= NULL;
	CompBuf *imbuf2= NULL;
	/* NodeChroma *c; */ /* UNUSED */

	/*is anything connected?*/
	if (out[0]->hasoutput==0 && out[1]->hasoutput==0) return;

	/*must have an image imput*/
	if (in[0]->data==NULL) return;


	imbuf1=typecheck_compbuf(in[0]->data, CB_RGBA);

	/* if there's an image, use that, if not use the color */
	if (in[1]->data) {
		imbuf2=typecheck_compbuf(in[1]->data, CB_RGBA);
	}

	/* c=node->storage; */ /* UNUSED */
	outbuf=dupalloc_compbuf(imbuf1);

	/* note, processor gets a keyvals array passed on as buffer constant */
	composit2_pixel_processor(node, outbuf, imbuf1, in[0]->vec, imbuf2, in[1]->vec, do_diff_matte, CB_RGBA, CB_RGBA);

	out[0]->data=outbuf;
	if (out[1]->hasoutput)
		out[1]->data=valbuf_from_rgbabuf(outbuf, CHAN_A);
	generate_preview(data, node, outbuf);

	if (imbuf1!=in[0]->data)
		free_compbuf(imbuf1);

	if (imbuf2!=in[1]->data)
		free_compbuf(imbuf2);
}

static void node_composit_init_diff_matte(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
	node->storage= c;
	c->t1= 0.1f;
	c->t2= 0.1f;
}

void register_node_type_cmp_diff_matte(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_DIFF_MATTE, "Difference Key", NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_diff_matte_in, cmp_node_diff_matte_out);
	node_type_size(&ntype, 200, 80, 250);
	node_type_init(&ntype, node_composit_init_diff_matte);
	node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_diff_matte);

	nodeRegisterType(ttype, &ntype);
}
