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

/* **************** INVERT ******************** */ 
static bNodeSocketType inputs[]= { 
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType outputs[]= { 
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float col[4];
	
	tex_input_rgba(col, in[0], p, thread);

	col[0] = 1.0f - col[0];
	col[1] = 1.0f - col[1];
	col[2] = 1.0f - col[2];
	
	VECCOPY(out, col);
	out[3] = col[3];
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn);
	
	tex_do_preview(node, out[0], data);
}

bNodeType tex_node_invert= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_INVERT, 
	/* name        */	"Invert", 
	/* width+range */	90, 80, 100, 
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS, 
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

