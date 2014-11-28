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

/** \file blender/blenkernel/intern/displist.c
 *  \ingroup bke
 */


#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"

#include "BLI_blenlib.h"
#include "BLI_memarena.h"
#include "BLI_math.h"
#include "BLI_scanfill.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_object.h"
#include "BKE_mball.h"
#include "BKE_curve.h"
#include "BKE_key.h"
#include "BKE_anim.h"
#include "BKE_font.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "BLI_sys_types.h" // for intptr_t support

static void boundbox_displist_object(Object *ob);

void BKE_displist_elem_free(DispList *dl)
{
	if (dl) {
		if (dl->verts) MEM_freeN(dl->verts);
		if (dl->nors) MEM_freeN(dl->nors);
		if (dl->index) MEM_freeN(dl->index);
		if (dl->bevelSplitFlag) MEM_freeN(dl->bevelSplitFlag);
		MEM_freeN(dl);
	}
}

void BKE_displist_free(ListBase *lb)
{
	DispList *dl;

	while ((dl = BLI_pophead(lb))) {
		BKE_displist_elem_free(dl);
	}
}

DispList *BKE_displist_find_or_create(ListBase *lb, int type)
{
	DispList *dl;

	dl = lb->first;
	while (dl) {
		if (dl->type == type)
			return dl;
		dl = dl->next;
	}

	dl = MEM_callocN(sizeof(DispList), "find_disp");
	dl->type = type;
	BLI_addtail(lb, dl);

	return dl;
}

DispList *BKE_displist_find(ListBase *lb, int type)
{
	DispList *dl;

	dl = lb->first;
	while (dl) {
		if (dl->type == type)
			return dl;
		dl = dl->next;
	}

	return NULL;
}

bool BKE_displist_has_faces(ListBase *lb)
{
	DispList *dl;

	for (dl = lb->first; dl; dl = dl->next) {
		if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
			return true;
		}
	}

	return false;
}

void BKE_displist_copy(ListBase *lbn, ListBase *lb)
{
	DispList *dln, *dl;

	BKE_displist_free(lbn);

	dl = lb->first;
	while (dl) {
		dln = MEM_dupallocN(dl);
		BLI_addtail(lbn, dln);
		dln->verts = MEM_dupallocN(dl->verts);
		dln->nors = MEM_dupallocN(dl->nors);
		dln->index = MEM_dupallocN(dl->index);

		if (dl->bevelSplitFlag)
			dln->bevelSplitFlag = MEM_dupallocN(dl->bevelSplitFlag);

		dl = dl->next;
	}
}

void BKE_displist_normals_add(ListBase *lb)
{
	DispList *dl = NULL;
	float *vdata, *ndata, nor[3];
	float *v1, *v2, *v3, *v4;
	float *n1, *n2, *n3, *n4;
	int a, b, p1, p2, p3, p4;

	dl = lb->first;

	while (dl) {
		if (dl->type == DL_INDEX3) {
			if (dl->nors == NULL) {
				dl->nors = MEM_callocN(sizeof(float) * 3, "dlnors");

				if (dl->verts[2] < 0.0f)
					dl->nors[2] = -1.0f;
				else
					dl->nors[2] = 1.0f;
			}
		}
		else if (dl->type == DL_SURF) {
			if (dl->nors == NULL) {
				dl->nors = MEM_callocN(sizeof(float) * 3 * dl->nr * dl->parts, "dlnors");

				vdata = dl->verts;
				ndata = dl->nors;

				for (a = 0; a < dl->parts; a++) {

					if (BKE_displist_surfindex_get(dl, a, &b, &p1, &p2, &p3, &p4) == 0)
						break;

					v1 = vdata + 3 * p1;
					n1 = ndata + 3 * p1;
					v2 = vdata + 3 * p2;
					n2 = ndata + 3 * p2;
					v3 = vdata + 3 * p3;
					n3 = ndata + 3 * p3;
					v4 = vdata + 3 * p4;
					n4 = ndata + 3 * p4;

					for (; b < dl->nr; b++) {
						normal_quad_v3(nor, v1, v3, v4, v2);

						add_v3_v3(n1, nor);
						add_v3_v3(n2, nor);
						add_v3_v3(n3, nor);
						add_v3_v3(n4, nor);

						v2 = v1; v1 += 3;
						v4 = v3; v3 += 3;
						n2 = n1; n1 += 3;
						n4 = n3; n3 += 3;
					}
				}
				a = dl->parts * dl->nr;
				v1 = ndata;
				while (a--) {
					normalize_v3(v1);
					v1 += 3;
				}
			}
		}
		dl = dl->next;
	}
}

void BKE_displist_count(ListBase *lb, int *totvert, int *totface, int *tottri)
{
	DispList *dl;

	for (dl = lb->first; dl; dl = dl->next) {
		int vert_tot = 0;
		int face_tot = 0;
		int tri_tot = 0;

		switch (dl->type) {
			case DL_SURF:
			{
				vert_tot = dl->nr * dl->parts;
				face_tot = (dl->nr - 1) * (dl->parts - 1);
				tri_tot  = face_tot * 2;
				break;
			}
			case DL_INDEX3:
			{
				vert_tot = dl->nr;
				face_tot = dl->parts;
				tri_tot  = face_tot;
				break;
			}
			case DL_INDEX4:
			{
				vert_tot = dl->nr;
				face_tot = dl->parts;
				tri_tot  = face_tot * 2;
				break;
			}
			case DL_POLY:
			case DL_SEGM:
			{
				vert_tot = dl->nr * dl->parts;
				break;
			}
		}

		*totvert += vert_tot;
		*totface += face_tot;
		*tottri  += tri_tot;
	}
}

bool BKE_displist_surfindex_get(DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4)
{
	if ((dl->flag & DL_CYCL_V) == 0 && a == (dl->parts) - 1) {
		return false;
	}

	if (dl->flag & DL_CYCL_U) {
		(*p1) = dl->nr * a;
		(*p2) = (*p1) + dl->nr - 1;
		(*p3) = (*p1) + dl->nr;
		(*p4) = (*p2) + dl->nr;
		(*b) = 0;
	}
	else {
		(*p2) = dl->nr * a;
		(*p1) = (*p2) + 1;
		(*p4) = (*p2) + dl->nr;
		(*p3) = (*p1) + dl->nr;
		(*b) = 1;
	}

	if ((dl->flag & DL_CYCL_V) && a == dl->parts - 1) {
		(*p3) -= dl->nr * dl->parts;
		(*p4) -= dl->nr * dl->parts;
	}

	return true;
}

/* ****************** make displists ********************* */

