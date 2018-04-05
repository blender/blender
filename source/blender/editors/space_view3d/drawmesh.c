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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, full update, glsl support
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawmesh.c
 *  \ingroup spview3d
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_property.h"
#include "BKE_editmesh.h"
#include "BKE_scene.h"

#include "BIF_glutil.h"

#include "UI_resources.h"

#include "GPU_draw.h"
#include "GPU_material.h"
#include "GPU_basic_shader.h"
#include "GPU_shader.h"
#include "GPU_matrix.h"

#include "RE_engine.h"

#include "ED_uvedit.h"

#include "view3d_intern.h"  /* own include */

/* user data structures for derived mesh callbacks */
typedef struct drawMeshFaceSelect_userData {
	Mesh *me;
	BLI_bitmap *edge_flags; /* pairs of edge options (visible, select) */
} drawMeshFaceSelect_userData;

/**************************** Face Select Mode *******************************/

/* mainly to be less confusing */
BLI_INLINE int edge_vis_index(const int index) { return index * 2; }
BLI_INLINE int edge_sel_index(const int index) { return index * 2 + 1; }

static BLI_bitmap *get_tface_mesh_marked_edge_info(Mesh *me, bool draw_select_edges)
{
	BLI_bitmap *bitmap_edge_flags = BLI_BITMAP_NEW(me->totedge * 2, __func__);
	MPoly *mp;
	MLoop *ml;
	int i, j;
	bool select_set;
	
	for (i = 0; i < me->totpoly; i++) {
		mp = &me->mpoly[i];

		if (!(mp->flag & ME_HIDE)) {
			select_set = (mp->flag & ME_FACE_SEL) != 0;

			ml = me->mloop + mp->loopstart;
			for (j = 0; j < mp->totloop; j++, ml++) {
				if ((draw_select_edges == false) &&
				    (select_set && BLI_BITMAP_TEST(bitmap_edge_flags, edge_sel_index(ml->e))))
				{
					BLI_BITMAP_DISABLE(bitmap_edge_flags, edge_vis_index(ml->e));
				}
				else {
					BLI_BITMAP_ENABLE(bitmap_edge_flags, edge_vis_index(ml->e));
					if (select_set) {
						BLI_BITMAP_ENABLE(bitmap_edge_flags, edge_sel_index(ml->e));
					}
				}
			}
		}
	}

	return bitmap_edge_flags;
}


