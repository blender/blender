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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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



/* **************** INVERT ******************** */ 
static bNodeSocketType sh_node_invert_in[]= { 
	{ SOCK_VALUE, 1, "Fac", 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType sh_node_invert_out[]= { 
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void node_shader_exec_invert(void *data, bNode *node, bNodeStack **in, 
bNodeStack **out) 
{
	float col[3], facm;

	col[0] = 1.0f - in[1]->vec[0];
	col[1] = 1.0f - in[1]->vec[1];
	col[2] = 1.0f - in[1]->vec[2];
	
	/* if fac, blend result against original input */
	if (in[0]->vec[0] < 1.0f) {
		facm = 1.0 - in[0]->vec[0];

		col[0] = in[0]->vec[0]*col[0] + (facm*in[1]->vec[0]);
		col[1] = in[0]->vec[0]*col[1] + (facm*in[1]->vec[1]);
		col[2] = in[0]->vec[0]*col[2] + (facm*in[1]->vec[2]);
	}
	
	VECCOPY(out[0]->vec, col);
}

static int gpu_shader_invert(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "invert", in, out);
}

bNodeType sh_node_invert= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_INVERT, 
	/* name        */	"Invert", 
	/* width+range */	90, 80, 100, 
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS, 
	/* input sock  */	sh_node_invert_in, 
	/* output sock */	sh_node_invert_out, 
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_invert,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_invert
};

