/**
 * $Id$
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** VALUE SQUEEZE ******************** */ 
static bNodeSocketType sh_node_squeeze_in[]= { 
	{ SOCK_VALUE, 1, "Value", 0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f}, 
	{ SOCK_VALUE, 1, "Width", 1.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f}, 
	{ SOCK_VALUE, 1, "Center", 0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType sh_node_squeeze_out[]= { 
	{ SOCK_VALUE, 0, "Value", 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void node_shader_exec_squeeze(void *data, bNode *node, bNodeStack **in, 
bNodeStack **out) 
{
	float vec[3];
	
	nodestack_get_vec(vec, SOCK_VALUE, in[0]);
	nodestack_get_vec(vec+1, SOCK_VALUE, in[1]);
	nodestack_get_vec(vec+2, SOCK_VALUE, in[2]);

	out[0]->vec[0] = 1.0f / (1.0f + pow(2.71828183,-((vec[0]-vec[2])*vec[1]))) ;
}

static int gpu_shader_squeeze(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "squeeze", in, out);
}

bNodeType sh_node_squeeze= { 
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_SQUEEZE, 
	/* name        */	"Squeeze Value", 
	/* width+range */	120, 110, 160, 
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS, 
	/* input sock  */	sh_node_squeeze_in, 
	/* output sock */	sh_node_squeeze_out, 
	/* storage     */	"node_squeeze", 
	/* execfunc    */	node_shader_exec_squeeze,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_squeeze
};

