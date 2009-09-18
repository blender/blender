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
	{ SOCK_RGBA, 1, "Color", 1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketType outputs[]= {
	{ -1, 0, "" }
};

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_do_preview(node, in[0], data);
}

bNodeType tex_node_viewer = {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_VIEWER,
	/* name        */	"Viewer",
	/* width+range */	100, 60, 150,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
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
