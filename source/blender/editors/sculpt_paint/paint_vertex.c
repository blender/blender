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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_vertex.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_array_utils.h"
#include "BLI_bitmap.h"
#include "BLI_task.h"
#include "BLI_string_utils.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_colortools.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "bmesh.h"
#include "BKE_ccg.h"

#include "sculpt_intern.h"
#include "paint_intern.h"  /* own include */

/* Use for 'blur' brush, align with PBVH nodes, created and freed on each update. */
struct VPaintAverageAccum {
	uint len;
	uint value[3];
};

struct WPaintAverageAccum {
	uint len;
	double value;
};

static void defweight_prev_init(const MDeformWeight *dw, float *weight_prev)
{
	if (UNLIKELY(*weight_prev == -1.0f)) {
		*weight_prev = dw ? dw->weight : 0.0f;
	}
}

/* check if we can do partial updates and have them draw realtime
 * (without rebuilding the 'derivedFinal') */
static bool vertex_paint_use_fast_update_check(Object *ob)
{
	DerivedMesh *dm = ob->derivedFinal;

	if (dm) {
		Mesh *me = BKE_mesh_from_object(ob);
		if (me && me->mloopcol) {
			return (me->mloopcol == CustomData_get_layer(&dm->loopData, CD_MLOOPCOL));
		}
	}

	return false;
}

static void paint_last_stroke_update(Scene *scene, ARegion *ar, const float mval[2])
{
	const int mval_i[2] = {mval[0], mval[1]};
	float world[3];

	if (ED_view3d_autodist_simple(ar, mval_i, world, 0, NULL)) {
		UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
		ups->average_stroke_counter++;
		add_v3_v3(ups->average_stroke_accum, world);
		ups->last_stroke_valid = true;
	}
}

/* polling - retrieve whether cursor should be set or operator should be done */

/* Returns true if vertex paint mode is active */
int vertex_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && ob->mode == OB_MODE_VERTEX_PAINT && ((Mesh *)ob->data)->totpoly;
}

int vertex_paint_poll(bContext *C)
{
	if (vertex_paint_mode_poll(C) && 
	    BKE_paint_brush(&CTX_data_tool_settings(C)->vpaint->paint))
	{
		ScrArea *sa = CTX_wm_area(C);
		if (sa && sa->spacetype == SPACE_VIEW3D) {
			ARegion *ar = CTX_wm_region(C);
			if (ar->regiontype == RGN_TYPE_WINDOW)
				return 1;
		}
	}
	return 0;
}

int weight_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && ob->mode == OB_MODE_WEIGHT_PAINT && ((Mesh *)ob->data)->totpoly;
}

int weight_paint_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	ScrArea *sa;

	if ((ob != NULL) &&
	    (ob->mode & OB_MODE_WEIGHT_PAINT) &&
	    (BKE_paint_brush(&CTX_data_tool_settings(C)->wpaint->paint) != NULL) &&
	    (sa = CTX_wm_area(C)) &&
	    (sa->spacetype == SPACE_VIEW3D))
	{
		ARegion *ar = CTX_wm_region(C);
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			return 1;
		}
	}
	return 0;
}

static VPaint *new_vpaint(int wpaint)
{
	VPaint *vp = MEM_callocN(sizeof(VPaint), "VPaint");
	
	vp->flag = (wpaint) ? 0 : VP_SPRAY;
	vp->paint.flags |= PAINT_SHOW_BRUSH;

	return vp;
}

uint vpaint_get_current_col(Scene *scene, VPaint *vp)
{
	Brush *brush = BKE_paint_brush(&vp->paint);
	uchar col[4];
	rgb_float_to_uchar(col, BKE_brush_color_get(scene, brush));
	col[3] = 255; /* alpha isn't used, could even be removed to speedup paint a little */
	return *(uint *)col;
}

static void do_shared_vertexcol(Mesh *me, bool *mlooptag)
{
	const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	const MPoly *mp;
	int (*scol)[4];
	int i, j;
	bool has_shared = false;

	/* if no mloopcol: do not do */
	/* if mtexpoly: only the involved faces, otherwise all */

	if (me->mloopcol == NULL || me->totvert == 0 || me->totpoly == 0) return;

	scol = MEM_callocN(sizeof(int) * me->totvert * 5, "scol");

	for (i = 0, mp = me->mpoly; i < me->totpoly; i++, mp++) {
		if ((use_face_sel == false) || (mp->flag & ME_FACE_SEL)) {
			const MLoop *ml = me->mloop + mp->loopstart;
			MLoopCol *lcol = me->mloopcol + mp->loopstart;
			for (j = 0; j < mp->totloop; j++, ml++, lcol++) {
				scol[ml->v][0] += lcol->r;
				scol[ml->v][1] += lcol->g;
				scol[ml->v][2] += lcol->b;
				scol[ml->v][3] += 1;
				has_shared = 1;
			}
		}
	}

	if (has_shared) {
		for (i = 0; i < me->totvert; i++) {
			if (scol[i][3] != 0) {
				scol[i][0] = divide_round_i(scol[i][0], scol[i][3]);
				scol[i][1] = divide_round_i(scol[i][1], scol[i][3]);
				scol[i][2] = divide_round_i(scol[i][2], scol[i][3]);
			}
		}

		for (i = 0, mp = me->mpoly; i < me->totpoly; i++, mp++) {
			if ((use_face_sel == false) || (mp->flag & ME_FACE_SEL)) {
				const MLoop *ml = me->mloop + mp->loopstart;
				MLoopCol *lcol = me->mloopcol + mp->loopstart;
				for (j = 0; j < mp->totloop; j++, ml++, lcol++) {
					if (mlooptag[mp->loopstart + j]) {
						lcol->r = scol[ml->v][0];
						lcol->g = scol[ml->v][1];
						lcol->b = scol[ml->v][2];
					}
				}
			}
		}
	}

	MEM_freeN(scol);
}

static bool make_vertexcol(Object *ob)  /* single ob */
{
	Mesh *me;

	if (ID_IS_LINKED_DATABLOCK(ob) ||
	    ((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->totpoly == 0) ||
	    (me->edit_btmesh))
	{
		return false;
	}

	/* copies from shadedisplist to mcol */
	if (!me->mloopcol && me->totloop) {
		CustomData_add_layer(&me->ldata, CD_MLOOPCOL, CD_DEFAULT, NULL, me->totloop);
		BKE_mesh_update_customdata_pointers(me, true);
	}

	DEG_id_tag_update(&me->id, 0);
	
	return (me->mloopcol != NULL);
}

/* mirror_vgroup is set to -1 when invalid */
static int wpaint_mirror_vgroup_ensure(Object *ob, const int vgroup_active)
{
	bDeformGroup *defgroup = BLI_findlink(&ob->defbase, vgroup_active);

	if (defgroup) {
		int mirrdef;
		char name_flip[MAXBONENAME];

		BLI_string_flip_side_name(name_flip, defgroup->name, false, sizeof(name_flip));
		mirrdef = defgroup_name_index(ob, name_flip);
		if (mirrdef == -1) {
			if (BKE_defgroup_new(ob, name_flip)) {
				mirrdef = BLI_listbase_count(&ob->defbase) - 1;
			}
		}

		/* curdef should never be NULL unless this is
		 * a  lamp and BKE_object_defgroup_add_name fails */
		return mirrdef;
	}

	return -1;
}

struct WPaintPrev {
	struct MDeformVert *wpaint_prev;	/* previous vertex weights */
	int tot;							/* allocation size of prev buffers */
};

static void wpaint_prev_init(struct WPaintPrev *wpp)
{
	wpp->wpaint_prev = NULL;
	wpp->tot = 0;
}

static void wpaint_prev_create(struct WPaintPrev *wpp, MDeformVert *dverts, int dcount)
{
	wpaint_prev_init(wpp);

	if (dverts && dcount) {
		wpp->wpaint_prev = MEM_mallocN(sizeof(MDeformVert) * dcount, "wpaint prev");
		wpp->tot = dcount;
		BKE_defvert_array_copy(wpp->wpaint_prev, dverts, dcount);
	}
}

static void wpaint_prev_destroy(struct WPaintPrev *wpp)
{
	if (wpp->wpaint_prev) {
		BKE_defvert_array_free(wpp->wpaint_prev, wpp->tot);
	}
	wpp->wpaint_prev = NULL;
	wpp->tot = 0;
}

bool ED_vpaint_fill(Object *ob, uint paintcol)
{
	Mesh *me;
	const MPoly *mp;
	int i, j;

	if (((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->mloopcol == NULL && (make_vertexcol(ob) == false)))
	{
		return false;
	}

	const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	mp = me->mpoly;
	for (i = 0; i < me->totpoly; i++, mp++) {
		MLoopCol *lcol = me->mloopcol + mp->loopstart;

		if (use_face_sel && !(mp->flag & ME_FACE_SEL))
			continue;

		for (j = 0; j < mp->totloop; j++, lcol++) {
			*(int *)lcol = paintcol;
		}
	}
	
	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	DEG_id_tag_update(&me->id, 0);

	return true;
}


/* fills in the selected faces with the current weight and vertex group */
bool ED_wpaint_fill(Object *ob, float paintweight)
{
	Mesh *me = ob->data;
	const MPoly *mp;
	MDeformWeight *dw, *dw_prev;
	int vgroup_active, vgroup_mirror = -1;
	uint index;
	const bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

	/* mutually exclusive, could be made into a */
	const short paint_selmode = ME_EDIT_PAINT_SEL_MODE(me);

	if (me->totpoly == 0 || me->dvert == NULL || !me->mpoly) {
		return false;
	}
	
	vgroup_active = ob->actdef - 1;

	/* if mirror painting, find the other group */
	if (me->editflag & ME_EDIT_MIRROR_X) {
		vgroup_mirror = wpaint_mirror_vgroup_ensure(ob, vgroup_active);
	}
	
	struct WPaintPrev wpp;
	wpaint_prev_create(&wpp, me->dvert, me->totvert);
	
	for (index = 0, mp = me->mpoly; index < me->totpoly; index++, mp++) {
		uint fidx = mp->totloop - 1;

		if ((paint_selmode == SCE_SELECT_FACE) && !(mp->flag & ME_FACE_SEL)) {
			continue;
		}

		do {
			uint vidx = me->mloop[mp->loopstart + fidx].v;

			if (!me->dvert[vidx].flag) {
				if ((paint_selmode == SCE_SELECT_VERTEX) && !(me->mvert[vidx].flag & SELECT)) {
					continue;
				}

				dw = defvert_verify_index(&me->dvert[vidx], vgroup_active);
				if (dw) {
					dw_prev = defvert_verify_index(wpp.wpaint_prev + vidx, vgroup_active);
					dw_prev->weight = dw->weight; /* set the undo weight */
					dw->weight = paintweight;

					if (me->editflag & ME_EDIT_MIRROR_X) {  /* x mirror painting */
						int j = mesh_get_x_mirror_vert(ob, NULL, vidx, topology);
						if (j >= 0) {
							/* copy, not paint again */
							if (vgroup_mirror != -1) {
								dw = defvert_verify_index(me->dvert + j, vgroup_mirror);
								dw_prev = defvert_verify_index(wpp.wpaint_prev + j, vgroup_mirror);
							}
							else {
								dw = defvert_verify_index(me->dvert + j, vgroup_active);
								dw_prev = defvert_verify_index(wpp.wpaint_prev + j, vgroup_active);
							}
							dw_prev->weight = dw->weight; /* set the undo weight */
							dw->weight = paintweight;
						}
					}
				}
				me->dvert[vidx].flag = 1;
			}

		} while (fidx--);
	}

	{
		MDeformVert *dv = me->dvert;
		for (index = me->totvert; index != 0; index--, dv++) {
			dv->flag = 0;
		}
	}

	wpaint_prev_destroy(&wpp);

	DEG_id_tag_update(&me->id, 0);

	return true;
}

bool ED_vpaint_smooth(Object *ob)
{
	Mesh *me;
	const MPoly *mp;

	int i, j;

	bool *mlooptag;

	if (((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->mloopcol == NULL && (make_vertexcol(ob) == false)))
	{
		return false;
	}

	const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	mlooptag = MEM_callocN(sizeof(bool) * me->totloop, "VPaintData mlooptag");

	/* simply tag loops of selected faces */
	mp = me->mpoly;
	for (i = 0; i < me->totpoly; i++, mp++) {
		const MLoop *ml = me->mloop + mp->loopstart;
		int ml_index = mp->loopstart;

		if (use_face_sel && !(mp->flag & ME_FACE_SEL))
			continue;

		for (j = 0; j < mp->totloop; j++, ml_index++, ml++) {
			mlooptag[ml_index] = true;
		}
	}

	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	do_shared_vertexcol(me, mlooptag);

	MEM_freeN(mlooptag);

	DEG_id_tag_update(&me->id, 0);

	return true;
}

/**
 * Apply callback to each vertex of the active vertex color layer.
 */
bool ED_vpaint_color_transform(
        struct Object *ob,
        VPaintTransform_Callback vpaint_tx_fn,
        const void *user_data)
{
	Mesh *me;
	const MPoly *mp;

	if (((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->mloopcol == NULL && (make_vertexcol(ob) == false)))
	{
		return false;
	}

	const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	mp = me->mpoly;

	for (int i = 0; i < me->totpoly; i++, mp++) {
		MLoopCol *lcol = &me->mloopcol[mp->loopstart];

		if (use_face_sel && !(mp->flag & ME_FACE_SEL)) {
			continue;
		}

		for (int j = 0; j < mp->totloop; j++, lcol++) {
			float col[3];
			rgb_uchar_to_float(col, &lcol->r);

			vpaint_tx_fn(col, user_data, col);

			rgb_float_to_uchar(&lcol->r, col);
		}
	}

	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	DEG_id_tag_update(&me->id, 0);

	return true;
}

/* XXX: should be re-implemented as a vertex/weight paint 'color correct' operator */
#if 0
void vpaint_dogamma(Scene *scene)
{
	VPaint *vp = scene->toolsettings->vpaint;
	Mesh *me;
	Object *ob;
	float igam, fac;
	int a, temp;
	uchar *cp, gamtab[256];

	ob = OBACT;
	me = BKE_mesh_from_object(ob);

	if (!(ob->mode & OB_MODE_VERTEX_PAINT)) return;
	if (me == 0 || me->mcol == 0 || me->totface == 0) return;

	igam = 1.0 / vp->gamma;
	for (a = 0; a < 256; a++) {

		fac = ((float)a) / 255.0;
		fac = vp->mul * pow(fac, igam);

		temp = 255.9 * fac;

		if (temp <= 0) gamtab[a] = 0;
		else if (temp >= 255) gamtab[a] = 255;
		else gamtab[a] = temp;
	}

	a = 4 * me->totface;
	cp = (uchar *)me->mcol;
	while (a--) {

		cp[1] = gamtab[cp[1]];
		cp[2] = gamtab[cp[2]];
		cp[3] = gamtab[cp[3]];

		cp += 4;
	}
}
#endif

BLI_INLINE uint mcol_blend(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* Updated to use the rgb squared color model which blends nicer. */
	int r1 = cp1[0] * cp1[0];
	int g1 = cp1[1] * cp1[1];
	int b1 = cp1[2] * cp1[2];

	int r2 = cp2[0] * cp2[0];
	int g2 = cp2[1] * cp2[1];
	int b2 = cp2[2] * cp2[2];

	cp[0] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * r1 + fac * r2), 255)));
	cp[1] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * g1 + fac * g2), 255)));
	cp[2] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * b1 + fac * b2), 255)));
	cp[3] = 255;

	return col;
}

BLI_INLINE uint mcol_add(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	temp = cp1[0] + divide_round_i((fac * cp2[0]), 255);
	cp[0] = (temp > 254) ? 255 : temp;
	temp = cp1[1] + divide_round_i((fac * cp2[1]), 255);
	cp[1] = (temp > 254) ? 255 : temp;
	temp = cp1[2] + divide_round_i((fac * cp2[2]), 255);
	cp[2] = (temp > 254) ? 255 : temp;
	cp[3] = 255;
	
	return col;
}

BLI_INLINE uint mcol_sub(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	temp = cp1[0] - divide_round_i((fac * cp2[0]), 255);
	cp[0] = (temp < 0) ? 0 : temp;
	temp = cp1[1] - divide_round_i((fac * cp2[1]), 255);
	cp[1] = (temp < 0) ? 0 : temp;
	temp = cp1[2] - divide_round_i((fac * cp2[2]), 255);
	cp[2] = (temp < 0) ? 0 : temp;
	cp[3] = 255;

	return col;
}