static void curve_to_displist(Curve *cu, ListBase *nubase, ListBase *dispbase,
                              const bool for_render, const bool use_render_resolution)
{
	Nurb *nu;
	DispList *dl;
	BezTriple *bezt, *prevbezt;
	BPoint *bp;
	float *data;
	int a, len, resolu;
	const bool editmode = (!for_render && (cu->editnurb || cu->editfont));

	nu = nubase->first;
	while (nu) {
		if (nu->hide == 0 || editmode == false) {
			if (use_render_resolution && cu->resolu_ren != 0)
				resolu = cu->resolu_ren;
			else
				resolu = nu->resolu;

			if (!BKE_nurb_check_valid_u(nu)) {
				/* pass */
			}
			else if (nu->type == CU_BEZIER) {
				/* count */
				len = 0;
				a = nu->pntsu - 1;
				if (nu->flagu & CU_NURB_CYCLIC) a++;

				prevbezt = nu->bezt;
				bezt = prevbezt + 1;
				while (a--) {
					if (a == 0 && (nu->flagu & CU_NURB_CYCLIC))
						bezt = nu->bezt;

					if (prevbezt->h2 == HD_VECT && bezt->h1 == HD_VECT)
						len++;
					else
						len += resolu;

					if (a == 0 && (nu->flagu & CU_NURB_CYCLIC) == 0)
						len++;

					prevbezt = bezt;
					bezt++;
				}

				dl = MEM_callocN(sizeof(DispList), "makeDispListbez");
				/* len+1 because of 'forward_diff_bezier' function */
				dl->verts = MEM_mallocN((len + 1) * sizeof(float[3]), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts = 1;
				dl->nr = len;
				dl->col = nu->mat_nr;
				dl->charidx = nu->charidx;

				data = dl->verts;

				if (nu->flagu & CU_NURB_CYCLIC) {
					dl->type = DL_POLY;
					a = nu->pntsu;
				}
				else {
					dl->type = DL_SEGM;
					a = nu->pntsu - 1;
				}

				prevbezt = nu->bezt;
				bezt = prevbezt + 1;

				while (a--) {
					if (a == 0 && dl->type == DL_POLY)
						bezt = nu->bezt;

					if (prevbezt->h2 == HD_VECT && bezt->h1 == HD_VECT) {
						copy_v3_v3(data, prevbezt->vec[1]);
						data += 3;
					}
					else {
						int j;
						for (j = 0; j < 3; j++) {
							BKE_curve_forward_diff_bezier(prevbezt->vec[1][j],
							                              prevbezt->vec[2][j],
							                              bezt->vec[0][j],
							                              bezt->vec[1][j],
							                              data + j, resolu, 3 * sizeof(float));
						}

						data += 3 * resolu;
					}

					if (a == 0 && dl->type == DL_SEGM) {
						copy_v3_v3(data, bezt->vec[1]);
					}

					prevbezt = bezt;
					bezt++;
				}
			}
			else if (nu->type == CU_NURBS) {
				len = (resolu * SEGMENTSU(nu));

				dl = MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts = 1;

				dl->nr = len;
				dl->col = nu->mat_nr;
				dl->charidx = nu->charidx;

				data = dl->verts;
				if (nu->flagu & CU_NURB_CYCLIC)
					dl->type = DL_POLY;
				else dl->type = DL_SEGM;
				BKE_nurb_makeCurve(nu, data, NULL, NULL, NULL, resolu, 3 * sizeof(float));
			}
			else if (nu->type == CU_POLY) {
				len = nu->pntsu;
				dl = MEM_callocN(sizeof(DispList), "makeDispListpoly");
				dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");
				BLI_addtail(dispbase, dl);
				dl->parts = 1;
				dl->nr = len;
				dl->col = nu->mat_nr;
				dl->charidx = nu->charidx;

				data = dl->verts;
				if (nu->flagu & CU_NURB_CYCLIC) dl->type = DL_POLY;
				else dl->type = DL_SEGM;

				a = len;
				bp = nu->bp;
				while (a--) {
					copy_v3_v3(data, bp->vec);
					bp++;
					data += 3;
				}
			}
		}
		nu = nu->next;
	}
}

/**
 * \param normal_proj  Optional normal thats used to project the scanfill verts into 2d coords.
 * Pass this along if known since it saves time calculating the normal.
 * \param flipnormal  Flip the normal (same as passing \a normal_proj negated)
 */
void BKE_displist_fill(ListBase *dispbase, ListBase *to, const float normal_proj[3], const bool flipnormal)
{
	ScanFillContext sf_ctx;
	ScanFillVert *sf_vert, *sf_vert_new, *sf_vert_last;
	ScanFillFace *sf_tri;
	MemArena *sf_arena;
	DispList *dlnew = NULL, *dl;
	float *f1;
	int colnr = 0, charidx = 0, cont = 1, tot, a, *index, nextcol = 0;
	int totvert;
	const int scanfill_flag = BLI_SCANFILL_CALC_REMOVE_DOUBLES | BLI_SCANFILL_CALC_POLYS | BLI_SCANFILL_CALC_HOLES;

	if (dispbase == NULL)
		return;
	if (BLI_listbase_is_empty(dispbase))
		return;

	sf_arena = BLI_memarena_new(BLI_SCANFILL_ARENA_SIZE, __func__);

	while (cont) {
		cont = 0;
		totvert = 0;
		nextcol = 0;

		BLI_scanfill_begin_arena(&sf_ctx, sf_arena);

		dl = dispbase->first;
		while (dl) {
			if (dl->type == DL_POLY) {
				if (charidx < dl->charidx)
					cont = 1;
				else if (charidx == dl->charidx) { /* character with needed index */
					if (colnr == dl->col) {

						sf_ctx.poly_nr++;

						/* make editverts and edges */
						f1 = dl->verts;
						a = dl->nr;
						sf_vert = sf_vert_new = NULL;

						while (a--) {
							sf_vert_last = sf_vert;

							sf_vert = BLI_scanfill_vert_add(&sf_ctx, f1);
							totvert++;

							if (sf_vert_last == NULL)
								sf_vert_new = sf_vert;
							else {
								BLI_scanfill_edge_add(&sf_ctx, sf_vert_last, sf_vert);
							}
							f1 += 3;
						}

						if (sf_vert != NULL && sf_vert_new != NULL) {
							BLI_scanfill_edge_add(&sf_ctx, sf_vert, sf_vert_new);
						}
					}
					else if (colnr < dl->col) {
						/* got poly with next material at current char */
						cont = 1;
						nextcol = 1;
					}
				}
			}
			dl = dl->next;
		}

		/* XXX (obedit && obedit->actcol) ? (obedit->actcol-1) : 0)) { */
		if (totvert && (tot = BLI_scanfill_calc_ex(&sf_ctx,
		                                           scanfill_flag,
		                                           normal_proj)))
		{
			if (tot) {
				dlnew = MEM_callocN(sizeof(DispList), "filldisplist");
				dlnew->type = DL_INDEX3;
				dlnew->col = colnr;
				dlnew->nr = totvert;
				dlnew->parts = tot;

				dlnew->index = MEM_mallocN(tot * 3 * sizeof(int), "dlindex");
				dlnew->verts = MEM_mallocN(totvert * 3 * sizeof(float), "dlverts");

				/* vert data */
				f1 = dlnew->verts;
				totvert = 0;

				for (sf_vert = sf_ctx.fillvertbase.first; sf_vert; sf_vert = sf_vert->next) {
					copy_v3_v3(f1, sf_vert->co);
					f1 += 3;

					/* index number */
					sf_vert->tmp.i = totvert;
					totvert++;
				}

				/* index data */

				index = dlnew->index;
				for (sf_tri = sf_ctx.fillfacebase.first; sf_tri; sf_tri = sf_tri->next) {
					index[0] = sf_tri->v1->tmp.i;
					index[1] = sf_tri->v2->tmp.i;
					index[2] = sf_tri->v3->tmp.i;

					if (flipnormal)
						SWAP(int, index[0], index[2]);

					index += 3;
				}
			}

			BLI_addhead(to, dlnew);
		}
		BLI_scanfill_end_arena(&sf_ctx, sf_arena);

		if (nextcol) {
			/* stay at current char but fill polys with next material */
			colnr++;
		}
		else {
			/* switch to next char and start filling from first material */
			charidx++;
			colnr = 0;
		}
	}

	BLI_memarena_free(sf_arena);

	/* do not free polys, needed for wireframe display */
}

static void bevels_to_filledpoly(Curve *cu, ListBase *dispbase)
{
	const float z_up[3] = {0.0f, 0.0f, 1.0f};
	ListBase front, back;
	DispList *dl, *dlnew;
	float *fp, *fp1;
	int a, dpoly;

	BLI_listbase_clear(&front);
	BLI_listbase_clear(&back);

	dl = dispbase->first;
	while (dl) {
		if (dl->type == DL_SURF) {
			if ((dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U) == 0) {
				if ((cu->flag & CU_BACK) && (dl->flag & DL_BACK_CURVE)) {
					dlnew = MEM_callocN(sizeof(DispList), "filldisp");
					BLI_addtail(&front, dlnew);
					dlnew->verts = fp1 = MEM_mallocN(sizeof(float) * 3 * dl->parts, "filldisp1");
					dlnew->nr = dl->parts;
					dlnew->parts = 1;
					dlnew->type = DL_POLY;
					dlnew->col = dl->col;
					dlnew->charidx = dl->charidx;

					fp = dl->verts;
					dpoly = 3 * dl->nr;

					a = dl->parts;
					while (a--) {
						copy_v3_v3(fp1, fp);
						fp1 += 3;
						fp += dpoly;
					}
				}
				if ((cu->flag & CU_FRONT) && (dl->flag & DL_FRONT_CURVE)) {
					dlnew = MEM_callocN(sizeof(DispList), "filldisp");
					BLI_addtail(&back, dlnew);
					dlnew->verts = fp1 = MEM_mallocN(sizeof(float) * 3 * dl->parts, "filldisp1");
					dlnew->nr = dl->parts;
					dlnew->parts = 1;
					dlnew->type = DL_POLY;
					dlnew->col = dl->col;
					dlnew->charidx = dl->charidx;

					fp = dl->verts + 3 * (dl->nr - 1);
					dpoly = 3 * dl->nr;

					a = dl->parts;
					while (a--) {
						copy_v3_v3(fp1, fp);
						fp1 += 3;
						fp += dpoly;
					}
				}
			}
		}
		dl = dl->next;
	}

	BKE_displist_fill(&front, dispbase, z_up, true);
	BKE_displist_fill(&back, dispbase, z_up, false);

	BKE_displist_free(&front);
	BKE_displist_free(&back);

	BKE_displist_fill(dispbase, dispbase, z_up, false);
}

static void curve_to_filledpoly(Curve *cu, ListBase *UNUSED(nurb), ListBase *dispbase)
{
	if (!CU_DO_2DFILL(cu))
		return;

	if (dispbase->first && ((DispList *) dispbase->first)->type == DL_SURF) {
		bevels_to_filledpoly(cu, dispbase);
	}
	else {
		const float z_up[3] = {0.0f, 0.0f, 1.0f};
		BKE_displist_fill(dispbase, dispbase, z_up, false);
	}
}

/* taper rules:
 * - only 1 curve
 * - first point left, last point right
 * - based on subdivided points in original curve, not on points in taper curve (still)
 */
static float displist_calc_taper(Scene *scene, Object *taperobj, float fac)
{
	DispList *dl;

	if (taperobj == NULL || taperobj->type != OB_CURVE)
		return 1.0;

	dl = taperobj->curve_cache ? taperobj->curve_cache->disp.first : NULL;
	if (dl == NULL) {
		BKE_displist_make_curveTypes(scene, taperobj, 0);
		dl = taperobj->curve_cache->disp.first;
	}
	if (dl) {
		float minx, dx, *fp;
		int a;

		/* horizontal size */
		minx = dl->verts[0];
		dx = dl->verts[3 * (dl->nr - 1)] - minx;
		if (dx > 0.0f) {
			fp = dl->verts;
			for (a = 0; a < dl->nr; a++, fp += 3) {
				if ((fp[0] - minx) / dx >= fac) {
					/* interpolate with prev */
					if (a > 0) {
						float fac1 = (fp[-3] - minx) / dx;
						float fac2 = (fp[0] - minx) / dx;
						if (fac1 != fac2)
							return fp[1] * (fac1 - fac) / (fac1 - fac2) + fp[-2] * (fac - fac2) / (fac1 - fac2);
					}
					return fp[1];
				}
			}
			return fp[-2];  // last y coord
		}
	}

	return 1.0;
}

float BKE_displist_calc_taper(Scene *scene, Object *taperobj, int cur, int tot)
{
	float fac = ((float)cur) / (float)(tot - 1);

	return displist_calc_taper(scene, taperobj, fac);
}

void BKE_displist_make_mball(EvaluationContext *eval_ctx, Scene *scene, Object *ob)
{
	if (!ob || ob->type != OB_MBALL)
		return;

	if (ob == BKE_mball_basis_find(scene, ob)) {
		if (ob->curve_cache) {
			BKE_displist_free(&(ob->curve_cache->disp));
		}
		else {
			ob->curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for MBall");
		}

		BKE_mball_polygonize(eval_ctx, scene, ob, &ob->curve_cache->disp);
		BKE_mball_texspace_calc(ob);

		object_deform_mball(ob, &ob->curve_cache->disp);

		/* NOP for MBALLs anyway... */
		boundbox_displist_object(ob);
	}
}

void BKE_displist_make_mball_forRender(EvaluationContext *eval_ctx, Scene *scene, Object *ob, ListBase *dispbase)
{
	BKE_mball_polygonize(eval_ctx, scene, ob, dispbase);
	BKE_mball_texspace_calc(ob);

	object_deform_mball(ob, dispbase);
}

static ModifierData *curve_get_tessellate_point(Scene *scene, Object *ob,
                                                const bool use_render_resolution, const bool editmode)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	ModifierData *pretessellatePoint;
	int required_mode;

	if (use_render_resolution)
		required_mode = eModifierMode_Render;
	else
		required_mode = eModifierMode_Realtime;

	if (editmode)
		required_mode |= eModifierMode_Editmode;

	pretessellatePoint = NULL;
	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!modifier_isEnabled(scene, md, required_mode))
			continue;
		if (mti->type == eModifierTypeType_Constructive)
			return pretessellatePoint;

		if (ELEM(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform)) {
			pretessellatePoint = md;

			/* this modifiers are moving point of tessellation automatically
			 * (some of them even can't be applied on tessellated curve), set flag
			 * for information button in modifier's header
			 */
			md->mode |= eModifierMode_ApplyOnSpline;
		}
		else if (md->mode & eModifierMode_ApplyOnSpline) {
			pretessellatePoint = md;
		}
	}

	return pretessellatePoint;
}

