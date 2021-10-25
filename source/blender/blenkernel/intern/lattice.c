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

/** \file blender/blenkernel/intern/lattice.c
 *  \ingroup bke
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_bitmap.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_lattice_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"

#include "BKE_animsys.h"
#include "BKE_anim.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BKE_deform.h"

/* Workaround for cyclic dependency with curves.
 * In such case curve_cache might not be ready yet,
 */
#define CYCLIC_DEPENDENCY_WORKAROUND

int BKE_lattice_index_from_uvw(Lattice *lt,
                               const int u, const int v, const int w)
{
	const int totu = lt->pntsu;
	const int totv = lt->pntsv;

	return (w * (totu * totv) + (v * totu) + u);
}

void BKE_lattice_index_to_uvw(Lattice *lt, const int index,
                              int *r_u, int *r_v, int *r_w)
{
	const int totu = lt->pntsu;
	const int totv = lt->pntsv;

	*r_u = (index % totu);
	*r_v = (index / totu) % totv;
	*r_w = (index / (totu * totv));
}

int BKE_lattice_index_flip(Lattice *lt, const int index,
                           const bool flip_u, const bool flip_v, const bool flip_w)
{
	int u, v, w;

	BKE_lattice_index_to_uvw(lt, index, &u, &v, &w);

	if (flip_u) {
		u = (lt->pntsu - 1) - u;
	}

	if (flip_v) {
		v = (lt->pntsv - 1) - v;
	}

	if (flip_w) {
		w = (lt->pntsw - 1) - w;
	}

	return BKE_lattice_index_from_uvw(lt, u, v, w);
}

void BKE_lattice_bitmap_from_flag(Lattice *lt, BLI_bitmap *bitmap, const short flag,
                                  const bool clear, const bool respecthide)
{
	const unsigned int tot = lt->pntsu * lt->pntsv * lt->pntsw;
	unsigned int i;
	BPoint *bp;

	bp = lt->def;
	for (i = 0; i < tot; i++, bp++) {
		if ((bp->f1 & flag) && (!respecthide || !bp->hide)) {
			BLI_BITMAP_ENABLE(bitmap, i);
		}
		else {
			if (clear) {
				BLI_BITMAP_DISABLE(bitmap, i);
			}
		}
	}

}

void calc_lat_fudu(int flag, int res, float *r_fu, float *r_du)
{
	if (res == 1) {
		*r_fu = 0.0;
		*r_du = 0.0;
	}
	else if (flag & LT_GRID) {
		*r_fu = -0.5f * (res - 1);
		*r_du = 1.0f;
	}
	else {
		*r_fu = -1.0f;
		*r_du = 2.0f / (res - 1);
	}
}

void BKE_lattice_resize(Lattice *lt, int uNew, int vNew, int wNew, Object *ltOb)
{
	BPoint *bp;
	int i, u, v, w;
	float fu, fv, fw, uc, vc, wc, du = 0.0, dv = 0.0, dw = 0.0;
	float *co, (*vertexCos)[3] = NULL;
	
	/* vertex weight groups are just freed all for now */
	if (lt->dvert) {
		BKE_defvert_array_free(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
		lt->dvert = NULL;
	}
	
	while (uNew * vNew * wNew > 32000) {
		if (uNew >= vNew && uNew >= wNew) uNew--;
		else if (vNew >= uNew && vNew >= wNew) vNew--;
		else wNew--;
	}

	vertexCos = MEM_mallocN(sizeof(*vertexCos) * uNew * vNew * wNew, "tmp_vcos");

	calc_lat_fudu(lt->flag, uNew, &fu, &du);
	calc_lat_fudu(lt->flag, vNew, &fv, &dv);
	calc_lat_fudu(lt->flag, wNew, &fw, &dw);

	/* If old size is different then resolution changed in interface,
	 * try to do clever reinit of points. Pretty simply idea, we just
	 * deform new verts by old lattice, but scaling them to match old
	 * size first.
	 */
	if (ltOb) {
		if (uNew != 1 && lt->pntsu != 1) {
			fu = lt->fu;
			du = (lt->pntsu - 1) * lt->du / (uNew - 1);
		}

		if (vNew != 1 && lt->pntsv != 1) {
			fv = lt->fv;
			dv = (lt->pntsv - 1) * lt->dv / (vNew - 1);
		}

		if (wNew != 1 && lt->pntsw != 1) {
			fw = lt->fw;
			dw = (lt->pntsw - 1) * lt->dw / (wNew - 1);
		}
	}

	co = vertexCos[0];
	for (w = 0, wc = fw; w < wNew; w++, wc += dw) {
		for (v = 0, vc = fv; v < vNew; v++, vc += dv) {
			for (u = 0, uc = fu; u < uNew; u++, co += 3, uc += du) {
				co[0] = uc;
				co[1] = vc;
				co[2] = wc;
			}
		}
	}
	
	if (ltOb) {
		float mat[4][4];
		int typeu = lt->typeu, typev = lt->typev, typew = lt->typew;

		/* works best if we force to linear type (endpoints match) */
		lt->typeu = lt->typev = lt->typew = KEY_LINEAR;

		/* prevent using deformed locations */
		BKE_displist_free(&ltOb->curve_cache->disp);

		copy_m4_m4(mat, ltOb->obmat);
		unit_m4(ltOb->obmat);
		lattice_deform_verts(ltOb, NULL, NULL, vertexCos, uNew * vNew * wNew, NULL, 1.0f);
		copy_m4_m4(ltOb->obmat, mat);

		lt->typeu = typeu;
		lt->typev = typev;
		lt->typew = typew;
	}

	lt->fu = fu;
	lt->fv = fv;
	lt->fw = fw;
	lt->du = du;
	lt->dv = dv;
	lt->dw = dw;

	lt->pntsu = uNew;
	lt->pntsv = vNew;
	lt->pntsw = wNew;

	lt->actbp = LT_ACTBP_NONE;
	MEM_freeN(lt->def);
	lt->def = MEM_callocN(lt->pntsu * lt->pntsv * lt->pntsw * sizeof(BPoint), "lattice bp");
	
	bp = lt->def;
	
	for (i = 0; i < lt->pntsu * lt->pntsv * lt->pntsw; i++, bp++) {
		copy_v3_v3(bp->vec, vertexCos[i]);
	}

	MEM_freeN(vertexCos);
}

void BKE_lattice_init(Lattice *lt)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(lt, id));

	lt->flag = LT_GRID;
	
	lt->typeu = lt->typev = lt->typew = KEY_BSPLINE;
	
	lt->def = MEM_callocN(sizeof(BPoint), "lattvert"); /* temporary */
	BKE_lattice_resize(lt, 2, 2, 2, NULL);  /* creates a uniform lattice */
	lt->actbp = LT_ACTBP_NONE;
}