BLI_INLINE uint mcol_mul(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* first mul, then blend the fac */
	cp[0] = divide_round_i(mfac * cp1[0] * 255 + fac * cp2[0] * cp1[0], 255 * 255);
	cp[1] = divide_round_i(mfac * cp1[1] * 255 + fac * cp2[1] * cp1[1], 255 * 255);
	cp[2] = divide_round_i(mfac * cp1[2] * 255 + fac * cp2[2] * cp1[2], 255 * 255);
	cp[3] = 255;

	return col;
}

BLI_INLINE uint mcol_lighten(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}
	else if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* See if are lighter, if so mix, else don't do anything.
	 * if the paint col is darker then the original, then ignore */
	if (IMB_colormanagement_get_luminance_byte(cp1) > IMB_colormanagement_get_luminance_byte(cp2)) {
		return col1;
	}

	cp[0] = divide_round_i(mfac * cp1[0] + fac * cp2[0], 255);
	cp[1] = divide_round_i(mfac * cp1[1] + fac * cp2[1], 255);
	cp[2] = divide_round_i(mfac * cp1[2] + fac * cp2[2], 255);
	cp[3] = 255;

	return col;
}

BLI_INLINE uint mcol_darken(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}
	else if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* See if were darker, if so mix, else don't do anything.
	 * if the paint col is brighter then the original, then ignore */
	if (IMB_colormanagement_get_luminance_byte(cp1) < IMB_colormanagement_get_luminance_byte(cp2)) {
		return col1;
	}

	cp[0] = divide_round_i((mfac * cp1[0] + fac * cp2[0]), 255);
	cp[1] = divide_round_i((mfac * cp1[1] + fac * cp2[1]), 255);
	cp[2] = divide_round_i((mfac * cp1[2] + fac * cp2[2]), 255);
	cp[3] = 255;
	return col;
}

/* wpaint has 'wpaint_blend_tool' */
static uint vpaint_blend_tool(const int tool, const uint col,
                                      const uint paintcol, const int alpha_i)
{
	switch (tool) {
		case PAINT_BLEND_MIX:
		case PAINT_BLEND_BLUR:     return mcol_blend(col, paintcol, alpha_i);
		case PAINT_BLEND_AVERAGE:  return mcol_blend(col, paintcol, alpha_i);
		case PAINT_BLEND_SMEAR:    return mcol_blend(col, paintcol, alpha_i);
		case PAINT_BLEND_ADD:      return mcol_add(col, paintcol, alpha_i);
		case PAINT_BLEND_SUB:      return mcol_sub(col, paintcol, alpha_i);
		case PAINT_BLEND_MUL:      return mcol_mul(col, paintcol, alpha_i);
		case PAINT_BLEND_LIGHTEN:  return mcol_lighten(col, paintcol, alpha_i);
		case PAINT_BLEND_DARKEN:   return mcol_darken(col, paintcol, alpha_i);
		default:
			BLI_assert(0);
			return 0;
	}
}

/* wpaint has 'wpaint_blend' */
static uint vpaint_blend(
        VPaint *vp, uint col, uint colorig,
        const uint paintcol, const int alpha_i,
        /* pre scaled from [0-1] --> [0-255] */
        const int brush_alpha_value_i)
{
	Brush *brush = BKE_paint_brush(&vp->paint);
	const int tool = brush->vertexpaint_tool;

	col = vpaint_blend_tool(tool, col, paintcol, alpha_i);

	/* if no spray, clip color adding with colorig & orig alpha */
	if ((vp->flag & VP_SPRAY) == 0) {
		uint testcol, a;
		char *cp, *ct, *co;
		
		testcol = vpaint_blend_tool(tool, colorig, paintcol, brush_alpha_value_i);
		
		cp = (char *)&col;
		ct = (char *)&testcol;
		co = (char *)&colorig;
		
		for (a = 0; a < 4; a++) {
			if (ct[a] < co[a]) {
				if (cp[a] < ct[a]) cp[a] = ct[a];
				else if (cp[a] > co[a]) cp[a] = co[a];
			}
			else {
				if (cp[a] < co[a]) cp[a] = co[a];
				else if (cp[a] > ct[a]) cp[a] = ct[a];
			}
		}
	}

	return col;
}


/* whats _dl mean? */
static float calc_vp_strength_col_dl(
        VPaint *vp, const ViewContext *vc, const float co[3],
        const float mval[2], const float brush_size_pressure, float rgba[4])
{
	float co_ss[2];  /* screenspace */

	if (ED_view3d_project_float_object(vc->ar,
	                                   co, co_ss,
	                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
	{
		const float dist_sq = len_squared_v2v2(mval, co_ss);

		if (dist_sq <= SQUARE(brush_size_pressure)) {
			Brush *brush = BKE_paint_brush(&vp->paint);
			const float dist = sqrtf(dist_sq);
			float factor;

			if (brush->mtex.tex && rgba) {
				if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_3D) {
					BKE_brush_sample_tex_3D(vc->scene, brush, co, rgba, 0, NULL);
				}
				else {
					const float co_ss_3d[3] = {co_ss[0], co_ss[1], 0.0f};  /* we need a 3rd empty value */
					BKE_brush_sample_tex_3D(vc->scene, brush, co_ss_3d, rgba, 0, NULL);
				}
				factor = rgba[3];
			}
			else {
				factor = 1.0f;
			}
			return factor * BKE_brush_curve_strength_clamped(brush, dist, brush_size_pressure);
		}
	}
	if (rgba)
		zero_v4(rgba);
	return 0.0f;
}

static float calc_vp_alpha_col_dl(
        VPaint *vp, const ViewContext *vc,
        float vpimat[3][3], const DMCoNo *v_co_no,
        const float mval[2],
        const float brush_size_pressure, const float brush_alpha_pressure, float rgba[4])
{
	float strength = calc_vp_strength_col_dl(vp, vc, v_co_no->co, mval, brush_size_pressure, rgba);

	if (strength > 0.0f) {
		float alpha = brush_alpha_pressure * strength;

		if (vp->flag & VP_NORMALS) {
			float dvec[3];

			/* transpose ! */
			dvec[2] = dot_v3v3(vpimat[2], v_co_no->no);
			if (dvec[2] > 0.0f) {
				dvec[0] = dot_v3v3(vpimat[0], v_co_no->no);
				dvec[1] = dot_v3v3(vpimat[1], v_co_no->no);

				alpha *= dvec[2] / len_v3(dvec);
			}
			else {
				return 0.0f;
			}
		}

		return alpha;
	}

	return 0.0f;
}


BLI_INLINE float wval_blend(const float weight, const float paintval, const float alpha)
{
	const float talpha = min_ff(alpha, 1.0f);  /* blending with values over 1 doesn't make sense */
	return (paintval * talpha) + (weight * (1.0f - talpha));
}
BLI_INLINE float wval_add(const float weight, const float paintval, const float alpha)
{
	return weight + (paintval * alpha);
}
BLI_INLINE float wval_sub(const float weight, const float paintval, const float alpha)
{
	return weight - (paintval * alpha);
}
BLI_INLINE float wval_mul(const float weight, const float paintval, const float alpha)
{   /* first mul, then blend the fac */
	return ((1.0f - alpha) + (alpha * paintval)) * weight;
}
BLI_INLINE float wval_lighten(const float weight, const float paintval, const float alpha)
{
	return (weight < paintval) ? wval_blend(weight, paintval, alpha) : weight;
}
BLI_INLINE float wval_darken(const float weight, const float paintval, const float alpha)
{
	return (weight > paintval) ? wval_blend(weight, paintval, alpha) : weight;
}


/* vpaint has 'vpaint_blend_tool' */
/* result is not clamped from [0-1] */
static float wpaint_blend_tool(const int tool,
                               /* dw->weight */
                               const float weight,
                               const float paintval, const float alpha)
{
	switch (tool) {
		case PAINT_BLEND_MIX:
		case PAINT_BLEND_AVERAGE:
		case PAINT_BLEND_SMEAR:
		case PAINT_BLEND_BLUR:     return wval_blend(weight, paintval, alpha);
		case PAINT_BLEND_ADD:      return wval_add(weight, paintval, alpha);
		case PAINT_BLEND_SUB:      return wval_sub(weight, paintval, alpha);
		case PAINT_BLEND_MUL:      return wval_mul(weight, paintval, alpha);
		case PAINT_BLEND_LIGHTEN:  return wval_lighten(weight, paintval, alpha);
		case PAINT_BLEND_DARKEN:   return wval_darken(weight, paintval, alpha);
		default:
			BLI_assert(0);
			return 0.0f;
	}
}

/* vpaint has 'vpaint_blend' */
static float wpaint_blend(VPaint *wp, float weight,
                          const float alpha, float paintval,
                          const float UNUSED(brush_alpha_value),
                          const short do_flip)
{
	Brush *brush = BKE_paint_brush(&wp->paint);
	int tool = brush->vertexpaint_tool;

	if (do_flip) {
		switch (tool) {
			case PAINT_BLEND_MIX:
				paintval = 1.f - paintval; break;
			case PAINT_BLEND_ADD:
				tool = PAINT_BLEND_SUB; break;
			case PAINT_BLEND_SUB:
				tool = PAINT_BLEND_ADD; break;
			case PAINT_BLEND_LIGHTEN:
				tool = PAINT_BLEND_DARKEN; break;
			case PAINT_BLEND_DARKEN:
				tool = PAINT_BLEND_LIGHTEN; break;
		}
	}
	
	weight = wpaint_blend_tool(tool, weight, paintval, alpha);

	CLAMP(weight, 0.0f, 1.0f);
	
	return weight;
}

/* ----------------------------------------------------- */


/* sets wp->weight to the closest weight value to vertex */
/* note: we cant sample frontbuf, weight colors are interpolated too unpredictable */
static int weight_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewContext vc;
	Mesh *me;
	bool changed = false;

	view3d_set_viewcontext(C, &vc);
	me = BKE_mesh_from_object(vc.obact);

	if (me && me->dvert && vc.v3d && vc.rv3d && (vc.obact->actdef != 0)) {
		const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
		int v_idx_best = -1;
		uint index;

		view3d_operator_needs_opengl(C);
		ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

		if (use_vert_sel) {
			if (ED_mesh_pick_vert(C, vc.obact, event->mval, &index, ED_MESH_PICK_DEFAULT_VERT_SIZE, true)) {
				v_idx_best = index;
			}
		}
		else {
			if (ED_mesh_pick_face_vert(C, vc.obact, event->mval, &index, ED_MESH_PICK_DEFAULT_FACE_SIZE)) {
				v_idx_best = index;
			}
			else if (ED_mesh_pick_face(C, vc.obact, event->mval, &index, ED_MESH_PICK_DEFAULT_FACE_SIZE)) {
				/* this relies on knowning the internal worksings of ED_mesh_pick_face_vert() */
				BKE_report(op->reports, RPT_WARNING, "The modifier used does not support deformed locations");
			}
		}

		if (v_idx_best != -1) { /* should always be valid */
			ToolSettings *ts = vc.scene->toolsettings;
			Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
			const int vgroup_active = vc.obact->actdef - 1;
			float vgroup_weight = defvert_find_weight(&me->dvert[v_idx_best], vgroup_active);

			/* use combined weight in multipaint mode, since that's what is displayed to the user in the colors */
			if (ts->multipaint) {
				int defbase_tot_sel;
				const int defbase_tot = BLI_listbase_count(&vc.obact->defbase);
				bool *defbase_sel = BKE_object_defgroup_selected_get(vc.obact, defbase_tot, &defbase_tot_sel);

				if (defbase_tot_sel > 1) {
					if (me->editflag & ME_EDIT_MIRROR_X) {
						BKE_object_defgroup_mirror_selection(
						        vc.obact, defbase_tot, defbase_sel, defbase_sel, &defbase_tot_sel);
					}

					vgroup_weight = BKE_defvert_multipaint_collective_weight(
					        &me->dvert[v_idx_best], defbase_tot, defbase_sel, defbase_tot_sel, ts->auto_normalize);

					/* if autonormalize is enabled, but weights are not normalized, the value can exceed 1 */
					CLAMP(vgroup_weight, 0.0f, 1.0f);
				}

				MEM_freeN(defbase_sel);
			}

			BKE_brush_weight_set(vc.scene, brush, vgroup_weight);
			changed = true;
		}
	}

	if (changed) {
		/* not really correct since the brush didnt change, but redraws the toolbar */
		WM_main_add_notifier(NC_BRUSH | NA_EDITED, NULL); /* ts->wpaint->paint.brush */

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void PAINT_OT_weight_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Weight Paint Sample Weight";
	ot->idname = "PAINT_OT_weight_sample";
	ot->description = "Use the mouse to sample a weight in the 3D view";

	/* api callbacks */
	ot->invoke = weight_sample_invoke;
	ot->poll = weight_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/* samples cursor location, and gives menu with vertex groups to activate */
static bool weight_paint_sample_enum_itemf__helper(const MDeformVert *dvert, const int defbase_tot, int *groups)
{
	/* this func fills in used vgroup's */
	bool found = false;
	int i = dvert->totweight;
	MDeformWeight *dw;
	for (dw = dvert->dw; i > 0; dw++, i--) {
		if (dw->def_nr < defbase_tot) {
			groups[dw->def_nr] = true;
			found = true;
		}
	}
	return found;
}
static EnumPropertyItem *weight_paint_sample_enum_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	if (C) {
		wmWindow *win = CTX_wm_window(C);
		if (win && win->eventstate) {
			ViewContext vc;
			Mesh *me;

			view3d_set_viewcontext(C, &vc);
			me = BKE_mesh_from_object(vc.obact);

			if (me && me->dvert && vc.v3d && vc.rv3d && vc.obact->defbase.first) {
				const int defbase_tot = BLI_listbase_count(&vc.obact->defbase);
				const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
				int *groups = MEM_callocN(defbase_tot * sizeof(int), "groups");
				bool found = false;
				uint index;

				const int mval[2] = {
				    win->eventstate->x - vc.ar->winrct.xmin,
				    win->eventstate->y - vc.ar->winrct.ymin,
				};

				view3d_operator_needs_opengl(C);
				ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

				if (use_vert_sel) {
					if (ED_mesh_pick_vert(C, vc.obact, mval, &index, ED_MESH_PICK_DEFAULT_VERT_SIZE, true)) {
						MDeformVert *dvert = &me->dvert[index];
						found |= weight_paint_sample_enum_itemf__helper(dvert, defbase_tot, groups);
					}
				}
				else {
					if (ED_mesh_pick_face(C, vc.obact, mval, &index, ED_MESH_PICK_DEFAULT_FACE_SIZE)) {
						const MPoly *mp = &me->mpoly[index];
						uint fidx = mp->totloop - 1;

						do {
							MDeformVert *dvert = &me->dvert[me->mloop[mp->loopstart + fidx].v];
							found |= weight_paint_sample_enum_itemf__helper(dvert, defbase_tot, groups);
						} while (fidx--);
					}
				}

				if (found == false) {
					MEM_freeN(groups);
				}
				else {
					EnumPropertyItem *item = NULL, item_tmp = {0};
					int totitem = 0;
					int i = 0;
					bDeformGroup *dg;
					for (dg = vc.obact->defbase.first; dg && i < defbase_tot; i++, dg = dg->next) {
						if (groups[i]) {
							item_tmp.identifier = item_tmp.name = dg->name;
							item_tmp.value = i;
							RNA_enum_item_add(&item, &totitem, &item_tmp);
						}
					}

					RNA_enum_item_end(&item, &totitem);
					*r_free = true;

					MEM_freeN(groups);
					return item;
				}
			}
		}
	}

	return DummyRNA_NULL_items;
}

static int weight_sample_group_exec(bContext *C, wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "group");
	ViewContext vc;
	view3d_set_viewcontext(C, &vc);

	BLI_assert(type + 1 >= 0);
	vc.obact->actdef = type + 1;

	DEG_id_tag_update(&vc.obact->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, vc.obact);
	return OPERATOR_FINISHED;
}

