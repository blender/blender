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

/** \file blender/nodes/shader/nodes/node_shader_geom.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

#include "DNA_customdata_types.h"

/* **************** GEOMETRY  ******************** */

/* output socket type definition */
static bNodeSocketTemplate sh_node_geom_out[] = {
	{	SOCK_VECTOR, 0, N_("Global")},
	{	SOCK_VECTOR, 0, N_("Local")},
	{	SOCK_VECTOR, 0, N_("View")},
	{	SOCK_VECTOR, 0, N_("Orco")},
	{	SOCK_VECTOR, 0, N_("UV")},
	{	SOCK_VECTOR, 0, N_("Normal")},
	{	SOCK_RGBA,   0, N_("Vertex Color")},
	{	SOCK_FLOAT,   0, N_("Vertex Alpha")},
	{	SOCK_FLOAT,   0, N_("Front/Back")},
	{	-1, 0, ""	}
};

/* node execute callback */
static void node_shader_exec_geom(void *data, int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **UNUSED(in), bNodeStack **out)
{
	if (data) {
		ShadeInput *shi = ((ShaderCallData *)data)->shi;
		NodeGeometry *ngeo = (NodeGeometry *)node->storage;
		ShadeInputUV *suv = &shi->uv[shi->actuv];
		static float defaultvcol[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		int i;

		if (ngeo->uvname[0]) {
			/* find uv map by name */
			for (i = 0; i < shi->totuv; i++) {
				if (strcmp(shi->uv[i].name, ngeo->uvname) == 0) {
					suv = &shi->uv[i];
					break;
				}
			}
		}

		/* out: global, local, view, orco, uv, normal, vertex color */
		copy_v3_v3(out[GEOM_OUT_GLOB]->vec, shi->gl);
		copy_v3_v3(out[GEOM_OUT_LOCAL]->vec, shi->co);
		copy_v3_v3(out[GEOM_OUT_VIEW]->vec, shi->view);
		copy_v3_v3(out[GEOM_OUT_ORCO]->vec, shi->lo);
		copy_v3_v3(out[GEOM_OUT_UV]->vec, suv->uv);
		copy_v3_v3(out[GEOM_OUT_NORMAL]->vec, shi->vno);

		if (shi->totcol) {
			/* find vertex color layer by name */
			ShadeInputCol *scol = &shi->col[0];

			if (ngeo->colname[0]) {
				for (i = 0; i < shi->totcol; i++) {
					if (strcmp(shi->col[i].name, ngeo->colname) == 0) {
						scol = &shi->col[i];
						break;
					}
				}
			}

			copy_v3_v3(out[GEOM_OUT_VCOL]->vec, scol->col);
			out[GEOM_OUT_VCOL]->vec[3] = scol->col[3];
			out[GEOM_OUT_VCOL_ALPHA]->vec[0] = scol->col[3];
		}
		else {
			memcpy(out[GEOM_OUT_VCOL]->vec, defaultvcol, sizeof(defaultvcol));
			out[GEOM_OUT_VCOL_ALPHA]->vec[0] = 1.0f;
		}
		
		if (shi->osatex) {
			out[GEOM_OUT_GLOB]->data = shi->dxgl;
			out[GEOM_OUT_GLOB]->datatype = NS_OSA_VECTORS;
			out[GEOM_OUT_LOCAL]->data = shi->dxco;
			out[GEOM_OUT_LOCAL]->datatype = NS_OSA_VECTORS;
			out[GEOM_OUT_VIEW]->data = &shi->dxview;
			out[GEOM_OUT_VIEW]->datatype = NS_OSA_VALUES;
			out[GEOM_OUT_ORCO]->data = shi->dxlo;
			out[GEOM_OUT_ORCO]->datatype = NS_OSA_VECTORS;
			out[GEOM_OUT_UV]->data = suv->dxuv;
			out[GEOM_OUT_UV]->datatype = NS_OSA_VECTORS;
			out[GEOM_OUT_NORMAL]->data = shi->dxno;
			out[GEOM_OUT_NORMAL]->datatype = NS_OSA_VECTORS;
		}
		
		/* front/back, normal flipping was stored */
		out[GEOM_OUT_FRONTBACK]->vec[0] = (shi->flippednor) ? 0.0f : 1.0f;
	}
}

static void node_shader_init_geometry(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->storage = MEM_callocN(sizeof(NodeGeometry), "NodeGeometry");
}

static int gpu_shader_geom(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	NodeGeometry *ngeo = (NodeGeometry *)node->storage;
	GPUNodeLink *orco = GPU_attribute(CD_ORCO, "");
	GPUNodeLink *mtface = GPU_attribute(CD_MTFACE, ngeo->uvname);
	GPUNodeLink *mcol = GPU_attribute(CD_MCOL, ngeo->colname);

	return GPU_stack_link(mat, "geom", in, out,
	                      GPU_builtin(GPU_VIEW_POSITION), GPU_builtin(GPU_VIEW_NORMAL),
	                      GPU_builtin(GPU_INVERSE_VIEW_MATRIX), orco, mtface, mcol);
}

/* node type definition */
void register_node_type_sh_geom(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_GEOMETRY, "Geometry", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_geom_out);
	node_type_init(&ntype, node_shader_init_geometry);
	node_type_storage(&ntype, "NodeGeometry", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_geom);
	node_type_gpu(&ntype, gpu_shader_geom);

	nodeRegisterType(&ntype);
}
