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
 * Contributor(s): Bob Holcomb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"

/* ******************* channel Difference Matte ********************************* */
static bNodeSocketType cmp_node_diff_matte_in[]={
	{SOCK_RGBA,1,"Image 1", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_RGBA,1,"Image 2", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_diff_matte_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

/* note, keyvals is passed on from caller as stack array */
/* might have been nicer as temp struct though... */
static void do_diff_matte(bNode *node, float *colorbuf, float *imbuf1, float *imbuf2)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float tolerence=c->t1;
   float falloff=c->t2;
	float difference;
   float alpha;
	
   difference=fabs(imbuf2[0]-imbuf1[0])+
               fabs(imbuf2[1]-imbuf1[1])+
               fabs(imbuf2[2]-imbuf1[2]);

   /*average together the distances*/
   difference=difference/3.0;

   VECCOPY(colorbuf, imbuf1);

   /*make 100% transparent*/
	if(difference < tolerence){
      colorbuf[3]=0.0;
	}
   /*in the falloff region, make partially transparent */
   else if(difference < falloff+tolerence){
      difference=difference-tolerence;
      alpha=difference/falloff;
      /*only change if more transparent than before */
      if(alpha < imbuf1[3]) {      
         colorbuf[3]=alpha;
      }
      else { /* leave as before */
         colorbuf[3]=imbuf1[3];
      }
   }
	else {
		/*foreground object*/
		colorbuf[3]= imbuf1[3];
	}
}

static void node_composit_exec_diff_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *outbuf;
	CompBuf *imbuf1;
   CompBuf *imbuf2;
	NodeChroma *c;
	
	/*is anything connected?*/
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	/*must have an image imput*/
	if(in[0]->data==NULL) return;
   if(in[1]->data==NULL) return;
	
	imbuf1=typecheck_compbuf(in[0]->data, CB_RGBA);
   imbuf2=typecheck_compbuf(in[1]->data, CB_RGBA);
	
	c=node->storage;
	outbuf=dupalloc_compbuf(imbuf1);
	
	/* note, processor gets a keyvals array passed on as buffer constant */
	composit2_pixel_processor(node, outbuf, imbuf1, in[0]->vec, imbuf2, in[1]->vec, do_diff_matte, CB_RGBA, CB_RGBA);
	
	out[0]->data=outbuf;
	if(out[1]->hasoutput)
		out[1]->data=valbuf_from_rgbabuf(outbuf, CHAN_A);
	generate_preview(data, node, outbuf);

	if(imbuf1!=in[0]->data)
		free_compbuf(imbuf1);

	if(imbuf2!=in[1]->data)
		free_compbuf(imbuf2);
}

static void node_composit_init_diff_matte(bNode *node)
{
   NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
   node->storage= c;
   c->t1= 0.1f;
   c->t2= 0.1f;
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


