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

#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

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
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "BKE_deform.h"

//XXX #include "BIF_editdeform.h"

void calc_lat_fudu(int flag, int res, float *fu, float *du)
{
	if (res == 1) {
		*fu = 0.0;
		*du = 0.0;
	}
	else if (flag & LT_GRID) {
		*fu = -0.5f * (res - 1);
		*du = 1.0f;
	}
	else {
		*fu = -1.0f;
		*du = 2.0f / (res - 1);
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
		free_dverts(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
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
		BKE_displist_free(&ltOb->disp);

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

	MEM_freeN(lt->def);
	lt->def = MEM_callocN(lt->pntsu * lt->pntsv * lt->pntsw * sizeof(BPoint), "lattice bp");
	
	bp = lt->def;
	
	for (i = 0; i < lt->pntsu * lt->pntsv * lt->pntsw; i++, bp++) {
		copy_v3_v3(bp->vec, vertexCos[i]);
	}

	MEM_freeN(vertexCos);
}

Lattice *BKE_lattice_add(const char *name)
{
	Lattice *lt;
	
	lt = BKE_libblock_alloc(&G.main->latt, ID_LT, name);
	
	lt->flag = LT_GRID;
	
	lt->typeu = lt->typev = lt->typew = KEY_BSPLINE;
	
	lt->def = MEM_callocN(sizeof(BPoint), "lattvert"); /* temporary */
	BKE_lattice_resize(lt, 2, 2, 2, NULL);  /* creates a uniform lattice */
		
	return lt;
}

Lattice *BKE_lattice_copy(Lattice *lt)
{
	Lattice *ltn;

	ltn = BKE_libblock_copy(&lt->id);
	ltn->def = MEM_dupallocN(lt->def);

	ltn->key = BKE_key_copy(ltn->key);
	if (ltn->key) ltn->key->from = (ID *)ltn;
	
	if (lt->dvert) {
		int tot = lt->pntsu * lt->pntsv * lt->pntsw;
		ltn->dvert = MEM_mallocN(sizeof (MDeformVert) * tot, "Lattice MDeformVert");
		copy_dverts(ltn->dvert, lt->dvert, tot);
	}

	ltn->editlatt = NULL;

	return ltn;
}

void BKE_lattice_free(Lattice *lt)
{
	if (lt->def) MEM_freeN(lt->def);
	if (lt->dvert) free_dverts(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
	if (lt->editlatt) {
		Lattice *editlt = lt->editlatt->latt;

		if (editlt->def) MEM_freeN(editlt->def);
		if (editlt->dvert) free_dverts(editlt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);

		MEM_freeN(editlt);
		MEM_freeN(lt->editlatt);
	}
	
	/* free animation data */
	if (lt->adt) {
		BKE_free_animdata(&lt->id);
		lt->adt = NULL;
	}
}


void BKE_lattice_make_local(Lattice *lt)
{
	Main *bmain = G.main;
	Object *ob;
	int is_local = FALSE, is_lib = FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (lt->id.lib == NULL) return;
	if (lt->id.us == 1) {
		id_clear_lib_data(bmain, &lt->id);
		return;
	}
	
	for (ob = bmain->object.first; ob && ELEM(FALSE, is_lib, is_local); ob = ob->id.next) {
		if (ob->data == lt) {
			if (ob->id.lib) is_lib = TRUE;
			else is_local = TRUE;
		}
	}
	
	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &lt->id);
	}
	else if (is_local && is_lib) {
		Lattice *lt_new = BKE_lattice_copy(lt);
		lt_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, lt->id.lib, &lt_new->id);

		for (ob = bmain->object.first; ob; ob = ob->id.next) {
			if (ob->data == lt) {
				if (ob->id.lib == NULL) {
					ob->data = lt_new;
					lt_new->id.us++;
					lt->id.us--;
				}
			}
		}
	}
}

