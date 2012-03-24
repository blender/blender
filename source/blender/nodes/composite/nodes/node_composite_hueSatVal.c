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

/** \file blender/nodes/composite/nodes/node_composite_hueSatVal.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** Hue Saturation ******************** */
static bNodeSocketTemplate cmp_node_hue_sat_in[]= {
	{	SOCK_FLOAT, 1, "Fac",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_hue_sat_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void do_hue_sat_fac(bNode *node, float *out, float *in, float *fac)
{
	NodeHueSat *nhs= node->storage;
	
	if (*fac!=0.0f && (nhs->hue!=0.5f || nhs->sat!=1.0f || nhs->val!=1.0f)) {
		float col[3], hsv[3], mfac= 1.0f - *fac;
		
		rgb_to_hsv(in[0], in[1], in[2], hsv, hsv+1, hsv+2);
		hsv[0]+= (nhs->hue - 0.5f);
		if (hsv[0]>1.0f) hsv[0]-=1.0f; else if (hsv[0]<0.0f) hsv[0]+= 1.0f;
		hsv[1]*= nhs->sat;
		hsv[2]*= nhs->val;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], col, col+1, col+2);
		
		out[0]= mfac*in[0] + *fac*col[0];
		out[1]= mfac*in[1] + *fac*col[1];
		out[2]= mfac*in[2] + *fac*col[2];
		out[3]= in[3];
	}
	else {
		copy_v4_v4(out, in);
	}
}

static void node_composit_exec_hue_sat(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: Fac, Image */
	/* stack order out: Image */
	if (out[0]->hasoutput==0) return;
	
	/* input no image? then only color operation */
	if (in[1]->data==NULL) {
		do_hue_sat_fac(node, out[0]->vec, in[1]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= dupalloc_compbuf(in[1]->data);
		CompBuf *stackbuf=typecheck_compbuf(cbuf,CB_RGBA);
		
		composit2_pixel_processor(node, stackbuf, stackbuf, in[1]->vec, in[0]->data, in[0]->vec, do_hue_sat_fac, CB_RGBA, CB_VAL);

		out[0]->data= stackbuf;

		/* get rid of intermediary cbuf if it's extra */		
		if (stackbuf!=cbuf)
			free_compbuf(cbuf);
	}
}

static void node_composit_init_hue_sat(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeHueSat *nhs= MEM_callocN(sizeof(NodeHueSat), "node hue sat");
	node->storage= nhs;
	nhs->hue= 0.5f;
	nhs->sat= 1.0f;
	nhs->val= 1.0f;
}

void register_node_type_cmp_hue_sat(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_HUE_SAT, "Hue Saturation Value", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_hue_sat_in, cmp_node_hue_sat_out);
	node_type_size(&ntype, 150, 80, 250);
	node_type_init(&ntype, node_composit_init_hue_sat);
	node_type_storage(&ntype, "NodeHueSat", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_hue_sat);

	nodeRegisterType(ttype, &ntype);
}
