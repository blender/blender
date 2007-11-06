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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"

/* ******************* channel Difference Matte ********************************* */
static bNodeSocketType cmp_node_diff_matte_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_RGBA,1,"Key Color", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_diff_matte_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

/* note, keyvals is passed on from caller as stack array */
/* might have been nicer as temp struct though... */
static void do_diff_matte(bNode *node, float *colorbuf, float *inbuf, float *keyvals)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float *keymin= keyvals;
	float *keymax= keyvals+3;
	float *key=    keyvals+6;
	float tolerance= keyvals[9];
	float distance, alpha;
	
	/*process the pixel if it is close to the key or already transparent*/
	if(((colorbuf[0]>keymin[0] && colorbuf[0]<keymax[0]) &&
		(colorbuf[1]>keymin[1] && colorbuf[1]<keymax[1]) &&
		(colorbuf[2]>keymin[2] && colorbuf[2]<keymax[2])) || inbuf[3]<1.0f) {
		
		/*true distance from key*/
		distance= sqrt((colorbuf[0]-key[0])*(colorbuf[0]-key[0])+
					  (colorbuf[1]-key[1])*(colorbuf[1]-key[1])+
					  (colorbuf[2]-key[2])*(colorbuf[2]-key[2]));
		
		/*is it less transparent than the prevous pixel*/
		alpha= distance/tolerance;
		if(alpha > inbuf[3]) alpha= inbuf[3];
		if(alpha > c->fstrength) alpha= 0.0f;
		
		/*clamp*/
		if (alpha>1.0f) alpha=1.0f;
		if (alpha<0.0f) alpha=0.0f;
		
		/*premultiplied picture*/
		colorbuf[3]= alpha;
	}
	else {
		/*foreground object*/
		colorbuf[3]= inbuf[3];
	}
}

static void node_composit_exec_diff_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/*
	Losely based on the Sequencer chroma key plug-in, but enhanced to work in other color spaces and
	uses a differnt difference function (suggested in forums of vfxtalk.com).
	*/
	CompBuf *workbuf;
	CompBuf *inbuf;
	NodeChroma *c;
	float keyvals[10];
	float *keymin= keyvals;
	float *keymax= keyvals+3;
	float *key=    keyvals+6;
	float *tolerance= keyvals+9;
	float t[3];
	
	/*is anything connected?*/
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	/*must have an image imput*/
	if(in[0]->data==NULL) return;
	
	inbuf=typecheck_compbuf(in[0]->data, CB_RGBA);
	
	c=node->storage;
	workbuf=dupalloc_compbuf(inbuf);
	
	/*use the input color*/
	key[0]= in[1]->vec[0];
	key[1]= in[1]->vec[1];
	key[2]= in[1]->vec[2];
	
	/*get the tolerances from the UI*/
	t[0]=c->t1;
	t[1]=c->t2;
	t[2]=c->t3;
	
	/*convert to colorspace*/
	switch(node->custom1) {
		case 1: /*RGB*/
				break;
		case 2: /*HSV*/
		/*convert the key (in place)*/
			rgb_to_hsv(key[0], key[1], key[2], &key[0], &key[1], &key[2]);
			composit1_pixel_processor(node, workbuf, inbuf, in[1]->vec, do_rgba_to_hsva, CB_RGBA);
			break;
		case 3: /*YUV*/
			rgb_to_yuv(key[0], key[1], key[2], &key[0], &key[1], &key[2]);
			composit1_pixel_processor(node, workbuf, inbuf, in[1]->vec, do_rgba_to_yuva, CB_RGBA);
			break;
		case 4: /*YCC*/
			rgb_to_ycc(key[0], key[1], key[2], &key[0], &key[1], &key[2]);
			composit1_pixel_processor(node, workbuf, inbuf, in[1]->vec, do_rgba_to_ycca, CB_RGBA);
			/*account for ycc is on a 0..255 scale*/
			t[0]= c->t1*255.0;
			t[1]= c->t2*255.0;
			t[2]= c->t3*255.0;
			break;
		default:
				break;
	}
	
	/*find min/max tolerances*/
	keymin[0]= key[0]-t[0];
	keymin[1]= key[1]-t[1];
	keymin[2]= key[2]-t[2];
	keymax[0]= key[0]+t[0];
	keymax[1]= key[1]+t[1];
	keymax[2]= key[2]+t[2];
	
	/*tolerance*/
	*tolerance= sqrt((t[0])*(t[0])+
					(t[1])*(t[1])+
					(t[2])*(t[2]));
	
	/* note, processor gets a keyvals array passed on as buffer constant */
	composit2_pixel_processor(node, workbuf, workbuf, in[0]->vec, NULL, keyvals, do_diff_matte, CB_RGBA, CB_VAL);
	
	/*convert back to RGB colorspace*/
	switch(node->custom1) {
		case 1: /*RGB*/
			composit1_pixel_processor(node, workbuf, workbuf, in[1]->vec, do_copy_rgba, CB_RGBA);
			break;
		case 2: /*HSV*/
			composit1_pixel_processor(node, workbuf, workbuf, in[1]->vec, do_hsva_to_rgba, CB_RGBA);
			break;
		case 3: /*YUV*/
			composit1_pixel_processor(node, workbuf, workbuf, in[1]->vec, do_yuva_to_rgba, CB_RGBA);
			break;
		case 4: /*YCC*/
			composit1_pixel_processor(node, workbuf, workbuf, in[1]->vec, do_ycca_to_rgba, CB_RGBA);
			break;
		default:
			break;
	}
	
	out[0]->data=workbuf;
	if(out[1]->hasoutput)
		out[1]->data=valbuf_from_rgbabuf(workbuf, CHAN_A);
	generate_preview(node, workbuf);

	if(inbuf!=in[0]->data)
		free_compbuf(inbuf);
}

static void node_composit_init_diff_matte(bNode *node)
{
   NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
   node->storage= c;
   c->t1= 0.01f;
   c->t2= 0.01f;
   c->t3= 0.01f;
   c->fsize= 0.0f;
   c->fstrength= 0.0f;
   node->custom1= 1; /* RGB */
}

bNodeType cmp_node_diff_matte={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_DIFF_MATTE,
	/* name        */	"Difference Key",
	/* width+range */	200, 80, 250,
	/* class+opts  */	NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_diff_matte_in,
	/* output sock */	cmp_node_diff_matte_out,
	/* storage     */	"NodeChroma",
	/* execfunc    */	node_composit_exec_diff_matte,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_diff_matte,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};