static void curve_calc_modifiers_pre(Scene *scene, Object *ob, ListBase *nurb,
                                     const bool for_render, const bool use_render_resolution)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	ModifierData *pretessellatePoint;
	Curve *cu = ob->data;
	int numVerts = 0;
	const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
	ModifierApplyFlag app_flag = 0;
	float (*deformedVerts)[3] = NULL;
	float *keyVerts = NULL;
	int required_mode;

	modifiers_clearErrors(ob);

	if (editmode)
		app_flag |= MOD_APPLY_USECACHE;
	if (use_render_resolution) {
		app_flag |= MOD_APPLY_RENDER;
		required_mode = eModifierMode_Render;
	}
	else
		required_mode = eModifierMode_Realtime;

	pretessellatePoint = curve_get_tessellate_point(scene, ob, use_render_resolution, editmode);

	if (editmode)
		required_mode |= eModifierMode_Editmode;

	if (cu->editnurb == NULL) {
		keyVerts = BKE_key_evaluate_object(scene, ob, &numVerts);

		if (keyVerts) {
			/* split coords from key data, the latter also includes
			 * tilts, which is passed through in the modifier stack.
			 * this is also the reason curves do not use a virtual
			 * shape key modifier yet. */
			deformedVerts = BKE_curve_nurbs_keyVertexCos_get(nurb, keyVerts);
			BLI_assert(BKE_nurbList_verts_count(nurb) == numVerts);
		}
	}

	if (pretessellatePoint) {
		for (; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			md->scene = scene;

			if (!modifier_isEnabled(scene, md, required_mode))
				continue;
			if (mti->type != eModifierTypeType_OnlyDeform)
				continue;

			if (!deformedVerts) {
				deformedVerts = BKE_curve_nurbs_vertexCos_get(nurb, &numVerts);
			}

			mti->deformVerts(md, ob, NULL, deformedVerts, numVerts, app_flag);

			if (md == pretessellatePoint)
				break;
		}
	}

	if (deformedVerts) {
		BK_curve_nurbs_vertexCos_apply(nurb, deformedVerts);
		MEM_freeN(deformedVerts);
	}
	if (keyVerts) /* these are not passed through modifier stack */
		BKE_curve_nurbs_keyVertexTilts_apply(nurb, keyVerts);

	if (keyVerts)
		MEM_freeN(keyVerts);
}

