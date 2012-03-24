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

/** \file blender/nodes/composite/nodes/node_composite_flip.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Flip  ******************** */
static bNodeSocketTemplate cmp_node_flip_in[]= {
	{	SOCK_RGBA, 1, "Image",		    1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_flip_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void node_composit_exec_flip(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (in[0]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 1);	/* note, this returns zero'd image */
		int i, src_pix, src_width, src_height, srcydelt, outydelt, x, y;
		float *srcfp, *outfp;
		
		src_pix= cbuf->type;
		src_width= cbuf->x;
		src_height= cbuf->y;
		srcfp= cbuf->rect;
		outfp= stackbuf->rect;
		srcydelt= src_width*src_pix;
		outydelt= srcydelt;
		
		if (node->custom1) {		/*set up output pointer for y flip*/
			outfp+= (src_height-1)*outydelt;
			outydelt= -outydelt;
		}

		for (y=0; y<src_height; y++) {
			if (node->custom1 == 1) {	/* no x flip so just copy line*/
				memcpy(outfp, srcfp, sizeof(float) * src_pix * src_width);
				srcfp+=srcydelt;
			}
			else {
				outfp += (src_width-1)*src_pix;
				for (x=0; x<src_width; x++) {
					for (i=0; i<src_pix; i++) {
						outfp[i]= srcfp[i];
					}
					outfp -= src_pix;
					srcfp += src_pix;
				}
				outfp += src_pix;
			}
			outfp += outydelt;
		}

		out[0]->data= stackbuf;

	}
}

void register_node_type_cmp_flip(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_FLIP, "Flip", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_flip_in, cmp_node_flip_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_flip);

	nodeRegisterType(ttype, &ntype);
}
