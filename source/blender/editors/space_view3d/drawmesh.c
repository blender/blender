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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"

#include "GPU_draw.h"
#include "GPU_material.h"
#include "GPU_basic_shader.h"
#include "GPU_shader.h"

#include "RE_engine.h"

#include "ED_uvedit.h"

#include "view3d_intern.h"  /* own include */

/* user data structures for derived mesh callbacks */
typedef struct drawMeshFaceSelect_userData {
	Mesh *me;
	BLI_bitmap *edge_flags; /* pairs of edge options (visible, select) */
} drawMeshFaceSelect_userData;

typedef struct drawEMTFMapped_userData {
	BMEditMesh *em;
	bool has_mcol;
	int cd_poly_tex_offset;
	const MPoly *mpoly;
	const MTexPoly *mtexpoly;
} drawEMTFMapped_userData;

typedef struct drawTFace_userData {
	const Mesh *me;
	const MPoly *mpoly;
	const MTexPoly *mtexpoly;
} drawTFace_userData;

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
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

static Material *give_current_material_or_def(Object *ob, int matnr)
{
	extern Material defmaterial;  /* render module abuse... */
	Material *ma = give_current_material(ob, matnr);

	return ma ? ma : &defmaterial;
}

/* Icky globals, fix with userdata parameter */

static struct TextureDrawState {
	Object *ob;
	Image *stencil; /* texture painting stencil */
	Image *canvas;  /* texture painting canvas, for image mode */
	bool use_game_mat;
	int is_lit, is_tex;
	int color_profile;
	bool use_backface_culling;
	bool two_sided_lighting;
	unsigned char obcol[4];
	bool is_texpaint;
	bool texpaint_material; /* use material slots for texture painting */
} Gtexdraw = {NULL, NULL, NULL, false, 0, 0, 0, false, false, {0, 0, 0, 0}, false, false};

static bool set_draw_settings_cached(
        int clearcache, MTexPoly *texface, Material *ma,
        const struct TextureDrawState *gtexdraw)
{
	static Material *c_ma;
	static int c_textured;
	static MTexPoly c_texface;
	static int c_backculled;
	static bool c_badtex;
	static int c_lit;
	static int c_has_texface;

	int backculled = 1;
	int alphablend = GPU_BLEND_SOLID;
	int textured = 0;
	int lit = 0;
	int has_texface = texface != NULL;
	bool need_set_tpage = false;
	bool texpaint = ((gtexdraw->ob->mode & OB_MODE_TEXTURE_PAINT) != 0);

	Image *ima = NULL;

	if (ma != NULL) {
		if (ma->mode & MA_TRANSP) {
			alphablend = GPU_BLEND_ALPHA;
		}
	}

	if (clearcache) {
		c_textured = c_lit = c_backculled = -1;
		memset(&c_texface, 0, sizeof(c_texface));
		c_badtex = false;
		c_has_texface = -1;
		c_ma = NULL;
	}
	else {
		textured = gtexdraw->is_tex;
	}

	/* convert number of lights into boolean */
	if (gtexdraw->is_lit) {
		lit = 1;
	}

	backculled = gtexdraw->use_backface_culling;
	if (ma) {
		if (ma->mode & MA_SHLESS) lit = 0;
		if (gtexdraw->use_game_mat) {
			backculled = backculled || (ma->game.flag & GEMAT_BACKCULL);
			alphablend = ma->game.alpha_blend;
		}
	}

	if (texface && !texpaint) {
		textured = textured && (texface->tpage);

		/* no material, render alpha if texture has depth=32 */
		if (!ma && BKE_image_has_alpha(texface->tpage))
			alphablend = GPU_BLEND_ALPHA;
	}
	else if (texpaint) {
		if (gtexdraw->texpaint_material)
			ima = ma && ma->texpaintslot ? ma->texpaintslot[ma->paint_active_slot].ima : NULL;
		else
			ima = gtexdraw->canvas;
	}
	else
		textured = 0;

	if (backculled != c_backculled) {
		if (backculled) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);

		c_backculled = backculled;
	}

	/* need to re-set tpage if textured flag changed or existsment of texface changed..  */
	need_set_tpage = textured != c_textured || has_texface != c_has_texface;
	/* ..or if settings inside texface were changed (if texface was used) */
	need_set_tpage |= (texpaint && c_ma != ma) || (texface && memcmp(&c_texface, texface, sizeof(c_texface)));

	if (need_set_tpage) {
		if (textured) {
			if (texpaint) {
				c_badtex = false;
				if (GPU_verify_image(ima, NULL, GL_TEXTURE_2D, 0, 1, 0, false)) {
					glEnable(GL_TEXTURE_2D);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
					
					glActiveTexture(GL_TEXTURE1);
					glEnable(GL_TEXTURE_2D);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_PREVIOUS);
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
					glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);
					glActiveTexture(GL_TEXTURE0);					
				}
				else {
					glActiveTexture(GL_TEXTURE1);
					glDisable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, 0);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					glActiveTexture(GL_TEXTURE0);									

					c_badtex = true;
					GPU_clear_tpage(true);
					glDisable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, 0);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				}
			}
			else {
				c_badtex = !GPU_set_tpage(texface, !texpaint, alphablend);
			}
		}
		else {
			GPU_set_tpage(NULL, 0, 0);
			c_badtex = false;
		}
		c_textured = textured;
		c_has_texface = has_texface;
		if (texface)
			memcpy(&c_texface, texface, sizeof(c_texface));
	}

	if (c_badtex) lit = 0;
	if (lit != c_lit || ma != c_ma || textured != c_textured) {
		int options = GPU_SHADER_USE_COLOR;

		if (c_textured && !c_badtex) {
			options |= GPU_SHADER_TEXTURE_2D;
		}
		if (gtexdraw->two_sided_lighting) {
			options |= GPU_SHADER_TWO_SIDED;
		}

		if (lit) {
			options |= GPU_SHADER_LIGHTING;
			if (!ma)
				ma = give_current_material_or_def(NULL, 0);  /* default material */

			float specular[3];
			mul_v3_v3fl(specular, &ma->specr, ma->spec);

			GPU_basic_shader_colors(NULL, specular, ma->har, 1.0f);
		}

		GPU_basic_shader_bind(options);

		c_lit = lit;
		c_ma = ma;
	}

	return c_badtex;
}

