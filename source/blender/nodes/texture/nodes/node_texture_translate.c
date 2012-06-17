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

/** \file blender/nodes/texture/nodes/node_texture_translate.c
 *  \ingroup texnodes
 */


#include <math.h>
#include "node_texture_util.h"
#include "NOD_texture.h"

static bNodeSocketTemplate inputs[]= { 
	{ SOCK_RGBA, 1, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
	{ SOCK_VECTOR, 1, N_("Offset"),   0.0f, 0.0f, 0.0f, 0.0f,  -10000.0f, 10000.0f, PROP_TRANSLATION },
	{ -1, 0, "" } 
};

static bNodeSocketTemplate outputs[]= { 
	{ SOCK_RGBA, 0, N_("Color")}, 
	{ -1, 0, "" } 
};

static void colorfn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	float offset[3], new_co[3];
	TexParams np = *p;
	np.co = new_co;
	
	tex_input_vec(offset, in[1], p, thread);
	
	new_co[0] = p->co[0] + offset[0];
	new_co[1] = p->co[1] + offset[1];
	new_co[2] = p->co[2] + offset[2];
	
	tex_input_rgba(out, in[0], &np, thread);
}
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &colorfn, data);
}

void register_node_type_tex_translate(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_TRANSLATE, "Translate", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 90, 80, 100);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
