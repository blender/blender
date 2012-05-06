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

/** \file blender/nodes/composite/nodes/node_composite_moviedistortion.c
 *  \ingroup cmpnodes
 */

#include "BLF_translation.h"


#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_moviedistortion_in[] = {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_moviedistortion_out[] = {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (in[0]->data) {
		if (node->id) {
			MovieClip *clip = (MovieClip *)node->id;
			CompBuf *cbuf = typecheck_compbuf(in[0]->data, CB_RGBA);
			CompBuf *stackbuf = alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 0);
			ImBuf *ibuf;

			ibuf = IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);

			if (ibuf) {
				RenderData *rd = data;
				ImBuf *obuf;
				MovieTracking *tracking = &clip->tracking;
				int width, height;
				float overscan = 0.0f;
				MovieClipUser user = {0};

				BKE_movieclip_user_set_frame(&user, rd->cfra);

				ibuf->rect_float = cbuf->rect;

				BKE_movieclip_get_size(clip, &user, &width, &height);

				if (!node->storage)
					node->storage = BKE_tracking_distortion_create();

				if (node->custom1 == 0)
					obuf= BKE_tracking_distortion_exec(node->storage, tracking, ibuf, width, height, overscan, 1);
				else
					obuf= BKE_tracking_distortion_exec(node->storage, tracking, ibuf, width, height, overscan, 0);

				stackbuf->rect = obuf->rect_float;
				stackbuf->malloc = TRUE;

				obuf->mall &= ~IB_rectfloat;
				obuf->rect_float = NULL;

				IMB_freeImBuf(ibuf);
				IMB_freeImBuf(obuf);
			}

			/* pass on output and free */
			out[0]->data = stackbuf;

			if (cbuf != in[0]->data)
				free_compbuf(cbuf);
		}
		else {
			CompBuf *cbuf = in[0]->data;
			CompBuf *stackbuf = pass_on_compbuf(cbuf);

			out[0]->data = stackbuf;
		}
	}
}

static const char *label(bNode *node)
{
	if (node->custom1 == 0)
		return IFACE_("Undistortion");
	else
		return IFACE_("Distortion");
}

static void storage_free(bNode *node)
{
	if (node->storage)
		BKE_tracking_distortion_destroy(node->storage);

	node->storage= NULL;
}

static void storage_copy(bNode *orig_node, bNode *new_node)
{
	if (orig_node->storage)
		new_node->storage = BKE_tracking_distortion_copy(orig_node->storage);
}

void register_node_type_cmp_moviedistortion(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_MOVIEDISTORTION, "Movie Distortion", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_moviedistortion_in, cmp_node_moviedistortion_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_label(&ntype, label);
	node_type_exec(&ntype, exec);
	node_type_storage(&ntype, NULL, storage_free, storage_copy);

	nodeRegisterType(ttype, &ntype);
}
