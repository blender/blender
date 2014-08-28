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

#include "GPU_buffers.h"
#include "GPU_extensions.h"
#include "GPU_draw.h"
#include "GPU_material.h"

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
	bool has_mtface;
	MFace *mf;
	MTFace *tf;
} drawEMTFMapped_userData;

typedef struct drawTFace_userData {
	Mesh *me;
	MFace *mf;
	MTFace *tf;
} drawTFace_userData;

/**************************** Face Select Mode *******************************/

/* mainly to be less confusing */
BLI_INLINE int edge_vis_index(const int index) { return index * 2; }
BLI_INLINE int edge_sel_index(const int index) { return index * 2 + 1; }

static BLI_bitmap *get_tface_mesh_marked_edge_info(Mesh *me)
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
				BLI_BITMAP_ENABLE(bitmap_edge_flags, edge_vis_index(ml->e));
				if (select_set) BLI_BITMAP_ENABLE(bitmap_edge_flags, edge_sel_index(ml->e));
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
		if ((me->drawflag & ME_HIDDENEDGES) || (BLI_BITMAP_TEST(data->edge_flags, edge_vis_index(index))))
			return DM_DRAW_OPTION_NORMAL;
		else
			return DM_DRAW_OPTION_SKIP;
	}
	else if (BLI_BITMAP_TEST(data->edge_flags, edge_sel_index(index)))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

static DMDrawOption draw_mesh_face_select__setSelectOpts(void *userData, int index)
{
	drawMeshFaceSelect_userData *data = userData;
	return (BLI_BITMAP_TEST(data->edge_flags, edge_sel_index(index))) ? DM_DRAW_OPTION_NORMAL : DM_DRAW_OPTION_SKIP;
}

/* draws unselected */
static DMDrawOption draw_mesh_face_select__drawFaceOptsInv(void *userData, int index)
{
	Mesh *me = (Mesh *)userData;

	MPoly *mpoly = &me->mpoly[index];
	if (!(mpoly->flag & ME_HIDE) && !(mpoly->flag & ME_FACE_SEL))
		return DM_DRAW_OPTION_NO_MCOL;  /* Don't set color */
	else
		return DM_DRAW_OPTION_SKIP;
}

void draw_mesh_face_select(RegionView3D *rv3d, Mesh *me, DerivedMesh *dm)
{
	drawMeshFaceSelect_userData data;

	data.me = me;
	data.edge_flags = get_tface_mesh_marked_edge_info(me);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
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
		dm->drawMappedFaces(dm, draw_mesh_face_select__drawFaceOptsInv, NULL, NULL, (void *)me, 0);
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
	bool stencil_invert;
	bool use_game_mat;
	int is_lit, is_tex;
	int color_profile;
	bool use_backface_culling;
	unsigned char obcol[4];
	float stencil_col[4];
	bool is_texpaint;
	bool texpaint_material; /* use material slots for texture painting */
} Gtexdraw = {NULL, NULL, NULL, false, false, 0, 0, 0, false, {0, 0, 0, 0}, {0.0f, 0.0f, 0.0f, 1.0f}, false, false};