static float (*displist_get_allverts(ListBase *dispbase, int *totvert))[3]
{
	DispList *dl;
	float (*allverts)[3], *fp;

	*totvert = 0;

	for (dl = dispbase->first; dl; dl = dl->next)
		*totvert += (dl->type == DL_INDEX3) ? dl->nr : dl->parts * dl->nr;

	allverts = MEM_mallocN((*totvert) * sizeof(float) * 3, "displist_get_allverts allverts");
	fp = (float *)allverts;
	for (dl = dispbase->first; dl; dl = dl->next) {
		int offs = 3 * ((dl->type == DL_INDEX3) ? dl->nr : dl->parts * dl->nr);
		memcpy(fp, dl->verts, sizeof(float) * offs);
		fp += offs;
	}

	return allverts;
}

static void displist_apply_allverts(ListBase *dispbase, float (*allverts)[3])
{
	DispList *dl;
	const float *fp;

	fp = (float *)allverts;
	for (dl = dispbase->first; dl; dl = dl->next) {
		int offs = 3 * ((dl->type == DL_INDEX3) ? dl->nr : dl->parts * dl->nr);
		memcpy(dl->verts, fp, sizeof(float) * offs);
		fp += offs;
	}
}

static void curve_calc_modifiers_post(Scene *scene, Object *ob, ListBase *nurb,
                                      ListBase *dispbase, DerivedMesh **r_dm_final,
                                      const bool for_render, const bool use_render_resolution)
{
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	ModifierData *pretessellatePoint;
	Curve *cu = ob->data;
	int required_mode = 0, totvert = 0;
	const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
	DerivedMesh *dm = NULL, *ndm;
	float (*vertCos)[3] = NULL;
	int useCache = !for_render;
	ModifierApplyFlag app_flag = 0;

	if (use_render_resolution) {
		app_flag |= MOD_APPLY_RENDER;
		required_mode = eModifierMode_Render;
	}
	else
		required_mode = eModifierMode_Realtime;

	pretessellatePoint = curve_get_tessellate_point(scene, ob, use_render_resolution, editmode);

	if (editmode)
		required_mode |= eModifierMode_Editmode;

	if (pretessellatePoint) {
		md = pretessellatePoint->next;
	}

	if (r_dm_final && *r_dm_final) {
		(*r_dm_final)->release(*r_dm_final);
	}

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		ModifierApplyFlag appf = app_flag;

		md->scene = scene;

		if (!modifier_isEnabled(scene, md, required_mode))
			continue;

		if (mti->type == eModifierTypeType_OnlyDeform ||
		    (mti->type == eModifierTypeType_DeformOrConstruct && !dm))
		{
			if (editmode)
				appf |= MOD_APPLY_USECACHE;
			if (dm) {
				if (!vertCos) {
					totvert = dm->getNumVerts(dm);
					vertCos = MEM_mallocN(sizeof(*vertCos) * totvert, "dfmv");
					dm->getVertCos(dm, vertCos);
				}

				mti->deformVerts(md, ob, dm, vertCos, totvert, appf);
			}
			else {
				if (!vertCos) {
					vertCos = displist_get_allverts(dispbase, &totvert);
				}

				mti->deformVerts(md, ob, NULL, vertCos, totvert, appf);
			}
		}
		else {
			if (!r_dm_final) {
				/* makeDisplistCurveTypes could be used for beveling, where derived mesh
				 * is totally unnecessary, so we could stop modifiers applying
				 * when we found constructive modifier but derived mesh is unwanted result
				 */
				break;
			}

			if (dm) {
				if (vertCos) {
					DerivedMesh *tdm = CDDM_copy(dm);
					dm->release(dm);
					dm = tdm;

					CDDM_apply_vert_coords(dm, vertCos);
				}
			}
			else {
				if (vertCos) {
					displist_apply_allverts(dispbase, vertCos);
				}

				if (ELEM(ob->type, OB_CURVE, OB_FONT) && (cu->flag & CU_DEFORM_FILL)) {
					curve_to_filledpoly(cu, nurb, dispbase);
				}

				dm = CDDM_from_curve_displist(ob, dispbase);
			}

			if (vertCos) {
				/* Vertex coordinates were applied to necessary data, could free it */
				MEM_freeN(vertCos);
				vertCos = NULL;
			}

			if (useCache)
				appf |= MOD_APPLY_USECACHE;

			ndm = modwrap_applyModifier(md, ob, dm, appf);

			if (ndm) {
				/* Modifier returned a new derived mesh */

				if (dm && dm != ndm) /* Modifier  */
					dm->release(dm);
				dm = ndm;
			}
		}
	}

	if (vertCos) {
		if (dm) {
			DerivedMesh *tdm = CDDM_copy(dm);
			dm->release(dm);
			dm = tdm;

			CDDM_apply_vert_coords(dm, vertCos);
			CDDM_calc_normals_mapping(dm);
			MEM_freeN(vertCos);
		}
		else {
			displist_apply_allverts(dispbase, vertCos);
			MEM_freeN(vertCos);
			vertCos = NULL;
		}
	}

	if (r_dm_final) {
		if (dm) {
			/* see: mesh_calc_modifiers */
			if (dm->getNumTessFaces(dm) == 0) {
				dm->recalcTessellation(dm);
			}
			/* Even if tessellation is not needed, some modifiers might have modified CD layers
			 * (like mloopcol or mloopuv), hence we have to update those. */
			else if (dm->dirty & DM_DIRTY_TESS_CDLAYERS) {
				DM_update_tessface_data(dm);
			}

			if (dm->type == DM_TYPE_CDDM) {
				CDDM_calc_normals_mapping_ex(dm, (dm->dirty & DM_DIRTY_NORMALS) ? false : true);
			}
		}
		(*r_dm_final) = dm;
	}
}

