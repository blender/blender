/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"

#include <math.h>
#include <string.h>

/* MULTIRES MODIFIER */
static const int multires_max_levels = 13;
static const int multires_quad_tot[] = {4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_side_tot[] = {2, 3, 5,  9,  17,  33,   65,   129,   257,   513,    1025,    2049,    4097};

MultiresModifierData *find_multires_modifier(Object *ob)
{
	ModifierData *md;
	MultiresModifierData *mmd = NULL;

	for(md = ob->modifiers.first; md; md = md->next) {
		if(md->type == eModifierType_Multires) {
			mmd = (MultiresModifierData*)md;
			break;
		}
	}

	return mmd;

}

int multiresModifier_switch_level(Object *ob, const int distance)
{
	MultiresModifierData *mmd = find_multires_modifier(ob);

	if(mmd) {
		mmd->lvl += distance;
		if(mmd->lvl < 1) mmd->lvl = 1;
		else if(mmd->lvl > mmd->totlvl) mmd->lvl = mmd->totlvl;
		/* XXX: DAG_id_flush_update(&ob->id, OB_RECALC_DATA); 
		   object_handle_update(ob);*/
		return 1;
	}
	else
		return 0;
}

/* XXX */
#if 0
void multiresModifier_join(Object *ob)
{
	Base *base = NULL;
	int highest_lvl = 0;

	/* First find the highest level of subdivision */
	base = FIRSTBASE;
	while(base) {
		if(TESTBASELIB_BGMODE(v3d, scene, base) && base->object->type==OB_MESH) {
			ModifierData *md;
			for(md = base->object->modifiers.first; md; md = md->next) {
				if(md->type == eModifierType_Multires) {
					int totlvl = ((MultiresModifierData*)md)->totlvl;
					if(totlvl > highest_lvl)
						highest_lvl = totlvl;

					/* Ensure that all updates are processed */
					multires_force_update(base->object);
				}
			}
		}
		base = base->next;
	}

	/* No multires meshes selected */
	if(highest_lvl == 0)
		return;

	/* Subdivide all the displacements to the highest level */
	base = FIRSTBASE;
	while(base) {
		if(TESTBASELIB_BGMODE(v3d, scene, base) && base->object->type==OB_MESH) {
			ModifierData *md = NULL;
			MultiresModifierData *mmd = NULL;

			for(md = base->object->modifiers.first; md; md = md->next) {
				if(md->type == eModifierType_Multires)
					mmd = (MultiresModifierData*)md;
			}

			/* If the object didn't have multires enabled, give it a new modifier */
			if(!mmd) {
				md = base->object->modifiers.first;
				
				while(md && modifierType_getInfo(md->type)->type == eModifierTypeType_OnlyDeform)
					md = md->next;
				
				mmd = (MultiresModifierData*)modifier_new(eModifierType_Multires);
				BLI_insertlinkbefore(&base->object->modifiers, md, mmd);
				modifier_unique_name(&base->object->modifiers, mmd);
			}

			if(mmd)
				multiresModifier_subdivide(mmd, base->object, highest_lvl - mmd->totlvl, 0, 0);
		}
		base = base->next;
	}
}
#endif

/* Returns 0 on success, 1 if the src's totvert doesn't match */
int multiresModifier_reshape(MultiresModifierData *mmd, Object *dst, Object *src)
{
	Mesh *src_me = get_mesh(src);
	DerivedMesh *mrdm = dst->derivedFinal;

	if(mrdm && mrdm->getNumVerts(mrdm) == src_me->totvert) {
		MVert *mvert = CDDM_get_verts(mrdm);
		int i;

		for(i = 0; i < src_me->totvert; ++i)
			VecCopyf(mvert[i].co, src_me->mvert[i].co);
		mrdm->needsFree = 1;
		MultiresDM_mark_as_modified(mrdm);
		mrdm->release(mrdm);
		dst->derivedFinal = NULL;

		return 0;
	}

	return 1;
}

static void Mat3FromColVecs(float mat[][3], float v1[3], float v2[3], float v3[3])
{
	VecCopyf(mat[0], v1);
	VecCopyf(mat[1], v2);
	VecCopyf(mat[2], v3);
}

static DerivedMesh *multires_subdisp_pre(DerivedMesh *mrdm, int distance, int simple)
{
	DerivedMesh *final;
	SubsurfModifierData smd;

	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = distance;
	if(simple)
		smd.subdivType = ME_SIMPLE_SUBSURF;

	final = subsurf_make_derived_from_derived_with_multires(mrdm, &smd, NULL, 0, NULL, 0, 0);

	return final;
}

static void VecAddUf(float a[3], float b[3])
{
	a[0] += b[0];
	a[1] += b[1];
	a[2] += b[2];
}

static void multires_subdisp(DerivedMesh *orig, Object *ob, DerivedMesh *final, int lvl, int totlvl,
			     int totsubvert, int totsubedge, int totsubface, int addverts)
{
	DerivedMesh *mrdm;
	Mesh *me = ob->data;
	MultiresModifierData mmd_sub;
	MVert *mvs = CDDM_get_verts(final);
	MVert *mvd, *mvd_f1, *mvs_f1, *mvd_f3, *mvd_f4;
	MVert *mvd_f2, *mvs_f2, *mvs_e1, *mvd_e1, *mvs_e2;
	int totvert;
	int slo1 = multires_side_tot[lvl - 1];
	int sll = slo1 / 2;
	int slo2 = multires_side_tot[totlvl - 2];
	int shi2 = multires_side_tot[totlvl - 1];
	int skip = multires_side_tot[totlvl - lvl] - 1;
	int i, j, k;

	memset(&mmd_sub, 0, sizeof(MultiresModifierData));
	mmd_sub.lvl = mmd_sub.totlvl = totlvl;
	mrdm = multires_dm_create_from_derived(&mmd_sub, 1, orig, ob, 0, 0);
		
	mvd = CDDM_get_verts(mrdm);
	/* Need to map from ccg to mrdm */
	totvert = mrdm->getNumVerts(mrdm);

	if(!addverts) {
		for(i = 0; i < totvert; ++i) {
			float z[3] = {0,0,0};
			VecCopyf(mvd[i].co, z);
		}
	}

	/* Load base verts */
	for(i = 0; i < me->totvert; ++i)
		VecAddUf(mvd[totvert - me->totvert + i].co, mvs[totvert - me->totvert + i].co);

	mvd_f1 = mvd;
	mvs_f1 = mvs;
	mvd_f2 = mvd;
	mvs_f2 = mvs + totvert - totsubvert;
	mvs_e1 = mvs + totsubface * (skip-1) * (skip-1);

	for(i = 0; i < me->totface; ++i) {
		const int end = me->mface[i].v4 ? 4 : 3;
		int x, y, x2, y2, mov= 0;

		mvd_f1 += 1 + end * (slo2-2); //center+edgecross
		mvd_f3 = mvd_f4 = mvd_f1;

		for(j = 0; j < end; ++j) {
			mvd_f1 += (skip/2 - 1) * (slo2 - 2) + (skip/2 - 1);
			/* Update sub faces */
			for(y = 0; y < sll; ++y) {
				for(x = 0; x < sll; ++x) {
					/* Face center */
					VecAddUf(mvd_f1->co, mvs_f1->co);
					mvs_f1 += 1;

					/* Now we hold the center of the subface at mvd_f1
					   and offset it to the edge cross and face verts */

					/* Edge cross */
					for(k = 0; k < 4; ++k) {
						if(k == 0) mov = -1;
						else if(k == 1) mov = slo2 - 2;
						else if(k == 2) mov = 1;
						else if(k == 3) mov = -(slo2 - 2);

						for(x2 = 1; x2 < skip/2; ++x2) {
							VecAddUf((mvd_f1 + mov * x2)->co, mvs_f1->co);
							++mvs_f1;
						}
					}

					/* Main face verts */
					for(k = 0; k < 4; ++k) {
						int movx= 0, movy= 0;

						if(k == 0) { movx = -1; movy = -(slo2 - 2); }
						else if(k == 1) { movx = slo2 - 2; movy = -1; }
						else if(k == 2) { movx = 1; movy = slo2 - 2; }
						else if(k == 3) { movx = -(slo2 - 2); movy = 1; }

						for(y2 = 1; y2 < skip/2; ++y2) {
							for(x2 = 1; x2 < skip/2; ++x2) {
								VecAddUf((mvd_f1 + movy * y2 + movx * x2)->co, mvs_f1->co);
								++mvs_f1;
							}
						}
					}
							
					mvd_f1 += skip;
				}
				mvd_f1 += (skip - 1) * (slo2 - 2) - 1;
			}
			mvd_f1 -= (skip - 1) * (slo2 - 2) - 1 + skip;
			mvd_f1 += (slo2 - 2) * (skip/2-1) + skip/2-1 + 1;
		}

		/* update face center verts */
		VecAddUf(mvd_f2->co, mvs_f2->co);

		mvd_f2 += 1;
		mvs_f2 += 1;

		/* update face edge verts */
		for(j = 0; j < end; ++j) {
			MVert *restore;

			/* Super-face edge cross */
			for(k = 0; k < skip-1; ++k) {
				VecAddUf(mvd_f2->co, mvs_e1->co);
				mvd_f2++;
				mvs_e1++;
			}
			for(x = 1; x < sll; ++x) {
				VecAddUf(mvd_f2->co, mvs_f2->co);
				mvd_f2++;
				mvs_f2++;

				for(k = 0; k < skip-1; ++k) {
					VecAddUf(mvd_f2->co, mvs_e1->co);
					mvd_f2++;
					mvs_e1++;
				}
			}

			restore = mvs_e1;
			for(y = 0; y < sll - 1; ++y) {
				for(x = 0; x < sll; ++x) {
					for(k = 0; k < skip - 1; ++k) {
						VecAddUf(mvd_f3[(skip-1)+(y*skip) + (x*skip+k)*(slo2-2)].co,
							 mvs_e1->co);
						++mvs_e1;
					}
					mvs_e1 += skip-1;
				}
			}
			
			mvs_e1 = restore + skip - 1;
			for(y = 0; y < sll - 1; ++y) {
				for(x = 0; x < sll; ++x) {
					for(k = 0; k < skip - 1; ++k) {
						VecAddUf(mvd_f3[(slo2-2)*(skip-1)+(x*skip)+k + y*skip*(slo2-2)].co,
							 mvs_e1->co);
						++mvs_e1;
					}
					mvs_e1 += skip - 1;
				}
			}

			mvd_f3 += (slo2-2)*(slo2-2);
			mvs_e1 -= skip - 1;
		}

		/* update base (2) face verts */
		for(j = 0; j < end; ++j) {
			mvd_f2 += (slo2 - 1) * (skip - 1);
			for(y = 0; y < sll - 1; ++y) {
				for(x = 0; x < sll - 1; ++x) {
					VecAddUf(mvd_f2->co, mvs_f2->co);
					mvd_f2 += skip;
					++mvs_f2;
				}
				mvd_f2 += (slo2 - 1) * (skip - 1);
			}
			mvd_f2 -= (skip - 1);
		}
	}

	/* edges */
	mvd_e1 = mvd + totvert - me->totvert - me->totedge * (shi2-2);
	mvs_e2 = mvs + totvert - me->totvert - me->totedge * (slo1-2);
	for(i = 0; i < me->totedge; ++i) {
		for(j = 0; j < skip - 1; ++j) {
			VecAddUf(mvd_e1->co, mvs_e1->co);
			mvd_e1++;
			mvs_e1++;
		}
		for(j = 0; j < slo1 - 2; j++) {
			VecAddUf(mvd_e1->co, mvs_e2->co);
			mvd_e1++;
			mvs_e2++;
			
			for(k = 0; k < skip - 1; ++k) {
				VecAddUf(mvd_e1->co, mvs_e1->co);
				mvd_e1++;
				mvs_e1++;
			}
		}
	}

	final->needsFree  = 1;
	final->release(final);
	mrdm->needsFree = 1;
	MultiresDM_mark_as_modified(mrdm);
	mrdm->release(mrdm);
}

/* direction=1 for delete higher, direction=0 for lower (not implemented yet) */
void multiresModifier_del_levels(struct MultiresModifierData *mmd, struct Object *ob, int direction)
{
	Mesh *me = get_mesh(ob);
	int distance = mmd->totlvl - mmd->lvl;
	MDisps *mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);

	multires_force_update(ob);

	if(mdisps && distance > 0 && direction == 1) {
		int skip = multires_side_tot[distance] - 1;
		int st = multires_side_tot[mmd->totlvl - 1];
		int totdisp = multires_quad_tot[mmd->lvl - 1];
		int i, j, x, y;

		for(i = 0; i < me->totface; ++i) {
			float (*disps)[3] = MEM_callocN(sizeof(float) * 3 * totdisp, "multires del disps");
			
			for(j = 0, y = 0; y < st; y += skip) {
				for(x = 0; x < st; x += skip) {
					VecCopyf(disps[j], mdisps[i].disps[y * st + x]);
					++j;
				}
			}

			MEM_freeN(mdisps[i].disps);
			mdisps[i].disps = disps;
			mdisps[i].totdisp = totdisp;
		}
	}

	mmd->totlvl = mmd->lvl;
}

