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
 * Contributor(s): Mathias Panzenböck (panzi) <grosser.meister.morti@gmx.net>.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_distance.c
 *  \ingroup texnodes
 */


#include <math.h>
#include "BLI_math.h"
#include "node_texture_util.h"
#include "NOD_texture.h"

static bNodeSocketTemplate inputs[]= {
	{ SOCK_VECTOR, 1, "Coordinate 1", 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, PROP_NONE },
	{ SOCK_VECTOR, 1, "Coordinate 2", 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, PROP_NONE },
	{ -1, 0, "" } 
};

static bNodeSocketTemplate outputs[]= {
	{ SOCK_FLOAT, 0, "Value" },
	{ -1, 0, "" }
};

static void valuefn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	float co1[3], co2[3];

	tex_input_vec(co1, in[0], p, thread);
	tex_input_vec(co2, in[1], p, thread);

	*out = len_v3v3(co2, co1);
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &valuefn, data);
}

void register_node_type_tex_distance(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_DISTANCE, "Distance", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 120, 110, 160);
	node_type_storage(&ntype, "node_distance", NULL, NULL);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