static bool set_draw_settings_cached(int clearcache, MTFace *texface, Material *ma, struct TextureDrawState gtexdraw)
{
	static Material *c_ma;
	static int c_textured;
	static MTFace c_texface;
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
	bool texpaint = ((gtexdraw.ob->mode & OB_MODE_TEXTURE_PAINT) != 0);

	Image *ima = NULL;

	if (ma != NULL) {
		if (ma->mode & MA_TRANSP) {
			alphablend = GPU_BLEND_ALPHA;
		}
	}

	if (clearcache) {
		c_textured = c_lit = c_backculled = -1;
		memset(&c_texface, 0, sizeof(MTFace));
		c_badtex = false;
		c_has_texface = -1;
		c_ma = NULL;
	}
	else {
		textured = gtexdraw.is_tex;
	}

	/* convert number of lights into boolean */
	if (gtexdraw.is_lit) lit = 1;

	backculled = gtexdraw.use_backface_culling;
	if (ma) {
		if (ma->mode & MA_SHLESS) lit = 0;
		if (gtexdraw.use_game_mat) {
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
	else if (texpaint && ma) {
		if (gtexdraw.texpaint_material)
			ima = ma->texpaintslot ? ma->texpaintslot[ma->paint_active_slot].ima : NULL;
		else 
			ima = gtexdraw.canvas;
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
				if (GPU_verify_image(ima, NULL, 0, 1, 0, false)) {
					glEnable(GL_TEXTURE_2D);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
				}
				else {
					c_badtex = true;
					GPU_clear_tpage(true);
					glDisable(GL_TEXTURE_2D);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					glBindTexture(GL_TEXTURE_2D, 0);
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
	if (lit != c_lit || ma != c_ma) {
		if (lit) {
			float spec[4];
			if (!ma) ma = give_current_material_or_def(NULL, 0);  /* default material */

			spec[0] = ma->spec * ma->specr;
			spec[1] = ma->spec * ma->specg;
			spec[2] = ma->spec * ma->specb;
			spec[3] = 1.0;

			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, CLAMPIS(ma->har, 0, 128));
			glEnable(GL_LIGHTING);
			glEnable(GL_COLOR_MATERIAL);
		}
		else {
			glDisable(GL_LIGHTING); 
			glDisable(GL_COLOR_MATERIAL);
		}
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
	else if (v3d->drawtype == OB_SOLID || ((ob->mode & OB_MODE_EDIT) && v3d->drawtype != OB_TEXTURE)) {
		/* draw with default lights in solid draw mode and edit mode */
		solidtex = true;
		Gtexdraw.is_lit = -1;
	}
	else {
		/* draw with lights in the scene otherwise */
		solidtex = false;
		if (v3d->flag2 & V3D_SHADELESS_TEX)
			Gtexdraw.is_lit = 0;
		else
			Gtexdraw.is_lit = GPU_scene_object_lights(scene, ob, v3d->lay, rv3d->viewmat, !rv3d->is_persp);
	}
	
	rgba_float_to_uchar(obcol, ob->col);

	if (solidtex || v3d->drawtype == OB_TEXTURE) is_tex = true;
	else is_tex = false;

	Gtexdraw.ob = ob;
	Gtexdraw.stencil = (imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL) ? imapaint->stencil : NULL;
	Gtexdraw.stencil_invert = ((imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) != 0);
	Gtexdraw.is_texpaint = (ob->mode == OB_MODE_TEXTURE_PAINT);
	copy_v3_v3(Gtexdraw.stencil_col, imapaint->stencil_col);
	Gtexdraw.texpaint_material = (imapaint->mode == IMAGEPAINT_MODE_MATERIAL);
	Gtexdraw.canvas = (Gtexdraw.texpaint_material) ? NULL : imapaint->canvas;
	Gtexdraw.is_tex = is_tex;

	/* load the stencil texture here */
	if (Gtexdraw.is_texpaint && (Gtexdraw.stencil != NULL)) {
		glActiveTexture(GL_TEXTURE1);
		if (GPU_verify_image(Gtexdraw.stencil, NULL, false, false, false, false)) {
			glEnable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
			glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, Gtexdraw.stencil_col);
			if (!Gtexdraw.stencil_invert) {
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_ONE_MINUS_SRC_COLOR);
			}
			else {
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
			}
		}
		glActiveTexture(GL_TEXTURE0);
	}

	Gtexdraw.color_profile = BKE_scene_check_color_management_enabled(scene);
	Gtexdraw.use_game_mat = (RE_engines_find(scene->r.engine)->flag & RE_GAME) != 0;
	Gtexdraw.use_backface_culling = (v3d->flag2 & V3D_BACKFACE_CULLING) != 0;

	memcpy(Gtexdraw.obcol, obcol, sizeof(obcol));
	set_draw_settings_cached(1, NULL, NULL, Gtexdraw);
	glShadeModel(GL_SMOOTH);
	glCullFace(GL_BACK);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, (me->flag & ME_TWOSIDED) ? GL_TRUE : GL_FALSE);
}

static void draw_textured_end(void)
{
	if (Gtexdraw.ob->mode & OB_MODE_TEXTURE_PAINT) {
		if (Gtexdraw.stencil != NULL) {
			glActiveTexture(GL_TEXTURE1);
			glDisable(GL_TEXTURE_2D);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
		}		
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

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

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

static DMDrawOption draw_tface__set_draw_legacy(MTFace *tface, const bool has_mcol, int matnr)
{
	Material *ma = give_current_material(Gtexdraw.ob, matnr + 1);
	bool invalidtexture = false;

	if (ma && (ma->game.flag & GEMAT_INVISIBLE))
		return DM_DRAW_OPTION_SKIP;

	invalidtexture = set_draw_settings_cached(0, tface, ma, Gtexdraw);

	if (tface && invalidtexture) {
		glColor3ub(0xFF, 0x00, 0xFF);
		return DM_DRAW_OPTION_NO_MCOL; /* Don't set color */
	}
	else if (ma && (ma->shade_flag & MA_OBCOLOR)) {
		glColor3ubv(Gtexdraw.obcol);
		return DM_DRAW_OPTION_NO_MCOL; /* Don't set color */
	}
	else if (!has_mcol) {
		if (tface) {
			glColor3f(1.0, 1.0, 1.0);
		}
		else {
			if (ma) {
				float col[3];
				if (Gtexdraw.color_profile) linearrgb_to_srgb_v3_v3(col, &ma->r);
				else copy_v3_v3(col, &ma->r);
				
				glColor3fv(col);
			}
			else {
				glColor3f(1.0, 1.0, 1.0);
			}
		}
		return DM_DRAW_OPTION_NO_MCOL; /* Don't set color */
	}
	else {
		return DM_DRAW_OPTION_NORMAL; /* Set color from mcol */
	}
}

static DMDrawOption draw_mcol__set_draw_legacy(MTFace *UNUSED(tface), const bool has_mcol, int UNUSED(matnr))
{
	if (has_mcol)
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_NO_MCOL;
}

static DMDrawOption draw_tface__set_draw(MTFace *tface, const bool UNUSED(has_mcol), int matnr)
{
	Material *ma = give_current_material(Gtexdraw.ob, matnr + 1);

	if (ma && (ma->game.flag & GEMAT_INVISIBLE)) return 0;

	if (tface || Gtexdraw.is_texpaint)
		set_draw_settings_cached(0, tface, ma, Gtexdraw);

	/* always use color from mcol, as set in update_tface_color_layer */
	return DM_DRAW_OPTION_NORMAL;
}

static void update_tface_color_layer(DerivedMesh *dm)
{
	MTFace *tface = DM_get_tessface_data_layer(dm, CD_MTFACE);
	MFace *mface = dm->getTessFaceArray(dm);
	MCol *finalCol;
	int i, j;
	MCol *mcol = dm->getTessFaceDataArray(dm, CD_PREVIEW_MCOL);
	if (!mcol)
		mcol = dm->getTessFaceDataArray(dm, CD_MCOL);

	if (CustomData_has_layer(&dm->faceData, CD_TEXTURE_MCOL)) {
		finalCol = CustomData_get_layer(&dm->faceData, CD_TEXTURE_MCOL);
	}
	else {
		finalCol = MEM_mallocN(sizeof(MCol) * 4 * dm->getNumTessFaces(dm), "add_tface_color_layer");

		CustomData_add_layer(&dm->faceData, CD_TEXTURE_MCOL, CD_ASSIGN, finalCol, dm->numTessFaceData);
	}

	for (i = 0; i < dm->getNumTessFaces(dm); i++) {
		Material *ma = give_current_material(Gtexdraw.ob, mface[i].mat_nr + 1);

		if (ma && (ma->game.flag & GEMAT_INVISIBLE)) {
			if (mcol)
				memcpy(&finalCol[i * 4], &mcol[i * 4], sizeof(MCol) * 4);
			else
				for (j = 0; j < 4; j++) {
					finalCol[i * 4 + j].b = 255;
					finalCol[i * 4 + j].g = 255;
					finalCol[i * 4 + j].r = 255;
				}
		}
		else if (tface && set_draw_settings_cached(0, tface, ma, Gtexdraw)) {
			for (j = 0; j < 4; j++) {
				finalCol[i * 4 + j].b = 255;
				finalCol[i * 4 + j].g = 0;
				finalCol[i * 4 + j].r = 255;
			}
		}
		else if (ma && (ma->shade_flag & MA_OBCOLOR)) {
			for (j = 0; j < 4; j++) {
				finalCol[i * 4 + j].b = Gtexdraw.obcol[0];
				finalCol[i * 4 + j].g = Gtexdraw.obcol[1];
				finalCol[i * 4 + j].r = Gtexdraw.obcol[2];
			}
		}
		else if (!mcol) {
			if (tface) {
				for (j = 0; j < 4; j++) {
					finalCol[i * 4 + j].b = 255;
					finalCol[i * 4 + j].g = 255;
					finalCol[i * 4 + j].r = 255;
				}
			}
			else {
				float col[3];

				if (ma) {
					if (Gtexdraw.color_profile) linearrgb_to_srgb_v3_v3(col, &ma->r);
					else copy_v3_v3(col, &ma->r);
					
					for (j = 0; j < 4; j++) {
						finalCol[i * 4 + j].b = FTOCHAR(col[0]);
						finalCol[i * 4 + j].g = FTOCHAR(col[1]);
						finalCol[i * 4 + j].r = FTOCHAR(col[2]);
					}
				}
				else
					for (j = 0; j < 4; j++) {
						finalCol[i * 4 + j].b = 255;
						finalCol[i * 4 + j].g = 255;
						finalCol[i * 4 + j].r = 255;
					}
			}
		}
		else {
			for (j = 0; j < 4; j++) {
				finalCol[i * 4 + j].r = mcol[i * 4 + j].r;
				finalCol[i * 4 + j].g = mcol[i * 4 + j].g;
				finalCol[i * 4 + j].b = mcol[i * 4 + j].b;
			}
		}
	}
}

static DMDrawOption draw_tface_mapped__set_draw(void *userData, int index)
{
	Mesh *me = ((drawTFace_userData *)userData)->me;

	/* array checked for NULL before calling */
	MPoly *mpoly = &me->mpoly[index];

	BLI_assert(index >= 0 && index < me->totpoly);

	if (mpoly->flag & ME_HIDE) {
		return DM_DRAW_OPTION_SKIP;
	}
	else {
		MTexPoly *tpoly = (me->mtpoly) ? &me->mtpoly[index] : NULL;
		MTFace mtf = {{{0}}};
		int matnr = mpoly->mat_nr;

		if (tpoly) {
			ME_MTEXFACE_CPY(&mtf, tpoly);
		}

		return draw_tface__set_draw(&mtf, (me->mloopcol != NULL), matnr);
	}
}

static DMDrawOption draw_em_tf_mapped__set_draw(void *userData, int index)
{
	drawEMTFMapped_userData *data = userData;
	BMEditMesh *em = data->em;
	BMFace *efa;

	if (UNLIKELY(index >= em->bm->totface))
		return DM_DRAW_OPTION_NORMAL;

	efa = BM_face_at_index(em->bm, index);

	if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return DM_DRAW_OPTION_SKIP;
	}
	else {
		MTFace mtf = {{{0}}};
		int matnr = efa->mat_nr;

		if (data->has_mtface) {
			MTexPoly *tpoly = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			ME_MTEXFACE_CPY(&mtf, tpoly);
		}

		return draw_tface__set_draw_legacy(data->has_mtface ? &mtf : NULL,
		                                   data->has_mcol, matnr);
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
	MTFace tmp_tf      = {{{0}}};

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
		int mf_smooth = mp->flag & ME_SMOOTH;
		Material *mat = (me->mat) ? me->mat[matnr] : NULL;
		int mode = mat ? mat->game.flag : GEMAT_INVISIBLE;


		if (!(mode & GEMAT_INVISIBLE) && (mode & GEMAT_TEXT) && mp->totloop >= 3) {
			/* get the polygon as a tri/quad */
			int mp_vi[4];
			float v1[3], v2[3], v3[3], v4[3];
			char string[MAX_PROPSTRING];
			int characters, i, glattrib = -1, badtex = 0;


			/* TEXFACE */
			ME_MTEXFACE_CPY(&tmp_tf, mtpoly);

			if (glsl) {
				GPU_enable_material(matnr + 1, &gattribs);

				for (i = 0; i < gattribs.totlayer; i++) {
					if (gattribs.layer[i].type == CD_MTFACE) {
						glattrib = gattribs.layer[i].glindex;
						break;
					}
				}
			}
			else {
				badtex = set_draw_settings_cached(0, &tmp_tf, mat, Gtexdraw);
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
			copy_v2_v2(tmp_tf.uv[0], luv->uv); luv++;
			copy_v2_v2(tmp_tf.uv[1], luv->uv); luv++;
			copy_v2_v2(tmp_tf.uv[2], luv->uv); luv++;
			if (mp->totloop >= 4) {
				copy_v2_v2(tmp_tf.uv[3], luv->uv);
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
			ddm->getVertCo(ddm, mp_vi[0], v1);
			ddm->getVertCo(ddm, mp_vi[1], v2);
			ddm->getVertCo(ddm, mp_vi[2], v3);
			if (mp->totloop >= 4) {
				ddm->getVertCo(ddm, mp_vi[3], v4);
			}



			/* The BM_FONT handling is in the gpu module, shared with the
			 * game engine, was duplicated previously */

			BKE_bproperty_set_valstr(prop, string);
			characters = strlen(string);
			
			if (!BKE_image_has_ibuf(mtpoly->tpage, NULL))
				characters = 0;

			if (!mf_smooth) {
				float nor[3];

				normal_tri_v3(nor, v1, v2, v3);

				glNormal3fv(nor);
			}

			GPU_render_text(&tmp_tf, mode, string, characters,
			                (unsigned int *)tmp_mcol_pt, v1, v2, v3, (mp->totloop >= 4 ? v4 : NULL), glattrib);
		}
	}

	ddm->release(ddm);
}

static int compareDrawOptions(void *userData, int cur_index, int next_index)
{
	drawTFace_userData *data = userData;

	if (data->mf && data->mf[cur_index].mat_nr != data->mf[next_index].mat_nr)
		return 0;

	if (data->tf && data->tf[cur_index].tpage != data->tf[next_index].tpage)
		return 0;

	return 1;
}


static int compareDrawOptionsEm(void *userData, int cur_index, int next_index)
{
	drawEMTFMapped_userData *data = userData;

	if (data->mf && data->mf[cur_index].mat_nr != data->mf[next_index].mat_nr)
		return 0;

	if (data->tf && data->tf[cur_index].tpage != data->tf[next_index].tpage)
		return 0;

	return 1;
}

static void draw_mesh_textured_old(Scene *scene, View3D *v3d, RegionView3D *rv3d,
                                   Object *ob, DerivedMesh *dm, const int draw_flags)
{
	Mesh *me = ob->data;
	DMDrawFlag uvflag = DM_DRAW_USE_ACTIVE_UV;

	/* correct for negative scale */
	if (ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	/* draw the textured mesh */
	draw_textured_begin(scene, v3d, rv3d, ob);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (ob->mode & OB_MODE_TEXTURE_PAINT) {
		uvflag = DM_DRAW_USE_TEXPAINT_UV;
	}

	if (ob->mode & OB_MODE_EDIT) {
		drawEMTFMapped_userData data;

		data.em = me->edit_btmesh;
		data.has_mcol = CustomData_has_layer(&me->edit_btmesh->bm->ldata, CD_MLOOPCOL);
		data.has_mtface = CustomData_has_layer(&me->edit_btmesh->bm->pdata, CD_MTEXPOLY);
		data.mf = DM_get_tessface_data_layer(dm, CD_MFACE);
		data.tf = DM_get_tessface_data_layer(dm, CD_MTFACE);

		dm->drawMappedFacesTex(dm, draw_em_tf_mapped__set_draw, compareDrawOptionsEm, &data, 0);
	}
	else if (draw_flags & DRAW_FACE_SELECT) {
		if (ob->mode & OB_MODE_WEIGHT_PAINT)
			dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions_facemask, GPU_enable_material, NULL, me,
			                    DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH);
		else {
			drawTFace_userData userData;

			userData.mf = DM_get_tessface_data_layer(dm, CD_MFACE);
			userData.tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
			userData.me = me;
			dm->drawMappedFacesTex(dm, me->mpoly ? draw_tface_mapped__set_draw : NULL, compareDrawOptions, &userData, uvflag);
		}
	}
	else {
		if (GPU_buffer_legacy(dm)) {
			if (draw_flags & DRAW_MODIFIERS_PREVIEW)
				dm->drawFacesTex(dm, draw_mcol__set_draw_legacy, NULL, NULL, uvflag);
			else 
				dm->drawFacesTex(dm, draw_tface__set_draw_legacy, NULL, NULL, uvflag);
		}
		else {
			drawTFace_userData userData;

			update_tface_color_layer(dm);

			userData.mf = DM_get_tessface_data_layer(dm, CD_MFACE);
			userData.tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
			userData.me = NULL;

			dm->drawFacesTex(dm, draw_tface__set_draw, compareDrawOptions, &userData, uvflag);
		}
	}

	/* draw game engine text hack */
	if (BKE_bproperty_object_get(ob, "Text"))
		draw_mesh_text(scene, ob, 0);

	draw_textured_end();
	
	/* draw edges and selected faces over textured mesh */
	if (!(ob == scene->obedit) && (draw_flags & DRAW_FACE_SELECT))
		draw_mesh_face_select(rv3d, me, dm);

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
} TexMatCallback;

static void tex_mat_set_material_cb(void *UNUSED(userData), int mat_nr, void *attribs)
{
	/* all we have to do here is simply enable the GLSL material, but note
	 * that the GLSL code will give different result depending on the drawtype,
	 * in texture draw mode it will output the active texture node, in material
	 * draw mode it will show the full material. */
	GPU_enable_material(mat_nr, attribs);
}

static void tex_mat_set_texture_cb(void *userData, int mat_nr, void *attribs)
{
	/* texture draw mode without GLSL */
	TexMatCallback *data = (TexMatCallback *)userData;
	GPUVertexAttribs *gattribs = attribs;
	Image *ima;
	ImageUser *iuser;
	bNode *node;
	int texture_set = 0;

	/* draw image texture if we find one */
	if (ED_object_get_active_image(data->ob, mat_nr, &ima, &iuser, &node, NULL)) {
		/* get openl texture */
		int mipmap = 1;
		int bindcode = (ima) ? GPU_verify_image(ima, iuser, 0, 0, mipmap, false) : 0;
		float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		if (bindcode) {
			NodeTexBase *texbase = node->storage;

			/* disable existing material */
			GPU_disable_material();
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, zero);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, zero);
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 0);

			/* bind texture */
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glEnable(GL_COLOR_MATERIAL);
			glEnable(GL_TEXTURE_2D);

			glBindTexture(GL_TEXTURE_2D, ima->bindcode);
			glColor3f(1.0f, 1.0f, 1.0f);

			glMatrixMode(GL_TEXTURE);
			glLoadMatrixf(texbase->tex_mapping.mat);
			glMatrixMode(GL_MODELVIEW);

			/* use active UV texture layer */
			memset(gattribs, 0, sizeof(*gattribs));

			gattribs->layer[0].type = CD_MTFACE;
			gattribs->layer[0].name[0] = '\0';
			gattribs->layer[0].gltexco = 1;
			gattribs->totlayer = 1;

			texture_set = 1;
		}
	}

	if (!texture_set) {
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);

		/* disable texture */
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_COLOR_MATERIAL);

		/* draw single color */
		GPU_enable_material(mat_nr, attribs);
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

	if ((v3d->drawtype == OB_TEXTURE) && (v3d->flag2 & V3D_SHADELESS_TEX))
		glColor3f(1.0f, 1.0f, 1.0f);
	else
		glEnable(GL_LIGHTING);

	{
		Mesh *me = ob->data;
		TexMatCallback data = {scene, ob, me, dm};
		bool (*set_face_cb)(void *, int);
		int glsl, picking = (G.f & G_PICKSEL);
		
		/* face hiding callback depending on mode */
		if (ob == scene->obedit)
			set_face_cb = tex_mat_set_face_editmesh_cb;
		else if (draw_flags & DRAW_FACE_SELECT)
			set_face_cb = tex_mat_set_face_mesh_cb;
		else
			set_face_cb = NULL;

		/* test if we can use glsl */
		glsl = (v3d->drawtype == OB_MATERIAL) && GPU_glsl_support() && !picking;

		GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);

		if (glsl || picking) {
			/* draw glsl or solid */
			dm->drawMappedFacesMat(dm,
			                       tex_mat_set_material_cb,
			                       set_face_cb, &data);
		}
		else {
			float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

			/* draw textured */
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, zero);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, zero);
			glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 0);

			dm->drawMappedFacesMat(dm,
			                       tex_mat_set_texture_cb,
			                       set_face_cb, &data);
		}

		GPU_end_object_materials();
	}

	/* reset opengl state */
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFrontFace(GL_CCW);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	/* faceselect mode drawing over textured mesh */
	if (!(ob == scene->obedit) && (draw_flags & DRAW_FACE_SELECT))
		draw_mesh_face_select(rv3d, ob->data, dm);
}

