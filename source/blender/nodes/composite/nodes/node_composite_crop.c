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
 * Contributor(s): Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_crop.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Crop  ******************** */

static bNodeSocketTemplate cmp_node_crop_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_crop_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void node_composit_exec_crop(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (in[0]->data) {
		NodeTwoXYs *ntxy= node->storage;
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf;
		int x, y;
		float *srcfp, *outfp;
		rcti outputrect;

		if (node->custom2) {
			ntxy->x1= cbuf->x* ntxy->fac_x1;
			ntxy->x2= cbuf->x* ntxy->fac_x2;
			ntxy->y1= cbuf->y* ntxy->fac_y1;
			ntxy->y2= cbuf->y* ntxy->fac_y2;
		}

		/* check input image size */
		if (cbuf->x <= ntxy->x1 + 1)
			ntxy->x1= cbuf->x - 1;

		if (cbuf->y <= ntxy->y1 + 1)
			ntxy->y1= cbuf->y - 1;

		if (cbuf->x <= ntxy->x2 + 1)
			ntxy->x2= cbuf->x - 1;

		if (cbuf->y <= ntxy->y2 + 1)
			ntxy->y2= cbuf->y - 1;

		/* figure out the minimums and maximums */
		outputrect.xmax=MAX2(ntxy->x1, ntxy->x2) + 1;
		outputrect.xmin=MIN2(ntxy->x1, ntxy->x2);
		outputrect.ymax=MAX2(ntxy->y1, ntxy->y2) + 1;
		outputrect.ymin=MIN2(ntxy->y1, ntxy->y2);

		if (node->custom1) {
			/* this option crops the image size too  */	
			stackbuf= get_cropped_compbuf(&outputrect, cbuf->rect, cbuf->x, cbuf->y, cbuf->type);
		}
		else {
			/* this option won't crop the size of the image as well  */
			/* allocate memory for the output image            */
			stackbuf = alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 1);

			/* select the cropped part of the image and set it to the output */
			for (y=outputrect.ymin; y<outputrect.ymax; y++) {
				srcfp= cbuf->rect     + (y * cbuf->x     + outputrect.xmin) * cbuf->type;
				outfp= stackbuf->rect + (y * stackbuf->x + outputrect.xmin) * stackbuf->type;
				for (x=outputrect.xmin; x<outputrect.xmax; x++, outfp+= stackbuf->type, srcfp+= cbuf->type)
							memcpy(outfp, srcfp, sizeof(float)*stackbuf->type);
			}
		}

		out[0]->data= stackbuf;
	}
}

static void node_composit_init_crop(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTwoXYs *nxy= MEM_callocN(sizeof(NodeTwoXYs), "node xy data");
	node->storage= nxy;
	nxy->x1= 0;
	nxy->x2= 0;
	nxy->y1= 0;
	nxy->y2= 0;
}

void register_node_type_cmp_crop(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_CROP, "Crop", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_crop_in, cmp_node_crop_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_crop);
	node_type_storage(&ntype, "NodeTwoXYs", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_crop);

	nodeRegisterType(ttype, &ntype);
}
