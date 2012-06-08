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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_mask.c
 *  \ingroup cmpnodes
 */

#include "BLF_translation.h"

#include "DNA_mask_types.h"

#include "BKE_mask.h"

#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_mask_in[] = {
	{   SOCK_RGBA, 1, "Image",          0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{   -1, 0, ""   }
};

static bNodeSocketTemplate cmp_node_mask_out[] = {
	{   SOCK_RGBA, 0, "Image"},
	{   -1, 0, ""   }
};

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (node->id) {
		Mask *mask = (Mask *)node->id;
		CompBuf *stackbuf;
		RenderData *rd = data;
		float *res;
		int sx, sy;

		if (!out[0]->hasoutput) {
			/* the node's output socket is not connected to anything...
			 * do not execute any further, just exit the node immediately
			 */
			return;
		}

		if (in[0]->hasinput && in[0]->data) {
			CompBuf *cbuf = typecheck_compbuf(in[0]->data, CB_RGBA);

			sx = cbuf->x;
			sy = cbuf->y;
		}
		else {
			sx = (rd->size * rd->xsch) / 100;
			sy = (rd->size * rd->ysch) / 100;
		}

		/* allocate the output buffer */
		stackbuf = alloc_compbuf(sx, sy, CB_VAL, TRUE);
		res = stackbuf->rect;

		BKE_mask_rasterize(mask, sx, sy, res, TRUE, TRUE);

		/* pass on output and free */
		out[0]->data = stackbuf;
	}
}

void register_node_type_cmp_mask(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MASK, "Mask", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_mask_in, cmp_node_mask_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, exec);

	nodeRegisterType(ttype, &ntype);
}
