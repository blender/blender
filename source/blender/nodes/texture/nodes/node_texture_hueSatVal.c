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
 * Contributor(s): Juho VepsÃ¤lÃ¤inen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_hueSatVal.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"


static bNodeSocketTemplate inputs[] = {
	{ SOCK_FLOAT, 1, N_("Hue"),        0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.5f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("Saturation"), 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("Value"),      1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("Factor"),     1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, PROP_NONE },
	{ SOCK_RGBA,  1, N_("Color"),      0.8f, 0.8f, 0.8f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs[] = {
	{ SOCK_RGBA, 0, N_("Color") },
	{ -1, 0, "" }
};

static void do_hue_sat_fac(bNode *UNUSED(node), float *out, float hue, float sat, float val, float *in, float fac)
{
	if (fac != 0 && (hue != 0.5f || sat != 1 || val != 1)) {
		float col[3], hsv[3], mfac = 1.0f - fac;
		
		rgb_to_hsv(in[0], in[1], in[2], hsv, hsv + 1, hsv + 2);
		hsv[0] += (hue - 0.5f);
		if (hsv[0] > 1.0f) hsv[0] -= 1.0f; else if (hsv[0] < 0.0f) hsv[0] += 1.0f;
		hsv[1] *= sat;
		if (hsv[1] > 1.0f) hsv[1] = 1.0f; else if (hsv[1] < 0.0f) hsv[1] = 0.0f;
		hsv[2] *= val;
		if (hsv[2] > 1.0f) hsv[2] = 1.0f; else if (hsv[2] < 0.0f) hsv[2] = 0.0f;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], col, col + 1, col + 2);

		out[0] = mfac * in[0] + fac * col[0];
		out[1] = mfac * in[1] + fac * col[1];
		out[2] = mfac * in[2] + fac * col[2];
	}
	else {
		copy_v4_v4(out, in);
	}
}

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{	
	float hue = tex_input_value(in[0], p, thread);
	float sat = tex_input_value(in[1], p, thread);
	float val = tex_input_value(in[2], p, thread);
	float fac = tex_input_value(in[3], p, thread);
	
	float col[4];
	tex_input_rgba(col, in[4], p, thread);
	
	hue += 0.5f; /* [-0.5, 0.5] -> [0, 1] */
	
	do_hue_sat_fac(node, out, hue, sat, val, col, fac);
	
	out[3] = col[3];
}

static void exec(void *data, int UNUSED(thread), bNode *node, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, execdata, in, out[0], &colorfn, data);
}

void register_node_type_tex_hue_sat(void)
{
	static bNodeType ntype;
	
	tex_node_type_base(&ntype, TEX_NODE_HUE_SAT, "Hue Saturation Value", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_exec(&ntype, NULL, NULL, exec);
	
	nodeRegisterType(&ntype);
}
