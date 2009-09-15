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

static bNodeSocketType inputs[]= {
	{ SOCK_VALUE, 1, "Red",   0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 1, "Green", 0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 1, "Blue",  0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ SOCK_VALUE, 1, "Alpha", 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketType outputs[]= {
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	int i;
	for(i = 0; i < 4; i++)
		out[i] = tex_input_value(in[i], p, thread);
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn);
}

bNodeType tex_node_compose= {
	/* *next,*prev     */ NULL, NULL,
	/* type code       */ TEX_NODE_COMPOSE,
	/* name            */ "Compose RGBA",
	/* width+range     */ 100, 60, 150,
	/* class+opts      */ NODE_CLASS_OP_COLOR, 0,
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
