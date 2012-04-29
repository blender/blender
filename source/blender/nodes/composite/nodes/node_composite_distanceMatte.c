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

/** \file blender/nodes/composite/nodes/node_composite_distanceMatte.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* ******************* channel Distance Matte ********************************* */
static bNodeSocketTemplate cmp_node_distance_matte_in[]={
	{SOCK_RGBA, 1, "Image", 1.0f, 1.0f, 1.0f, 1.0f},
	{SOCK_RGBA, 1, "Key Color", 1.0f, 1.0f, 1.0f, 1.0f},
	{-1, 0, ""}
};

static bNodeSocketTemplate cmp_node_distance_matte_out[]={
	{SOCK_RGBA, 0, "Image"},
	{SOCK_FLOAT, 0, "Matte"},
	{-1, 0, ""}
};

/* note, keyvals is passed on from caller as stack array */
/* might have been nicer as temp struct though... */
static void do_distance_matte(bNode *node, float *out, float *in)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float tolerence=c->t1;
	float falloff=c->t2;
	float distance;
	float alpha;

	distance=sqrt((c->key[0]-in[0])*(c->key[0]-in[0]) +
				  (c->key[1]-in[1])*(c->key[1]-in[1]) +
				  (c->key[2]-in[2])*(c->key[2]-in[2]));

	copy_v3_v3(out, in);

	/*make 100% transparent */
	if (distance < tolerence) {
		out[3]=0.0;
	}
	/*in the falloff region, make partially transparent */
	else if (distance < falloff+tolerence) {
		distance=distance-tolerence;
		alpha=distance/falloff;
		/*only change if more transparent than before */
		if (alpha < in[3]) {
			out[3]=alpha;
		}
		else { /* leave as before */
			out[3]=in[3];
		}
	}
	else {
		out[3]=in[3];
	}
}

static void node_composit_exec_distance_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/*
	Loosely based on the Sequencer chroma key plug-in, but enhanced to work in other color spaces and
	uses a different difference function (suggested in forums of vfxtalk.com).
	*/
	CompBuf *workbuf;
	CompBuf *inbuf;
	NodeChroma *c;
	
	/*is anything connected?*/
	if (out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	/*must have an image imput*/
	if (in[0]->data==NULL) return;
	
	inbuf=typecheck_compbuf(in[0]->data, CB_RGBA);
	
	c=node->storage;
	workbuf=dupalloc_compbuf(inbuf);
	
	/*use the input color*/
	c->key[0]= in[1]->vec[0];
	c->key[1]= in[1]->vec[1];
	c->key[2]= in[1]->vec[2];
	
	/* note, processor gets a keyvals array passed on as buffer constant */
	composit1_pixel_processor(node, workbuf, workbuf, in[0]->vec, do_distance_matte, CB_RGBA);
	
	
	out[0]->data=workbuf;
	if (out[1]->hasoutput)
		out[1]->data=valbuf_from_rgbabuf(workbuf, CHAN_A);
	generate_preview(data, node, workbuf);

	if (inbuf!=in[0]->data)
		free_compbuf(inbuf);
}

static void node_composit_init_distance_matte(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
	node->storage= c;
	c->t1= 0.1f;
	c->t2= 0.1f;
}

void register_node_type_cmp_distance_matte(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_DIST_MATTE, "Distance Key", NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_distance_matte_in, cmp_node_distance_matte_out);
	node_type_size(&ntype, 200, 80, 250);
	node_type_init(&ntype, node_composit_init_distance_matte);
	node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_distance_matte);

	nodeRegisterType(ttype, &ntype);
}
