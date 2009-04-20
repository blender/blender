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


/* **************** ID Mask  ******************** */

static bNodeSocketType cmp_node_idmask_in[]= {
	{	SOCK_VALUE, 1, "ID value",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_idmask_out[]= {
	{	SOCK_VALUE, 0, "Alpha",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* stackbuf should be zeroed */
static void do_idmask(CompBuf *stackbuf, CompBuf *cbuf, float idnr)
{
	float *rect;
	int x;
	char *abuf= MEM_mapallocN(cbuf->x*cbuf->y, "anti ali buf");
	
	rect= cbuf->rect;
	for(x= cbuf->x*cbuf->y - 1; x>=0; x--)
		if(rect[x]==idnr)
			abuf[x]= 255;
	
	antialias_tagbuf(cbuf->x, cbuf->y, abuf);
	
	rect= stackbuf->rect;
	for(x= cbuf->x*cbuf->y - 1; x>=0; x--)
		if(abuf[x]>1)
			rect[x]= (1.0f/255.0f)*(float)abuf[x];
	
	MEM_freeN(abuf);
}

/* full sample version */
static void do_idmask_fsa(CompBuf *stackbuf, CompBuf *cbuf, float idnr)
{
	float *rect, *rs;
	int x;
	
	rect= cbuf->rect;
	rs= stackbuf->rect;
	for(x= cbuf->x*cbuf->y - 1; x>=0; x--)
		if(rect[x]==idnr)
			rs[x]= 1.0f;
	
}


static void node_composit_exec_idmask(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	RenderData *rd= data;
	
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf;
		
		if(cbuf->type!=CB_VAL)
			return;
		
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */;
		
		if(rd->scemode & R_FULL_SAMPLE)
			do_idmask_fsa(stackbuf, cbuf, (float)node->custom1);
		else
			do_idmask(stackbuf, cbuf, (float)node->custom1);
		
		out[0]->data= stackbuf;
	}
}


bNodeType cmp_node_idmask= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_ID_MASK,
	/* name        */	"ID Mask",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_idmask_in,
	/* output sock */	cmp_node_idmask_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_idmask,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};