static void draw_textured_begin(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob)
{
	unsigned char obcol[4];
	bool is_tex, solidtex;
	Mesh *me = ob->data;
	ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;

	/* XXX scene->obedit warning */

	/* texture draw is abused for mask selection mode, do this so wire draw
	 * with face selection in weight paint is not lit. */
	if ((v3d->drawtype <= OB_WIRE) && (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT))) {
		solidtex = false;
		Gtexdraw.is_lit = 0;
	}
	else if ((ob->mode & OB_MODE_TEXTURE_PAINT) && BKE_scene_use_new_shading_nodes(scene)) {
		solidtex = true;
		if (v3d->flag2 & V3D_SHADELESS_TEX)
			Gtexdraw.is_lit = 0;
		else
			Gtexdraw.is_lit = -1;
	}
	else if ((v3d->drawtype == OB_SOLID) ||
	         ((ob->mode & OB_MODE_EDIT) && (v3d->drawtype != OB_TEXTURE)))
	{
		/* draw with default lights in solid draw mode and edit mode */
		solidtex = true;
		Gtexdraw.is_lit = -1;
	}
	else {
		/* draw with lights in the scene otherwise */
		solidtex = false;
		if (v3d->flag2 & V3D_SHADELESS_TEX) {
			Gtexdraw.is_lit = 0;
		}
		else {
			Gtexdraw.is_lit = GPU_scene_object_lights(
			                      scene, ob, v3d->localvd ? v3d->localvd->lay : v3d->lay,
			                      rv3d->viewmat, !rv3d->is_persp);
		}
	}

	rgba_float_to_uchar(obcol, ob->col);

	if (solidtex || v3d->drawtype == OB_TEXTURE) is_tex = true;
	else is_tex = false;

	Gtexdraw.ob = ob;
	Gtexdraw.stencil = (imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL) ? imapaint->stencil : NULL;
	Gtexdraw.is_texpaint = (ob->mode == OB_MODE_TEXTURE_PAINT);
	Gtexdraw.texpaint_material = (imapaint->mode == IMAGEPAINT_MODE_MATERIAL);
	Gtexdraw.canvas = (Gtexdraw.texpaint_material) ? NULL : imapaint->canvas;
	Gtexdraw.is_tex = is_tex;

	/* naughty multitexturing hacks to quickly support stencil + shading + alpha blending 
	 * in new texpaint code. The better solution here would be to support GLSL */
	if (Gtexdraw.is_texpaint) {			
		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
		glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_PREVIOUS);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
		
		/* load the stencil texture here */
		if (Gtexdraw.stencil != NULL) {
			glActiveTexture(GL_TEXTURE2);
			if (GPU_verify_image(Gtexdraw.stencil, NULL, GL_TEXTURE_2D, false, false, false, false)) {
				float col[4] = {imapaint->stencil_col[0], imapaint->stencil_col[1], imapaint->stencil_col[2], 1.0f};
				glEnable(GL_TEXTURE_2D);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_TEXTURE);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE);
				glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, col);
				if ((imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) == 0) {
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_ONE_MINUS_SRC_COLOR);
				}
				else {
					glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
				}
			}
		}
		glActiveTexture(GL_TEXTURE0);
	}
	
	Gtexdraw.color_profile = BKE_scene_check_color_management_enabled(scene);
	Gtexdraw.use_game_mat = (RE_engines_find(scene->r.engine)->flag & RE_GAME) != 0;
	Gtexdraw.use_backface_culling = (v3d->flag2 & V3D_BACKFACE_CULLING) != 0;
	Gtexdraw.two_sided_lighting = (me->flag & ME_TWOSIDED);

	memcpy(Gtexdraw.obcol, obcol, sizeof(obcol));
	set_draw_settings_cached(1, NULL, NULL, &Gtexdraw);
	glCullFace(GL_BACK);
}

