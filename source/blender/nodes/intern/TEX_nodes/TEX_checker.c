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

#include "../TEX_util.h"                                                   
#include <math.h>

static bNodeSocketType inputs[]= {
	{ SOCK_RGBA, 1, "Color1", 1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 1.0f },
	{ SOCK_RGBA, 1, "Color2", 1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 1, "Size",   0.5f, 0.0f, 0.0f, 0.0f,  0.0f, 100.0f },
	{ -1, 0, "" }
};
static bNodeSocketType outputs[]= {
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float x  = p->coord[0];
	float y  = p->coord[1];
	float z  = p->coord[2];
	float sz = tex_input_value(in[2], p, thread);
	
	/* 0.00001  because of unit sized stuff */
	int xi = (int)fabs(floor(0.00001 + x / sz));
	int yi = (int)fabs(floor(0.00001 + y / sz));
	int zi = (int)fabs(floor(0.00001 + z / sz));
	
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

bNodeType tex_node_checker= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_CHECKER,
	/* name        */	"Checker",
	/* width+range */	100, 60, 150,
	/* class+opts  */	NODE_CLASS_PATTERN, NODE_OPTIONS | NODE_PREVIEW,
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
