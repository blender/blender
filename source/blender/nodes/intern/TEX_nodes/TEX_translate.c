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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include "../TEX_util.h"

static bNodeSocketType inputs[]= { 
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ SOCK_VECTOR, 1, "Offset",   0.0f, 0.0f, 0.0f, 0.0f,  -10000.0f, 10000.0f },
	{ -1, 0, "" } 
};

static bNodeSocketType outputs[]= { 
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float offset[3], new_coord[3];
	TexParams np = *p;
	np.coord = new_coord;
	
	tex_input_vec(offset, in[1], p, thread);
	
	new_coord[0] = p->coord[0] + offset[0];
	new_coord[1] = p->coord[1] + offset[1];
	new_coord[2] = p->coord[2] + offset[2];
	
	tex_input_rgba(out, in[0], &np, thread);
}
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &colorfn, data);
}

bNodeType tex_node_translate = {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_TRANSLATE, 
	/* name        */	"Translate", 
	/* width+range */	90, 80, 100, 
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS, 
	/* input sock  */	inputs, 
	/* output sock */	outputs, 
	/* storage     */	"", 
	/* execfunc    */	exec,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};