/* TODO, we could make this a menu into OBJECT_OT_vertex_group_set_active rather than its own operator */
void PAINT_OT_weight_sample_group(wmOperatorType *ot)
{
	PropertyRNA *prop = NULL;

	/* identifiers */
	ot->name = "Weight Paint Sample Group";
	ot->idname = "PAINT_OT_weight_sample_group";
	ot->description = "Select one of the vertex groups available under current mouse position";

	/* api callbacks */
	ot->exec = weight_sample_group_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = weight_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* keyingset to use (dynamic enum) */
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
	RNA_def_enum_funcs(prop, weight_paint_sample_enum_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

static void do_weight_paint_normalize_all(MDeformVert *dvert, const int defbase_tot, const bool *vgroup_validmap)
{
	float sum = 0.0f, fac;
	uint i, tot = 0;
	MDeformWeight *dw;

	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
			tot++;
			sum += dw->weight;
		}
	}

	if ((tot == 0) || (sum == 1.0f)) {
		return;
	}

	if (sum != 0.0f) {
		fac = 1.0f / sum;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				dw->weight *= fac;
			}
		}
	}
	else {
		/* hrmf, not a factor in this case */
		fac = 1.0f / tot;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				dw->weight = fac;
			}
		}
	}
}

/**
 * A version of #do_weight_paint_normalize_all that includes locked weights
 * but only changes unlocked weights.
 */
static bool do_weight_paint_normalize_all_locked(
        MDeformVert *dvert, const int defbase_tot, const bool *vgroup_validmap,
        const bool *lock_flags)
{
	float sum = 0.0f, fac;
	float sum_unlock = 0.0f;
	float lock_weight = 0.0f;
	uint i, tot = 0;
	MDeformWeight *dw;

	if (lock_flags == NULL) {
		do_weight_paint_normalize_all(dvert, defbase_tot, vgroup_validmap);
		return true;
	}

	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
			sum += dw->weight;

			if (lock_flags[dw->def_nr]) {
				lock_weight += dw->weight;
			}
			else {
				tot++;
				sum_unlock += dw->weight;
			}
		}
	}

	if (sum == 1.0f) {
		return true;
	}

	if (tot == 0) {
		return false;
	}

	if (lock_weight >= 1.0f) {
		/* locked groups make it impossible to fully normalize,
		 * zero out what we can and return false */
		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				if (lock_flags[dw->def_nr] == false) {
					dw->weight = 0.0f;
				}
			}
		}

		return (lock_weight == 1.0f);
	}
	else if (sum_unlock != 0.0f) {
		fac = (1.0f - lock_weight) / sum_unlock;

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				if (lock_flags[dw->def_nr] == false) {
					dw->weight *= fac;
					/* paranoid but possibly with float error */
					CLAMP(dw->weight, 0.0f, 1.0f);
				}
			}
		}
	}
	else {
		/* hrmf, not a factor in this case */
		fac = (1.0f - lock_weight) / tot;
		/* paranoid but possibly with float error */
		CLAMP(fac, 0.0f, 1.0f);

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				if (lock_flags[dw->def_nr] == false) {
					dw->weight = fac;
				}
			}
		}
	}

	return true;
}

/**
 * \note same as function above except it does a second pass without active group
 * if normalize fails with it.
 */
static void do_weight_paint_normalize_all_locked_try_active(
        MDeformVert *dvert, const int defbase_tot, const bool *vgroup_validmap,
        const bool *lock_flags, const bool *lock_with_active)
{
	/* first pass with both active and explicitly locked groups restricted from change */

	bool success = do_weight_paint_normalize_all_locked(dvert, defbase_tot, vgroup_validmap, lock_with_active);

	if (!success) {
		/**
		 * Locks prevented the first pass from full completion, so remove restriction on active group; e.g:
		 *
		 * - With 1.0 weight painted into active:
		 *   nonzero locked weight; first pass zeroed out unlocked weight; scale 1 down to fit.
		 * - With 0.0 weight painted into active:
		 *   no unlocked groups; first pass did nothing; increase 0 to fit.
		 */
		do_weight_paint_normalize_all_locked(dvert, defbase_tot, vgroup_validmap, lock_flags);
	}
}

#if 0 /* UNUSED */
static bool has_unselected_unlocked_bone_group(int defbase_tot, bool *defbase_sel, int selected,
                                               const bool *lock_flags, const bool *vgroup_validmap)
{
	int i;
	if (defbase_tot == selected) {
		return false;
	}
	for (i = 0; i < defbase_tot; i++) {
		if (vgroup_validmap[i] && !defbase_sel[i] && !lock_flags[i]) {
			return true;
		}
	}
	return false;
}
#endif

static void multipaint_clamp_change(
        MDeformVert *dvert, const int defbase_tot, const bool *defbase_sel,
        float *change_p)
{
	int i;
	MDeformWeight *dw;
	float val;
	float change = *change_p;

	/* verify that the change does not cause values exceeding 1 and clamp it */
	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot && defbase_sel[dw->def_nr]) {
			if (dw->weight) {
				val = dw->weight * change;
				if (val > 1) {
					change = 1.0f / dw->weight;
				}
			}
		}
	}

	*change_p = change;
}

static bool multipaint_verify_change(MDeformVert *dvert, const int defbase_tot, float change, const bool *defbase_sel)
{
	int i;
	MDeformWeight *dw;
	float val;

	/* in case the change is reduced, you need to recheck
	 * the earlier values to make sure they are not 0
	 * (precision error) */
	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot && defbase_sel[dw->def_nr]) {
			if (dw->weight) {
				val = dw->weight * change;
				/* the value should never reach zero while multi-painting if it
				 * was nonzero beforehand */
				if (val <= 0) {
					return false;
				}
			}
		}
	}

	return true;
}

static void multipaint_apply_change(MDeformVert *dvert, const int defbase_tot, float change, const bool *defbase_sel)
{
	int i;
	MDeformWeight *dw;

	/* apply the valid change */
	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot && defbase_sel[dw->def_nr]) {
			if (dw->weight) {
				dw->weight = dw->weight * change;
				CLAMP(dw->weight, 0.0f, 1.0f);
			}
		}
	}
}

/**
 * Variables stored both for 'active' and 'mirror' sides.
 */
struct WeightPaintGroupData {
	/** index of active group or its mirror
	 *
	 * - 'active' is always `ob->actdef`.
	 * - 'mirror' is -1 when 'ME_EDIT_MIRROR_X' flag id disabled,
	 *   otherwise this will be set to the mirror or the active group (if the group isn't mirrored).
	 */
	int index;
	/** lock that includes the 'index' as locked too
	 *
	 * - 'active' is set of locked or active/selected groups
	 * - 'mirror' is set of locked or mirror groups
	 */
	const bool *lock;
};

/* struct to avoid passing many args each call to do_weight_paint_vertex()
 * this _could_ be made a part of the operators 'WPaintData' struct, or at
 * least a member, but for now keep its own struct, initialized on every
 * paint stroke update - campbell */
typedef struct WeightPaintInfo {

	int defbase_tot;

	/* both must add up to 'defbase_tot' */
	int defbase_tot_sel;
	int defbase_tot_unsel;

	struct WeightPaintGroupData active, mirror;

	const bool *lock_flags;  /* boolean array for locked bones,
	                          * length of defbase_tot */
	const bool *defbase_sel; /* boolean array for selected bones,
	                          * length of defbase_tot, cant be const because of how its passed */

	const bool *vgroup_validmap; /* same as WeightPaintData.vgroup_validmap,
	                              * only added here for convenience */

	bool do_flip;
	bool do_multipaint;
	bool do_auto_normalize;

	float brush_alpha_value;  /* result of BKE_brush_alpha_get() */
} WeightPaintInfo;

static void do_weight_paint_vertex_single(
        /* vars which remain the same for every vert */
        VPaint *wp, Object *ob, const WeightPaintInfo *wpi,
        /* vars which change on each stroke */
        const uint index, float alpha, float paintweight
        )
{
	Mesh *me = ob->data;
	MDeformVert *dv = &me->dvert[index];
	bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

	MDeformWeight *dw;

	/* mirror vars */
	int index_mirr;
	int vgroup_mirr;

	MDeformVert *dv_mirr;
	MDeformWeight *dw_mirr;

	if (wp->flag & VP_ONLYVGROUP) {
		dw = defvert_find_index(dv, wpi->active.index);
	}
	else {
		dw = defvert_verify_index(dv, wpi->active.index);
	}

	if (dw == NULL) {
		return;
	}


	/* from now on we can check if mirrors enabled if this var is -1 and not bother with the flag */
	if (me->editflag & ME_EDIT_MIRROR_X) {
		index_mirr = mesh_get_x_mirror_vert(ob, NULL, index, topology);
		vgroup_mirr = wpi->mirror.index;

		/* another possible error - mirror group _and_ active group are the same (which is fine),
		 * but we also are painting onto a center vertex - this would paint the same weight twice */
		if (index_mirr == index && vgroup_mirr == wpi->active.index) {
			index_mirr = vgroup_mirr = -1;
		}
	}
	else {
		index_mirr = vgroup_mirr = -1;
	}


	/* get the mirror def vars */
	if (index_mirr != -1) {
		dv_mirr = &me->dvert[index_mirr];
		if (wp->flag & VP_ONLYVGROUP) {
			dw_mirr = defvert_find_index(dv_mirr, vgroup_mirr);

			if (dw_mirr == NULL) {
				index_mirr = vgroup_mirr = -1;
				dv_mirr = NULL;
			}
		}
		else {
			if (index != index_mirr) {
				dw_mirr = defvert_verify_index(dv_mirr, vgroup_mirr);
			}
			else {
				/* dv and dv_mirr are the same */
				int totweight_prev = dv_mirr->totweight;
				int dw_offset = (int)(dw - dv_mirr->dw);
				dw_mirr = defvert_verify_index(dv_mirr, vgroup_mirr);

				/* if we added another, get our old one back */
				if (totweight_prev != dv_mirr->totweight) {
					dw = &dv_mirr->dw[dw_offset];
				}
			}
		}
	}
	else {
		dv_mirr = NULL;
		dw_mirr = NULL;
	}

	/* If there are no normalize-locks or multipaint,
	 * then there is no need to run the more complicated checks */

	{
		dw->weight = wpaint_blend(wp, dw->weight, alpha, paintweight,
		                          wpi->brush_alpha_value, wpi->do_flip);

		/* WATCH IT: take care of the ordering of applying mirror -> normalize,
		 * can give wrong results [#26193], least confusing if normalize is done last */

		/* apply mirror */
		if (index_mirr != -1) {
			/* copy, not paint again */
			dw_mirr->weight = dw->weight;
		}

		/* apply normalize */
		if (wpi->do_auto_normalize) {
			/* note on normalize - this used to be applied after painting and normalize all weights,
			 * in some ways this is good because there is feedback where the more weights involved would
			 * 'resist' so you couldn't instantly zero out other weights by painting 1.0 on the active.
			 *
			 * However this gave a problem since applying mirror, then normalize both verts
			 * the resulting weight wont match on both sides.
			 *
			 * If this 'resisting', slower normalize is nicer, we could call
			 * do_weight_paint_normalize_all() and only use...
			 * do_weight_paint_normalize_all_active() when normalizing the mirror vertex.
			 * - campbell
			 */
			do_weight_paint_normalize_all_locked_try_active(
			        dv, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->active.lock);

			if (index_mirr != -1) {
				/* only normalize if this is not a center vertex, else we get a conflict, normalizing twice */
				if (index != index_mirr) {
					do_weight_paint_normalize_all_locked_try_active(
					        dv_mirr, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->mirror.lock);
				}
				else {
					/* this case accounts for...
					 * - painting onto a center vertex of a mesh
					 * - x mirror is enabled
					 * - auto normalize is enabled
					 * - the group you are painting onto has a L / R version
					 *
					 * We want L/R vgroups to have the same weight but this cant be if both are over 0.5,
					 * We _could_ have special check for that, but this would need its own normalize function which
					 * holds 2 groups from changing at once.
					 *
					 * So! just balance out the 2 weights, it keeps them equal and everything normalized.
					 *
					 * While it wont hit the desired weight immediately as the user waggles their mouse,
					 * constant painting and re-normalizing will get there. this is also just simpler logic.
					 * - campbell */
					dw_mirr->weight = dw->weight = (dw_mirr->weight + dw->weight) * 0.5f;
				}
			}
		}
	}
}

static void do_weight_paint_vertex_multi(
        /* vars which remain the same for every vert */
        VPaint *wp, Object *ob, const WeightPaintInfo *wpi,
        /* vars which change on each stroke */
        const uint index, float alpha, float paintweight)
{
	Mesh *me = ob->data;
	MDeformVert *dv = &me->dvert[index];
	bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

	/* mirror vars */
	int index_mirr = -1;
	MDeformVert *dv_mirr = NULL;

	/* weights */
	float curw, neww, change, curw_mirr, change_mirr;

	/* from now on we can check if mirrors enabled if this var is -1 and not bother with the flag */
	if (me->editflag & ME_EDIT_MIRROR_X) {
		index_mirr = mesh_get_x_mirror_vert(ob, NULL, index, topology);

		if (index_mirr != -1 && index_mirr != index) {
			dv_mirr = &me->dvert[index_mirr];
		}
	}

	/* compute weight change by applying the brush to average or sum of group weights */
	curw = BKE_defvert_multipaint_collective_weight(
	        dv, wpi->defbase_tot, wpi->defbase_sel, wpi->defbase_tot_sel, wpi->do_auto_normalize);

	if (curw == 0.0f) {
		/* note: no weight to assign to this vertex, could add all groups? */
		return;
	}

	neww = wpaint_blend(wp, curw, alpha, paintweight, wpi->brush_alpha_value, wpi->do_flip);

	change = neww / curw;

	/* verify for all groups that 0 < result <= 1 */
	multipaint_clamp_change(dv, wpi->defbase_tot, wpi->defbase_sel, &change);

	if (dv_mirr != NULL) {
		curw_mirr = BKE_defvert_multipaint_collective_weight(
		        dv_mirr, wpi->defbase_tot, wpi->defbase_sel, wpi->defbase_tot_sel, wpi->do_auto_normalize);

		if (curw_mirr == 0.0f) {
			/* can't mirror into a zero weight vertex */
			dv_mirr = NULL;
		}
		else {
			/* mirror is changed to achieve the same collective weight value */
			float orig = change_mirr = curw * change / curw_mirr;

			multipaint_clamp_change(dv_mirr, wpi->defbase_tot, wpi->defbase_sel, &change_mirr);

			if (!multipaint_verify_change(dv_mirr, wpi->defbase_tot, change_mirr, wpi->defbase_sel)) {
				return;
			}

			change *= change_mirr / orig;
		}
	}

	if (!multipaint_verify_change(dv, wpi->defbase_tot, change, wpi->defbase_sel)) {
		return;
	}

	/* apply validated change to vertex and mirror */
	multipaint_apply_change(dv, wpi->defbase_tot, change, wpi->defbase_sel);

	if (dv_mirr != NULL) {
		multipaint_apply_change(dv_mirr, wpi->defbase_tot, change_mirr, wpi->defbase_sel);
	}

	/* normalize */
	if (wpi->do_auto_normalize) {
		do_weight_paint_normalize_all_locked_try_active(
		        dv, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->active.lock);

		if (dv_mirr != NULL) {
			do_weight_paint_normalize_all_locked_try_active(
			        dv_mirr, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags, wpi->active.lock);
		}
	}
}

static void do_weight_paint_vertex(
        /* vars which remain the same for every vert */
        VPaint *wp, Object *ob, const WeightPaintInfo *wpi,
        /* vars which change on each stroke */
        const uint index, float alpha, float paintweight)
{
	if (wpi->do_multipaint) {
		do_weight_paint_vertex_multi(wp, ob, wpi, index, alpha, paintweight);
	}
	else {
		do_weight_paint_vertex_single(wp, ob, wpi, index, alpha, paintweight);
	}
}


/* Toggle operator for turning vertex paint mode on or off (copied from sculpt.c) */
static void vertex_paint_init_session(const EvaluationContext *eval_ctx, Scene *scene, Object *ob)
{
	if (ob->sculpt == NULL) {
		ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");
		BKE_sculpt_update_mesh_elements(eval_ctx, scene, scene->toolsettings->sculpt, ob, 0, false);
	}
}

