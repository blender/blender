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

/* **************** OUTPUT ******************** */
static bNodeSocketType sh_node_output_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_output(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		float col[4];
		
		/* stack order input sockets: col, alpha, normal */
		nodestack_get_vec(col, SOCK_VECTOR, in[0]);
		nodestack_get_vec(col+3, SOCK_VALUE, in[1]);
		
		if(shi->do_preview) {
			nodeAddToPreview(node, col, shi->xs, shi->ys);
			node->lasty= shi->ys;
		}
		
		if(node->flag & NODE_DO_OUTPUT) {
			ShadeResult *shr= ((ShaderCallData *)data)->shr;
			
			QUATCOPY(shr->combined, col);
			shr->alpha= col[3];
			
			//	VECCOPY(shr->nor, in[3]->vec);
		}
	}	
}

static int gpu_shader_output(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *outlink;

	/*if(in[1].hasinput)
		GPU_material_enable_alpha(mat);*/

	GPU_stack_link(mat, "output_node", in, out, &outlink);
	GPU_material_output_link(mat, outlink);

	return 1;
}

bNodeType sh_node_output= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_OUTPUT,
	/* name        */	"Output",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	sh_node_output_in,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_output,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_output
	
};

