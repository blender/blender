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

/** \file blender/nodes/composite/nodes/node_composite_mapUV.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Map UV  ******************** */

static bNodeSocketTemplate cmp_node_mapuv_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_VECTOR, 1, "UV",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_mapuv_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

/* foreach UV, use these values to read in cbuf and write to stackbuf */
/* stackbuf should be zeroed */
static void do_mapuv(CompBuf *stackbuf, CompBuf *cbuf, CompBuf *uvbuf, float threshold)
{
	ImBuf *ibuf;
	float *out= stackbuf->rect, *uv, *uvnext, *uvprev;
	float dx, dy, alpha;
	int x, y, sx, sy, row= 3*stackbuf->x;
	
	/* ibuf needed for sampling */
	ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
	ibuf->rect_float= cbuf->rect;
	
	/* vars for efficient looping */
	uv= uvbuf->rect;
	uvnext= uv+row;
	uvprev= uv-row;
	sx= stackbuf->x;
	sy= stackbuf->y;
	
	for (y=0; y<sy; y++) {
		for (x=0; x<sx; x++, out+=4, uv+=3, uvnext+=3, uvprev+=3) {
			if (x>0 && x<sx-1 && y>0 && y<sy-1) {
				if (uv[2]!=0.0f) {
					float uv_l, uv_r;
					
					/* adaptive sampling, red (U) channel */
					
					/* prevent alpha zero UVs to be used */
					uv_l= uv[-1]!=0.0f? fabsf(uv[0]-uv[-3]) : 0.0f;
					uv_r= uv[ 5]!=0.0f? fabsf(uv[0]-uv[ 3]) : 0.0f;
					
					//dx= 0.5f*(fabs(uv[0]-uv[-3]) + fabs(uv[0]-uv[3]));
					dx= 0.5f*(uv_l + uv_r);
					
					uv_l= uvprev[-1]!=0.0f? fabsf(uv[0]-uvprev[-3]) : 0.0f;
					uv_r= uvnext[-1]!=0.0f? fabsf(uv[0]-uvnext[-3]) : 0.0f;
					
					//dx+= 0.25f*(fabs(uv[0]-uvprev[-3]) + fabs(uv[0]-uvnext[-3]));
					dx+= 0.25f*(uv_l + uv_r);
						
					uv_l= uvprev[ 5]!=0.0f? fabsf(uv[0]-uvprev[+3]) : 0.0f;
					uv_r= uvnext[ 5]!=0.0f? fabsf(uv[0]-uvnext[+3]) : 0.0f;
					
					//dx+= 0.25f*(fabs(uv[0]-uvprev[+3]) + fabs(uv[0]-uvnext[+3]));
					dx+= 0.25f*(uv_l + uv_r);
					
					/* adaptive sampling, green (V) channel */
					
					uv_l= uv[-row+2]!=0.0f? fabsf(uv[1]-uv[-row+1]) : 0.0f;
					uv_r= uv[ row+2]!=0.0f? fabsf(uv[1]-uv[ row+1]) : 0.0f;
					
					//dy= 0.5f*(fabs(uv[1]-uv[-row+1]) + fabs(uv[1]-uv[row+1]));
					dy= 0.5f*(uv_l + uv_r);
					
					uv_l= uvprev[-1]!=0.0f? fabsf(uv[1]-uvprev[+1-3]) : 0.0f;
					uv_r= uvnext[-1]!=0.0f? fabsf(uv[1]-uvnext[+1-3]) : 0.0f;
					
					//dy+= 0.25f*(fabs(uv[1]-uvprev[+1-3]) + fabs(uv[1]-uvnext[+1-3]));
					dy+= 0.25f*(uv_l + uv_r);
					
					uv_l= uvprev[ 5]!=0.0f? fabsf(uv[1]-uvprev[+1+3]) : 0.0f;
					uv_r= uvnext[ 5]!=0.0f? fabsf(uv[1]-uvnext[+1+3]) : 0.0f;
					
					//dy+= 0.25f*(fabs(uv[1]-uvprev[+1+3]) + fabs(uv[1]-uvnext[+1+3]));
					dy+= 0.25f*(uv_l + uv_r);
					
					/* UV to alpha threshold */
					alpha= 1.0f - threshold*(dx+dy);
					if (alpha<0.0f) alpha= 0.0f;
					else alpha*= uv[2];
					
					/* should use mipmap */
					if (dx > 0.20f) dx= 0.20f;
					if (dy > 0.20f) dy= 0.20f;
					
					ibuf_sample(ibuf, uv[0], uv[1], dx, dy, out);
					/* premul */
					if (alpha<1.0f) {
						out[0]*= alpha;
						out[1]*= alpha;
						out[2]*= alpha;
						out[3]*= alpha;
					}
				}
			}
		}
	}

	IMB_freeImBuf(ibuf);	
}


static void node_composit_exec_mapuv(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (out[0]->hasoutput==0)
		return;
	
	if (in[0]->data && in[1]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *uvbuf= in[1]->data;
		CompBuf *stackbuf;
		
		cbuf= typecheck_compbuf(cbuf, CB_RGBA);
		uvbuf= typecheck_compbuf(uvbuf, CB_VEC3);
		stackbuf= alloc_compbuf(uvbuf->x, uvbuf->y, CB_RGBA, 1); /* allocs */;
		
		do_mapuv(stackbuf, cbuf, uvbuf, 0.05f*(float)node->custom1);
		
		out[0]->data= stackbuf;
		
		if (cbuf!=in[0]->data)
			free_compbuf(cbuf);
		if (uvbuf!=in[1]->data)
			free_compbuf(uvbuf);
	}
}

void register_node_type_cmp_mapuv(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MAP_UV, "Map UV", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_mapuv_in, cmp_node_mapuv_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_mapuv);

	nodeRegisterType(ttype, &ntype);
}