void multiresModifier_subdivide(MultiresModifierData *mmd, Object *ob, int distance, int updateblock, int simple)
{
	DerivedMesh *final = NULL;
	int totsubvert = 0, totsubface = 0, totsubedge = 0;
	Mesh *me = get_mesh(ob);
	MDisps *mdisps;
	int i;

	if(distance == 0)
		return;

	if(mmd->totlvl > multires_max_levels)
		mmd->totlvl = multires_max_levels;
	if(mmd->lvl > multires_max_levels)
		mmd->lvl = multires_max_levels;

	multires_force_update(ob);

	mmd->lvl = mmd->totlvl;
	mmd->totlvl += distance;

	mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);
	if(!mdisps)
		mdisps = CustomData_add_layer(&me->fdata, CD_MDISPS, CD_DEFAULT, NULL, me->totface);

	if(mdisps->disps && !updateblock && mmd->totlvl > 2) {
		DerivedMesh *orig, *mrdm;
		MultiresModifierData mmd_sub;

		orig = CDDM_from_mesh(me, NULL);
		memset(&mmd_sub, 0, sizeof(MultiresModifierData));
		mmd_sub.lvl = mmd_sub.totlvl = mmd->lvl;
		mmd_sub.simple = simple;
		mrdm = multires_dm_create_from_derived(&mmd_sub, 1, orig, ob, 0, 0);
		totsubvert = mrdm->getNumVerts(mrdm);
		totsubedge = mrdm->getNumEdges(mrdm);
		totsubface = mrdm->getNumFaces(mrdm);
		orig->needsFree = 1;
		orig->release(orig);
		
		final = multires_subdisp_pre(mrdm, distance, simple);
		mrdm->needsFree = 1;
		mrdm->release(mrdm);
	}

	for(i = 0; i < me->totface; ++i) {
		const int totdisp = multires_quad_tot[mmd->totlvl - 1];
		float (*disps)[3] = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disps");

		if(mdisps[i].disps)
			MEM_freeN(mdisps[i].disps);

		mdisps[i].disps = disps;
		mdisps[i].totdisp = totdisp;
	}


	if(final) {
		DerivedMesh *orig;

		orig = CDDM_from_mesh(me, NULL);

		multires_subdisp(orig, ob, final, mmd->lvl, mmd->totlvl, totsubvert, totsubedge, totsubface, 0);

		orig->needsFree = 1;
		orig->release(orig);
	}

	mmd->lvl = mmd->totlvl;
}

