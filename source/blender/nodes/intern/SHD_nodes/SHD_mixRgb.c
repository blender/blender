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


/* **************** MIX RGB ******************** */
static bNodeSocketType sh_node_mix_rgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Color1",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Color2",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType sh_node_mix_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_mix_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, col1, col2 */
	/* stack order out: col */
	float col[3];
	float fac;
	float vec[3];

	nodestack_get_vec(&fac, SOCK_VALUE, in[0]);
	CLAMP(fac, 0.0f, 1.0f);
	
	nodestack_get_vec(col, SOCK_VECTOR, in[1]);
	nodestack_get_vec(vec, SOCK_VECTOR, in[2]);

	ramp_blend(node->custom1, col, col+1, col+2, fac, vec);
	VECCOPY(out[0]->vec, col);
}

static int gpu_shader_mix_rgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	static char *names[] = {"mix_blend", "mix_add", "mix_mult", "mix_sub",
		"mix_screen", "mix_div", "mix_diff", "mix_dark", "mix_light",
		"mix_overlay", "mix_dodge", "mix_burn", "mix_hue", "mix_sat",
		"mix_val", "mix_color", "mix_soft", "mix_linear"};

	return GPU_stack_link(mat, names[node->custom1], in, out);
}


bNodeType sh_node_mix_rgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_MIX_RGB,
	/* name        */	"Mix",
	/* width+range */	100, 60, 150,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	sh_node_mix_rgb_in,
	/* output sock */	sh_node_mix_rgb_out,
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_mix_rgb,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_mix_rgb
	
};
