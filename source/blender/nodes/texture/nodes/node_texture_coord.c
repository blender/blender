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

/** \file blender/nodes/texture/nodes/node_texture_coord.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

static bNodeSocketTemplate outputs[]= { 
	{ SOCK_VECTOR, 0, N_("Coordinates") },
	{ -1, 0, "" }
};

static void vectorfn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **UNUSED(in), short UNUSED(thread))
{
	copy_v3_v3(out, p->co);
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &vectorfn, data);
}

void register_node_type_tex_coord(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_COORD, "Coordinates", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, outputs);
	node_type_size(&ntype, 120, 110, 160);
	node_type_storage(&ntype, "node_coord", NULL, NULL);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