Lattice *BKE_lattice_add(Main *bmain, const char *name)
{
	Lattice *lt;

	lt = BKE_libblock_alloc(bmain, ID_LT, name);

	BKE_lattice_init(lt);

	return lt;
}

Lattice *BKE_lattice_copy(Main *bmain, const Lattice *lt)
{
	Lattice *ltn;

	ltn = BKE_libblock_copy(bmain, &lt->id);
	ltn->def = MEM_dupallocN(lt->def);

	if (lt->key) {
		ltn->key = BKE_key_copy(bmain, ltn->key);
		ltn->key->from = (ID *)ltn;
	}
	
	if (lt->dvert) {
		int tot = lt->pntsu * lt->pntsv * lt->pntsw;
		ltn->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		BKE_defvert_array_copy(ltn->dvert, lt->dvert, tot);
	}

	ltn->editlatt = NULL;

	BKE_id_copy_ensure_local(bmain, &lt->id, &ltn->id);

	return ltn;
}

/** Free (or release) any data used by this lattice (does not free the lattice itself). */
void BKE_lattice_free(Lattice *lt)
{
	BKE_animdata_free(&lt->id, false);

	MEM_SAFE_FREE(lt->def);
	if (lt->dvert) {
		BKE_defvert_array_free(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
		lt->dvert = NULL;
	}
	if (lt->editlatt) {
		Lattice *editlt = lt->editlatt->latt;

		if (editlt->def)
			MEM_freeN(editlt->def);
		if (editlt->dvert)
			BKE_defvert_array_free(editlt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);

		MEM_freeN(editlt);
		MEM_freeN(lt->editlatt);
		lt->editlatt = NULL;
	}
}


void BKE_lattice_make_local(Main *bmain, Lattice *lt, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &lt->id, true, lib_local);
}

typedef struct LatticeDeformData {
	Object *object;
	float *latticedata;
	float latmat[4][4];
} LatticeDeformData;

