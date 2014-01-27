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
#include "BLI_memarena.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

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
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_colortools.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_buffers.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"  /* own include */


/* check if we can do partial updates and have them draw realtime
 * (without rebuilding the 'derivedFinal') */
static bool vertex_paint_use_fast_update_check(Object *ob)
{
	DerivedMesh *dm = ob->derivedFinal;

	if (dm) {
		Mesh *me = BKE_mesh_from_object(ob);
		if (me && me->mcol) {
			return (me->mcol == CustomData_get_layer(&dm->faceData, CD_MCOL));
		}
	}

	return FALSE;
}

/* if the polygons from the mesh and the 'derivedFinal' match
 * we can assume that no modifiers are applied and that its worth adding tessellated faces
 * so 'vertex_paint_use_fast_update_check()' returns TRUE */
static bool vertex_paint_use_tessface_check(Object *ob, Mesh *me)
{
	DerivedMesh *dm = ob->derivedFinal;

	if (me && dm) {
		return (me->mpoly == CustomData_get_layer(&dm->polyData, CD_MPOLY));
	}

	return FALSE;
}

static void update_tessface_data(Object *ob, Mesh *me)
{
	if (vertex_paint_use_tessface_check(ob, me)) {
		/* assume if these exist, that they are up to date & valid */
		if (!me->mcol || !me->mface) {
			/* should always be true */
			/* XXX Why this clearing? tessface_calc will reset it anyway! */
#if 0
			if (me->mcol) {
				memset(me->mcol, 255, 4 * sizeof(MCol) * me->totface);
			}
#endif

			/* create tessfaces because they will be used for drawing & fast updates */
			BKE_mesh_tessface_calc(me); /* does own call to update pointers */
		}
	}
	else {
		if (me->totface) {
			/* this wont be used, theres no need to keep it */
			BKE_mesh_tessface_clear(me);
		}
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

	return vp;
}

static int *get_indexarray(Mesh *me)
{
	return MEM_mallocN(sizeof(int) * (me->totpoly + 1), "vertexpaint");
}

unsigned int vpaint_get_current_col(VPaint *vp)
{
	Brush *brush = BKE_paint_brush(&vp->paint);
	unsigned char col[4];
	rgb_float_to_uchar(col, brush->rgb);
	col[3] = 255; /* alpha isn't used, could even be removed to speedup paint a little */
	return *(unsigned int *)col;
}

static void do_shared_vertex_tesscol(Mesh *me, bool *mfacetag)
{
	/* if no mcol: do not do */
	/* if tface: only the involved faces, otherwise all */
	const int use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL);
	MFace *mface;
	int a;
	short *scolmain, *scol;
	char *mcol;
	bool *mftag;
	
	if (me->mcol == NULL || me->totvert == 0 || me->totface == 0) return;
	
	scolmain = MEM_callocN(4 * sizeof(short) * me->totvert, "colmain");
	
	mface = me->mface;
	mcol = (char *)me->mcol;
	for (a = me->totface; a > 0; a--, mface++, mcol += 16) {
		if ((use_face_sel == FALSE) || (mface->flag & ME_FACE_SEL)) {
			scol = scolmain + 4 * mface->v1;
			scol[0]++; scol[1] += mcol[1]; scol[2] += mcol[2]; scol[3] += mcol[3];
			scol = scolmain + 4 * mface->v2;
			scol[0]++; scol[1] += mcol[5]; scol[2] += mcol[6]; scol[3] += mcol[7];
			scol = scolmain + 4 * mface->v3;
			scol[0]++; scol[1] += mcol[9]; scol[2] += mcol[10]; scol[3] += mcol[11];
			if (mface->v4) {
				scol = scolmain + 4 * mface->v4;
				scol[0]++; scol[1] += mcol[13]; scol[2] += mcol[14]; scol[3] += mcol[15];
			}
		}
	}
	
	a = me->totvert;
	scol = scolmain;
	while (a--) {
		if (scol[0] > 1) {
			scol[1] = divide_round_i(scol[1], scol[0]);
			scol[2] = divide_round_i(scol[2], scol[0]);
			scol[3] = divide_round_i(scol[3], scol[0]);
		}
		scol += 4;
	}

	mface = me->mface;
	mcol = (char *)me->mcol;
	mftag = mfacetag;
	for (a = me->totface; a > 0; a--, mface++, mcol += 16, mftag += 4) {
		if ((use_face_sel == FALSE) || (mface->flag & ME_FACE_SEL)) {
			if (mftag[0]) {
				scol = scolmain + 4 * mface->v1;
				mcol[1] = scol[1]; mcol[2] = scol[2]; mcol[3] = scol[3];
			}

			if (mftag[1]) {
				scol = scolmain + 4 * mface->v2;
				mcol[5] = scol[1]; mcol[6] = scol[2]; mcol[7] = scol[3];
			}

			if (mftag[2]) {
				scol = scolmain + 4 * mface->v3;
				mcol[9] = scol[1]; mcol[10] = scol[2]; mcol[11] = scol[3];
			}

			if (mface->v4 && mftag[3]) {
				scol = scolmain + 4 * mface->v4;
				mcol[13] = scol[1]; mcol[14] = scol[2]; mcol[15] = scol[3];
			}
		}
	}

	MEM_freeN(scolmain);
}