static void draw_textured_end(void)
{
	if (Gtexdraw.ob->mode & OB_MODE_TEXTURE_PAINT) {
		glActiveTexture(GL_TEXTURE1);
		glDisable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBindTexture(GL_TEXTURE_2D, 0);

		if (Gtexdraw.stencil != NULL) {
			glActiveTexture(GL_TEXTURE2);
			glDisable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}		
		glActiveTexture(GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		/* manual reset, since we don't use tpage */
		glBindTexture(GL_TEXTURE_2D, 0);
		/* force switch off textures */
		GPU_clear_tpage(true);
	}
	else {
		/* switch off textures */
		GPU_set_tpage(NULL, 0, 0);
	}

	glDisable(GL_CULL_FACE);
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	/* XXX, bad patch - GPU_default_lights() calls
	 * glLightfv(GL_POSITION, ...) which
	 * is transformed by the current matrix... we
	 * need to make sure that matrix is identity.
	 * 
	 * It would be better if drawmesh.c kept track
	 * of and restored the light settings it changed.
	 *  - zr
	 */
	glPushMatrix();
	glLoadIdentity();
	GPU_default_lights();
	glPopMatrix();
}

static DMDrawOption draw_tface__set_draw_legacy(MTexPoly *mtexpoly, const bool has_mcol, int matnr)
{
	Material *ma = give_current_material(Gtexdraw.ob, matnr + 1);
	bool invalidtexture = false;

	if (ma && (ma->game.flag & GEMAT_INVISIBLE))
		return DM_DRAW_OPTION_SKIP;

	invalidtexture = set_draw_settings_cached(0, mtexpoly, ma, &Gtexdraw);

	if (mtexpoly && invalidtexture) {
		glColor3ub(0xFF, 0x00, 0xFF);
		return DM_DRAW_OPTION_NO_MCOL; /* Don't set color */
	}
	else if (!has_mcol) {
		if (mtexpoly) {
			glColor3f(1.0, 1.0, 1.0);
		}
		else {
			if (ma) {
				if (ma->shade_flag & MA_OBCOLOR) {
					glColor3ubv(Gtexdraw.obcol);
				}
				else {
					float col[3];
					if (Gtexdraw.color_profile) linearrgb_to_srgb_v3_v3(col, &ma->r);
					else copy_v3_v3(col, &ma->r);

					glColor3fv(col);
				}
			}
			else {
				glColor3f(1.0, 1.0, 1.0);
			}
		}
		return DM_DRAW_OPTION_NORMAL; /* normal drawing (no mcols anyway, no need to turn off) */
	}
	else {
		return DM_DRAW_OPTION_NORMAL; /* Set color from mcol */
	}
}

static DMDrawOption draw_tface__set_draw(MTexPoly *mtexpoly, const bool UNUSED(has_mcol), int matnr)
{
	Material *ma = give_current_material(Gtexdraw.ob, matnr + 1);

	if (ma && (ma->game.flag & GEMAT_INVISIBLE)) return DM_DRAW_OPTION_SKIP;

	if (mtexpoly || Gtexdraw.is_texpaint)
		set_draw_settings_cached(0, mtexpoly, ma, &Gtexdraw);

	/* always use color from mcol, as set in update_tface_color_layer */
	return DM_DRAW_OPTION_NORMAL;
}

static void update_tface_color_layer(DerivedMesh *dm, bool use_mcol)
{
	const MPoly *mp = dm->getPolyArray(dm);
	const int mpoly_num = dm->getNumPolys(dm);
	MTexPoly *mtexpoly = DM_get_poly_data_layer(dm, CD_MTEXPOLY);
	MLoopCol *finalCol;
	int i, j;
	MLoopCol *mloopcol = NULL;

	/* cache material values to avoid a lot of lookups */
	Material *ma = NULL;
	short mat_nr_prev = -1;
	enum {
		COPY_CALC,
		COPY_ORIG,
		COPY_PREV,
	} copy_mode = COPY_CALC;

	if (use_mcol) {
		mloopcol = dm->getLoopDataArray(dm, CD_PREVIEW_MLOOPCOL);
		if (!mloopcol)
			mloopcol = dm->getLoopDataArray(dm, CD_MLOOPCOL);
	}

	if (CustomData_has_layer(&dm->loopData, CD_TEXTURE_MLOOPCOL)) {
		finalCol = CustomData_get_layer(&dm->loopData, CD_TEXTURE_MLOOPCOL);
	}
	else {
		finalCol = MEM_mallocN(sizeof(MLoopCol) * dm->numLoopData, "add_tface_color_layer");
		CustomData_add_layer(&dm->loopData, CD_TEXTURE_MLOOPCOL, CD_ASSIGN, finalCol, dm->numLoopData);
	}

	for (i = mpoly_num; i--; mp++) {
		const short mat_nr = mp->mat_nr;

		if (UNLIKELY(mat_nr_prev != mat_nr)) {
			ma = give_current_material(Gtexdraw.ob, mat_nr + 1);
			copy_mode = COPY_CALC;
			mat_nr_prev = mat_nr;
		}

		/* avoid lookups  */
		if (copy_mode == COPY_ORIG) {
			memcpy(&finalCol[mp->loopstart], &mloopcol[mp->loopstart], sizeof(*finalCol) * mp->totloop);
		}
		else if (copy_mode == COPY_PREV) {
			int loop_index = mp->loopstart;
			const MLoopCol *lcol_prev = &finalCol[(mp - 1)->loopstart];
			for (j = 0; j < mp->totloop; j++, loop_index++) {
				finalCol[loop_index] = *lcol_prev;
			}
		}

		/* (copy_mode == COPY_CALC) */
		else if (ma && (ma->game.flag & GEMAT_INVISIBLE)) {
			if (mloopcol) {
				memcpy(&finalCol[mp->loopstart], &mloopcol[mp->loopstart], sizeof(*finalCol) * mp->totloop);
				copy_mode = COPY_ORIG;
			}
			else {
				memset(&finalCol[mp->loopstart], 0xff, sizeof(*finalCol) * mp->totloop);
				copy_mode = COPY_PREV;
			}
		}
		else if (mtexpoly && set_draw_settings_cached(0, mtexpoly, ma, &Gtexdraw)) {
			int loop_index = mp->loopstart;
			for (j = 0; j < mp->totloop; j++, loop_index++) {
				finalCol[loop_index].r = 255;
				finalCol[loop_index].g = 0;
				finalCol[loop_index].b = 255;
				finalCol[loop_index].a = 255;
			}
			copy_mode = COPY_PREV;
		}
		else if (ma && (ma->shade_flag & MA_OBCOLOR)) {
			int loop_index = mp->loopstart;
			for (j = 0; j < mp->totloop; j++, loop_index++) {
				copy_v3_v3_uchar(&finalCol[loop_index].r, Gtexdraw.obcol);
				finalCol[loop_index].a = 255;
			}
			copy_mode = COPY_PREV;
		}
		else {
			if (mloopcol) {
				memcpy(&finalCol[mp->loopstart], &mloopcol[mp->loopstart], sizeof(*finalCol) * mp->totloop);
				copy_mode = COPY_ORIG;
			}
			else if (mtexpoly) {
				memset(&finalCol[mp->loopstart], 0xff, sizeof(*finalCol) * mp->totloop);
				copy_mode = COPY_PREV;
			}
			else {
				float col[3];

				if (ma) {
					int loop_index = mp->loopstart;
					MLoopCol lcol;

					if (Gtexdraw.color_profile) linearrgb_to_srgb_v3_v3(col, &ma->r);
					else copy_v3_v3(col, &ma->r);
					rgb_float_to_uchar((unsigned char *)&lcol.r, col);
					lcol.a = 255;
					
					for (j = 0; j < mp->totloop; j++, loop_index++) {
						finalCol[loop_index] = lcol;
					}
				}
				else {
					memset(&finalCol[mp->loopstart], 0xff, sizeof(*finalCol) * mp->totloop);
				}
				copy_mode = COPY_PREV;
			}
		}
	}

	dm->dirty |= DM_DIRTY_MCOL_UPDATE_DRAW;
}

static DMDrawOption draw_tface_mapped__set_draw(void *userData, int origindex, int UNUSED(mat_nr))
{
	const Mesh *me = ((drawTFace_userData *)userData)->me;

	/* array checked for NULL before calling */
	MPoly *mpoly = &me->mpoly[origindex];

	BLI_assert(origindex >= 0 && origindex < me->totpoly);

	if (mpoly->flag & ME_HIDE) {
		return DM_DRAW_OPTION_SKIP;
	}
	else {
		MTexPoly *tpoly = (me->mtpoly) ? &me->mtpoly[origindex] : NULL;
		int matnr = mpoly->mat_nr;
		
		return draw_tface__set_draw(tpoly, (me->mloopcol != NULL), matnr);
	}
}

static DMDrawOption draw_em_tf_mapped__set_draw(void *userData, int origindex, int mat_nr)
{
	drawEMTFMapped_userData *data = userData;
	BMEditMesh *em = data->em;
	BMFace *efa;

	if (UNLIKELY(origindex >= em->bm->totface))
		return DM_DRAW_OPTION_NORMAL;

	efa = BM_face_at_index(em->bm, origindex);

	if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return DM_DRAW_OPTION_SKIP;
	}
	else {
		MTexPoly *mtexpoly = (data->cd_poly_tex_offset != -1) ?
		        BM_ELEM_CD_GET_VOID_P(efa, data->cd_poly_tex_offset) : NULL;
		int matnr = (mat_nr != -1) ? mat_nr : efa->mat_nr;

		return draw_tface__set_draw_legacy(mtexpoly, data->has_mcol, matnr);
	}
}

