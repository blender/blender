/*
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
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/intern/CMP_nodes/CMP_translate.c
 *  \ingroup cmpnodes
 */


#include "../CMP_util.h"

/* **************** Translate  ******************** */

static bNodeSocketType cmp_node_stabilize2d_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_stabilize2d_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_stabilize2d(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(in[0]->data && node->id) {
		RenderData *rd= data;
		MovieClip *clip= (MovieClip *)node->id;
		MovieTrackingStabilization *stab= &clip->tracking.stabilization;
		CompBuf *cbuf= cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf;
		float mat[4][4];
	
		BKE_tracking_stabilization_matrix(&clip->tracking, rd->cfra, cbuf->x, cbuf->y, mat);

		if(stab->scale) {
			ImBuf *scaleibuf, *ibuf;

			scaleibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, IB_rectfloat);
			memcpy(scaleibuf->rect_float, cbuf->rect, sizeof(float)*4*scaleibuf->x*scaleibuf->y);

			IMB_scaleImBuf(scaleibuf, scaleibuf->x*stab->scale, scaleibuf->y*stab->scale);

			ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, IB_rectfloat);
			IMB_rectcpy(ibuf, scaleibuf, mat[3][0], mat[3][1], 0, 0, scaleibuf->x, scaleibuf->y);

			stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 0);
			stackbuf->rect= ibuf->rect_float;
			stackbuf->malloc= 1;

			ibuf->rect_float= NULL;
			ibuf->mall &= ~IB_rectfloat;

			IMB_freeImBuf(ibuf);
			IMB_freeImBuf(scaleibuf);
		} else {
			stackbuf= pass_on_compbuf(in[0]->data);

			stackbuf->xof+= mat[3][0];
			stackbuf->yof+= mat[3][1];
		}

		out[0]->data= stackbuf;
	}
}

void register_node_type_cmp_stabilize2d(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, CMP_NODE_STABILIZE2D, "Stabilize 2D", NODE_CLASS_DISTORT, NODE_OPTIONS,
		cmp_node_stabilize2d_in, cmp_node_stabilize2d_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_stabilize2d);

	nodeRegisterType(lb, &ntype);
}