typedef struct DisplacerEdges {
	/* DerivedMesh index at the start of each edge (using face x/y directions to define the start) */
	int base[4];
	/* 1 if edge moves in the positive x or y direction, -1 otherwise */
	int dir[4];
} DisplacerEdges;

typedef struct DisplacerSpill {
	/* Index of face (in base mesh), -1 for none */
	int face;

	/* Spill flag */
	/* 1 = Negative variable axis */
	/* 2 = Near fixed axis */
	/* 4 = Flip axes */
	int f;

	/* Neighboring edges */
	DisplacerEdges edges;
} DisplacerSpill;

typedef struct MultiresDisplacer {
	Mesh *me;
	MDisps *grid;
	MFace *face;
	
	int dm_first_base_vert_index;

	int spacing;
	int sidetot, interior_st, disp_st;
	int sidendx;
	int type;
	int invert;
	MVert *subco;
	int subco_index, face_index;
	float weight;

	/* Valence for each corner */
	int valence[4];

	/* Neighboring edges for current face */
	DisplacerEdges edges_primary;

	/* Neighboring faces */
	DisplacerSpill spill_x, spill_y;

	int *face_offsets;

	int x, y, ax, ay;
} MultiresDisplacer;

static int mface_v(MFace *f, int v)
{
	return v == 0 ? f->v1 : v == 1 ? f->v2 : v == 2 ? f->v3 : v == 3 ? f->v4 : -1;
}

/* Get the edges (and their directions) */
static void find_displacer_edges(MultiresDisplacer *d, DerivedMesh *dm, DisplacerEdges *de, MFace *f)
{
	ListBase *emap = MultiresDM_get_vert_edge_map(dm);
	IndexNode *n;
	int i, end = f->v4 ? 4 : 3;
	int offset = dm->getNumVerts(dm) - d->me->totvert - d->me->totedge * d->interior_st;

	for(i = 0; i < end; ++i) {
		int vcur = mface_v(f, i);
		int vnext = mface_v(f, i == end - 1 ? 0 : i + 1);

		de->dir[i] = 1;
		
		for(n = emap[vcur].first; n; n = n->next) {
			MEdge *e = &d->me->medge[n->index];
			
			if(e->v1 == vnext || e->v2 == vnext) {
				de->base[i] = n->index * d->interior_st;
				if(((i == 0 || i == 1) && e->v1 == vnext) ||
				   ((i == 2 || i == 3) && e->v2 == vnext)) {
					de->dir[i] = -1;
					de->base[i] += d->interior_st - 1;
				}
				de->base[i] += offset;
				break;
			}
		}
	}
}

/* Returns in out the corners [0-3] that use v1 and v2 */
static void find_face_corners(MFace *f, int v1, int v2, int out[2])
{
	int i, end = f->v4 ? 4 : 3;

	for(i = 0; i < end; ++i) {
		int corner = mface_v(f, i);
		if(corner == v1)
			out[0] = i;
		if(corner == v2)
			out[1] = i;
	}
}

