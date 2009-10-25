/**
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jucas.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../TEX_util.h"

static bNodeSocketType inputs[]= { 
	{ SOCK_VALUE, 1, "Val",   0.0f,   0.0f, 0.0f, 1.0f,  0.0f,   1.0f },
	{ SOCK_VALUE, 1, "Nabla", 0.025f, 0.0f, 0.0f, 0.0f,  0.001f, 0.1f },
	{ -1, 0, "" } 
};

static bNodeSocketType outputs[]= { 
	{ SOCK_VECTOR, 0, "Normal", 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f }, 
	{ -1, 0, "" } 
};

static void normalfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float new_coord[3];
	float *coord = p->coord;

	float nabla = tex_input_value(in[1], p, thread);	
	float val;
	float nor[3];
	
	TexParams np = *p;
	np.coord = new_coord;

	val = tex_input_value(in[0], p, thread);

	new_coord[0] = coord[0] + nabla;
	new_coord[1] = coord[1];
	new_coord[2] = coord[2];
	nor[0] = tex_input_value(in[0], &np, thread);

	new_coord[0] = coord[0];
	new_coord[1] = coord[1] + nabla;
	nor[1] = tex_input_value(in[0], &np, thread);
	
	new_coord[1] = coord[1];
	new_coord[2] = coord[2] + nabla;
	nor[2] = tex_input_value(in[0], &np, thread);

	out[0] = val-nor[0];
	out[1] = val-nor[1];
	out[2] = val-nor[2];
}
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &normalfn, data);
}

bNodeType tex_node_valtonor = {
	/* *next,*prev     */ NULL, NULL,
	/* type code       */ TEX_NODE_VALTONOR, 
	/* name            */ "Value to Normal", 
	/* width+range     */ 90, 80, 100, 
	/* class+opts      */ NODE_CLASS_CONVERTOR, NODE_OPTIONS, 
	/* input sock      */ inputs, 
	/* output sock     */ outputs, 
	/* storage         */ "", 
	/* execfunc        */ exec,
	/* butfunc         */ NULL,
	/* initfunc        */ NULL,
	/* freestoragefunc */ NULL,
	/* copystoragefunc */ NULL,
	/* id              */ NULL
};