LatticeDeformData *init_latt_deform(Object *oblatt, Object *ob)
{
	/* we make an array with all differences */
	Lattice *lt = oblatt->data;
	BPoint *bp;
	DispList *dl = oblatt->curve_cache ? BKE_displist_find(&oblatt->curve_cache->disp, DL_VERTS) : NULL;
	const float *co = dl ? dl->verts : NULL;
	float *fp, imat[4][4];
	float fu, fv, fw;
	int u, v, w;
	float *latticedata;
	float latmat[4][4];
	LatticeDeformData *lattice_deform_data;

	if (lt->editlatt) lt = lt->editlatt->latt;
	bp = lt->def;
	
	fp = latticedata = MEM_mallocN(sizeof(float) * 3 * lt->pntsu * lt->pntsv * lt->pntsw, "latticedata");
	
	/* for example with a particle system: (ob == NULL) */
	if (ob == NULL) {
		/* in deformspace, calc matrix  */
		invert_m4_m4(latmat, oblatt->obmat);
	
		/* back: put in deform array */
		invert_m4_m4(imat, latmat);
	}
	else {
		/* in deformspace, calc matrix */
		invert_m4_m4(imat, oblatt->obmat);
		mul_m4_m4m4(latmat, imat, ob->obmat);
	
		/* back: put in deform array */
		invert_m4_m4(imat, latmat);
	}
	
	for (w = 0, fw = lt->fw; w < lt->pntsw; w++, fw += lt->dw) {
		for (v = 0, fv = lt->fv; v < lt->pntsv; v++, fv += lt->dv) {
			for (u = 0, fu = lt->fu; u < lt->pntsu; u++, bp++, co += 3, fp += 3, fu += lt->du) {
				if (dl) {
					fp[0] = co[0] - fu;
					fp[1] = co[1] - fv;
					fp[2] = co[2] - fw;
				}
				else {
					fp[0] = bp->vec[0] - fu;
					fp[1] = bp->vec[1] - fv;
					fp[2] = bp->vec[2] - fw;
				}

				mul_mat3_m4_v3(imat, fp);
			}
		}
	}

	lattice_deform_data = MEM_mallocN(sizeof(LatticeDeformData), "Lattice Deform Data");
	lattice_deform_data->latticedata = latticedata;
	lattice_deform_data->object = oblatt;
	copy_m4_m4(lattice_deform_data->latmat, latmat);

	return lattice_deform_data;
}

void calc_latt_deform(LatticeDeformData *lattice_deform_data, float co[3], float weight)
{
	Object *ob = lattice_deform_data->object;
	Lattice *lt = ob->data;
	float u, v, w, tu[4], tv[4], tw[4];
	float vec[3];
	int idx_w, idx_v, idx_u;
	int ui, vi, wi, uu, vv, ww;

	/* vgroup influence */
	int defgrp_index = -1;
	float co_prev[3], weight_blend = 0.0f;
	MDeformVert *dvert = BKE_lattice_deform_verts_get(ob);


	if (lt->editlatt) lt = lt->editlatt->latt;
	if (lattice_deform_data->latticedata == NULL) return;

	if (lt->vgroup[0] && dvert) {
		defgrp_index = defgroup_name_index(ob, lt->vgroup);
		copy_v3_v3(co_prev, co);
	}

	/* co is in local coords, treat with latmat */
	mul_v3_m4v3(vec, lattice_deform_data->latmat, co);

	/* u v w coords */

	if (lt->pntsu > 1) {
		u = (vec[0] - lt->fu) / lt->du;
		ui = (int)floor(u);
		u -= ui;
		key_curve_position_weights(u, tu, lt->typeu);
	}
	else {
		tu[0] = tu[2] = tu[3] = 0.0; tu[1] = 1.0;
		ui = 0;
	}

	if (lt->pntsv > 1) {
		v = (vec[1] - lt->fv) / lt->dv;
		vi = (int)floor(v);
		v -= vi;
		key_curve_position_weights(v, tv, lt->typev);
	}
	else {
		tv[0] = tv[2] = tv[3] = 0.0; tv[1] = 1.0;
		vi = 0;
	}

	if (lt->pntsw > 1) {
		w = (vec[2] - lt->fw) / lt->dw;
		wi = (int)floor(w);
		w -= wi;
		key_curve_position_weights(w, tw, lt->typew);
	}
	else {
		tw[0] = tw[2] = tw[3] = 0.0; tw[1] = 1.0;
		wi = 0;
	}

	for (ww = wi - 1; ww <= wi + 2; ww++) {
		w = tw[ww - wi + 1];

		if (w != 0.0f) {
			if (ww > 0) {
				if (ww < lt->pntsw) idx_w = ww * lt->pntsu * lt->pntsv;
				else                idx_w = (lt->pntsw - 1) * lt->pntsu * lt->pntsv;
			}
			else {
				idx_w = 0;
			}

			for (vv = vi - 1; vv <= vi + 2; vv++) {
				v = w * tv[vv - vi + 1];

				if (v != 0.0f) {
					if (vv > 0) {
						if (vv < lt->pntsv) idx_v = idx_w + vv * lt->pntsu;
						else                idx_v = idx_w + (lt->pntsv - 1) * lt->pntsu;
					}
					else {
						idx_v = idx_w;
					}

					for (uu = ui - 1; uu <= ui + 2; uu++) {
						u = weight * v * tu[uu - ui + 1];

						if (u != 0.0f) {
							if (uu > 0) {
								if (uu < lt->pntsu) idx_u = idx_v + uu;
								else                idx_u = idx_v + (lt->pntsu - 1);
							}
							else {
								idx_u = idx_v;
							}

							madd_v3_v3fl(co, &lattice_deform_data->latticedata[idx_u * 3], u);

							if (defgrp_index != -1)
								weight_blend += (u * defvert_find_weight(dvert + idx_u, defgrp_index));
						}
					}
				}
			}
		}
	}

	if (defgrp_index != -1)
		interp_v3_v3v3(co, co_prev, co, weight_blend);

}