static void multires_displacer_get_spill_faces(MultiresDisplacer *d, DerivedMesh *dm, MFace *mface)
{
	ListBase *map = MultiresDM_get_vert_face_map(dm);
	IndexNode *n1, *n2;
	int v4 = d->face->v4 ? d->face->v4 : d->face->v1;
	int crn[2], lv;

	memset(&d->spill_x, 0, sizeof(DisplacerSpill));
	memset(&d->spill_y, 0, sizeof(DisplacerSpill));
	d->spill_x.face = d->spill_y.face = -1;

	for(n1 = map[d->face->v3].first; n1; n1 = n1->next) {
		if(n1->index == d->face_index)
			continue;

		for(n2 = map[d->face->v2].first; n2; n2 = n2->next) {
			if(n1->index == n2->index)
				d->spill_x.face = n1->index;
		}
		for(n2 = map[v4].first; n2; n2 = n2->next) {
			if(n1->index == n2->index)
				d->spill_y.face = n1->index;
		}
	}

	if(d->spill_x.face != -1) {
		/* Neighbor of v2/v3 found, find flip and orientation */
		find_face_corners(&mface[d->spill_x.face], d->face->v2, d->face->v3, crn);
		lv = mface[d->spill_x.face].v4 ? 3 : 2;

		if(crn[0] == 0 && crn[1] == lv)
			d->spill_x.f = 0+2+0;
		else if(crn[0] == lv && crn[1] == 0)
			d->spill_x.f = 1+2+0;
		else if(crn[0] == 1 && crn[1] == 0)
			d->spill_x.f = 1+2+4;
		else if(crn[0] == 0 && crn[1] == 1)
			d->spill_x.f = 0+2+4;
		else if(crn[0] == 2 && crn[1] == 1)
			d->spill_x.f = 1+0+0;
		else if(crn[0] == 1 && crn[1] == 2)
			d->spill_x.f = 0+0+0;
		else if(crn[0] == 3 && crn[1] == 2)
			d->spill_x.f = 0+0+4;
		else if(crn[0] == 2 && crn[1] == 3)
			d->spill_x.f = 1+0+4;

		find_displacer_edges(d, dm, &d->spill_x.edges, &mface[d->spill_x.face]);
	}

	if(d->spill_y.face != -1) {
		/* Neighbor of v3/v4 found, find flip and orientation */
		find_face_corners(&mface[d->spill_y.face], d->face->v3, v4, crn);
		lv = mface[d->spill_y.face].v4 ? 3 : 2;

		if(crn[0] == 1 && crn[1] == 0)
			d->spill_y.f = 1+2+0;
		else if(crn[0] == 0 && crn[1] == 1)
			d->spill_y.f = 0+2+0;
		else if(crn[0] == 2 && crn[1] == 1)
			d->spill_y.f = 1+0+4;
		else if(crn[0] == 1 && crn[1] == 2)
			d->spill_y.f = 0+0+4;
		else if(crn[0] == 3 && crn[1] == 2)
			d->spill_y.f = 0+0+0;
		else if(crn[0] == 2 && crn[1] == 3)
			d->spill_y.f = 1+0+0;
		else if(crn[0] == 0 && crn[1] == lv)
			d->spill_y.f = 0+2+4;
		else if(crn[0] == lv && crn[1] == 0)
			d->spill_y.f = 1+2+4;

		find_displacer_edges(d, dm, &d->spill_y.edges, &mface[d->spill_y.face]);
	}
}

static void find_corner_valences(MultiresDisplacer *d, DerivedMesh *dm)
{
	int i;

	d->valence[3] = -1;

	/* Set the vertex valence for the corners */
	for(i = 0; i < (d->face->v4 ? 4 : 3); ++i)
		d->valence[i] = BLI_countlist(&MultiresDM_get_vert_edge_map(dm)[mface_v(d->face, i)]);
}

static void multires_displacer_init(MultiresDisplacer *d, DerivedMesh *dm,
			     const int face_index, const int invert)
{
	Mesh *me = MultiresDM_get_mesh(dm);

	d->me = me;
	d->face = me->mface + face_index;
	d->face_index = face_index;
	d->face_offsets = MultiresDM_get_face_offsets(dm);
	/* Get the multires grid from customdata */
	d->grid = CustomData_get_layer(&me->fdata, CD_MDISPS);
	if(d->grid)
		d->grid += face_index;

	d->spacing = pow(2, MultiresDM_get_totlvl(dm) - MultiresDM_get_lvl(dm));
	d->sidetot = multires_side_tot[MultiresDM_get_lvl(dm) - 1];
	d->interior_st = d->sidetot - 2;
	d->disp_st = multires_side_tot[MultiresDM_get_totlvl(dm) - 1];
	d->invert = invert;

	multires_displacer_get_spill_faces(d, dm, me->mface);
	find_displacer_edges(d, dm, &d->edges_primary, d->face);
	find_corner_valences(d, dm);

	d->dm_first_base_vert_index = dm->getNumVerts(dm) - me->totvert;
}

static void multires_displacer_weight(MultiresDisplacer *d, const float w)
{
	d->weight = w;
}

static void multires_displacer_anchor(MultiresDisplacer *d, const int type, const int side_index)
{
	d->sidendx = side_index;
	d->x = d->y = d->sidetot / 2;
	d->type = type;

	if(type == 2) {
		if(side_index == 0)
			d->y -= 1;
		else if(side_index == 1)
			d->x += 1;
		else if(side_index == 2)
			d->y += 1;
		else if(side_index == 3)
			d->x -= 1;
	}
	else if(type == 3) {
		if(side_index == 0) {
			d->x -= 1;
			d->y -= 1;
		}
		else if(side_index == 1) {
			d->x += 1;
			d->y -= 1;
		}
		else if(side_index == 2) {
			d->x += 1;
			d->y += 1;
		}
		else if(side_index == 3) {
			d->x -= 1;
			d->y += 1;
		}
	}

	d->ax = d->x;
	d->ay = d->y;
}

static void multires_displacer_anchor_edge(MultiresDisplacer *d, int v1, int v2, int x)
{
	d->type = 4;

	if(v1 == d->face->v1) {
		d->x = 0;
		d->y = 0;
		if(v2 == d->face->v2)
			d->x += x;
		else if(v2 == d->face->v3) {
			if(x < d->sidetot / 2)
				d->y = x;
			else {
				d->x = x;
				d->y = d->sidetot - 1;
			}
		}
		else
			d->y += x;
	}
	else if(v1 == d->face->v2) {
		d->x = d->sidetot - 1;
		d->y = 0;
		if(v2 == d->face->v1)
			d->x -= x;
		else
			d->y += x;
	}
	else if(v1 == d->face->v3) {
		d->x = d->sidetot - 1;
		d->y = d->sidetot - 1;
		if(v2 == d->face->v2)
			d->y -= x;
		else if(v2 == d->face->v1) {
			if(x < d->sidetot / 2)
				d->x -= x;
			else {
				d->x = 0;
				d->y -= x;
			}
		}
		else
			d->x -= x;
	}
	else if(v1 == d->face->v4) {
		d->x = 0;
		d->y = d->sidetot - 1;
		if(v2 == d->face->v3)
			d->x += x;
		else
			d->y -= x;
	}
}