void init_latt_deform(Object *oblatt, Object *ob)
{
	/* we make an array with all differences */
	Lattice *lt = oblatt->data;
	BPoint *bp;
	DispList *dl = BKE_displist_find(&oblatt->disp, DL_VERTS);
	float *co = dl ? dl->verts : NULL;
	float *fp, imat[4][4];
	float fu, fv, fw;
	int u, v, w;

	if (lt->editlatt) lt = lt->editlatt->latt;
	bp = lt->def;
	
	fp = lt->latticedata = MEM_mallocN(sizeof(float) * 3 * lt->pntsu * lt->pntsv * lt->pntsw, "latticedata");
	
	/* for example with a particle system: ob==0 */
	if (ob == NULL) {
		/* in deformspace, calc matrix  */
		invert_m4_m4(lt->latmat, oblatt->obmat);
	
		/* back: put in deform array */
		invert_m4_m4(imat, lt->latmat);
	}
	else {
		/* in deformspace, calc matrix */
		invert_m4_m4(imat, oblatt->obmat);
		mult_m4_m4m4(lt->latmat, imat, ob->obmat);
	
		/* back: put in deform array */
		invert_m4_m4(imat, lt->latmat);
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
}

void calc_latt_deform(Object *ob, float co[3], float weight)
{
	Lattice *lt = ob->data;
	float u, v, w, tu[4], tv[4], tw[4];
	float vec[3];
	int idx_w, idx_v, idx_u;
	int ui, vi, wi, uu, vv, ww;

	/* vgroup influence */
	int defgroup_nr = -1;
	float co_prev[3], weight_blend = 0.0f;
	MDeformVert *dvert = BKE_lattice_deform_verts_get(ob);


	if (lt->editlatt) lt = lt->editlatt->latt;
	if (lt->latticedata == NULL) return;

	if (lt->vgroup[0] && dvert) {
		defgroup_nr = defgroup_name_index(ob, lt->vgroup);
		copy_v3_v3(co_prev, co);
	}

	/* co is in local coords, treat with latmat */
	mul_v3_m4v3(vec, lt->latmat, co);

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
				else idx_w = (lt->pntsw - 1) * lt->pntsu * lt->pntsv;
			}
			else idx_w = 0;

			for (vv = vi - 1; vv <= vi + 2; vv++) {
				v = w * tv[vv - vi + 1];

				if (v != 0.0f) {
					if (vv > 0) {
						if (vv < lt->pntsv) idx_v = idx_w + vv * lt->pntsu;
						else idx_v = idx_w + (lt->pntsv - 1) * lt->pntsu;
					}
					else idx_v = idx_w;

					for (uu = ui - 1; uu <= ui + 2; uu++) {
						u = weight * v * tu[uu - ui + 1];

						if (u != 0.0f) {
							if (uu > 0) {
								if (uu < lt->pntsu) idx_u = idx_v + uu;
								else idx_u = idx_v + (lt->pntsu - 1);
							}
							else idx_u = idx_v;

							madd_v3_v3fl(co, &lt->latticedata[idx_u * 3], u);

							if (defgroup_nr != -1)
								weight_blend += (u * defvert_find_weight(dvert + idx_u, defgroup_nr));
						}
					}
				}
			}
		}
	}

	if (defgroup_nr != -1)
		interp_v3_v3v3(co, co_prev, co, weight_blend);

}

void end_latt_deform(Object *ob)
{
	Lattice *lt = ob->data;
	
	if (lt->editlatt) lt = lt->editlatt->latt;
	
	if (lt->latticedata)
		MEM_freeN(lt->latticedata);
	lt->latticedata = NULL;
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
	mult_m4_m4m4(cd->objectspace, ob->imat, par->obmat);
	invert_m4_m4(cd->curvespace, cd->objectspace);
	copy_m3_m4(cd->objectspace3, cd->objectspace);
	cd->no_rot_axis = 0;
}

/* this makes sure we can extend for non-cyclic.
 *
 * returns OK: 1/0
 */