void end_latt_deform(LatticeDeformData *lattice_deform_data)
{
	if (lattice_deform_data->latticedata)
		MEM_freeN(lattice_deform_data->latticedata);

	MEM_freeN(lattice_deform_data);
}

/* calculations is in local space of deformed object
 * so we store in latmat transform from path coord inside object
 */
typedef struct {
	float dmin[3], dmax[3];
	float curvespace[4][4], objectspace[4][4], objectspace3[3][3];
	int no_rot_axis;
} CurveDeform;

static void init_curve_deform(Object *par, Object *ob, CurveDeform *cd)
{
	invert_m4_m4(ob->imat, ob->obmat);
	mul_m4_m4m4(cd->objectspace, ob->imat, par->obmat);
	invert_m4_m4(cd->curvespace, cd->objectspace);
	copy_m3_m4(cd->objectspace3, cd->objectspace);
	cd->no_rot_axis = 0;
}

/* this makes sure we can extend for non-cyclic.
 *
 * returns OK: 1/0
 */
static bool where_on_path_deform(Object *ob, float ctime, float vec[4], float dir[3], float quat[4], float *radius)
{
	BevList *bl;
	float ctime1;
	int cycl = 0;
	
	/* test for cyclic */
	bl = ob->curve_cache->bev.first;
	if (!bl->nr) return false;
	if (bl->poly > -1) cycl = 1;

	if (cycl == 0) {
		ctime1 = CLAMPIS(ctime, 0.0f, 1.0f);
	}
	else {
		ctime1 = ctime;
	}
	
	/* vec needs 4 items */
	if (where_on_path(ob, ctime1, vec, dir, quat, radius, NULL)) {
		
		if (cycl == 0) {
			Path *path = ob->curve_cache->path;
			float dvec[3];
			
			if (ctime < 0.0f) {
				sub_v3_v3v3(dvec, path->data[1].vec, path->data[0].vec);
				mul_v3_fl(dvec, ctime * (float)path->len);
				add_v3_v3(vec, dvec);
				if (quat) copy_qt_qt(quat, path->data[0].quat);
				if (radius) *radius = path->data[0].radius;
			}
			else if (ctime > 1.0f) {
				sub_v3_v3v3(dvec, path->data[path->len - 1].vec, path->data[path->len - 2].vec);
				mul_v3_fl(dvec, (ctime - 1.0f) * (float)path->len);
				add_v3_v3(vec, dvec);
				if (quat) copy_qt_qt(quat, path->data[path->len - 1].quat);
				if (radius) *radius = path->data[path->len - 1].radius;
				/* weight - not used but could be added */
			}
		}
		return true;
	}
	return false;
}

/* for each point, rotate & translate to curve */
/* use path, since it has constant distances */
/* co: local coord, result local too */
/* returns quaternion for rotation, using cd->no_rot_axis */
/* axis is using another define!!! */
static bool calc_curve_deform(Scene *scene, Object *par, float co[3],
                              const short axis, CurveDeform *cd, float r_quat[4])
{
	Curve *cu = par->data;
	float fac, loc[4], dir[3], new_quat[4], radius;
	short index;
	const bool is_neg_axis = (axis > 2);

	/* to be sure, mostly after file load, also cyclic dependencies */
#ifdef CYCLIC_DEPENDENCY_WORKAROUND
	if (par->curve_cache == NULL) {
		BKE_displist_make_curveTypes(scene, par, false);
	}
#endif

	if (par->curve_cache->path == NULL) {
		return false;  /* happens on append, cyclic dependencies and empty curves */
	}

	/* options */
	if (is_neg_axis) {
		index = axis - 3;
		if (cu->flag & CU_STRETCH)
			fac = (-co[index] - cd->dmax[index]) / (cd->dmax[index] - cd->dmin[index]);
		else
			fac = -(co[index] - cd->dmax[index]) / (par->curve_cache->path->totdist);
	}
	else {
		index = axis;
		if (cu->flag & CU_STRETCH) {
			fac = (co[index] - cd->dmin[index]) / (cd->dmax[index] - cd->dmin[index]);
		}
		else {
			if (LIKELY(par->curve_cache->path->totdist > FLT_EPSILON)) {
				fac = +(co[index] - cd->dmin[index]) / (par->curve_cache->path->totdist);
			}
			else {
				fac = 0.0f;
			}
		}
	}
	
	if (where_on_path_deform(par, fac, loc, dir, new_quat, &radius)) {  /* returns OK */
		float quat[4], cent[3];

		if (cd->no_rot_axis) {  /* set by caller */

			/* this is not exactly the same as 2.4x, since the axis is having rotation removed rather than
			 * changing the axis before calculating the tilt but serves much the same purpose */
			float dir_flat[3] = {0, 0, 0}, q[4];
			copy_v3_v3(dir_flat, dir);
			dir_flat[cd->no_rot_axis - 1] = 0.0f;

			normalize_v3(dir);
			normalize_v3(dir_flat);

			rotation_between_vecs_to_quat(q, dir, dir_flat); /* Could this be done faster? */

			mul_qt_qtqt(new_quat, q, new_quat);
		}


		/* Logic for 'cent' orientation *
		 *
		 * The way 'co' is copied to 'cent' may seem to have no meaning, but it does.
		 *
		 * Use a curve modifier to stretch a cube out, color each side RGB, positive side light, negative dark.
		 * view with X up (default), from the angle that you can see 3 faces RGB colors (light), anti-clockwise
		 * Notice X,Y,Z Up all have light colors and each ordered CCW.
		 *
		 * Now for Neg Up XYZ, the colors are all dark, and ordered clockwise - Campbell
		 *
		 * note: moved functions into quat_apply_track/vec_apply_track
		 * */
		copy_qt_qt(quat, new_quat);
		copy_v3_v3(cent, co);

		/* zero the axis which is not used,
		 * the big block of text above now applies to these 3 lines */
		quat_apply_track(quat, axis, (axis == 0 || axis == 2) ? 1 : 0); /* up flag is a dummy, set so no rotation is done */
		vec_apply_track(cent, axis);
		cent[index] = 0.0f;


		/* scale if enabled */
		if (cu->flag & CU_PATH_RADIUS)
			mul_v3_fl(cent, radius);
		
		/* local rotation */
		normalize_qt(quat);
		mul_qt_v3(quat, cent);

		/* translation */
		add_v3_v3v3(co, cent, loc);

		if (r_quat)
			copy_qt_qt(r_quat, quat);

		return true;
	}
	return false;
}

