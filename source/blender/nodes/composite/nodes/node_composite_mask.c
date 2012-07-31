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

static bNodeSocketTemplate cmp_node_mask_out[] = {
	{   SOCK_FLOAT, 0, "Mask"},
	{   -1, 0, ""   }
};

static void exec(void *data, bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	if (node->id) {
		Mask *mask = (Mask *)node->id;
		MaskRasterHandle *mr_handle;
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

		sx = (rd->size * rd->xsch) / 100;
		sy = (rd->size * rd->ysch) / 100;

		/* allocate the output buffer */
		stackbuf = alloc_compbuf(sx, sy, CB_VAL, TRUE);
		res = stackbuf->rect;

		/* mask raster begin */
		mr_handle = BKE_maskrasterize_handle_new();
		BKE_maskrasterize_handle_init(mr_handle, mask,
		                              sx, sy,
		                              TRUE,
		                              (node->custom1 & CMP_NODEFLAG_MASK_AA) != 0,
		                              (node->custom1 & CMP_NODEFLAG_MASK_NO_FEATHER) == 0);
		BKE_maskrasterize_buffer(mr_handle, sx, sy, res);
		BKE_maskrasterize_handle_free(mr_handle);
		/* mask raster end */

		/* pass on output and free */
		out[0]->data = stackbuf;
	}
}

static void node_composit_init_mask(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeMask *data = MEM_callocN(sizeof(NodeMask), STRINGIFY(NodeMask));
	data->size_x = data->size_y = 256;
	node->storage = data;

	node->custom2 = 16;    /* samples */
	node->custom3 = 0.5f;  /* shutter */
}

void register_node_type_cmp_mask(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MASK, "Mask", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, cmp_node_mask_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_mask);
	node_type_exec(&ntype, exec);

	node_type_storage(&ntype, "NodeMask", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(ttype, &ntype);
}