static int where_on_path_deform(Object *ob, float ctime, float vec[4], float dir[3], float quat[4], float *radius)
{
	Curve *cu = ob->data;
	BevList *bl;
	float ctime1;
	int cycl = 0;
	
	/* test for cyclic */
	bl = cu->bev.first;
	if (!bl->nr) return 0;
	if (bl && bl->poly > -1) cycl = 1;

	if (cycl == 0) {
		ctime1 = CLAMPIS(ctime, 0.0f, 1.0f);
	}
	else ctime1 = ctime;
	
	/* vec needs 4 items */
	if (where_on_path(ob, ctime1, vec, dir, quat, radius, NULL)) {
		
		if (cycl == 0) {
			Path *path = cu->path;
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
		return 1;
	}
	return 0;
}

/* for each point, rotate & translate to curve */
/* use path, since it has constant distances */
/* co: local coord, result local too */
/* returns quaternion for rotation, using cd->no_rot_axis */
/* axis is using another define!!! */
static int calc_curve_deform(Scene *scene, Object *par, float co[3],
                             const short axis, CurveDeform *cd, float quat_r[4])
{
	Curve *cu = par->data;
	float fac, loc[4], dir[3], new_quat[4], radius;
	short index;
	const int is_neg_axis = (axis > 2);

	/* to be sure, mostly after file load */
	if (cu->path == NULL) {
		BKE_displist_make_curveTypes(scene, par, 0);
		if (cu->path == NULL) return 0;  // happens on append...
	}
	
	/* options */
	if (is_neg_axis) {
		index = axis - 3;
		if (cu->flag & CU_STRETCH)
			fac = (-co[index] - cd->dmax[index]) / (cd->dmax[index] - cd->dmin[index]);
		else
			fac = -(co[index] - cd->dmax[index]) / (cu->path->totdist);
	}
	else {
		index = axis;
		if (cu->flag & CU_STRETCH)
			fac = (co[index] - cd->dmin[index]) / (cd->dmax[index] - cd->dmin[index]);
		else
			fac = +(co[index] - cd->dmin[index]) / (cu->path->totdist);
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

		if (quat_r)
			copy_qt_qt(quat_r, quat);

		return 1;
	}
	return 0;
}

void curve_deform_verts(Scene *scene, Object *cuOb, Object *target,
                        DerivedMesh *dm, float (*vertexCos)[3],
                        int numVerts, const char *vgroup, short defaxis)
{
	Curve *cu;
	int a, flag;
	CurveDeform cd;
	int use_vgroups;
	const int is_neg_axis = (defaxis > 2);

	if (cuOb->type != OB_CURVE)
		return;

	cu = cuOb->data;
	flag = cu->flag;
	cu->flag |= (CU_PATH | CU_FOLLOW); // needed for path & bevlist

	init_curve_deform(cuOb, target, &cd);

	/* dummy bounds, keep if CU_DEFORM_BOUNDS_OFF is set */
	if (is_neg_axis == FALSE) {
		cd.dmin[0] = cd.dmin[1] = cd.dmin[2] = 0.0f;
		cd.dmax[0] = cd.dmax[1] = cd.dmax[2] = 1.0f;
	}
	else {
		/* negative, these bounds give a good rest position */
		cd.dmin[0] = cd.dmin[1] = cd.dmin[2] = -1.0f;
		cd.dmax[0] = cd.dmax[1] = cd.dmax[2] =  0.0f;
	}
	
	/* check whether to use vertex groups (only possible if target is a Mesh)
	 * we want either a Mesh with no derived data, or derived data with
	 * deformverts
	 */
	if (target && target->type == OB_MESH) {
		/* if there's derived data without deformverts, don't use vgroups */
		if (dm && !dm->getVertData(dm, 0, CD_MDEFORMVERT))
			use_vgroups = 0;
		else
			use_vgroups = 1;
	}
	else {
		use_vgroups = 0;
	}
	
	if (vgroup && vgroup[0] && use_vgroups) {
		Mesh *me = target->data;
		int index = defgroup_name_index(target, vgroup);

		if (index != -1 && (me->dvert || dm)) {
			MDeformVert *dvert = me->dvert;
			float vec[3];
			float weight;
	

			if (cu->flag & CU_DEFORM_BOUNDS_OFF) {
				dvert = me->dvert;
				for (a = 0; a < numVerts; a++, dvert++) {
					if (dm) dvert = dm->getVertData(dm, a, CD_MDEFORMVERT);
					weight = defvert_find_weight(dvert, index);
	
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
	
				for (a = 0; a < numVerts; a++, dvert++) {
					if (dm) dvert = dm->getVertData(dm, a, CD_MDEFORMVERT);
					
					if (defvert_find_weight(dvert, index) > 0.0f) {
						mul_m4_v3(cd.curvespace, vertexCos[a]);
						DO_MINMAX(vertexCos[a], cd.dmin, cd.dmax);
					}
				}
	
				dvert = me->dvert;
				for (a = 0; a < numVerts; a++, dvert++) {
					if (dm) dvert = dm->getVertData(dm, a, CD_MDEFORMVERT);
					
					weight = defvert_find_weight(dvert, index);
	
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
				DO_MINMAX(vertexCos[a], cd.dmin, cd.dmax);
			}
	
			for (a = 0; a < numVerts; a++) {
				/* already in 'cd.curvespace', prev for loop */
				calc_curve_deform(scene, cuOb, vertexCos[a], defaxis, &cd, NULL);
				mul_m4_v3(cd.objectspace, vertexCos[a]);
			}
		}
	}
	cu->flag = flag;
}

/* input vec and orco = local coord in armature space */
/* orco is original not-animated or deformed reference point */
/* result written in vec and mat */
void curve_deform_vector(Scene *scene, Object *cuOb, Object *target,
                         float orco[3], float vec[3], float mat[][3], int no_rot_axis)
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
	int a;
	int use_vgroups;

	if (laOb->type != OB_LATTICE)
		return;

	init_latt_deform(laOb, target);

	/* check whether to use vertex groups (only possible if target is a Mesh)
	 * we want either a Mesh with no derived data, or derived data with
	 * deformverts
	 */
	if (target && target->type == OB_MESH) {
		/* if there's derived data without deformverts, don't use vgroups */
		if (dm && !dm->getVertData(dm, 0, CD_MDEFORMVERT))
			use_vgroups = 0;
		else
			use_vgroups = 1;
	}
	else {
		use_vgroups = 0;
	}
	
	if (vgroup && vgroup[0] && use_vgroups) {
		Mesh *me = target->data;
		int index = defgroup_name_index(target, vgroup);
		float weight;

		if (index >= 0 && (me->dvert || dm)) {
			MDeformVert *dvert = me->dvert;
			
			for (a = 0; a < numVerts; a++, dvert++) {
				if (dm) dvert = dm->getVertData(dm, a, CD_MDEFORMVERT);

				weight = defvert_find_weight(dvert, index);

				if (weight > 0.0f)
					calc_latt_deform(laOb, vertexCos[a], weight * fac);
			}
		}
	}
	else {
		for (a = 0; a < numVerts; a++) {
			calc_latt_deform(laOb, vertexCos[a], fac);
		}
	}
	end_latt_deform(laOb);
}

int object_deform_mball(Object *ob, ListBase *dispbase)
{
	if (ob->parent && ob->parent->type == OB_LATTICE && ob->partype == PARSKEL) {
		DispList *dl;

		for (dl = dispbase->first; dl; dl = dl->next) {
			lattice_deform_verts(ob->parent, ob, NULL,
			                     (float(*)[3])dl->verts, dl->nr, NULL, 1.0f);
		}

		return 1;
	}
	else {
		return 0;
	}
}

static BPoint *latt_bp(Lattice *lt, int u, int v, int w)
{
	return &lt->def[LT_INDEX(lt, u, v, w)];
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
					if (u == 0 || v == 0 || w == 0 || u == lt->pntsu - 1 || v == lt->pntsv - 1 || w == lt->pntsw - 1) ;
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
						
						mul_v3_fl(bp->vec, 0.3333333f);
						
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

float (*BKE_lattice_vertexcos_get(struct Object *ob, int *numVerts_r))[3]
{
	Lattice *lt = ob->data;
	int i, numVerts;
	float (*vertexCos)[3];

	if (lt->editlatt) lt = lt->editlatt->latt;
	numVerts = *numVerts_r = lt->pntsu * lt->pntsv * lt->pntsw;
	
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
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	float (*vertexCos)[3] = NULL;
	int numVerts, editmode = (lt->editlatt != NULL);

	BKE_displist_free(&ob->disp);

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene = scene;
		
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
		
		BLI_addtail(&ob->disp, dl);
	}
}

struct MDeformVert *BKE_lattice_deform_verts_get(struct Object *oblatt)
{
	Lattice *lt = (Lattice *)oblatt->data;
	BLI_assert(oblatt->type == OB_LATTICE);
	if (lt->editlatt) lt = lt->editlatt->latt;
	return lt->dvert;
}
