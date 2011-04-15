/*
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

/** \file blender/nodes/intern/SHD_nodes/SHD_camera.c
 *  \ingroup shdnodes
 */


#include "../SHD_util.h"

/* **************** CAMERA INFO  ******************** */
static bNodeSocketType sh_node_camera_out[]= {
	{	SOCK_VECTOR, 0, "View Vector",			1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f},		/* None of these actually */
	{	SOCK_VALUE, 0, "View Z Depth",			0.f, 0.0f, 0.0f, 0.0f, 0.0f, 99999999999.0f},	/* have any limits on their */
	{	SOCK_VALUE, 0, "View Distance",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 99999999999.0f},	/* values. */
	{	-1, 0, ""	}
};


static void node_shader_exec_camera(void *data, bNode *UNUSED(node), bNodeStack **UNUSED(in), bNodeStack **out)
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;  /* Data we need for shading. */
		
		VECCOPY(out[0]->vec, shi->co);		/* get view vector */
		out[1]->vec[0]= fabs(shi->co[2]);		/* get view z-depth */
		out[2]->vec[0]= normalize_v3(out[0]->vec);	/* get view distance */
	}
}

static int gpu_shader_camera(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "camera", in, out, GPU_builtin(GPU_VIEW_POSITION));
}

void register_node_type_sh_camera(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_CAMERA, "Camera Data", NODE_CLASS_INPUT, 0,
		NULL, sh_node_camera_out);
	node_type_size(&ntype, 95, 95, 120);
	node_type_storage(&ntype, "node_camera", NULL, NULL);
	node_type_exec(&ntype, node_shader_exec_camera);
	node_type_gpu(&ntype, gpu_shader_camera);

	nodeRegisterType(lb, &ntype);
}