static void vertex_paint_init_session_data(const ToolSettings *ts, Object *ob)
{
	/* Create maps */
	struct SculptVertexPaintGeomMap *gmap = NULL;
	if (ob->mode == OB_MODE_VERTEX_PAINT) {
		gmap = &ob->sculpt->mode.vpaint.gmap;
		ob->sculpt->mode_type = OB_MODE_VERTEX_PAINT;
	}
	else if (ob->mode == OB_MODE_WEIGHT_PAINT) {
		gmap = &ob->sculpt->mode.wpaint.gmap;
		ob->sculpt->mode_type = OB_MODE_WEIGHT_PAINT;
	}
	else {
		ob->sculpt->mode_type = 0;
		BLI_assert(0);
		return;
	}

	Mesh *me = ob->data;

	if (gmap->vert_to_loop == NULL) {
		gmap->vert_map_mem = NULL;
		gmap->vert_to_loop = NULL;
		gmap->poly_map_mem = NULL;
		gmap->vert_to_poly = NULL;
		BKE_mesh_vert_loop_map_create(
		        &gmap->vert_to_loop,
		        &gmap->vert_map_mem,
		        me->mpoly, me->mloop, me->totvert, me->totpoly, me->totloop);
		BKE_mesh_vert_poly_map_create(
		        &gmap->vert_to_poly,
		        &gmap->poly_map_mem,
		        me->mpoly, me->mloop, me->totvert, me->totpoly, me->totloop);
	}

	/* Create average brush arrays */
	if (ob->mode == OB_MODE_VERTEX_PAINT) {
		if ((ts->vpaint->flag & VP_SPRAY) == 0) {
			if (ob->sculpt->mode.vpaint.previous_color == NULL) {
				ob->sculpt->mode.vpaint.previous_color =
				        MEM_callocN(me->totloop * sizeof(uint), "previous_color");
			}
		}
		else {
			MEM_SAFE_FREE(ob->sculpt->mode.vpaint.previous_color);
		}
	}
	else if (ob->mode == OB_MODE_WEIGHT_PAINT) {
		if ((ts->wpaint->flag & VP_SPRAY) == 0) {
			if (ob->sculpt->mode.wpaint.alpha_weight == NULL) {
				ob->sculpt->mode.wpaint.alpha_weight =
				        MEM_callocN(me->totvert * sizeof(float), "alpha_weight");
			}
			if (ob->sculpt->mode.wpaint.previous_weight == NULL) {
				ob->sculpt->mode.wpaint.previous_weight =
				        MEM_mallocN(me->totvert * sizeof(float), "previous_weight");
			}
		}
		else {
			MEM_SAFE_FREE(ob->sculpt->mode.wpaint.alpha_weight);
			MEM_SAFE_FREE(ob->sculpt->mode.wpaint.previous_weight);
		}
	}

}

/* *************** set wpaint operator ****************** */

/**
 * \note Keep in sync with #vpaint_mode_toggle_exec
 */
static int wpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{		
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_WEIGHT_PAINT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;
	Scene *scene = CTX_data_scene(C);
	VPaint *wp = scene->toolsettings->wpaint;
	Mesh *me;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	me = BKE_mesh_from_object(ob);

	if (ob->mode & mode_flag) {
		ob->mode &= ~mode_flag;

		if (me->editflag & ME_EDIT_PAINT_VERT_SEL) {
			BKE_mesh_flush_select_from_verts(me);
		}
		else if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
			BKE_mesh_flush_select_from_polys(me);
		}

		/* weight paint specific */
		ED_mesh_mirror_spatial_table(NULL, NULL, NULL, NULL, 'e');
		ED_mesh_mirror_topo_table(NULL, NULL, 'e');

		/* If the cache is not released by a cancel or a done, free it now. */
		if (ob->sculpt->cache) {
			sculpt_cache_free(ob->sculpt->cache);
			ob->sculpt->cache = NULL;
		}

		BKE_sculptsession_free(ob);

		paint_cursor_delete_textures();
	}
	else {
		EvaluationContext eval_ctx;

		CTX_data_eval_ctx(C, &eval_ctx);

		ob->mode |= mode_flag;

		if (wp == NULL)
			wp = scene->toolsettings->wpaint = new_vpaint(1);

		paint_cursor_start(C, weight_paint_poll);

		BKE_paint_init(scene, ePaintWeight, PAINT_CURSOR_WEIGHT_PAINT);

		/* weight paint specific */
		ED_mesh_mirror_spatial_table(ob, NULL, NULL, NULL, 's');
		ED_vgroup_sync_from_pose(ob);

		/* Create vertex/weight paint mode session data */
		if (ob->sculpt) {
			BKE_sculptsession_free(ob);
		}
		vertex_paint_init_session(&eval_ctx, scene, ob);
	}

	BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
	
	/* Weightpaint works by overriding colors in mesh,
	 * so need to make sure we recalc on enter and
	 * exit (exit needs doing regardless because we
	 * should redeform).
	 */
	DEG_id_tag_update(&me->id, 0);

	WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

	return OPERATOR_FINISHED;
}

/* for switching to/from mode */
static int paint_poll_test(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if (ob == NULL || ob->type != OB_MESH)
		return 0;
	if (!ob->data || ID_IS_LINKED_DATABLOCK(ob->data))
		return 0;
	if (CTX_data_edit_object(C))
		return 0;
	return 1;
}

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Weight Paint Mode";
	ot->idname = "PAINT_OT_weight_paint_toggle";
	ot->description = "Toggle weight paint mode in 3D view";
	
	/* api callbacks */
	ot->exec = wpaint_mode_toggle_exec;
	ot->poll = paint_poll_test;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
}

/* ************ weight paint operator ********** */

enum eWPaintFlag {
	WPAINT_ENSURE_MIRROR = (1 << 0),
};

struct WPaintVGroupIndex {
	int active;
	int mirror;
};

struct WPaintData {
	ViewContext vc;

	struct WeightPaintGroupData active, mirror;

	void *vp_handle;
	DMCoNo *vertexcosnos;

	float wpimat[3][3];

	/* variables for auto normalize */
	const bool *vgroup_validmap; /* stores if vgroups tie to deforming bones or not */
	const bool *lock_flags;

	/* variables for multipaint */
	const bool *defbase_sel;      /* set of selected groups */
	int defbase_tot_sel;          /* number of selected groups */
	bool do_multipaint;           /* true if multipaint enabled and multiple groups selected */

	int defbase_tot;
};

/* ensure we have data on wpaint start, add if needed */
static bool wpaint_ensure_data(
        bContext *C, wmOperator *op,
        enum eWPaintFlag flag, struct WPaintVGroupIndex *vgroup_index)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = BKE_mesh_from_object(ob);

	if (vgroup_index) {
		vgroup_index->active = -1;
		vgroup_index->mirror = -1;
	}

	if (scene->obedit) {
		return false;
	}

	if (me == NULL || me->totpoly == 0) {
		return false;
	}

	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert) {
		BKE_object_defgroup_data_create(&me->id);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
	}

	/* this happens on a Bone select, when no vgroup existed yet */
	if (ob->actdef <= 0) {
		Object *modob;
		if ((modob = modifiers_isDeformedByArmature(ob))) {
			Bone *actbone = ((bArmature *)modob->data)->act_bone;
			if (actbone) {
				bPoseChannel *pchan = BKE_pose_channel_find_name(modob->pose, actbone->name);

				if (pchan) {
					bDeformGroup *dg = defgroup_find_name(ob, pchan->name);
					if (dg == NULL) {
						dg = BKE_object_defgroup_add_name(ob, pchan->name);  /* sets actdef */
					}
					else {
						int actdef = 1 + BLI_findindex(&ob->defbase, dg);
						BLI_assert(actdef >= 0);
						ob->actdef = actdef;
					}
				}
			}
		}
	}
	if (BLI_listbase_is_empty(&ob->defbase)) {
		BKE_object_defgroup_add(ob);
	}

	/* ensure we don't try paint onto an invalid group */
	if (ob->actdef <= 0) {
		BKE_report(op->reports, RPT_WARNING, "No active vertex group for painting, aborting");
		return false;
	}

	if (vgroup_index) {
		vgroup_index->active = ob->actdef - 1;
	}

	if (flag & WPAINT_ENSURE_MIRROR) {
		if (me->editflag & ME_EDIT_MIRROR_X) {
			int mirror = wpaint_mirror_vgroup_ensure(ob, ob->actdef - 1);
			if (vgroup_index) {
				vgroup_index->mirror = mirror;
			}
		}
	}

	return true;
}

/* Initialize the stroke cache invariants from operator properties */
static void vwpaint_update_cache_invariants(
        bContext *C, VPaint *vd, SculptSession *ss, wmOperator *op, const float mouse[2])
{
	StrokeCache *cache;
	Scene *scene = CTX_data_scene(C);
	UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
	Brush *brush = BKE_paint_brush(&vd->paint);
	ViewContext *vc = paint_stroke_view_context(op->customdata);
	Object *ob = CTX_data_active_object(C);
	float mat[3][3];
	float view_dir[3] = {0.0f, 0.0f, 1.0f};
	int mode;

	/* VW paint needs to allocate stroke cache before update is called. */
	if (!ss->cache) {
		cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
		ss->cache = cache;
	}
	else {
		cache = ss->cache;
	}

	/* Initial mouse location */
	if (mouse)
		copy_v2_v2(cache->initial_mouse, mouse);
	else
		zero_v2(cache->initial_mouse);

	mode = RNA_enum_get(op->ptr, "mode");
	cache->invert = mode == BRUSH_STROKE_INVERT;
	cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
	/* not very nice, but with current events system implementation
	* we can't handle brush appearance inversion hotkey separately (sergey) */
	if (cache->invert) ups->draw_inverted = true;
	else ups->draw_inverted = false;

	copy_v2_v2(cache->mouse, cache->initial_mouse);
	/* Truly temporary data that isn't stored in properties */
	cache->vc = vc;
	cache->brush = brush;
	cache->first_time = 1;

	/* cache projection matrix */
	ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

	invert_m4_m4(ob->imat, ob->obmat);
	copy_m3_m4(mat, cache->vc->rv3d->viewinv);
	mul_m3_v3(mat, view_dir);
	copy_m3_m4(mat, ob->imat);
	mul_m3_v3(mat, view_dir);
	normalize_v3_v3(cache->true_view_normal, view_dir);

	copy_v3_v3(cache->view_normal, cache->true_view_normal);
	cache->bstrength = BKE_brush_alpha_get(scene, brush);
	cache->is_last_valid = false;
}

/* Initialize the stroke cache variants from operator properties */
static void vwpaint_update_cache_variants(bContext *C, VPaint *vd, Object *ob, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	Brush *brush = BKE_paint_brush(&vd->paint);

	/* This effects the actual brush radius, so things farther away
	 * are compared with a larger radius and vise versa. */
	if (cache->first_time) {
		RNA_float_get_array(ptr, "location", cache->true_location);
	}

	RNA_float_get_array(ptr, "mouse", cache->mouse);

	/* XXX: Use pressure value from first brush step for brushes which don't
	 * support strokes (grab, thumb). They depends on initial state and
	 * brush coord/pressure/etc.
	 * It's more an events design issue, which doesn't split coordinate/pressure/angle
	 * changing events. We should avoid this after events system re-design */
	if (paint_supports_dynamic_size(brush, ePaintSculpt) || cache->first_time) {
		cache->pressure = RNA_float_get(ptr, "pressure");
	}

	/* Truly temporary data that isn't stored in properties */
	if (cache->first_time) {
		if (!BKE_brush_use_locked_size(scene, brush)) {
			cache->initial_radius = paint_calc_object_space_radius(
			        cache->vc, cache->true_location, BKE_brush_size_get(scene, brush));
			BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
		}
		else {
			cache->initial_radius = BKE_brush_unprojected_radius_get(scene, brush);
		}
	}

	if (BKE_brush_use_size_pressure(scene, brush) && paint_supports_dynamic_size(brush, ePaintSculpt)) {
		cache->radius = cache->initial_radius * cache->pressure;
	}
	else {
		cache->radius = cache->initial_radius;
	}

	cache->radius_squared = cache->radius * cache->radius;

	if (ss->pbvh) {
		BKE_pbvh_update(ss->pbvh, PBVH_UpdateRedraw, NULL);
		BKE_pbvh_update(ss->pbvh, PBVH_UpdateBB, NULL);
	}
}

static bool wpaint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
	Scene *scene = CTX_data_scene(C);
	struct PaintStroke *stroke = op->customdata;
	ToolSettings *ts = scene->toolsettings;
	Object *ob = CTX_data_active_object(C);
	Mesh *me = BKE_mesh_from_object(ob);
	struct WPaintData *wpd;
	struct WPaintVGroupIndex vgroup_index;
	int defbase_tot, defbase_tot_sel;
	bool *defbase_sel;
	SculptSession *ss = ob->sculpt;
	VPaint *vd = CTX_data_tool_settings(C)->wpaint;
	EvaluationContext eval_ctx;

	float mat[4][4], imat[4][4];

	if (wpaint_ensure_data(C, op, WPAINT_ENSURE_MIRROR, &vgroup_index) == false) {
		return false;
	}

	CTX_data_eval_ctx(C, &eval_ctx);

	{
		/* check if we are attempting to paint onto a locked vertex group,
		 * and other options disallow it from doing anything useful */
		bDeformGroup *dg;
		dg = BLI_findlink(&ob->defbase, vgroup_index.active);
		if (dg->flag & DG_LOCK_WEIGHT) {
			BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
			return false;
		}
		if (vgroup_index.mirror != -1) {
			dg = BLI_findlink(&ob->defbase, vgroup_index.mirror);
			if (dg->flag & DG_LOCK_WEIGHT) {
				BKE_report(op->reports, RPT_WARNING, "Mirror group is locked, aborting");
				return false;
			}
		}
	}

	/* check that multipaint groups are unlocked */
	defbase_tot = BLI_listbase_count(&ob->defbase);
	defbase_sel = BKE_object_defgroup_selected_get(ob, defbase_tot, &defbase_tot_sel);

	if (ts->multipaint && defbase_tot_sel > 1) {
		int i;
		bDeformGroup *dg;

		if (me->editflag & ME_EDIT_MIRROR_X) {
			BKE_object_defgroup_mirror_selection(ob, defbase_tot, defbase_sel, defbase_sel, &defbase_tot_sel);
		}

		for (i = 0; i < defbase_tot; i++) {
			if (defbase_sel[i]) {
				dg = BLI_findlink(&ob->defbase, i);
				if (dg->flag & DG_LOCK_WEIGHT) {
					BKE_report(op->reports, RPT_WARNING, "Multipaint group is locked, aborting");
					MEM_freeN(defbase_sel);
					return false;
				}
			}
		}
	}

	/* ALLOCATIONS! no return after this line */
	/* make mode data storage */
	wpd = MEM_callocN(sizeof(struct WPaintData), "WPaintData");
	paint_stroke_set_mode_data(stroke, wpd);
	view3d_set_viewcontext(C, &wpd->vc);

	wpd->active.index = vgroup_index.active;
	wpd->mirror.index = vgroup_index.mirror;

	/* multipaint */
	wpd->defbase_tot = defbase_tot;
	wpd->defbase_sel = defbase_sel;
	wpd->defbase_tot_sel = defbase_tot_sel > 1 ? defbase_tot_sel : 1;
	wpd->do_multipaint = (ts->multipaint && defbase_tot_sel > 1);

	/* set up auto-normalize, and generate map for detecting which
	 * vgroups affect deform bones */
	wpd->lock_flags = BKE_object_defgroup_lock_flags_get(ob, wpd->defbase_tot);
	if (ts->auto_normalize || ts->multipaint || wpd->lock_flags) {
		wpd->vgroup_validmap = BKE_object_defgroup_validmap_get(ob, wpd->defbase_tot);
	}

	if (wpd->do_multipaint && ts->auto_normalize) {
		bool *tmpflags;
		tmpflags = MEM_mallocN(sizeof(bool) * defbase_tot, __func__);
		if (wpd->lock_flags) {
			BLI_array_binary_or(tmpflags, wpd->defbase_sel, wpd->lock_flags, wpd->defbase_tot);
		}
		else {
			memcpy(tmpflags, wpd->defbase_sel, sizeof(*tmpflags) * wpd->defbase_tot);
		}
		wpd->active.lock = tmpflags;
	}
	else if (ts->auto_normalize) {
		bool *tmpflags;

		tmpflags = wpd->lock_flags ?
		        MEM_dupallocN(wpd->lock_flags) :
		        MEM_callocN(sizeof(bool) * defbase_tot, __func__);
		tmpflags[wpd->active.index] = true;
		wpd->active.lock = tmpflags;

		tmpflags = wpd->lock_flags ?
		        MEM_dupallocN(wpd->lock_flags) :
		        MEM_callocN(sizeof(bool) * defbase_tot, __func__);
		tmpflags[(wpd->mirror.index != -1) ? wpd->mirror.index : wpd->active.index] = true;
		wpd->mirror.lock = tmpflags;
	}

	/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	ob->sculpt->building_vp_handle = true;
	wpd->vp_handle = ED_vpaint_proj_handle_create(&eval_ctx, scene, ob, &wpd->vertexcosnos);
	ob->sculpt->building_vp_handle = false;

	/* imat for normals */
	mul_m4_m4m4(mat, wpd->vc.rv3d->viewmat, ob->obmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(wpd->wpimat, imat);

	/* If not previously created, create vertex/weight paint mode session data */
	vertex_paint_init_session(&eval_ctx, scene, ob);
	vwpaint_update_cache_invariants(C, vd, ss, op, mouse);
	vertex_paint_init_session_data(ts, ob);

	if (ss->mode.wpaint.previous_weight != NULL) {
		copy_vn_fl(ss->mode.wpaint.previous_weight, me->totvert, -1.0f);
	}

	return true;
}

