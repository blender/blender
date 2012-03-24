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


#include <math.h>
#include <string.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"


#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"

/* check if we can do partial updates and have them draw realtime
 * (without rebuilding the 'derivedFinal') */
static int vertex_paint_use_fast_update_check(Object *ob)
{
	DerivedMesh *dm = ob->derivedFinal;

	if (dm) {
		Mesh *me = get_mesh(ob);
		if (me && me->mcol) {
			return (me->mcol == CustomData_get_layer(&dm->faceData, CD_MCOL));
		}
	}

	return FALSE;
}

/* if the polygons from the mesh and the 'derivedFinal' match
 * we can assume that no modifiers are applied and that its worth adding tessellated faces
 * so 'vertex_paint_use_fast_update_check()' returns TRUE */
static int vertex_paint_use_tessface_check(Object *ob)
{
	DerivedMesh *dm = ob->derivedFinal;

	if (dm) {
		Mesh *me = get_mesh(ob);
		return (me->mpoly == CustomData_get_layer(&dm->faceData, CD_MPOLY));
	}

	return FALSE;
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
	   paint_brush(&CTX_data_tool_settings(C)->vpaint->paint)) {
		ScrArea *sa = CTX_wm_area(C);
		if (sa->spacetype == SPACE_VIEW3D) {
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
	    (paint_brush(&CTX_data_tool_settings(C)->wpaint->paint) != NULL) &&
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
	
	vp->flag = VP_AREA + VP_SPRAY;
	
	if (wpaint)
		vp->flag = VP_AREA;

	return vp;
}

static int *get_indexarray(Mesh *me)
{
	return MEM_mallocN(sizeof(int) * (me->totpoly + 1), "vertexpaint");
}

unsigned int vpaint_get_current_col(VPaint *vp)
{
	Brush *brush = paint_brush(&vp->paint);
	unsigned char col[4];
	rgb_float_to_uchar(col, brush->rgb);
	col[3] = 255; /* alpha isn't used, could even be removed to speedup paint a little */
	return *(unsigned int *)col;
}

static void do_shared_vertex_tesscol(Mesh *me)
{
	/* if no mcol: do not do */
	/* if tface: only the involved faces, otherwise all */
	MFace *mface;
	MTFace *tface;
	int a;
	short *scolmain, *scol;
	char *mcol;
	
	if (me->mcol == NULL || me->totvert == 0 || me->totface == 0) return;
	
	scolmain = MEM_callocN(4 * sizeof(short) * me->totvert, "colmain");
	
	tface = me->mtface;
	mface = me->mface;
	mcol = (char *)me->mcol;
	for (a = me->totface; a > 0; a--, mface++, mcol += 16) {
		if ((tface && tface->mode & TF_SHAREDCOL) || (me->editflag & ME_EDIT_PAINT_MASK) == 0) {
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
		if (tface) tface++;
	}
	
	a = me->totvert;
	scol = scolmain;
	while (a--) {
		if (scol[0] > 1) {
			scol[1] /= scol[0];
			scol[2] /= scol[0];
			scol[3] /= scol[0];
		}
		scol += 4;
	}
	
	tface = me->mtface;
	mface = me->mface;
	mcol = (char *)me->mcol;
	for (a = me->totface; a > 0; a--, mface++, mcol += 16) {
		if ((tface && tface->mode & TF_SHAREDCOL) || (me->editflag & ME_EDIT_PAINT_MASK) == 0) {
			scol = scolmain + 4 * mface->v1;
			mcol[1] = scol[1]; mcol[2] = scol[2]; mcol[3] = scol[3];
			scol = scolmain + 4 * mface->v2;
			mcol[5] = scol[1]; mcol[6] = scol[2]; mcol[7] = scol[3];
			scol = scolmain + 4 * mface->v3;
			mcol[9] = scol[1]; mcol[10] = scol[2]; mcol[11] = scol[3];
			if (mface->v4) {
				scol = scolmain + 4 * mface->v4;
				mcol[13] = scol[1]; mcol[14] = scol[2]; mcol[15] = scol[3];
			}
		}
		if (tface) tface++;
	}

	MEM_freeN(scolmain);
}

void do_shared_vertexcol(Mesh *me, int do_tessface)
{
	MLoop *ml = me->mloop;
	MLoopCol *lcol = me->mloopcol;
	MTexPoly *mtp = me->mtpoly;
	MPoly *mp = me->mpoly;
	float (*scol)[5];
	int i, has_shared = 0;

	/* if no mloopcol: do not do */
	/* if mtexpoly: only the involved faces, otherwise all */

	if (me->mloopcol == 0 || me->totvert == 0 || me->totpoly == 0) return;

	scol = MEM_callocN(sizeof(float) * me->totvert * 5, "scol");

	for (i = 0; i < me->totloop; i++, ml++, lcol++) {
		if (i >= mp->loopstart + mp->totloop) {
			mp++;
			if (mtp) mtp++;
		}

		if (!(mtp && (mtp->mode & TF_SHAREDCOL)) && (me->editflag & ME_EDIT_PAINT_MASK) != 0)
			continue;

		scol[ml->v][0] += lcol->r;
		scol[ml->v][1] += lcol->g;
		scol[ml->v][2] += lcol->b;
		scol[ml->v][3] += lcol->a;
		scol[ml->v][4] += 1.0;
		has_shared = 1;
	}
	
	if (has_shared) {
		for (i = 0; i < me->totvert; i++) {
			if (!scol[i][4]) continue;

			scol[i][0] /= scol[i][4];
			scol[i][1] /= scol[i][4];
			scol[i][2] /= scol[i][4];
			scol[i][3] /= scol[i][4];
		}
	
		ml = me->mloop;
		lcol = me->mloopcol;
		for (i = 0; i < me->totloop; i++, ml++, lcol++) {
			if (!scol[ml->v][4]) continue;

			lcol->r = scol[ml->v][0];
			lcol->g = scol[ml->v][1];
			lcol->b = scol[ml->v][2];
			lcol->a = scol[ml->v][3];
		}
	}

	MEM_freeN(scol);

	if (has_shared && do_tessface) {
		do_shared_vertex_tesscol(me);
	}
}

static void make_vertexcol(Object *ob)	/* single ob */
{
	Mesh *me;
	if (!ob || ob->id.lib) return;
	me = get_mesh(ob);
	if (me == NULL) return;
	if (me->edit_btmesh) return;

	/* copies from shadedisplist to mcol */
	if (!me->mloopcol) {
		if (!me->mcol) {
			CustomData_add_layer(&me->fdata, CD_MCOL, CD_DEFAULT, NULL, me->totface);
		}
		if (!me->mloopcol) {
			CustomData_add_layer(&me->ldata, CD_MLOOPCOL, CD_DEFAULT, NULL, me->totloop);	
		}
		mesh_update_customdata_pointers(me, TRUE);
	}

	if (vertex_paint_use_tessface_check(ob)) {
		/* assume if these exist, that they are up to date & valid */
		if (!me->mcol || !me->mface) {
			/* should always be true */
			if (me->mcol) {
				memset(me->mcol, 255, 4 * sizeof(MCol) * me->totface);
			}

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

	//if(shade)
	//	shadeMeshMCol(scene, ob, me);
	//else
	
	DAG_id_tag_update(&me->id, 0);
	
}

/* mirror_vgroup is set to -1 when invalid */
static int wpaint_mirror_vgroup_ensure(Object *ob, const int vgroup_active)
{
	bDeformGroup *defgroup = BLI_findlink(&ob->defbase, vgroup_active);

	if (defgroup) {
		bDeformGroup *curdef;
		int mirrdef;
		char name[MAXBONENAME];

		flip_side_name(name, defgroup->name, FALSE);

		if (strcmp(name, defgroup->name) != 0) {
			for (curdef = ob->defbase.first, mirrdef = 0; curdef; curdef = curdef->next, mirrdef++) {
				if (!strcmp(curdef->name, name)) {
					break;
				}
			}

			if (curdef == NULL) {
				int olddef = ob->actdef;	/* tsk, ED_vgroup_add sets the active defgroup */
				curdef = ED_vgroup_add_name(ob, name);
				ob->actdef = olddef;
			}

			/* curdef should never be NULL unless this is
			 * a  lamp and ED_vgroup_add_name fails */
			if (curdef) {
				return mirrdef;
			}
		}
	}

	return -1;
}

static void copy_vpaint_prev(VPaint *vp, unsigned int *lcol, int tot)
{
	if (vp->vpaint_prev) {
		MEM_freeN(vp->vpaint_prev);
		vp->vpaint_prev = NULL;
	}
	vp->tot = tot;
	
	if (lcol == NULL || tot == 0) return;
	
	vp->vpaint_prev = MEM_mallocN(sizeof(int) * tot, "vpaint_prev");
	memcpy(vp->vpaint_prev, lcol, sizeof(int) * tot);
	
}

static void copy_wpaint_prev (VPaint *wp, MDeformVert *dverts, int dcount)
{
	if (wp->wpaint_prev) {
		free_dverts(wp->wpaint_prev, wp->tot);
		wp->wpaint_prev = NULL;
	}
	
	if (dverts && dcount) {
		
		wp->wpaint_prev = MEM_mallocN (sizeof(MDeformVert) * dcount, "wpaint prev");
		wp->tot = dcount;
		copy_dverts (wp->wpaint_prev, dverts, dcount);
	}
}


void vpaint_fill(Object *ob, unsigned int paintcol)
{
	Mesh *me;
	MPoly *mp;
	MLoopCol *lcol;
	int i, j, selected;

	me = get_mesh(ob);
	if (me == NULL || me->totpoly == 0) return;

	if (!me->mloopcol) make_vertexcol(ob);
	if (!me->mloopcol) return; /* possible we can't make mcol's */


	selected = (me->editflag & ME_EDIT_PAINT_MASK);

	mp = me->mpoly;
	for (i = 0; i < me->totpoly; i++, mp++) {
		if (!(!selected || mp->flag & ME_FACE_SEL))
			continue;

		lcol = me->mloopcol + mp->loopstart;
		for (j = 0; j < mp->totloop; j++, lcol++) {
			*(int *)lcol = paintcol;
		}
	}
	
	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	DAG_id_tag_update(&me->id, 0);
}


/* fills in the selected faces with the current weight and vertex group */
void wpaint_fill(VPaint *wp, Object *ob, float paintweight)
{
	Mesh *me = ob->data;
	MPoly *mf;
	MDeformWeight *dw, *dw_prev;
	int vgroup_active, vgroup_mirror = -1;
	unsigned int index;

	/* mutually exclusive, could be made into a */
	const short paint_selmode = ME_EDIT_PAINT_SEL_MODE(me);

	if (me->totpoly == 0 || me->dvert == NULL || !me->mpoly) return;
	
	vgroup_active = ob->actdef - 1;

	/* if mirror painting, find the other group */
	if (me->editflag & ME_EDIT_MIRROR_X) {
		vgroup_mirror = wpaint_mirror_vgroup_ensure(ob, vgroup_active);
	}
	
	copy_wpaint_prev(wp, me->dvert, me->totvert);
	
	for (index = 0, mf = me->mpoly; index < me->totpoly; index++, mf++) {
		unsigned int fidx = mf->totloop - 1;

		if ((paint_selmode == SCE_SELECT_FACE) && !(mf->flag & ME_FACE_SEL)) {
			continue;
		}

		do {
			unsigned int vidx = me->mloop[mf->loopstart + fidx].v;

			if (!me->dvert[vidx].flag) {
				if ((paint_selmode == SCE_SELECT_VERTEX) && !(me->mvert[vidx].flag & SELECT)) {
					continue;
				}

				dw = defvert_verify_index(&me->dvert[vidx], vgroup_active);
				if (dw) {
					dw_prev = defvert_verify_index(wp->wpaint_prev + vidx, vgroup_active);
					dw_prev->weight = dw->weight; /* set the undo weight */
					dw->weight = paintweight;

					if (me->editflag & ME_EDIT_MIRROR_X) {	/* x mirror painting */
						int j = mesh_get_x_mirror_vert(ob, vidx);
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
	me = get_mesh(ob);

	if (!(ob->mode & OB_MODE_VERTEX_PAINT)) return;
	if (me == 0 || me->mcol == 0 || me->totface == 0) return;

	igam = 1.0 / vp->gamma;
	for (a = 0; a < 256; a++) {

		fac = ((float)a) / 255.0;
		fac = vp->mul * pow( fac, igam);

		temp = 255.9 * fac;

		if (temp <= 0) gamtab[a] = 0;
		else if (temp >= 255) gamtab[a] = 255;
		else gamtab[a] = temp;
	}

	a = 4 * me->totface;
	cp = (unsigned char *)me->mcol;
	while (a--) {

		cp[1] = gamtab[ cp[1] ];
		cp[2] = gamtab[ cp[2] ];
		cp[3] = gamtab[ cp[3] ];

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

	cp[0] = (mfac * cp1[0] + fac * cp2[0]) / 255;
	cp[1] = (mfac * cp1[1] + fac * cp2[1]) / 255;
	cp[2] = (mfac * cp1[2] + fac * cp2[2]) / 255;
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

	temp = cp1[0] + ((fac * cp2[0]) / 255);
	cp[0] = (temp > 254) ? 255 : temp;
	temp = cp1[1] + ((fac * cp2[1]) / 255);
	cp[1] = (temp > 254) ? 255 : temp;
	temp = cp1[2] + ((fac * cp2[2]) / 255);
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

	temp = cp1[0] - ((fac * cp2[0]) / 255);
	cp1[0] = (temp < 0) ? 0 : temp;
	temp = cp1[1] - ((fac * cp2[1]) / 255);
	cp1[1] = (temp < 0) ? 0 : temp;
	temp = cp1[2] - ((fac * cp2[2]) / 255);
	cp1[2] = (temp < 0) ? 0 : temp;
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
	cp[0] = (mfac * cp1[0] + fac * ((cp2[0] * cp1[0]) / 255)) / 255;
	cp[1] = (mfac * cp1[1] + fac * ((cp2[1] * cp1[1]) / 255)) / 255;
	cp[2] = (mfac * cp1[2] + fac * ((cp2[2] * cp1[2]) / 255)) / 255;
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

	cp[0] = (mfac * cp1[0] + fac * cp2[0]) / 255;
	cp[1] = (mfac * cp1[1] + fac * cp2[1]) / 255;
	cp[2] = (mfac * cp1[2] + fac * cp2[2]) / 255;
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

	cp[0] = (mfac * cp1[0] + fac * cp2[0]) / 255;
	cp[1] = (mfac * cp1[1] + fac * cp2[1]) / 255;
	cp[2] = (mfac * cp1[2] + fac * cp2[2]) / 255;
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
	Brush *brush = paint_brush(&vp->paint);
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
			if ( ct[a] < co[a] ) {
				if ( cp[a] < ct[a] ) cp[a] = ct[a];
				else if ( cp[a] > co[a] ) cp[a] = co[a];
			}
			else {
				if ( cp[a] < co[a] ) cp[a] = co[a];
				else if ( cp[a] > ct[a] ) cp[a] = ct[a];
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
	/*if(size > 64.0) size = 64.0;*/
	
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
static float calc_vp_strength_dl(VPaint *vp, ViewContext *vc, const float *vert_nor,
                              const float mval[2], const float brush_size_pressure)
{
	Brush *brush = paint_brush(&vp->paint);
	float dist_squared;
	float vertco[2], delta[2];

	project_float_noclip(vc->ar, vert_nor, vertco);
	sub_v2_v2v2(delta, mval, vertco);
	dist_squared = dot_v2v2(delta, delta); /* len squared */
	if (dist_squared > brush_size_pressure * brush_size_pressure) {
		return 0.0f;
	}
	else {
		const float dist = sqrtf(dist_squared);
		return brush_curve_strength_clamp(brush, dist, brush_size_pressure);
	}
}

static float calc_vp_alpha_dl(VPaint *vp, ViewContext *vc,
                              float vpimat[][3], const float *vert_nor,
                              const float mval[2],
                              const float brush_size_pressure, const float brush_alpha_pressure)
{
	float strength = calc_vp_strength_dl(vp, vc, vert_nor, mval, brush_size_pressure);

	if (strength > 0.0f) {
		float alpha = brush_alpha_pressure * strength;

		if (vp->flag & VP_NORMALS) {
			float dvec[3];
			const float *no = vert_nor + 3;

			/* transpose ! */
			dvec[2] = dot_v3v3(vpimat[2], no);
			if (dvec[2] > 0.0f) {
				dvec[0] = dot_v3v3(vpimat[0], no);
				dvec[1] = dot_v3v3(vpimat[1], no);

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
	return (paintval * alpha) + (weight * (1.0f - alpha));
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
{	/* first mul, then blend the fac */
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
	Brush *brush = paint_brush(&wp->paint);
	int tool = brush->vertexpaint_tool;

	if (do_flip) {
		switch(tool) {
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
static int weight_sample_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewContext vc;
	Mesh *me;
	short change = FALSE;

	view3d_set_viewcontext(C, &vc);
	me = get_mesh(vc.obact);

	if (me && me->dvert && vc.v3d && vc.rv3d) {
		int index;

		view3d_operator_needs_opengl(C);

		index = view3d_sample_backbuf(&vc, event->mval[0], event->mval[1]);

		if (index && index <= me->totpoly) {
			DerivedMesh *dm = mesh_get_derived_final(vc.scene, vc.obact, CD_MASK_BAREMESH);

			if (dm->getVertCo == NULL) {
				BKE_report(op->reports, RPT_WARNING, "The modifier used does not support deformed locations");
			}
			else {
				MPoly *mf = ((MPoly *)me->mpoly) + index - 1;
				const int vgroup_active = vc.obact->actdef - 1;
				ToolSettings *ts = vc.scene->toolsettings;
				float mval_f[2];
				int v_idx_best = -1;
				int fidx;
				float len_best = FLT_MAX;

				mval_f[0] = (float)event->mval[0];
				mval_f[1] = (float)event->mval[1];

				fidx = mf->totloop - 1;
				do {
					float co[3], sco[3], len;
					const int v_idx = me->mloop[mf->loopstart + fidx].v;
					dm->getVertCo(dm, v_idx, co);
					project_float_noclip(vc.ar, co, sco);
					len = len_squared_v2v2(mval_f, sco);
					if (len < len_best) {
						len_best = len;
						v_idx_best = v_idx;
					}
				} while (fidx--);

				if (v_idx_best != -1) { /* should always be valid */
					ts->vgroup_weight = defvert_find_weight(&me->dvert[v_idx_best], vgroup_active);
					change = TRUE;
				}
			}
			dm->release(dm);
		}
	}

	if (change) {
		/* not really correct since the brush didnt change, but redraws the toolbar */
		WM_main_add_notifier(NC_BRUSH|NA_EDITED, NULL); /* ts->wpaint->paint.brush */

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

	/* api callbacks */
	ot->invoke = weight_sample_invoke;
	ot->poll = weight_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/* samples cursor location, and gives menu with vertex groups to activate */
static EnumPropertyItem *weight_paint_sample_enum_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), int *free)
{
	if (C) {
		wmWindow *win = CTX_wm_window(C);
		if (win && win->eventstate) {
			ViewContext vc;
			Mesh *me;

			view3d_set_viewcontext(C, &vc);
			me = get_mesh(vc.obact);

			if (me && me->dvert && vc.v3d && vc.rv3d) {
				int index;

				view3d_operator_needs_opengl(C);

				index = view3d_sample_backbuf(&vc, win->eventstate->x - vc.ar->winrct.xmin, win->eventstate->y - vc.ar->winrct.ymin);

				if (index && index <= me->totpoly) {
					const int defbase_tot = BLI_countlist(&vc.obact->defbase);
					if (defbase_tot) {
						MPoly *mf = ((MPoly *)me->mpoly) + index - 1;
						unsigned int fidx = mf->totloop - 1;
						int *groups = MEM_callocN(defbase_tot * sizeof(int), "groups");
						int found = FALSE;

						do {
							MDeformVert *dvert = me->dvert + me->mloop[mf->loopstart + fidx].v;
							int i = dvert->totweight;
							MDeformWeight *dw;
							for (dw = dvert->dw; i > 0; dw++, i--) {
								if (dw->def_nr < defbase_tot) {
									groups[dw->def_nr] = TRUE;
									found = TRUE;
								}
							}
						} while (fidx--);

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
							*free = 1;

							MEM_freeN(groups);
							return item;
						}
					}
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
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, vc.obact);
	return OPERATOR_FINISHED;
}

/* TODO, we could make this a menu into OBJECT_OT_vertex_group_set_active rather than its own operator */
void PAINT_OT_weight_sample_group(wmOperatorType *ot)
{
	PropertyRNA *prop = NULL;

	/* identifiers */
	ot->name = "Weight Paint Sample Group";
	ot->idname = "PAINT_OT_weight_sample_group";

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

static void do_weight_paint_normalize_all(MDeformVert *dvert, const int defbase_tot, const char *vgroup_validmap)
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
static void do_weight_paint_normalize_all_active(MDeformVert *dvert, const int defbase_tot, const char *vgroup_validmap,
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
static char has_locked_group(MDeformVert *dvert, const int defbase_tot,
                             const char *bone_groups, const char *lock_flags)
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
/* 
 * gen_lck_flags gets the status of "flag" for each bDeformGroup
 *in ob->defbase and returns an array containing them
 */
static char *gen_lock_flags(Object *ob, int defbase_tot)
{
	char is_locked = FALSE;
	int i;
	//int defbase_tot = BLI_countlist(&ob->defbase);
	char *lock_flags = MEM_mallocN(defbase_tot * sizeof(char), "defflags");
	bDeformGroup *defgroup;

	for (i = 0, defgroup = ob->defbase.first; i < defbase_tot && defgroup; defgroup = defgroup->next, i++) {
		lock_flags[i] = ((defgroup->flag & DG_LOCK_WEIGHT) != 0);
		is_locked |= lock_flags[i];
	}
	if (is_locked) {
		return lock_flags;
	}

	MEM_freeN(lock_flags);
	return NULL;
}

static int has_locked_group_selected(int defbase_tot, const char *defbase_sel, const char *lock_flags)
{
	int i;
	for (i = 0; i < defbase_tot; i++) {
		if (defbase_sel[i] && lock_flags[i]) {
			return TRUE;
		}
	}
	return FALSE;
}


#if 0 /* UNUSED */
static int has_unselected_unlocked_bone_group(int defbase_tot, char *defbase_sel, int selected, char *lock_flags, char *vgroup_validmap)
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


static void multipaint_selection(MDeformVert *dvert, const int defbase_tot, float change, const char *defbase_sel)
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
	float was_change;
	float change;
	float oldval;
	MDeformWeight *ndw;
	int i;
	do {
		/* assume there is no change until you see one */
		was_change = FALSE;
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
					else {/* a perfectly valid change occurred to ndw->weight */
						totchange -= change;
						ndw->weight += change;
					}
					/* see if there was a change */
					if (oldval != ndw->weight) {
						was_change = TRUE;
					}
				}
			}
		}
		/* don't go again if there was no change, if there is no valid group,
		 * or there is no change left */
	} while (was_change && total_valid && totchange);
	/* left overs */
	return totchange;
}
static float get_mp_change(MDeformVert *odv, const int defbase_tot, const char *defbase_sel, float brush_change);
/* observe the changes made to the weights of groups.
 * make sure all locked groups on the vertex have the same deformation
 * by moving the changes made to groups onto other unlocked groups */
static void enforce_locks(MDeformVert *odv, MDeformVert *ndv,
                          const int defbase_tot, const char *defbase_sel,
                          const char *lock_flags, const char *vgroup_validmap,
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

	float changed_sum = 0.0f;

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
			changed_sum += ndw->weight;
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
			unsigned int i;
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
static float get_mp_change(MDeformVert *odv, const int defbase_tot, const char *defbase_sel, float brush_change)
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

	const char *lock_flags;  /* boolean array for locked bones,
	                          * length of defbase_tot */
	const char *defbase_sel; /* boolean array for selected bones,
	                          * length of defbase_tot, cant be const because of how its passed */

	const char *vgroup_validmap; /* same as WeightPaintData.vgroup_validmap,
	                              * only added here for convenience */

	char do_flip;
	char do_multipaint;
	char do_auto_normalize;

	float brush_alpha_value;  /* result of brush_alpha() */
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
	if ( (wpi->lock_flags == NULL) ||
	     ((wpi->lock_flags[dw->def_nr] == FALSE) && /* def_nr range has to be checked for by caller */
	      has_locked_group_selected(wpi->defbase_tot, wpi->defbase_sel, wpi->lock_flags) == FALSE))
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
static int get_first_selected_nonzero_weight(MDeformVert *dvert, const int defbase_tot, const char *defbase_sel)
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


static char *wpaint_make_validmap(Object *ob);


static void do_weight_paint_vertex(/* vars which remain the same for every vert */
                                   VPaint *wp, Object *ob, const WeightPaintInfo *wpi,
                                   /* vars which change on each stroke */
                                   const unsigned int index, float alpha, float paintweight
                                   )
{
	Mesh *me = ob->data;
	MDeformVert *dv = &me->dvert[index];
	
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
		index_mirr = mesh_get_x_mirror_vert(ob, index);
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
	if ( (do_multipaint_totsel == FALSE) &&
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
					 * While it wont hit the desired weight immediatelty as the user waggles their mouse,
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
		float testw = 0;
		float change = 0;
		float oldChange = 0;
		int i;
		MDeformWeight *tdw = NULL, *tdw_prev;
		MDeformVert dv_copy = {NULL};

		oldw = dw->weight;
		neww = wpaint_blend(wp, dw->weight, dw_prev->weight, alpha, paintweight,
		                    wpi->brush_alpha_value, wpi->do_flip, do_multipaint_totsel);
		
		/* setup multi-paint */
		if (do_multipaint_totsel) {
			dv_copy.dw = MEM_dupallocN(dv->dw);
			dv_copy.flag = dv->flag;
			dv_copy.totweight = dv->totweight;
			tdw = dw;
			tdw_prev = dw_prev;
			change = get_mp_change(&wp->wpaint_prev[index], wpi->defbase_tot, wpi->defbase_sel, neww - oldw);
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
						testw = tdw_prev->weight * change;
						if ( testw > tdw_prev->weight ) {
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
		dw = NULL; /* UNUSED after assignment, set to NULL to ensuyre we don't
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

static int set_wpaint(bContext *C, wmOperator *UNUSED(op))		/* toggle */
{		
	Object *ob = CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);
	VPaint *wp = scene->toolsettings->wpaint;
	Mesh *me;
	
	me = get_mesh(ob);
	if (ob->id.lib || me == NULL) return OPERATOR_PASS_THROUGH;
	
	if (ob->mode & OB_MODE_WEIGHT_PAINT) ob->mode &= ~OB_MODE_WEIGHT_PAINT;
	else ob->mode |= OB_MODE_WEIGHT_PAINT;
	
	
	/* Weightpaint works by overriding colors in mesh,
	 * so need to make sure we recalc on enter and
	 * exit (exit needs doing regardless because we
	 * should redeform).
	 */
	DAG_id_tag_update(&me->id, 0);
	
	if (ob->mode & OB_MODE_WEIGHT_PAINT) {
		Object *par;
		
		if (wp == NULL)
			wp = scene->toolsettings->wpaint = new_vpaint(1);

		paint_init(&wp->paint, PAINT_CURSOR_WEIGHT_PAINT);
		paint_cursor_start(C, weight_paint_poll);
		
		mesh_octree_table(ob, NULL, NULL, 's');
		
		/* verify if active weight group is also active bone */
		par = modifiers_isDeformedByArmature(ob);
		if (par && (par->mode & OB_MODE_POSE)) {
			bArmature *arm = par->data;

			if (arm->act_bone)
				ED_vgroup_select_by_name(ob, arm->act_bone->name);
		}
	}
	else {
		mesh_octree_table(NULL, NULL, NULL, 'e');
		mesh_mirrtopo_table(NULL, 'e');
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

/* for switching to/from mode */
static int paint_poll_test(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if (CTX_data_edit_object(C))
		return 0;
	if (CTX_data_active_object(C) == NULL)
		return 0;
	if (!ob->data || ((ID *)ob->data)->lib)
		return 0;
	return 1;
}

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Weight Paint Mode";
	ot->idname = "PAINT_OT_weight_paint_toggle";
	
	/* api callbacks */
	ot->exec = set_wpaint;
	ot->poll = paint_poll_test;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

/* ************ weight paint operator ********** */

struct WPaintData {
	ViewContext vc;
	int *indexar;
	int vgroup_active;
	int vgroup_mirror;
	float *vertexcosnos;
	float wpimat[3][3];
	
	/* variables for auto normalize */
	const char *vgroup_validmap; /* stores if vgroups tie to deforming bones or not */
	const char *lock_flags;
	int defbase_tot;
};

static char *wpaint_make_validmap(Object *ob)
{
	bDeformGroup *dg;
	ModifierData *md;
	char *vgroup_validmap;
	GHash *gh;
	int i, step1 = 1;

	if (ob->defbase.first == NULL) {
		return NULL;
	}

	gh = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "wpaint_make_validmap gh");

	/* add all names to a hash table */
	for (dg = ob->defbase.first; dg; dg = dg->next) {
		BLI_ghash_insert(gh, dg->name, NULL);
	}

	/* now loop through the armature modifiers and identify deform bones */
	for (md = ob->modifiers.first; md; md = !md->next && step1 ? (step1 = 0), modifiers_getVirtualModifierList(ob) : md->next) {
		if (!(md->mode & (eModifierMode_Realtime|eModifierMode_Virtual)))
			continue;

		if (md->type == eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData *) md;

			if (amd->object && amd->object->pose) {
				bPose *pose = amd->object->pose;
				bPoseChannel *chan;
				
				for (chan = pose->chanbase.first; chan; chan = chan->next) {
					if (chan->bone->flag & BONE_NO_DEFORM)
						continue;

					if (BLI_ghash_haskey(gh, chan->name)) {
						BLI_ghash_remove(gh, chan->name, NULL, NULL);
						BLI_ghash_insert(gh, chan->name, SET_INT_IN_POINTER(1));
					}
				}
			}
		}
	}

	vgroup_validmap = MEM_mallocN(BLI_ghash_size(gh), "wpaint valid map");

	/* add all names to a hash table */
	for (dg = ob->defbase.first, i = 0; dg; dg = dg->next, i++) {
		vgroup_validmap[i] = (BLI_ghash_lookup(gh, dg->name) != NULL);
	}

	BLI_assert(i == BLI_ghash_size(gh));

	BLI_ghash_free(gh, NULL, NULL);

	return vgroup_validmap;
}

static int wpaint_stroke_test_start(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	struct PaintStroke *stroke = op->customdata;
	ToolSettings *ts = scene->toolsettings;
	VPaint *wp = ts->wpaint;
	Object *ob = CTX_data_active_object(C);
	struct WPaintData *wpd;
	Mesh *me;
	bDeformGroup *dg;

	float mat[4][4], imat[4][4];
	
	if (scene->obedit) {
		return FALSE;
	}
	
	me = get_mesh(ob);
	if (me == NULL || me->totpoly == 0) return OPERATOR_PASS_THROUGH;
	
	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert) {
		ED_vgroup_data_create(&me->id);
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);
	}

	/* this happens on a Bone select, when no vgroup existed yet */
	if (ob->actdef <= 0) {
		Object *modob;
		if ((modob = modifiers_isDeformedByArmature(ob))) {
			Bone *actbone = ((bArmature *)modob->data)->act_bone;
			if (actbone) {
				bPoseChannel *pchan = get_pose_channel(modob->pose, actbone->name);

				if (pchan) {
					bDeformGroup *dg = defgroup_find_name(ob, pchan->name);
					if (dg == NULL) {
						dg = ED_vgroup_add_name(ob, pchan->name);	/* sets actdef */
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

	/* check if we are attempting to paint onto a locked vertex group,
	 * and other options disallow it from doing anything useful */
	dg = BLI_findlink(&ob->defbase, (ob->actdef - 1));
	if (dg->flag & DG_LOCK_WEIGHT) {
		BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
		return FALSE;
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
	wpd->lock_flags = gen_lock_flags(ob, wpd->defbase_tot);
	if (ts->auto_normalize || ts->multipaint || wpd->lock_flags) {
		wpd->vgroup_validmap = wpaint_make_validmap(ob);
	}

	/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	wpd->vertexcosnos = mesh_get_mapped_verts_nors(scene, ob);
	wpd->indexar = get_indexarray(me);
	copy_wpaint_prev(wp, me->dvert, me->totvert);

	/* imat for normals */
	mult_m4_m4m4(mat, wpd->vc.rv3d->viewmat, ob->obmat);
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
	Brush *brush = paint_brush(&wp->paint);
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
	int use_vert_sel;
	char *defbase_sel;

	const float pressure = RNA_float_get(itemptr, "pressure");
	const float brush_size_pressure = brush_size(scene, brush) * (brush_use_size_pressure(scene, brush) ? pressure : 1.0f);
	const float brush_alpha_value = brush_alpha(scene, brush);
	const float brush_alpha_pressure = brush_alpha_value * (brush_use_alpha_pressure(scene, brush) ? pressure : 1.0f);

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
		
	/* load projection matrix */
	mult_m4_m4m4(mat, vc->rv3d->persmat, ob->obmat);

	RNA_float_get_array(itemptr, "mouse", mval);
	mval[0] -= vc->ar->winrct.xmin;
	mval[1] -= vc->ar->winrct.ymin;



	/* *** setup WeightPaintInfo - pass onto do_weight_paint_vertex *** */
	wpi.defbase_tot =        wpd->defbase_tot;
	defbase_sel =            MEM_mallocN(wpi.defbase_tot * sizeof(char), "wpi.defbase_sel");
	wpi.defbase_tot_sel =    get_selected_defgroups(ob, defbase_sel, wpi.defbase_tot);
	wpi.defbase_sel =        defbase_sel; /* so we can stay const */
	if (wpi.defbase_tot_sel == 0 && ob->actdef > 0) wpi.defbase_tot_sel = 1;

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

	use_vert_sel = (me->editflag & ME_EDIT_VERT_SEL) != 0;

	/* which faces are involved */
	if (wp->flag & VP_AREA) {
		/* Ugly hack, to avoid drawing vertex index when getting the face index buffer - campbell */
		me->editflag &= ~ME_EDIT_VERT_SEL;
		totindex = sample_backbuf_area(vc, indexar, me->totpoly, mval[0], mval[1], brush_size_pressure);
		me->editflag |= use_vert_sel ? ME_EDIT_VERT_SEL : 0;
	}
	else {
		indexar[0] = view3d_sample_backbuf(vc, mval[0], mval[1]);
		if (indexar[0]) totindex = 1;
		else totindex = 0;
	}
			
	if (wp->flag & VP_COLINDEX) {
		for (index = 0; index < totindex; index++) {
			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = ((MPoly *)me->mpoly) + (indexar[index] - 1);
						
				if (mpoly->mat_nr != ob->actcol - 1) {
					indexar[index] = 0;
				}
			}
		}
	}
			
	if ((me->editflag & ME_EDIT_PAINT_MASK) && me->mpoly) {
		for (index = 0; index < totindex; index++) {
			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = ((MPoly *)me->mpoly) + (indexar[index] - 1);
						
				if ((mpoly->flag & ME_FACE_SEL) == 0) {
					indexar[index] = 0;
				}
			}
		}
	}

	/* make sure each vertex gets treated only once */
	/* and calculate filter weight */
	totw = 0.0f;
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR)
		paintweight = 0.0f;
	else
		paintweight = ts->vgroup_weight;
			
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
				MDeformWeight *dw, *(*dw_func)(MDeformVert *, const int);
						
				if (wp->flag & VP_ONLYVGROUP)
					dw_func = (MDeformWeight *(*)(MDeformVert *, const int))defvert_find_index;
				else
					dw_func = defvert_verify_index;
						
				ml = me->mloop + mpoly->loopstart;
				for (i = 0; i < mpoly->totloop; i++, ml++) {
					unsigned int vidx = ml->v;
					const float fac = calc_vp_strength_dl(wp, vc, wpd->vertexcosnos + 6 * vidx, mval, brush_size_pressure);
					if (fac > 0.0f) {
						dw = dw_func(&me->dvert[vidx], wpi.vgroup_active);
						paintweight += dw ? (dw->weight * fac) : 0.0f;
						totw += fac;
					}
				}
			}
		}
	}
			
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		paintweight /= totw;
	}

	for (index = 0; index < totindex; index++) {

		if (indexar[index] && indexar[index] <= me->totpoly) {
			MPoly *mpoly = me->mpoly + (indexar[index] - 1);
			MLoop *ml = me->mloop + mpoly->loopstart;
			int i;

			for (i = 0; i < mpoly->totloop; i++, ml++) {
				unsigned int vidx = ml->v;

				if (me->dvert[vidx].flag) {
					alpha = calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos + 6 * vidx,
					                        mval, brush_size_pressure, brush_alpha_pressure);
					if (alpha) {
						do_weight_paint_vertex(wp, ob, &wpi, vidx, alpha, paintweight);
					}
					me->dvert[vidx].flag = 0;
				}
			}
		}
	}


	/* *** free wpi members */
	MEM_freeN((void *)wpi.defbase_sel);
	/* *** don't freeing wpi members */


	swap_m4m4(vc->rv3d->persmat, mat);
			
	DAG_id_tag_update(ob->data, 0);
	ED_region_tag_redraw(vc->ar);
}

static void wpaint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	struct WPaintData *wpd = paint_stroke_mode_data(stroke);
	
	if (wpd) {
		if (wpd->vertexcosnos)
			MEM_freeN(wpd->vertexcosnos);
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
}


static int wpaint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	op->customdata = paint_stroke_new(C, NULL, wpaint_stroke_test_start,
	                                  wpaint_stroke_update_step,
	                                  wpaint_stroke_done, event->type);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

static int wpaint_cancel(bContext *C, wmOperator *op)
{
	paint_stroke_cancel(C, op);

	return OPERATOR_CANCELLED;
}

void PAINT_OT_weight_paint(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Weight Paint";
	ot->idname = "PAINT_OT_weight_paint";
	
	/* api callbacks */
	ot->invoke = wpaint_invoke;
	ot->modal = paint_stroke_modal;
	/* ot->exec = vpaint_exec; <-- needs stroke property */
	ot->poll = weight_paint_poll;
	ot->cancel = wpaint_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

static int weight_paint_set_exec(bContext *C, wmOperator *UNUSED(op))
{
	struct Scene *scene = CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);

	wpaint_fill(scene->toolsettings->wpaint, obact, scene->toolsettings->vgroup_weight);
	ED_region_tag_redraw(CTX_wm_region(C)); /* XXX - should redraw all 3D views */
	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Weight";
	ot->idname = "PAINT_OT_weight_set";

	/* api callbacks */
	ot->exec = weight_paint_set_exec;
	ot->poll = mask_paint_poll; /* it was facemask_paint_poll */

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************ set / clear vertex paint mode ********** */


static int set_vpaint(bContext *C, wmOperator *op)		/* toggle */
{	
	Object *ob = CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);
	VPaint *vp = scene->toolsettings->vpaint;
	Mesh *me;
	
	me = get_mesh(ob);
	
	if (me == NULL || object_data_is_libdata(ob)) {
		ob->mode &= ~OB_MODE_VERTEX_PAINT;
		return OPERATOR_PASS_THROUGH;
	}
	
	if (me && me->mloopcol == NULL) {
		make_vertexcol(ob);
	}
	
	/* toggle: end vpaint */
	if (ob->mode & OB_MODE_VERTEX_PAINT) {
		
		ob->mode &= ~OB_MODE_VERTEX_PAINT;
	}
	else {
		ob->mode |= OB_MODE_VERTEX_PAINT;
		/* Turn off weight painting */
		if (ob->mode & OB_MODE_WEIGHT_PAINT)
			set_wpaint(C, op);
		
		if (vp == NULL)
			vp = scene->toolsettings->vpaint = new_vpaint(0);
		
		paint_cursor_start(C, vertex_paint_poll);

		paint_init(&vp->paint, PAINT_CURSOR_VERTEX_PAINT);
	}
	
	if (me)
		/* update modifier stack for mapping requirements */
		DAG_id_tag_update(&me->id, 0);
	
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Vertex Paint Mode";
	ot->idname = "PAINT_OT_vertex_paint_toggle";
	
	/* api callbacks */
	ot->exec = set_vpaint;
	ot->poll = paint_poll_test;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
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

typedef struct polyfacemap_e {
	struct polyfacemap_e *next, *prev;
	int facenr;
} polyfacemap_e;

typedef struct VPaintData {
	ViewContext vc;
	unsigned int paintcol;
	int *indexar;
	float *vertexcosnos;
	float vpimat[3][3];

	/* modify 'me->mcol' directly, since the derived mesh is drawing from this array,
	 * otherwise we need to refresh the modifier stack */
	int use_fast_update;

	/* mpoly -> mface mapping */
	MemArena *polyfacemap_arena;
	ListBase *polyfacemap;
} VPaintData;

static void vpaint_build_poly_facemap(struct VPaintData *vd, Mesh *me)
{
	MFace *mf;
	polyfacemap_e *e;
	int *origIndex;
	int i;

	vd->polyfacemap_arena = BLI_memarena_new(1 << 13, "vpaint tmp");
	BLI_memarena_use_calloc(vd->polyfacemap_arena);

	vd->polyfacemap = BLI_memarena_alloc(vd->polyfacemap_arena, sizeof(ListBase) * me->totpoly);

	origIndex = CustomData_get_layer(&me->fdata, CD_POLYINDEX);
	mf = me->mface;

	if (!origIndex)
		return;

	for (i = 0; i < me->totface; i++, mf++, origIndex++) {
		if (*origIndex == ORIGINDEX_NONE)
			continue;

		e = BLI_memarena_alloc(vd->polyfacemap_arena, sizeof(polyfacemap_e));
		e->facenr = i;
		
		BLI_addtail(&vd->polyfacemap[*origIndex], e);
	}
}

static int vpaint_stroke_test_start(bContext *C, struct wmOperator *op, wmEvent *UNUSED(event))
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct PaintStroke *stroke = op->customdata;
	VPaint *vp = ts->vpaint;
	struct VPaintData *vpd;
	Object *ob = CTX_data_active_object(C);
	Mesh *me;
	float mat[4][4], imat[4][4];

	/* context checks could be a poll() */
	me = get_mesh(ob);
	if (me == NULL || me->totpoly == 0)
		return OPERATOR_PASS_THROUGH;
	
	if (me->mloopcol == NULL)
		make_vertexcol(ob);
	if (me->mloopcol == NULL)
		return OPERATOR_CANCELLED;
	
	/* make mode data storage */
	vpd = MEM_callocN(sizeof(struct VPaintData), "VPaintData");
	paint_stroke_set_mode_data(stroke, vpd);
	view3d_set_viewcontext(C, &vpd->vc);
	
	vpd->vertexcosnos = mesh_get_mapped_verts_nors(vpd->vc.scene, ob);
	vpd->indexar = get_indexarray(me);
	vpd->paintcol = vpaint_get_current_col(vp);


	/* are we painting onto a modified mesh?,
	 * if not we can skip face map trickyness */
	if (vertex_paint_use_fast_update_check(ob)) {
		vpaint_build_poly_facemap(vpd, me);
		vpd->use_fast_update = TRUE;
	}
	else {
		vpd->use_fast_update = FALSE;
	}

	/* for filtering */
	copy_vpaint_prev(vp, (unsigned int *)me->mloopcol, me->totloop);
	
	/* some old cruft to sort out later */
	mult_m4_m4m4(mat, vpd->vc.rv3d->viewmat, ob->obmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(vpd->vpimat, imat);

	return 1;
}

#if 0
static void vpaint_paint_face(VPaint *vp, VPaintData *vpd, Object *ob,
                              const unsigned int index, const float mval[2],
                              const float brush_size_pressure, const float brush_alpha_pressure,
                              int UNUSED(flip))
{
	ViewContext *vc = &vpd->vc;
	Brush *brush = paint_brush(&vp->paint);
	Mesh *me = get_mesh(ob);
	MFace *mface = &me->mface[index];
	unsigned int *mcol = ((unsigned int *)me->mcol) + 4 * index;
	unsigned int *mcolorig = ((unsigned int *)vp->vpaint_prev) + 4 * index;
	float alpha;
	int i;

	int brush_alpha_pressure_i;
	
	if ((vp->flag & VP_COLINDEX && mface->mat_nr != ob->actcol - 1) ||
	   ((me->editflag & ME_EDIT_PAINT_MASK) && !(mface->flag & ME_FACE_SEL)))
		return;

	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		unsigned int fcol1 = mcol_blend( mcol[0], mcol[1], 128);
		if (mface->v4) {
			unsigned int fcol2 = mcol_blend( mcol[2], mcol[3], 128);
			vpd->paintcol = mcol_blend( fcol1, fcol2, 128);
		}
		else {
			vpd->paintcol = mcol_blend( mcol[2], fcol1, 170);
		}
	}

	brush_alpha_pressure_i = (int)(brush_alpha_pressure * 255.0f);

	for (i = 0; i < (mface->v4 ? 4 : 3); ++i) {
		alpha = calc_vp_alpha_dl(vp, vc, vpd->vpimat, vpd->vertexcosnos + 6 * (&mface->v1)[i],
		                         mval, brush_size_pressure, brush_alpha_pressure);
		if (alpha) {
			const int alpha_i = (int)(alpha * 255.0f);
			mcol[i] = vpaint_blend(vp, mcol[i], mcolorig[i], vpd->paintcol, alpha_i, brush_alpha_pressure_i);
		}
	}
}
#endif

/* BMESH version of vpaint_paint_face (commented above) */

static void vpaint_paint_poly(VPaint *vp, VPaintData *vpd, Object *ob,
                              const unsigned int index, const float mval[2],
                              const float brush_size_pressure, const float brush_alpha_pressure,
                              int UNUSED(flip)
                              )
{
	ViewContext *vc = &vpd->vc;
	Brush *brush = paint_brush(&vp->paint);
	Mesh *me = get_mesh(ob);
	MPoly *mpoly = &me->mpoly[index];
	MFace *mf;
	MCol *mc;
	MLoop *ml;
	MLoopCol *mlc;
	polyfacemap_e *e;
	unsigned int *lcol = ((unsigned int *)me->mloopcol) + mpoly->loopstart;
	unsigned int *lcolorig = ((unsigned int *)vp->vpaint_prev) + mpoly->loopstart;
	float alpha;
	int i, j;

	int brush_alpha_pressure_i = (int)(brush_alpha_pressure * 255.0f);

	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		unsigned int blend[4] = {0};
		unsigned int tcol;
		char *col;

		for (j = 0; j < mpoly->totloop; j++) {
			col = (char *)(lcol + j);
			blend[0] += col[0];
			blend[1] += col[1];
			blend[2] += col[2];
			blend[3] += col[3];
		}

		blend[0] /= mpoly->totloop;
		blend[1] /= mpoly->totloop;
		blend[2] /= mpoly->totloop;
		blend[3] /= mpoly->totloop;
		col = (char *)&tcol;
		col[0] = blend[0];
		col[1] = blend[1];
		col[2] = blend[2];
		col[3] = blend[3];

		vpd->paintcol = *((unsigned int *)col);
	}

	ml = me->mloop + mpoly->loopstart;
	for (i = 0; i < mpoly->totloop; i++, ml++) {
		alpha = calc_vp_alpha_dl(vp, vc, vpd->vpimat,
		                         vpd->vertexcosnos + 6 * ml->v, mval,
		                         brush_size_pressure, brush_alpha_pressure);
		if (alpha > 0.0f) {
			const int alpha_i = (int)(alpha * 255.0f);
			lcol[i] = vpaint_blend(vp, lcol[i], lcolorig[i], vpd->paintcol, alpha_i, brush_alpha_pressure_i);
		}
	}

	if (vpd->use_fast_update) {

#ifdef CPYCOL
#  undef CPYCOL
#endif
#define CPYCOL(c, l) (c)->a = (l)->a, (c)->r = (l)->r, (c)->g = (l)->g, (c)->b = (l)->b

		/* update vertex colors for tessellations incrementally,
		 * rather then regenerating the tessellation altogether */
		for (e = vpd->polyfacemap[index].first; e; e = e->next) {
			mf = me->mface + e->facenr;
			mc = me->mcol + e->facenr * 4;

			ml = me->mloop + mpoly->loopstart;
			mlc = me->mloopcol + mpoly->loopstart;
			for (j = 0; j < mpoly->totloop; j++, ml++, mlc++) {
				if (ml->v == mf->v1)
					CPYCOL(mc, mlc);
				else if (ml->v == mf->v2)
					CPYCOL(mc + 1, mlc);
				else if (ml->v == mf->v3)
					CPYCOL(mc + 2, mlc);
				else if (mf->v4 && ml->v == mf->v4)
					CPYCOL(mc + 3, mlc);

			}
		}
#undef CPYCOL
	}

}

static void vpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	VPaint *vp = ts->vpaint;
	Brush *brush = paint_brush(&vp->paint);
	ViewContext *vc = &vpd->vc;
	Object *ob = vc->obact;
	Mesh *me = ob->data;
	float mat[4][4];
	int *indexar = vpd->indexar;
	int totindex, index, flip;
	float mval[2];

	const float pressure = RNA_float_get(itemptr, "pressure");
	const float brush_size_pressure = brush_size(scene, brush) * (brush_use_size_pressure(scene, brush) ? pressure : 1.0f);
	const float brush_alpha_pressure = brush_alpha(scene, brush) * (brush_use_alpha_pressure(scene, brush) ? pressure : 1.0f);

	RNA_float_get_array(itemptr, "mouse", mval);
	flip = RNA_boolean_get(itemptr, "pen_flip");

	(void)flip; /* BMESH_TODO */

	view3d_operator_needs_opengl(C);
			
	/* load projection matrix */
	mult_m4_m4m4(mat, vc->rv3d->persmat, ob->obmat);

	mval[0] -= vc->ar->winrct.xmin;
	mval[1] -= vc->ar->winrct.ymin;

			
	/* which faces are involved */
	if (vp->flag & VP_AREA) {
		totindex = sample_backbuf_area(vc, indexar, me->totpoly, mval[0], mval[1], brush_size_pressure);
	}
	else {
		indexar[0] = view3d_sample_backbuf(vc, mval[0], mval[1]);
		if (indexar[0]) totindex = 1;
		else totindex = 0;
	}
			
			
	if (vp->flag & VP_COLINDEX) {
		for (index = 0; index < totindex; index++) {
			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = ((MPoly *)me->mpoly) + (indexar[index] - 1);
						
				if (mpoly->mat_nr != ob->actcol - 1) {
					indexar[index] = 0;
				}
			}
		}
	}

	if ((me->editflag & ME_EDIT_PAINT_MASK) && me->mpoly) {
		for (index = 0; index < totindex; index++) {
			if (indexar[index] && indexar[index] <= me->totpoly) {
				MPoly *mpoly = ((MPoly *)me->mpoly) + (indexar[index] - 1);
						
				if ((mpoly->flag & ME_FACE_SEL) == 0)
					indexar[index] = 0;
			}					
		}
	}
	
	swap_m4m4(vc->rv3d->persmat, mat);

			
	for (index = 0; index < totindex; index++) {
				
		if (indexar[index] && indexar[index] <= me->totpoly) {
			vpaint_paint_poly(vp, vpd, ob, indexar[index] - 1, mval, brush_size_pressure, brush_alpha_pressure, flip);
		}
	}
		
	swap_m4m4(vc->rv3d->persmat, mat);

	/* was disabled because it is slow, but necessary for blur */
	if (brush->vertexpaint_tool == PAINT_BLEND_BLUR) {
		int do_tessface = vpd->use_fast_update;
		do_shared_vertexcol(me, do_tessface);
	}

	ED_region_tag_redraw(vc->ar);

	if (vpd->use_fast_update == FALSE) {
		/* recalculate modifier stack to get new colors, slow,
		 * avoid this if we can! */
		DAG_id_tag_update(ob->data, 0);
	}
}

static void vpaint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	struct VPaintData *vpd = paint_stroke_mode_data(stroke);
	
	if (vpd->vertexcosnos)
		MEM_freeN(vpd->vertexcosnos);
	MEM_freeN(vpd->indexar);
	
	/* frees prev buffer */
	copy_vpaint_prev(ts->vpaint, NULL, 0);

	if (vpd->polyfacemap_arena) {
		BLI_memarena_free(vpd->polyfacemap_arena);
	}

	MEM_freeN(vpd);
}

static int vpaint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	op->customdata = paint_stroke_new(C, NULL, vpaint_stroke_test_start,
					  vpaint_stroke_update_step,
					  vpaint_stroke_done, event->type);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

static int vpaint_cancel(bContext *C, wmOperator *op)
{
	paint_stroke_cancel(C, op);

	return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Paint";
	ot->idname = "PAINT_OT_vertex_paint";
	
	/* api callbacks */
	ot->invoke = vpaint_invoke;
	ot->modal = paint_stroke_modal;
	/* ot->exec = vpaint_exec; <-- needs stroke property */
	ot->poll = vertex_paint_poll;
	ot->cancel = vpaint_cancel;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

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
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_from_bones(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{ARM_GROUPS_AUTO, "AUTOMATIC", 0, "Automatic", "Automatic weights froms bones"},
		{ARM_GROUPS_ENVELOPE, "ENVELOPES", 0, "From Envelopes", "Weights from envelopes with user defined radius"},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Weight from Bones";
	ot->idname = "PAINT_OT_weight_from_bones";
	
	/* api callbacks */
	ot->exec = weight_from_bones_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = weight_from_bones_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "Method to use for assigning weights");
}
