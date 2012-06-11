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

/** \file blender/nodes/composite/nodes/node_composite_keying.c
 *  \ingroup cmpnodes
 */

#include "BLF_translation.h"

#include "DNA_movieclip_types.h"

#include "BKE_movieclip.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_voronoi.h"

#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_keying_in[] = {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Key Color", 1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_keying_out[] = {
	{	SOCK_RGBA,  0, "Image"},
	{	SOCK_FLOAT, 0, "Matte"},
	{	SOCK_FLOAT, 0, "Edges"},
	{	-1, 0, ""	}
};

static int get_pixel_primary_channel(float *pixel)
{
	float max_value = MAX3(pixel[0], pixel[1], pixel[2]);

	if (max_value == pixel[0])
		return 0;
	else if (max_value == pixel[1])
		return 1;

	return 2;
}

static float get_pixel_saturation(float *pixel, float screen_balance)
{
	float min = MIN3(pixel[0], pixel[1], pixel[2]);
	float max = MAX3(pixel[0], pixel[1], pixel[2]);
	float mid = pixel[0] + pixel[1] + pixel[2] - min - max;
	float val = (1.0f - screen_balance) * min + screen_balance * mid;

	return max - val;
}

static void despil_pixel(float *out, float *pixel, float *screen, float screen_gain)
{
	int screen_primary_channel = get_pixel_primary_channel(screen);
	float average_value, amount;

	average_value = (pixel[0] + pixel[1] + pixel[2] - pixel[screen_primary_channel]) / 2.0f;
	amount = pixel[screen_primary_channel] - average_value;

	if (screen_gain * amount > 0) {
		out[screen_primary_channel] = pixel[screen_primary_channel] - screen_gain * amount;
	}
}

static void do_key(bNode *node, float *out, float *pixel, float *screen)
{
	NodeKeyingData *data = node->storage;

	float screen_balance = 0.5f;
	float despill_factor = data->despill_factor;
	float clip_black = data->clip_black;
	float clip_white = data->clip_white;

	float saturation = get_pixel_saturation(pixel, screen_balance);
	float screen_saturation = get_pixel_saturation(screen, screen_balance);
	int primary_channel = get_pixel_primary_channel(pixel);
	int screen_primary_channel = get_pixel_primary_channel(screen);

	if (primary_channel != screen_primary_channel) {
		/* different main channel means pixel is on foreground,
		 * but screen color still need to be despilled from it */
		despil_pixel(out, pixel, screen, despill_factor);
		out[3] = 1.0f;
	}
	else if (saturation >= screen_saturation) {
		/* saturation of main channel is more than screen, definitely a background */
		out[0] = 0.0f;
		out[1] = 0.0f;
		out[2] = 0.0f;
		out[3] = 0.0f;
	}
	else {
		float distance;

		despil_pixel(out, pixel, screen, despill_factor);

		distance = 1.0f - saturation / screen_saturation;

		out[3] = distance;

		if (out[3] < clip_black)
			out[3] = 0.0f;
		else if (out[3] >= clip_white)
			out[3] = 1.0f;
		else
			out[3] = (out[3] - clip_black) / (clip_white - clip_black);
	}
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (in[0]->data) {
		NodeKeyingData *keying_data = node->storage;
		CompBuf *cbuf = typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *keybuf, *mattebuf;

		keybuf = dupalloc_compbuf(cbuf);

		/* single color is used for screen detection */
		composit2_pixel_processor(node, keybuf, cbuf, in[0]->vec, in[1]->data, in[1]->vec, do_key, CB_RGBA, CB_VAL);

		/* create a matte from alpha channel */
		mattebuf = valbuf_from_rgbabuf(keybuf, CHAN_A);

		/* apply dilate/erode if needed */
		if (keying_data->dilate_distance != 0) {
			int i;

			if (keying_data->dilate_distance > 0) {
				for (i = 0; i < keying_data->dilate_distance; i++)
					node_composite_morpho_dilate(mattebuf);
			}
			else {
				for (i = 0; i < -keying_data->dilate_distance; i++)
					node_composite_morpho_erode(mattebuf);
			}
		}

		if (keying_data->blur_post > 0.0f) {
			/* post-blur of matte */
			CompBuf *newmatte = alloc_compbuf(mattebuf->x, mattebuf->y, mattebuf->type, TRUE);
			int size = keying_data->blur_post;

			node_composit_blur_single_image(node, R_FILTER_BOX, size, size, newmatte, mattebuf, 1.0f);

			free_compbuf(mattebuf);
			mattebuf = newmatte;

			/* apply blurred matte on output buffer alpha */
			valbuf_to_rgbabuf(mattebuf, keybuf, CHAN_A);
		}

		out[0]->data = keybuf;
		out[1]->data = mattebuf;

		generate_preview(data, node, keybuf);

		if (cbuf!=in[0]->data)
			free_compbuf(cbuf);
	}
}

static void node_composit_init_keying(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeKeyingData *data;

	data = MEM_callocN(sizeof(NodeKeyingData), "node keying data");

	data->screen_balance = 0.5f;
	data->despill_factor = 1.0f;
	data->edge_kernel_radius = 3;
	data->edge_kernel_tolerance = 0.1f;
	data->clip_white = 1.0f;
	data->clip_black = 0.0f;
	data->clip_white = 1.0f;

	node->storage = data;
}

void register_node_type_cmp_keying(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_KEYING, "Keying", NODE_CLASS_MATTE, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_keying_in, cmp_node_keying_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_keying);
	node_type_storage(&ntype, "NodeKeyingData", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, exec);

	nodeRegisterType(ttype, &ntype);
}