void curve_deform_verts(
        Scene *scene, Object *cuOb, Object *target, DerivedMesh *dm, float (*vertexCos)[3],
        int numVerts, const char *vgroup, short defaxis)
{
	Curve *cu;
	int a;
	CurveDeform cd;
	MDeformVert *dvert = NULL;
	int defgrp_index = -1;
	const bool is_neg_axis = (defaxis > 2);

	if (cuOb->type != OB_CURVE)
		return;

	cu = cuOb->data;

	init_curve_deform(cuOb, target, &cd);

	/* dummy bounds, keep if CU_DEFORM_BOUNDS_OFF is set */
	if (is_neg_axis == false) {
		cd.dmin[0] = cd.dmin[1] = cd.dmin[2] = 0.0f;
		cd.dmax[0] = cd.dmax[1] = cd.dmax[2] = 1.0f;
	}
	else {
		/* negative, these bounds give a good rest position */
		cd.dmin[0] = cd.dmin[1] = cd.dmin[2] = -1.0f;
		cd.dmax[0] = cd.dmax[1] = cd.dmax[2] =  0.0f;
	}
	
	/* Check whether to use vertex groups (only possible if target is a Mesh or Lattice).
	 * We want either a Mesh/Lattice with no derived data, or derived data with deformverts.
	 */
	if (vgroup && vgroup[0] && ELEM(target->type, OB_MESH, OB_LATTICE)) {
		defgrp_index = defgroup_name_index(target, vgroup);

		if (defgrp_index != -1) {
			/* if there's derived data without deformverts, don't use vgroups */
			if (dm) {
				dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
			}
			else if (target->type == OB_LATTICE) {
				dvert = ((Lattice *)target->data)->dvert;
			}
			else {
				dvert = ((Mesh *)target->data)->dvert;
			}
		}
	}

	if (dvert) {
		MDeformVert *dvert_iter;
		float vec[3];

		if (cu->flag & CU_DEFORM_BOUNDS_OFF) {
			for (a = 0, dvert_iter = dvert; a < numVerts; a++, dvert_iter++) {
				const float weight = defvert_find_weight(dvert_iter, defgrp_index);

				if (weight > 0.0f) {
					mul_m4_v3(cd.curvespace, vertexCos[a]);
					copy_v3_v3(vec, vertexCos[a]);
					calc_curve_deform(scene, cuOb, vec, defaxis, &cd, NULL);
					interp_v3_v3v3(vertexCos[a], vertexCos[a], vec, weight);
					mul_m4_v3(cd.objectspace, vertexCos[a]);
				}
			}
		}
		else {
			/* set mesh min/max bounds */
			INIT_MINMAX(cd.dmin, cd.dmax);

			for (a = 0, dvert_iter = dvert; a < numVerts; a++, dvert_iter++) {
				if (defvert_find_weight(dvert_iter, defgrp_index) > 0.0f) {
					mul_m4_v3(cd.curvespace, vertexCos[a]);
					minmax_v3v3_v3(cd.dmin, cd.dmax, vertexCos[a]);
				}
			}

			for (a = 0, dvert_iter = dvert; a < numVerts; a++, dvert_iter++) {
				const float weight = defvert_find_weight(dvert_iter, defgrp_index);

				if (weight > 0.0f) {
					/* already in 'cd.curvespace', prev for loop */
					copy_v3_v3(vec, vertexCos[a]);
					calc_curve_deform(scene, cuOb, vec, defaxis, &cd, NULL);
					interp_v3_v3v3(vertexCos[a], vertexCos[a], vec, weight);
					mul_m4_v3(cd.objectspace, vertexCos[a]);
				}
			}
		}
	}
	else {
		if (cu->flag & CU_DEFORM_BOUNDS_OFF) {
			for (a = 0; a < numVerts; a++) {
				mul_m4_v3(cd.curvespace, vertexCos[a]);
				calc_curve_deform(scene, cuOb, vertexCos[a], defaxis, &cd, NULL);
				mul_m4_v3(cd.objectspace, vertexCos[a]);
			}
		}
		else {
			/* set mesh min max bounds */
			INIT_MINMAX(cd.dmin, cd.dmax);
				
			for (a = 0; a < numVerts; a++) {
				mul_m4_v3(cd.curvespace, vertexCos[a]);
				minmax_v3v3_v3(cd.dmin, cd.dmax, vertexCos[a]);
			}
	
			for (a = 0; a < numVerts; a++) {
				/* already in 'cd.curvespace', prev for loop */
				calc_curve_deform(scene, cuOb, vertexCos[a], defaxis, &cd, NULL);
				mul_m4_v3(cd.objectspace, vertexCos[a]);
			}
		}
	}
}