static void calc_area_normal_and_center_task_cb(void *userdata, const int n)
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	float(*area_nos)[3] = data->area_nos;
	float(*area_cos)[3] = data->area_cos;

	float private_co[2][3] = {{0.0f}};
	float private_no[2][3] = {{0.0f}};
	int   private_count[2] = {0};

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		const float *co;

		co = vd.co;

		if (sculpt_brush_test_fast(&test, co)) {
			float no_buf[3];
			const float *no;
			int flip_index;

			if (vd.no) {
				normal_short_to_float_v3(no_buf, vd.no);
				no = no_buf;
			}
			else {
				no = vd.fno;
			}

			flip_index = (dot_v3v3(ss->cache->view_normal, no) <= 0.0f);
			if (area_cos)
				add_v3_v3(private_co[flip_index], co);
			if (area_nos)
				add_v3_v3(private_no[flip_index], no);
			private_count[flip_index] += 1;
		}
	}
	BKE_pbvh_vertex_iter_end;


	BLI_mutex_lock(&data->mutex);

	/* for flatten center */
	if (area_cos) {
		add_v3_v3(area_cos[0], private_co[0]);
		add_v3_v3(area_cos[1], private_co[1]);
	}

	/* for area normal */
	if (area_nos) {
		add_v3_v3(area_nos[0], private_no[0]);
		add_v3_v3(area_nos[1], private_no[1]);
	}

	/* weights */
	data->count[0] += private_count[0];
	data->count[1] += private_count[1];

	BLI_mutex_unlock(&data->mutex);
}

static void calc_area_normal(
        VPaint *vp, Object *ob,
        PBVHNode **nodes, int totnode,
        float r_area_no[3])
{
	/* 0=towards view, 1=flipped */
	float area_nos[2][3] = {{0.0f}};

	int count[2] = {0};

	SculptThreadedTaskData data = {
		.vp = vp, .ob = ob, .nodes = nodes, .totnode = totnode,
		.area_cos = NULL, .area_nos = area_nos, .count = count,
	};
	BLI_mutex_init(&data.mutex);

	BLI_task_parallel_range(
	        0, totnode, &data, calc_area_normal_and_center_task_cb, true);

	BLI_mutex_end(&data.mutex);

	/* for area normal */
	for (int i = 0; i < ARRAY_SIZE(area_nos); i++) {
		if (normalize_v3_v3(r_area_no, area_nos[i]) != 0.0f) {
			break;
		}
	}
}

static float dot_vf3vs3(const float brushNormal[3], const short vertexNormal[3])
{
	float normal[3];
	normal_short_to_float_v3(normal, vertexNormal);
	return dot_v3v3(brushNormal, normal);
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
 * calculate multiple modifications to the mesh when symmetry is enabled. */
static void calc_brushdata_symm(
        VPaint *vd, StrokeCache *cache, const char symm,
        const char axis, const float angle)
{
	(void)vd; /* unused */

	flip_v3_v3(cache->location, cache->true_location, symm);
	flip_v3_v3(cache->last_location, cache->true_last_location, symm);
	flip_v3_v3(cache->grab_delta_symmetry, cache->grab_delta, symm);
	flip_v3_v3(cache->view_normal, cache->true_view_normal, symm);

	unit_m4(cache->symm_rot_mat);
	unit_m4(cache->symm_rot_mat_inv);
	zero_v3(cache->plane_offset);

	if (axis) { /* expects XYZ */
		rotate_m4(cache->symm_rot_mat, axis, angle);
		rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
	}

	mul_m4_v3(cache->symm_rot_mat, cache->location);
	mul_m4_v3(cache->symm_rot_mat, cache->last_location);
	mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);

	if (cache->supports_gravity) {
		flip_v3_v3(cache->gravity_direction, cache->true_gravity_direction, symm);
		mul_m4_v3(cache->symm_rot_mat, cache->gravity_direction);
	}

	if (cache->is_rake_rotation_valid) {
		flip_qt_qt(cache->rake_rotation_symmetry, cache->rake_rotation, symm);
	}
}

static void get_brush_alpha_data(
        Scene *scene, SculptSession *ss, Brush *brush,
        float *r_brush_size_pressure, float *r_brush_alpha_value, float *r_brush_alpha_pressure)
{
	*r_brush_size_pressure =
	        BKE_brush_size_get(scene, brush) *
	        (BKE_brush_use_size_pressure(scene, brush) ? ss->cache->pressure : 1.0f);
	*r_brush_alpha_value =
	        BKE_brush_alpha_get(scene, brush);
	*r_brush_alpha_pressure =
	        *r_brush_alpha_value *
	        (BKE_brush_use_alpha_pressure(scene, brush) ? ss->cache->pressure : 1.0f);
}

static void do_wpaint_brush_blur_task_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);
	const struct SculptVertexPaintGeomMap *gmap = &ss->mode.wpaint.gmap;

	Brush *brush = data->brush;
	StrokeCache *cache = ss->cache;
	Scene *scene = CTX_data_scene(data->C);

	const float brush_strength = cache->bstrength;
	float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
	get_brush_alpha_data(scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	/* For each vertex */
	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		/* Test to see if the vertex coordinates are within the spherical brush region. */
		if (sculpt_brush_test_sq(&test, vd.co)) {
			/* For grid based pbvh, take the vert whose loop coopresponds to the current grid.
			 * Otherwise, take the current vert. */
			const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
			const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;
			const char v_flag = data->me->mvert[v_index].flag;
			/* If the vertex is selected */
			if (!(use_face_sel || use_vert_sel) || v_flag & SELECT) {
				/* Get the average poly weight */
				int total_hit_loops = 0;
				float weight_final = 0.0f;
				for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
					const int p_index = gmap->vert_to_poly[v_index].indices[j];
					const MPoly *mp = &data->me->mpoly[p_index];

					total_hit_loops += mp->totloop;
					for (int k = 0; k < mp->totloop; k++) {
						const int l_index = mp->loopstart + k;
						const MLoop *ml = &data->me->mloop[l_index];
						const MDeformVert *dv = &data->me->dvert[ml->v];
						weight_final += defvert_find_weight(dv, data->wpi->active.index);
					}
				}

				/* Apply the weight to the vertex. */
				if (total_hit_loops != 0) {
					const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
					if (view_dot > 0.0f) {
						const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);
						const float final_alpha =
						        view_dot * brush_fade * brush_strength *
						        grid_alpha * brush_alpha_pressure;
						weight_final /= total_hit_loops;

						/* Only paint visable verts */
						do_weight_paint_vertex(
						        data->vp, data->ob, data->wpi,
						        v_index, final_alpha, weight_final);
					}
				}
			}
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void do_wpaint_brush_smear_task_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);
	const struct SculptVertexPaintGeomMap *gmap = &ss->mode.wpaint.gmap;

	Brush *brush = data->brush;
	Scene *scene = CTX_data_scene(data->C);
	StrokeCache *cache = ss->cache;
	const float brush_strength = cache->bstrength;
	float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
	get_brush_alpha_data(scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
	float brush_dir[3];

	sub_v3_v3v3(brush_dir, cache->location, cache->last_location);
	if (normalize_v3(brush_dir) != 0.0f) {

		SculptBrushTest test;
		sculpt_brush_test_init(ss, &test);

		/* For each vertex */
		PBVHVertexIter vd;
		BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			/* Test to see if the vertex coordinates are within the spherical brush region. */
			if (sculpt_brush_test_fast(&test, vd.co)) {
				const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
				if (view_dot > 0.0f) {
					bool do_color = false;

					/* For grid based pbvh, take the vert whose loop cooresponds to the current grid.
					 * Otherwise, take the current vert. */
					const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
					const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;
					const MVert *mv_curr = &data->me->mvert[v_index];
					const char v_flag = data->me->mvert[v_index].flag;

					/* If the vertex is selected */
					if (!(use_face_sel || use_vert_sel) || v_flag & SELECT) {
						/* Minimum dot product between brush direction and current
						 * to neighbor direction is 0.0, meaning orthogonal. */
						float stroke_dot_max = 0.0f;

						/* Get the color of the loop in the opposite direction of the brush movement
						 * (this callback is specifically for smear.) */
						float weight_final = 0.0;
						for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
							const int p_index = gmap->vert_to_poly[v_index].indices[j];
							const MPoly *mp = &data->me->mpoly[p_index];
							for (int k = 0; k < mp->totloop; k++) {
								const uint l_index = mp->loopstart + k;
								const MLoop *ml = &data->me->mloop[l_index];
								const uint v_other_index = ml->v;
								const MVert *mv_other = &data->me->mvert[v_other_index];

								/* Get the direction from the selected vert to the neighbor. */
								float other_dir[3];
								sub_v3_v3v3(other_dir, mv_curr->co, mv_other->co);
								normalize_v3(other_dir);

								const float stroke_dot = dot_v3v3(other_dir, brush_dir);

								if (stroke_dot > stroke_dot_max) {
									stroke_dot_max = stroke_dot;
									MDeformVert *dv = &data->me->dvert[v_other_index];
									weight_final = defvert_find_weight(dv, data->wpi->active.index);
									do_color = true;
								}
							}
						}
						/* Apply weight to vertex */
						if (do_color) {
							const float brush_fade = BKE_brush_curve_strength(brush, test.dist, cache->radius);
							const float final_alpha =
							        view_dot * brush_fade * brush_strength *
							        grid_alpha * brush_alpha_pressure;
							do_weight_paint_vertex(
							        data->vp, data->ob, data->wpi,
							        v_index, final_alpha, (float)weight_final);
						}
					}
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void do_wpaint_brush_draw_task_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);
	Scene *scene = CTX_data_scene(data->C);

	Brush *brush = data->brush;
	StrokeCache *cache = ss->cache;
	const float brush_strength = cache->bstrength;
	const float paintweight = BKE_brush_weight_get(scene, brush);
	float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
	get_brush_alpha_data(scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	/* For each vertex */
	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		/* Test to see if the vertex coordinates are within the spherical brush region. */
		if (sculpt_brush_test_sq(&test, vd.co)) {
			/* Note: grids are 1:1 with corners (aka loops).
			 * For multires, take the vert whose loop cooresponds to the current grid.
			 * Otherwise, take the current vert. */
			const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
			const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;

			const char v_flag = data->me->mvert[v_index].flag;
			/* If the vertex is selected */
			if (!(use_face_sel || use_vert_sel) || v_flag & SELECT) {
				const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
				if (view_dot > 0.0f) {
					const float brush_fade = BKE_brush_curve_strength(brush, sqrtf(test.dist), cache->radius);
					float final_alpha = view_dot * brush_fade * brush_strength * grid_alpha * brush_alpha_pressure;

					/* Non-spray logic. */
					if ((data->vp->flag & VP_SPRAY) == 0) {
						/* Only paint if we have greater alpha. */
						if (ss->mode.wpaint.alpha_weight[v_index] < final_alpha) {
							ss->mode.wpaint.alpha_weight[v_index] = final_alpha;
						}
						else {
							continue;
						}

						MDeformVert *dv = &data->me->dvert[v_index];
						MDeformWeight *dw = defvert_find_index(dv, data->wpi->active.index);
						float *weight_prev = &ss->mode.wpaint.previous_weight[v_index];
						defweight_prev_init(dw, weight_prev);
						if (dw) {
							dw->weight = *weight_prev;
						}
					}

					do_weight_paint_vertex(
					        data->vp, data->ob, data->wpi,
					        v_index, final_alpha, paintweight);
				}
			}
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void do_wpaint_brush_calc_average_weight_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	StrokeCache *cache = ss->cache;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);

	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	const bool use_vert_sel = (data->me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

	struct WPaintAverageAccum *accum = (struct WPaintAverageAccum *)data->custom_data + n;
	accum->len = 0;
	accum->value = 0.0;

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	/* For each vertex */
	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		/* Test to see if the vertex coordinates are within the spherical brush region. */
		if (sculpt_brush_test_sq(&test, vd.co)) {
			const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
			if (view_dot > 0.0 && BKE_brush_curve_strength(data->brush, sqrtf(test.dist), cache->radius) > 0.0) {
				const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
				// const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;
				const char v_flag = data->me->mvert[v_index].flag;

				/* If the vertex is selected. */
				if (!(use_face_sel || use_vert_sel) || v_flag & SELECT) {
					const MDeformVert *dv = &data->me->dvert[v_index];
					accum->len += 1;
					accum->value += defvert_find_weight(dv, data->wpi->active.index);
				}
			}
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void calculate_average_weight(SculptThreadedTaskData *data, PBVHNode **UNUSED(nodes), int totnode)
{
	Scene *scene = CTX_data_scene(data->C);
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	struct WPaintAverageAccum *accum = MEM_mallocN(sizeof(*accum) * totnode, __func__);
	data->custom_data = accum;

	BLI_task_parallel_range_ex(
	        0, totnode, data, NULL, 0, do_wpaint_brush_calc_average_weight_cb_ex,
	        ((data->sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_THREADED_LIMIT), false);

	uint accum_len = 0;
	double accum_weight = 0.0;
	for (int i = 0; i < totnode; i++) {
		accum_len += accum[i].len;
		accum_weight += accum[i].value;
	}
	if (accum_len != 0) {
		accum_weight /= accum_len;
		if (ups->flag & UNIFIED_PAINT_WEIGHT)
			ups->weight = (float)accum_weight;
		else
			data->brush->weight = (float)accum_weight;
	}

	MEM_SAFE_FREE(data->custom_data);  /* 'accum' */
}


static void wpaint_paint_leaves(
        bContext *C, Object *ob, Sculpt *sd, VPaint *vp, struct WPaintData *wpd, WeightPaintInfo *wpi,
        Mesh *me, PBVHNode **nodes, int totnode)
{
	Brush *brush = ob->sculpt->cache->brush;

	/* threaded loop over nodes */
	SculptThreadedTaskData data = {
		.sd = sd, .ob = ob, .brush = brush, .nodes = nodes, .vp = vp, .wpd = wpd, .wpi = wpi, .me = me, .C = C,
	};

	switch (brush->vertexpaint_tool) {
		case PAINT_BLEND_AVERAGE:
			calculate_average_weight(&data, nodes, totnode);
			BLI_task_parallel_range_ex(
			        0, totnode, &data, NULL, 0,
			        do_wpaint_brush_draw_task_cb_ex, true, false);
			break;
		case PAINT_BLEND_SMEAR:
			BLI_task_parallel_range_ex(
			        0, totnode, &data, NULL, 0,
			        do_wpaint_brush_smear_task_cb_ex, true, false);
			break;
		case PAINT_BLEND_BLUR:
			BLI_task_parallel_range_ex(
			        0, totnode, &data, NULL, 0,
			        do_wpaint_brush_blur_task_cb_ex, true, false);
			break;
		default:
			BLI_task_parallel_range_ex(
			        0, totnode, &data, NULL, 0,
			        do_wpaint_brush_draw_task_cb_ex, true, false);
			break;
	}
}

static void wpaint_do_paint(
        bContext *C, Object *ob, VPaint *wp, Sculpt *sd, struct WPaintData *wpd, WeightPaintInfo *wpi,
        Mesh *me, Brush *UNUSED(brush), const char symm, const int axis, const int i, const float angle)
{
	SculptSession *ss = ob->sculpt;
	ss->cache->radial_symmetry_pass = i;
	calc_brushdata_symm(wp, ss->cache, symm, axis, angle);

	SculptSearchSphereData data;
	PBVHNode **nodes = NULL;
	int totnode;


	/* Build a list of all nodes that are potentially within the brush's area of influence */
	data.ss = ss;
	data.sd = sd;
	data.radius_squared = ss->cache->radius_squared;
	data.original = true;
	BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);

	calc_area_normal(wp, ob, nodes, totnode, ss->cache->sculpt_normal_symm);
	wpaint_paint_leaves(C, ob, sd, wp, wpd, wpi, me, nodes, totnode);

	if (nodes)
		MEM_freeN(nodes);
}

static void wpaint_do_radial_symmetry(
        bContext *C, Object *ob, VPaint *wp, Sculpt *sd, struct WPaintData *wpd, WeightPaintInfo *wpi,
        Mesh *me, Brush *brush, const char symm, const int axis)
{
	for (int i = 1; i < wp->radial_symm[axis - 'X']; i++) {
		const float angle = (2.0 * M_PI) * i / wp->radial_symm[axis - 'X'];
		wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, symm, axis, i, angle);
	}
}

static void wpaint_do_symmetrical_brush_actions(
        bContext *C, Object *ob, VPaint *wp, Sculpt *sd, struct WPaintData *wpd, WeightPaintInfo *wpi)
{
	Brush *brush = BKE_paint_brush(&wp->paint);
	Mesh *me = ob->data;
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	const char symm = wp->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
	int i = 0;

	/* initial stroke */
	wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, 0, 'X', 0, 0);
	wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, 0, 'X');
	wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, 0, 'Y');
	wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, 0, 'Z');

	cache->symmetry = symm;

	/* symm is a bit combination of XYZ - 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
	for (i = 1; i <= symm; i++) {
		if ((symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {
			cache->mirror_symmetry_pass = i;
			cache->radial_symmetry_pass = 0;
			calc_brushdata_symm(wp, cache, i, 0, 0);

			if (i & (1 << 0)) {
				wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, i, 'X', 0, 0);
				wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, i, 'X');
			}
			if (i & (1 << 1)) {
				wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, i, 'Y', 0, 0);
				wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, i, 'Y');
			}
			if (i & (1 << 2)) {
				wpaint_do_paint(C, ob, wp, sd, wpd, wpi, me, brush, i, 'Z', 0, 0);
				wpaint_do_radial_symmetry(C, ob, wp, sd, wpd, wpi, me, brush, i, 'Z');
			}
		}
	}
	copy_v3_v3(cache->true_last_location, cache->true_location);
	cache->is_last_valid = true;
}

static void wpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	VPaint *wp = ts->wpaint;
	Brush *brush = BKE_paint_brush(&wp->paint);
	struct WPaintData *wpd = paint_stroke_mode_data(stroke);
	ViewContext *vc;
	Object *ob = CTX_data_active_object(C);

	SculptSession *ss = ob->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	vwpaint_update_cache_variants(C, wp, ob, itemptr);

	float mat[4][4];
	float mval[2];

	const float brush_alpha_value = BKE_brush_alpha_get(scene, brush);

	/* intentionally don't initialize as NULL, make sure we initialize all members below */
	WeightPaintInfo wpi;

	/* cannot paint if there is no stroke data */
	if (wpd == NULL) {
		/* XXX: force a redraw here, since even though we can't paint,
		 * at least view won't freeze until stroke ends */
		ED_region_tag_redraw(CTX_wm_region(C));
		return;
	}

	vc = &wpd->vc;
	ob = vc->obact;
	
	view3d_operator_needs_opengl(C);
	ED_view3d_init_mats_rv3d(ob, vc->rv3d);

	/* load projection matrix */
	mul_m4_m4m4(mat, vc->rv3d->persmat, ob->obmat);


	/* *** setup WeightPaintInfo - pass onto do_weight_paint_vertex *** */
	wpi.defbase_tot =        wpd->defbase_tot;
	wpi.defbase_sel =        wpd->defbase_sel;
	wpi.defbase_tot_sel =    wpd->defbase_tot_sel;

	wpi.defbase_tot_unsel =  wpi.defbase_tot - wpi.defbase_tot_sel;
	wpi.active =             wpd->active;
	wpi.mirror =             wpd->mirror;
	wpi.lock_flags =         wpd->lock_flags;
	wpi.vgroup_validmap =    wpd->vgroup_validmap;
	wpi.do_flip =            RNA_boolean_get(itemptr, "pen_flip");
	wpi.do_multipaint =      wpd->do_multipaint;
	wpi.do_auto_normalize =  ((ts->auto_normalize != 0) && (wpi.vgroup_validmap != NULL));
	wpi.brush_alpha_value =  brush_alpha_value;
	/* *** done setting up WeightPaintInfo *** */

	wpaint_do_symmetrical_brush_actions(C, ob, wp, sd, wpd, &wpi);

	swap_m4m4(vc->rv3d->persmat, mat);

	/* calculate pivot for rotation around seletion if needed */
	/* also needed for "View Selected" on last stroke */
	paint_last_stroke_update(scene, vc->ar, mval);

	BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

	DEG_id_tag_update(ob->data, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	swap_m4m4(wpd->vc.rv3d->persmat, mat);

	rcti r;
	if (sculpt_get_redraw_rect(vc->ar, CTX_wm_region_view3d(C), ob, &r)) {
		if (ss->cache) {
			ss->cache->current_r = r;
		}

		/* previous is not set in the current cache else
		 * the partial rect will always grow */
		if (ss->cache) {
			if (!BLI_rcti_is_empty(&ss->cache->previous_r))
				BLI_rcti_union(&r, &ss->cache->previous_r);
		}

		r.xmin += vc->ar->winrct.xmin - 2;
		r.xmax += vc->ar->winrct.xmin + 2;
		r.ymin += vc->ar->winrct.ymin - 2;
		r.ymax += vc->ar->winrct.ymin + 2;

		ss->partial_redraw = 1;
	}
	ED_region_tag_redraw_partial(vc->ar, &r);
}

static void wpaint_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
	Object *ob = CTX_data_active_object(C);
	struct WPaintData *wpd = paint_stroke_mode_data(stroke);
	
	if (wpd) {
		ED_vpaint_proj_handle_free(wpd->vp_handle);

		if (wpd->defbase_sel)
			MEM_freeN((void *)wpd->defbase_sel);
		if (wpd->vgroup_validmap)
			MEM_freeN((void *)wpd->vgroup_validmap);
		if (wpd->lock_flags)
			MEM_freeN((void *)wpd->lock_flags);
		if (wpd->active.lock)
			MEM_freeN((void *)wpd->active.lock);
		if (wpd->mirror.lock)
			MEM_freeN((void *)wpd->mirror.lock);

		MEM_freeN(wpd);
	}
	
	/* and particles too */
	if (ob->particlesystem.first) {
		ParticleSystem *psys;
		int i;
		
		for (psys = ob->particlesystem.first; psys; psys = psys->next) {
			for (i = 0; i < PSYS_TOT_VG; i++) {
				if (psys->vgroup[i] == ob->actdef) {
					psys->recalc |= PSYS_RECALC_RESET;
					break;
				}
			}
		}
	}

	DEG_id_tag_update(ob->data, 0);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	sculpt_cache_free(ob->sculpt->cache);
	ob->sculpt->cache = NULL;
}