static void displist_surf_indices(DispList *dl)
{
	int a, b, p1, p2, p3, p4;
	int *index;

	dl->totindex = 0;

	index = dl->index = MEM_mallocN(4 * sizeof(int) * (dl->parts + 1) * (dl->nr + 1), "index array nurbs");

	for (a = 0; a < dl->parts; a++) {

		if (BKE_displist_surfindex_get(dl, a, &b, &p1, &p2, &p3, &p4) == 0)
			break;

		for (; b < dl->nr; b++, index += 4) {
			index[0] = p1;
			index[1] = p2;
			index[2] = p4;
			index[3] = p3;

			dl->totindex++;

			p2 = p1; p1++;
			p4 = p3; p3++;
		}
	}
}

static DerivedMesh *create_orco_dm(Scene *scene, Object *ob)
{
	DerivedMesh *dm;
	ListBase disp = {NULL, NULL};

	/* OrcoDM should be created from underformed disp lists */
	BKE_displist_make_curveTypes_forOrco(scene, ob, &disp);
	dm = CDDM_from_curve_displist(ob, &disp);

	BKE_displist_free(&disp);

	return dm;
}

static void add_orco_dm(Object *ob, DerivedMesh *dm, DerivedMesh *orcodm)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert, a;
	Curve *cu = ob->data;

	totvert = dm->getNumVerts(dm);

	orco = MEM_callocN(sizeof(float) * 3 * totvert, "dm orco");

	if (orcodm->getNumVerts(orcodm) == totvert)
		orcodm->getVertCos(orcodm, orco);
	else
		dm->getVertCos(dm, orco);

	for (a = 0; a < totvert; a++) {
		float *co = orco[a];
		co[0] = (co[0] - cu->loc[0]) / cu->size[0];
		co[1] = (co[1] - cu->loc[1]) / cu->size[1];
		co[2] = (co[2] - cu->loc[2]) / cu->size[2];
	}

	if ((layerorco = DM_get_vert_data_layer(dm, CD_ORCO))) {
		memcpy(layerorco, orco, sizeof(float) * totvert);
		MEM_freeN(orco);
	}
	else
		DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, orco);
}

static void curve_calc_orcodm(Scene *scene, Object *ob, DerivedMesh *dm_final,
                              const bool for_render, const bool use_render_resolution)
{
	/* this function represents logic of mesh's orcodm calculation
	 * for displist-based objects
	 */
	VirtualModifierData virtualModifierData;
	ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	ModifierData *pretessellatePoint;
	Curve *cu = ob->data;
	int required_mode;
	const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
	DerivedMesh *ndm, *orcodm = NULL;
	ModifierApplyFlag app_flag = MOD_APPLY_ORCO;

	if (use_render_resolution) {
		app_flag |= MOD_APPLY_RENDER;
		required_mode = eModifierMode_Render;
	}
	else
		required_mode = eModifierMode_Realtime;

	pretessellatePoint = curve_get_tessellate_point(scene, ob, use_render_resolution, editmode);

	if (editmode)
		required_mode |= eModifierMode_Editmode;

	if (pretessellatePoint) {
		md = pretessellatePoint->next;
	}

	/* If modifiers are disabled, we wouldn't be here because
	 * this function is only called if there're enabled constructive
	 * modifiers applied on the curve.
	 *
	 * This means we can create ORCO DM in advance and assume it's
	 * never NULL.
	 */
	orcodm = create_orco_dm(scene, ob);

	for (; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene = scene;

		if (!modifier_isEnabled(scene, md, required_mode))
			continue;
		if (mti->type != eModifierTypeType_Constructive)
			continue;

		ndm = modwrap_applyModifier(md, ob, orcodm, app_flag);

		if (ndm) {
			/* if the modifier returned a new dm, release the old one */
			if (orcodm && orcodm != ndm) {
				orcodm->release(orcodm);
			}
			orcodm = ndm;
		}
	}

	/* add an orco layer if needed */
	add_orco_dm(ob, dm_final, orcodm);

	orcodm->release(orcodm);
}

void BKE_displist_make_surf(Scene *scene, Object *ob, ListBase *dispbase,
                            DerivedMesh **r_dm_final,
                            const bool for_render, const bool for_orco, const bool use_render_resolution)
{
	ListBase nubase = {NULL, NULL};
	Nurb *nu;
	Curve *cu = ob->data;
	DispList *dl;
	float *data;
	int len;

	if (!for_render && cu->editnurb) {
		BKE_nurbList_duplicate(&nubase, BKE_curve_editNurbs_get(cu));
	}
	else {
		BKE_nurbList_duplicate(&nubase, &cu->nurb);
	}

	if (!for_orco)
		curve_calc_modifiers_pre(scene, ob, &nubase, for_render, use_render_resolution);

	for (nu = nubase.first; nu; nu = nu->next) {
		if ((for_render || nu->hide == 0) && BKE_nurb_check_valid_uv(nu)) {
			int resolu = nu->resolu, resolv = nu->resolv;

			if (use_render_resolution) {
				if (cu->resolu_ren)
					resolu = cu->resolu_ren;
				if (cu->resolv_ren)
					resolv = cu->resolv_ren;
			}

			if (nu->pntsv == 1) {
				len = SEGMENTSU(nu) * resolu;

				dl = MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");

				BLI_addtail(dispbase, dl);
				dl->parts = 1;
				dl->nr = len;
				dl->col = nu->mat_nr;
				dl->charidx = nu->charidx;

				/* dl->rt will be used as flag for render face and */
				/* CU_2D conflicts with R_NOPUNOFLIP */
				dl->rt = nu->flag & ~CU_2D;

				data = dl->verts;
				if (nu->flagu & CU_NURB_CYCLIC) dl->type = DL_POLY;
				else dl->type = DL_SEGM;

				BKE_nurb_makeCurve(nu, data, NULL, NULL, NULL, resolu, 3 * sizeof(float));
			}
			else {
				len = (nu->pntsu * resolu) * (nu->pntsv * resolv);

				dl = MEM_callocN(sizeof(DispList), "makeDispListsurf");
				dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");
				BLI_addtail(dispbase, dl);

				dl->col = nu->mat_nr;
				dl->charidx = nu->charidx;

				/* dl->rt will be used as flag for render face and */
				/* CU_2D conflicts with R_NOPUNOFLIP */
				dl->rt = nu->flag & ~CU_2D;

				data = dl->verts;
				dl->type = DL_SURF;

				dl->parts = (nu->pntsu * resolu);  /* in reverse, because makeNurbfaces works that way */
				dl->nr = (nu->pntsv * resolv);
				if (nu->flagv & CU_NURB_CYCLIC) dl->flag |= DL_CYCL_U;  /* reverse too! */
				if (nu->flagu & CU_NURB_CYCLIC) dl->flag |= DL_CYCL_V;

				BKE_nurb_makeFaces(nu, data, 0, resolu, resolv);

				/* gl array drawing: using indices */
				displist_surf_indices(dl);
			}
		}
	}

	if (!for_orco) {
		BKE_nurbList_duplicate(&ob->curve_cache->deformed_nurbs, &nubase);
		curve_calc_modifiers_post(scene, ob, &nubase, dispbase, r_dm_final,
		                          for_render, use_render_resolution);
	}

	BKE_nurbList_free(&nubase);
}

