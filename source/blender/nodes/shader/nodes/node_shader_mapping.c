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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_mapping.c
 *  \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** MAPPING  ******************** */
static bNodeSocketTemplate sh_node_mapping_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_mapping_out[] = {
	{	SOCK_VECTOR, 0, N_("Vector")},
	{	-1, 0, ""	}
};

static void *node_shader_initexec_mapping(bNodeExecContext *UNUSED(context),
                                          bNode *node,
                                          bNodeInstanceKey UNUSED(key))
{
	TexMapping *texmap = node->storage;
	BKE_texture_mapping_init(texmap);
	return NULL;
}

/* do the regular mapping options for blender textures */
static void node_shader_exec_mapping(void *UNUSED(data), int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	TexMapping *texmap = node->storage;
	float *vec = out[0]->vec;

	/* stack order input:  vector */
	/* stack order output: vector */
	nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	mul_m4_v3(texmap->mat, vec);

	if (texmap->flag & TEXMAP_CLIP_MIN) {
		if (vec[0] < texmap->min[0]) vec[0] = texmap->min[0];
		if (vec[1] < texmap->min[1]) vec[1] = texmap->min[1];
		if (vec[2] < texmap->min[2]) vec[2] = texmap->min[2];
	}
	if (texmap->flag & TEXMAP_CLIP_MAX) {
		if (vec[0] > texmap->max[0]) vec[0] = texmap->max[0];
		if (vec[1] > texmap->max[1]) vec[1] = texmap->max[1];
		if (vec[2] > texmap->max[2]) vec[2] = texmap->max[2];
	}

	if (texmap->type == TEXMAP_TYPE_NORMAL)
		normalize_v3(vec);
}


static void node_shader_init_mapping(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->storage = BKE_texture_mapping_add(TEXMAP_TYPE_POINT);
}

static int gpu_shader_mapping(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	TexMapping *texmap = node->storage;
	float domin = (texmap->flag & TEXMAP_CLIP_MIN) != 0;
	float domax = (texmap->flag & TEXMAP_CLIP_MAX) != 0;
	static float max[3] = { FLT_MAX,  FLT_MAX,  FLT_MAX, 0.0};
	static float min[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0};
	GPUNodeLink *tmin, *tmax, *tmat0, *tmat1, *tmat2, *tmat3;

	tmin = GPU_uniform_buffer((domin) ? texmap->min : min, GPU_VEC3);
	tmax = GPU_uniform_buffer((domax) ? texmap->max : max, GPU_VEC3);
	tmat0 = GPU_uniform_buffer((float *)texmap->mat[0], GPU_VEC4);
	tmat1 = GPU_uniform_buffer((float *)texmap->mat[1], GPU_VEC4);
	tmat2 = GPU_uniform_buffer((float *)texmap->mat[2], GPU_VEC4);
	tmat3 = GPU_uniform_buffer((float *)texmap->mat[3], GPU_VEC4);

	GPU_stack_link(mat, node, "mapping", in, out, tmat0, tmat1, tmat2, tmat3, tmin, tmax);

	if (texmap->type == TEXMAP_TYPE_NORMAL)
		GPU_link(mat, "texco_norm", out[0].link, &out[0].link);

	return true;
}

void register_node_type_sh_mapping(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_MAPPING, "Mapping", NODE_CLASS_OP_VECTOR, 0);
	node_type_socket_templates(&ntype, sh_node_mapping_in, sh_node_mapping_out);
	node_type_size(&ntype, 320, 160, 360);
	node_type_init(&ntype, node_shader_init_mapping);
	node_type_storage(&ntype, "TexMapping", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_initexec_mapping, NULL, node_shader_exec_mapping);
	node_type_gpu(&ntype, gpu_shader_mapping);

	nodeRegisterType(&ntype);
}