/* when face select is on, use face hidden flag */
static DMDrawOption wpaint__setSolidDrawOptions_facemask(void *userData, int index)
{
	Mesh *me = (Mesh *)userData;
	MPoly *mp = &me->mpoly[index];
	if (mp->flag & ME_HIDE)
		return DM_DRAW_OPTION_SKIP;
	return DM_DRAW_OPTION_NORMAL;
}

static void draw_mesh_text(Scene *scene, Object *ob, int glsl)
{
	Mesh *me = ob->data;
	DerivedMesh *ddm;
	MPoly *mp, *mface  = me->mpoly;
	MTexPoly *mtpoly   = me->mtpoly;
	MLoopUV *mloopuv   = me->mloopuv;
	MLoopUV *luv;
	MLoopCol *mloopcol = me->mloopcol;  /* why does mcol exist? */
	MLoopCol *lcol;

	bProperty *prop = BKE_bproperty_object_get(ob, "Text");
	GPUVertexAttribs gattribs;
	int a, totpoly = me->totpoly;

	/* fake values to pass to GPU_render_text() */
	MCol  tmp_mcol[4]  = {{0}};
	MCol *tmp_mcol_pt  = mloopcol ? tmp_mcol : NULL;

	/* don't draw without tfaces */
	if (!mtpoly || !mloopuv)
		return;

	/* don't draw when editing */
	if (ob->mode & OB_MODE_EDIT)
		return;
	else if (ob == OBACT)
		if (BKE_paint_select_elem_test(ob))
			return;

	ddm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);

	for (a = 0, mp = mface; a < totpoly; a++, mtpoly++, mp++) {
		short matnr = mp->mat_nr;
		const bool mf_smooth = (mp->flag & ME_SMOOTH) != 0;
		Material *mat = (me->mat) ? me->mat[matnr] : NULL;
		int mode = mat ? mat->game.flag : GEMAT_INVISIBLE;


		if (!(mode & GEMAT_INVISIBLE) && (mode & GEMAT_TEXT) && mp->totloop >= 3) {
			/* get the polygon as a tri/quad */
			int mp_vi[4];
			float   v_quad_data[4][3];
			const float  *v_quad[4];
			const float *uv_quad[4];
			char string[MAX_PROPSTRING];
			int characters, i, glattrib = -1, badtex = 0;


			/* TEXFACE */
			if (glsl) {
				GPU_object_material_bind(matnr + 1, &gattribs);

				for (i = 0; i < gattribs.totlayer; i++) {
					if (gattribs.layer[i].type == CD_MTFACE) {
						glattrib = gattribs.layer[i].glindex;
						break;
					}
				}
			}
			else {
				badtex = set_draw_settings_cached(0, mtpoly, mat, &Gtexdraw);
				if (badtex) {
					continue;
				}
			}

			mp_vi[0] = me->mloop[mp->loopstart + 0].v;
			mp_vi[1] = me->mloop[mp->loopstart + 1].v;
			mp_vi[2] = me->mloop[mp->loopstart + 2].v;
			mp_vi[3] = (mp->totloop >= 4) ? me->mloop[mp->loopstart + 3].v : 0;

			/* UV */
			luv = &mloopuv[mp->loopstart];
			uv_quad[0] = luv->uv; luv++;
			uv_quad[1] = luv->uv; luv++;
			uv_quad[2] = luv->uv; luv++;
			if (mp->totloop >= 4) {
				uv_quad[3] = luv->uv;
			}
			else {
				uv_quad[3] = NULL;
			}


			/* COLOR */
			if (mloopcol) {
				unsigned int totloop_clamp = min_ii(4, mp->totloop);
				unsigned int j;
				lcol = &mloopcol[mp->loopstart];

				for (j = 0; j < totloop_clamp; j++, lcol++) {
					MESH_MLOOPCOL_TO_MCOL(lcol, &tmp_mcol[j]);
				}
			}

			/* LOCATION */
			ddm->getVertCo(ddm, mp_vi[0], v_quad_data[0]);
			ddm->getVertCo(ddm, mp_vi[1], v_quad_data[1]);
			ddm->getVertCo(ddm, mp_vi[2], v_quad_data[2]);
			if (mp->totloop >= 4) {
				ddm->getVertCo(ddm, mp_vi[3], v_quad_data[3]);
			}

			v_quad[0] = v_quad_data[0];
			v_quad[1] = v_quad_data[1];
			v_quad[2] = v_quad_data[2];
			if (mp->totloop >= 4) {
				v_quad[3] = v_quad_data[2];
			}
			else {
				v_quad[3] = NULL;
			}


			/* The BM_FONT handling is in the gpu module, shared with the
			 * game engine, was duplicated previously */

			BKE_bproperty_set_valstr(prop, string);
			characters = strlen(string);
			
			if (!BKE_image_has_ibuf(mtpoly->tpage, NULL))
				characters = 0;

			if (!mf_smooth) {
				float nor[3];

				normal_tri_v3(nor, v_quad[0], v_quad[1], v_quad[2]);

				glNormal3fv(nor);
			}

			GPU_render_text(
			        mtpoly, mode, string, characters,
			        (unsigned int *)tmp_mcol_pt,
			        v_quad, uv_quad,
			        glattrib);
		}
	}

	ddm->release(ddm);
}