static void rotateBevelPiece(Curve *cu, BevPoint *bevp, BevPoint *nbevp, DispList *dlb, float bev_blend, float widfac, float fac, float **r_data)
{
	float *fp, *data = *r_data;
	int b;

	fp = dlb->verts;
	for (b = 0; b < dlb->nr; b++, fp += 3, data += 3) {
		if (cu->flag & CU_3D) {
			float vec[3], quat[4];

			vec[0] = fp[1] + widfac;
			vec[1] = fp[2];
			vec[2] = 0.0;

			if (nbevp == NULL) {
				copy_v3_v3(data, bevp->vec);
				copy_qt_qt(quat, bevp->quat);
			}
			else {
				interp_v3_v3v3(data, bevp->vec, nbevp->vec, bev_blend);
				interp_qt_qtqt(quat, bevp->quat, nbevp->quat, bev_blend);
			}

			mul_qt_v3(quat, vec);

			data[0] += fac * vec[0];
			data[1] += fac * vec[1];
			data[2] += fac * vec[2];
		}
		else {
			float sina, cosa;

			if (nbevp == NULL) {
				copy_v3_v3(data, bevp->vec);
				sina = bevp->sina;
				cosa = bevp->cosa;
			}
			else {
				interp_v3_v3v3(data, bevp->vec, nbevp->vec, bev_blend);

				/* perhaps we need to interpolate angles instead. but the thing is
				 * cosa and sina are not actually sine and cosine
				 */
				sina = nbevp->sina * bev_blend + bevp->sina * (1.0f - bev_blend);
				cosa = nbevp->cosa * bev_blend + bevp->cosa * (1.0f - bev_blend);
			}

			data[0] += fac * (widfac + fp[1]) * sina;
			data[1] += fac * (widfac + fp[1]) * cosa;
			data[2] += fac * fp[2];
		}
	}

	*r_data = data;
}

static void fillBevelCap(Nurb *nu, DispList *dlb, float *prev_fp, ListBase *dispbase)
{
	DispList *dl;

	dl = MEM_callocN(sizeof(DispList), "makeDispListbev2");
	dl->verts = MEM_mallocN(sizeof(float[3]) * dlb->nr, "dlverts");
	memcpy(dl->verts, prev_fp, 3 * sizeof(float) * dlb->nr);

	dl->type = DL_POLY;

	dl->parts = 1;
	dl->nr = dlb->nr;
	dl->col = nu->mat_nr;
	dl->charidx = nu->charidx;

	/* dl->rt will be used as flag for render face and */
	/* CU_2D conflicts with R_NOPUNOFLIP */
	dl->rt = nu->flag & ~CU_2D;

	BLI_addtail(dispbase, dl);
}

static void calc_bevfac_segment_mapping(BevList *bl, float bevfac, float spline_length, int *r_bev, float *r_blend)
{
	float normlen, normsum = 0.0f;
	float *seglen = bl->seglen;
	int *segbevcount = bl->segbevcount;
	int bevcount = 0, nr = bl->nr;

	float bev_fl = bevfac * (bl->nr - 1);
	*r_bev = (int)bev_fl;

	while (bevcount < nr - 1) {
		normlen = *seglen / spline_length;
		if (normsum + normlen > bevfac) {
			bev_fl = bevcount + (bevfac - normsum) / normlen * *segbevcount;
			*r_bev = (int) bev_fl;
			*r_blend = bev_fl - *r_bev;
			break;
		}
		normsum += normlen;
		bevcount += *segbevcount;
		segbevcount++;
		seglen++;
	}
}

static void calc_bevfac_spline_mapping(BevList *bl, float bevfac,
                                       float spline_length,
                                       int *r_bev, float *r_blend)
{
	const float len_target = bevfac * spline_length;
	BevPoint *bevp = bl->bevpoints;
	float len_next = 0.0f, len = 0.0f;
	int i = 0, nr = bl->nr;

	while (nr--) {
		bevp++;
		len_next = len + bevp->offset;
		if (len_next > len_target) {
			break;
		}
		len = len_next;
		i++;
	}

	*r_bev = i;
	*r_blend = (len_target - len) / bevp->offset;
}

static void calc_bevfac_mapping_default(BevList *bl,
                                        int *r_start, float *r_firstblend,
                                        int *r_steps, float *r_lastblend)
{
	*r_start = 0;
	*r_steps = bl->nr;
	*r_firstblend = 1.0f;
	*r_lastblend = 1.0f;
}