static void multires_displacer_anchor_vert(MultiresDisplacer *d, const int v)
{
	const int e = d->sidetot - 1;

	d->type = 5;

	d->x = d->y = 0;
	if(v == d->face->v2)
		d->x = e;
	else if(v == d->face->v3)
		d->x = d->y = e;
	else if(v == d->face->v4)
		d->y = e;
}

static void multires_displacer_jump(MultiresDisplacer *d)
{
	if(d->sidendx == 0) {
		d->x -= 1;
		d->y = d->ay;
	}
	else if(d->sidendx == 1) {
		d->x = d->ax;
		d->y -= 1;
	}
	else if(d->sidendx == 2) {
		d->x += 1;
		d->y = d->ay;
	}
	else if(d->sidendx == 3) {
		d->x = d->ax;
		d->y += 1;
	}
}

/* Treating v1 as (0,0) and v3 as (st-1,st-1),
   returns the index of the vertex at (x,y).
   If x or y is >= st, wraps over to the adjacent face,
   or if there is no adjacent face, returns -2. */
static int multires_index_at_loc(int face_index, int x, int y, MultiresDisplacer *d, DisplacerEdges *de)
{
	int coord_edge = d->sidetot - 1; /* Max value of x/y at edge of grid */
	int mid = d->sidetot / 2;
	int lim = mid - 1;
	int qtot = lim * lim;
	int base = d->face_offsets[face_index];
 
	/* Edge spillover */
	if(x == d->sidetot || y == d->sidetot) {
		int flags, v_axis, f_axis, lx, ly;

		if(x == d->sidetot && d->spill_x.face != -1) {
			flags = d->spill_x.f;

			/* Handle triangle seam between v1 and v3 */
			if(!d->me->mface[d->spill_x.face].v4 &&
			   ((flags == 2 && y >= mid) || (flags == 3 && y < mid)))
				flags += 2;

			v_axis = (flags & 1) ? d->sidetot - 1 - y : y;
			f_axis = (flags & 2) ? 1 : d->sidetot - 2;
			lx = f_axis, ly = v_axis;

			if(flags & 4) {
				lx = v_axis;
				ly = f_axis;
			}

			return multires_index_at_loc(d->spill_x.face, lx, ly, d, &d->spill_x.edges);
		}
		else if(y == d->sidetot && d->spill_y.face != -1) {
			flags = d->spill_y.f;

			/* Handle triangle seam between v1 and v3 */
			if(!d->me->mface[d->spill_y.face].v4 &&
			   ((flags == 6 && x >= mid) || (flags == 7 && x < mid)))
				flags = ~flags;

			v_axis = (flags & 1) ? x : d->sidetot - 1 - x;
			f_axis = (flags & 2) ? 1 : d->sidetot - 2;
			lx = v_axis, ly = f_axis;

			if(flags & 4) {
				lx = f_axis;
				ly = v_axis;
			}
			
			return multires_index_at_loc(d->spill_y.face, lx, ly, d, &d->spill_y.edges);
		}
		else
			return -2;
	}
	/* Corners */
	else if(x == 0 && y == 0)
		return d->dm_first_base_vert_index + d->face->v1;
	else if(x == coord_edge && y == 0)
		return d->dm_first_base_vert_index + d->face->v2;
	else if(x == coord_edge && y == coord_edge)
		return d->dm_first_base_vert_index + d->face->v3;
	else if(x == 0 && y == coord_edge)
		return d->dm_first_base_vert_index + d->face->v4;
	/* Edges */
	else if(x == 0) {
		if(d->face->v4)
			return de->base[3] + de->dir[3] * (y - 1);
		else
			return de->base[2] + de->dir[2] * (y - 1);
	}
	else if(y == 0)
		return de->base[0] + de->dir[0] * (x - 1);
	else if(x == d->sidetot - 1)
		return de->base[1] + de->dir[1] * (y - 1);
	else if(y == d->sidetot - 1)
		return de->base[2] + de->dir[2] * (x - 1);
	/* Face center */
	else if(x == mid && y == mid)
		return base;
	/* Cross */
	else if(x == mid && y < mid)
		return base + (mid - y);
	else if(y == mid && x > mid)
		return base + lim + (x - mid);
	else if(x == mid && y > mid)
		return base + lim*2 + (y - mid);
	else if(y == mid && x < mid) {
		if(d->face->v4)
			return base + lim*3 + (mid - x);
		else
			return base + lim*2 + (mid - x);
	}
	/* Quarters */
	else {
		int offset = base + lim * (d->face->v4 ? 4 : 3);
		if(x < mid && y < mid)
			return offset + ((mid - x - 1)*lim + (mid - y));
		else if(x > mid && y < mid)
			return offset + qtot + ((mid - y - 1)*lim + (x - mid));
		else if(x > mid && y > mid)
			return offset + qtot*2 + ((x - mid - 1)*lim + (y - mid));
		else if(x < mid && y > mid)
			return offset + qtot*3 + ((y - mid - 1)*lim + (mid - x));
	}
		
	return -1;
}

/* Calculate the TS matrix used for applying displacements.
   Uses the undisplaced subdivided mesh's curvature to find a
   smoothly normal and tangents. */
