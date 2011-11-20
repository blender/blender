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

/** \file blender/nodes/texture/nodes/node_texture_decompose.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"        
#include "NOD_texture.h"
#include <math.h>

static bNodeSocketTemplate inputs[]= {
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs[]= {
	{ SOCK_FLOAT, 0, "Red" },
	{ SOCK_FLOAT, 0, "Green" },
	{ SOCK_FLOAT, 0, "Blue" },
	{ SOCK_FLOAT, 0, "Alpha" },
	{ -1, 0, "" }
};

static void valuefn_r(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[0];
}

static void valuefn_g(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[1];
}

static void valuefn_b(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[2];
}

static void valuefn_a(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[3];
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &valuefn_r, data);
	tex_output(node, in, out[1], &valuefn_g, data);
	tex_output(node, in, out[2], &valuefn_b, data);
	tex_output(node, in, out[3], &valuefn_a, data);
}

void register_node_type_tex_decompose(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_DECOMPOSE, "Decompose RGBA", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 100, 60, 150);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
