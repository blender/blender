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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "../CMP_util.h"


/* ******************* Channel Matte Node ********************************* */
static bNodeSocketType cmp_node_channel_matte_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_channel_matte_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_normalized_rgba_to_ycca2(bNode *node, float *out, float *in)
{
	/*normalize to the range 0.0 to 1.0) */
	rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[0]=(out[0])/255.0;
	out[1]=(out[1])/255.0;
	out[2]=(out[2])/255.0;
	out[3]=in[3];
}

static void do_normalized_ycca_to_rgba2(bNode *node, float *out, float *in)
{
	/*un-normalize the normalize from above */
	in[0]=in[0]*255.0;
	in[1]=in[1]*255.0;
	in[2]=in[2]*255.0;
	ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[3]=in[3];
}


static void do_channel_matte(bNode *node, float *out, float *in)
{
	NodeChroma *c=(NodeChroma *)node->storage;
	float alpha=0.0;
	
	/* Alpha=G-MAX(R, B) */
	
	switch(node->custom2)
	{
	case 1:
		{
			alpha=in[0]-MAX2(in[1],in[2]);
			break;
		}
	case 2:
		{
			alpha=in[1]-MAX2(in[0],in[2]);
			break;
		}
	case 3:
		{
			alpha=in[2]-MAX2(in[0],in[1]);
			break;
		}
	default:
		break;
	}
	
	/*flip because 0.0 is transparent, not 1.0*/
	alpha=1-alpha;
	
	/* test range*/
	if(alpha>c->t1) {
		alpha=in[3]; /*whatever it was prior */
	}
	else if(alpha<c->t2){
		alpha=0.0;
	}
	else {/*blend */
		alpha=(alpha-c->t2)/(c->t1-c->t2);
	}

	
	/* don't make something that was more transparent less transparent */
	if (alpha<in[3]) {
		out[3]=alpha;
	}
	else {
		out[3]=in[3];
	}

}

static void node_composit_exec_channel_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf;
	CompBuf *outbuf;
	
	if(in[0]->hasinput==0)  return;
	if(in[0]->data==NULL) return;
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	
	cbuf=typecheck_compbuf(in[0]->data, CB_RGBA);
	
	outbuf=dupalloc_compbuf(cbuf);
	
	/*convert to colorspace*/
	switch(node->custom1) {
	case 1: /*RGB */
		break;
	case 2: /*HSV*/
		composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_rgba_to_hsva, CB_RGBA);
		break;
	case 3: /*YUV*/
		composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_rgba_to_yuva, CB_RGBA);
		break;
	case 4: /*YCC*/
		composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_normalized_rgba_to_ycca2, CB_RGBA);
		break;
	default:
		break;
	}

	/*use the selected channel information to do the key */
	composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_channel_matte, CB_RGBA);

	/*convert back to RGB colorspace in place*/
	switch(node->custom1) {
	case 1: /*RGB*/
		break;
	case 2: /*HSV*/
		composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_hsva_to_rgba, CB_RGBA);
		break;
	case 3: /*YUV*/
		composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_yuva_to_rgba, CB_RGBA);
		break;
	case 4: /*YCC*/
		composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_normalized_ycca_to_rgba2, CB_RGBA);
		break;
	default:
		break;
	}

	generate_preview(node, outbuf);
	out[0]->data=outbuf;
	if(out[1]->hasoutput)
		out[1]->data=valbuf_from_rgbabuf(outbuf, CHAN_A);
	
	if(cbuf!=in[0]->data)
		free_compbuf(cbuf);

}

static void node_composit_init_channel_matte(bNode *node)
{
   NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
   node->storage=c;
   c->t1= 1.0f;
   c->t2= 0.0f;
   c->t3= 0.0f;
   c->fsize= 0.0f;
   c->fstrength= 0.0f;
   node->custom1= 1; /* RGB channel */
   node->custom2= 2; /* Green Channel */
}

bNodeType cmp_node_channel_matte={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_CHANNEL_MATTE,
	/* name        */	"Channel Key",
	/* width+range */	200, 80, 250,
	/* class+opts  */	NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_channel_matte_in,
	/* output sock */	cmp_node_channel_matte_out,
	/* storage     */	"NodeChroma",
	/* execfunc    */	node_composit_exec_channel_matte,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_channel_matte,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};