static int wpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

	op->customdata = paint_stroke_new(
	        C, op, sculpt_stroke_get_location, wpaint_stroke_test_start,
	        wpaint_stroke_update_step, NULL,
	        wpaint_stroke_done, event->type);
	
	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		paint_stroke_data_free(op);
		return OPERATOR_FINISHED;
	}
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);
	
	return OPERATOR_RUNNING_MODAL;
}

static int wpaint_exec(bContext *C, wmOperator *op)
{
	op->customdata = paint_stroke_new(
	        C, op, sculpt_stroke_get_location, wpaint_stroke_test_start,
	        wpaint_stroke_update_step, NULL,
	        wpaint_stroke_done, 0);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

static void wpaint_cancel(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	if (ob->sculpt->cache) {
		sculpt_cache_free(ob->sculpt->cache);
		ob->sculpt->cache = NULL;
	}

	paint_stroke_cancel(C, op);
}

void PAINT_OT_weight_paint(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Weight Paint";
	ot->idname = "PAINT_OT_weight_paint";
	ot->description = "Paint a stroke in the current vertex group's weights";
	
	/* api callbacks */
	ot->invoke = wpaint_invoke;
	ot->modal = paint_stroke_modal;
	ot->exec = wpaint_exec;
	ot->poll = weight_paint_poll;
	ot->cancel = wpaint_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;
	
	paint_stroke_operator_properties(ot);
}

static int weight_paint_set_exec(bContext *C, wmOperator *op)
{
	struct Scene *scene = CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
	float vgroup_weight = BKE_brush_weight_get(scene, brush);

	if (wpaint_ensure_data(C, op, WPAINT_ENSURE_MIRROR, NULL) == false) {
		return OPERATOR_CANCELLED;
	}

	if (ED_wpaint_fill(obact, vgroup_weight)) {
		ED_region_tag_redraw(CTX_wm_region(C)); /* XXX - should redraw all 3D views */
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void PAINT_OT_weight_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Weight";
	ot->idname = "PAINT_OT_weight_set";
	ot->description = "Fill the active vertex group with the current paint weight";

	/* api callbacks */
	ot->exec = weight_paint_set_exec;
	ot->poll = mask_paint_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************ set / clear vertex paint mode ********** */

/**
 * \note Keep in sync with #wpaint_mode_toggle_exec
 */
static int vpaint_mode_toggle_exec(bContext *C, wmOperator *op)
{	
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_VERTEX_PAINT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;
	Scene *scene = CTX_data_scene(C);
	VPaint *vp = scene->toolsettings->vpaint;
	Mesh *me;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	me = BKE_mesh_from_object(ob);
	
	/* toggle: end vpaint */
	if (is_mode_set) {
		ob->mode &= ~mode_flag;

		if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
			BKE_mesh_flush_select_from_polys(me);
		}

		/* If the cache is not released by a cancel or a done, free it now. */
		if (ob->sculpt->cache) {
			sculpt_cache_free(ob->sculpt->cache);
			ob->sculpt->cache = NULL;
		}

		BKE_sculptsession_free(ob);

		paint_cursor_delete_textures();
	}
	else {
		EvaluationContext eval_ctx;

		CTX_data_eval_ctx(C, &eval_ctx);

		ob->mode |= mode_flag;

		if (me->mloopcol == NULL) {
			make_vertexcol(ob);
		}

		if (vp == NULL)
			vp = scene->toolsettings->vpaint = new_vpaint(0);
		
		paint_cursor_start(C, vertex_paint_poll);

		BKE_paint_init(scene, ePaintVertex, PAINT_CURSOR_VERTEX_PAINT);

		/* Create vertex/weight paint mode session data */
		if (ob->sculpt) {
			if (ob->sculpt->cache) {
				sculpt_cache_free(ob->sculpt->cache);
				ob->sculpt->cache = NULL;
			}
			BKE_sculptsession_free(ob);
		}
		vertex_paint_init_session(&eval_ctx, scene, ob);
	}

	BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

	/* update modifier stack for mapping requirements */
	DEG_id_tag_update(&me->id, 0);
	
	WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Vertex Paint Mode";
	ot->idname = "PAINT_OT_vertex_paint_toggle";
	ot->description = "Toggle the vertex paint mode in 3D view";
	
	/* api callbacks */
	ot->exec = vpaint_mode_toggle_exec;
	ot->poll = paint_poll_test;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}



/* ********************** vertex paint operator ******************* */

/* Implementation notes:
 *
 * Operator->invoke()
 * - validate context (add mcol)
 * - create customdata storage
 * - call paint once (mouse click)
 * - add modal handler 
 *
 * Operator->modal()
 * - for every mousemove, apply vertex paint
 * - exit on mouse release, free customdata
 *   (return OPERATOR_FINISHED also removes handler and operator)
 *
 * For future:
 * - implement a stroke event (or mousemove with past positons)
 * - revise whether op->customdata should be added in object, in set_vpaint
 */

typedef struct PolyFaceMap {
	struct PolyFaceMap *next, *prev;
	int facenr;
} PolyFaceMap;

struct VPaintData {
	ViewContext vc;
	uint paintcol;

	struct VertProjHandle *vp_handle;
	struct DMCoNo *vertexcosnos;

	float vpimat[3][3];

	/* modify 'me->mcol' directly, since the derived mesh is drawing from this
	 * array, otherwise we need to refresh the modifier stack */
	bool use_fast_update;

	/* loops tagged as having been painted, to apply shared vertex color
	 * blending only to modified loops */
	bool *mlooptag;

	bool is_texbrush;
};

static bool vpaint_stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	struct PaintStroke *stroke = op->customdata;
	VPaint *vp = ts->vpaint;
	Brush *brush = BKE_paint_brush(&vp->paint);
	struct VPaintData *vpd;
	Object *ob = CTX_data_active_object(C);
	Mesh *me;
	float mat[4][4], imat[4][4];
	SculptSession *ss = ob->sculpt;
	EvaluationContext eval_ctx;

	CTX_data_eval_ctx(C, &eval_ctx);

	/* context checks could be a poll() */
	me = BKE_mesh_from_object(ob);
	if (me == NULL || me->totpoly == 0)
		return false;
	
	if (me->mloopcol == NULL)
		make_vertexcol(ob);
	if (me->mloopcol == NULL)
		return false;

	/* make mode data storage */
	vpd = MEM_callocN(sizeof(*vpd), "VPaintData");
	paint_stroke_set_mode_data(stroke, vpd);
	view3d_set_viewcontext(C, &vpd->vc);
	
	vpd->paintcol = vpaint_get_current_col(scene, vp);

	vpd->is_texbrush = !(brush->vertexpaint_tool == PAINT_BLEND_BLUR) &&
	                   brush->mtex.tex;

	/* are we painting onto a modified mesh?,
	 * if not we can skip face map trickiness */
	if (vertex_paint_use_fast_update_check(ob)) {
		vpd->use_fast_update = true;
/*		printf("Fast update!\n");*/
	}
	else {
		vpd->use_fast_update = false;
/*		printf("No fast update!\n");*/
	}

	/* to keep tracked of modified loops for shared vertex color blending */
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		vpd->mlooptag = MEM_mallocN(sizeof(bool) * me->totloop, "VPaintData mlooptag");
	}

	/* Create projection handle */
	if (vpd->is_texbrush) {
		ob->sculpt->building_vp_handle = true;
		vpd->vp_handle = ED_vpaint_proj_handle_create(&eval_ctx, scene, ob, &vpd->vertexcosnos);
		ob->sculpt->building_vp_handle = false;
	}

	/* some old cruft to sort out later */
	mul_m4_m4m4(mat, vpd->vc.rv3d->viewmat, ob->obmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(vpd->vpimat, imat);

	/* If not previously created, create vertex/weight paint mode session data */
	vertex_paint_init_session(&eval_ctx, scene, ob);
	vwpaint_update_cache_invariants(C, vp, ss, op, mouse);
	vertex_paint_init_session_data(ts, ob);

	if (ob->sculpt->mode.vpaint.previous_color != NULL) {
		memset(ob->sculpt->mode.vpaint.previous_color, 0, sizeof(uint) * me->totloop);
	}

	return 1;
}

static void do_vpaint_brush_calc_average_color_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);
	const struct SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;

	StrokeCache *cache = ss->cache;
	uint *lcol = data->lcol;
	char *col;

	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	struct VPaintAverageAccum *accum = (struct VPaintAverageAccum *)data->custom_data + n;
	accum->len = 0;
	memset(accum->value, 0, sizeof(accum->value));

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	/* For each vertex */
	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		/* Test to see if the vertex coordinates are within the spherical brush region. */
		if (sculpt_brush_test_fast(&test, vd.co)) {
			const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
			if (BKE_brush_curve_strength(data->brush, test.dist, cache->radius) > 0.0) {
				/* If the vertex is selected for painting. */
				const MVert *mv = &data->me->mvert[v_index];
				if (!use_face_sel || mv->flag & SELECT) {
					accum->len += gmap->vert_to_loop[v_index].count;
					/* if a vertex is within the brush region, then add it's color to the blend. */
					for (int j = 0; j < gmap->vert_to_loop[v_index].count; j++) {
						const int l_index = gmap->vert_to_loop[v_index].indices[j];
						col = (char *)(&lcol[l_index]);
						/* Color is squared to compensate the sqrt color encoding. */
						accum->value[0] += col[0] * col[0];
						accum->value[1] += col[1] * col[1];
						accum->value[2] += col[2] * col[2];
					}
				}
			}
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void handle_texture_brush(
        SculptThreadedTaskData *data, PBVHVertexIter vd, float size_pressure, float alpha_pressure,
        float *r_alpha, uint *r_color)
{
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);
	const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];

	float rgba[4];
	float rgba_br[3];

	*r_alpha = calc_vp_alpha_col_dl(
	        data->vp, &data->vpd->vc, data->vpd->vpimat,
	        &data->vpd->vertexcosnos[v_index], ss->cache->mouse, size_pressure, alpha_pressure, rgba);
	rgb_uchar_to_float(rgba_br, (const uchar *)&data->vpd->paintcol);
	mul_v3_v3(rgba_br, rgba);
	rgb_float_to_uchar((uchar *)r_color, rgba_br);
}

