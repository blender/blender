/*
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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Thomas Dinges
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_sepcombXYZ.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** SEPARATE XYZ ******************** */
static bNodeSocketTemplate sh_node_sepxyz_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),			0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_sepxyz_out[] = {
	{	SOCK_FLOAT, 0, N_("X")},
	{	SOCK_FLOAT, 0, N_("Y")},
	{	SOCK_FLOAT, 0, N_("Z")},
	{	-1, 0, ""	}
};

static int gpu_shader_sepxyz(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "separate_xyz", in, out);
}

void register_node_type_sh_sepxyz(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_SEPXYZ, "Separate XYZ", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_sepxyz_in, sh_node_sepxyz_out);
	node_type_gpu(&ntype, gpu_shader_sepxyz);

	nodeRegisterType(&ntype);
}



/* **************** COMBINE XYZ ******************** */
static bNodeSocketTemplate sh_node_combxyz_in[] = {
	{	SOCK_FLOAT, 1, N_("X"),			0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f},
	{	SOCK_FLOAT, 1, N_("Y"),			0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f},
	{	SOCK_FLOAT, 1, N_("Z"),			0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_combxyz_out[] = {
	{	SOCK_VECTOR, 0, N_("Vector")},
	{	-1, 0, ""	}
};

static int gpu_shader_combxyz(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "combine_xyz", in, out);
}

void register_node_type_sh_combxyz(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_COMBXYZ, "Combine XYZ", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_combxyz_in, sh_node_combxyz_out);
	node_type_gpu(&ntype, gpu_shader_combxyz);

	nodeRegisterType(&ntype);
}