static void calc_disp_mat(MultiresDisplacer *d, float mat[3][3])
{
	int u = multires_index_at_loc(d->face_index, d->x + 1, d->y, d, &d->edges_primary);
	int v = multires_index_at_loc(d->face_index, d->x, d->y + 1, d, &d->edges_primary);
	float norm[3], t1[3], t2[3], inv[3][3];
	MVert *base = d->subco + d->subco_index;

	//printf("f=%d, x=%d, y=%d, i=%d, u=%d, v=%d ", d->face_index, d->x, d->y, d->subco_index, u, v);
	
	norm[0] = base->no[0] / 32767.0f;
	norm[1] = base->no[1] / 32767.0f;
	norm[2] = base->no[2] / 32767.0f;

	/* Special handling for vertices of valence 3 */
	if(d->valence[1] == 3 && d->x == d->sidetot - 1 && d->y == 0)
		u = -1;
	else if(d->valence[2] == 3 && d->x == d->sidetot - 1 && d->y == d->sidetot - 1)
		u = v = -1;
	else if(d->valence[3] == 3 && d->x == 0 && d->y == d->sidetot - 1)
		v = -1;

	/* If either u or v is -2, it's on a boundary. In this
	   case, back up by one row/column and use the same
	   vector as the preceeding sub-edge. */

	if(u < 0) {
		u = multires_index_at_loc(d->face_index, d->x - 1, d->y, d, &d->edges_primary);
		VecSubf(t1, base->co, d->subco[u].co);
	}
	else
		VecSubf(t1, d->subco[u].co, base->co);

	if(v < 0) {
		v = multires_index_at_loc(d->face_index, d->x, d->y - 1, d, &d->edges_primary);
		VecSubf(t2, base->co, d->subco[v].co);
	}
	else
		VecSubf(t2, d->subco[v].co, base->co);

	//printf("uu=%d, vv=%d\n", u, v);

	Normalize(t1);
	Normalize(t2);
	Mat3FromColVecs(mat, t1, t2, norm);

	if(d->invert) {
		Mat3Inv(inv, mat);
		Mat3CpyMat3(mat, inv);
	}
}

static void multires_displace(MultiresDisplacer *d, float co[3])
{
	float disp[3], mat[3][3];
	float *data;
	MVert *subco = &d->subco[d->subco_index];

	if(!d->grid || !d->grid->disps) return;

	data = d->grid->disps[(d->y * d->spacing) * d->disp_st + (d->x * d->spacing)];

	if(d->invert)
		VecSubf(disp, co, subco->co);
	else
		VecCopyf(disp, data);


	/* Apply ts matrix to displacement */
	calc_disp_mat(d, mat);
	Mat3MulVecfl(mat, disp);

	if(d->invert) {
		VecCopyf(data, disp);
		
	}
	else {
		if(d->type == 4 || d->type == 5)
			VecMulf(disp, d->weight);
		VecAddf(co, co, disp);
	}

	if(d->type == 2) {
		if(d->sidendx == 0)
			d->y -= 1;
		else if(d->sidendx == 1)
			d->x += 1;
		else if(d->sidendx == 2)
			d->y += 1;
		else if(d->sidendx == 3)
			d->x -= 1;
	}
	else if(d->type == 3) {
		if(d->sidendx == 0)
			d->y -= 1;
		else if(d->sidendx == 1)
			d->x += 1;
		else if(d->sidendx == 2)
			d->y += 1;
		else if(d->sidendx == 3)
			d->x -= 1;
	}
}

static void multiresModifier_disp_run(DerivedMesh *dm, MVert *subco, int invert)
{
	const int lvl = MultiresDM_get_lvl(dm);
	const int gridFaces = multires_side_tot[lvl - 2] - 1;
	const int edgeSize = multires_side_tot[lvl - 1] - 1;
	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = MultiresDM_get_mesh(dm)->medge;
	MFace *mface = MultiresDM_get_mesh(dm)->mface;
	ListBase *map = MultiresDM_get_vert_face_map(dm);
	Mesh *me = MultiresDM_get_mesh(dm);
	MultiresDisplacer d;
	int i, S, x, y;

	d.subco = subco;
	d.subco_index = 0;

	for(i = 0; i < me->totface; ++i) {
		const int numVerts = mface[i].v4 ? 4 : 3;
		
		/* Center */
		multires_displacer_init(&d, dm, i, invert);
		multires_displacer_anchor(&d, 1, 0);
		multires_displace(&d, mvert->co);
		++mvert;
		++d.subco_index;

		/* Cross */
		for(S = 0; S < numVerts; ++S) {
			multires_displacer_anchor(&d, 2, S);
			for(x = 1; x < gridFaces; ++x) {
				multires_displace(&d, mvert->co);
				++mvert;
				++d.subco_index;
			}
		}

		/* Quarters */
		for(S = 0; S < numVerts; S++) {
			multires_displacer_anchor(&d, 3, S);
			for(y = 1; y < gridFaces; y++) {
				for(x = 1; x < gridFaces; x++) {
					multires_displace(&d, mvert->co);
					++mvert;
					++d.subco_index;
				}
				multires_displacer_jump(&d);
			}
		}
	}

	for(i = 0; i < me->totedge; ++i) {
		const MEdge *e = &medge[i];
		for(x = 1; x < edgeSize; ++x) {
			IndexNode *n1, *n2;
			int numFaces = 0;
			for(n1 = map[e->v1].first; n1; n1 = n1->next) {
				for(n2 = map[e->v2].first; n2; n2 = n2->next) {
					if(n1->index == n2->index)
						++numFaces;
				}
			}
			multires_displacer_weight(&d, 1.0f / numFaces);
			/* TODO: Better to have these loops outside the x loop */
			for(n1 = map[e->v1].first; n1; n1 = n1->next) {
				for(n2 = map[e->v2].first; n2; n2 = n2->next) {
					if(n1->index == n2->index) {
						multires_displacer_init(&d, dm, n1->index, invert);
						multires_displacer_anchor_edge(&d, e->v1, e->v2, x);
						multires_displace(&d, mvert->co);
					}
				}
			}
			++mvert;
			++d.subco_index;
		}
	}
		
	for(i = 0; i < me->totvert; ++i) {
		IndexNode *n;
		multires_displacer_weight(&d, 1.0f / BLI_countlist(&map[i]));
		for(n = map[i].first; n; n = n->next) {
			multires_displacer_init(&d, dm, n->index, invert);
			multires_displacer_anchor_vert(&d, i);
			multires_displace(&d, mvert->co);
		}
		++mvert;
		++d.subco_index;
	}

	if(!invert)
		CDDM_calc_normals(dm);
}