static void do_vpaint_brush_draw_task_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);
	const struct SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;

	Brush *brush = data->brush;
	StrokeCache *cache = ss->cache;
	const float brush_strength = cache->bstrength;
	uint *lcol = data->lcol;
	Scene *scene = CTX_data_scene(data->C);
	float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
	get_brush_alpha_data(scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	/* For each vertex*/
	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		/* Test to see if the vertex coordinates are within the spherical brush region. */
		if (sculpt_brush_test(&test, vd.co)) {
			/* Note: Grids are 1:1 with corners (aka loops).
			 * For grid based pbvh, take the vert whose loop cooresponds to the current grid.
			 * Otherwise, take the current vert. */
			const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
			const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;
			const MVert *mv = &data->me->mvert[v_index];

			/* If the vertex is selected for painting. */
			if (!use_face_sel || mv->flag & SELECT) {
				/* Calc the dot prod. between ray norm on surf and current vert
				 * (ie splash prevention factor), and only paint front facing verts. */
				const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
				if (view_dot > 0.0f) {
					const float brush_fade = BKE_brush_curve_strength(brush, test.dist, cache->radius);
					uint color_final = data->vpd->paintcol;

					/* If we're painting with a texture, sample the texture color and alpha. */
					float tex_alpha = 1.0;
					if (data->vpd->is_texbrush) {
						handle_texture_brush(
						        data, vd, brush_size_pressure, brush_alpha_pressure,
						        &tex_alpha, &color_final);
					}
					/* For each poly owning this vert, paint each loop belonging to this vert. */
					for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
						const int p_index = gmap->vert_to_poly[v_index].indices[j];
						const int l_index = gmap->vert_to_loop[v_index].indices[j];
						BLI_assert(data->me->mloop[l_index].v == v_index);
						const MPoly *mp = &data->me->mpoly[p_index];
						if (!use_face_sel || mp->flag & ME_FACE_SEL) {
							uint color_orig = 0;  /* unused when array is NULL */
							if (ss->mode.vpaint.previous_color != NULL) {
								/* Get the previous loop color */
								if (ss->mode.vpaint.previous_color[l_index] == 0) {
									ss->mode.vpaint.previous_color[l_index] = lcol[l_index];
								}
								color_orig = ss->mode.vpaint.previous_color[l_index];
							}
							const float final_alpha =
							        255 * brush_fade * brush_strength * view_dot *
							        tex_alpha * brush_alpha_pressure * grid_alpha;
							/* Mix the new color with the original based on final_alpha. */
							lcol[l_index] = vpaint_blend(
							        data->vp, lcol[l_index], color_orig, color_final,
							        final_alpha, 255 * brush_strength);
						}
					}
				}
			}
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void do_vpaint_brush_blur_task_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);

	Scene *scene = CTX_data_scene(data->C);
	const struct SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
	Brush *brush = data->brush;
	StrokeCache *cache = ss->cache;
	const float brush_strength = cache->bstrength;
	uint *lcol = data->lcol;
	float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
	get_brush_alpha_data(scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	SculptBrushTest test;
	sculpt_brush_test_init(ss, &test);

	/* For each vertex */
	PBVHVertexIter vd;
	BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
	{
		/* Test to see if the vertex coordinates are within the spherical brush region. */
		if (sculpt_brush_test(&test, vd.co)) {
			/* For grid based pbvh, take the vert whose loop cooresponds to the current grid. 
			Otherwise, take the current vert. */
			const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
			const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;
			const MVert *mv = &data->me->mvert[v_index];

			const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
			if (view_dot > 0.0f) {
				const float brush_fade = BKE_brush_curve_strength(brush, test.dist, cache->radius);

				/* If the vertex is selected for painting. */
				if (!use_face_sel || mv->flag & SELECT) {
					/* Get the average poly color */
					uint color_final = 0;
					int total_hit_loops = 0;
					uint blend[4] = {0};
					for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
						int p_index = gmap->vert_to_poly[v_index].indices[j];
						const MPoly *mp = &data->me->mpoly[p_index];
						if (!use_face_sel || mp->flag & ME_FACE_SEL) {
							total_hit_loops += mp->totloop;
							for (int k = 0; k < mp->totloop; k++) {
								const uint l_index = mp->loopstart + k;
								const char *col = (const char *)(&lcol[l_index]);
								/* Color is squared to compensate the sqrt color encoding. */
								blend[0] += (uint)col[0] * (uint)col[0];
								blend[1] += (uint)col[1] * (uint)col[1];
								blend[2] += (uint)col[2] * (uint)col[2];
								blend[3] += (uint)col[3] * (uint)col[3];
							}
						}
					}
					if (total_hit_loops != 0) {
						/* Use rgb^2 color averaging. */
						char *col = (char *)(&color_final);
						col[0] = round_fl_to_uchar(sqrtf(divide_round_i(blend[0], total_hit_loops)));
						col[1] = round_fl_to_uchar(sqrtf(divide_round_i(blend[1], total_hit_loops)));
						col[2] = round_fl_to_uchar(sqrtf(divide_round_i(blend[2], total_hit_loops)));
						col[3] = round_fl_to_uchar(sqrtf(divide_round_i(blend[3], total_hit_loops)));

						/* For each poly owning this vert, paint each loop belonging to this vert. */
						for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
							const int p_index = gmap->vert_to_poly[v_index].indices[j];
							const int l_index = gmap->vert_to_loop[v_index].indices[j];
							BLI_assert(data->me->mloop[l_index].v == v_index);
							const MPoly *mp = &data->me->mpoly[p_index];
							if (!use_face_sel || mp->flag & ME_FACE_SEL) {
								uint color_orig = 0;  /* unused when array is NULL */
								if (ss->mode.vpaint.previous_color != NULL) {
									/* Get the previous loop color */
									if (ss->mode.vpaint.previous_color[l_index] == 0) {
										ss->mode.vpaint.previous_color[l_index] = lcol[l_index];
									}
									color_orig = ss->mode.vpaint.previous_color[l_index];
								}
								const float final_alpha =
								        255 * brush_fade * brush_strength * view_dot *
								        brush_alpha_pressure * grid_alpha;
								/* Mix the new color with the original
								 * based on the brush strength and the curve. */
								lcol[l_index] = vpaint_blend(
								        data->vp, lcol[l_index], color_orig, *((uint *)col),
								        final_alpha, 255 * brush_strength);
							}
						}
					}
				}
			}
		}
	}
	BKE_pbvh_vertex_iter_end;
}

static void do_vpaint_brush_smear_task_cb_ex(
        void *userdata, void *UNUSED(userdata_chunk), const int n, const int UNUSED(thread_id))
{
	SculptThreadedTaskData *data = userdata;
	SculptSession *ss = data->ob->sculpt;
	CCGDerivedMesh *ccgdm = BKE_pbvh_get_ccgdm(ss->pbvh);

	Scene *scene = CTX_data_scene(data->C);
	const struct SculptVertexPaintGeomMap *gmap = &ss->mode.vpaint.gmap;
	Brush *brush = data->brush;
	StrokeCache *cache = ss->cache;
	const float brush_strength = cache->bstrength;
	uint *lcol = data->lcol;
	float brush_size_pressure, brush_alpha_value, brush_alpha_pressure;
	get_brush_alpha_data(scene, ss, brush, &brush_size_pressure, &brush_alpha_value, &brush_alpha_pressure);
	float brush_dir[3];
	const bool use_face_sel = (data->me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	sub_v3_v3v3(brush_dir, cache->location, cache->last_location);
	if (normalize_v3(brush_dir) != 0.0f) {

		SculptBrushTest test;
		sculpt_brush_test_init(ss, &test);

		/* For each vertex */
		PBVHVertexIter vd;
		BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
		{
			/* Test to see if the vertex coordinates are within the spherical brush region. */
			if (sculpt_brush_test(&test, vd.co)) {
				/* For grid based pbvh, take the vert whose loop cooresponds to the current grid.
				Otherwise, take the current vert. */
				const int v_index = ccgdm ? data->me->mloop[vd.grid_indices[vd.g]].v : vd.vert_indices[vd.i];
				const float grid_alpha = ccgdm ? 1.0f / vd.gridsize : 1.0f;
				const MVert *mv_curr = &data->me->mvert[v_index];

				/* if the vertex is selected for painting. */
				if (!use_face_sel || mv_curr->flag & SELECT) {
					/* Calc the dot prod. between ray norm on surf and current vert
					(ie splash prevention factor), and only paint front facing verts. */
					const float view_dot = (vd.no) ? dot_vf3vs3(cache->sculpt_normal_symm, vd.no) : 1.0;
					if (view_dot > 0.0f) {
						const float brush_fade = BKE_brush_curve_strength(brush, test.dist, cache->radius);

						bool do_color = false;
						/* Minimum dot product between brush direction and current
						 * to neighbor direction is 0.0, meaning orthogonal. */
						float stroke_dot_max = 0.0f;

						/* Get the color of the loop in the opposite direction of the brush movement */
						uint color_final = 0;
						for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
							const int p_index = gmap->vert_to_poly[v_index].indices[j];
							const int l_index = gmap->vert_to_loop[v_index].indices[j];
							BLI_assert(data->me->mloop[l_index].v == v_index);
							const MPoly *mp = &data->me->mpoly[p_index];
							if (!use_face_sel || mp->flag & ME_FACE_SEL) {
								for (int k = 0; k < mp->totloop; k++) {
									const MLoop *ml = &data->me->mloop[l_index];
									const uint v_other_index = ml->v;
									const MVert *mv_other = &data->me->mvert[v_other_index];

									/* Get the direction from the selected vert to the neighbor. */
									float other_dir[3];
									sub_v3_v3v3(other_dir, mv_curr->co, mv_other->co);
									normalize_v3(other_dir);

									const float stroke_dot = dot_v3v3(other_dir, brush_dir);

									if (stroke_dot > stroke_dot_max) {
										stroke_dot_max = stroke_dot;
										color_final = lcol[l_index];
										do_color = true;
									}
								}
							}
						}

						if (do_color) {
							/* For each poly owning this vert, paint each loop belonging to this vert. */
							for (int j = 0; j < gmap->vert_to_poly[v_index].count; j++) {
								const int p_index = gmap->vert_to_poly[v_index].indices[j];
								const int l_index = gmap->vert_to_loop[v_index].indices[j];
								BLI_assert(data->me->mloop[l_index].v == v_index);
								const MPoly *mp = &data->me->mpoly[p_index];
								if (!use_face_sel || mp->flag & ME_FACE_SEL) {
									/* Get the previous loop color */
									uint color_orig = 0;  /* unused when array is NULL */
									if (ss->mode.vpaint.previous_color != NULL) {
										/* Get the previous loop color */
										if (ss->mode.vpaint.previous_color[l_index] == 0) {
											ss->mode.vpaint.previous_color[l_index] = lcol[l_index];
										}
										color_orig = ss->mode.vpaint.previous_color[l_index];
									}
									const float final_alpha =
									        255 * brush_fade * brush_strength *
									        view_dot * brush_alpha_pressure * grid_alpha;
									/* Mix the new color with the original
									 * based on the brush strength and the curve. */
									lcol[l_index] = vpaint_blend(
									        data->vp, lcol[l_index], color_orig, color_final,
									        final_alpha, 255 * brush_strength);
								}
							}
						}
					}
				}
			}
		}
		BKE_pbvh_vertex_iter_end;
	}
}

static void calculate_average_color(SculptThreadedTaskData *data, PBVHNode **UNUSED(nodes), int totnode)
{
	struct VPaintAverageAccum *accum = MEM_mallocN(sizeof(*accum) * totnode, __func__);
	data->custom_data = accum;

	BLI_task_parallel_range_ex(
	        0, totnode, data, NULL, 0, do_vpaint_brush_calc_average_color_cb_ex,
	        true, false);

	uint accum_len = 0;
	uint accum_value[3] = {0};
	uchar blend[4] = {0};
	for (int i = 0; i < totnode; i++) {
		accum_len += accum[i].len;
		accum_value[0] += accum[i].value[0];
		accum_value[1] += accum[i].value[1];
		accum_value[2] += accum[i].value[2];
	}
	if (accum_len != 0) {
		blend[0] = round_fl_to_uchar(sqrtf(divide_round_i(accum_value[0], accum_len)));
		blend[1] = round_fl_to_uchar(sqrtf(divide_round_i(accum_value[1], accum_len)));
		blend[2] = round_fl_to_uchar(sqrtf(divide_round_i(accum_value[2], accum_len)));
		blend[3] = 255;
		data->vpd->paintcol = *((uint *)blend);
	}

	MEM_SAFE_FREE(data->custom_data);  /* 'accum' */
}

static void vpaint_paint_leaves(
        bContext *C, Sculpt *sd, VPaint *vp, struct VPaintData *vpd,
        Object *ob, Mesh *me, PBVHNode **nodes, int totnode)
{
	Brush *brush = ob->sculpt->cache->brush;

	SculptThreadedTaskData data = {
		.sd = sd, .ob = ob, .brush = brush, .nodes = nodes, .vp = vp, .vpd = vpd,
		.lcol = (uint *)me->mloopcol, .me = me, .C = C,
	};
	switch (brush->vertexpaint_tool) {
		case PAINT_BLEND_AVERAGE:
			calculate_average_color(&data, nodes, totnode);
			BLI_task_parallel_range_ex(
			    0, totnode, &data, NULL, 0,
			    do_vpaint_brush_draw_task_cb_ex, true, false);
			break;
		case PAINT_BLEND_BLUR:
			BLI_task_parallel_range_ex(
			    0, totnode, &data, NULL, 0,
			    do_vpaint_brush_blur_task_cb_ex, true, false);
			break;
		case PAINT_BLEND_SMEAR:
			BLI_task_parallel_range_ex(
			    0, totnode, &data, NULL, 0,
			    do_vpaint_brush_smear_task_cb_ex, true, false);
			break;
		default:
			BLI_task_parallel_range_ex(
			    0, totnode, &data, NULL, 0,
			    do_vpaint_brush_draw_task_cb_ex, true, false);
			break;
	}
}

static void vpaint_do_paint(
        bContext *C, Sculpt *sd, VPaint *vd, struct VPaintData *vpd,
        Object *ob, Mesh *me, Brush *UNUSED(brush), const char symm, const int axis, const int i, const float angle)
{
	SculptSession *ss = ob->sculpt;
	ss->cache->radial_symmetry_pass = i;
	calc_brushdata_symm(vd, ss->cache, symm, axis, angle);
	SculptSearchSphereData data;
	PBVHNode **nodes = NULL;
	int totnode;

	/* Build a list of all nodes that are potentially within the brush's area of influence */
	data.ss = ss;
	data.sd = sd;
	data.radius_squared = ss->cache->radius_squared;
	data.original = true;
	BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);

	calc_area_normal(vd, ob, nodes, totnode, ss->cache->sculpt_normal_symm);

	/* Paint those leaves. */
	vpaint_paint_leaves(C, sd, vd, vpd, ob, me, nodes, totnode);

	if (nodes) {
		MEM_freeN(nodes);
	}
}

static void vpaint_do_radial_symmetry(
        bContext *C, Sculpt *sd, VPaint *vd, struct VPaintData *vpd, Object *ob, Mesh *me,
        Brush *brush, const char symm, const int axis)
{
	for (int i = 1; i < vd->radial_symm[axis - 'X']; i++) {
		const float angle = (2.0 * M_PI) * i / vd->radial_symm[axis - 'X'];
		vpaint_do_paint(C, sd, vd, vpd, ob, me, brush, symm, axis, i, angle);
	}
}

