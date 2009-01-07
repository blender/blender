/**
 *
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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** SEPARATE RGBA ******************** */
static bNodeSocketType sh_node_seprgb_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_seprgb_out[]= {
	{	SOCK_VALUE, 0, "R",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "G",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "B",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_seprgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	out[0]->vec[0] = in[0]->vec[0];
	out[1]->vec[0] = in[0]->vec[1];
	out[2]->vec[0] = in[0]->vec[2];
}

static int gpu_shader_seprgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "separate_rgb", in, out);
}

bNodeType sh_node_seprgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_SEPRGB,
	/* name        */	"Separate RGB",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	sh_node_seprgb_in,
	/* output sock */	sh_node_seprgb_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_seprgb,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_seprgb
	
};


/* **************** COMBINE RGB ******************** */
static bNodeSocketType sh_node_combrgb_in[]= {
	{	SOCK_VALUE, 1, "R",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "G",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "B",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_combrgb_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_combrgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	out[0]->vec[0] = in[0]->vec[0];
	out[0]->vec[1] = in[1]->vec[0];
	out[0]->vec[2] = in[2]->vec[0];
}

static int gpu_shader_combrgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "combine_rgb", in, out);
}

bNodeType sh_node_combrgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_COMBRGB,
	/* name        */	"Combine RGB",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_combrgb_in,
	/* output sock */	sh_node_combrgb_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_combrgb,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_combrgb
	
};
