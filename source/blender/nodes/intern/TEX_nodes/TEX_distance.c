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
 * Contributor(s): Mathias Panzenböck (panzi) <grosser.meister.morti@gmx.net>.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include "BLI_arithb.h"
#include "../TEX_util.h"

static bNodeSocketType inputs[]= {
	{ SOCK_VECTOR, 1, "Coordinate 1", 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f },
	{ SOCK_VECTOR, 1, "Coordinate 2", 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f },
	{ -1, 0, "" } 
};

static bNodeSocketType outputs[]= {
	{ SOCK_VALUE, 0, "Value", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

static void valuefn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float coord1[3], coord2[3];

	tex_input_vec(coord1, in[0], p, thread);
	tex_input_vec(coord2, in[1], p, thread);

	*out = VecLenf(coord2, coord1);
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &valuefn, data);
}

bNodeType tex_node_distance= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_DISTANCE,
	/* name        */	"Distance",
	/* width+range */	120, 110, 160,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	inputs,
	/* output sock */	outputs,
	/* storage     */	"node_distance",
	/* execfunc    */	exec,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};