static void calc_bevfac_mapping(Curve *cu, BevList *bl, Nurb *nu,
        int *r_start, float *r_firstblend, int *r_steps, float *r_lastblend)
{
	float tmpf, total_length = 0.0f;
	int end = 0, i;

	if ((BKE_nurb_check_valid_u(nu) == false) ||
	    /* not essential, but skips unnecessary calculation */
	    (min_ff(cu->bevfac1, cu->bevfac2) == 0.0f &&
	     max_ff(cu->bevfac1, cu->bevfac2) == 1.0f))
	{
		calc_bevfac_mapping_default(bl, r_start, r_firstblend, r_steps, r_lastblend);
		return;
	}

	if (ELEM(cu->bevfac1_mapping,
	         CU_BEVFAC_MAP_SEGMENT,
	         CU_BEVFAC_MAP_SPLINE))
	{
		for (i = 0; i < SEGMENTSU(nu); i++) {
			total_length += bl->seglen[i];
		}
	}

	switch (cu->bevfac1_mapping) {
		case CU_BEVFAC_MAP_RESOLU:
		{
			const float start_fl = cu->bevfac1 * (bl->nr - 1);
			*r_start = (int)start_fl;
			*r_firstblend = 1.0f - (start_fl - (*r_start));
			break;
		}
		case CU_BEVFAC_MAP_SEGMENT:
		{
			calc_bevfac_segment_mapping(bl, cu->bevfac1, total_length, r_start, r_firstblend);
			*r_firstblend = 1.0f - *r_firstblend;
			break;
		}
		case CU_BEVFAC_MAP_SPLINE:
		{
			calc_bevfac_spline_mapping(bl, cu->bevfac1, total_length, r_start, r_firstblend);
			*r_firstblend = 1.0f - *r_firstblend;
			break;
		}
	}

	switch (cu->bevfac2_mapping) {
		case CU_BEVFAC_MAP_RESOLU:
		{
			const float end_fl = cu->bevfac2 * (bl->nr - 1);
			end = (int)end_fl;

			*r_steps = 2 + end - *r_start;
			*r_lastblend = end_fl - end;
			break;
		}
		case CU_BEVFAC_MAP_SEGMENT:
		{
			calc_bevfac_segment_mapping(bl, cu->bevfac2, total_length, &end, r_lastblend);
			*r_steps = end - *r_start + 2;
			break;
		}
		case CU_BEVFAC_MAP_SPLINE:
		{
			calc_bevfac_spline_mapping(bl, cu->bevfac2, total_length, &end, r_lastblend);
			*r_steps = end - *r_start + 2;
			break;
		}
	}

	if (end < *r_start || (end == *r_start && *r_lastblend < 1.0f - *r_firstblend )) {
		SWAP(int, *r_start, end);
		tmpf = *r_lastblend;
		*r_lastblend = 1.0f - *r_firstblend;
		*r_firstblend = 1.0f - tmpf;
		*r_steps = end - *r_start + 2;
	}

	if (*r_start + *r_steps > bl->nr) {
		*r_steps = bl->nr - *r_start;
		*r_lastblend = 1.0f;
	}
}

static void do_makeDispListCurveTypes(Scene *scene, Object *ob, ListBase *dispbase,
                                      DerivedMesh **r_dm_final,
                                      const bool for_render, const bool for_orco, const bool use_render_resolution)
{
	Curve *cu = ob->data;

	/* we do allow duplis... this is only displist on curve level */
	if (!ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) return;

	if (ob->type == OB_SURF) {
		BKE_displist_make_surf(scene, ob, dispbase, r_dm_final, for_render, for_orco, use_render_resolution);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
		ListBase dlbev;
		ListBase nubase = {NULL, NULL};

		BKE_curve_bevelList_free(&ob->curve_cache->bev);

		/* We only re-evlauate path if evaluation is not happening for orco.
		 * If the calculation happens for orco, we should never free data which
		 * was needed before and only not needed for orco calculation.
		 */
		if (!for_orco) {
			if (ob->curve_cache->path) free_path(ob->curve_cache->path);
			ob->curve_cache->path = NULL;
		}

		if (ob->type == OB_FONT) {
			BKE_vfont_to_curve_nubase(G.main, ob, FO_EDIT, &nubase);
		}
		else {
			BKE_nurbList_duplicate(&nubase, BKE_curve_nurbs_get(cu));
		}

		if (!for_orco)
			curve_calc_modifiers_pre(scene, ob, &nubase, for_render, use_render_resolution);

		BKE_curve_bevelList_make(ob, &nubase, for_render != false);

		/* If curve has no bevel will return nothing */
		BKE_curve_bevel_make(scene, ob, &dlbev, for_render, use_render_resolution);

		/* no bevel or extrude, and no width correction? */
		if (!dlbev.first && cu->width == 1.0f) {
			curve_to_displist(cu, &nubase, dispbase, for_render, use_render_resolution);
		}
		else {
			float widfac = cu->width - 1.0f;
			BevList *bl = ob->curve_cache->bev.first;
			Nurb *nu = nubase.first;

			for (; bl && nu; bl = bl->next, nu = nu->next) {
				DispList *dl;
				float *data;
				int a;

				if (bl->nr) { /* blank bevel lists can happen */

					/* exception handling; curve without bevel or extrude, with width correction */
					if (BLI_listbase_is_empty(&dlbev)) {
						BevPoint *bevp;
						dl = MEM_callocN(sizeof(DispList), "makeDispListbev");
						dl->verts = MEM_mallocN(sizeof(float[3]) * bl->nr, "dlverts");
						BLI_addtail(dispbase, dl);

						if (bl->poly != -1) dl->type = DL_POLY;
						else dl->type = DL_SEGM;

						if (dl->type == DL_SEGM) dl->flag = (DL_FRONT_CURVE | DL_BACK_CURVE);

						dl->parts = 1;
						dl->nr = bl->nr;
						dl->col = nu->mat_nr;
						dl->charidx = nu->charidx;

						/* dl->rt will be used as flag for render face and */
						/* CU_2D conflicts with R_NOPUNOFLIP */
						dl->rt = nu->flag & ~CU_2D;

						a = dl->nr;
						bevp = bl->bevpoints;
						data = dl->verts;
						while (a--) {
							data[0] = bevp->vec[0] + widfac * bevp->sina;
							data[1] = bevp->vec[1] + widfac * bevp->cosa;
							data[2] = bevp->vec[2];
							bevp++;
							data += 3;
						}
					}
					else {
						DispList *dlb;
						ListBase bottom_capbase = {NULL, NULL};
						ListBase top_capbase = {NULL, NULL};
						float bottom_no[3] = {0.0f};
						float top_no[3] = {0.0f};
						float firstblend = 0.0f, lastblend = 0.0f;
						int i, start, steps;

						if (nu->flagu & CU_NURB_CYCLIC) {
							calc_bevfac_mapping_default(bl,
							                            &start, &firstblend, &steps, &lastblend);
						}
						else {
							if (fabsf(cu->bevfac2 - cu->bevfac1) < FLT_EPSILON) {
								continue;
							}

							calc_bevfac_mapping(cu, bl, nu, &start, &firstblend, &steps, &lastblend);
						}

						for (dlb = dlbev.first; dlb; dlb = dlb->next) {
							BevPoint *bevp_first, *bevp_last;
							BevPoint *bevp;

							/* for each part of the bevel use a separate displblock */
							dl = MEM_callocN(sizeof(DispList), "makeDispListbev1");
							dl->verts = data = MEM_mallocN(sizeof(float[3]) * dlb->nr * steps, "dlverts");
							BLI_addtail(dispbase, dl);

							dl->type = DL_SURF;

							dl->flag = dlb->flag & (DL_FRONT_CURVE | DL_BACK_CURVE);
							if (dlb->type == DL_POLY) dl->flag |= DL_CYCL_U;
							if (bl->poly >= 0) dl->flag |= DL_CYCL_V;

							dl->parts = steps;
							dl->nr = dlb->nr;
							dl->col = nu->mat_nr;
							dl->charidx = nu->charidx;

							/* dl->rt will be used as flag for render face and */
							/* CU_2D conflicts with R_NOPUNOFLIP */
							dl->rt = nu->flag & ~CU_2D;

							dl->bevelSplitFlag = MEM_callocN(sizeof(*dl->bevelSplitFlag) * ((steps + 0x1F) >> 5),
							                                 "bevelSplitFlag");

							/* for each point of poly make a bevel piece */
							bevp_first =  bl->bevpoints;
							bevp_last  = &bl->bevpoints[bl->nr - 1];
							bevp       = &bl->bevpoints[start];
							for (i = start, a = 0; a < steps; i++, bevp++, a++) {
								float fac = 1.0;
								float *cur_data = data;

								if (cu->taperobj == NULL) {
									fac = bevp->radius;
								}
								else {
									float len, taper_fac;

									if (cu->flag & CU_MAP_TAPER) {
										len = (steps - 3) + firstblend + lastblend;

										if (a == 0)
											taper_fac = 0.0f;
										else if (a == steps - 1)
											taper_fac = 1.0f;
										else
											taper_fac = ((float) a - (1.0f - firstblend)) / len;
									}
									else {
										len = bl->nr - 1;
										taper_fac = (float) i / len;

										if (a == 0)
											taper_fac += (1.0f - firstblend) / len;
										else if (a == steps - 1)
											taper_fac -= (1.0f - lastblend) / len;
									}

									fac = displist_calc_taper(scene, cu->taperobj, taper_fac);
								}

								if (bevp->split_tag) {
									dl->bevelSplitFlag[a >> 5] |= 1 << (a & 0x1F);
								}

								/* rotate bevel piece and write in data */
								if ((a == 0) && (bevp != bevp_last)) {
									rotateBevelPiece(cu, bevp, bevp + 1, dlb, 1.0f - firstblend, widfac, fac, &data);
								}
								else if ((a == steps - 1) && (bevp != bevp_first) ) {
									rotateBevelPiece(cu, bevp, bevp - 1, dlb, 1.0f - lastblend, widfac, fac, &data);
								}
								else {
									rotateBevelPiece(cu, bevp, NULL, dlb, 0.0f, widfac, fac, &data);
								}

								if (cu->bevobj && (cu->flag & CU_FILL_CAPS) && !(nu->flagu & CU_NURB_CYCLIC)) {
									if (a == 1) {
										fillBevelCap(nu, dlb, cur_data - 3 * dlb->nr, &bottom_capbase);
										negate_v3_v3(bottom_no, bevp->dir);
									}
									if (a == steps - 1) {
										fillBevelCap(nu, dlb, cur_data, &top_capbase);
										copy_v3_v3(top_no, bevp->dir);
									}
								}
							}

							/* gl array drawing: using indices */
							displist_surf_indices(dl);
						}

						if (bottom_capbase.first) {
							BKE_displist_fill(&bottom_capbase, dispbase, bottom_no, false);
							BKE_displist_fill(&top_capbase, dispbase, top_no, false);
							BKE_displist_free(&bottom_capbase);
							BKE_displist_free(&top_capbase);
						}
					}
				}

			}
			BKE_displist_free(&dlbev);
		}

		if (!(cu->flag & CU_DEFORM_FILL)) {
			curve_to_filledpoly(cu, &nubase, dispbase);
		}

		if (!for_orco) {
			if ((cu->flag & CU_PATH) ||
			    DAG_get_eval_flags_for_object(scene, ob) & DAG_EVAL_NEED_CURVE_PATH)
			{
				calc_curvepath(ob, &nubase);
			}
		}

		if (!for_orco) {
			BKE_nurbList_duplicate(&ob->curve_cache->deformed_nurbs, &nubase);
			curve_calc_modifiers_post(scene, ob, &nubase, dispbase, r_dm_final, for_render, use_render_resolution);
		}

		if (cu->flag & CU_DEFORM_FILL && !ob->derivedFinal) {
			curve_to_filledpoly(cu, &nubase, dispbase);
		}

		BKE_nurbList_free(&nubase);
	}
}