/* Vertex Paint and Weight Paint */
static void draw_mesh_paint_light_begin(void)
{
	const float spec[4] = {0.47f, 0.47f, 0.47f, 0.47f};

	GPU_enable_material(0, NULL);

	/* but set default spec */
	glColorMaterial(GL_FRONT_AND_BACK, GL_SPECULAR);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);

	/* diffuse */
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);
}
static void draw_mesh_paint_light_end(void)
{
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);

	GPU_disable_material();
}

void draw_mesh_paint_weight_faces(DerivedMesh *dm, const bool use_light,
                                  void *facemask_cb, void *user_data)
{
	if (use_light) {
		draw_mesh_paint_light_begin();
	}

	dm->drawMappedFaces(dm, (DMSetDrawOptions)facemask_cb, GPU_enable_material, NULL, user_data,
	                    DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH);

	if (use_light) {
		draw_mesh_paint_light_end();
	}
}

void draw_mesh_paint_vcolor_faces(DerivedMesh *dm, const bool use_light,
                                  void *facemask_cb, void *user_data,
                                  const Mesh *me)
{
	if (use_light) {
		draw_mesh_paint_light_begin();
	}

	if (me->mloopcol) {
		dm->drawMappedFaces(dm, facemask_cb, GPU_enable_material, NULL, user_data,
		                    DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH);
	}
	else {
		glColor3f(1.0f, 1.0f, 1.0f);
		dm->drawMappedFaces(dm, facemask_cb, GPU_enable_material, NULL, user_data,
		                    DM_DRAW_ALWAYS_SMOOTH);
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
	glEnable(GL_LINE_STIPPLE);
	glLineStipple(1, 0xAAAA);

	dm->drawMappedEdges(dm, (DMSetDrawOptions)edgemask_cb, user_data);

	if (use_depth) {
		ED_view3d_polygon_offset(rv3d, 0.0);
		glDepthMask(1);
	}
	else {
		glEnable(GL_DEPTH_TEST);
	}

	glDisable(GL_LINE_STIPPLE);

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
		draw_mesh_face_select(rv3d, me, dm);
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

