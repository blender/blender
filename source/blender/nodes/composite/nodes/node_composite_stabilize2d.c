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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_stabilize2d.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_stabilize2d_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_stabilize2d_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void node_composit_exec_stabilize2d(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (in[0]->data && node->id) {
		RenderData *rd = data;
		MovieClip *clip = (MovieClip *)node->id;
		CompBuf *cbuf = typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf;
		float loc[2], scale, angle;

		BKE_tracking_stabilization_data(&clip->tracking, rd->cfra, cbuf->x, cbuf->y, loc, &scale, &angle);

		stackbuf = node_composit_transform(cbuf, loc[0], loc[1], angle, scale, node->custom1);

		/* pass on output and free */
		out[0]->data = stackbuf;

		if (cbuf != in[0]->data)
			free_compbuf(cbuf);
	}
}

void register_node_type_cmp_stabilize2d(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_STABILIZE2D, "Stabilize 2D", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_stabilize2d_in, cmp_node_stabilize2d_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_stabilize2d);

	nodeRegisterType(ttype, &ntype);
}
