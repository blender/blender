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

#include "../node_shader_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_normal_map_in[] = {
	{   SOCK_FLOAT, 1, N_("Strength"),	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f},
	{	SOCK_RGBA, 1, N_("Color"), 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_normal_map_out[] = {
	{	SOCK_VECTOR, 0, N_("Normal"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_normal_map(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeShaderNormalMap *attr = MEM_callocN(sizeof(NodeShaderNormalMap), "NodeShaderNormalMap");
	node->storage = attr;
}

static void node_shader_exec_normal_map(
        void *data, int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata),
        bNodeStack **in, bNodeStack **out)
{
	if (data) {
		ShadeInput *shi = ((ShaderCallData *)data)->shi;

		NodeShaderNormalMap *nm = node->storage;

		float strength, vecIn[3];
		nodestack_get_vec(&strength, SOCK_FLOAT, in[0]);
		nodestack_get_vec(vecIn, SOCK_VECTOR, in[1]);

		vecIn[0] = -2 * (vecIn[0] - 0.5f);
		vecIn[1] =  2 * (vecIn[1] - 0.5f);
		vecIn[2] =  2 * (vecIn[2] - 0.5f);

		CLAMP_MIN(strength, 0.0f);

		float *N = shi->nmapnorm;
		int uv_index = 0;
		switch (nm->space) {
			case SHD_NORMAL_MAP_TANGENT:
				if (nm->uv_map[0]) {
					/* find uv map by name */
					for (int i = 0; i < shi->totuv; i++) {
						if (STREQ(shi->uv[i].name, nm->uv_map)) {
							uv_index = i;
							break;
						}
					}
				}
				else {
					uv_index = shi->actuv;
				}

				float *T = shi->tangents[uv_index];

				float B[3];
				cross_v3_v3v3(B, N, T);
				mul_v3_fl(B, T[3]);

				for (int j = 0; j < 3; j++)
					out[0]->vec[j] = vecIn[0] * T[j] + vecIn[1] * B[j] + vecIn[2] * N[j];
				interp_v3_v3v3(out[0]->vec, N, out[0]->vec, strength);
				if (shi->use_world_space_shading) {
					mul_mat3_m4_v3((float (*)[4])RE_render_current_get_matrix(RE_VIEWINV_MATRIX), out[0]->vec);
				}
				break;

			case SHD_NORMAL_MAP_OBJECT:
			case SHD_NORMAL_MAP_BLENDER_OBJECT:
				if (shi->use_world_space_shading) {
					mul_mat3_m4_v3((float (*)[4])RE_object_instance_get_matrix(shi->obi, RE_OBJECT_INSTANCE_MATRIX_OB), vecIn);
					mul_mat3_m4_v3((float (*)[4])RE_render_current_get_matrix(RE_VIEWINV_MATRIX), N);
				}
				else
					mul_mat3_m4_v3((float (*)[4])RE_object_instance_get_matrix(shi->obi, RE_OBJECT_INSTANCE_MATRIX_LOCALTOVIEW), vecIn);
				interp_v3_v3v3(out[0]->vec, N, vecIn, strength);
				break;

			case SHD_NORMAL_MAP_WORLD:
			case SHD_NORMAL_MAP_BLENDER_WORLD:
				if (shi->use_world_space_shading)
					mul_mat3_m4_v3((float (*)[4])RE_render_current_get_matrix(RE_VIEWINV_MATRIX), N);
				else
					mul_mat3_m4_v3((float (*)[4])RE_render_current_get_matrix(RE_VIEW_MATRIX), vecIn);
				interp_v3_v3v3(out[0]->vec, N, vecIn, strength);
				break;
		}
		if (shi->use_world_space_shading) {
			negate_v3(out[0]->vec);
		}
		normalize_v3(out[0]->vec);
	}
}

static int gpu_shader_normal_map(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	NodeShaderNormalMap *nm = node->storage;
	GPUNodeLink *negnorm;
	GPUNodeLink *realnorm;
	GPUNodeLink *strength;

	float d[4] = {0, 0, 0, 0};

	if (in[0].link)
		strength = in[0].link;
	else
		strength = GPU_uniform(in[0].vec);

	if (in[1].link)
		realnorm = in[1].link;
	else
		realnorm = GPU_uniform(in[1].vec);

	negnorm = GPU_builtin(GPU_VIEW_NORMAL);
	GPU_link(mat, "math_max", strength, GPU_uniform(d), &strength);

	if (GPU_material_use_world_space_shading(mat)) {

		/* ******* CYCLES or BLENDER INTERNAL with world space shading flag ******* */

		const char *color_to_normal_fnc_name = "color_to_normal_new_shading";
		if (nm->space == SHD_NORMAL_MAP_BLENDER_OBJECT || nm->space == SHD_NORMAL_MAP_BLENDER_WORLD || !GPU_material_use_new_shading_nodes(mat))
			color_to_normal_fnc_name = "color_to_blender_normal_new_shading";
		switch (nm->space) {
			case SHD_NORMAL_MAP_TANGENT:
				GPU_link(mat, "color_to_normal_new_shading", realnorm, &realnorm);
				GPU_link(mat, "node_normal_map", GPU_attribute(CD_TANGENT, nm->uv_map), negnorm, realnorm, &realnorm);
				GPU_link(mat, "vec_math_mix", strength, realnorm, GPU_builtin(GPU_VIEW_NORMAL), &out[0].link);
				/* for uniform scale this is sufficient to match Cycles */
				GPU_link(mat, "direction_transform_m4v3", out[0].link, GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &out[0].link);
				GPU_link(mat, "vect_normalize", out[0].link, &out[0].link);
				return true;
			case SHD_NORMAL_MAP_OBJECT:
			case SHD_NORMAL_MAP_BLENDER_OBJECT:
				GPU_link(mat, "direction_transform_m4v3", negnorm, GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &negnorm);
				GPU_link(mat, color_to_normal_fnc_name, realnorm, &realnorm);
				GPU_link(mat, "direction_transform_m4v3", realnorm, GPU_builtin(GPU_OBJECT_MATRIX), &realnorm);
				break;
			case SHD_NORMAL_MAP_WORLD:
			case SHD_NORMAL_MAP_BLENDER_WORLD:
				GPU_link(mat, "direction_transform_m4v3", negnorm, GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &negnorm);
				GPU_link(mat, color_to_normal_fnc_name, realnorm, &realnorm);
				break;
		}

	}
	else {

		/* ************** BLENDER INTERNAL without world space shading flag ******* */

		GPU_link(mat, "color_to_normal", realnorm, &realnorm);
		GPU_link(mat, "mtex_negate_texnormal", realnorm,  &realnorm);
		GPU_link(mat, "vec_math_negate", negnorm, &negnorm);

		switch (nm->space) {
			case SHD_NORMAL_MAP_TANGENT:
				GPU_link(mat, "node_normal_map", GPU_attribute(CD_TANGENT, nm->uv_map), negnorm, realnorm, &realnorm);
				break;
			case SHD_NORMAL_MAP_OBJECT:
			case SHD_NORMAL_MAP_BLENDER_OBJECT:
				GPU_link(mat, "direction_transform_m4v3", realnorm, GPU_builtin(GPU_LOC_TO_VIEW_MATRIX),  &realnorm);
				break;
			case SHD_NORMAL_MAP_WORLD:
			case SHD_NORMAL_MAP_BLENDER_WORLD:
				GPU_link(mat, "direction_transform_m4v3", realnorm, GPU_builtin(GPU_VIEW_MATRIX),  &realnorm);
				break;
		}
	}

	GPU_link(mat, "vec_math_mix", strength, realnorm, negnorm,  &out[0].link);
	GPU_link(mat, "vect_normalize", out[0].link, &out[0].link);

	return true;
}

/* node type definition */
void register_node_type_sh_normal_map(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_NORMAL_MAP, "Normal Map", NODE_CLASS_OP_VECTOR, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING | NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_normal_map_in, sh_node_normal_map_out);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_init(&ntype, node_shader_init_normal_map);
	node_type_storage(&ntype, "NodeShaderNormalMap", node_free_standard_storage, node_copy_standard_storage);
	node_type_gpu(&ntype, gpu_shader_normal_map);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_normal_map);

	nodeRegisterType(&ntype);
}
