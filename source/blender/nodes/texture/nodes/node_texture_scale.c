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

/** \file blender/nodes/texture/nodes/node_texture_scale.c
 *  \ingroup texnodes
 */


#include <math.h>
#include "node_texture_util.h"

static bNodeSocketTemplate inputs[]= { 
	{ SOCK_RGBA,   1, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f },
	{ SOCK_VECTOR, 1, N_("Scale"), 1.0f, 1.0f, 1.0f, 0.0f,  -10.0f, 10.0f, PROP_XYZ },
	{ -1, 0, "" } 
};

static bNodeSocketTemplate outputs[]= { 
	{ SOCK_RGBA, 0, N_("Color")}, 
	{ -1, 0, "" } 
};

static void colorfn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	float scale[3], new_co[3], new_dxt[3], new_dyt[3];
	TexParams np = *p;

	np.co = new_co;
	np.dxt = new_dxt;
	np.dyt = new_dyt;
	
	tex_input_vec(scale, in[1], p, thread);

	mul_v3_v3v3(new_co, p->co, scale);
	if (p->osatex) {
		mul_v3_v3v3(new_dxt, p->dxt, scale);
		mul_v3_v3v3(new_dyt, p->dyt, scale);
	}
	
	tex_input_rgba(out, in[0], &np, thread);
}
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &colorfn, data);
}

void register_node_type_tex_scale(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_SCALE, "Scale", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 90, 80, 100);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
