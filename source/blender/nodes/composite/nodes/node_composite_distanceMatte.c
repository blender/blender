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
	{SOCK_RGBA, 1, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f},
	{SOCK_RGBA, 1, N_("Key Color"), 1.0f, 1.0f, 1.0f, 1.0f},
	{-1, 0, ""}
};

static bNodeSocketTemplate cmp_node_distance_matte_out[]={
	{SOCK_RGBA, 0, N_("Image")},
	{SOCK_FLOAT, 0, N_("Matte")},
	{-1, 0, ""}
};

/* note, keyvals is passed on from caller as stack array */
/* might have been nicer as temp struct though... */
static void do_distance_matte(bNode *node, float *out, float *in)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float tolerence=c->t1;
	float fper=c->t2;
	/* get falloff amount over tolerence size */
	float falloff=(1.0f-fper) * tolerence;
	float distance;
	float alpha;

	distance=sqrt((c->key[0]-in[0])*(c->key[0]-in[0]) +
		(c->key[1]-in[1])*(c->key[1]-in[1]) +
		(c->key[2]-in[2])*(c->key[2]-in[2]));

	copy_v3_v3(out, in);

	if (distance <= tolerence) {
		if (distance <= falloff) {
			alpha = 0.0f;
		}
		else {
			/* alpha as percent (distance / tolerance), each modified by falloff amount (in pixels)*/
			alpha=(distance-falloff)/(tolerence-falloff);
		}

		/*only change if more transparent than before */
		if (alpha < in[3]) {
			/*clamp*/
			if (alpha < 0.0f) alpha = 0.0f;
			if (alpha > 1.0f) alpha = 1.0f;
			out[3]=alpha;
		}
		else { /* leave as before */
			out[3]=in[3];
		}
	}
}

static void do_chroma_distance_matte(bNode *node, float *out, float *in)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float tolerence=c->t1;
	float fper=c->t2;
	/* get falloff amount over tolerence size */
	float falloff=(1.0f-fper) * tolerence;
	float y_key, cb_key, cr_key;
	float y_pix, cb_pix, cr_pix;
	float distance;
	float alpha;

	/*convert key to chroma colorspace */
	rgb_to_ycc(c->key[0], c->key[1], c->key[2], &y_key, &cb_key, &cr_key, BLI_YCC_JFIF_0_255);
	/* normalize the values */
	cb_key=cb_key/255.0f;
	cr_key=cr_key/255.0f;

	/*convert pixel to chroma colorspace */
	rgb_to_ycc(in[0], in[1], in[2], &y_pix, &cb_pix, &cr_pix, BLI_YCC_JFIF_0_255);
	/*normalize the values */
	cb_pix=cb_pix/255.0f;
	cr_pix=cr_pix/255.0f;

	distance=sqrt((cb_key-cb_pix)*(cb_key-cb_pix) +
		(cr_key-cr_pix)*(cr_key-cr_pix));

	copy_v3_v3(out, in);

	if (distance <= tolerence) {
		if (distance <= falloff) {
			alpha = 0.0f;
		}
		else {
			/* alpha as percent (distance / tolerance), each modified by falloff amount (in pixels)*/
			alpha=(distance-falloff)/(tolerence-falloff);
		}

		/*only change if more transparent than before */
		if (alpha < in[3]) {
			/*clamp*/
			if (alpha < 0.0f) alpha = 0.0f;
			if (alpha > 1.0f) alpha = 1.0f;
			out[3]=alpha;
		}
		else { /* leave as before */
			out[3]=in[3];
		}
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

	/* work in RGB color space */
	if (c->channel == 1) {
		/* note, processor gets a keyvals array passed on as buffer constant */
		composit1_pixel_processor(node, workbuf, workbuf, in[0]->vec, do_distance_matte, CB_RGBA);
	}
	/* work in YCbCr color space */
	else {
		composit1_pixel_processor(node, workbuf, workbuf, in[0]->vec, do_chroma_distance_matte, CB_RGBA);
	}



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
	c->channel=1;
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