/* input vec and orco = local coord in armature space */
/* orco is original not-animated or deformed reference point */
/* result written in vec and mat */
void curve_deform_vector(Scene *scene, Object *cuOb, Object *target,
                         float orco[3], float vec[3], float mat[3][3], int no_rot_axis)
{
	CurveDeform cd;
	float quat[4];
	
	if (cuOb->type != OB_CURVE) {
		unit_m3(mat);
		return;
	}

	init_curve_deform(cuOb, target, &cd);
	cd.no_rot_axis = no_rot_axis;                /* option to only rotate for XY, for example */
	
	copy_v3_v3(cd.dmin, orco);
	copy_v3_v3(cd.dmax, orco);

	mul_m4_v3(cd.curvespace, vec);
	
	if (calc_curve_deform(scene, cuOb, vec, target->trackflag, &cd, quat)) {
		float qmat[3][3];
		
		quat_to_mat3(qmat, quat);
		mul_m3_m3m3(mat, qmat, cd.objectspace3);
	}
	else
		unit_m3(mat);
	
	mul_m4_v3(cd.objectspace, vec);

}

void lattice_deform_verts(Object *laOb, Object *target, DerivedMesh *dm,
                          float (*vertexCos)[3], int numVerts, const char *vgroup, float fac)
{
	LatticeDeformData *lattice_deform_data;
	int a;
	bool use_vgroups;

	if (laOb->type != OB_LATTICE)
		return;

	lattice_deform_data = init_latt_deform(laOb, target);

	/* check whether to use vertex groups (only possible if target is a Mesh)
	 * we want either a Mesh with no derived data, or derived data with
	 * deformverts
	 */
	if (target && target->type == OB_MESH) {
		/* if there's derived data without deformverts, don't use vgroups */
		if (dm) {
			use_vgroups = (dm->getVertDataArray(dm, CD_MDEFORMVERT) != NULL);
		}
		else {
			Mesh *me = target->data;
			use_vgroups = (me->dvert != NULL);
		}
	}
	else {
		use_vgroups = false;
	}
	
	if (vgroup && vgroup[0] && use_vgroups) {
		Mesh *me = target->data;
		const int defgrp_index = defgroup_name_index(target, vgroup);
		float weight;

		if (defgrp_index >= 0 && (me->dvert || dm)) {
			MDeformVert *dvert = me->dvert;
			
			for (a = 0; a < numVerts; a++, dvert++) {
				if (dm) dvert = dm->getVertData(dm, a, CD_MDEFORMVERT);

				weight = defvert_find_weight(dvert, defgrp_index);

				if (weight > 0.0f)
					calc_latt_deform(lattice_deform_data, vertexCos[a], weight * fac);
			}
		}
	}
	else {
		for (a = 0; a < numVerts; a++) {
			calc_latt_deform(lattice_deform_data, vertexCos[a], fac);
		}
	}
	end_latt_deform(lattice_deform_data);
}

bool object_deform_mball(Object *ob, ListBase *dispbase)
{
	if (ob->parent && ob->parent->type == OB_LATTICE && ob->partype == PARSKEL) {
		DispList *dl;

		for (dl = dispbase->first; dl; dl = dl->next) {
			lattice_deform_verts(ob->parent, ob, NULL,
			                     (float(*)[3])dl->verts, dl->nr, NULL, 1.0f);
		}

		return true;
	}
	else {
		return false;
	}
}

static BPoint *latt_bp(Lattice *lt, int u, int v, int w)
{
	return &lt->def[BKE_lattice_index_from_uvw(lt, u, v, w)];
}