void BKE_displist_make_curveTypes(Scene *scene, Object *ob, const bool for_orco)
{
	ListBase *dispbase;

	/* The same check for duplis as in do_makeDispListCurveTypes.
	 * Happens when curve used for constraint/bevel was converted to mesh.
	 * check there is still needed for render displist and orco displists. */
	if (!ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT))
		return;

	BKE_object_free_derived_caches(ob);

	if (!ob->curve_cache) {
		ob->curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for curve types");
	}

	dispbase = &(ob->curve_cache->disp);

	do_makeDispListCurveTypes(scene, ob, dispbase, &ob->derivedFinal, 0, for_orco, 0);

	boundbox_displist_object(ob);
}

void BKE_displist_make_curveTypes_forRender(Scene *scene, Object *ob, ListBase *dispbase,
                                            DerivedMesh **r_dm_final, const bool for_orco,
                                            const bool use_render_resolution)
{
	if (ob->curve_cache == NULL) {
		ob->curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for Curve");
	}

	do_makeDispListCurveTypes(scene, ob, dispbase, r_dm_final, true, for_orco, use_render_resolution);
}

void BKE_displist_make_curveTypes_forOrco(struct Scene *scene, struct Object *ob, struct ListBase *dispbase)
{
	if (ob->curve_cache == NULL) {
		ob->curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for Curve");
	}

	do_makeDispListCurveTypes(scene, ob, dispbase, NULL, 1, 1, 1);
}

/* add Orco layer to the displist object which has got derived mesh and return orco */
float *BKE_displist_make_orco(Scene *scene, Object *ob, DerivedMesh *dm_final,
                              const bool for_render,
                              const bool use_render_resolution)
{
	float *orco;

	if (dm_final == NULL)
		dm_final = ob->derivedFinal;

	if (!dm_final->getVertDataArray(dm_final, CD_ORCO)) {
		curve_calc_orcodm(scene, ob, dm_final, for_render, use_render_resolution);
	}

	orco = dm_final->getVertDataArray(dm_final, CD_ORCO);

	if (orco) {
		orco = MEM_dupallocN(orco);
	}

	return orco;
}

void BKE_displist_minmax(ListBase *dispbase, float min[3], float max[3])
{
	DispList *dl;
	const float *vert;
	int a, tot = 0;
	int doit = 0;

	for (dl = dispbase->first; dl; dl = dl->next) {
		tot = (dl->type == DL_INDEX3) ? dl->nr : dl->nr * dl->parts;
		vert = dl->verts;
		for (a = 0; a < tot; a++, vert += 3) {
			minmax_v3v3_v3(min, max, vert);
		}
		doit |= (tot != 0);
	}

	if (!doit) {
		/* there's no geometry in displist, use zero-sized boundbox */
		zero_v3(min);
		zero_v3(max);
	}
}

/* this is confusing, there's also min_max_object, appplying the obmat... */
static void boundbox_displist_object(Object *ob)
{
	if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		/* Curve's BB is already calculated as a part of modifier stack,
		 * here we only calculate object BB based on final display list.
		 */

		/* object's BB is calculated from final displist */
		if (ob->bb == NULL)
			ob->bb = MEM_callocN(sizeof(BoundBox), "boundbox");

		if (ob->derivedFinal) {
			DM_set_object_boundbox(ob, ob->derivedFinal);
		}
		else {
			float min[3], max[3];

			INIT_MINMAX(min, max);
			BKE_displist_minmax(&ob->curve_cache->disp, min, max);
			BKE_boundbox_init_from_minmax(ob->bb, min, max);
		}
	}
}