static int compareDrawOptions(void *userData, int cur_index, int next_index)
{
	drawTFace_userData *data = userData;

	if (data->mpoly && data->mpoly[cur_index].mat_nr != data->mpoly[next_index].mat_nr)
		return 0;

	if (data->mtexpoly && data->mtexpoly[cur_index].tpage != data->mtexpoly[next_index].tpage)
		return 0;

	return 1;
}


static int compareDrawOptionsEm(void *userData, int cur_index, int next_index)
{
	drawEMTFMapped_userData *data = userData;

	if (data->mpoly && data->mpoly[cur_index].mat_nr != data->mpoly[next_index].mat_nr)
		return 0;

	if (data->mtexpoly && data->mtexpoly[cur_index].tpage != data->mtexpoly[next_index].tpage)
		return 0;

	return 1;
}

static void draw_mesh_textured_old(Scene *scene, View3D *v3d, RegionView3D *rv3d,
                                   Object *ob, DerivedMesh *dm, const int draw_flags)
{
	Mesh *me = ob->data;

	/* correct for negative scale */
	if (ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	/* draw the textured mesh */
	draw_textured_begin(scene, v3d, rv3d, ob);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (ob->mode & OB_MODE_EDIT) {
		drawEMTFMapped_userData data;

		data.em = me->edit_btmesh;
		data.has_mcol = CustomData_has_layer(&me->edit_btmesh->bm->ldata, CD_MLOOPCOL);
		data.cd_poly_tex_offset = CustomData_get_offset(&me->edit_btmesh->bm->pdata, CD_MTEXPOLY);

		data.mpoly = DM_get_poly_data_layer(dm, CD_MPOLY);
		data.mtexpoly = DM_get_poly_data_layer(dm, CD_MTEXPOLY);

		dm->drawMappedFacesTex(dm, draw_em_tf_mapped__set_draw, compareDrawOptionsEm, &data, 0);
	}
	else {
		DMDrawFlag dm_draw_flag;
		drawTFace_userData userData;

		if (ob->mode & OB_MODE_TEXTURE_PAINT) {
			dm_draw_flag = DM_DRAW_USE_TEXPAINT_UV;
		}
		else {
			dm_draw_flag = DM_DRAW_USE_ACTIVE_UV;
		}

		if (ob == OBACT) {
			if (ob->mode & OB_MODE_WEIGHT_PAINT) {
				dm_draw_flag |= DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH | DM_DRAW_SKIP_HIDDEN;
			}
			else if (ob->mode & OB_MODE_SCULPT) {
				dm_draw_flag |= DM_DRAW_SKIP_HIDDEN | DM_DRAW_USE_COLORS;
			}
			else if ((ob->mode & OB_MODE_TEXTURE_PAINT) == 0) {
				dm_draw_flag |= DM_DRAW_USE_COLORS;
			}
		}
		else {
			if ((ob->mode & OB_MODE_TEXTURE_PAINT) == 0) {
				dm_draw_flag |= DM_DRAW_USE_COLORS;
			}
		}


		userData.mpoly = DM_get_poly_data_layer(dm, CD_MPOLY);
		userData.mtexpoly = DM_get_poly_data_layer(dm, CD_MTEXPOLY);

		if (draw_flags & DRAW_FACE_SELECT) {
			userData.me = me;

			dm->drawMappedFacesTex(
			        dm, me->mpoly ? draw_tface_mapped__set_draw : NULL, compareDrawOptions,
			        &userData, dm_draw_flag);
		}
		else {
			userData.me = NULL;

			/* if ((ob->mode & OB_MODE_ALL_PAINT) == 0) */ {

				/* Note: this isn't efficient and runs on every redraw,
				 * its needed so material colors are used for vertex colors.
				 * In the future we will likely remove 'texface' so, just avoid running this where possible,
				 * (when vertex paint or weight paint are used).
				 *
				 * Note 2: We disable optimization for now since it causes T48788
				 * and it is now too close to release to do something smarter.
				 *
				 * TODO(sergey): Find some real solution here.
				 */

				update_tface_color_layer(dm, !(ob->mode & OB_MODE_TEXTURE_PAINT));
			}

			dm->drawFacesTex(
			        dm, draw_tface__set_draw, compareDrawOptions,
			        &userData, dm_draw_flag);
		}
	}

	/* draw game engine text hack */
	if (rv3d->rflag & RV3D_IS_GAME_ENGINE) {
		if (BKE_bproperty_object_get(ob, "Text")) {
			draw_mesh_text(scene, ob, 0);
		}
	}

	draw_textured_end();
	
	/* draw edges and selected faces over textured mesh */
	if (!(ob == scene->obedit) && (draw_flags & DRAW_FACE_SELECT)) {
		bool draw_select_edges = (ob->mode & OB_MODE_TEXTURE_PAINT) == 0;
		draw_mesh_face_select(rv3d, me, dm, draw_select_edges);
	}

	/* reset from negative scale correction */
	glFrontFace(GL_CCW);
	
	/* in editmode, the blend mode needs to be set in case it was ADD */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

static void tex_mat_set_material_cb(void *UNUSED(userData), int mat_nr, void *attribs)
{
	/* all we have to do here is simply enable the GLSL material, but note
	 * that the GLSL code will give different result depending on the drawtype,
	 * in texture draw mode it will output the active texture node, in material
	 * draw mode it will show the full material. */
	GPU_object_material_bind(mat_nr, attribs);
}

static void tex_mat_set_texture_cb(void *userData, int mat_nr, void *attribs)
{
	/* texture draw mode without GLSL */
	TexMatCallback *data = (TexMatCallback *)userData;
	GPUVertexAttribs *gattribs = attribs;
	Image *ima;
	ImageUser *iuser;
	bNode *node;

	/* draw image texture if we find one */
	if (ED_object_get_active_image(data->ob, mat_nr, &ima, &iuser, &node, NULL)) {
		/* get openl texture */
		int mipmap = 1;
		int bindcode = (ima) ? GPU_verify_image(ima, iuser, GL_TEXTURE_2D, 0, 0, mipmap, false) : 0;

		if (bindcode) {
			NodeTexBase *texbase = node->storage;

			/* disable existing material */
			GPU_object_material_unbind();

			/* bind texture */
			glBindTexture(GL_TEXTURE_2D, ima->bindcode[TEXTARGET_TEXTURE_2D]);

			glMatrixMode(GL_TEXTURE);
			glLoadMatrixf(texbase->tex_mapping.mat);
			glMatrixMode(GL_MODELVIEW);

			/* use active UV texture layer */
			memset(gattribs, 0, sizeof(*gattribs));

			gattribs->layer[0].type = CD_MTFACE;
			gattribs->layer[0].name[0] = '\0';
			gattribs->layer[0].gltexco = 1;
			gattribs->totlayer = 1;

			/* bind material */
			float diffuse[3] = {1.0f, 1.0f, 1.0f};

			int options = GPU_SHADER_TEXTURE_2D;
			if (!data->shadeless)
				options |= GPU_SHADER_LIGHTING;
			if (data->two_sided_lighting)
				options |= GPU_SHADER_TWO_SIDED;

			GPU_basic_shader_colors(diffuse, NULL, 0, 1.0f);
			GPU_basic_shader_bind(options);

			return;
		}
	}

	/* disable texture material */
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	if (data->shadeless) {
		glColor3f(1.0f, 1.0f, 1.0f);
		memset(gattribs, 0, sizeof(*gattribs));
	}
	else {
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);

		/* enable solid material */
		GPU_object_material_bind(mat_nr, attribs);
	}
}

static bool tex_mat_set_face_mesh_cb(void *userData, int index)
{
	/* faceselect mode face hiding */
	TexMatCallback *data = (TexMatCallback *)userData;
	Mesh *me = (Mesh *)data->me;
	MPoly *mp = &me->mpoly[index];

	return !(mp->flag & ME_HIDE);
}

static bool tex_mat_set_face_editmesh_cb(void *userData, int index)
{
	/* editmode face hiding */
	TexMatCallback *data = (TexMatCallback *)userData;
	Mesh *me = (Mesh *)data->me;
	BMEditMesh *em = me->edit_btmesh;
	BMFace *efa;

	if (UNLIKELY(index >= em->bm->totface))
		return DM_DRAW_OPTION_NORMAL;

	efa = BM_face_at_index(em->bm, index);

	return !BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
}

void draw_mesh_textured(Scene *scene, View3D *v3d, RegionView3D *rv3d,
                        Object *ob, DerivedMesh *dm, const int draw_flags)
{
	/* if not cycles, or preview-modifiers, or drawing matcaps */
	if ((draw_flags & DRAW_MODIFIERS_PREVIEW) ||
	    (v3d->flag2 & V3D_SHOW_SOLID_MATCAP) ||
	    (BKE_scene_use_new_shading_nodes(scene) == false) ||
	    ((ob->mode & OB_MODE_TEXTURE_PAINT) && ELEM(v3d->drawtype, OB_TEXTURE, OB_SOLID)))
	{
		draw_mesh_textured_old(scene, v3d, rv3d, ob, dm, draw_flags);
		return;
	}
	else if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
		draw_mesh_paint(v3d, rv3d, ob, dm, draw_flags);
		return;
	}

	/* set opengl state for negative scale & color */
	if (ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);

	Mesh *me = ob->data;

	bool shadeless = ((v3d->flag2 & V3D_SHADELESS_TEX) &&
	    ((v3d->drawtype == OB_TEXTURE) || (ob->mode & OB_MODE_TEXTURE_PAINT)));
	bool two_sided_lighting = (me->flag & ME_TWOSIDED) != 0;

	TexMatCallback data = {scene, ob, me, dm, shadeless, two_sided_lighting};
	bool (*set_face_cb)(void *, int);
	bool picking = (G.f & G_PICKSEL) != 0;
	
	/* face hiding callback depending on mode */
	if (ob == scene->obedit)
		set_face_cb = tex_mat_set_face_editmesh_cb;
	else if (draw_flags & DRAW_FACE_SELECT)
		set_face_cb = tex_mat_set_face_mesh_cb;
	else
		set_face_cb = NULL;

	/* test if we can use glsl */
	const int drawtype = view3d_effective_drawtype(v3d);
	bool glsl = (drawtype == OB_MATERIAL) && !picking;

	GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);

	if (glsl || picking) {
		/* draw glsl or solid */
		dm->drawMappedFacesMat(dm,
		                       tex_mat_set_material_cb,
		                       set_face_cb, &data);
	}
	else {
		/* draw textured */
		dm->drawMappedFacesMat(dm,
		                       tex_mat_set_texture_cb,
		                       set_face_cb, &data);
	}

	GPU_end_object_materials();

	/* reset opengl state */
	GPU_end_object_materials();
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	glBindTexture(GL_TEXTURE_2D, 0);

	glFrontFace(GL_CCW);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	/* faceselect mode drawing over textured mesh */
	if (!(ob == scene->obedit) && (draw_flags & DRAW_FACE_SELECT)) {
		bool draw_select_edges = (ob->mode & OB_MODE_TEXTURE_PAINT) == 0;
		draw_mesh_face_select(rv3d, ob->data, dm, draw_select_edges);
	}
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

	/* Don't show alpha in wire mode. */
	const bool show_alpha = use_light;
	if (show_alpha) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (me->mloopcol) {
		dm->drawMappedFaces(dm, facemask_cb, setMaterial, NULL, user_data,
		                    DM_DRAW_USE_COLORS | flags);
	}
	else {
		glColor3f(1.0f, 1.0f, 1.0f);
		dm->drawMappedFaces(dm, facemask_cb, setMaterial, NULL, user_data, flags);
	}

	if (show_alpha) {
		glDisable(GL_BLEND);
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