static void multiresModifier_update(DerivedMesh *dm)
{
	Object *ob;
	Mesh *me;
	MDisps *mdisps;

	ob = MultiresDM_get_object(dm);
	me = MultiresDM_get_mesh(dm);
	mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);

	if(mdisps) {
		const int lvl = MultiresDM_get_lvl(dm);
		const int totlvl = MultiresDM_get_totlvl(dm);
		
		if(lvl < totlvl) {
			/* Propagate disps upwards */
			DerivedMesh *final, *subco_dm, *orig;
			MVert *verts_new = NULL, *cur_lvl_orig_verts = NULL;
			MultiresModifierData mmd;
			int i;

			orig = CDDM_from_mesh(me, NULL);
			
			/* Regenerate the current level's vertex coordinates
			   (includes older displacements but not new sculpts) */
			mmd.totlvl = totlvl;
			mmd.lvl = lvl;
			subco_dm = multires_dm_create_from_derived(&mmd, 1, orig, ob, 0, 0);
			cur_lvl_orig_verts = CDDM_get_verts(subco_dm);

			/* Subtract the original vertex cos from the new vertex cos */
			verts_new = CDDM_get_verts(dm);
			for(i = 0; i < dm->getNumVerts(dm); ++i)
				VecSubf(verts_new[i].co, verts_new[i].co, cur_lvl_orig_verts[i].co);

			final = multires_subdisp_pre(dm, totlvl - lvl, 0);

			multires_subdisp(orig, ob, final, lvl, totlvl, dm->getNumVerts(dm), dm->getNumEdges(dm),
					 dm->getNumFaces(dm), 1);

			subco_dm->release(subco_dm);
			orig->release(orig);
		}
		else
			multiresModifier_disp_run(dm, MultiresDM_get_subco(dm), 1);
	}
}

void multires_mark_as_modified(struct Object *ob)
{
	if(ob && ob->derivedFinal) {
		MultiresDM_mark_as_modified(ob->derivedFinal);
	}
}

void multires_force_update(Object *ob)
{
	if(ob && ob->derivedFinal) {
		ob->derivedFinal->needsFree =1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal = NULL;
	}
}

struct DerivedMesh *multires_dm_create_from_derived(MultiresModifierData *mmd, int local_mmd, DerivedMesh *dm, Object *ob,
						    int useRenderParams, int isFinalCalc)
{
	SubsurfModifierData smd;
	MultiresSubsurf ms;
	DerivedMesh *result;
	int i;

	ms.mmd = mmd;
	ms.ob = ob;
	ms.local_mmd = local_mmd;

	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = smd.renderLevels = mmd->lvl - 1;
	smd.flags |= eSubsurfModifierFlag_SubsurfUv;

	result = subsurf_make_derived_from_derived_with_multires(dm, &smd, &ms, useRenderParams, NULL, isFinalCalc, 0);
	for(i = 0; i < result->getNumVerts(result); ++i)
		MultiresDM_get_subco(result)[i] = CDDM_get_verts(result)[i];
	multiresModifier_disp_run(result, MultiresDM_get_subco(result), 0);
	MultiresDM_set_update(result, multiresModifier_update);

	return result;
}

/**** Old Multires code ****
***************************/

/* Does not actually free lvl itself */
static void multires_free_level(MultiresLevel *lvl)
{
	if(lvl) {
		if(lvl->faces) MEM_freeN(lvl->faces);
		if(lvl->edges) MEM_freeN(lvl->edges);
		if(lvl->colfaces) MEM_freeN(lvl->colfaces);
	}
}

void multires_free(Multires *mr)
{
	if(mr) {
		MultiresLevel* lvl= mr->levels.first;

		/* Free the first-level data */
		if(lvl) {
			CustomData_free(&mr->vdata, lvl->totvert);
			CustomData_free(&mr->fdata, lvl->totface);
			if(mr->edge_flags)
				MEM_freeN(mr->edge_flags);
			if(mr->edge_creases)
				MEM_freeN(mr->edge_creases);
		}

		while(lvl) {
			multires_free_level(lvl);			
			lvl= lvl->next;
		}

		MEM_freeN(mr->verts);

		BLI_freelistN(&mr->levels);

		MEM_freeN(mr);
	}
}

static void create_old_vert_face_map(ListBase **map, IndexNode **mem, const MultiresFace *mface,
				     const int totvert, const int totface)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert face map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totface*4, "vert face map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totface; ++i){
		for(j = 0; j < (mface[i].v[3]?4:3); ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[mface[i].v[j]], node);
		}
	}
}

static void create_old_vert_edge_map(ListBase **map, IndexNode **mem, const MultiresEdge *medge,
				     const int totvert, const int totedge)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert edge map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totedge*2, "vert edge map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totedge; ++i){
		for(j = 0; j < 2; ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[medge[i].v[j]], node);
		}
	}
}

static MultiresFace *find_old_face(ListBase *map, MultiresFace *faces, int v1, int v2, int v3, int v4)
{
	IndexNode *n1;
	int v[4] = {v1, v2, v3, v4}, i, j;

	for(n1 = map[v1].first; n1; n1 = n1->next) {
		int fnd[4] = {0, 0, 0, 0};

		for(i = 0; i < 4; ++i) {
			for(j = 0; j < 4; ++j) {
				if(v[i] == faces[n1->index].v[j])
					fnd[i] = 1;
			}
		}

		if(fnd[0] && fnd[1] && fnd[2] && fnd[3])
			return &faces[n1->index];
	}

	return NULL;
}

static MultiresEdge *find_old_edge(ListBase *map, MultiresEdge *edges, int v1, int v2)
{
	IndexNode *n1, *n2;

	for(n1 = map[v1].first; n1; n1 = n1->next) {
		for(n2 = map[v2].first; n2; n2 = n2->next) {
			if(n1->index == n2->index)
				return &edges[n1->index];
		}
	}

	return NULL;
}

static void multires_load_old_edges(ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst, int v1, int v2, int mov)
{
	int emid = find_old_edge(emap[2], lvl->edges, v1, v2)->mid;
	vvmap[dst + mov] = emid;

	if(lvl->next->next) {
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v1, emid, mov / 2);
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v2, emid, -mov / 2);
	}
}

static void multires_load_old_faces(ListBase **fmap, ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst,
				    int v1, int v2, int v3, int v4, int st2, int st3)
{
	int fmid;
	int emid13, emid14, emid23, emid24;

	if(lvl && lvl->next) {
		fmid = find_old_face(fmap[1], lvl->faces, v1, v2, v3, v4)->mid;
		vvmap[dst] = fmid;

		emid13 = find_old_edge(emap[1], lvl->edges, v1, v3)->mid;
		emid14 = find_old_edge(emap[1], lvl->edges, v1, v4)->mid;
		emid23 = find_old_edge(emap[1], lvl->edges, v2, v3)->mid;
		emid24 = find_old_edge(emap[1], lvl->edges, v2, v4)->mid;


		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst + st2 * st3 + st3,
					fmid, v2, emid23, emid24, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst - st2 * st3 + st3,
					emid14, emid24, fmid, v4, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst + st2 * st3 - st3,
					emid13, emid23, v3, fmid, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst - st2 * st3 - st3,
					v1, fmid, emid13, emid14, st2, st3 / 2);

		if(lvl->next->next) {
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid24, fmid, st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid13, fmid, -st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid14, fmid, -st2 * st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid23, fmid, st2 * st3);
		}
	}
}

