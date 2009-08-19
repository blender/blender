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

#include "../TEX_util.h"                                                   
#include <math.h>

static bNodeSocketType inputs[]= {
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketType outputs[]= {
	{ SOCK_VALUE, 0, "Red",   0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 0, "Green", 0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 0, "Blue",  0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 0, "Alpha", 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ -1, 0, "" }
};

static void valuefn_r(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[0];
}

static void valuefn_g(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[1];
}

static void valuefn_b(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[2];
}

static void valuefn_a(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	tex_input_rgba(out, in[0], p, thread);
	*out = out[3];
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &valuefn_r);
	tex_output(node, in, out[1], &valuefn_g);
	tex_output(node, in, out[2], &valuefn_b);
	tex_output(node, in, out[3], &valuefn_a);
}

bNodeType tex_node_decompose= {
	/* *next,*prev     */  NULL, NULL,
	/* type code       */  TEX_NODE_DECOMPOSE,
	/* name            */  "Decompose RGBA",
	/* width+range     */  100, 60, 150,
	/* class+opts      */  NODE_CLASS_OP_COLOR, 0,
	/* input sock      */  inputs,
	/* output sock     */  outputs,
	/* storage         */  "", 
	/* execfunc        */  exec,
	/* butfunc         */  NULL,
	/* initfunc        */  NULL,
	/* freestoragefunc */  NULL,
	/* copystoragefunc */  NULL,
	/* id              */  NULL   
	
};
