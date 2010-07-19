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

#include "../CMP_util.h"

/* ******************* Chroma Key ********************************************************** */
static bNodeSocketType cmp_node_chroma_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_RGBA,1,"Key Color", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_chroma_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_rgba_to_ycca_normalized(bNode *node, float *out, float *in)
{
	/*normalize to the range -1.0 to 1.0) */
	rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2], BLI_YCC_ITU_BT601);
	out[0]=((out[0])-16)/255.0;
	out[1]=((out[1])-128)/255.0;
	out[2]=((out[2])-128)/255.0;
	out[3]=in[3];
}

static void do_ycca_to_rgba_normalized(bNode *node, float *out, float *in)
{
	/*un-normalize the normalize from above */
	in[0]=(in[0]*255.0)+16;
	in[1]=(in[1]*255.0)+128;
	in[2]=(in[2]*255.0)+128;
	ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2], BLI_YCC_ITU_BT601);
	out[3]=in[3];
}

static void do_chroma_key(bNode *node, float *out, float *in)
{
	NodeChroma *c;
	float x, z, alpha;
	float theta, beta, angle;
	float kfg, newY, newCb, newCr;

	c=node->storage;

	/* Algorithm from book "Video Demistified" */

	/* find theta, the angle that the color space should be rotated based on key*/
	theta=atan2(c->key[2],c->key[1]);

	/*rotate the cb and cr into x/z space */
	x=in[1]*cos(theta)+in[2]*sin(theta);
	z=in[2]*cos(theta)-in[1]*sin(theta);

	/*if within the acceptance angle */
	angle=c->t1*M_PI/180.0; /* convert to radians */
	
	/* if kfg is <0 then the pixel is outside of the key color */
	kfg=x-(fabs(z)/tan(angle/2.0));

	if(kfg>0.0) {  /* found a pixel that is within key color */

		newY=in[0]-(1-c->t3)*kfg;
		newCb=in[1]-kfg*cos((double)theta);
		newCr=in[2]-kfg*sin((double)theta);
		alpha=(kfg+c->fsize)*(c->fstrength);

		beta=atan2(newCr,newCb);
		beta=beta*180.0/M_PI; /* convert to degrees for compare*/

		/* if beta is within the clippin angle */
		if(fabs(beta)<(c->t2/2.0)) {
			newCb=0.0;
			newCr=0.0;
			alpha=0.0;
		}

		out[0]=newY;
		out[1]=newCb;
		out[2]=newCr;

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
};


static void node_composit_init_chroma_matte(bNode *node)
{
   NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
   node->storage= c;
   c->t1= 30.0f;
   c->t2= 10.0f;
   c->t3= 0.0f;
   c->fsize= 0.0f;
   c->fstrength= 1.0f;
};

bNodeType cmp_node_chroma_matte={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_CHROMA_MATTE,
	/* name        */	"Chroma Key",
	/* width+range */	200, 80, 300,
	/* class+opts  */	NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_chroma_in,
	/* output sock */	cmp_node_chroma_out,
	/* storage     */	"NodeChroma",
	/* execfunc    */	node_composit_exec_chroma_matte,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_chroma_matte,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};