static DMDrawOption draw_mesh_face_select__setHiddenOpts(void *userData, int index)
{
	drawMeshFaceSelect_userData *data = userData;
	Mesh *me = data->me;

	if (me->drawflag & ME_DRAWEDGES) {
		if ((BLI_BITMAP_TEST(data->edge_flags, edge_vis_index(index))))
			return DM_DRAW_OPTION_NORMAL;
		else
			return DM_DRAW_OPTION_SKIP;
	}
	else if (BLI_BITMAP_TEST(data->edge_flags, edge_sel_index(index)) &&
	         BLI_BITMAP_TEST(data->edge_flags, edge_vis_index(index)))
	{
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static DMDrawOption draw_mesh_face_select__setSelectOpts(void *userData, int index)
{
	drawMeshFaceSelect_userData *data = userData;
	return (BLI_BITMAP_TEST(data->edge_flags, edge_sel_index(index)) &&
	        BLI_BITMAP_TEST(data->edge_flags, edge_vis_index(index))) ? DM_DRAW_OPTION_NORMAL : DM_DRAW_OPTION_SKIP;
}

/* draws unselected */
static DMDrawOption draw_mesh_face_select__drawFaceOptsInv(void *userData, int index)
{
	Mesh *me = (Mesh *)userData;

	MPoly *mpoly = &me->mpoly[index];
	if (!(mpoly->flag & ME_HIDE) && !(mpoly->flag & ME_FACE_SEL))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

void draw_mesh_face_select(RegionView3D *rv3d, Mesh *me, DerivedMesh *dm, bool draw_select_edges)
{
	drawMeshFaceSelect_userData data;

	data.me = me;
	data.edge_flags = get_tface_mesh_marked_edge_info(me, draw_select_edges);

	glEnable(GL_DEPTH_TEST);
	ED_view3d_polygon_offset(rv3d, 1.0);

	/* Draw (Hidden) Edges */
	setlinestyle(1);
	UI_ThemeColor(TH_EDGE_FACESEL);
	dm->drawMappedEdges(dm, draw_mesh_face_select__setHiddenOpts, &data);
	setlinestyle(0);

	/* Draw Selected Faces */
	if (me->drawflag & ME_DRAWFACES) {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		/* dull unselected faces so as not to get in the way of seeing color */
		glColor4ub(96, 96, 96, 64);
		dm->drawMappedFaces(dm, draw_mesh_face_select__drawFaceOptsInv, NULL, NULL, (void *)me, DM_DRAW_SKIP_HIDDEN);
		glDisable(GL_BLEND);
	}
	
	ED_view3d_polygon_offset(rv3d, 1.0);

	/* Draw Stippled Outline for selected faces */
	glColor3ub(255, 255, 255);
	setlinestyle(1);
	dm->drawMappedEdges(dm, draw_mesh_face_select__setSelectOpts, &data);
	setlinestyle(0);

	ED_view3d_polygon_offset(rv3d, 0.0);  /* resets correctly now, even after calling accumulated offsets */

	MEM_freeN(data.edge_flags);
}

/***************************** Texture Drawing ******************************/

/* when face select is on, use face hidden flag */
static DMDrawOption wpaint__setSolidDrawOptions_facemask(void *userData, int index)
{
	Mesh *me = (Mesh *)userData;
	MPoly *mp = &me->mpoly[index];
	if (mp->flag & ME_HIDE)
		return DM_DRAW_OPTION_SKIP;
	return DM_DRAW_OPTION_NORMAL;
}

/************************** NEW SHADING NODES ********************************/

typedef struct TexMatCallback {
	Scene *scene;
	Object *ob;
	Mesh *me;
	DerivedMesh *dm;
	bool shadeless;
	bool two_sided_lighting;
} TexMatCallback;

void draw_mesh_textured(Scene *scene, ViewLayer *view_layer, View3D *v3d, RegionView3D *rv3d,
                        Object *ob, DerivedMesh *dm, const int draw_flags)
{
	UNUSED_VARS(scene, view_layer, v3d, rv3d, ob, dm, draw_flags);
	return;
}

/* Vertex Paint and Weight Paint */
static void draw_mesh_paint_light_begin(void)
{
	/* get material diffuse color from vertex colors but set default spec */
	const float specular[3] = {0.47f, 0.47f, 0.47f};
	GPU_basic_shader_colors(NULL, specular, 35, 1.0f);
	GPU_basic_shader_bind(GPU_SHADER_LIGHTING | GPU_SHADER_USE_COLOR);
}

static void draw_mesh_paint_light_end(void)
{
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
}

void draw_mesh_paint_weight_faces(DerivedMesh *dm, const bool use_light,
                                  void *facemask_cb, void *user_data)
{
	DMSetMaterial setMaterial = GPU_object_materials_check() ? GPU_object_material_bind : NULL;
	int flags = DM_DRAW_USE_COLORS;

	if (use_light) {
		draw_mesh_paint_light_begin();
		flags |= DM_DRAW_NEED_NORMALS;
	}

	dm->drawMappedFaces(dm, (DMSetDrawOptions)facemask_cb, setMaterial, NULL, user_data, flags);

	if (use_light) {
		draw_mesh_paint_light_end();
	}
}

void draw_mesh_paint_vcolor_faces(DerivedMesh *dm, const bool use_light,
                                  void *facemask_cb, void *user_data,
                                  const Mesh *me)
{
	DMSetMaterial setMaterial = GPU_object_materials_check() ? GPU_object_material_bind : NULL;
	int flags = 0;

	if (use_light) {
		draw_mesh_paint_light_begin();
		flags |= DM_DRAW_NEED_NORMALS;
	}

	if (me->mloopcol) {
		dm->drawMappedFaces(dm, facemask_cb, setMaterial, NULL, user_data,
		                    DM_DRAW_USE_COLORS | flags);
	}
	else {
		glColor3f(1.0f, 1.0f, 1.0f);
		dm->drawMappedFaces(dm, facemask_cb, setMaterial, NULL, user_data, flags);
	}

	if (use_light) {
		draw_mesh_paint_light_end();
	}
}

void draw_mesh_paint_weight_edges(RegionView3D *rv3d, DerivedMesh *dm,
                                  const bool use_depth, const bool use_alpha,
                                  void *edgemask_cb, void *user_data)
{
	/* weight paint in solid mode, special case. focus on making the weights clear
	 * rather than the shading, this is also forced in wire view */

	if (use_depth) {
		ED_view3d_polygon_offset(rv3d, 1.0);
		glDepthMask(0);  /* disable write in zbuffer, selected edge wires show better */
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}

	if (use_alpha) {
		glEnable(GL_BLEND);
	}

	glColor4ub(255, 255, 255, 96);
	GPU_basic_shader_bind_enable(GPU_SHADER_LINE | GPU_SHADER_STIPPLE);
	GPU_basic_shader_line_stipple(1, 0xAAAA);

	dm->drawMappedEdges(dm, (DMSetDrawOptions)edgemask_cb, user_data);

	if (use_depth) {
		ED_view3d_polygon_offset(rv3d, 0.0);
		glDepthMask(1);
	}
	else {
		glEnable(GL_DEPTH_TEST);
	}

	GPU_basic_shader_bind_disable(GPU_SHADER_LINE | GPU_SHADER_STIPPLE);

	if (use_alpha) {
		glDisable(GL_BLEND);
	}
}

void draw_mesh_paint(View3D *v3d, RegionView3D *rv3d,
                     Object *ob, DerivedMesh *dm, const int draw_flags)
{
	DMSetDrawOptions facemask = NULL;
	Mesh *me = ob->data;
	const bool use_light = (v3d->drawtype >= OB_SOLID);

	/* hide faces in face select mode */
	if (me->editflag & (ME_EDIT_PAINT_VERT_SEL | ME_EDIT_PAINT_FACE_SEL))
		facemask = wpaint__setSolidDrawOptions_facemask;

	if (ob->mode & OB_MODE_WEIGHT_PAINT) {
		draw_mesh_paint_weight_faces(dm, use_light, facemask, me);
	}
	else if (ob->mode & OB_MODE_VERTEX_PAINT) {
		draw_mesh_paint_vcolor_faces(dm, use_light, facemask, me, me);
	}

	/* draw face selection on top */
	if (draw_flags & DRAW_FACE_SELECT) {
		bool draw_select_edges = (ob->mode & OB_MODE_TEXTURE_PAINT) == 0;
		draw_mesh_face_select(rv3d, me, dm, draw_select_edges);
	}
	else if ((use_light == false) || (ob->dtx & OB_DRAWWIRE)) {
		const bool use_depth = (v3d->flag & V3D_ZBUF_SELECT) || !(ob->mode & OB_MODE_WEIGHT_PAINT);
		const bool use_alpha = (ob->mode & OB_MODE_VERTEX_PAINT) == 0;

		if (use_alpha == false) {
			set_inverted_drawing(1);
		}

		draw_mesh_paint_weight_edges(rv3d, dm, use_depth, use_alpha, NULL, NULL);

		if (use_alpha == false) {
			set_inverted_drawing(0);
		}
	}
}

