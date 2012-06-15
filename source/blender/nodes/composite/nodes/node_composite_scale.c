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

/** \file blender/nodes/composite/nodes/node_composite_scale.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Scale  ******************** */

static bNodeSocketTemplate cmp_node_scale_in[] = {
	{   SOCK_RGBA, 1, N_("Image"),          1.0f, 1.0f, 1.0f, 1.0f},
	{   SOCK_FLOAT, 1, N_("X"),             1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX, PROP_FACTOR},
	{   SOCK_FLOAT, 1, N_("Y"),             1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX, PROP_FACTOR},
	{   -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_scale_out[] = {
	{   SOCK_RGBA, 0, N_("Image")},
	{   -1, 0, ""   }
};

/* only supports RGBA nodes now */
/* node->custom1 stores if input values are absolute or relative scale */
static void node_composit_exec_scale(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (out[0]->hasoutput == 0)
		return;

	if (in[0]->data) {
		RenderData *rd = data;
		CompBuf *stackbuf, *cbuf = typecheck_compbuf(in[0]->data, CB_RGBA);
		ImBuf *ibuf;
		int newx, newy;
		float ofsx = 0.0f, ofsy = 0.0f;

		if (node->custom1 == CMP_SCALE_RELATIVE) {
			newx = MAX2((int)(in[1]->vec[0] * cbuf->x), 1);
			newy = MAX2((int)(in[2]->vec[0] * cbuf->y), 1);
		}
		else if (node->custom1 == CMP_SCALE_SCENEPERCENT) {
			newx = cbuf->x * (rd->size / 100.0f);
			newy = cbuf->y * (rd->size / 100.0f);
		}
		else if (node->custom1 == CMP_SCALE_RENDERPERCENT) {

			if (node->custom3 != 0.0f || node->custom4 != 0.0f) {
				const float w_dst = (rd->xsch * rd->size) / 100;
				const float h_dst = (rd->ysch * rd->size) / 100;

				if (w_dst > h_dst) {
					ofsx = node->custom3 * w_dst;
					ofsy = node->custom4 * w_dst;
				}
				else {
					ofsx = node->custom3 * h_dst;
					ofsy = node->custom4 * h_dst;
				}
			}

			/* supports framing options */
			if (node->custom2 & CMP_SCALE_RENDERSIZE_FRAME_ASPECT) {
				/* apply aspect from clip */
				const float w_src = cbuf->x;
				const float h_src = cbuf->y;

				/* destination aspect is already applied from the camera frame */
				const float w_dst = (rd->xsch * rd->size) / 100;
				const float h_dst = (rd->ysch * rd->size) / 100;

				const float asp_src = w_src / h_src;
				const float asp_dst = w_dst / h_dst;

				if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
					if ((asp_src > asp_dst) == ((node->custom2 & CMP_SCALE_RENDERSIZE_FRAME_CROP) != 0)) {
						/* fit X */
						const float div = asp_src / asp_dst;
						newx = w_dst * div;
						newy = h_dst;
					}
					else {
						/* fit Y */
						const float div = asp_dst / asp_src;
						newx = w_dst;
						newy = h_dst * div;
					}
				}
				else {
					/* same as below - no aspect correction needed  */
					newx = w_dst;
					newy = h_dst;
				}
			}
			else {
				/* stretch */
				newx = (rd->xsch * rd->size) / 100;
				newy = (rd->ysch * rd->size) / 100;
			}
		}
		else {  /* CMP_SCALE_ABSOLUTE */
			newx = MAX2((int)in[1]->vec[0], 1);
			newy = MAX2((int)in[2]->vec[0], 1);
		}
		newx = MIN2(newx, CMP_SCALE_MAX);
		newy = MIN2(newy, CMP_SCALE_MAX);

		ibuf = IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
		if (ibuf) {
			ibuf->rect_float = cbuf->rect;
			IMB_scaleImBuf(ibuf, newx, newy);

			if (ibuf->rect_float == cbuf->rect) {
				/* no scaling happened. */
				stackbuf = pass_on_compbuf(in[0]->data);
			}
			else {
				stackbuf = alloc_compbuf(newx, newy, CB_RGBA, 0);
				stackbuf->rect = ibuf->rect_float;
				stackbuf->malloc = 1;
			}

			ibuf->rect_float = NULL;
			ibuf->mall &= ~IB_rectfloat;
			IMB_freeImBuf(ibuf);

			/* also do the translation vector */
			stackbuf->xof = (int)(ofsx + (((float)newx / (float)cbuf->x) * (float)cbuf->xof));
			stackbuf->yof = (int)(ofsy + (((float)newy / (float)cbuf->y) * (float)cbuf->yof));
		}
		else {
			stackbuf = dupalloc_compbuf(cbuf);
			printf("Scaling to %dx%d failed\n", newx, newy);
		}

		out[0]->data = stackbuf;
		if (cbuf != in[0]->data)
			free_compbuf(cbuf);
	}
	else if (node->custom1 == CMP_SCALE_ABSOLUTE) {
		CompBuf *stackbuf;
		int a, x, y;
		float *fp;

		x = MAX2((int)in[1]->vec[0], 1);
		y = MAX2((int)in[2]->vec[0], 1);

		stackbuf = alloc_compbuf(x, y, CB_RGBA, 1);
		fp = stackbuf->rect;

		a = stackbuf->x * stackbuf->y;
		while (a--) {
			copy_v4_v4(fp, in[0]->vec);
			fp += 4;
		}

		out[0]->data = stackbuf;
	}
}

void register_node_type_cmp_scale(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_SCALE, "Scale", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_scale_in, cmp_node_scale_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_scale);

	nodeRegisterType(ttype, &ntype);
}