void outside_lattice(Lattice *lt)
{
	BPoint *bp, *bp1, *bp2;
	int u, v, w;
	float fac1, du = 0.0, dv = 0.0, dw = 0.0;

	if (lt->flag & LT_OUTSIDE) {
		bp = lt->def;

		if (lt->pntsu > 1) du = 1.0f / ((float)lt->pntsu - 1);
		if (lt->pntsv > 1) dv = 1.0f / ((float)lt->pntsv - 1);
		if (lt->pntsw > 1) dw = 1.0f / ((float)lt->pntsw - 1);
			
		for (w = 0; w < lt->pntsw; w++) {
			
			for (v = 0; v < lt->pntsv; v++) {
			
				for (u = 0; u < lt->pntsu; u++, bp++) {
					if (u == 0 || v == 0 || w == 0 || u == lt->pntsu - 1 || v == lt->pntsv - 1 || w == lt->pntsw - 1) {
						/* pass */
					}
					else {
						bp->hide = 1;
						bp->f1 &= ~SELECT;
						
						/* u extrema */
						bp1 = latt_bp(lt, 0, v, w);
						bp2 = latt_bp(lt, lt->pntsu - 1, v, w);
						
						fac1 = du * u;
						bp->vec[0] = (1.0f - fac1) * bp1->vec[0] + fac1 * bp2->vec[0];
						bp->vec[1] = (1.0f - fac1) * bp1->vec[1] + fac1 * bp2->vec[1];
						bp->vec[2] = (1.0f - fac1) * bp1->vec[2] + fac1 * bp2->vec[2];
						
						/* v extrema */
						bp1 = latt_bp(lt, u, 0, w);
						bp2 = latt_bp(lt, u, lt->pntsv - 1, w);
						
						fac1 = dv * v;
						bp->vec[0] += (1.0f - fac1) * bp1->vec[0] + fac1 * bp2->vec[0];
						bp->vec[1] += (1.0f - fac1) * bp1->vec[1] + fac1 * bp2->vec[1];
						bp->vec[2] += (1.0f - fac1) * bp1->vec[2] + fac1 * bp2->vec[2];
						
						/* w extrema */
						bp1 = latt_bp(lt, u, v, 0);
						bp2 = latt_bp(lt, u, v, lt->pntsw - 1);
						
						fac1 = dw * w;
						bp->vec[0] += (1.0f - fac1) * bp1->vec[0] + fac1 * bp2->vec[0];
						bp->vec[1] += (1.0f - fac1) * bp1->vec[1] + fac1 * bp2->vec[1];
						bp->vec[2] += (1.0f - fac1) * bp1->vec[2] + fac1 * bp2->vec[2];
						
						mul_v3_fl(bp->vec, 1.0f / 3.0f);
						
					}
				}
				
			}
			
		}
	}
	else {
		bp = lt->def;

		for (w = 0; w < lt->pntsw; w++)
			for (v = 0; v < lt->pntsv; v++)
				for (u = 0; u < lt->pntsu; u++, bp++)
					bp->hide = 0;
	}
}

float (*BKE_lattice_vertexcos_get(struct Object *ob, int *r_numVerts))[3]
{
	Lattice *lt = ob->data;
	int i, numVerts;
	float (*vertexCos)[3];

	if (lt->editlatt) lt = lt->editlatt->latt;
	numVerts = *r_numVerts = lt->pntsu * lt->pntsv * lt->pntsw;
	
	vertexCos = MEM_mallocN(sizeof(*vertexCos) * numVerts, "lt_vcos");
	
	for (i = 0; i < numVerts; i++) {
		copy_v3_v3(vertexCos[i], lt->def[i].vec);
	}

	return vertexCos;
}

void BKE_lattice_vertexcos_apply(struct Object *ob, float (*vertexCos)[3])
{
	Lattice *lt = ob->data;
	int i, numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

	for (i = 0; i < numVerts; i++) {
		copy_v3_v3(lt->def[i].vec, vertexCos[i]);
	}
}

void BKE_lattice_modifiers_calc(Scene *scene, Object *ob)
{
	Lattice *lt = ob->data;
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	float (*vertexCos)[3] = NULL;
	int numVerts, editmode = (lt->editlatt != NULL);

	if (ob->curve_cache) {
		BKE_displist_free(&ob->curve_cache->disp);
	}
	else {
		ob->curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for lattice");
	}

	for (; md; md = md->next) {
		const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene = scene;
		
		if (!(mti->flags & eModifierTypeFlag_AcceptsLattice)) continue;
		if (!(md->mode & eModifierMode_Realtime)) continue;
		if (editmode && !(md->mode & eModifierMode_Editmode)) continue;
		if (mti->isDisabled && mti->isDisabled(md, 0)) continue;
		if (mti->type != eModifierTypeType_OnlyDeform) continue;

		if (!vertexCos) vertexCos = BKE_lattice_vertexcos_get(ob, &numVerts);
		mti->deformVerts(md, ob, NULL, vertexCos, numVerts, 0);
	}

	/* always displist to make this work like derivedmesh */
	if (!vertexCos) vertexCos = BKE_lattice_vertexcos_get(ob, &numVerts);
	
	{
		DispList *dl = MEM_callocN(sizeof(*dl), "lt_dl");
		dl->type = DL_VERTS;
		dl->parts = 1;
		dl->nr = numVerts;
		dl->verts = (float *) vertexCos;
		
		BLI_addtail(&ob->curve_cache->disp, dl);
	}
}

