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

/** \file blender/nodes/texture/nodes/node_texture_checker.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"
#include <math.h>

static bNodeSocketTemplate inputs[]= {
	{ SOCK_RGBA, 1, "Color1", 1.0f, 0.0f, 0.0f, 1.0f },
	{ SOCK_RGBA, 1, "Color2", 1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_FLOAT, 1, "Size",   0.5f, 0.0f, 0.0f, 0.0f,  0.0f, 100.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs[]= {
	{ SOCK_RGBA, 0, "Color" },
	{ -1, 0, "" }
};

static void colorfn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	float x  = p->co[0];
	float y  = p->co[1];
	float z  = p->co[2];
	float sz = tex_input_value(in[2], p, thread);
	
	/* 0.00001  because of unit sized stuff */
	int xi = (int)fabs(floor(0.00001f + x / sz));
	int yi = (int)fabs(floor(0.00001f + y / sz));
	int zi = (int)fabs(floor(0.00001f + z / sz));
	
	if( (xi % 2 == yi % 2) == (zi % 2) ) {
		tex_input_rgba(out, in[0], p, thread);
	} else {
		tex_input_rgba(out, in[1], p, thread);
	} 
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn, data);
}

void register_node_type_tex_checker(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_CHECKER, "Checker", NODE_CLASS_PATTERN, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 100, 60, 150);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