static void vpaint_do_symmetrical_brush_actions(
        bContext *C, Sculpt *sd, VPaint *vd, struct VPaintData *vpd, Object *ob)
{
	Brush *brush = BKE_paint_brush(&vd->paint);
	Mesh *me = ob->data;
	SculptSession *ss = ob->sculpt;
	StrokeCache *cache = ss->cache;
	const char symm = vd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
	int i = 0;

	/* initial stroke */
	vpaint_do_paint(C, sd, vd, vpd, ob, me, brush, i, 'X', 0, 0);
	vpaint_do_radial_symmetry(C, sd, vd, vpd, ob, me, brush, i, 'X');
	vpaint_do_radial_symmetry(C, sd, vd, vpd, ob, me, brush, i, 'Y');
	vpaint_do_radial_symmetry(C, sd, vd, vpd, ob, me, brush, i, 'Z');

	cache->symmetry = symm;

	/* symm is a bit combination of XYZ - 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
	for (i = 1; i <= symm; i++) {
		if (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5))) {
			cache->mirror_symmetry_pass = i;
			cache->radial_symmetry_pass = 0;
			calc_brushdata_symm(vd, cache, i, 0, 0);

			if (i & (1 << 0)) {
				vpaint_do_paint(C, sd, vd, vpd, ob, me, brush, i, 'X', 0, 0);
				vpaint_do_radial_symmetry(C, sd, vd, vpd, ob, me, brush, i, 'X');
			}
			if (i & (1 << 1)) {
				vpaint_do_paint(C, sd, vd, vpd, ob, me, brush, i, 'Y', 0, 0);
				vpaint_do_radial_symmetry(C, sd, vd, vpd, ob, me, brush, i, 'Y');
			}
			if (i & (1 << 2)) {
				vpaint_do_paint(C, sd, vd, vpd, ob, me, brush, i, 'Z', 0, 0);
				vpaint_do_radial_symmetry(C, sd, vd, vpd, ob, me, brush, i, 'Z');
			}
		}
	}

	copy_v3_v3(cache->true_last_location, cache->true_location);
	cache->is_last_valid = true;
}

static void vpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	VPaint *vp = ts->vpaint;
	ViewContext *vc = &vpd->vc;
	Object *ob = vc->obact;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	vwpaint_update_cache_variants(C, vp, ob, itemptr);

	float mat[4][4];
	float mval[2];

	ED_view3d_init_mats_rv3d(ob, vc->rv3d);

	/* load projection matrix */
	mul_m4_m4m4(mat, vc->rv3d->persmat, ob->obmat);

	swap_m4m4(vc->rv3d->persmat, mat);

	vpaint_do_symmetrical_brush_actions(C, sd, vp, vpd, ob);

	swap_m4m4(vc->rv3d->persmat, mat);

	BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

	/* calculate pivot for rotation around seletion if needed */
	/* also needed for "View Selected" on last stroke */
	paint_last_stroke_update(scene, vc->ar, mval);

	ED_region_tag_redraw(vc->ar);

	if (vpd->use_fast_update == false) {
		/* recalculate modifier stack to get new colors, slow,
		 * avoid this if we can! */
		DEG_id_tag_update(ob->data, 0);
	}
	else {
		/* If using new VBO drawing, mark mcol as dirty to force colors gpu buffer refresh! */
		ob->derivedFinal->dirty |= DM_DIRTY_MCOL_UPDATE_DRAW;
	}
}

static void vpaint_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	ViewContext *vc = &vpd->vc;
	Object *ob = vc->obact;

	if (vpd->mlooptag)
		MEM_freeN(vpd->mlooptag);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	MEM_freeN(vpd);

	sculpt_cache_free(ob->sculpt->cache);
	ob->sculpt->cache = NULL;
}

static int vpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

	op->customdata = paint_stroke_new(C, op, sculpt_stroke_get_location, vpaint_stroke_test_start,
	                                  vpaint_stroke_update_step, NULL,
	                                  vpaint_stroke_done, event->type);
	
	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		paint_stroke_data_free(op);
		return OPERATOR_FINISHED;
	}

	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);
	
	return OPERATOR_RUNNING_MODAL;
}

static int vpaint_exec(bContext *C, wmOperator *op)
{
	op->customdata = paint_stroke_new(C, op, sculpt_stroke_get_location, vpaint_stroke_test_start,
	                                  vpaint_stroke_update_step, NULL,
	                                  vpaint_stroke_done, 0);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

static void vpaint_cancel(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	if (ob->sculpt->cache) {
		sculpt_cache_free(ob->sculpt->cache);
		ob->sculpt->cache = NULL;
	}

	paint_stroke_cancel(C, op);
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Paint";
	ot->idname = "PAINT_OT_vertex_paint";
	ot->description = "Paint a stroke in the active vertex color layer";
	
	/* api callbacks */
	ot->invoke = vpaint_invoke;
	ot->modal = paint_stroke_modal;
	ot->exec = vpaint_exec;
	ot->poll = vertex_paint_poll;
	ot->cancel = vpaint_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

	paint_stroke_operator_properties(ot);
}

/* ********************** weight from bones operator ******************* */

static int weight_from_bones_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return (ob && (ob->mode & OB_MODE_WEIGHT_PAINT) && modifiers_isDeformedByArmature(ob));
}

static int weight_from_bones_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Object *armob = modifiers_isDeformedByArmature(ob);
	Mesh *me = ob->data;
	int type = RNA_enum_get(op->ptr, "type");
	EvaluationContext eval_ctx;

	CTX_data_eval_ctx(C, &eval_ctx);
	create_vgroups_from_armature(op->reports, &eval_ctx, scene, ob, armob, type, (me->editflag & ME_EDIT_MIRROR_X));

	DEG_id_tag_update(&me->id, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_from_bones(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{ARM_GROUPS_AUTO, "AUTOMATIC", 0, "Automatic", "Automatic weights from bones"},
		{ARM_GROUPS_ENVELOPE, "ENVELOPES", 0, "From Envelopes", "Weights from envelopes with user defined radius"},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Weight from Bones";
	ot->idname = "PAINT_OT_weight_from_bones";
	ot->description = "Set the weights of the groups matching the attached armature's selected bones, "
	                  "using the distance between the vertices and the bones";
	
	/* api callbacks */
	ot->exec = weight_from_bones_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = weight_from_bones_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "Method to use for assigning weights");
}

/* *** VGroups Gradient *** */
typedef struct DMGradient_vertStore {
	float sco[2];
	float weight_orig;
	enum {
		VGRAD_STORE_NOP      = 0,
		VGRAD_STORE_DW_EXIST = (1 << 0)
	} flag;
} DMGradient_vertStore;

typedef struct DMGradient_vertStoreBase {
	struct WPaintPrev wpp;
	DMGradient_vertStore elem[0];
} DMGradient_vertStoreBase;

typedef struct DMGradient_userData {
	struct ARegion *ar;
	Scene *scene;
	Mesh *me;
	Brush *brush;
	const float *sco_start;     /* [2] */
	const float *sco_end;       /* [2] */
	float        sco_line_div;  /* store (1.0f / len_v2v2(sco_start, sco_end)) */
	int def_nr;
	bool is_init;
	DMGradient_vertStoreBase *vert_cache;
	/* only for init */
	BLI_bitmap *vert_visit;

	/* options */
	short use_select;
	short type;
	float weightpaint;
} DMGradient_userData;

static void gradientVert_update(DMGradient_userData *grad_data, int index)
{
	Mesh *me = grad_data->me;
	DMGradient_vertStore *vs = &grad_data->vert_cache->elem[index];
	float alpha;

	if (grad_data->type == WPAINT_GRADIENT_TYPE_LINEAR) {
		alpha = line_point_factor_v2(vs->sco, grad_data->sco_start, grad_data->sco_end);
	}
	else {
		BLI_assert(grad_data->type == WPAINT_GRADIENT_TYPE_RADIAL);
		alpha = len_v2v2(grad_data->sco_start, vs->sco) * grad_data->sco_line_div;
	}
	/* no need to clamp 'alpha' yet */

	/* adjust weight */
	alpha = BKE_brush_curve_strength_clamped(grad_data->brush, alpha, 1.0f);

	if (alpha != 0.0f) {
		MDeformVert *dv = &me->dvert[index];
		MDeformWeight *dw = defvert_verify_index(dv, grad_data->def_nr);
		// dw->weight = alpha; // testing
		int tool = grad_data->brush->vertexpaint_tool;
		float testw;

		/* init if we just added */
		testw = wpaint_blend_tool(tool, vs->weight_orig, grad_data->weightpaint, alpha * grad_data->brush->alpha);
		CLAMP(testw, 0.0f, 1.0f);
		dw->weight = testw;
	}
	else {
		MDeformVert *dv = &me->dvert[index];
		if (vs->flag & VGRAD_STORE_DW_EXIST) {
			/* normally we NULL check, but in this case we know it exists */
			MDeformWeight *dw = defvert_find_index(dv, grad_data->def_nr);
			dw->weight = vs->weight_orig;
		}
		else {
			/* wasn't originally existing, remove */
			MDeformWeight *dw = defvert_find_index(dv, grad_data->def_nr);
			if (dw) {
				defvert_remove_group(dv, dw);
			}
		}
	}
}

static void gradientVertUpdate__mapFunc(
        void *userData, int index, const float UNUSED(co[3]),
        const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	DMGradient_userData *grad_data = userData;
	Mesh *me = grad_data->me;
	if ((grad_data->use_select == false) || (me->mvert[index].flag & SELECT)) {
		DMGradient_vertStore *vs = &grad_data->vert_cache->elem[index];
		if (vs->sco[0] != FLT_MAX) {
			gradientVert_update(grad_data, index);
		}
	}
}

static void gradientVertInit__mapFunc(
        void *userData, int index, const float co[3],
        const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	DMGradient_userData *grad_data = userData;
	Mesh *me = grad_data->me;

	if ((grad_data->use_select == false) || (me->mvert[index].flag & SELECT)) {
		/* run first pass only,
		 * the screen coords of the verts need to be cached because
		 * updating the mesh may move them about (entering feedback loop) */

		if (BLI_BITMAP_TEST(grad_data->vert_visit, index) == 0) {
			DMGradient_vertStore *vs = &grad_data->vert_cache->elem[index];
			if (ED_view3d_project_float_object(grad_data->ar,
			                                   co, vs->sco,
			                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
			{
				/* ok */
				MDeformVert *dv = &me->dvert[index];
				const MDeformWeight *dw;
				dw = defvert_find_index(dv, grad_data->def_nr);
				if (dw) {
					vs->weight_orig = dw->weight;
					vs->flag = VGRAD_STORE_DW_EXIST;
				}
				else {
					vs->weight_orig = 0.0f;
					vs->flag = VGRAD_STORE_NOP;
				}

				BLI_BITMAP_ENABLE(grad_data->vert_visit, index);

				gradientVert_update(grad_data, index);
			}
			else {
				/* no go */
				copy_v2_fl(vs->sco, FLT_MAX);
			}
		}
	}
}

static int paint_weight_gradient_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	int ret = WM_gesture_straightline_modal(C, op, event);
	wmGesture *gesture = op->customdata;
	DMGradient_vertStoreBase *vert_cache = gesture->userdata;
	bool do_gesture_free = false;

	if (ret & OPERATOR_RUNNING_MODAL) {
		if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {  /* XXX, hardcoded */
			/* generally crap! redo! */
			do_gesture_free = true;
			ret &= ~OPERATOR_RUNNING_MODAL;
			ret |= OPERATOR_FINISHED;
		}
	}

	if (ret & OPERATOR_CANCELLED) {
		Object *ob = CTX_data_active_object(C);
		Mesh *me = ob->data;
		if (vert_cache->wpp.wpaint_prev) {
			BKE_defvert_array_free_elems(me->dvert, me->totvert);
			BKE_defvert_array_copy(me->dvert, vert_cache->wpp.wpaint_prev, me->totvert);
			wpaint_prev_destroy(&vert_cache->wpp);
		}

		DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	else if (ret & OPERATOR_FINISHED) {
		wpaint_prev_destroy(&vert_cache->wpp);
	}

	if (do_gesture_free) {
		WM_gesture_straightline_cancel(C, op);
	}

	return ret;
}

static int paint_weight_gradient_exec(bContext *C, wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	DMGradient_vertStoreBase *vert_cache;
	struct ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	EvaluationContext eval_ctx;
	Mesh *me = ob->data;
	int x_start = RNA_int_get(op->ptr, "xstart");
	int y_start = RNA_int_get(op->ptr, "ystart");
	int x_end = RNA_int_get(op->ptr, "xend");
	int y_end = RNA_int_get(op->ptr, "yend");
	float sco_start[2] = {x_start, y_start};
	float sco_end[2] = {x_end, y_end};
	const bool is_interactive = (gesture != NULL);
	DerivedMesh *dm;

	CTX_data_eval_ctx(C, &eval_ctx);

	dm = mesh_get_derived_final(&eval_ctx, scene, ob, scene->customdata_mask);

	DMGradient_userData data = {NULL};

	if (is_interactive) {
		if (gesture->userdata == NULL) {
			gesture->userdata = MEM_mallocN(
			        sizeof(DMGradient_vertStoreBase) +
			        (sizeof(DMGradient_vertStore) * me->totvert),
			        __func__);
			data.is_init = true;

			wpaint_prev_create(&((DMGradient_vertStoreBase *)gesture->userdata)->wpp, me->dvert, me->totvert);

			/* on init only, convert face -> vert sel  */
			if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
				BKE_mesh_flush_select_from_polys(me);
			}
		}

		vert_cache = gesture->userdata;
	}
	else {
		if (wpaint_ensure_data(C, op, 0, NULL) == false) {
			return OPERATOR_CANCELLED;
		}

		data.is_init = true;
		vert_cache = MEM_mallocN(
		        sizeof(DMGradient_vertStoreBase) +
		        (sizeof(DMGradient_vertStore) * me->totvert),
		        __func__);
	}

	data.ar = ar;
	data.scene = scene;
	data.me = ob->data;
	data.sco_start = sco_start;
	data.sco_end   = sco_end;
	data.sco_line_div = 1.0f / len_v2v2(sco_start, sco_end);
	data.def_nr = ob->actdef - 1;
	data.use_select = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL));
	data.vert_cache = vert_cache;
	data.vert_visit = NULL;
	data.type = RNA_enum_get(op->ptr, "type");

	{
		ToolSettings *ts = CTX_data_tool_settings(C);
		VPaint *wp = ts->wpaint;
		struct Brush *brush = BKE_paint_brush(&wp->paint);

		curvemapping_initialize(brush->curve);

		data.brush = brush;
		data.weightpaint = BKE_brush_weight_get(scene, brush);
	}

	ED_view3d_init_mats_rv3d(ob, ar->regiondata);

	if (data.is_init) {
		data.vert_visit = BLI_BITMAP_NEW(me->totvert, __func__);

		dm->foreachMappedVert(dm, gradientVertInit__mapFunc, &data, DM_FOREACH_NOP);

		MEM_freeN(data.vert_visit);
		data.vert_visit = NULL;
	}
	else {
		dm->foreachMappedVert(dm, gradientVertUpdate__mapFunc, &data, DM_FOREACH_NOP);
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	if (is_interactive == false) {
		MEM_freeN(vert_cache);
	}

	return OPERATOR_FINISHED;
}

static int paint_weight_gradient_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int ret;

	if (wpaint_ensure_data(C, op, 0, NULL) == false) {
		return OPERATOR_CANCELLED;
	}

	ret = WM_gesture_straightline_invoke(C, op, event);
	if (ret & OPERATOR_RUNNING_MODAL) {
		struct ARegion *ar = CTX_wm_region(C);
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			/* TODO, hardcoded, extend WM_gesture_straightline_ */
			if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
				wmGesture *gesture = op->customdata;
				gesture->mode = 1;
			}
		}
	}
	return ret;
}

void PAINT_OT_weight_gradient(wmOperatorType *ot)
{
	/* defined in DNA_space_types.h */
	static EnumPropertyItem gradient_types[] = {
		{WPAINT_GRADIENT_TYPE_LINEAR, "LINEAR", 0, "Linear", ""},
		{WPAINT_GRADIENT_TYPE_RADIAL, "RADIAL", 0, "Radial", ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Weight Gradient";
	ot->idname = "PAINT_OT_weight_gradient";
	ot->description = "Draw a line to apply a weight gradient to selected vertices";

	/* api callbacks */
	ot->invoke = paint_weight_gradient_invoke;
	ot->modal = paint_weight_gradient_modal;
	ot->exec = paint_weight_gradient_exec;
	ot->poll = weight_paint_poll;
	ot->cancel = WM_gesture_straightline_cancel;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_enum(ot->srna, "type", gradient_types, 0, "Type", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	WM_operator_properties_gesture_straightline(ot, CURSOR_EDIT);
}
