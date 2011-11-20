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

/** \file blender/nodes/composite/nodes/node_composite_chromaMatte.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* ******************* Chroma Key ********************************************************** */
static bNodeSocketTemplate cmp_node_chroma_in[]={
	{SOCK_RGBA,1,"Image", 1.0f, 1.0f, 1.0f, 1.0f},
	{SOCK_RGBA,1,"Key Color", 1.0f, 1.0f, 1.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketTemplate cmp_node_chroma_out[]={
	{SOCK_RGBA,0,"Image"},
	{SOCK_FLOAT,0,"Matte"},
	{-1,0,""}
};

static void do_rgba_to_ycca_normalized(bNode *UNUSED(node), float *out, float *in)
{
	rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2], BLI_YCC_ITU_BT601);

	//normalize to 0..1.0
	out[0]=out[0]/255.0f;
	out[1]=out[1]/255.0f;
	out[2]=out[2]/255.0f;

	//rescale to -1.0..1.0
	out[0]=(out[0]*2.0f)-1.0f;
	out[1]=(out[1]*2.0f)-1.0f;
	out[2]=(out[2]*2.0f)-1.0f;

//	out[0]=((out[0])-16)/255.0;
//	out[1]=((out[1])-128)/255.0;
//	out[2]=((out[2])-128)/255.0;
	out[3]=in[3];
}

static void do_ycca_to_rgba_normalized(bNode *UNUSED(node), float *out, float *in)
{
	/*un-normalize the normalize from above */
	in[0]=(in[0]+1.0f)/2.0f;
	in[1]=(in[1]+1.0f)/2.0f;
	in[2]=(in[2]+1.0f)/2.0f;

	in[0]=(in[0]*255.0f);
	in[1]=(in[1]*255.0f);
	in[2]=(in[2]*255.0f);

	//	in[0]=(in[0]*255.0)+16;
//	in[1]=(in[1]*255.0)+128;
//	in[2]=(in[2]*255.0)+128;
	ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2], BLI_YCC_ITU_BT601);
	out[3]=in[3];
}

static void do_chroma_key(bNode *node, float *out, float *in)
{
	NodeChroma *c;
	float x, z, alpha;
	float theta, beta, angle, angle2;
	float kfg;

	c=node->storage;

	/* Algorithm from book "Video Demistified," does not include the spill reduction part */

	/* find theta, the angle that the color space should be rotated based on key*/
	theta=atan2(c->key[2], c->key[1]);

	/*rotate the cb and cr into x/z space */
	x=in[1]*cosf(theta)+in[2]*sinf(theta);
	z=in[2]*cosf(theta)-in[1]*sinf(theta);

	/*if within the acceptance angle */
	angle=c->t1; /* t1 is radians. */

	/* if kfg is <0 then the pixel is outside of the key color */
	kfg= x-(fabsf(z)/tanf(angle/2.0f));

	out[0]=in[0];
	out[1]=in[1];
	out[2]=in[2];

	if(kfg>0.0f) {  /* found a pixel that is within key color */
		alpha=(1.0f-kfg)*(c->fstrength);

		beta=atan2(z,x);
		angle2=c->t2; /* t2 is radians. */

		/* if beta is within the cutoff angle */
		if(fabsf(beta) < (angle2/2.0f)) {
			alpha=0.0;
		}

		/* don't make something that was more transparent less transparent */
		if (alpha<in[3]) {
			out[3]=alpha;
		}
		else {
			out[3]=in[3];
		}
	}
	else { /*pixel is outside key color */
		out[0]=in[0];
		out[1]=in[1];
		out[2]=in[2];
		out[3]=in[3]; /* make pixel just as transparent as it was before */
	}
}

static void node_composit_exec_chroma_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf;
	CompBuf *chromabuf;
	NodeChroma *c;
	
	if(in[0]->hasinput==0) return;
	if(in[0]->data==NULL) return;
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	
	cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
	
	chromabuf= dupalloc_compbuf(cbuf);
	
	c=node->storage;
	
	/*convert rgbbuf to normalized chroma space*/
	composit1_pixel_processor(node, chromabuf, cbuf, in[0]->vec, do_rgba_to_ycca_normalized, CB_RGBA);
	/*convert key to normalized chroma color space */
	do_rgba_to_ycca_normalized(node, c->key, in[1]->vec);
	
	/*per pixel chroma key*/
	composit1_pixel_processor(node, chromabuf, chromabuf, in[0]->vec, do_chroma_key, CB_RGBA);
	
	/*convert back*/
	composit1_pixel_processor(node, chromabuf, chromabuf, in[0]->vec, do_ycca_to_rgba_normalized, CB_RGBA);
	
	out[0]->data= chromabuf;
	if(out[1]->hasoutput)
		out[1]->data= valbuf_from_rgbabuf(chromabuf, CHAN_A);
	
	generate_preview(data, node, chromabuf);

	if(cbuf!=in[0]->data)
		free_compbuf(cbuf);
}


static void node_composit_init_chroma_matte(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
	node->storage= c;
	c->t1= DEG2RADF(30.0f);
	c->t2= DEG2RADF(10.0f);
	c->t3= 0.0f;
	c->fsize= 0.0f;
	c->fstrength= 1.0f;
}

void register_node_type_cmp_chroma_matte(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_CHROMA_MATTE, "Chroma Key", NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_chroma_in, cmp_node_chroma_out);
	node_type_size(&ntype, 200, 80, 300);
	node_type_init(&ntype, node_composit_init_chroma_matte);
	node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_chroma_matte);

	nodeRegisterType(ttype, &ntype);
}
