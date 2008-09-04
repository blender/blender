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

#include "DNA_customdata_types.h"

/* **************** GEOMETRY  ******************** */

/* output socket type definition */
static bNodeSocketType sh_node_geom_out[]= {
	{	SOCK_VECTOR, 0, "Global",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},	/* btw; uses no limit */
	{	SOCK_VECTOR, 0, "Local",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "View",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Orco",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "UV",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA,   0, "Vertex Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE,   0, "Front/Back", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* node execute callback */
static void node_shader_exec_geom(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		NodeGeometry *ngeo= (NodeGeometry*)node->storage;
		ShadeInputUV *suv= &shi->uv[shi->actuv];
		static float defaultvcol[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		static float front= 0.0;
		int i;

		if(ngeo->uvname[0]) {
			/* find uv layer by name */
			for(i = 0; i < shi->totuv; i++) {
				if(strcmp(shi->uv[i].name, ngeo->uvname)==0) {
					suv= &shi->uv[i];
					break;
				}
			}
		}

		/* out: global, local, view, orco, uv, normal, vertex color */
		VECCOPY(out[GEOM_OUT_GLOB]->vec, shi->gl);
		VECCOPY(out[GEOM_OUT_LOCAL]->vec, shi->co);
		VECCOPY(out[GEOM_OUT_VIEW]->vec, shi->view);
		VECCOPY(out[GEOM_OUT_ORCO]->vec, shi->lo);
		VECCOPY(out[GEOM_OUT_UV]->vec, suv->uv);
		VECCOPY(out[GEOM_OUT_NORMAL]->vec, shi->vno);

		if (shi->totcol) {
			/* find vertex color layer by name */
			ShadeInputCol *scol= &shi->col[0];

			if(ngeo->colname[0]) {
				for(i = 0; i < shi->totcol; i++) {
					if(strcmp(shi->col[i].name, ngeo->colname)==0) {
						scol= &shi->col[i];
						break;
					}
				}
			}

			VECCOPY(out[GEOM_OUT_VCOL]->vec, scol->col);
			out[GEOM_OUT_VCOL]->vec[3]= 1.0f;
		}
		else
			memcpy(out[GEOM_OUT_VCOL]->vec, defaultvcol, sizeof(defaultvcol));
		
		if(shi->osatex) {
			out[GEOM_OUT_GLOB]->data= shi->dxgl;
			out[GEOM_OUT_GLOB]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_LOCAL]->data= shi->dxco;
			out[GEOM_OUT_LOCAL]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_VIEW]->data= &shi->dxview;
			out[GEOM_OUT_VIEW]->datatype= NS_OSA_VALUES;
			out[GEOM_OUT_ORCO]->data= shi->dxlo;
			out[GEOM_OUT_ORCO]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_UV]->data= suv->dxuv;
			out[GEOM_OUT_UV]->datatype= NS_OSA_VECTORS;
			out[GEOM_OUT_NORMAL]->data= shi->dxno;
			out[GEOM_OUT_NORMAL]->datatype= NS_OSA_VECTORS;
		}
		
		/* front/back
		* check the original un-flipped normals to determine front or back side */
		if (shi->orignor[2] < FLT_EPSILON) {
			front= 1.0f;
		} else {
			front = 0.0f;
		}
		out[GEOM_OUT_FRONTBACK]->vec[0]= front;
	}
}

static void node_shader_init_geometry(bNode *node)
{
   node->storage= MEM_callocN(sizeof(NodeGeometry), "NodeGeometry");
}

static int gpu_shader_geom(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	NodeGeometry *ngeo= (NodeGeometry*)node->storage;
	GPUNodeLink *orco = GPU_attribute(CD_ORCO, "");
	GPUNodeLink *mtface = GPU_attribute(CD_MTFACE, ngeo->uvname);
	GPUNodeLink *mcol = GPU_attribute(CD_MCOL, ngeo->colname);

	return GPU_stack_link(mat, "geom", in, out,
		GPU_builtin(GPU_VIEW_POSITION), GPU_builtin(GPU_VIEW_NORMAL),
		GPU_builtin(GPU_INVERSE_VIEW_MATRIX), orco, mtface, mcol);
}

/* node type definition */
bNodeType sh_node_geom= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_GEOMETRY,
	/* name        */	"Geometry",
	/* width+range */	120, 80, 160,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	sh_node_geom_out,
	/* storage     */	"NodeGeometry",
	/* execfunc    */	node_shader_exec_geom,
	/* butfunc     */	NULL,
	/* initfunc    */	node_shader_init_geometry,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_geom
	
};