/* Loads a multires object stored in the old Multires struct into the new format */
void multires_load_old(DerivedMesh *dm, Multires *mr)
{
	MultiresLevel *lvl, *lvl1;
	MVert *vsrc, *vdst;
	int src, dst;
	int totlvl = MultiresDM_get_totlvl(dm);
	int st = multires_side_tot[totlvl - 2] - 1;
	int extedgelen = multires_side_tot[totlvl - 1] - 2;
	int *vvmap; // inorder for dst, map to src
	int crossedgelen;
	int i, j, s, x, totvert, tottri, totquad;

	src = 0;
	dst = 0;
	vsrc = mr->verts;
	vdst = CDDM_get_verts(dm);
	totvert = dm->getNumVerts(dm);
	vvmap = MEM_callocN(sizeof(int) * totvert, "multires vvmap");

	lvl1 = mr->levels.first;
	/* Load base verts */
	for(i = 0; i < lvl1->totvert; ++i) {
		vvmap[totvert - lvl1->totvert + i] = src;
		++src;
	}

	/* Original edges */
	dst = totvert - lvl1->totvert - extedgelen * lvl1->totedge;
	for(i = 0; i < lvl1->totedge; ++i) {
		int ldst = dst + extedgelen * i;
		int lsrc = src;
		lvl = lvl1->next;

		for(j = 2; j <= mr->level_count; ++j) {
			int base = multires_side_tot[totlvl - j] - 2;
			int skip = multires_side_tot[totlvl - j + 1] - 1;
			int st = multires_side_tot[j - 2] - 1;

			for(x = 0; x < st; ++x)
				vvmap[ldst + base + x * skip] = lsrc + st * i + x;

			lsrc += lvl->totvert - lvl->prev->totvert;
			lvl = lvl->next;
		}
	}

	/* Center points */
	dst = 0;
	for(i = 0; i < lvl1->totface; ++i) {
		int sides = lvl1->faces[i].v[3] ? 4 : 3;

		vvmap[dst] = src + lvl1->totedge + i;
		dst += 1 + sides * (st - 1) * st;
	}


	/* The rest is only for level 3 and up */
	if(lvl1->next && lvl1->next->next) {
		ListBase **fmap, **emap;
		IndexNode **fmem, **emem;

		/* Face edge cross */
		tottri = totquad = 0;
		crossedgelen = multires_side_tot[totlvl - 2] - 2;
		dst = 0;
		for(i = 0; i < lvl1->totface; ++i) {
			int sides = lvl1->faces[i].v[3] ? 4 : 3;

			lvl = lvl1->next->next;
			++dst;

			for(j = 3; j <= mr->level_count; ++j) {
				int base = multires_side_tot[totlvl - j] - 2;
				int skip = multires_side_tot[totlvl - j + 1] - 1;
				int st = pow(2, j - 2);
				int st2 = pow(2, j - 3);
				int lsrc = lvl->prev->totvert;

				/* Skip exterior edge verts */
				lsrc += lvl1->totedge * st;

				/* Skip earlier face edge crosses */
				lsrc += st2 * (tottri * 3 + totquad * 4);

				for(s = 0; s < sides; ++s) {
					for(x = 0; x < st2; ++x) {
						vvmap[dst + crossedgelen * (s + 1) - base - x * skip - 1] = lsrc;
						++lsrc;
					}
				}

				lvl = lvl->next;
			}

			dst += sides * (st - 1) * st;

			if(sides == 4) ++totquad;
			else ++tottri;

		}

		/* calculate vert to edge/face maps for each level (except the last) */
		fmap = MEM_callocN(sizeof(ListBase*) * (mr->level_count-1), "multires fmap");
		emap = MEM_callocN(sizeof(ListBase*) * (mr->level_count-1), "multires emap");
		fmem = MEM_callocN(sizeof(IndexNode*) * (mr->level_count-1), "multires fmem");
		emem = MEM_callocN(sizeof(IndexNode*) * (mr->level_count-1), "multires emem");
		lvl = lvl1;
		for(i = 0; i < mr->level_count - 1; ++i) {
			create_old_vert_face_map(fmap + i, fmem + i, lvl->faces, lvl->totvert, lvl->totface);
			create_old_vert_edge_map(emap + i, emem + i, lvl->edges, lvl->totvert, lvl->totedge);
			lvl = lvl->next;
		}

		/* Interior face verts */
		lvl = lvl1->next->next;
		dst = 0;
		for(j = 0; j < lvl1->totface; ++j) {
			int sides = lvl1->faces[j].v[3] ? 4 : 3;
			int ldst = dst + 1 + sides * (st - 1);

			for(s = 0; s < sides; ++s) {
				int st2 = multires_side_tot[totlvl - 2] - 2;
				int st3 = multires_side_tot[totlvl - 3] - 2;
				int st4 = st3 == 0 ? 1 : (st3 + 1) / 2;
				int mid = ldst + st2 * st3 + st3;
				int cv = lvl1->faces[j].v[s];
				int nv = lvl1->faces[j].v[s == sides - 1 ? 0 : s + 1];
				int pv = lvl1->faces[j].v[s == 0 ? sides - 1 : s - 1];

				multires_load_old_faces(fmap, emap, lvl1->next, vvmap, mid,
							vvmap[dst], cv,
							find_old_edge(emap[0], lvl1->edges, pv, cv)->mid,
							find_old_edge(emap[0], lvl1->edges, cv, nv)->mid,
							st2, st4);

				ldst += (st - 1) * (st - 1);
			}


			dst = ldst;
		}

		lvl = lvl->next;

		for(i = 0; i < mr->level_count - 1; ++i) {
			MEM_freeN(fmap[i]);
			MEM_freeN(fmem[i]);
			MEM_freeN(emap[i]);
			MEM_freeN(emem[i]);
		}

		MEM_freeN(fmap);
		MEM_freeN(emap);
		MEM_freeN(fmem);
		MEM_freeN(emem);
	}

	/* Transfer verts */
	for(i = 0; i < totvert; ++i)
		VecCopyf(vdst[i].co, vsrc[vvmap[i]].co);

	MEM_freeN(vvmap);
}