struct MDeformVert *BKE_lattice_deform_verts_get(struct Object *oblatt)
{
	Lattice *lt = (Lattice *)oblatt->data;
	BLI_assert(oblatt->type == OB_LATTICE);
	if (lt->editlatt) lt = lt->editlatt->latt;
	return lt->dvert;
}

struct BPoint *BKE_lattice_active_point_get(Lattice *lt)
{
	BLI_assert(GS(lt->id.name) == ID_LT);

	if (lt->editlatt) {
		lt = lt->editlatt->latt;
	}

	BLI_assert(lt->actbp < lt->pntsu * lt->pntsv * lt->pntsw);

	if ((lt->actbp != LT_ACTBP_NONE) && (lt->actbp < lt->pntsu * lt->pntsv * lt->pntsw)) {
		return &lt->def[lt->actbp];
	}
	else {
		return NULL;
	}
}

void BKE_lattice_center_median(Lattice *lt, float cent[3])
{
	int i, numVerts;

	if (lt->editlatt) lt = lt->editlatt->latt;
	numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

	zero_v3(cent);

	for (i = 0; i < numVerts; i++)
		add_v3_v3(cent, lt->def[i].vec);

	mul_v3_fl(cent, 1.0f / (float)numVerts);
}

static void boundbox_lattice(Object *ob)
{
	BoundBox *bb;
	Lattice *lt;
	float min[3], max[3];

	if (ob->bb == NULL) {
		ob->bb = MEM_callocN(sizeof(BoundBox), "Lattice boundbox");
	}

	bb = ob->bb;
	lt = ob->data;

	INIT_MINMAX(min, max);
	BKE_lattice_minmax_dl(ob, lt, min, max);
	BKE_boundbox_init_from_minmax(bb, min, max);

	bb->flag &= ~BOUNDBOX_DIRTY;
}

BoundBox *BKE_lattice_boundbox_get(Object *ob)
{
	boundbox_lattice(ob);

	return ob->bb;
}

void BKE_lattice_minmax_dl(Object *ob, Lattice *lt, float min[3], float max[3])
{
	DispList *dl = ob->curve_cache ? BKE_displist_find(&ob->curve_cache->disp, DL_VERTS) : NULL;

	if (!dl) {
		BKE_lattice_minmax(lt, min, max);
	}
	else {
		int i, numVerts;
		
		if (lt->editlatt) lt = lt->editlatt->latt;
		numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

		for (i = 0; i < numVerts; i++)
			minmax_v3v3_v3(min, max, &dl->verts[i * 3]);
	}
}

void BKE_lattice_minmax(Lattice *lt, float min[3], float max[3])
{
	int i, numVerts;

	if (lt->editlatt) lt = lt->editlatt->latt;
	numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

	for (i = 0; i < numVerts; i++)
		minmax_v3v3_v3(min, max, lt->def[i].vec);
}

void BKE_lattice_center_bounds(Lattice *lt, float cent[3])
{
	float min[3], max[3];

	INIT_MINMAX(min, max);

	BKE_lattice_minmax(lt, min, max);
	mid_v3_v3v3(cent, min, max);
}

void BKE_lattice_transform(Lattice *lt, float mat[4][4], bool do_keys)
{
	BPoint *bp = lt->def;
	int i = lt->pntsu * lt->pntsv * lt->pntsw;

	while (i--) {
		mul_m4_v3(mat, bp->vec);
		bp++;
	}

	if (do_keys && lt->key) {
		KeyBlock *kb;

		for (kb = lt->key->block.first; kb; kb = kb->next) {
			float *fp = kb->data;
			for (i = kb->totelem; i--; fp += 3) {
				mul_m4_v3(mat, fp);
			}
		}
	}
}

void BKE_lattice_translate(Lattice *lt, float offset[3], bool do_keys)
{
	int i, numVerts;

	numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

	if (lt->def)
		for (i = 0; i < numVerts; i++)
			add_v3_v3(lt->def[i].vec, offset);

	if (lt->editlatt)
		for (i = 0; i < numVerts; i++)
			add_v3_v3(lt->editlatt->latt->def[i].vec, offset);

	if (do_keys && lt->key) {
		KeyBlock *kb;

		for (kb = lt->key->block.first; kb; kb = kb->next) {
			float *fp = kb->data;
			for (i = kb->totelem; i--; fp += 3) {
				add_v3_v3(fp, offset);
			}
		}
	}
}

/* **** Depsgraph evaluation **** */

void BKE_lattice_eval_geometry(EvaluationContext *UNUSED(eval_ctx),
                               Lattice *UNUSED(latt))
{
}

