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

/** \file blender/nodes/composite/nodes/node_composite_moviedistort.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_moviedistort_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_moviedistort_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void node_composit_exec_moviedistort(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(in[0]->data && node->id) {
		MovieClip *clip= (MovieClip *)node->id;
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 0);
		ImBuf *ibuf;

		ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);

		if(ibuf) {
			ImBuf *obuf;
			MovieClipUser *user= (MovieClipUser *)node->storage;
			float aspy= 1.f/clip->tracking.camera.pixel_aspect;
			int scaled= 0, width, height;

			ibuf->rect_float= cbuf->rect;

			BKE_movieclip_acquire_size(clip, user, &width, &height);
			height*= aspy;

			if(ibuf->x!=width || ibuf->y!=height) {
				/* TODO: not sure this is really needed, but distortion coefficients are
				         calculated using camera resolution, so if image with other resolution
				         is passed to distortion node, it'll be distorted correct (but quality can hurt)
				         This is also needed to deal when camera pixel aspect isn't 1. Problems in this case
				         are caused because of how aspect x/y are calculating. Currently. projeciton
				         matrices and reconstruction stuff are supposing that aspect x is always 1 and
				         aspect y is less than 1 (if x resolution is larger than y resolution) */

				IMB_scaleImBuf(ibuf, width, height);
				scaled= 1;
			}

			obuf= BKE_tracking_distort(&clip->tracking, ibuf);

			if(scaled)
				IMB_scaleImBuf(obuf, cbuf->x, cbuf->y);

			stackbuf->rect= obuf->rect_float;
			stackbuf->malloc= 1;

			obuf->mall&= ~IB_rectfloat;
			obuf->rect_float= NULL;

			IMB_freeImBuf(ibuf);
			IMB_freeImBuf(obuf);
		}

		/* pass on output and free */
		out[0]->data= stackbuf;

		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
	}
}

void register_node_type_cmp_moviedistort(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, CMP_NODE_MOVIEDISTORT, "Movie Distort", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_moviedistort_in, cmp_node_moviedistort_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_moviedistort);

	nodeRegisterType(lb, &ntype);
}
