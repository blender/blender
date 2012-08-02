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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_texture.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

#include "RE_shader_ext.h"

static bNodeSocketTemplate inputs[]= {
	{ SOCK_RGBA, 1, N_("Color1"), 1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_RGBA, 1, N_("Color2"), 0.0f, 0.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

static bNodeSocketTemplate outputs[]= {
	{ SOCK_RGBA, 0, N_("Color") },
	{ -1, 0, "" }
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	Tex *nodetex = (Tex *)node->id;
	static float red[] = {1, 0, 0, 1};
	static float white[] = {1, 1, 1, 1};
	float co[3], dxt[3], dyt[3];
	
	copy_v3_v3(co, p->co);
	if (p->osatex) {
		copy_v3_v3(dxt, p->dxt);
		copy_v3_v3(dyt, p->dyt);
	}
	else {
		zero_v3(dxt);
		zero_v3(dyt);
	}
	
	if (node->custom2 || node->need_exec==0) {
		/* this node refers to its own texture tree! */
		copy_v4_v4(out, (fabsf(co[0] - co[1]) < 0.01f) ? white : red);
	}
	else if (nodetex) {
		TexResult texres;
		int textype;
		float nor[] = {0, 0, 0};
		float col1[4], col2[4];
		
		tex_input_rgba(col1, in[0], p, thread);
		tex_input_rgba(col2, in[1], p, thread);
		
		texres.nor = nor;
		textype = multitex_nodes(nodetex, co, dxt, dyt, p->osatex,
			&texres, thread, 0, p->shi, p->mtex);
		
		if (textype & TEX_RGB) {
			copy_v4_v4(out, &texres.tr);
		}
		else {
			copy_v4_v4(out, col1);
			ramp_blend(MA_RAMP_BLEND, out, texres.tin, col2);
		}
	}
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn, data);
}

void register_node_type_tex_texture(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_TEXTURE, "Texture", NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 120, 80, 240);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