static void do_shared_vertexcol(Mesh *me, bool *mlooptag, bool *mfacetag, int do_tessface)
{
	const int use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL);
	MPoly *mp;
	int (*scol)[4];
	int i, j, has_shared = 0;

	/* if no mloopcol: do not do */
	/* if mtexpoly: only the involved faces, otherwise all */

	if (me->mloopcol == NULL || me->totvert == 0 || me->totpoly == 0) return;

	scol = MEM_callocN(sizeof(int) * me->totvert * 5, "scol");

	for (i = 0, mp = me->mpoly; i < me->totpoly; i++, mp++) {
		if ((use_face_sel == FALSE) || (mp->flag & ME_FACE_SEL)) {
			MLoop *ml = me->mloop + mp->loopstart;
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
			if ((use_face_sel == FALSE) || (mp->flag & ME_FACE_SEL)) {
				MLoop *ml = me->mloop + mp->loopstart;
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

	if (has_shared && do_tessface) {
		do_shared_vertex_tesscol(me, mfacetag);
	}
}

static bool make_vertexcol(Object *ob)  /* single ob */
{
	Mesh *me;

	if ((ob->id.lib) ||
	    ((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->totpoly == 0) ||
	    (me->edit_btmesh))
	{
		return false;
	}

	/* copies from shadedisplist to mcol */
	if (!me->mloopcol && me->totloop) {
		if (!me->mcol) {
			CustomData_add_layer(&me->fdata, CD_MCOL, CD_DEFAULT, NULL, me->totface);
		}
		if (!me->mloopcol) {
			CustomData_add_layer(&me->ldata, CD_MLOOPCOL, CD_DEFAULT, NULL, me->totloop);
		}
		BKE_mesh_update_customdata_pointers(me, true);
	}

	update_tessface_data(ob, me);
	
	DAG_id_tag_update(&me->id, 0);
	
	return (me->mloopcol != NULL);
}

/* mirror_vgroup is set to -1 when invalid */
static int wpaint_mirror_vgroup_ensure(Object *ob, const int vgroup_active)
{
	bDeformGroup *defgroup = BLI_findlink(&ob->defbase, vgroup_active);

	if (defgroup) {
		int mirrdef;
		char name_flip[MAXBONENAME];

		BKE_deform_flip_side_name(name_flip, defgroup->name, false);
		mirrdef = defgroup_name_index(ob, name_flip);
		if (mirrdef == -1) {
			if (BKE_defgroup_new(ob, name_flip)) {
				mirrdef = BLI_countlist(&ob->defbase) - 1;
			}
		}

		/* curdef should never be NULL unless this is
		 * a  lamp and ED_vgroup_add_name fails */
		return mirrdef;
	}

	return -1;
}

static void free_vpaint_prev(VPaint *vp)
{
	if (vp->vpaint_prev) {
		MEM_freeN(vp->vpaint_prev);
		vp->vpaint_prev = NULL;
		vp->tot = 0;
	}
}

static void free_wpaint_prev(VPaint *vp)
{
	if (vp->wpaint_prev) {
		BKE_defvert_array_free(vp->wpaint_prev, vp->tot);
		vp->wpaint_prev = NULL;
		vp->tot = 0;
	}
}

static void copy_vpaint_prev(VPaint *vp, unsigned int *lcol, int tot)
{
	free_vpaint_prev(vp);

	vp->tot = tot;
	
	if (lcol == NULL || tot == 0) return;
	
	vp->vpaint_prev = MEM_mallocN(sizeof(int) * tot, "vpaint_prev");
	memcpy(vp->vpaint_prev, lcol, sizeof(int) * tot);
	
}

static void copy_wpaint_prev(VPaint *wp, MDeformVert *dverts, int dcount)
{
	free_wpaint_prev(wp);
	
	if (dverts && dcount) {
		
		wp->wpaint_prev = MEM_mallocN(sizeof(MDeformVert) * dcount, "wpaint prev");
		wp->tot = dcount;
		BKE_defvert_array_copy(wp->wpaint_prev, dverts, dcount);
	}
}

bool ED_vpaint_fill(Object *ob, unsigned int paintcol)
{
	Mesh *me;
	MPoly *mp;
	int i, j;
	bool selected;

	if (((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->mloopcol == NULL && (make_vertexcol(ob) == false)))
	{
		return false;
	}

	selected = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	mp = me->mpoly;
	for (i = 0; i < me->totpoly; i++, mp++) {
		MLoopCol *lcol = me->mloopcol + mp->loopstart;

		if (selected && !(mp->flag & ME_FACE_SEL))
			continue;

		for (j = 0; j < mp->totloop; j++, lcol++) {
			*(int *)lcol = paintcol;
		}
	}
	
	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	DAG_id_tag_update(&me->id, 0);

	return true;
}


/* fills in the selected faces with the current weight and vertex group */
bool ED_wpaint_fill(VPaint *wp, Object *ob, float paintweight)
{
	Mesh *me = ob->data;
	MPoly *mp;
	MDeformWeight *dw, *dw_prev;
	int vgroup_active, vgroup_mirror = -1;
	unsigned int index;
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
	
	copy_wpaint_prev(wp, me->dvert, me->totvert);
	
	for (index = 0, mp = me->mpoly; index < me->totpoly; index++, mp++) {
		unsigned int fidx = mp->totloop - 1;

		if ((paint_selmode == SCE_SELECT_FACE) && !(mp->flag & ME_FACE_SEL)) {
			continue;
		}

		do {
			unsigned int vidx = me->mloop[mp->loopstart + fidx].v;

			if (!me->dvert[vidx].flag) {
				if ((paint_selmode == SCE_SELECT_VERTEX) && !(me->mvert[vidx].flag & SELECT)) {
					continue;
				}

				dw = defvert_verify_index(&me->dvert[vidx], vgroup_active);
				if (dw) {
					dw_prev = defvert_verify_index(wp->wpaint_prev + vidx, vgroup_active);
					dw_prev->weight = dw->weight; /* set the undo weight */
					dw->weight = paintweight;

					if (me->editflag & ME_EDIT_MIRROR_X) {  /* x mirror painting */
						int j = mesh_get_x_mirror_vert(ob, vidx, topology);
						if (j >= 0) {
							/* copy, not paint again */
							if (vgroup_mirror != -1) {
								dw = defvert_verify_index(me->dvert + j, vgroup_mirror);
								dw_prev = defvert_verify_index(wp->wpaint_prev + j, vgroup_mirror);
							}
							else {
								dw = defvert_verify_index(me->dvert + j, vgroup_active);
								dw_prev = defvert_verify_index(wp->wpaint_prev + j, vgroup_active);
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

	copy_wpaint_prev(wp, NULL, 0);

	DAG_id_tag_update(&me->id, 0);

	return true;
}

bool ED_vpaint_smooth(Object *ob)
{
	Mesh *me;
	MPoly *mp;

	int i, j;

	bool *mlooptag;
	bool selected;

	if (((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (me->mloopcol == NULL && (make_vertexcol(ob) == false)))
	{
		return false;
	}

	selected = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

	mlooptag = MEM_callocN(sizeof(bool) * me->totloop, "VPaintData mlooptag");

	/* simply tag loops of selected faces */
	mp = me->mpoly;
	for (i = 0; i < me->totpoly; i++, mp++) {
		MLoop *ml = me->mloop + mp->loopstart;
		int ml_index = mp->loopstart;

		if (selected && !(mp->flag & ME_FACE_SEL))
			continue;

		for (j = 0; j < mp->totloop; j++, ml_index++, ml++) {
			mlooptag[ml_index] = true;
		}
	}

	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	do_shared_vertexcol(me, mlooptag, NULL, false);

	MEM_freeN(mlooptag);

	DAG_id_tag_update(&me->id, 0);

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
	unsigned char *cp, gamtab[256];

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
	cp = (unsigned char *)me->mcol;
	while (a--) {

		cp[1] = gamtab[cp[1]];
		cp[2] = gamtab[cp[2]];
		cp[3] = gamtab[cp[3]];

		cp += 4;
	}
}
#endif

BLI_INLINE unsigned int mcol_blend(unsigned int col1, unsigned int col2, int fac)
{
	unsigned char *cp1, *cp2, *cp;
	int mfac;
	unsigned int col = 0;

	if (fac == 0) {
		return col1;
	}

	if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (unsigned char *)&col1;
	cp2 = (unsigned char *)&col2;
	cp  = (unsigned char *)&col;

	cp[0] = divide_round_i((mfac * cp1[0] + fac * cp2[0]), 255);
	cp[1] = divide_round_i((mfac * cp1[1] + fac * cp2[1]), 255);
	cp[2] = divide_round_i((mfac * cp1[2] + fac * cp2[2]), 255);
	cp[3] = 255;

	return col;
}

BLI_INLINE unsigned int mcol_add(unsigned int col1, unsigned int col2, int fac)
{
	unsigned char *cp1, *cp2, *cp;
	int temp;
	unsigned int col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (unsigned char *)&col1;
	cp2 = (unsigned char *)&col2;
	cp  = (unsigned char *)&col;

	temp = cp1[0] + divide_round_i((fac * cp2[0]), 255);
	cp[0] = (temp > 254) ? 255 : temp;
	temp = cp1[1] + divide_round_i((fac * cp2[1]), 255);
	cp[1] = (temp > 254) ? 255 : temp;
	temp = cp1[2] + divide_round_i((fac * cp2[2]), 255);
	cp[2] = (temp > 254) ? 255 : temp;
	cp[3] = 255;
	
	return col;
}

BLI_INLINE unsigned int mcol_sub(unsigned int col1, unsigned int col2, int fac)
{
	unsigned char *cp1, *cp2, *cp;
	int temp;
	unsigned int col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (unsigned char *)&col1;
	cp2 = (unsigned char *)&col2;
	cp  = (unsigned char *)&col;

	temp = cp1[0] - divide_round_i((fac * cp2[0]), 255);
	cp[0] = (temp < 0) ? 0 : temp;
	temp = cp1[1] - divide_round_i((fac * cp2[1]), 255);
	cp[1] = (temp < 0) ? 0 : temp;
	temp = cp1[2] - divide_round_i((fac * cp2[2]), 255);
	cp[2] = (temp < 0) ? 0 : temp;
	cp[3] = 255;

	return col;
}

BLI_INLINE unsigned int mcol_mul(unsigned int col1, unsigned int col2, int fac)
{
	unsigned char *cp1, *cp2, *cp;
	int mfac;
	unsigned int col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (unsigned char *)&col1;
	cp2 = (unsigned char *)&col2;
	cp  = (unsigned char *)&col;

	/* first mul, then blend the fac */
	cp[0] = divide_round_i(mfac * cp1[0] * 255 + fac * cp2[0] * cp1[0], 255 * 255);
	cp[1] = divide_round_i(mfac * cp1[1] * 255 + fac * cp2[1] * cp1[1], 255 * 255);
	cp[2] = divide_round_i(mfac * cp1[2] * 255 + fac * cp2[2] * cp1[2], 255 * 255);
	cp[3] = 255;

	return col;
}

BLI_INLINE unsigned int mcol_lighten(unsigned int col1, unsigned int col2, int fac)
{
	unsigned char *cp1, *cp2, *cp;
	int mfac;
	unsigned int col = 0;

	if (fac == 0) {
		return col1;
	}
	else if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (unsigned char *)&col1;
	cp2 = (unsigned char *)&col2;
	cp  = (unsigned char *)&col;

	/* See if are lighter, if so mix, else don't do anything.
	 * if the paint col is darker then the original, then ignore */
	if (rgb_to_grayscale_byte(cp1) > rgb_to_grayscale_byte(cp2)) {
		return col1;
	}

	cp[0] = divide_round_i(mfac * cp1[0] + fac * cp2[0], 255);
	cp[1] = divide_round_i(mfac * cp1[1] + fac * cp2[1], 255);
	cp[2] = divide_round_i(mfac * cp1[2] + fac * cp2[2], 255);
	cp[3] = 255;

	return col;
}

BLI_INLINE unsigned int mcol_darken(unsigned int col1, unsigned int col2, int fac)
{
	unsigned char *cp1, *cp2, *cp;
	int mfac;
	unsigned int col = 0;

	if (fac == 0) {
		return col1;
	}
	else if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (unsigned char *)&col1;
	cp2 = (unsigned char *)&col2;
	cp  = (unsigned char *)&col;

	/* See if were darker, if so mix, else don't do anything.
	 * if the paint col is brighter then the original, then ignore */
	if (rgb_to_grayscale_byte(cp1) < rgb_to_grayscale_byte(cp2)) {
		return col1;
	}

	cp[0] = divide_round_i((mfac * cp1[0] + fac * cp2[0]), 255);
	cp[1] = divide_round_i((mfac * cp1[1] + fac * cp2[1]), 255);
	cp[2] = divide_round_i((mfac * cp1[2] + fac * cp2[2]), 255);
	cp[3] = 255;
	return col;
}

/* wpaint has 'wpaint_blend_tool' */
static unsigned int vpaint_blend_tool(const int tool, const unsigned int col,
                                      const unsigned int paintcol, const int alpha_i)
{
	switch (tool) {
		case PAINT_BLEND_MIX:
		case PAINT_BLEND_BLUR:     return mcol_blend(col, paintcol, alpha_i);
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
static unsigned int vpaint_blend(VPaint *vp, unsigned int col, unsigned int colorig, const
                                 unsigned int paintcol, const int alpha_i,
                                 /* pre scaled from [0-1] --> [0-255] */
                                 const int brush_alpha_value_i)
{
	Brush *brush = BKE_paint_brush(&vp->paint);
	const int tool = brush->vertexpaint_tool;

	col = vpaint_blend_tool(tool, col, paintcol, alpha_i);

	/* if no spray, clip color adding with colorig & orig alpha */
	if ((vp->flag & VP_SPRAY) == 0) {
		unsigned int testcol, a;
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


static int sample_backbuf_area(ViewContext *vc, int *indexar, int totface, int x, int y, float size)
{
	struct ImBuf *ibuf;
	int a, tot = 0, index;
	
	/* brecht: disabled this because it obviously fails for
	 * brushes with size > 64, why is this here? */
	/*if (size > 64.0) size = 64.0;*/
	
	ibuf = view3d_read_backbuf(vc, x - size, y - size, x + size, y + size);
	if (ibuf) {
		unsigned int *rt = ibuf->rect;

		memset(indexar, 0, sizeof(int) * (totface + 1));
		
		size = ibuf->x * ibuf->y;
		while (size--) {
				
			if (*rt) {
				index = WM_framebuffer_to_index(*rt);
				if (index > 0 && index <= totface)
					indexar[index] = 1;
			}
		
			rt++;
		}
		
		for (a = 1; a <= totface; a++) {
			if (indexar[a]) indexar[tot++] = a;
		}

		IMB_freeImBuf(ibuf);
	}
	
	return tot;
}

/* whats _dl mean? */
static float calc_vp_strength_col_dl(VPaint *vp, ViewContext *vc, const float co[3],
                                 const float mval[2], const float brush_size_pressure, float rgba[4])
{
	float co_ss[2];  /* screenspace */

	if (ED_view3d_project_float_object(vc->ar,
	                                   co, co_ss,
	                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
	{
		const float dist_squared = len_squared_v2v2(mval, co_ss);

		if (dist_squared <= brush_size_pressure * brush_size_pressure) {
			Brush *brush = BKE_paint_brush(&vp->paint);
			const float dist = sqrtf(dist_squared);
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
			return factor * BKE_brush_curve_strength_clamp(brush, dist, brush_size_pressure);
		}
	}
	if (rgba)
		zero_v4(rgba);
	return 0.0f;
}

static float calc_vp_alpha_col_dl(VPaint *vp, ViewContext *vc,
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
static float wpaint_blend(VPaint *wp, float weight, float weight_prev,
                          const float alpha, float paintval,
                          const float brush_alpha_value,
                          const short do_flip, const short do_multipaint_totsel)
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

	/* delay clamping until the end so multi-paint can function when the active group is at the limits */
	if (do_multipaint_totsel == FALSE) {
		CLAMP(weight, 0.0f, 1.0f);
	}
	
	/* if no spray, clip result with orig weight & orig alpha */
	if ((wp->flag & VP_SPRAY) == 0) {
		if (do_multipaint_totsel == FALSE) {
			float testw = wpaint_blend_tool(tool, weight_prev, paintval, brush_alpha_value);

			CLAMP(testw, 0.0f, 1.0f);
			if (testw < weight_prev) {
				if (weight < testw) weight = testw;
				else if (weight > weight_prev) weight = weight_prev;
			}
			else {
				if (weight > testw) weight = testw;
				else if (weight < weight_prev) weight = weight_prev;
			}
		}
	}

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

	if (me && me->dvert && vc.v3d && vc.rv3d) {
		const int use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
		int v_idx_best = -1;
		unsigned int index;

		view3d_operator_needs_opengl(C);
		ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

		if (use_vert_sel) {
			if (ED_mesh_pick_vert(C, vc.obact, event->mval, &index, ED_MESH_PICK_DEFAULT_VERT_SIZE, TRUE)) {
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
static int weight_paint_sample_enum_itemf__helper(const MDeformVert *dvert, const int defbase_tot, int *groups)
{
	/* this func fills in used vgroup's */
	int found = FALSE;
	int i = dvert->totweight;
	MDeformWeight *dw;
	for (dw = dvert->dw; i > 0; dw++, i--) {
		if (dw->def_nr < defbase_tot) {
			groups[dw->def_nr] = TRUE;
			found = TRUE;
		}
	}
	return found;
}
static EnumPropertyItem *weight_paint_sample_enum_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	if (C) {
		wmWindow *win = CTX_wm_window(C);
		if (win && win->eventstate) {
			ViewContext vc;
			Mesh *me;

			view3d_set_viewcontext(C, &vc);
			me = BKE_mesh_from_object(vc.obact);

			if (me && me->dvert && vc.v3d && vc.rv3d && vc.obact->defbase.first) {
				const int defbase_tot = BLI_countlist(&vc.obact->defbase);
				const int use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
				int *groups = MEM_callocN(defbase_tot * sizeof(int), "groups");
				int found = FALSE;
				unsigned int index;

				int mval[2] = {win->eventstate->x - vc.ar->winrct.xmin,
				               win->eventstate->y - vc.ar->winrct.ymin};

				view3d_operator_needs_opengl(C);
				ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

				if (use_vert_sel) {
					if (ED_mesh_pick_vert(C, vc.obact, mval, &index, ED_MESH_PICK_DEFAULT_VERT_SIZE, TRUE)) {
						MDeformVert *dvert = &me->dvert[index];
						found |= weight_paint_sample_enum_itemf__helper(dvert, defbase_tot, groups);
					}
				}
				else {
					if (ED_mesh_pick_face(C, vc.obact, mval, &index, ED_MESH_PICK_DEFAULT_FACE_SIZE)) {
						MPoly *mp = &me->mpoly[index];
						unsigned int fidx = mp->totloop - 1;

						do {
							MDeformVert *dvert = &me->dvert[me->mloop[mp->loopstart + fidx].v];
							found |= weight_paint_sample_enum_itemf__helper(dvert, defbase_tot, groups);
						} while (fidx--);
					}
				}

				if (found == FALSE) {
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

	DAG_id_tag_update(&vc.obact->id, OB_RECALC_DATA);
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
	ot->prop = prop;
}

static void do_weight_paint_normalize_all(MDeformVert *dvert, const int defbase_tot, const bool *vgroup_validmap)
{
	float sum = 0.0f, fac;
	unsigned int i, tot = 0;
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

/* same as function above except it normalizes against the active vgroup which remains unchanged
 *
 * note that the active is just the group which is unchanged, it can be any,
 * can also be -1 to normalize all but in that case call 'do_weight_paint_normalize_all' */
static void do_weight_paint_normalize_all_active(MDeformVert *dvert, const int defbase_tot, const bool *vgroup_validmap,
                                                 const int vgroup_active)
{
	float sum = 0.0f, fac;
	unsigned int i, tot = 0;
	MDeformWeight *dw;
	float act_weight = 0.0f;

	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
			if (dw->def_nr != vgroup_active) {
				sum += dw->weight;
				tot++;
			}
			else {
				act_weight = dw->weight;
			}
		}
	}

	if ((tot == 0) || (sum + act_weight == 1.0f)) {
		return;
	}

	if (sum != 0.0f) {
		fac = (1.0f / sum) * (1.0f - act_weight);

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				if (dw->def_nr != vgroup_active) {
					dw->weight *= fac;

					/* paranoid but possibly with float error */
					CLAMP(dw->weight, 0.0f, 1.0f);
				}
			}
		}
	}
	else {
		/* corner case where we need to scale all weights evenly because they're all zero */

		/* hrmf, not a factor in this case */
		fac = (1.0f - act_weight) / tot;

		/* paranoid but possibly with float error */
		CLAMP(fac, 0.0f, 1.0f);

		for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
			if (dw->def_nr < defbase_tot && vgroup_validmap[dw->def_nr]) {
				if (dw->def_nr != vgroup_active) {
					dw->weight = fac;
				}
			}
		}
	}
}

/*
 * See if the current deform vertex has a locked group
 */
static bool has_locked_group(MDeformVert *dvert, const int defbase_tot,
                             const bool *bone_groups, const bool *lock_flags)
{
	int i;
	MDeformWeight *dw;

	for (i = dvert->totweight, dw = dvert->dw; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot) {
			if (bone_groups[dw->def_nr] && lock_flags[dw->def_nr] && dw->weight > 0.0f) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

static bool has_locked_group_selected(int defbase_tot, const bool *defbase_sel, const bool *lock_flags)
{
	int i;
	for (i = 0; i < defbase_tot; i++) {
		if (defbase_sel[i] && lock_flags[i]) {
			return true;
		}
	}
	return false;
}


#if 0 /* UNUSED */
static bool has_unselected_unlocked_bone_group(int defbase_tot, bool *defbase_sel, int selected,
                                               const bool *lock_flags, const bool *vgroup_validmap)
{
	int i;
	if (defbase_tot == selected) {
		return FALSE;
	}
	for (i = 0; i < defbase_tot; i++) {
		if (vgroup_validmap[i] && !defbase_sel[i] && !lock_flags[i]) {
			return TRUE;
		}
	}
	return FALSE;
}
#endif


static void multipaint_selection(MDeformVert *dvert, const int defbase_tot, float change, const bool *defbase_sel)
{
	int i;
	MDeformWeight *dw;
	float val;
	/* make sure they are all at most 1 after the change */
	for (i = 0; i < defbase_tot; i++) {
		if (defbase_sel[i]) {
			dw = defvert_find_index(dvert, i);
			if (dw && dw->weight) {
				val = dw->weight * change;
				if (val > 1) {
					/* TODO: when the change is reduced, you need to recheck
					 * the earlier values to make sure they are not 0
					 * (precision error) */
					change = 1.0f / dw->weight;
				}
				/* the value should never reach zero while multi-painting if it
				 * was nonzero beforehand */
				if (val <= 0) {
					return;
				}
			}
		}
	}
	/* apply the valid change */
	for (i = 0; i < defbase_tot; i++) {
		if (defbase_sel[i]) {
			dw = defvert_find_index(dvert, i);
			if (dw && dw->weight) {
				dw->weight = dw->weight * change;
			}
		}
	}
}

/* move all change onto valid, unchanged groups.  If there is change left over,
 * then return it.
 * assumes there are valid groups to shift weight onto */
static float redistribute_change(MDeformVert *ndv, const int defbase_tot,
                                 char *change_status, const char change_me, int changeto,
                                 float totchange, float total_valid,
                                 char do_auto_normalize)
{
	bool changed;
	float change;
	float oldval;
	MDeformWeight *ndw;
	int i;
	do {
		/* assume there is no change until you see one */
		changed = false;
		/* change each group by the same amount each time */
		change = totchange / total_valid;
		for (i = 0; i < ndv->totweight && total_valid && totchange; i++) {
			ndw = (ndv->dw + i);

			/* ignore anything outside the value range */
			if (ndw->def_nr < defbase_tot) {

				/* change only the groups with a valid status */
				if (change_status[ndw->def_nr] == change_me) {
					oldval = ndw->weight;
					/* if auto normalize is active, don't worry about upper bounds */
					if (do_auto_normalize == FALSE && ndw->weight + change > 1) {
						totchange -= 1.0f - ndw->weight;
						ndw->weight = 1.0f;
						/* stop the changes to this group */
						change_status[ndw->def_nr] = changeto;
						total_valid--;
					}
					else if (ndw->weight + change < 0) { /* check the lower bound */
						totchange -= ndw->weight;
						ndw->weight = 0;
						change_status[ndw->def_nr] = changeto;
						total_valid--;
					}
					else { /* a perfectly valid change occurred to ndw->weight */
						totchange -= change;
						ndw->weight += change;
					}
					/* see if there was a change */
					if (oldval != ndw->weight) {
						changed = true;
					}
				}
			}
		}
		/* don't go again if there was no change, if there is no valid group,
		 * or there is no change left */
	} while (changed && total_valid && totchange);
	/* left overs */
	return totchange;
}
static float get_mp_change(MDeformVert *odv, const int defbase_tot, const bool *defbase_sel, float brush_change);
/* observe the changes made to the weights of groups.
 * make sure all locked groups on the vertex have the same deformation
 * by moving the changes made to groups onto other unlocked groups */
static void enforce_locks(MDeformVert *odv, MDeformVert *ndv,
                          const int defbase_tot, const bool *defbase_sel,
                          const bool *lock_flags, const bool *vgroup_validmap,
                          char do_auto_normalize, char do_multipaint)
{
	float totchange = 0.0f;
	float totchange_allowed = 0.0f;
	float left_over;

	int total_valid = 0;
	int total_changed = 0;
	unsigned int i;
	MDeformWeight *ndw;
	MDeformWeight *odw;

	// float changed_sum = 0.0f;  // UNUSED

	char *change_status;

	if (!lock_flags || !has_locked_group(ndv, defbase_tot, vgroup_validmap, lock_flags)) {
		return;
	}
	/* record if a group was changed, unlocked and not changed, or locked */
	change_status = MEM_callocN(sizeof(char) * defbase_tot, "unlocked_unchanged");

	for (i = 0; i < defbase_tot; i++) {
		ndw = defvert_find_index(ndv, i);
		odw = defvert_find_index(odv, i);
		/* the weights are zero, so we can assume a lot */
		if (!ndw || !odw) {
			if (!lock_flags[i] && vgroup_validmap[i]) {
				defvert_verify_index(odv, i);
				defvert_verify_index(ndv, i);
				total_valid++;
				change_status[i] = 1; /* can be altered while redistributing */
			}
			continue;
		}
		/* locked groups should not be changed */
		if (lock_flags[i]) {
			ndw->weight = odw->weight;
		}
		else if (ndw->weight != odw->weight) { /* changed groups are handled here */
			totchange += ndw->weight - odw->weight;
			// changed_sum += ndw->weight;  // UNUSED
			change_status[i] = 2; /* was altered already */
			total_changed++;
		} /* unchanged, unlocked bone groups are handled here */
		else if (vgroup_validmap[i]) {
			totchange_allowed += ndw->weight;
			total_valid++;
			change_status[i] = 1; /* can be altered while redistributing */
		}
	}
	/* if there was any change, redistribute it */
	if (total_changed) {
		/* auto normalize will allow weights to temporarily go above 1 in redistribution */
		if (vgroup_validmap && total_changed < 0 && total_valid) {
			totchange_allowed = total_valid;
		}
		/* the way you modify the unlocked + unchanged groups is different depending
		 * on whether or not you are painting the weight(s) up or down */
		if (totchange < 0) {
			totchange_allowed = total_valid - totchange_allowed;
		}
		else {
			totchange_allowed *= -1;
		}
		/* there needs to be change allowed, or you should not bother */
		if (totchange_allowed) {
			left_over = 0;
			if (fabsf(totchange_allowed) < fabsf(totchange)) {
				/* this amount goes back onto the changed, unlocked weights */
				left_over = fabsf(fabsf(totchange) - fabsf(totchange_allowed));
				if (totchange > 0) {
					left_over *= -1;
				}
			}
			else {
				/* all of the change will be permitted */
				totchange_allowed = -totchange;
			}
			/* move the weight evenly between the allowed groups, move excess back onto the used groups based on the change */
			totchange_allowed = redistribute_change(ndv, defbase_tot, change_status, 1, -1, totchange_allowed, total_valid, do_auto_normalize);
			left_over += totchange_allowed;
			if (left_over) {
				/* more than one nonzero weights were changed with the same ratio with multipaint, so keep them changed that way! */
				if (total_changed > 1 && do_multipaint) {
					float undo_change = get_mp_change(ndv, defbase_tot, defbase_sel, left_over);
					multipaint_selection(ndv, defbase_tot, undo_change, defbase_sel);
				}
				/* or designatedw is still -1 put weight back as evenly as possible */
				else {
					redistribute_change(ndv, defbase_tot, change_status, 2, -2, left_over, total_changed, do_auto_normalize);
				}
			}
		}
		else {
			/* reset the weights */
			MDeformWeight *dw_old = odv->dw;
			MDeformWeight *dw_new = ndv->dw;

			for (i = odv->totweight; i != 0; i--, dw_old++, dw_new++) {
				dw_new->weight = dw_old->weight;
			}
		}
	}

	MEM_freeN(change_status);
}

/* multi-paint's initial, potential change is computed here based on the user's stroke */
static float get_mp_change(MDeformVert *odv, const int defbase_tot, const bool *defbase_sel, float brush_change)
{
	float selwsum = 0.0f;
	unsigned int i;
	MDeformWeight *dw = odv->dw;

	for (i = odv->totweight; i != 0; i--, dw++) {
		if (dw->def_nr < defbase_tot) {
			if (defbase_sel[dw->def_nr]) {
				selwsum += dw->weight;
			}
		}
	}
	if (selwsum && selwsum + brush_change > 0) {
		return (selwsum + brush_change) / selwsum;
	}
	return 0.0f;
}

/* change the weights back to the wv's weights
 * it assumes you already have the correct pointer index */
static void defvert_reset_to_prev(MDeformVert *dv_prev, MDeformVert *dv)
{
	MDeformWeight *dw = dv->dw;
	MDeformWeight *dw_prev;
	unsigned int i;
	for (i = dv->totweight; i != 0; i--, dw++) {
		dw_prev = defvert_find_index(dv_prev, dw->def_nr);
		/* if there was no w when there is a d, then the old weight was 0 */
		dw->weight = dw_prev ? dw_prev->weight : 0.0f;
	}
}

static void clamp_weights(MDeformVert *dvert)
{
	MDeformWeight *dw = dvert->dw;
	unsigned int i;
	for (i = dvert->totweight; i != 0; i--, dw++) {
		CLAMP(dw->weight, 0.0f, 1.0f);
	}
}

/* struct to avoid passing many args each call to do_weight_paint_vertex()
 * this _could_ be made a part of the operators 'WPaintData' struct, or at
 * least a member, but for now keep its own struct, initialized on every
 * paint stroke update - campbell */
typedef struct WeightPaintInfo {

	int defbase_tot;

	/* both must add up to 'defbase_tot' */
	int defbase_tot_sel;
	int defbase_tot_unsel;

	int vgroup_active; /* (ob->actdef - 1) */
	int vgroup_mirror; /* mirror group or -1 */

	const bool *lock_flags;  /* boolean array for locked bones,
	                          * length of defbase_tot */
	const bool *defbase_sel; /* boolean array for selected bones,
	                          * length of defbase_tot, cant be const because of how its passed */

	const bool *vgroup_validmap; /* same as WeightPaintData.vgroup_validmap,
	                              * only added here for convenience */

	char do_flip;
	char do_multipaint;
	char do_auto_normalize;

	float brush_alpha_value;  /* result of BKE_brush_alpha_get() */
} WeightPaintInfo;

/* fresh start to make multi-paint and locking modular */
/* returns TRUE if it thinks you need to reset the weights due to
 * normalizing while multi-painting
 *
 * note: this assumes dw->def_nr range has been checked by the caller
 */
static int apply_mp_locks_normalize(Mesh *me, const WeightPaintInfo *wpi,
                                    const unsigned int index,
                                    MDeformWeight *dw, MDeformWeight *tdw,
                                    float change, float oldChange,
                                    float oldw, float neww)
{
	MDeformVert *dv = &me->dvert[index];
	MDeformVert dv_test = {NULL};

	dv_test.dw = MEM_dupallocN(dv->dw);
	dv_test.flag = dv->flag;
	dv_test.totweight = dv->totweight;
	/* do not multi-paint if a locked group is selected or the active group is locked
	 * !lock_flags[dw->def_nr] helps if nothing is selected, but active group is locked */
	if ((wpi->lock_flags == NULL) ||
	    ((wpi->lock_flags[dw->def_nr] == false) && /* def_nr range has to be checked for by caller */
	     has_locked_group_selected(wpi->defbase_tot, wpi->defbase_sel, wpi->lock_flags) == false))
	{
		if (wpi->do_multipaint && wpi->defbase_tot_sel > 1) {
			if (change && change != 1) {
				multipaint_selection(dv, wpi->defbase_tot, change, wpi->defbase_sel);
			}
		}
		else { /* this lets users paint normally, but don't let them paint locked groups */
			dw->weight = neww;
		}
	}
	clamp_weights(dv);

	enforce_locks(&dv_test, dv, wpi->defbase_tot, wpi->defbase_sel, wpi->lock_flags, wpi->vgroup_validmap, wpi->do_auto_normalize, wpi->do_multipaint);

	if (wpi->do_auto_normalize) {
		/* XXX - should we pass the active group? - currently '-1' */
		do_weight_paint_normalize_all(dv, wpi->defbase_tot, wpi->vgroup_validmap);
	}

	if (oldChange && wpi->do_multipaint && wpi->defbase_tot_sel > 1) {
		if (tdw->weight != oldw) {
			if (neww > oldw) {
				if (tdw->weight <= oldw) {
					MEM_freeN(dv_test.dw);
					return TRUE;
				}
			}
			else {
				if (tdw->weight >= oldw) {
					MEM_freeN(dv_test.dw);
					return TRUE;
				}
			}
		}
	}
	MEM_freeN(dv_test.dw);
	return FALSE;
}

/* within the current dvert index, get the dw that is selected and has a weight
 * above 0, this helps multi-paint */
static int get_first_selected_nonzero_weight(MDeformVert *dvert, const int defbase_tot, const bool *defbase_sel)
{
	int i;
	MDeformWeight *dw = dvert->dw;
	for (i = 0; i < dvert->totweight; i++, dw++) {
		if (dw->def_nr < defbase_tot) {
			if (defbase_sel[dw->def_nr] && dw->weight > 0.0f) {
				return i;
			}
		}
	}
	return -1;
}

static void do_weight_paint_vertex(
        /* vars which remain the same for every vert */
        VPaint *wp, Object *ob, const WeightPaintInfo *wpi,
        /* vars which change on each stroke */
        const unsigned int index, float alpha, float paintweight
        )
{
	Mesh *me = ob->data;
	MDeformVert *dv = &me->dvert[index];
	bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
	
	MDeformWeight *dw, *dw_prev;

	/* mirror vars */
	int index_mirr;
	int vgroup_mirr;

	MDeformVert *dv_mirr;
	MDeformWeight *dw_mirr;

	const short do_multipaint_totsel = (wpi->do_multipaint && wpi->defbase_tot_sel > 1);

	if (wp->flag & VP_ONLYVGROUP) {
		dw = defvert_find_index(dv, wpi->vgroup_active);
		dw_prev = defvert_find_index(wp->wpaint_prev + index, wpi->vgroup_active);
	}
	else {
		dw = defvert_verify_index(dv, wpi->vgroup_active);
		dw_prev = defvert_verify_index(wp->wpaint_prev + index, wpi->vgroup_active);
	}

	if (dw == NULL || dw_prev == NULL) {
		return;
	}


	/* from now on we can check if mirrors enabled if this var is -1 and not bother with the flag */
	if (me->editflag & ME_EDIT_MIRROR_X) {
		index_mirr = mesh_get_x_mirror_vert(ob, index, topology);
		vgroup_mirr = (wpi->vgroup_mirror != -1) ? wpi->vgroup_mirror : wpi->vgroup_active;

		/* another possible error - mirror group _and_ active group are the same (which is fine),
		 * but we also are painting onto a center vertex - this would paint the same weight twice */
		if (index_mirr == index && vgroup_mirr == wpi->vgroup_active) {
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


	/* TODO: De-duplicate the simple weight paint - jason */
	/* ... or not, since its <10 SLOC - campbell */

	/* If there are no locks or multipaint,
	 * then there is no need to run the more complicated checks */
	if ((do_multipaint_totsel == FALSE) &&
	    (wpi->lock_flags == NULL || has_locked_group(dv, wpi->defbase_tot, wpi->vgroup_validmap, wpi->lock_flags) == FALSE))
	{
		dw->weight = wpaint_blend(wp, dw->weight, dw_prev->weight, alpha, paintweight,
		                          wpi->brush_alpha_value, wpi->do_flip, FALSE);

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
			do_weight_paint_normalize_all_active(dv, wpi->defbase_tot, wpi->vgroup_validmap, wpi->vgroup_active);

			if (index_mirr != -1) {
				/* only normalize if this is not a center vertex, else we get a conflict, normalizing twice */
				if (index != index_mirr) {
					do_weight_paint_normalize_all_active(dv_mirr, wpi->defbase_tot, wpi->vgroup_validmap, vgroup_mirr);
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
	else {
		/* use locks and/or multipaint */
		float oldw;
		float neww;
		/* float testw = 0; */ /* UNUSED */
		float observedChange = 0;
		float change = 0;
		float oldChange = 0;
		int i;
		MDeformWeight *tdw = NULL, *tdw_prev;
		MDeformVert dv_copy = {NULL};

		oldw = dw->weight;
		neww = wpaint_blend(wp, dw->weight, dw_prev->weight, alpha, paintweight,
		                    wpi->brush_alpha_value, wpi->do_flip, do_multipaint_totsel);
		
		/* setup multi-paint */
		observedChange = neww - oldw;
		if (do_multipaint_totsel && observedChange) {
			dv_copy.dw = MEM_dupallocN(dv->dw);
			dv_copy.flag = dv->flag;
			dv_copy.totweight = dv->totweight;
			tdw = dw;
			tdw_prev = dw_prev;
			change = get_mp_change(&wp->wpaint_prev[index], wpi->defbase_tot, wpi->defbase_sel, observedChange);
			if (change) {
				if (!tdw->weight) {
					i = get_first_selected_nonzero_weight(dv, wpi->defbase_tot, wpi->defbase_sel);
					if (i >= 0) {
						tdw = &(dv->dw[i]);
						tdw_prev = defvert_verify_index(&wp->wpaint_prev[index], tdw->def_nr);
					}
					else {
						change = 0;
					}
				}
				if (change && tdw_prev->weight && tdw_prev->weight * change) {
					if (tdw->weight != tdw_prev->weight) {
						oldChange = tdw->weight / tdw_prev->weight;
						/* testw = tdw_prev->weight * change; */ /* UNUSED */
						if (observedChange > 0) {
							if (change > oldChange) {
								/* reset the weights and use the new change */
								defvert_reset_to_prev(wp->wpaint_prev + index, dv);
							}
							else {
								/* the old change was more significant, so set
								 * the change to 0 so that it will not do another multi-paint */
								change = 0;
							}
						}
						else {
							if (change < oldChange) {
								defvert_reset_to_prev(wp->wpaint_prev + index, dv);
							}
							else {
								change = 0;
							}
						}
					}
				}
				else {
					change = 0;
				}
			}
		}
		
		if (apply_mp_locks_normalize(me, wpi, index, dw, tdw, change, oldChange, oldw, neww)) {
			defvert_reset_to_prev(&dv_copy, dv);
			change = 0;
			oldChange = 0;
		}
		if (dv_copy.dw) {
			MEM_freeN(dv_copy.dw);
		}
#if 0
		/* dv may have been altered greatly */
		dw = defvert_find_index(dv, vgroup);
#else
		dw = NULL; /* UNUSED after assignment, set to NULL to ensure we don't
		            * use again, we thats needed un-ifdef the line above */
		(void)dw;  /* quiet warnigns */
#endif

		/* x mirror painting */
		if (index_mirr != -1) {
			/* copy, not paint again */

			/* dw_mirr->weight = dw->weight; */  /* TODO, explain the logic in not assigning weight! - campbell */
			apply_mp_locks_normalize(me, wpi, index_mirr, dw_mirr, tdw, change, oldChange, oldw, neww);
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
		mesh_octree_table(NULL, NULL, NULL, 'e');
		mesh_mirrtopo_table(NULL, 'e');

		paint_cursor_delete_textures();
	}
	else {
		ob->mode |= mode_flag;

		if (wp == NULL)
			wp = scene->toolsettings->wpaint = new_vpaint(1);

		paint_cursor_start(C, weight_paint_poll);

		BKE_paint_init(&wp->paint, PAINT_CURSOR_WEIGHT_PAINT);

		/* weight paint specific */
		mesh_octree_table(ob, NULL, NULL, 's');
		ED_vgroup_sync_from_pose(ob);
	}
	
	/* Weightpaint works by overriding colors in mesh,
	 * so need to make sure we recalc on enter and
	 * exit (exit needs doing regardless because we
	 * should redeform).
	 */
	DAG_id_tag_update(&me->id, 0);

	WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

	return OPERATOR_FINISHED;
}

/* for switching to/from mode */
static int paint_poll_test(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if (ob == NULL || ob->type != OB_MESH)
		return 0;
	if (!ob->data || ((ID *)ob->data)->lib)
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

struct WPaintData {
	ViewContext vc;
	int *indexar;
	int vgroup_active;
	int vgroup_mirror;

	void *vp_handle;
	DMCoNo *vertexcosnos;

	float wpimat[3][3];

	/* variables for auto normalize */
	const bool *vgroup_validmap; /* stores if vgroups tie to deforming bones or not */
	const bool *lock_flags;
	int defbase_tot;
};

/* ensure we have data on wpaint start, add if needed */
static bool wpaint_ensure_data(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = BKE_mesh_from_object(ob);

	if (scene->obedit) {
		return FALSE;
	}

	if (me == NULL || me->totpoly == 0) {
		return FALSE;
	}

	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert) {
		ED_vgroup_data_create(&me->id);
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
						dg = ED_vgroup_add_name(ob, pchan->name);  /* sets actdef */
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
	if (ob->defbase.first == NULL) {
		ED_vgroup_add(ob);
	}

	/* ensure we don't try paint onto an invalid group */
	if (ob->actdef <= 0) {
		BKE_report(op->reports, RPT_WARNING, "No active vertex group for painting, aborting");
		return FALSE;
	}

	return TRUE;
}

static int wpaint_stroke_test_start(bContext *C, wmOperator *op, const float UNUSED(mouse[2]))
{
	Scene *scene = CTX_data_scene(C);
	struct PaintStroke *stroke = op->customdata;
	ToolSettings *ts = scene->toolsettings;
	VPaint *wp = ts->wpaint;
	Object *ob = CTX_data_active_object(C);
	Mesh *me = BKE_mesh_from_object(ob);
	struct WPaintData *wpd;

	float mat[4][4], imat[4][4];

	if (wpaint_ensure_data(C, op) == FALSE) {
		return FALSE;
	}

	{
		/* check if we are attempting to paint onto a locked vertex group,
		 * and other options disallow it from doing anything useful */
		bDeformGroup *dg = BLI_findlink(&ob->defbase, (ob->actdef - 1));
		if (dg->flag & DG_LOCK_WEIGHT) {
			BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
			return FALSE;
		}
	}

	/* ALLOCATIONS! no return after this line */
	/* make mode data storage */
	wpd = MEM_callocN(sizeof(struct WPaintData), "WPaintData");
	paint_stroke_set_mode_data(stroke, wpd);
	view3d_set_viewcontext(C, &wpd->vc);

	wpd->vgroup_active = ob->actdef - 1;
	wpd->vgroup_mirror = -1;

	/* set up auto-normalize, and generate map for detecting which
	 * vgroups affect deform bones */
	wpd->defbase_tot = BLI_countlist(&ob->defbase);
	wpd->lock_flags = BKE_objdef_lock_flags_get(ob, wpd->defbase_tot);
	if (ts->auto_normalize || ts->multipaint || wpd->lock_flags) {
		wpd->vgroup_validmap = BKE_objdef_validmap_get(ob, wpd->defbase_tot);
	}

	/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	wpd->vp_handle = ED_vpaint_proj_handle_create(scene, ob, &wpd->vertexcosnos);

	wpd->indexar = get_indexarray(me);
	copy_wpaint_prev(wp, me->dvert, me->totvert);

	/* imat for normals */
	mul_m4_m4m4(mat, wpd->vc.rv3d->viewmat, ob->obmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(wpd->wpimat, imat);

	/* if mirror painting, find the other group */
	if (me->editflag & ME_EDIT_MIRROR_X) {
		wpd->vgroup_mirror = wpaint_mirror_vgroup_ensure(ob, wpd->vgroup_active);
	}
	
	return TRUE;
}

static void wpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	VPaint *wp = ts->wpaint;
	Brush *brush = BKE_paint_brush(&wp->paint);
	struct WPaintData *wpd = paint_stroke_mode_data(stroke);
	ViewContext *vc;
	Object *ob;
	Mesh *me;
	float mat[4][4];
	float paintweight;
	int *indexar;
	float totw;
	unsigned int index, totindex;
	float alpha;
	float mval[2];
	bool use_vert_sel;
	bool use_face_sel;
	bool use_depth;

	MDeformWeight *(*dw_func)(MDeformVert *, const int) =
	        (brush->vertexpaint_tool == PAINT_BLEND_BLUR) ?
	        ((wp->flag & VP_ONLYVGROUP) ?
	             (MDeformWeight *(*)(MDeformVert *, const int))defvert_find_index :
	              defvert_verify_index) : NULL;

	const float pressure = RNA_float_get(itemptr, "pressure");
	const float brush_size_pressure = BKE_brush_size_get(scene, brush) * (BKE_brush_use_size_pressure(scene, brush) ? pressure : 1.0f);
	const float brush_alpha_value = BKE_brush_alpha_get(scene, brush);
	const float brush_alpha_pressure = brush_alpha_value * (BKE_brush_use_alpha_pressure(scene, brush) ? pressure : 1.0f);

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
	me = ob->data;
	indexar = wpd->indexar;
	
	view3d_operator_needs_opengl(C);
	ED_view3d_init_mats_rv3d(ob, vc->rv3d);

	/* load projection matrix */
	mul_m4_m4m4(mat, vc->rv3d->persmat, ob->obmat);

	RNA_float_get_array(itemptr, "mouse", mval);

	/* *** setup WeightPaintInfo - pass onto do_weight_paint_vertex *** */
	wpi.defbase_tot =        wpd->defbase_tot;
	wpi.defbase_sel = BKE_objdef_selected_get(ob, wpi.defbase_tot, &wpi.defbase_tot_sel);
	if (wpi.defbase_tot_sel == 0 && ob->actdef > 0) {
		wpi.defbase_tot_sel = 1;
	}

	wpi.defbase_tot_unsel =  wpi.defbase_tot - wpi.defbase_tot_sel;
	wpi.vgroup_active =      wpd->vgroup_active;
	wpi.vgroup_mirror =      wpd->vgroup_mirror;
	wpi.lock_flags =         wpd->lock_flags;
	wpi.vgroup_validmap =    wpd->vgroup_validmap;
	wpi.do_flip =            RNA_boolean_get(itemptr, "pen_flip");
	wpi.do_multipaint =      (ts->multipaint != 0);
	wpi.do_auto_normalize =  ((ts->auto_normalize != 0) && (wpi.vgroup_validmap != NULL));
	wpi.brush_alpha_value =  brush_alpha_value;
	/* *** done setting up WeightPaintInfo *** */



	swap_m4m4(wpd->vc.rv3d->persmat, mat);

	use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
	use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	use_depth = (vc->v3d->flag & V3D_ZBUF_SELECT) != 0;

	/* which faces are involved */
	if (use_depth) {
		char editflag_prev = me->editflag;

		/* Ugly hack, to avoid drawing vertex index when getting the face index buffer - campbell */
		me->editflag &= ~ME_EDIT_PAINT_VERT_SEL;
		if (use_vert_sel) {
			/* Ugly x2, we need this so hidden faces don't draw */
			me->editflag |= ME_EDIT_PAINT_FACE_SEL;
		}
		totindex = sample_backbuf_area(vc, indexar, me->totpoly, mval[0], mval[1], brush_size_pressure);
		me->editflag = editflag_prev;

		if (use_face_sel && me->totpoly) {
			MPoly *mpoly = me->mpoly;
			for (index = 0; index < totindex; index++) {
				if (indexar[index] && indexar[index] <= me->totpoly) {
					MPoly *mp = &mpoly[indexar[index] - 1];

					if ((mp->flag & ME_FACE_SEL) == 0) {
						indexar[index] = 0;
					}
				}
			}
		}
	}
	else {
		indexar = NULL;
	}

	/* incase we have modifiers */
	ED_vpaint_proj_handle_update(wpd->vp_handle, vc->ar, mval);

	/* make sure each vertex gets treated only once */
	/* and calculate filter weight */
	totw = 0.0f;
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR)
		paintweight = 0.0f;
	else
		paintweight = BKE_brush_weight_get(scene, brush);

#define WP_BLUR_ACCUM(v_idx_var)  \
	{ \
		const unsigned int vidx = v_idx_var; \
		const float fac = calc_vp_strength_col_dl(wp, vc, wpd->vertexcosnos[vidx].co, mval, brush_size_pressure, NULL); \
		if (fac > 0.0f) { \
			MDeformWeight *dw = dw_func(&me->dvert[vidx], wpi.vgroup_active); \
			paintweight += dw ? (dw->weight * fac) : 0.0f; \
			totw += fac; \
		} \
	} (void)0


	if (use_depth) {
		for (index = 0; index < totindex; index++) {
			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = me->mpoly + (indexar[index] - 1);
				MLoop *ml = me->mloop + mpoly->loopstart;
				int i;

				if (use_vert_sel) {
					for (i = 0; i < mpoly->totloop; i++, ml++) {
						me->dvert[ml->v].flag = (me->mvert[ml->v].flag & SELECT);
					}
				}
				else {
					for (i = 0; i < mpoly->totloop; i++, ml++) {
						me->dvert[ml->v].flag = 1;
					}
				}

				if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
					ml = me->mloop + mpoly->loopstart;
					for (i = 0; i < mpoly->totloop; i++, ml++) {
						WP_BLUR_ACCUM(ml->v);
					}
				}
			}
		}
	}
	else {
		const unsigned int totvert = me->totvert;
		unsigned int       i;

		/* in the case of face selection we need to flush */
		if (use_vert_sel || use_face_sel) {
			for (i = 0; i < totvert; i++) {
				me->dvert[i].flag = me->mvert[i].flag & SELECT;
			}
		}
		else {
			for (i = 0; i < totvert; i++) {
				me->dvert[i].flag = SELECT;
			}
		}

		if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
			for (i = 0; i < totvert; i++) {
				WP_BLUR_ACCUM(i);
			}
		}
	}

#undef WP_BLUR_ACCUM


	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		paintweight /= totw;
	}

#define WP_PAINT(v_idx_var)  \
	{ \
		unsigned int vidx = v_idx_var; \
		if (me->dvert[vidx].flag) { \
			alpha = calc_vp_alpha_col_dl(wp, vc, wpd->wpimat, &wpd->vertexcosnos[vidx], \
			                         mval, brush_size_pressure, brush_alpha_pressure, NULL); \
			if (alpha) { \
				do_weight_paint_vertex(wp, ob, &wpi, vidx, alpha, paintweight); \
			} \
			me->dvert[vidx].flag = 0; \
		} \
	} (void)0

	if (use_depth) {
		for (index = 0; index < totindex; index++) {

			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = me->mpoly + (indexar[index] - 1);
				MLoop *ml = me->mloop + mpoly->loopstart;
				int i;

				for (i = 0; i < mpoly->totloop; i++, ml++) {
					WP_PAINT(ml->v);
				}
			}
		}
	}
	else {
		const unsigned int totvert = me->totvert;
		unsigned int       i;

		for (i = 0; i < totvert; i++) {
			WP_PAINT(i);
		}
	}
#undef WP_PAINT


	/* *** free wpi members */
	MEM_freeN((void *)wpi.defbase_sel);
	/* *** done freeing wpi members */


	swap_m4m4(vc->rv3d->persmat, mat);

	{
		UnifiedPaintSettings *ups = &ts->unified_paint_settings;
		ups->pressure_value = pressure;
	}

	DAG_id_tag_update(ob->data, 0);
	ED_region_tag_redraw(vc->ar);
}

static void wpaint_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	struct WPaintData *wpd = paint_stroke_mode_data(stroke);
	
	if (wpd) {
		ED_vpaint_proj_handle_free(wpd->vp_handle);
		MEM_freeN(wpd->indexar);
		
		if (wpd->vgroup_validmap)
			MEM_freeN((void *)wpd->vgroup_validmap);
		if (wpd->lock_flags)
			MEM_freeN((void *)wpd->lock_flags);

		MEM_freeN(wpd);
	}
	
	/* frees prev buffer */
	copy_wpaint_prev(ts->wpaint, NULL, 0);
	
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

	DAG_id_tag_update(ob->data, 0);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
}


static int wpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

	op->customdata = paint_stroke_new(C, NULL, wpaint_stroke_test_start,
	                                  wpaint_stroke_update_step, NULL,
	                                  wpaint_stroke_done, event->type);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	retval = op->type->modal(C, op, event);
	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);
	
	return OPERATOR_RUNNING_MODAL;
}

static int wpaint_exec(bContext *C, wmOperator *op)
{
	op->customdata = paint_stroke_new(C, NULL, wpaint_stroke_test_start,
	                                  wpaint_stroke_update_step, NULL,
	                                  wpaint_stroke_done, 0);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

static void wpaint_cancel(bContext *C, wmOperator *op)
{
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

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

static int weight_paint_set_exec(bContext *C, wmOperator *op)
{
	struct Scene *scene = CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
	float vgroup_weight = BKE_brush_weight_get(scene, brush);

	if (wpaint_ensure_data(C, op) == FALSE) {
		return OPERATOR_CANCELLED;
	}

	if (ED_wpaint_fill(scene->toolsettings->wpaint, obact, vgroup_weight)) {
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

		paint_cursor_delete_textures();
	}
	else {
		ob->mode |= mode_flag;

		if (me->mloopcol == NULL) {
			make_vertexcol(ob);
		}

		if (vp == NULL)
			vp = scene->toolsettings->vpaint = new_vpaint(0);
		
		paint_cursor_start(C, vertex_paint_poll);

		BKE_paint_init(&vp->paint, PAINT_CURSOR_VERTEX_PAINT);
	}
	
	/* update modifier stack for mapping requirements */
	DAG_id_tag_update(&me->id, 0);
	
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

typedef struct VPaintData {
	ViewContext vc;
	unsigned int paintcol;
	int *indexar;

	struct VertProjHandle *vp_handle;
	DMCoNo *vertexcosnos;

	float vpimat[3][3];

	/* modify 'me->mcol' directly, since the derived mesh is drawing from this
	 * array, otherwise we need to refresh the modifier stack */
	bool use_fast_update;

	/* mpoly -> mface mapping */
	MeshElemMap *polyfacemap;
	void *polyfacemap_mem;

	/* loops tagged as having been painted, to apply shared vertex color
	 * blending only to modified loops */
	bool *mlooptag;
	bool *mfacetag;

	bool is_texbrush;
} VPaintData;

static void vpaint_build_poly_facemap(struct VPaintData *vd, Mesh *me)
{
	const int *tessface_origindex;

	vd->polyfacemap = NULL;
	vd->polyfacemap_mem = NULL;

	tessface_origindex = CustomData_get_layer(&me->fdata, CD_ORIGINDEX);

	if (!tessface_origindex)
		return;

	BKE_mesh_origindex_map_create(&vd->polyfacemap, (int **)&vd->polyfacemap_mem,
	                              me->totpoly,
	                              tessface_origindex, me->totface);
}

static int vpaint_stroke_test_start(bContext *C, struct wmOperator *op, const float UNUSED(mouse[2]))
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct PaintStroke *stroke = op->customdata;
	VPaint *vp = ts->vpaint;
	Brush *brush = BKE_paint_brush(&vp->paint);
	struct VPaintData *vpd;
	Object *ob = CTX_data_active_object(C);
	Mesh *me;
	float mat[4][4], imat[4][4];

	/* context checks could be a poll() */
	me = BKE_mesh_from_object(ob);
	if (me == NULL || me->totpoly == 0)
		return OPERATOR_PASS_THROUGH;
	
	if (me->mloopcol == NULL)
		make_vertexcol(ob);
	if (me->mloopcol == NULL)
		return OPERATOR_CANCELLED;

	/* Update tessface data if needed
	 * Added here too because e.g. switching to/from edit mode would remove tessface data,
	 * yet "fast_update" could still be used! */
	update_tessface_data(ob, me);

	/* make mode data storage */
	vpd = MEM_callocN(sizeof(struct VPaintData), "VPaintData");
	paint_stroke_set_mode_data(stroke, vpd);
	view3d_set_viewcontext(C, &vpd->vc);
	
	vpd->vp_handle = ED_vpaint_proj_handle_create(vpd->vc.scene, ob, &vpd->vertexcosnos);

	vpd->indexar = get_indexarray(me);
	vpd->paintcol = vpaint_get_current_col(vp);

	vpd->is_texbrush = !(brush->vertexpaint_tool == PAINT_BLEND_BLUR) &&
	                   brush->mtex.tex;

	/* are we painting onto a modified mesh?,
	 * if not we can skip face map trickyness */
	if (vertex_paint_use_fast_update_check(ob)) {
		vpaint_build_poly_facemap(vpd, me);
		vpd->use_fast_update = TRUE;
/*		printf("Fast update!\n");*/
	}
	else {
		vpd->use_fast_update = FALSE;
/*		printf("No fast update!\n");*/
	}

	/* to keep tracked of modified loops for shared vertex color blending */
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		vpd->mlooptag = MEM_mallocN(sizeof(bool) * me->totloop, "VPaintData mlooptag");
		if (vpd->use_fast_update)
			vpd->mfacetag = MEM_mallocN(sizeof(bool) * me->totface * 4, "VPaintData mfacetag");
	}

	/* for filtering */
	copy_vpaint_prev(vp, (unsigned int *)me->mloopcol, me->totloop);
	
	/* some old cruft to sort out later */
	mul_m4_m4m4(mat, vpd->vc.rv3d->viewmat, ob->obmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(vpd->vpimat, imat);

	return 1;
}

static void vpaint_paint_poly(VPaint *vp, VPaintData *vpd, Mesh *me,
                              const unsigned int index, const float mval[2],
                              const float brush_size_pressure, const float brush_alpha_pressure)
{
	ViewContext *vc = &vpd->vc;
	Brush *brush = BKE_paint_brush(&vp->paint);
	MPoly *mpoly = &me->mpoly[index];
	MFace *mf;
	MCol *mc;
	MLoop *ml;
	MLoopCol *mlc;
	unsigned int *lcol = ((unsigned int *)me->mloopcol) + mpoly->loopstart;
	unsigned int *lcolorig = ((unsigned int *)vp->vpaint_prev) + mpoly->loopstart;
	bool *mlooptag = (vpd->mlooptag) ? vpd->mlooptag + mpoly->loopstart : NULL;
	bool *mftag;
	float alpha;
	int i, j;
	int totloop = mpoly->totloop;

	int brush_alpha_pressure_i = (int)(brush_alpha_pressure * 255.0f);

	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		unsigned int blend[4] = {0};
		unsigned int tcol;
		char *col;

		for (j = 0; j < totloop; j++) {
			col = (char *)(lcol + j);
			blend[0] += col[0];
			blend[1] += col[1];
			blend[2] += col[2];
			blend[3] += col[3];
		}

		blend[0] = divide_round_i(blend[0], totloop);
		blend[1] = divide_round_i(blend[1], totloop);
		blend[2] = divide_round_i(blend[2], totloop);
		blend[3] = divide_round_i(blend[3], totloop);
		col = (char *)&tcol;
		col[0] = blend[0];
		col[1] = blend[1];
		col[2] = blend[2];
		col[3] = blend[3];

		vpd->paintcol = *((unsigned int *)col);
	}

	ml = me->mloop + mpoly->loopstart;
	for (i = 0; i < totloop; i++, ml++) {
		float rgba[4];
		unsigned int paintcol;
		alpha = calc_vp_alpha_col_dl(vp, vc, vpd->vpimat,
		                         &vpd->vertexcosnos[ml->v], mval,
		                         brush_size_pressure, brush_alpha_pressure, rgba);

		if (vpd->is_texbrush) {
			float rgba_br[3];
			rgb_uchar_to_float(rgba_br, (const unsigned char *)&vpd->paintcol);
			mul_v3_v3(rgba_br, rgba);
			rgb_float_to_uchar((unsigned char *)&paintcol, rgba_br);
		}
		else
			paintcol = vpd->paintcol;

		if (alpha > 0.0f) {
			const int alpha_i = (int)(alpha * 255.0f);
			lcol[i] = vpaint_blend(vp, lcol[i], lcolorig[i], paintcol, alpha_i, brush_alpha_pressure_i);

			if (mlooptag) mlooptag[i] = 1;
		}
	}

	if (vpd->use_fast_update) {
		const MeshElemMap *map = &vpd->polyfacemap[index];

		/* update vertex colors for tessellations incrementally,
		 * rather then regenerating the tessellation altogether */
		for (i = 0; i < map->count; i++) {
			const int index_tessface = map->indices[i];

			mf = &me->mface[index_tessface];
			mc = &me->mcol[index_tessface * 4];
			mftag = &vpd->mfacetag[index_tessface * 4];

			ml = me->mloop + mpoly->loopstart;
			mlc = me->mloopcol + mpoly->loopstart;

			for (j = 0; j < totloop; j++, ml++, mlc++) {
				/* search for the loop vertex within the tessface */
				const int fidx = BKE_MESH_TESSFACE_VINDEX_ORDER(mf, ml->v);
				if (fidx != -1) {
					MESH_MLOOPCOL_TO_MCOL(mlc, mc + fidx);
					if (mlooptag) {
						mftag[fidx] = mlooptag[j];
					}
				}
			}
		}
	}

}

static void vpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	VPaint *vp = ts->vpaint;
	Brush *brush = BKE_paint_brush(&vp->paint);
	ViewContext *vc = &vpd->vc;
	Object *ob = vc->obact;
	Mesh *me = ob->data;
	float mat[4][4];
	int *indexar = vpd->indexar;
	int totindex, index;
	float mval[2];

	const float pressure = RNA_float_get(itemptr, "pressure");
	const float brush_size_pressure = BKE_brush_size_get(scene, brush) * (BKE_brush_use_size_pressure(scene, brush) ? pressure : 1.0f);
	const float brush_alpha_pressure = BKE_brush_alpha_get(scene, brush) * (BKE_brush_use_alpha_pressure(scene, brush) ? pressure : 1.0f);

	RNA_float_get_array(itemptr, "mouse", mval);

	view3d_operator_needs_opengl(C);
	ED_view3d_init_mats_rv3d(ob, vc->rv3d);

	/* load projection matrix */
	mul_m4_m4m4(mat, vc->rv3d->persmat, ob->obmat);

	/* which faces are involved */
	totindex = sample_backbuf_area(vc, indexar, me->totpoly, mval[0], mval[1], brush_size_pressure);

	if ((me->editflag & ME_EDIT_PAINT_FACE_SEL) && me->mpoly) {
		for (index = 0; index < totindex; index++) {
			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = ((MPoly *)me->mpoly) + (indexar[index] - 1);
						
				if ((mpoly->flag & ME_FACE_SEL) == 0)
					indexar[index] = 0;
			}
		}
	}
	
	swap_m4m4(vc->rv3d->persmat, mat);

	/* incase we have modifiers */
	ED_vpaint_proj_handle_update(vpd->vp_handle, vc->ar, mval);

	/* clear modified tag for blur tool */
	if (vpd->mlooptag)
		memset(vpd->mlooptag, 0, sizeof(bool) * me->totloop);
	if (vpd->mfacetag)
		memset(vpd->mfacetag, 0, sizeof(bool) * me->totface * 4);

	for (index = 0; index < totindex; index++) {
		if (indexar[index] && indexar[index] <= me->totpoly) {
			vpaint_paint_poly(vp, vpd, me, indexar[index] - 1, mval, brush_size_pressure, brush_alpha_pressure);
		}
	}
		
	swap_m4m4(vc->rv3d->persmat, mat);

	/* was disabled because it is slow, but necessary for blur */
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		int do_tessface = vpd->use_fast_update;
		do_shared_vertexcol(me, vpd->mlooptag, vpd->mfacetag, do_tessface);
	}

	{
		UnifiedPaintSettings *ups = &ts->unified_paint_settings;
		ups->pressure_value = pressure;
	}

	ED_region_tag_redraw(vc->ar);

	if (vpd->use_fast_update == FALSE) {
		/* recalculate modifier stack to get new colors, slow,
		 * avoid this if we can! */
		DAG_id_tag_update(ob->data, 0);
	}
	else if (!GPU_buffer_legacy(ob->derivedFinal)) {
		/* If using new VBO drawing, mark mcol as dirty to force colors gpu buffer refresh! */
		ob->derivedFinal->dirty |= DM_DIRTY_MCOL_UPDATE_DRAW;
	}
}

static void vpaint_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	ViewContext *vc = &vpd->vc;
	Object *ob = vc->obact;
	
	ED_vpaint_proj_handle_free(vpd->vp_handle);
	MEM_freeN(vpd->indexar);
	
	/* frees prev buffer */
	copy_vpaint_prev(ts->vpaint, NULL, 0);

	if (vpd->polyfacemap) {
		MEM_freeN(vpd->polyfacemap);
	}

	if (vpd->polyfacemap_mem) {
		MEM_freeN(vpd->polyfacemap_mem);
	}

	if (vpd->mlooptag)
		MEM_freeN(vpd->mlooptag);

	if (vpd->mfacetag)
		MEM_freeN(vpd->mfacetag);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	MEM_freeN(vpd);
}

static int vpaint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

	op->customdata = paint_stroke_new(C, NULL, vpaint_stroke_test_start,
	                                  vpaint_stroke_update_step, NULL,
	                                  vpaint_stroke_done, event->type);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	retval = op->type->modal(C, op, event);
	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);
	
	return OPERATOR_RUNNING_MODAL;
}

static int vpaint_exec(bContext *C, wmOperator *op)
{
	op->customdata = paint_stroke_new(C, NULL, vpaint_stroke_test_start,
	                                  vpaint_stroke_update_step, NULL,
	                                  vpaint_stroke_done, 0);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

static void vpaint_cancel(bContext *C, wmOperator *op)
{
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

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
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

	create_vgroups_from_armature(op->reports, scene, ob, armob, type, (me->editflag & ME_EDIT_MIRROR_X));

	DAG_id_tag_update(&me->id, 0);
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

typedef struct DMGradient_userData {
	struct ARegion *ar;
	Scene *scene;
	Mesh *me;
	Brush *brush;
	const float *sco_start;     /* [2] */
	const float *sco_end;       /* [2] */
	float        sco_line_div;  /* store (1.0f / len_v2v2(sco_start, sco_end)) */
	int def_nr;
	short is_init;
	DMGradient_vertStore *vert_cache;

	/* options */
	short use_select;
	short type;
	float weightpaint;
} DMGradient_userData;

static void gradientVert__mapFunc(void *userData, int index, const float co[3],
                                  const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	DMGradient_userData *grad_data = userData;
	Mesh *me = grad_data->me;

	if (grad_data->use_select == FALSE || (me->mvert[index].flag & SELECT)) {
		DMGradient_vertStore *vs = &grad_data->vert_cache[index];

		/* run first pass only, could be split into its own mapFunc
		 * the screen coords of the verts need to be cached because
		 * updating the mesh may move them about (entering feedback loop) */
		if (grad_data->is_init) {
			if (ED_view3d_project_float_object(grad_data->ar,
			                                   co, vs->sco,
			                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
			{
				/* ok */
				MDeformVert *dv = &me->dvert[index];
				MDeformWeight *dw;
				dw = defvert_find_index(dv, grad_data->def_nr);
				if (dw) {
					vs->weight_orig = dw->weight;
					vs->flag = VGRAD_STORE_DW_EXIST;
				}
				else {
					vs->weight_orig = 0.0f;
					vs->flag = VGRAD_STORE_NOP;
				}
			}
			else {
				/* no go */
				copy_v2_fl(vs->sco, FLT_MAX);
			}
		}
		/* end init */

		if (vs->sco[0] != FLT_MAX) {
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
			alpha = BKE_brush_curve_strength_clamp(grad_data->brush, alpha, 1.0f);

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
	}
}

static int paint_weight_gradient_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	int ret = WM_gesture_straightline_modal(C, op, event);

	if (ret & OPERATOR_RUNNING_MODAL) {
		if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {  /* XXX, hardcoded */
			/* generally crap! redo! */
			WM_gesture_straightline_cancel(C, op);
			ret &= ~OPERATOR_RUNNING_MODAL;
			ret |= OPERATOR_FINISHED;
		}
	}

	if (ret & OPERATOR_CANCELLED) {
		ToolSettings *ts = CTX_data_tool_settings(C);
		VPaint *wp = ts->wpaint;
		Object *ob = CTX_data_active_object(C);
		Mesh *me = ob->data;
		if (wp->wpaint_prev) {
			BKE_defvert_array_free_elems(me->dvert, me->totvert);
			BKE_defvert_array_copy(me->dvert, wp->wpaint_prev, me->totvert);
			free_wpaint_prev(wp);
		}

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
	else if (ret & OPERATOR_FINISHED) {
		ToolSettings *ts = CTX_data_tool_settings(C);
		VPaint *wp = ts->wpaint;
		free_wpaint_prev(wp);
	}

	return ret;
}

static int paint_weight_gradient_exec(bContext *C, wmOperator *op)
{
	wmGesture *gesture = op->customdata;
	DMGradient_vertStore *vert_cache;
	struct ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	int x_start = RNA_int_get(op->ptr, "xstart");
	int y_start = RNA_int_get(op->ptr, "ystart");
	int x_end = RNA_int_get(op->ptr, "xend");
	int y_end = RNA_int_get(op->ptr, "yend");
	float sco_start[2] = {x_start, y_start};
	float sco_end[2] = {x_end, y_end};
	const bool is_interactive = (gesture != NULL);
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);

	DMGradient_userData data = {NULL};

	if (is_interactive) {
		if (gesture->userdata == NULL) {
			VPaint *wp = scene->toolsettings->wpaint;

			gesture->userdata = MEM_mallocN(sizeof(DMGradient_vertStore) * me->totvert, __func__);
			data.is_init = true;

			copy_wpaint_prev(wp, me->dvert, me->totvert);

			/* on init only, convert face -> vert sel  */
			if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
				BKE_mesh_flush_select_from_polys(me);
			}
		}

		vert_cache = gesture->userdata;
	}
	else {
		if (wpaint_ensure_data(C, op) == FALSE) {
			return OPERATOR_CANCELLED;
		}

		data.is_init = true;
		vert_cache = MEM_mallocN(sizeof(DMGradient_vertStore) * me->totvert, __func__);
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

	dm->foreachMappedVert(dm, gradientVert__mapFunc, &data, DM_FOREACH_NOP);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	if (is_interactive == false) {
		MEM_freeN(vert_cache);
	}

	return OPERATOR_FINISHED;
}

static int paint_weight_gradient_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int ret;

	if (wpaint_ensure_data(C, op) == FALSE) {
		return OPERATOR_CANCELLED;
	}

	ret = WM_gesture_straightline_invoke(C, op, event);
	if (ret & OPERATOR_RUNNING_MODAL) {
		struct ARegion *ar = CTX_wm_region(C);
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			if (event->type == LEFTMOUSE && event->val == KM_PRESS) {  /* TODO, hardcoded, extend WM_gesture_straightline_ */
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
