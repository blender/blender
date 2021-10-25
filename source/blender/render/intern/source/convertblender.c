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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/convertblender.c
 *  \ingroup render
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_rand.h"
#include "BLI_memarena.h"
#ifdef WITH_FREESTYLE
#  include "BLI_edgehash.h"
#endif

#include "BLT_translation.h"

#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_colortools.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "PIL_time.h"

#include "envmap.h"
#include "occlusion.h"
#include "pointdensity.h"
#include "voxeldata.h"
#include "render_types.h"
#include "rendercore.h"
#include "renderdatabase.h"
#include "renderpipeline.h"
#include "shadbuf.h"
#include "shading.h"
#include "strand.h"
#include "texture.h"
#include "volume_precache.h"
#include "sss.h"
#include "zbuf.h"
#include "sunsky.h"

/* 10 times larger than normal epsilon, test it on default nurbs sphere with ray_transp (for quad detection) */
/* or for checking vertex normal flips */
#define FLT_EPSILON10 1.19209290e-06F

/* could enable at some point but for now there are far too many conversions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

/* ------------------------------------------------------------------------- */
/* tool functions/defines for ad hoc simplification and possible future 
 * cleanup      */
/* ------------------------------------------------------------------------- */

#define UVTOINDEX(u, v) (startvlak + (u) * sizev + (v))
/*
 *
 * NOTE THAT U/V COORDINATES ARE SOMETIMES SWAPPED !!
 *
 * ^   ()----p4----p3----()
 * |   |     |     |     |
 * u   |     |  F1 |  F2 |
 *     |     |     |     |
 *     ()----p1----p2----()
 *            v ->
 */

/* ------------------------------------------------------------------------- */

#define CD_MASK_RENDER_INTERNAL \
    (CD_MASK_BAREMESH | CD_MASK_MFACE | CD_MASK_MTFACE | CD_MASK_MCOL)

static void split_v_renderfaces(ObjectRen *obr, int startvlak, int UNUSED(startvert), int UNUSED(usize), int vsize, int uIndex, int UNUSED(cyclu), int cyclv)
{
	int vLen = vsize-1+(!!cyclv);
	int v;

	for (v=0; v<vLen; v++) {
		VlakRen *vlr = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + v);
		VlakRen *vlr_other;
		VertRen *vert = RE_vertren_copy(obr, vlr->v2);

		if (cyclv) {
			vlr->v2 = vert;

			if (v == vLen - 1) {
				vlr_other = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + 0);
				vlr_other->v1 = vert;
			}
			else {
				vlr_other = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + v+1);
				vlr_other->v1 = vert;
			}
		}
		else {
			vlr->v2 = vert;

			if (v < vLen - 1) {
				vlr_other = RE_findOrAddVlak(obr, startvlak + vLen*uIndex + v+1);
				vlr_other->v1 = vert;
			}

			if (v == 0) {
				vlr->v1 = RE_vertren_copy(obr, vlr->v1);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Stress, tangents and normals                                              */
/* ------------------------------------------------------------------------- */

static void calc_edge_stress_add(float *accum, VertRen *v1, VertRen *v2)
{
	float len= len_v3v3(v1->co, v2->co)/len_v3v3(v1->orco, v2->orco);
	float *acc;
	
	acc= accum + 2*v1->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
	
	acc= accum + 2*v2->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
}

static void calc_edge_stress(Render *UNUSED(re), ObjectRen *obr, Mesh *me)
{
	float loc[3], size[3], *accum, *acc, *accumoffs, *stress;
	int a;
	
	if (obr->totvert==0) return;
	
	BKE_mesh_texspace_get(me, loc, NULL, size);
	
	accum= MEM_callocN(2*sizeof(float)*obr->totvert, "temp accum for stress");
	
	/* de-normalize orco */
	for (a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		if (ver->orco) {
			ver->orco[0]= ver->orco[0]*size[0] +loc[0];
			ver->orco[1]= ver->orco[1]*size[1] +loc[1];
			ver->orco[2]= ver->orco[2]*size[2] +loc[2];
		}
	}
	
	/* add stress values */
	accumoffs= accum;	/* so we can use vertex index */
	for (a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(obr, a);

		if (vlr->v1->orco && vlr->v4) {
			calc_edge_stress_add(accumoffs, vlr->v1, vlr->v2);
			calc_edge_stress_add(accumoffs, vlr->v2, vlr->v3);
			calc_edge_stress_add(accumoffs, vlr->v3, vlr->v1);
			if (vlr->v4) {
				calc_edge_stress_add(accumoffs, vlr->v3, vlr->v4);
				calc_edge_stress_add(accumoffs, vlr->v4, vlr->v1);
				calc_edge_stress_add(accumoffs, vlr->v2, vlr->v4);
			}
		}
	}
	
	for (a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		if (ver->orco) {
			/* find stress value */
			acc= accumoffs + 2*ver->index;
			if (acc[1]!=0.0f)
				acc[0]/= acc[1];
			stress= RE_vertren_get_stress(obr, ver, 1);
			*stress= *acc;
			
			/* restore orcos */
			ver->orco[0] = (ver->orco[0]-loc[0])/size[0];
			ver->orco[1] = (ver->orco[1]-loc[1])/size[1];
			ver->orco[2] = (ver->orco[2]-loc[2])/size[2];
		}
	}
	
	MEM_freeN(accum);
}

/* gets tangent from tface or orco */
static void calc_tangent_vector(ObjectRen *obr, VlakRen *vlr, int do_tangent)
{
	MTFace *tface= RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0);
	VertRen *v1=vlr->v1, *v2=vlr->v2, *v3=vlr->v3, *v4=vlr->v4;
	float tang[3], *tav;
	float *uv1, *uv2, *uv3, *uv4;
	float uv[4][2];
	
	if (tface) {
		uv1= tface->uv[0];
		uv2= tface->uv[1];
		uv3= tface->uv[2];
		uv4= tface->uv[3];
	}
	else if (v1->orco) {
		uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
		map_to_sphere(&uv[0][0], &uv[0][1], v1->orco[0], v1->orco[1], v1->orco[2]);
		map_to_sphere(&uv[1][0], &uv[1][1], v2->orco[0], v2->orco[1], v2->orco[2]);
		map_to_sphere(&uv[2][0], &uv[2][1], v3->orco[0], v3->orco[1], v3->orco[2]);
		if (v4)
			map_to_sphere(&uv[3][0], &uv[3][1], v4->orco[0], v4->orco[1], v4->orco[2]);
	}
	else return;

	tangent_from_uv(uv1, uv2, uv3, v1->co, v2->co, v3->co, vlr->n, tang);
	
	if (do_tangent) {
		tav= RE_vertren_get_tangent(obr, v1, 1);
		add_v3_v3(tav, tang);
		tav= RE_vertren_get_tangent(obr, v2, 1);
		add_v3_v3(tav, tang);
		tav= RE_vertren_get_tangent(obr, v3, 1);
		add_v3_v3(tav, tang);
	}
	
	if (v4) {
		tangent_from_uv(uv1, uv3, uv4, v1->co, v3->co, v4->co, vlr->n, tang);
		
		if (do_tangent) {
			tav= RE_vertren_get_tangent(obr, v1, 1);
			add_v3_v3(tav, tang);
			tav= RE_vertren_get_tangent(obr, v3, 1);
			add_v3_v3(tav, tang);
			tav= RE_vertren_get_tangent(obr, v4, 1);
			add_v3_v3(tav, tang);
		}
	}
}



/****************************************************************
 ************ tangent space generation interface ****************
 ****************************************************************/

typedef struct {
	ObjectRen *obr;
	int mtface_index;
} SRenderMeshToTangent;

/* interface */
#include "mikktspace.h"

static int GetNumFaces(const SMikkTSpaceContext *pContext)
{
	SRenderMeshToTangent *pMesh = (SRenderMeshToTangent *) pContext->m_pUserData;
	return pMesh->obr->totvlak;
}

static int GetNumVertsOfFace(const SMikkTSpaceContext *pContext, const int face_num)
{
	SRenderMeshToTangent *pMesh = (SRenderMeshToTangent *) pContext->m_pUserData;
	VlakRen *vlr= RE_findOrAddVlak(pMesh->obr, face_num);
	return vlr->v4!=NULL ? 4 : 3;
}

static void GetPosition(const SMikkTSpaceContext *pContext, float r_co[3], const int face_num, const int vert_index)
{
	//assert(vert_index>=0 && vert_index<4);
	SRenderMeshToTangent *pMesh = (SRenderMeshToTangent *) pContext->m_pUserData;
	VlakRen *vlr= RE_findOrAddVlak(pMesh->obr, face_num);
	const float *co = (&vlr->v1)[vert_index]->co;
	copy_v3_v3(r_co, co);
}

static void GetTextureCoordinate(const SMikkTSpaceContext *pContext, float r_uv[2], const int face_num, const int vert_index)
{
	//assert(vert_index>=0 && vert_index<4);
	SRenderMeshToTangent *pMesh = (SRenderMeshToTangent *) pContext->m_pUserData;
	VlakRen *vlr= RE_findOrAddVlak(pMesh->obr, face_num);
	MTFace *tface= RE_vlakren_get_tface(pMesh->obr, vlr, pMesh->mtface_index, NULL, 0);
	const float *coord;
	
	if (tface  != NULL) {
		coord= tface->uv[vert_index];
		copy_v2_v2(r_uv, coord);
	}
	else if ((coord = (&vlr->v1)[vert_index]->orco)) {
		map_to_sphere(&r_uv[0], &r_uv[1], coord[0], coord[1], coord[2]);
	}
	else { /* else we get un-initialized value, 0.0 ok default? */
		zero_v2(r_uv);
	}
}

static void GetNormal(const SMikkTSpaceContext *pContext, float r_no[3], const int face_num, const int vert_index)
{
	//assert(vert_index>=0 && vert_index<4);
	SRenderMeshToTangent *pMesh = (SRenderMeshToTangent *) pContext->m_pUserData;
	VlakRen *vlr= RE_findOrAddVlak(pMesh->obr, face_num);

	if (vlr->flag & ME_SMOOTH) {
		const float *n = (&vlr->v1)[vert_index]->n;
		copy_v3_v3(r_no, n);
	}
	else {
		negate_v3_v3(r_no, vlr->n);
	}
}
static void SetTSpace(const SMikkTSpaceContext *pContext, const float fvTangent[3], const float fSign, const int face_num, const int iVert)
{
	//assert(vert_index>=0 && vert_index<4);
	SRenderMeshToTangent *pMesh = (SRenderMeshToTangent *) pContext->m_pUserData;
	VlakRen *vlr = RE_findOrAddVlak(pMesh->obr, face_num);
	float *ftang = RE_vlakren_get_nmap_tangent(pMesh->obr, vlr, pMesh->mtface_index, true);
	if (ftang!=NULL) {
		copy_v3_v3(&ftang[iVert*4+0], fvTangent);
		ftang[iVert*4+3]=fSign;
	}
}

static void calc_vertexnormals(Render *UNUSED(re), ObjectRen *obr, bool do_vertex_normal, bool do_tangent, bool do_nmap_tangent)
{
	int a;

	/* clear all vertex normals */
	if (do_vertex_normal) {
		for (a=0; a<obr->totvert; a++) {
			VertRen *ver= RE_findOrAddVert(obr, a);
			ver->n[0]=ver->n[1]=ver->n[2]= 0.0f;
		}
	}

	/* calculate cos of angles and point-masses, use as weight factor to
	 * add face normal to vertex */
	for (a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(obr, a);
		if (do_vertex_normal && vlr->flag & ME_SMOOTH) {
			float *n4= (vlr->v4)? vlr->v4->n: NULL;
			const float *c4= (vlr->v4)? vlr->v4->co: NULL;

			accumulate_vertex_normals(vlr->v1->n, vlr->v2->n, vlr->v3->n, n4,
				vlr->n, vlr->v1->co, vlr->v2->co, vlr->v3->co, c4);
		}
		if (do_tangent) {
			/* tangents still need to be calculated for flat faces too */
			/* weighting removed, they are not vertexnormals */
			calc_tangent_vector(obr, vlr, do_tangent);
		}
	}

	/* do solid faces */
	for (a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= RE_findOrAddVlak(obr, a);

		if (do_vertex_normal && (vlr->flag & ME_SMOOTH)==0) {
			if (is_zero_v3(vlr->v1->n)) copy_v3_v3(vlr->v1->n, vlr->n);
			if (is_zero_v3(vlr->v2->n)) copy_v3_v3(vlr->v2->n, vlr->n);
			if (is_zero_v3(vlr->v3->n)) copy_v3_v3(vlr->v3->n, vlr->n);
			if (vlr->v4 && is_zero_v3(vlr->v4->n)) copy_v3_v3(vlr->v4->n, vlr->n);
		}
	}
	
	/* normalize vertex normals */
	for (a=0; a<obr->totvert; a++) {
		VertRen *ver= RE_findOrAddVert(obr, a);
		normalize_v3(ver->n);
		if (do_tangent) {
			float *tav= RE_vertren_get_tangent(obr, ver, 0);
			if (tav) {
				/* orthonorm. */
				const float tdn = dot_v3v3(tav, ver->n);
				tav[0] -= ver->n[0]*tdn;
				tav[1] -= ver->n[1]*tdn;
				tav[2] -= ver->n[2]*tdn;
				normalize_v3(tav);
			}
		}
	}

	/* normal mapping tangent with mikktspace */
	if (do_nmap_tangent != false) {
		SRenderMeshToTangent mesh2tangent;
		SMikkTSpaceContext sContext;
		SMikkTSpaceInterface sInterface;
		memset(&mesh2tangent, 0, sizeof(SRenderMeshToTangent));
		memset(&sContext, 0, sizeof(SMikkTSpaceContext));
		memset(&sInterface, 0, sizeof(SMikkTSpaceInterface));

		mesh2tangent.obr = obr;

		sContext.m_pUserData = &mesh2tangent;
		sContext.m_pInterface = &sInterface;
		sInterface.m_getNumFaces = GetNumFaces;
		sInterface.m_getNumVerticesOfFace = GetNumVertsOfFace;
		sInterface.m_getPosition = GetPosition;
		sInterface.m_getTexCoord = GetTextureCoordinate;
		sInterface.m_getNormal = GetNormal;
		sInterface.m_setTSpaceBasic = SetTSpace;

		for (a = 0; a < MAX_MTFACE; a++) {
			if (obr->tangent_mask & 1 << a) {
				mesh2tangent.mtface_index = a;
				genTangSpaceDefault(&sContext);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Autosmoothing:                                                            */
/* ------------------------------------------------------------------------- */

typedef struct ASvert {
	int totface;
	ListBase faces;
} ASvert;

typedef struct ASface {
	struct ASface *next, *prev;
	VlakRen *vlr[4];
	VertRen *nver[4];
} ASface;

static int as_addvert(ASvert *asv, VertRen *v1, VlakRen *vlr)
{
	ASface *asf;
	int a = -1;

	if (v1 == NULL)
		return a;

	asf = asv->faces.last;
	if (asf) {
		for (a = 0; a < 4 && asf->vlr[a]; a++) {
		}
	}
	else {
		a = 4;
	}

	/* new face struct */
	if (a == 4) {
		a = 0;
		asf = MEM_callocN(sizeof(ASface), "asface");
		BLI_addtail(&asv->faces, asf);
	}

	asf->vlr[a] = vlr;
	asv->totface++;

	return a;
}

static VertRen *as_findvertex_lnor(VlakRen *vlr, VertRen *ver, ASvert *asv, const float lnor[3])
{
	/* return when new vertex already was made, or existing one is OK */
	ASface *asf;
	int a;

	/* First face, we can use existing vert and assign it current lnor! */
	if (asv->totface == 1) {
		copy_v3_v3(ver->n, lnor);
		return ver;
	}

	/* In case existing ver has same normal as current lnor, we can simply use it! */
	if (equals_v3v3(lnor, ver->n)) {
		return ver;
	}

	asf = asv->faces.first;
	while (asf) {
		for (a = 0; a < 4; a++) {
			if (asf->vlr[a] && asf->vlr[a] != vlr) {
				/* this face already made a copy for this vertex! */
				if (asf->nver[a]) {
					if (equals_v3v3(lnor, asf->nver[a]->n)) {
						return asf->nver[a];
					}
				}
			}
		}
		asf = asf->next;
	}

	return NULL;
}

static void as_addvert_lnor(ObjectRen *obr, ASvert *asv, VertRen *ver, VlakRen *vlr, const short _lnor[3])
{
	VertRen *v1;
	ASface *asf;
	int asf_idx;
	float lnor[3];

	normal_short_to_float_v3(lnor, _lnor);

	asf_idx = as_addvert(asv, ver, vlr);
	if (asf_idx < 0) {
		return;
	}
	asf = asv->faces.last;

	/* already made a new vertex within threshold? */
	v1 = as_findvertex_lnor(vlr, ver, asv, lnor);
	if (v1 == NULL) {
		/* make a new vertex */
		v1 = RE_vertren_copy(obr, ver);
		copy_v3_v3(v1->n, lnor);
	}
	if (v1 != ver) {
		asf->nver[asf_idx] = v1;
		if (vlr->v1 == ver) vlr->v1 = v1;
		if (vlr->v2 == ver) vlr->v2 = v1;
		if (vlr->v3 == ver) vlr->v3 = v1;
		if (vlr->v4 == ver) vlr->v4 = v1;
	}
}

/* note; autosmooth happens in object space still, after applying autosmooth we rotate */
/* note2; actually, when original mesh and displist are equal sized, face normals are from original mesh */
static void autosmooth(Render *UNUSED(re), ObjectRen *obr, float mat[4][4], short (*lnors)[4][3])
{
	ASvert *asverts;
	VertRen *ver;
	VlakRen *vlr;
	int a, totvert;

	float rot[3][3];

	/* Note: For normals, we only want rotation, not scaling component.
	 *       Negative scales (aka mirroring) give wrong results, see T44102. */
	if (lnors) {
		float mat3[3][3], size[3];

		copy_m3_m4(mat3, mat);
		mat3_to_rot_size(rot, size, mat3);
	}

	if (obr->totvert == 0)
		return;

	totvert = obr->totvert;
	asverts = MEM_callocN(sizeof(ASvert) * totvert, "all smooth verts");

	if (lnors) {
		/* We construct listbase of all vertices and pointers to faces, and add new verts when needed
		 * (i.e. when existing ones do not share the same (loop)normal).
		 */
		for (a = 0; a < obr->totvlak; a++, lnors++) {
			vlr = RE_findOrAddVlak(obr, a);
			/* skip wire faces */
			if (vlr->v2 != vlr->v3) {
				as_addvert_lnor(obr, asverts+vlr->v1->index, vlr->v1, vlr, (const short*)lnors[0][0]);
				as_addvert_lnor(obr, asverts+vlr->v2->index, vlr->v2, vlr, (const short*)lnors[0][1]);
				as_addvert_lnor(obr, asverts+vlr->v3->index, vlr->v3, vlr, (const short*)lnors[0][2]);
				if (vlr->v4)
					as_addvert_lnor(obr, asverts+vlr->v4->index, vlr->v4, vlr, (const short*)lnors[0][3]);
			}
		}
	}

	/* free */
	for (a = 0; a < totvert; a++) {
		BLI_freelistN(&asverts[a].faces);
	}
	MEM_freeN(asverts);

	/* rotate vertices and calculate normal of faces */
	for (a = 0; a < obr->totvert; a++) {
		ver = RE_findOrAddVert(obr, a);
		mul_m4_v3(mat, ver->co);
		if (lnors) {
			mul_m3_v3(rot, ver->n);
			negate_v3(ver->n);
		}
	}
	for (a = 0; a < obr->totvlak; a++) {
		vlr = RE_findOrAddVlak(obr, a);

		/* skip wire faces */
		if (vlr->v2 != vlr->v3) {
			if (vlr->v4)
				normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
			else 
				normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Orco hash and Materials                                                   */
/* ------------------------------------------------------------------------- */

static float *get_object_orco(Render *re, void *ob)
{
	if (!re->orco_hash) {
		return NULL;
	}

	return BLI_ghash_lookup(re->orco_hash, ob);
}

static void set_object_orco(Render *re, void *ob, float *orco)
{
	if (!re->orco_hash)
		re->orco_hash = BLI_ghash_ptr_new("set_object_orco gh");
	
	BLI_ghash_insert(re->orco_hash, ob, orco);
}

static void free_mesh_orco_hash(Render *re) 
{
	if (re->orco_hash) {
		BLI_ghash_free(re->orco_hash, NULL, MEM_freeN);
		re->orco_hash = NULL;
	}
}

static void check_material_mapto(Material *ma)
{
	int a;
	ma->mapto_textured = 0;
	
	/* cache which inputs are actually textured.
	 * this can avoid a bit of time spent iterating through all the texture slots, map inputs and map tos
	 * every time a property which may or may not be textured is accessed */
	
	for (a=0; a<MAX_MTEX; a++) {
		if (ma->mtex[a] && ma->mtex[a]->tex) {
			/* currently used only in volume render, so we'll check for those flags */
			if (ma->mtex[a]->mapto & MAP_DENSITY) ma->mapto_textured |= MAP_DENSITY;
			if (ma->mtex[a]->mapto & MAP_EMISSION) ma->mapto_textured |= MAP_EMISSION;
			if (ma->mtex[a]->mapto & MAP_EMISSION_COL) ma->mapto_textured |= MAP_EMISSION_COL;
			if (ma->mtex[a]->mapto & MAP_SCATTERING) ma->mapto_textured |= MAP_SCATTERING;
			if (ma->mtex[a]->mapto & MAP_TRANSMISSION_COL) ma->mapto_textured |= MAP_TRANSMISSION_COL;
			if (ma->mtex[a]->mapto & MAP_REFLECTION) ma->mapto_textured |= MAP_REFLECTION;
			if (ma->mtex[a]->mapto & MAP_REFLECTION_COL) ma->mapto_textured |= MAP_REFLECTION_COL;
		}
	}
}
static void flag_render_node_material(Render *re, bNodeTree *ntree)
{
	bNode *node;

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id) {
			if (GS(node->id->name)==ID_MA) {
				Material *ma= (Material *)node->id;

				if ((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP))
					re->flag |= R_ZTRA;

				ma->flag |= MA_IS_USED;
			}
			else if (node->type==NODE_GROUP)
				flag_render_node_material(re, (bNodeTree *)node->id);
		}
	}
}

static Material *give_render_material(Render *re, Object *ob, short nr)
{
	extern Material defmaterial;	/* material.c */
	Material *ma;
	
	ma= give_current_material(ob, nr);
	if (ma==NULL)
		ma= &defmaterial;
	
	if (re->r.mode & R_SPEED) ma->texco |= NEED_UV;
	
	if (ma->material_type == MA_TYPE_VOLUME) {
		ma->mode |= MA_TRANSP;
		ma->mode &= ~MA_SHADBUF;
	}
	if ((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP))
		re->flag |= R_ZTRA;
	
	/* for light groups and SSS */
	ma->flag |= MA_IS_USED;

	if (ma->nodetree && ma->use_nodes)
		flag_render_node_material(re, ma->nodetree);
	
	check_material_mapto(ma);
	
	return ma;
}

/* ------------------------------------------------------------------------- */
/* Particles                                                                 */
/* ------------------------------------------------------------------------- */
typedef struct ParticleStrandData {
	struct MCol *mcol;
	float *orco, *uvco, *surfnor;
	float time, adapt_angle, adapt_pix, size;
	int totuv, totcol;
	int first, line, adapt, override_uv;
}
ParticleStrandData;
/* future thread problem... */
static void static_particle_strand(Render *re, ObjectRen *obr, Material *ma, ParticleStrandData *sd, const float vec[3], const float vec1[3])
{
	static VertRen *v1= NULL, *v2= NULL;
	VlakRen *vlr= NULL;
	float nor[3], cross[3], crosslen, w, dx, dy, width;
	static float anor[3], avec[3];
	int flag, i;
	static int second=0;
	
	sub_v3_v3v3(nor, vec, vec1);
	normalize_v3(nor);  /* nor needed as tangent */
	cross_v3_v3v3(cross, vec, nor);

	/* turn cross in pixelsize */
	w= vec[2]*re->winmat[2][3] + re->winmat[3][3];
	dx= re->winx*cross[0]*re->winmat[0][0];
	dy= re->winy*cross[1]*re->winmat[1][1];
	w = sqrtf(dx * dx + dy * dy) / w;
	
	if (w!=0.0f) {
		float fac;
		if (ma->strand_ease!=0.0f) {
			if (ma->strand_ease<0.0f)
				fac= pow(sd->time, 1.0f+ma->strand_ease);
			else
				fac= pow(sd->time, 1.0f/(1.0f-ma->strand_ease));
		}
		else fac= sd->time;

		width= ((1.0f-fac)*ma->strand_sta + (fac)*ma->strand_end);

		/* use actual Blender units for strand width and fall back to minimum width */
		if (ma->mode & MA_STR_B_UNITS) {
			crosslen= len_v3(cross);
			w= 2.0f*crosslen*ma->strand_min/w;

			if (width < w)
				width= w;

			/*cross is the radius of the strand so we want it to be half of full width */
			mul_v3_fl(cross, 0.5f/crosslen);
		}
		else
			width/=w;

		mul_v3_fl(cross, width);
	}
	
	if (ma->mode & MA_TANGENT_STR)
		flag= R_SMOOTH|R_TANGENT;
	else
		flag= R_SMOOTH;
	
	/* only 1 pixel wide strands filled in as quads now, otherwise zbuf errors */
	if (ma->strand_sta==1.0f)
		flag |= R_STRAND;
	
	/* single face line */
	if (sd->line) {
		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->flag= flag;
		vlr->v1= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v4= RE_findOrAddVert(obr, obr->totvert++);
		
		copy_v3_v3(vlr->v1->co, vec);
		add_v3_v3(vlr->v1->co, cross);
		copy_v3_v3(vlr->v1->n, nor);
		vlr->v1->orco= sd->orco;
		vlr->v1->accum = -1.0f;  /* accum abuse for strand texco */
		
		copy_v3_v3(vlr->v2->co, vec);
		sub_v3_v3v3(vlr->v2->co, vlr->v2->co, cross);
		copy_v3_v3(vlr->v2->n, nor);
		vlr->v2->orco= sd->orco;
		vlr->v2->accum= vlr->v1->accum;

		copy_v3_v3(vlr->v4->co, vec1);
		add_v3_v3(vlr->v4->co, cross);
		copy_v3_v3(vlr->v4->n, nor);
		vlr->v4->orco= sd->orco;
		vlr->v4->accum = 1.0f;  /* accum abuse for strand texco */

		copy_v3_v3(vlr->v3->co, vec1);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		copy_v3_v3(vlr->v3->n, nor);
		vlr->v3->orco= sd->orco;
		vlr->v3->accum= vlr->v4->accum;

		normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;

		if (sd->surfnor) {
			float *snor= RE_vlakren_get_surfnor(obr, vlr, 1);
			copy_v3_v3(snor, sd->surfnor);
		}

		if (sd->uvco) {
			for (i=0; i<sd->totuv; i++) {
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr, vlr, i, NULL, 1);
				mtf->uv[0][0]=mtf->uv[1][0]=
				mtf->uv[2][0]=mtf->uv[3][0]=(sd->uvco+2*i)[0];
				mtf->uv[0][1]=mtf->uv[1][1]=
				mtf->uv[2][1]=mtf->uv[3][1]=(sd->uvco+2*i)[1];
			}
			if (sd->override_uv>=0) {
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr, vlr, sd->override_uv, NULL, 0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=0.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=1.0f;
			}
		}
		if (sd->mcol) {
			for (i=0; i<sd->totcol; i++) {
				MCol *mc;
				mc=RE_vlakren_get_mcol(obr, vlr, i, NULL, 1);
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
			}
		}
	}
	/* first two vertices of a strand */
	else if (sd->first) {
		if (sd->adapt) {
			copy_v3_v3(anor, nor);
			copy_v3_v3(avec, vec);
			second=1;
		}

		v1= RE_findOrAddVert(obr, obr->totvert++);
		v2= RE_findOrAddVert(obr, obr->totvert++);
		
		copy_v3_v3(v1->co, vec);
		add_v3_v3(v1->co, cross);
		copy_v3_v3(v1->n, nor);
		v1->orco= sd->orco;
		v1->accum = -1.0f;  /* accum abuse for strand texco */
		
		copy_v3_v3(v2->co, vec);
		sub_v3_v3v3(v2->co, v2->co, cross);
		copy_v3_v3(v2->n, nor);
		v2->orco= sd->orco;
		v2->accum= v1->accum;
	}
	/* more vertices & faces to strand */
	else {
		if (sd->adapt==0 || second) {
			vlr= RE_findOrAddVlak(obr, obr->totvlak++);
			vlr->flag= flag;
			vlr->v1= v1;
			vlr->v2= v2;
			vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
			vlr->v4= RE_findOrAddVert(obr, obr->totvert++);

			v1= vlr->v4; /* cycle */
			v2= vlr->v3; /* cycle */


			if (sd->adapt) {
				second=0;
				copy_v3_v3(anor, nor);
				copy_v3_v3(avec, vec);
			}

		}
		else if (sd->adapt) {
			float dvec[3], pvec[3];
			sub_v3_v3v3(dvec, avec, vec);
			project_v3_v3v3(pvec, dvec, vec);
			sub_v3_v3v3(dvec, dvec, pvec);

			w= vec[2]*re->winmat[2][3] + re->winmat[3][3];
			dx= re->winx*dvec[0]*re->winmat[0][0]/w;
			dy= re->winy*dvec[1]*re->winmat[1][1]/w;
			w = sqrtf(dx * dx + dy * dy);
			if (dot_v3v3(anor, nor)<sd->adapt_angle && w>sd->adapt_pix) {
				vlr= RE_findOrAddVlak(obr, obr->totvlak++);
				vlr->flag= flag;
				vlr->v1= v1;
				vlr->v2= v2;
				vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
				vlr->v4= RE_findOrAddVert(obr, obr->totvert++);

				v1= vlr->v4; /* cycle */
				v2= vlr->v3; /* cycle */

				copy_v3_v3(anor, nor);
				copy_v3_v3(avec, vec);
			}
			else {
				vlr= RE_findOrAddVlak(obr, obr->totvlak-1);
			}
		}
	
		copy_v3_v3(vlr->v4->co, vec);
		add_v3_v3(vlr->v4->co, cross);
		copy_v3_v3(vlr->v4->n, nor);
		vlr->v4->orco= sd->orco;
		vlr->v4->accum= -1.0f + 2.0f * sd->time;  /* accum abuse for strand texco */

		copy_v3_v3(vlr->v3->co, vec);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		copy_v3_v3(vlr->v3->n, nor);
		vlr->v3->orco= sd->orco;
		vlr->v3->accum= vlr->v4->accum;
		
		normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;

		if (sd->surfnor) {
			float *snor= RE_vlakren_get_surfnor(obr, vlr, 1);
			copy_v3_v3(snor, sd->surfnor);
		}

		if (sd->uvco) {
			for (i=0; i<sd->totuv; i++) {
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr, vlr, i, NULL, 1);
				mtf->uv[0][0]=mtf->uv[1][0]=
				mtf->uv[2][0]=mtf->uv[3][0]=(sd->uvco+2*i)[0];
				mtf->uv[0][1]=mtf->uv[1][1]=
				mtf->uv[2][1]=mtf->uv[3][1]=(sd->uvco+2*i)[1];
			}
			if (sd->override_uv>=0) {
				MTFace *mtf;
				mtf=RE_vlakren_get_tface(obr, vlr, sd->override_uv, NULL, 0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=(vlr->v1->accum+1.0f)/2.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=(vlr->v3->accum+1.0f)/2.0f;
			}
		}
		if (sd->mcol) {
			for (i=0; i<sd->totcol; i++) {
				MCol *mc;
				mc=RE_vlakren_get_mcol(obr, vlr, i, NULL, 1);
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
			}
		}
	}
}

static void static_particle_wire(ObjectRen *obr, Material *ma, const float vec[3], const float vec1[3], int first, int line)
{
	VlakRen *vlr;
	static VertRen *v1;

	if (line) {
		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->v1= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v3= vlr->v2;
		vlr->v4= NULL;
		
		copy_v3_v3(vlr->v1->co, vec);
		copy_v3_v3(vlr->v2->co, vec1);
		
		sub_v3_v3v3(vlr->n, vec, vec1);
		normalize_v3(vlr->n);
		copy_v3_v3(vlr->v1->n, vlr->n);
		copy_v3_v3(vlr->v2->n, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V1V2;

	}
	else if (first) {
		v1= RE_findOrAddVert(obr, obr->totvert++);
		copy_v3_v3(v1->co, vec);
	}
	else {
		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->v1= v1;
		vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
		vlr->v3= vlr->v2;
		vlr->v4= NULL;
		
		v1= vlr->v2; /* cycle */
		copy_v3_v3(v1->co, vec);
		
		sub_v3_v3v3(vlr->n, vec, vec1);
		normalize_v3(vlr->n);
		copy_v3_v3(v1->n, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V1V2;
	}

}

static void particle_curve(Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma, ParticleStrandData *sd,
                           const float loc[3], const float loc1[3], int seed, float *pa_co)
{
	HaloRen *har = NULL;

	if (ma->material_type == MA_TYPE_WIRE)
		static_particle_wire(obr, ma, loc, loc1, sd->first, sd->line);
	else if (ma->material_type == MA_TYPE_HALO) {
		har= RE_inithalo_particle(re, obr, dm, ma, loc, loc1, sd->orco, sd->uvco, sd->size, 1.0, seed, pa_co);
		if (har) har->lay= obr->ob->lay;
	}
	else
		static_particle_strand(re, obr, ma, sd, loc, loc1);
}
static void particle_billboard(Render *re, ObjectRen *obr, Material *ma, ParticleBillboardData *bb)
{
	VlakRen *vlr;
	MTFace *mtf;
	float xvec[3], yvec[3], zvec[3], bb_center[3];
	/* Number of tiles */
	int totsplit = bb->uv_split * bb->uv_split;
	int tile, x, y;
	/* Tile offsets */
	float uvx = 0.0f, uvy = 0.0f, uvdx = 1.0f, uvdy = 1.0f, time = 0.0f;

	vlr= RE_findOrAddVlak(obr, obr->totvlak++);
	vlr->v1= RE_findOrAddVert(obr, obr->totvert++);
	vlr->v2= RE_findOrAddVert(obr, obr->totvert++);
	vlr->v3= RE_findOrAddVert(obr, obr->totvert++);
	vlr->v4= RE_findOrAddVert(obr, obr->totvert++);

	psys_make_billboard(bb, xvec, yvec, zvec, bb_center);

	add_v3_v3v3(vlr->v1->co, bb_center, xvec);
	add_v3_v3(vlr->v1->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v1->co);

	sub_v3_v3v3(vlr->v2->co, bb_center, xvec);
	add_v3_v3(vlr->v2->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v2->co);

	sub_v3_v3v3(vlr->v3->co, bb_center, xvec);
	sub_v3_v3v3(vlr->v3->co, vlr->v3->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v3->co);

	add_v3_v3v3(vlr->v4->co, bb_center, xvec);
	sub_v3_v3(vlr->v4->co, yvec);
	mul_m4_v3(re->viewmat, vlr->v4->co);

	normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	copy_v3_v3(vlr->v1->n, vlr->n);
	copy_v3_v3(vlr->v2->n, vlr->n);
	copy_v3_v3(vlr->v3->n, vlr->n);
	copy_v3_v3(vlr->v4->n, vlr->n);
	
	vlr->mat= ma;
	vlr->ec= ME_V2V3;

	if (bb->uv_split > 1) {
		uvdx = uvdy = 1.0f / (float)bb->uv_split;

		if (ELEM(bb->anim, PART_BB_ANIM_AGE, PART_BB_ANIM_FRAME)) {
			if (bb->anim == PART_BB_ANIM_FRAME)
				time = ((int)(bb->time * bb->lifetime) % totsplit)/(float)totsplit;
			else
				time = bb->time;
		}
		else if (bb->anim == PART_BB_ANIM_ANGLE) {
			if (bb->align == PART_BB_VIEW) {
				time = (float)fmod((bb->tilt + 1.0f) / 2.0f, 1.0);
			}
			else {
				float axis1[3] = {0.0f, 0.0f, 0.0f};
				float axis2[3] = {0.0f, 0.0f, 0.0f};

				axis1[(bb->align + 1) % 3] = 1.0f;
				axis2[(bb->align + 2) % 3] = 1.0f;

				if (bb->lock == 0) {
					zvec[bb->align] = 0.0f;
					normalize_v3(zvec);
				}
				
				time = saacos(dot_v3v3(zvec, axis1)) / (float)M_PI;
				
				if (dot_v3v3(zvec, axis2) < 0.0f)
					time = 1.0f - time / 2.0f;
				else
					time /= 2.0f;
			}
		}

		if (bb->split_offset == PART_BB_OFF_LINEAR)
			time = (float)fmod(time + (float)bb->num / (float)totsplit, 1.0f);
		else if (bb->split_offset==PART_BB_OFF_RANDOM)
			time = (float)fmod(time + bb->random, 1.0f);

		/* Find the coordinates in tile space (integer), then convert to UV
		 * space (float). Note that Y is flipped. */
		tile = (int)((time + FLT_EPSILON10) * totsplit);
		x = tile % bb->uv_split;
		y = tile / bb->uv_split;
		y = (bb->uv_split - 1) - y;
		uvx = uvdx * x;
		uvy = uvdy * y;
	}

	/* normal UVs */
	if (bb->uv[0] >= 0) {
		mtf = RE_vlakren_get_tface(obr, vlr, bb->uv[0], NULL, 1);
		mtf->uv[0][0] = 1.0f;
		mtf->uv[0][1] = 1.0f;
		mtf->uv[1][0] = 0.0f;
		mtf->uv[1][1] = 1.0f;
		mtf->uv[2][0] = 0.0f;
		mtf->uv[2][1] = 0.0f;
		mtf->uv[3][0] = 1.0f;
		mtf->uv[3][1] = 0.0f;
	}

	/* time-index UVs */
	if (bb->uv[1] >= 0) {
		mtf = RE_vlakren_get_tface(obr, vlr, bb->uv[1], NULL, 1);
		mtf->uv[0][0] = mtf->uv[1][0] = mtf->uv[2][0] = mtf->uv[3][0] = bb->time;
		mtf->uv[0][1] = mtf->uv[1][1] = mtf->uv[2][1] = mtf->uv[3][1] = (float)bb->num/(float)bb->totnum;
	}

	/* split UVs */
	if (bb->uv_split > 1 && bb->uv[2] >= 0) {
		mtf = RE_vlakren_get_tface(obr, vlr, bb->uv[2], NULL, 1);
		mtf->uv[0][0] = uvx + uvdx;
		mtf->uv[0][1] = uvy + uvdy;
		mtf->uv[1][0] = uvx;
		mtf->uv[1][1] = uvy + uvdy;
		mtf->uv[2][0] = uvx;
		mtf->uv[2][1] = uvy;
		mtf->uv[3][0] = uvx + uvdx;
		mtf->uv[3][1] = uvy;
	}
}
static void particle_normal_ren(short ren_as, ParticleSettings *part, Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma, ParticleStrandData *sd, ParticleBillboardData *bb, ParticleKey *state, int seed, float hasize, float *pa_co)
{
	float loc[3], loc0[3], loc1[3], vel[3];
	
	copy_v3_v3(loc, state->co);

	if (ren_as != PART_DRAW_BB)
		mul_m4_v3(re->viewmat, loc);

	switch (ren_as) {
		case PART_DRAW_LINE:
			sd->line = 1;
			sd->time = 0.0f;
			sd->size = hasize;

			mul_v3_mat3_m4v3(vel, re->viewmat, state->vel);
			normalize_v3(vel);

			if (part->draw & PART_DRAW_VEL_LENGTH)
				mul_v3_fl(vel, len_v3(state->vel));

			madd_v3_v3v3fl(loc0, loc, vel, -part->draw_line[0]);
			madd_v3_v3v3fl(loc1, loc, vel, part->draw_line[1]);

			particle_curve(re, obr, dm, ma, sd, loc0, loc1, seed, pa_co);

			break;

		case PART_DRAW_BB:

			copy_v3_v3(bb->vec, loc);
			copy_v3_v3(bb->vel, state->vel);

			particle_billboard(re, obr, ma, bb);

			break;

		default:
		{
			HaloRen *har = NULL;

			har = RE_inithalo_particle(re, obr, dm, ma, loc, NULL, sd->orco, sd->uvco, hasize, 0.0, seed, pa_co);
			
			if (har) har->lay= obr->ob->lay;

			break;
		}
	}
}
static void get_particle_uvco_mcol(short from, DerivedMesh *dm, float *fuv, int num, ParticleStrandData *sd)
{
	int i;

	/* get uvco */
	if (sd->uvco && ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		for (i=0; i<sd->totuv; i++) {
			if (!ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
				MFace *mface = dm->getTessFaceData(dm, num, CD_MFACE);
				MTFace *mtface = (MTFace*)CustomData_get_layer_n(&dm->faceData, CD_MTFACE, i);
				mtface += num;
				
				psys_interpolate_uvs(mtface, mface->v4, fuv, sd->uvco + 2 * i);
			}
			else {
				sd->uvco[2*i] = 0.0f;
				sd->uvco[2*i + 1] = 0.0f;
			}
		}
	}

	/* get mcol */
	if (sd->mcol && ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		for (i=0; i<sd->totcol; i++) {
			if (!ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
				MFace *mface = dm->getTessFaceData(dm, num, CD_MFACE);
				MCol *mc = (MCol*)CustomData_get_layer_n(&dm->faceData, CD_MCOL, i);
				mc += num * 4;

				psys_interpolate_mcol(mc, mface->v4, fuv, sd->mcol + i);
			}
			else
				memset(&sd->mcol[i], 0, sizeof(MCol));
		}
	}
}
static int render_new_particle_system(Render *re, ObjectRen *obr, ParticleSystem *psys, int timeoffset)
{
	Object *ob= obr->ob;
//	Object *tob=0;
	Material *ma = NULL;
	ParticleSystemModifierData *psmd;
	ParticleSystem *tpsys = NULL;
	ParticleSettings *part, *tpart = NULL;
	ParticleData *pars, *pa = NULL, *tpa = NULL;
	ParticleKey *states = NULL;
	ParticleKey state;
	ParticleCacheKey *cache = NULL;
	ParticleBillboardData bb;
	ParticleSimulationData sim = {NULL};
	ParticleStrandData sd;
	StrandBuffer *strandbuf = NULL;
	StrandVert *svert = NULL;
	StrandBound *sbound = NULL;
	StrandRen *strand = NULL;
	RNG *rng = NULL;
	float loc[3], loc1[3], loc0[3], mat[4][4], nmat[3][3], co[3], nor[3], duplimat[4][4];
	float strandlen=0.0f, curlen=0.0f;
	float hasize, pa_size, r_tilt, r_length;
	float pa_time, pa_birthtime, pa_dietime;
	float random, simplify[2], pa_co[3];
	const float cfra= BKE_scene_frame_get(re->scene);
	int i, a, k, max_k=0, totpart;
	bool do_simplify = false, do_surfacecache = false, use_duplimat = false;
	int totchild=0, step_nbr;
	int seed, path_nbr=0, orco1=0, num;
	int totface;

	const int *index_mf_to_mpoly = NULL;
	const int *index_mp_to_orig = NULL;

/* 1. check that everything is ok & updated */
	if (psys==NULL)
		return 0;

	part=psys->part;
	pars=psys->particles;

	if (part==NULL || pars==NULL || !psys_check_enabled(ob, psys, G.is_rendering))
		return 0;
	
	if (part->ren_as==PART_DRAW_OB || part->ren_as==PART_DRAW_GR || part->ren_as==PART_DRAW_NOT)
		return 1;

	if ((re->r.scemode & R_VIEWPORT_PREVIEW) && (ob->mode & OB_MODE_PARTICLE_EDIT))
		return 0;

	if (part->ren_as == PART_DRAW_BB && part->bb_ob == NULL && RE_GetCamera(re) == NULL)
		return 0;

/* 2. start initializing things */

	/* last possibility to bail out! */
	psmd = psys_get_modifier(ob, psys);
	if (!(psmd->modifier.mode & eModifierMode_Render))
		return 0;

	sim.scene= re->scene;
	sim.ob= ob;
	sim.psys= psys;
	sim.psmd= psmd;

	if (part->phystype==PART_PHYS_KEYED)
		psys_count_keyed_targets(&sim);

	totchild=psys->totchild;

	/* can happen for disconnected/global hair */
	if (part->type==PART_HAIR && !psys->childcache)
		totchild= 0;

	if (re->r.scemode & R_VIEWPORT_PREVIEW) { /* preview render */
		totchild = (int)((float)totchild * (float)part->disp / 100.0f);
		step_nbr = 1 << part->draw_step;
	}
	else {
		step_nbr = 1 << part->ren_step;
	}
	if (ELEM(part->kink, PART_KINK_SPIRAL))
		step_nbr += part->kink_extra_steps;

	psys->flag |= PSYS_DRAWING;

	rng= BLI_rng_new(psys->seed);

	totpart=psys->totpart;

	memset(&sd, 0, sizeof(ParticleStrandData));
	sd.override_uv = -1;

/* 2.1 setup material stff */
	ma= give_render_material(re, ob, part->omat);
	
#if 0  /* XXX old animation system */
	if (ma->ipo) {
		calc_ipo(ma->ipo, cfra);
		execute_ipo((ID *)ma, ma->ipo);
	}
#endif  /* XXX old animation system */

	hasize = ma->hasize;
	seed = ma->seed1;

	re->flag |= R_HALO;

	RE_set_customdata_names(obr, &psmd->dm_final->faceData);
	sd.totuv = CustomData_number_of_layers(&psmd->dm_final->faceData, CD_MTFACE);
	sd.totcol = CustomData_number_of_layers(&psmd->dm_final->faceData, CD_MCOL);

	if (ma->texco & TEXCO_UV && sd.totuv) {
		sd.uvco = MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");

		if (ma->strand_uvname[0]) {
			sd.override_uv = CustomData_get_named_layer_index(&psmd->dm_final->faceData, CD_MTFACE, ma->strand_uvname);
			sd.override_uv -= CustomData_get_layer_index(&psmd->dm_final->faceData, CD_MTFACE);
		}
	}
	else
		sd.uvco = NULL;

	if (sd.totcol)
		sd.mcol = MEM_callocN(sd.totcol * sizeof(MCol), "particle_mcols");

/* 2.2 setup billboards */
	if (part->ren_as == PART_DRAW_BB) {
		int first_uv = CustomData_get_layer_index(&psmd->dm_final->faceData, CD_MTFACE);

		bb.uv[0] = CustomData_get_named_layer_index(&psmd->dm_final->faceData, CD_MTFACE, psys->bb_uvname[0]);
		if (bb.uv[0] < 0)
			bb.uv[0] = CustomData_get_active_layer_index(&psmd->dm_final->faceData, CD_MTFACE);

		bb.uv[1] = CustomData_get_named_layer_index(&psmd->dm_final->faceData, CD_MTFACE, psys->bb_uvname[1]);

		bb.uv[2] = CustomData_get_named_layer_index(&psmd->dm_final->faceData, CD_MTFACE, psys->bb_uvname[2]);

		if (first_uv >= 0) {
			bb.uv[0] -= first_uv;
			bb.uv[1] -= first_uv;
			bb.uv[2] -= first_uv;
		}

		bb.align = part->bb_align;
		bb.anim = part->bb_anim;
		bb.lock = part->draw & PART_DRAW_BB_LOCK;
		bb.ob = (part->bb_ob ? part->bb_ob : RE_GetCamera(re));
		bb.split_offset = part->bb_split_offset;
		bb.totnum = totpart+totchild;
		bb.uv_split = part->bb_uv_split;
	}
	
/* 2.5 setup matrices */
	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	invert_m4_m4(ob->imat, mat);	/* need to be that way, for imat texture */
	transpose_m3_m4(nmat, ob->imat);

	if (psys->flag & PSYS_USE_IMAT) {
		/* psys->imat is the original emitter's inverse matrix, ob->obmat is the duplicated object's matrix */
		mul_m4_m4m4(duplimat, ob->obmat, psys->imat);
		use_duplimat = true;
	}

/* 2.6 setup strand rendering */
	if (part->ren_as == PART_DRAW_PATH && psys->pathcache) {
		path_nbr = step_nbr;

		if (path_nbr) {
			if (!ELEM(ma->material_type, MA_TYPE_HALO, MA_TYPE_WIRE)) {
				sd.orco = get_object_orco(re, psys);
				if (!sd.orco) {
					sd.orco = MEM_mallocN(3*sizeof(float)*(totpart+totchild), "particle orcos");
					set_object_orco(re, psys, sd.orco);
				}
			}
		}

		if (part->draw & PART_DRAW_REN_ADAPT) {
			sd.adapt = 1;
			sd.adapt_pix = (float)part->adapt_pix;
			sd.adapt_angle = cosf(DEG2RADF((float)part->adapt_angle));
		}

		if (part->draw & PART_DRAW_REN_STRAND) {
			strandbuf= RE_addStrandBuffer(obr, (totpart+totchild)*(path_nbr+1));
			strandbuf->ma= ma;
			strandbuf->lay= ob->lay;
			copy_m4_m4(strandbuf->winmat, re->winmat);
			strandbuf->winx= re->winx;
			strandbuf->winy= re->winy;
			strandbuf->maxdepth= 2;
			strandbuf->adaptcos= cosf(DEG2RADF((float)part->adapt_angle));
			strandbuf->overrideuv= sd.override_uv;
			strandbuf->minwidth= ma->strand_min;

			if (ma->strand_widthfade == 0.0f)
				strandbuf->widthfade= -1.0f;
			else if (ma->strand_widthfade >= 1.0f)
				strandbuf->widthfade= 2.0f - ma->strand_widthfade;
			else
				strandbuf->widthfade= 1.0f/MAX2(ma->strand_widthfade, 1e-5f);

			if (part->flag & PART_HAIR_BSPLINE)
				strandbuf->flag |= R_STRAND_BSPLINE;
			if (ma->mode & MA_STR_B_UNITS)
				strandbuf->flag |= R_STRAND_B_UNITS;

			svert= strandbuf->vert;

			if (re->r.mode & R_SPEED)
				do_surfacecache = true;
			else if ((re->wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && (re->wrld.ao_gather_method == WO_AOGATHER_APPROX))
				if (ma->amb != 0.0f)
					do_surfacecache = true;

			totface= psmd->dm_final->getNumTessFaces(psmd->dm_final);
			index_mf_to_mpoly = psmd->dm_final->getTessFaceDataArray(psmd->dm_final, CD_ORIGINDEX);
			index_mp_to_orig = psmd->dm_final->getPolyDataArray(psmd->dm_final, CD_ORIGINDEX);
			if (index_mf_to_mpoly == NULL) {
				index_mp_to_orig = NULL;
			}
			for (a=0; a<totface; a++)
				strandbuf->totbound = max_ii(strandbuf->totbound, (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a): a);

			strandbuf->totbound++;
			strandbuf->bound= MEM_callocN(sizeof(StrandBound)*strandbuf->totbound, "StrandBound");
			sbound= strandbuf->bound;
			sbound->start= sbound->end= 0;
		}
	}

	if (sd.orco == NULL) {
		sd.orco = MEM_mallocN(3 * sizeof(float), "particle orco");
		orco1 = 1;
	}

	if (path_nbr == 0)
		psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

/* 3. start creating renderable things */
	for (a=0, pa=pars; a<totpart+totchild; a++, pa++, seed++) {
		random = BLI_rng_get_float(rng);
		/* setup per particle individual stuff */
		if (a<totpart) {
			if (pa->flag & PARS_UNEXIST) continue;

			pa_time=(cfra-pa->time)/pa->lifetime;
			pa_birthtime = pa->time;
			pa_dietime = pa->dietime;

			hasize = ma->hasize;

			/* XXX 'tpsys' is alwyas NULL, this code won't run! */
			/* get orco */
			if (tpsys && part->phystype == PART_PHYS_NO) {
				tpa = tpsys->particles + pa->num;
				psys_particle_on_emitter(
				        psmd,
				        tpart->from, tpa->num, pa->num_dmcache, tpa->fuv,
				        tpa->foffset, co, nor, NULL, NULL, sd.orco, NULL);
			}
			else {
				psys_particle_on_emitter(
				        psmd,
				        part->from, pa->num, pa->num_dmcache,
				        pa->fuv, pa->foffset, co, nor, NULL, NULL, sd.orco, NULL);
			}

			/* get uvco & mcol */
			num= pa->num_dmcache;

			if (num == DMCACHE_NOTFOUND)
				if (pa->num < psmd->dm_final->getNumTessFaces(psmd->dm_final))
					num= pa->num;

			get_particle_uvco_mcol(part->from, psmd->dm_final, pa->fuv, num, &sd);

			pa_size = pa->size;

			r_tilt = 2.0f*(psys_frand(psys, a) - 0.5f);
			r_length = psys_frand(psys, a+1);

			if (path_nbr) {
				cache = psys->pathcache[a];
				max_k = (int)cache->segments;
			}

			if (totchild && (part->draw&PART_DRAW_PARENT)==0) continue;
		}
		else {
			ChildParticle *cpa= psys->child+a-totpart;

			if (path_nbr) {
				cache = psys->childcache[a-totpart];

				if (cache->segments < 0)
					continue;

				max_k = (int)cache->segments;
			}
			
			pa_time = psys_get_child_time(psys, cpa, cfra, &pa_birthtime, &pa_dietime);
			pa_size = psys_get_child_size(psys, cpa, cfra, &pa_time);

			r_tilt = 2.0f*(psys_frand(psys, a + 21) - 0.5f);
			r_length = psys_frand(psys, a + 22);

			num = cpa->num;

			/* get orco */
			if (part->childtype == PART_CHILD_FACES) {
				psys_particle_on_emitter(
				        psmd,
				        PART_FROM_FACE, cpa->num, DMCACHE_ISCHILD,
				        cpa->fuv, cpa->foffset, co, nor, NULL, NULL, sd.orco, NULL);
			}
			else {
				ParticleData *par = psys->particles + cpa->parent;
				psys_particle_on_emitter(
				        psmd,
				        part->from, par->num, DMCACHE_ISCHILD, par->fuv,
				        par->foffset, co, nor, NULL, NULL, sd.orco, NULL);
			}

			/* get uvco & mcol */
			if (part->childtype==PART_CHILD_FACES) {
				get_particle_uvco_mcol(PART_FROM_FACE, psmd->dm_final, cpa->fuv, cpa->num, &sd);
			}
			else {
				ParticleData *parent = psys->particles + cpa->parent;
				num = parent->num_dmcache;

				if (num == DMCACHE_NOTFOUND)
					if (parent->num < psmd->dm_final->getNumTessFaces(psmd->dm_final))
						num = parent->num;

				get_particle_uvco_mcol(part->from, psmd->dm_final, parent->fuv, num, &sd);
			}

			do_simplify = psys_render_simplify_params(psys, cpa, simplify);

			if (strandbuf) {
				int orignum = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, cpa->num) : cpa->num;

				if ((orignum > sbound - strandbuf->bound) &&
				    (orignum < strandbuf->totbound))
				{
					sbound = &strandbuf->bound[orignum];
					sbound->start = sbound->end = obr->totstrand;
				}
			}
		}

		/* TEXCO_PARTICLE */
		pa_co[0] = pa_time;
		pa_co[1] = 0.f;
		pa_co[2] = 0.f;

		/* surface normal shading setup */
		if (ma->mode_l & MA_STR_SURFDIFF) {
			mul_m3_v3(nmat, nor);
			sd.surfnor= nor;
		}
		else
			sd.surfnor= NULL;

		/* strand render setup */
		if (strandbuf) {
			strand= RE_findOrAddStrand(obr, obr->totstrand++);
			strand->buffer= strandbuf;
			strand->vert= svert;
			copy_v3_v3(strand->orco, sd.orco);

			if (do_simplify) {
				float *ssimplify= RE_strandren_get_simplify(obr, strand, 1);
				ssimplify[0]= simplify[0];
				ssimplify[1]= simplify[1];
			}

			if (sd.surfnor) {
				float *snor= RE_strandren_get_surfnor(obr, strand, 1);
				copy_v3_v3(snor, sd.surfnor);
			}

			if (do_surfacecache && num >= 0) {
				int *facenum= RE_strandren_get_face(obr, strand, 1);
				*facenum= num;
			}

			if (sd.uvco) {
				for (i=0; i<sd.totuv; i++) {
					if (i != sd.override_uv) {
						float *uv= RE_strandren_get_uv(obr, strand, i, NULL, 1);

						uv[0]= sd.uvco[2*i];
						uv[1]= sd.uvco[2*i+1];
					}
				}
			}
			if (sd.mcol) {
				for (i=0; i<sd.totcol; i++) {
					MCol *mc= RE_strandren_get_mcol(obr, strand, i, NULL, 1);
					*mc = sd.mcol[i];
				}
			}

			sbound->end++;
		}

		/* strandco computation setup */
		if (path_nbr) {
			strandlen= 0.0f;
			curlen= 0.0f;
			for (k=1; k<=path_nbr; k++)
				if (k<=max_k)
					strandlen += len_v3v3((cache+k-1)->co, (cache+k)->co);
		}

		if (path_nbr) {
			/* render strands */
			for (k=0; k<=path_nbr; k++) {
				float time;

				if (k<=max_k) {
					copy_v3_v3(state.co, (cache+k)->co);
					copy_v3_v3(state.vel, (cache+k)->vel);
				}
				else
					continue;

				if (k > 0)
					curlen += len_v3v3((cache+k-1)->co, (cache+k)->co);
				time= curlen/strandlen;

				copy_v3_v3(loc, state.co);
				mul_m4_v3(re->viewmat, loc);

				if (strandbuf) {
					copy_v3_v3(svert->co, loc);
					svert->strandco= -1.0f + 2.0f*time;
					svert++;
					strand->totvert++;
				}
				else {
					sd.size = hasize;

					if (k==1) {
						sd.first = 1;
						sd.time = 0.0f;
						sub_v3_v3v3(loc0, loc1, loc);
						add_v3_v3v3(loc0, loc1, loc0);

						particle_curve(re, obr, psmd->dm_final, ma, &sd, loc1, loc0, seed, pa_co);
					}

					sd.first = 0;
					sd.time = time;

					if (k)
						particle_curve(re, obr, psmd->dm_final, ma, &sd, loc, loc1, seed, pa_co);

					copy_v3_v3(loc1, loc);
				}
			}

		}
		else {
			/* render normal particles */
			if (part->trail_count > 1) {
				float length = part->path_end * (1.0f - part->randlength * r_length);
				int trail_count = part->trail_count * (1.0f - part->randlength * r_length);
				float ct = (part->draw & PART_ABS_PATH_TIME) ? cfra : pa_time;
				float dt = length / (trail_count ? (float)trail_count : 1.0f);

				/* make sure we have pointcache in memory before getting particle on path */
				psys_make_temp_pointcache(ob, psys);

				for (i=0; i < trail_count; i++, ct -= dt) {
					if (part->draw & PART_ABS_PATH_TIME) {
						if (ct < pa_birthtime || ct > pa_dietime)
							continue;
					}
					else if (ct < 0.0f || ct > 1.0f)
						continue;

					state.time = (part->draw & PART_ABS_PATH_TIME) ? -ct : ct;
					psys_get_particle_on_path(&sim, a, &state, 1);

					if (psys->parent)
						mul_m4_v3(psys->parent->obmat, state.co);

					if (use_duplimat)
						mul_m4_v4(duplimat, state.co);

					if (part->ren_as == PART_DRAW_BB) {
						bb.random = random;
						bb.offset[0] = part->bb_offset[0];
						bb.offset[1] = part->bb_offset[1];
						bb.size[0] = part->bb_size[0] * pa_size;
						if (part->bb_align==PART_BB_VEL) {
							float pa_vel = len_v3(state.vel);
							float head = part->bb_vel_head*pa_vel;
							float tail = part->bb_vel_tail*pa_vel;
							bb.size[1] = part->bb_size[1]*pa_size + head + tail;
							/* use offset to adjust the particle center. this is relative to size, so need to divide! */
							if (bb.size[1] > 0.0f)
								bb.offset[1] += (head-tail) / bb.size[1];
						}
						else
							bb.size[1] = part->bb_size[1] * pa_size;
						bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
						bb.time = ct;
						bb.num = a;
					}

					pa_co[0] = (part->draw & PART_ABS_PATH_TIME) ? (ct-pa_birthtime)/(pa_dietime-pa_birthtime) : ct;
					pa_co[1] = (float)i/(float)(trail_count-1);

					particle_normal_ren(part->ren_as, part, re, obr, psmd->dm_final, ma, &sd, &bb, &state, seed, hasize, pa_co);
				}
			}
			else {
				state.time=cfra;
				if (psys_get_particle_state(&sim, a, &state, 0)==0)
					continue;

				if (psys->parent)
					mul_m4_v3(psys->parent->obmat, state.co);

				if (use_duplimat)
					mul_m4_v3(duplimat, state.co);

				if (part->ren_as == PART_DRAW_BB) {
					bb.random = random;
					bb.offset[0] = part->bb_offset[0];
					bb.offset[1] = part->bb_offset[1];
					bb.size[0] = part->bb_size[0] * pa_size;
					if (part->bb_align==PART_BB_VEL) {
						float pa_vel = len_v3(state.vel);
						float head = part->bb_vel_head*pa_vel;
						float tail = part->bb_vel_tail*pa_vel;
						bb.size[1] = part->bb_size[1]*pa_size + head + tail;
						/* use offset to adjust the particle center. this is relative to size, so need to divide! */
						if (bb.size[1] > 0.0f)
							bb.offset[1] += (head-tail) / bb.size[1];
					}
					else
						bb.size[1] = part->bb_size[1] * pa_size;
					bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
					bb.time = pa_time;
					bb.num = a;
					bb.lifetime = pa_dietime-pa_birthtime;
				}

				particle_normal_ren(part->ren_as, part, re, obr, psmd->dm_final, ma, &sd, &bb, &state, seed, hasize, pa_co);
			}
		}

		if (orco1==0)
			sd.orco+=3;

		if (re->test_break(re->tbh))
			break;
	}

	if (do_surfacecache)
		strandbuf->surface= cache_strand_surface(re, obr, psmd->dm_final, mat, timeoffset);

/* 4. clean up */
#if 0  /* XXX old animation system */
	if (ma) do_mat_ipo(re->scene, ma);
#endif  /* XXX old animation system */

	if (orco1)
		MEM_freeN(sd.orco);

	if (sd.uvco)
		MEM_freeN(sd.uvco);
	
	if (sd.mcol)
		MEM_freeN(sd.mcol);

	if (states)
		MEM_freeN(states);
	
	BLI_rng_free(rng);

	psys->flag &= ~PSYS_DRAWING;

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}

	if (path_nbr && (ma->mode_l & MA_TANGENT_STR)==0)
		calc_vertexnormals(re, obr, 1, 0, 0);

	return 1;
}

/* ------------------------------------------------------------------------- */
/* Halo's   																 */
/* ------------------------------------------------------------------------- */

static void make_render_halos(Render *re, ObjectRen *obr, Mesh *UNUSED(me), int totvert, MVert *mvert, Material *ma, float *orco)
{
	Object *ob= obr->ob;
	HaloRen *har;
	float xn, yn, zn, nor[3], view[3];
	float vec[3], hasize, mat[4][4], imat[3][3];
	int a, ok, seed= ma->seed1;

	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	copy_m3_m4(imat, ob->imat);

	re->flag |= R_HALO;

	for (a=0; a<totvert; a++, mvert++) {
		ok= 1;

		if (ok) {
			hasize= ma->hasize;

			copy_v3_v3(vec, mvert->co);
			mul_m4_v3(mat, vec);

			if (ma->mode & MA_HALOPUNO) {
				xn= mvert->no[0];
				yn= mvert->no[1];
				zn= mvert->no[2];

				/* transpose ! */
				nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				normalize_v3(nor);

				copy_v3_v3(view, vec);
				normalize_v3(view);

				zn = dot_v3v3(nor, view);
				if (zn>=0.0f) hasize= 0.0f;
				else hasize*= zn*zn*zn*zn;
			}

			if (orco) har= RE_inithalo(re, obr, ma, vec, NULL, orco, hasize, 0.0, seed);
			else har= RE_inithalo(re, obr, ma, vec, NULL, mvert->co, hasize, 0.0, seed);
			if (har) har->lay= ob->lay;
		}
		if (orco) orco+= 3;
		seed++;
	}
}

static int verghalo(const void *a1, const void *a2)
{
	const HaloRen *har1= *(const HaloRen**)a1;
	const HaloRen *har2= *(const HaloRen**)a2;
	
	if (har1->zs < har2->zs) return 1;
	else if (har1->zs > har2->zs) return -1;
	return 0;
}

static void sort_halos(Render *re, int totsort)
{
	ObjectRen *obr;
	HaloRen *har= NULL, **haso;
	int a;

	if (re->tothalo==0) return;

	re->sortedhalos= MEM_callocN(sizeof(HaloRen*)*re->tothalo, "sorthalos");
	haso= re->sortedhalos;

	for (obr=re->objecttable.first; obr; obr=obr->next) {
		for (a=0; a<obr->tothalo; a++) {
			if ((a & 255)==0) har= obr->bloha[a>>8];
			else har++;

			*(haso++)= har;
		}
	}

	qsort(re->sortedhalos, totsort, sizeof(HaloRen*), verghalo);
}

/* ------------------------------------------------------------------------- */
/* Displacement Mapping														 */
/* ------------------------------------------------------------------------- */

static short test_for_displace(Render *re, Object *ob)
{
	/* return 1 when this object uses displacement textures. */
	Material *ma;
	int i;
	
	for (i=1; i<=ob->totcol; i++) {
		ma=give_render_material(re, ob, i);
		/* ma->mapto is ORed total of all mapto channels */
		if (ma && (ma->mapto & MAP_DISPLACE)) return 1;
	}
	return 0;
}

static void displace_render_vert(Render *re, ObjectRen *obr, ShadeInput *shi, VertRen *vr, int vindex, float *scale)
{
	MTFace *tface;
	short texco= shi->mat->texco;
	float sample=0, displace[3];
	char *name;
	int i;

	/* shi->co is current render coord, just make sure at least some vector is here */
	copy_v3_v3(shi->co, vr->co);
	/* vertex normal is used for textures type 'col' and 'var' */
	copy_v3_v3(shi->vn, vr->n);

	if (texco & TEXCO_UV) {
		shi->totuv= 0;
		shi->actuv= obr->actmtface;

		for (i=0; (tface=RE_vlakren_get_tface(obr, shi->vlr, i, &name, 0)); i++) {
			ShadeInputUV *suv= &shi->uv[i];

			/* shi.uv needs scale correction from tface uv */
			suv->uv[0]= 2*tface->uv[vindex][0]-1.0f;
			suv->uv[1]= 2*tface->uv[vindex][1]-1.0f;
			suv->uv[2]= 0.0f;
			suv->name= name;
			shi->totuv++;
		}
	}

	/* set all rendercoords, 'texco' is an ORed value for all textures needed */
	if ((texco & TEXCO_ORCO) && (vr->orco)) {
		copy_v3_v3(shi->lo, vr->orco);
	}
	if (texco & TEXCO_GLOB) {
		copy_v3_v3(shi->gl, shi->co);
		mul_m4_v3(re->viewinv, shi->gl);
	}
	if (texco & TEXCO_NORM) {
		copy_v3_v3(shi->orn, shi->vn);
	}
	if (texco & TEXCO_REFL) {
		/* not (yet?) */
	}
	if (texco & TEXCO_STRESS) {
		const float *s= RE_vertren_get_stress(obr, vr, 0);

		if (s) {
			shi->stress= *s;
			if (shi->stress<1.0f) shi->stress-= 1.0f;
			else shi->stress= (shi->stress-1.0f)/shi->stress;
		}
		else
			shi->stress= 0.0f;
	}

	shi->displace[0]= shi->displace[1]= shi->displace[2]= 0.0;
	
	do_material_tex(shi, re);
	
	//printf("no=%f, %f, %f\nbefore co=%f, %f, %f\n", vr->n[0], vr->n[1], vr->n[2], 
	//vr->co[0], vr->co[1], vr->co[2]);

	displace[0]= shi->displace[0] * scale[0];
	displace[1]= shi->displace[1] * scale[1];
	displace[2]= shi->displace[2] * scale[2];

	/* 0.5 could become button once?  */
	vr->co[0] += displace[0]; 
	vr->co[1] += displace[1];
	vr->co[2] += displace[2];
	
	//printf("after co=%f, %f, %f\n", vr->co[0], vr->co[1], vr->co[2]); 
	
	/* we just don't do this vertex again, bad luck for other face using same vertex with
	 * different material... */
	vr->flag |= 1;
	
	/* Pass sample back so displace_face can decide which way to split the quad */
	sample  = shi->displace[0]*shi->displace[0];
	sample += shi->displace[1]*shi->displace[1];
	sample += shi->displace[2]*shi->displace[2];
	
	vr->accum=sample; 
	/* Should be sqrt(sample), but I'm only looking for "bigger".  Save the cycles. */
	return;
}

static void displace_render_face(Render *re, ObjectRen *obr, VlakRen *vlr, float *scale)
{
	ShadeInput shi;

	/* Warning, This is not that nice, and possibly a bit slow,
	 * however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
	
	/* set up shadeinput struct for multitex() */
	
	/* memset above means we don't need this */
	/*shi.osatex= 0;*/		/* signal not to use dx[] and dy[] texture AA vectors */

	shi.obr= obr;
	shi.vlr= vlr;		/* current render face */
	shi.mat= vlr->mat;		/* current input material */
	shi.thread= 0;
	
	/* TODO, assign these, displacement with new bumpmap is skipped without - campbell */
#if 0
	/* order is not known ? */
	shi.v1= vlr->v1;
	shi.v2= vlr->v2;
	shi.v3= vlr->v3;
#endif

	/* Displace the verts, flag is set when done */
	if (!vlr->v1->flag)
		displace_render_vert(re, obr, &shi, vlr->v1, 0,  scale);
	
	if (!vlr->v2->flag)
		displace_render_vert(re, obr, &shi, vlr->v2, 1, scale);

	if (!vlr->v3->flag)
		displace_render_vert(re, obr, &shi, vlr->v3, 2, scale);

	if (vlr->v4) {
		if (!vlr->v4->flag)
			displace_render_vert(re, obr, &shi, vlr->v4, 3, scale);

		/*	closest in displace value.  This will help smooth edges.   */ 
		if (fabsf(vlr->v1->accum - vlr->v3->accum) > fabsf(vlr->v2->accum - vlr->v4->accum)) vlr->flag |=  R_DIVIDE_24;
		else                                                                                 vlr->flag &= ~R_DIVIDE_24;
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if (vlr->v4) {
		normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}
	else {
		normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}
}

static void displace(Render *re, ObjectRen *obr)
{
	VertRen *vr;
	VlakRen *vlr;
//	float min[3]={1e30, 1e30, 1e30}, max[3]={-1e30, -1e30, -1e30};
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3];//, xn
	int i; //, texflag=0;
	Object *obt;
		
	/* Object Size with parenting */
	obt=obr->ob;
	while (obt) {
		mul_v3_v3v3(temp, obt->size, obt->dscale);
		scale[0]*=temp[0]; scale[1]*=temp[1]; scale[2]*=temp[2];
		obt=obt->parent;
	}
	
	/* Clear all flags */
	for (i=0; i<obr->totvert; i++) {
		vr= RE_findOrAddVert(obr, i);
		vr->flag= 0;
	}

	for (i=0; i<obr->totvlak; i++) {
		vlr=RE_findOrAddVlak(obr, i);
		displace_render_face(re, obr, vlr, scale);
	}
	
	/* Recalc vertex normals */
	calc_vertexnormals(re, obr, 1, 0, 0);
}

/* ------------------------------------------------------------------------- */
/* Metaball   																 */
/* ------------------------------------------------------------------------- */

static void init_render_mball(Render *re, ObjectRen *obr)
{
	Object *ob= obr->ob;
	DispList *dl;
	VertRen *ver;
	VlakRen *vlr, *vlr1;
	Material *ma;
	float *data, *nors, *orco=NULL, mat[4][4], imat[3][3], xn, yn, zn;
	int a, need_orco, vlakindex, *index, negative_scale;
	ListBase dispbase= {NULL, NULL};

	if (ob!=BKE_mball_basis_find(re->scene, ob))
		return;

	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	invert_m4_m4(ob->imat, mat);
	copy_m3_m4(imat, ob->imat);
	negative_scale = is_negative_m4(mat);

	ma= give_render_material(re, ob, 1);

	need_orco= 0;
	if (ma->texco & TEXCO_ORCO) {
		need_orco= 1;
	}

	BKE_displist_make_mball_forRender(re->eval_ctx, re->scene, ob, &dispbase);
	dl= dispbase.first;
	if (dl == NULL) return;

	data= dl->verts;
	nors= dl->nors;
	if (need_orco) {
		orco= get_object_orco(re, ob);

		if (!orco) {
			/* orco hasn't been found in cache - create new one and add to cache */
			orco= BKE_mball_make_orco(ob, &dispbase);
			set_object_orco(re, ob, orco);
		}
	}

	for (a=0; a<dl->nr; a++, data+=3, nors+=3) {

		ver= RE_findOrAddVert(obr, obr->totvert++);
		copy_v3_v3(ver->co, data);
		mul_m4_v3(mat, ver->co);

		/* render normals are inverted */
		xn= -nors[0];
		yn= -nors[1];
		zn= -nors[2];

		/* transpose ! */
		ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		normalize_v3(ver->n);
		//if (ob->transflag & OB_NEG_SCALE) negate_v3(ver->n);
		
		if (need_orco) {
			ver->orco= orco;
			orco+=3;
		}
	}

	index= dl->index;
	for (a=0; a<dl->parts; a++, index+=4) {

		vlr= RE_findOrAddVlak(obr, obr->totvlak++);
		vlr->v1= RE_findOrAddVert(obr, index[0]);
		vlr->v2= RE_findOrAddVert(obr, index[1]);
		vlr->v3= RE_findOrAddVert(obr, index[2]);
		vlr->v4 = NULL;

		if (negative_scale)
			normal_tri_v3(vlr->n, vlr->v1->co, vlr->v2->co, vlr->v3->co);
		else
			normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);

		vlr->mat= ma;
		vlr->flag= ME_SMOOTH;
		vlr->ec= 0;

		/* mball -too bad- always has triangles, because quads can be non-planar */
		if (index[3] && index[3]!=index[2]) {
			vlr1= RE_findOrAddVlak(obr, obr->totvlak++);
			vlakindex= vlr1->index;
			*vlr1= *vlr;
			vlr1->index= vlakindex;
			vlr1->v2= vlr1->v3;
			vlr1->v3= RE_findOrAddVert(obr, index[3]);
			if (negative_scale)
				normal_tri_v3(vlr1->n, vlr1->v1->co, vlr1->v2->co, vlr1->v3->co);
			else
				normal_tri_v3(vlr1->n, vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
		}
	}

	/* enforce display lists remade */
	BKE_displist_free(&dispbase);
}

/* ------------------------------------------------------------------------- */
/* Surfaces and Curves														 */
/* ------------------------------------------------------------------------- */

/* returns amount of vertices added for orco */
static int dl_surf_to_renderdata(ObjectRen *obr, DispList *dl, Material **matar, float *orco, float mat[4][4])
{
	VertRen *v1, *v2, *v3, *v4, *ver;
	VlakRen *vlr, *vlr1, *vlr2, *vlr3;
	float *data, n1[3];
	int u, v, orcoret= 0;
	int p1, p2, p3, p4, a;
	int sizeu, nsizeu, sizev, nsizev;
	int startvert, startvlak;
	
	startvert= obr->totvert;
	nsizeu = sizeu = dl->parts; nsizev = sizev = dl->nr; 
	
	data= dl->verts;
	for (u = 0; u < sizeu; u++) {
		v1 = RE_findOrAddVert(obr, obr->totvert++); /* save this for possible V wrapping */
		copy_v3_v3(v1->co, data); data += 3;
		if (orco) {
			v1->orco= orco; orco+= 3; orcoret++;
		}
		mul_m4_v3(mat, v1->co);
		
		for (v = 1; v < sizev; v++) {
			ver= RE_findOrAddVert(obr, obr->totvert++);
			copy_v3_v3(ver->co, data); data += 3;
			if (orco) {
				ver->orco= orco; orco+= 3; orcoret++;
			}
			mul_m4_v3(mat, ver->co);
		}
		/* if V-cyclic, add extra vertices at end of the row */
		if (dl->flag & DL_CYCL_U) {
			ver= RE_findOrAddVert(obr, obr->totvert++);
			copy_v3_v3(ver->co, v1->co);
			if (orco) {
				ver->orco= orco; orco+=3; orcoret++; //orcobase + 3*(u*sizev + 0);
			}
		}
	}
	
	/* Done before next loop to get corner vert */
	if (dl->flag & DL_CYCL_U) nsizev++;
	if (dl->flag & DL_CYCL_V) nsizeu++;
	
	/* if U cyclic, add extra row at end of column */
	if (dl->flag & DL_CYCL_V) {
		for (v = 0; v < nsizev; v++) {
			v1= RE_findOrAddVert(obr, startvert + v);
			ver= RE_findOrAddVert(obr, obr->totvert++);
			copy_v3_v3(ver->co, v1->co);
			if (orco) {
				ver->orco= orco; orco+=3; orcoret++; //ver->orco= orcobase + 3*(0*sizev + v);
			}
		}
	}
	
	sizeu = nsizeu;
	sizev = nsizev;
	
	startvlak= obr->totvlak;
	
	for (u = 0; u < sizeu - 1; u++) {
		p1 = startvert + u * sizev; /* walk through face list */
		p2 = p1 + 1;
		p3 = p2 + sizev;
		p4 = p3 - 1;
		
		for (v = 0; v < sizev - 1; v++) {
			v1= RE_findOrAddVert(obr, p1);
			v2= RE_findOrAddVert(obr, p2);
			v3= RE_findOrAddVert(obr, p3);
			v4= RE_findOrAddVert(obr, p4);
			
			vlr= RE_findOrAddVlak(obr, obr->totvlak++);
			vlr->v1= v1; vlr->v2= v2; vlr->v3= v3; vlr->v4= v4;
			
			normal_quad_v3(n1, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
			
			copy_v3_v3(vlr->n, n1);
			
			vlr->mat= matar[ dl->col];
			vlr->ec= ME_V1V2+ME_V2V3;
			vlr->flag= dl->rt;
			
			add_v3_v3(v1->n, n1);
			add_v3_v3(v2->n, n1);
			add_v3_v3(v3->n, n1);
			add_v3_v3(v4->n, n1);
			
			p1++; p2++; p3++; p4++;
		}
	}
	/* fix normals for U resp. V cyclic faces */
	sizeu--; sizev--;  /* dec size for face array */
	if (dl->flag & DL_CYCL_V) {
		
		for (v = 0; v < sizev; v++) {
			/* optimize! :*/
			vlr= RE_findOrAddVlak(obr, UVTOINDEX(sizeu - 1, v));
			vlr1= RE_findOrAddVlak(obr, UVTOINDEX(0, v));
			add_v3_v3(vlr1->v1->n, vlr->n);
			add_v3_v3(vlr1->v2->n, vlr->n);
			add_v3_v3(vlr->v3->n, vlr1->n);
			add_v3_v3(vlr->v4->n, vlr1->n);
		}
	}
	if (dl->flag & DL_CYCL_U) {
		
		for (u = 0; u < sizeu; u++) {
			/* optimize! :*/
			vlr= RE_findOrAddVlak(obr, UVTOINDEX(u, 0));
			vlr1= RE_findOrAddVlak(obr, UVTOINDEX(u, sizev-1));
			add_v3_v3(vlr1->v2->n, vlr->n);
			add_v3_v3(vlr1->v3->n, vlr->n);
			add_v3_v3(vlr->v1->n, vlr1->n);
			add_v3_v3(vlr->v4->n, vlr1->n);
		}
	}

	/* last vertex is an extra case: 
	 *
	 *     ^     ()----()----()----()
	 *     |     |     |     ||     |
	 *     u     |     |(0,n)||(0,0)|
	 *     |     |     ||     |
	 *     ()====()====[]====()
	 *     |     |     ||     |
	 *     |     |(m,n)||(m,0)|
	 *     |     |     ||     |
	 *     ()----()----()----()
	 *     v ->
	 *  
	 *  vertex [] is no longer shared, therefore distribute
	 *  normals of the surrounding faces to all of the duplicates of []
	 */

	if ((dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U)) {
		vlr= RE_findOrAddVlak(obr, UVTOINDEX(sizeu - 1, sizev - 1)); /* (m, n) */
		vlr1= RE_findOrAddVlak(obr, UVTOINDEX(0, 0));  /* (0, 0) */
		add_v3_v3v3(n1, vlr->n, vlr1->n);
		vlr2= RE_findOrAddVlak(obr, UVTOINDEX(0, sizev-1)); /* (0, n) */
		add_v3_v3(n1, vlr2->n);
		vlr3= RE_findOrAddVlak(obr, UVTOINDEX(sizeu-1, 0)); /* (m, 0) */
		add_v3_v3(n1, vlr3->n);
		copy_v3_v3(vlr->v3->n, n1);
		copy_v3_v3(vlr1->v1->n, n1);
		copy_v3_v3(vlr2->v2->n, n1);
		copy_v3_v3(vlr3->v4->n, n1);
	}
	for (a = startvert; a < obr->totvert; a++) {
		ver= RE_findOrAddVert(obr, a);
		normalize_v3(ver->n);
	}
	
	
	return orcoret;
}

static void init_render_dm(DerivedMesh *dm, Render *re, ObjectRen *obr,
	int timeoffset, float *orco, float mat[4][4])
{
	Object *ob= obr->ob;
	int a, end, totvert, vertofs;
	short mat_iter;
	VertRen *ver;
	VlakRen *vlr;
	MVert *mvert = NULL;
	MFace *mface;
	Material *ma;
#ifdef WITH_FREESTYLE
	const int *index_mf_to_mpoly = NULL;
	const int *index_mp_to_orig = NULL;
	FreestyleFace *ffa = NULL;
#endif
	/* Curve *cu= ELEM(ob->type, OB_FONT, OB_CURVE) ? ob->data : NULL; */

	mvert= dm->getVertArray(dm);
	totvert= dm->getNumVerts(dm);

	for (a=0; a<totvert; a++, mvert++) {
		ver= RE_findOrAddVert(obr, obr->totvert++);
		copy_v3_v3(ver->co, mvert->co);
		mul_m4_v3(mat, ver->co);

		if (orco) {
			ver->orco= orco;
			orco+=3;
		}
	}

	if (!timeoffset) {
		/* store customdata names, because DerivedMesh is freed */
		RE_set_customdata_names(obr, &dm->faceData);

		/* still to do for keys: the correct local texture coordinate */

		/* faces in order of color blocks */
		vertofs= obr->totvert - totvert;
		for (mat_iter= 0; (mat_iter < ob->totcol || (mat_iter==0 && ob->totcol==0)); mat_iter++) {

			ma= give_render_material(re, ob, mat_iter+1);
			end= dm->getNumTessFaces(dm);
			mface= dm->getTessFaceArray(dm);

#ifdef WITH_FREESTYLE
			if (ob->type == OB_MESH) {
				Mesh *me= ob->data;
				index_mf_to_mpoly= dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
				index_mp_to_orig= dm->getPolyDataArray(dm, CD_ORIGINDEX);
				ffa= CustomData_get_layer(&me->pdata, CD_FREESTYLE_FACE);
			}
#endif

			for (a=0; a<end; a++, mface++) {
				int v1, v2, v3, v4, flag;

				if (mface->mat_nr == mat_iter) {
					float len;

					v1= mface->v1;
					v2= mface->v2;
					v3= mface->v3;
					v4= mface->v4;
					flag= mface->flag & ME_SMOOTH;

					vlr= RE_findOrAddVlak(obr, obr->totvlak++);
					vlr->v1= RE_findOrAddVert(obr, vertofs+v1);
					vlr->v2= RE_findOrAddVert(obr, vertofs+v2);
					vlr->v3= RE_findOrAddVert(obr, vertofs+v3);
					if (v4) vlr->v4= RE_findOrAddVert(obr, vertofs+v4);
					else vlr->v4 = NULL;

					/* render normals are inverted in render */
					if (vlr->v4)
						len= normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
					else
						len= normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);

					vlr->mat= ma;
					vlr->flag= flag;
					vlr->ec= 0; /* mesh edges rendered separately */
#ifdef WITH_FREESTYLE
					if (ffa) {
						int index = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a) : a;
						vlr->freestyle_face_mark= (ffa[index].flag & FREESTYLE_FACE_MARK) ? 1 : 0;
					}
					else {
						vlr->freestyle_face_mark= 0;
					}
#endif

					if (len==0) obr->totvlak--;
					else {
						CustomDataLayer *layer;
						MTFace *mtface, *mtf;
						MCol *mcol, *mc;
						int index, mtfn= 0, mcn= 0;
						char *name;

						for (index=0; index<dm->faceData.totlayer; index++) {
							layer= &dm->faceData.layers[index];
							name= layer->name;

							if (layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
								mtf= RE_vlakren_get_tface(obr, vlr, mtfn++, &name, 1);
								mtface= (MTFace*)layer->data;
								*mtf= mtface[a];
							}
							else if (layer->type == CD_MCOL && mcn < MAX_MCOL) {
								mc= RE_vlakren_get_mcol(obr, vlr, mcn++, &name, 1);
								mcol= (MCol*)layer->data;
								memcpy(mc, &mcol[a*4], sizeof(MCol)*4);
							}
						}
					}
				}
			}
		}

		/* Normals */
		calc_vertexnormals(re, obr, 1, 0, 0);
	}

}

static void init_render_surf(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Nurb *nu = NULL;
	Curve *cu;
	ListBase displist= {NULL, NULL};
	DispList *dl;
	Material **matar;
	float *orco=NULL, mat[4][4];
	int a, totmat;
	bool need_orco = false;
	DerivedMesh *dm= NULL;

	cu= ob->data;
	nu= cu->nurb.first;
	if (nu == NULL) return;

	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	invert_m4_m4(ob->imat, mat);

	/* material array */
	totmat= ob->totcol+1;
	matar= MEM_callocN(sizeof(Material*)*totmat, "init_render_surf matar");

	for (a=0; a<totmat; a++) {
		matar[a]= give_render_material(re, ob, a+1);

		if (matar[a] && matar[a]->texco & TEXCO_ORCO)
			need_orco= 1;
	}

	if (ob->parent && (ob->parent->type==OB_LATTICE)) need_orco= 1;

	BKE_displist_make_surf(re->scene, ob, &displist, &dm, 1, 0, 1);

	if (dm) {
		if (need_orco) {
			orco = get_object_orco(re, ob);
			if (!orco) {
				orco= BKE_displist_make_orco(re->scene, ob, dm, true, true);
				if (orco) {
					set_object_orco(re, ob, orco);
				}
			}
		}

		init_render_dm(dm, re, obr, timeoffset, orco, mat);
		dm->release(dm);
	}
	else {
		if (need_orco) {
			orco = get_object_orco(re, ob);
			if (!orco) {
				orco = BKE_curve_surf_make_orco(ob);
				set_object_orco(re, ob, orco);
			}
		}

		/* walk along displaylist and create rendervertices/-faces */
		for (dl=displist.first; dl; dl=dl->next) {
			/* watch out: u ^= y, v ^= x !! */
			if (dl->type==DL_SURF)
				orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
		}
	}

	BKE_displist_free(&displist);

	MEM_freeN(matar);
}

static void init_render_curve(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Curve *cu;
	VertRen *ver;
	VlakRen *vlr;
	DispList *dl;
	DerivedMesh *dm = NULL;
	ListBase disp={NULL, NULL};
	Material **matar;
	float *data, *fp, *orco=NULL;
	float n[3], mat[4][4], nmat[4][4];
	int nr, startvert, a, b, negative_scale;
	bool need_orco = false;
	int totmat;

	cu= ob->data;
	if (ob->type==OB_FONT && cu->str==NULL) return;
	else if (ob->type==OB_CURVE && cu->nurb.first==NULL) return;

	BKE_displist_make_curveTypes_forRender(re->scene, ob, &disp, &dm, false, true);
	dl= disp.first;
	if (dl==NULL) return;
	
	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	invert_m4_m4(ob->imat, mat);
	negative_scale = is_negative_m4(mat);

	/* local object -> world space transform for normals */
	transpose_m4_m4(nmat, mat);
	invert_m4(nmat);

	/* material array */
	totmat= ob->totcol+1;
	matar= MEM_callocN(sizeof(Material*)*totmat, "init_render_surf matar");

	for (a=0; a<totmat; a++) {
		matar[a]= give_render_material(re, ob, a+1);

		if (matar[a] && matar[a]->texco & TEXCO_ORCO)
			need_orco= 1;
	}

	if (dm) {
		if (need_orco) {
			orco = get_object_orco(re, ob);
			if (!orco) {
				orco = BKE_displist_make_orco(re->scene, ob, dm, true, true);
				if (orco) {
					set_object_orco(re, ob, orco);
				}
			}
		}

		init_render_dm(dm, re, obr, timeoffset, orco, mat);
		dm->release(dm);
	}
	else {
		if (need_orco) {
			orco = get_object_orco(re, ob);
			if (!orco) {
				orco = BKE_curve_make_orco(re->scene, ob, NULL);
				set_object_orco(re, ob, orco);
			}
		}

		while (dl) {
			if (dl->col > ob->totcol) {
				/* pass */
			}
			else if (dl->type==DL_INDEX3) {
				const int *index;

				startvert= obr->totvert;
				data= dl->verts;

				for (a=0; a<dl->nr; a++, data+=3) {
					ver= RE_findOrAddVert(obr, obr->totvert++);
					copy_v3_v3(ver->co, data);

					mul_m4_v3(mat, ver->co);

					if (orco) {
						ver->orco = orco;
						orco += 3;
					}
				}

				if (timeoffset==0) {
					float tmp[3];
					const int startvlak= obr->totvlak;

					zero_v3(n);
					index= dl->index;
					for (a=0; a<dl->parts; a++, index+=3) {
						int v1 = index[0], v2 = index[2], v3 = index[1];
						float *co1 = &dl->verts[v1 * 3],
						      *co2 = &dl->verts[v2 * 3],
						      *co3 = &dl->verts[v3 * 3];

						vlr= RE_findOrAddVlak(obr, obr->totvlak++);
						vlr->v1= RE_findOrAddVert(obr, startvert + v1);
						vlr->v2= RE_findOrAddVert(obr, startvert + v2);
						vlr->v3= RE_findOrAddVert(obr, startvert + v3);
						vlr->v4= NULL;

						/* to prevent float accuracy issues, we calculate normal in local object space (not world) */
						if (normal_tri_v3(tmp, co1, co2, co3) > FLT_EPSILON) {
							if (negative_scale == false) {
								add_v3_v3(n, tmp);
							}
							else {
								sub_v3_v3(n, tmp);
							}
						}

						vlr->mat= matar[ dl->col ];
						vlr->flag= 0;
						vlr->ec= 0;
					}

					/* transform normal to world space */
					mul_m4_v3(nmat, n);
					normalize_v3(n);

					/* vertex normals */
					for (a= startvlak; a<obr->totvlak; a++) {
						vlr= RE_findOrAddVlak(obr, a);

						copy_v3_v3(vlr->n, n);
						add_v3_v3(vlr->v1->n, vlr->n);
						add_v3_v3(vlr->v3->n, vlr->n);
						add_v3_v3(vlr->v2->n, vlr->n);
					}
					for (a=startvert; a<obr->totvert; a++) {
						ver= RE_findOrAddVert(obr, a);
						normalize_v3(ver->n);
					}
				}
			}
			else if (dl->type==DL_SURF) {

				/* cyclic U means an extruded full circular curve, we skip bevel splitting then */
				if (dl->flag & DL_CYCL_U) {
					orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
				}
				else {
					int p1, p2, p3, p4;

					fp= dl->verts;
					startvert= obr->totvert;
					nr= dl->nr*dl->parts;

					while (nr--) {
						ver= RE_findOrAddVert(obr, obr->totvert++);

						copy_v3_v3(ver->co, fp);
						mul_m4_v3(mat, ver->co);
						fp+= 3;

						if (orco) {
							ver->orco = orco;
							orco += 3;
						}
					}

					if (dl->flag & DL_CYCL_V && orco) {
						fp = dl->verts;
						nr = dl->nr;
						while (nr--) {
							ver = RE_findOrAddVert(obr, obr->totvert++);
							copy_v3_v3(ver->co, fp);
							mul_m4_v3(mat, ver->co);
							ver->orco = orco;
							fp += 3;
							orco += 3;
						}
					}

					if (dl->bevel_split || timeoffset == 0) {
						const int startvlak= obr->totvlak;

						for (a=0; a<dl->parts; a++) {

							if (BKE_displist_surfindex_get(dl, a, &b, &p1, &p2, &p3, &p4)==0)
								break;

							p1+= startvert;
							p2+= startvert;
							p3+= startvert;
							p4+= startvert;

							if (dl->flag & DL_CYCL_V && orco && a == dl->parts - 1) {
								p3 = p1 + dl->nr;
								p4 = p2 + dl->nr;
							}

							for (; b<dl->nr; b++) {
								vlr= RE_findOrAddVlak(obr, obr->totvlak++);
								/* important 1 offset in order is kept [#24913] */
								vlr->v1= RE_findOrAddVert(obr, p2);
								vlr->v2= RE_findOrAddVert(obr, p1);
								vlr->v3= RE_findOrAddVert(obr, p3);
								vlr->v4= RE_findOrAddVert(obr, p4);
								vlr->ec= ME_V2V3+ME_V3V4;
								if (a==0) vlr->ec+= ME_V1V2;

								vlr->flag= dl->rt;

								normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
								vlr->mat= matar[ dl->col ];

								p4= p3;
								p3++;
								p2= p1;
								p1++;
							}
						}

						if (dl->bevel_split) {
							for (a = 0; a < dl->parts - 1 + !!(dl->flag & DL_CYCL_V); a++) {
								if (BLI_BITMAP_TEST(dl->bevel_split, a)) {
									split_v_renderfaces(
									        obr, startvlak, startvert, dl->parts, dl->nr, a,
									        /* intentionally swap (v, u) --> (u, v) */
									        dl->flag & DL_CYCL_V, dl->flag & DL_CYCL_U);
								}
							}
						}

						/* vertex normals */
						for (a= startvlak; a<obr->totvlak; a++) {
							vlr= RE_findOrAddVlak(obr, a);

							add_v3_v3(vlr->v1->n, vlr->n);
							add_v3_v3(vlr->v3->n, vlr->n);
							add_v3_v3(vlr->v2->n, vlr->n);
							add_v3_v3(vlr->v4->n, vlr->n);
						}
						for (a=startvert; a<obr->totvert; a++) {
							ver= RE_findOrAddVert(obr, a);
							normalize_v3(ver->n);
						}
					}
				}
			}

			dl= dl->next;
		}
	}

	BKE_displist_free(&disp);

	MEM_freeN(matar);
}

/* ------------------------------------------------------------------------- */
/* Mesh     																 */
/* ------------------------------------------------------------------------- */

struct edgesort {
	unsigned int v1, v2;
	int f;
	unsigned int i1, i2;
};

/* edges have to be added with lowest index first for sorting */
static void to_edgesort(struct edgesort *ed,
                        unsigned int i1, unsigned int i2,
                        unsigned int v1, unsigned int v2, int f)
{
	if (v1 > v2) {
		SWAP(unsigned int, v1, v2);
		SWAP(unsigned int, i1, i2);
	}

	ed->v1= v1;
	ed->v2= v2;
	ed->i1= i1;
	ed->i2= i2;
	ed->f = f;
}

static int vergedgesort(const void *v1, const void *v2)
{
	const struct edgesort *x1=v1, *x2=v2;

	if ( x1->v1 > x2->v1) return 1;
	else if ( x1->v1 < x2->v1) return -1;
	else if ( x1->v2 > x2->v2) return 1;
	else if ( x1->v2 < x2->v2) return -1;

	return 0;
}

static struct edgesort *make_mesh_edge_lookup(DerivedMesh *dm, int *totedgesort)
{
	MFace *mf, *mface;
	MTFace *tface=NULL;
	struct edgesort *edsort, *ed;
	unsigned int *mcol=NULL;
	int a, totedge=0, totface;

	mface= dm->getTessFaceArray(dm);
	totface= dm->getNumTessFaces(dm);
	tface= dm->getTessFaceDataArray(dm, CD_MTFACE);
	mcol= dm->getTessFaceDataArray(dm, CD_MCOL);

	if (mcol==NULL && tface==NULL) return NULL;

	/* make sorted table with edges and face indices in it */
	for (a= totface, mf= mface; a>0; a--, mf++) {
		totedge += mf->v4 ? 4 : 3;
	}

	if (totedge==0)
		return NULL;

	ed= edsort= MEM_callocN(totedge*sizeof(struct edgesort), "edgesort");

	for (a=0, mf=mface; a<totface; a++, mf++) {
		to_edgesort(ed++, 0, 1, mf->v1, mf->v2, a);
		to_edgesort(ed++, 1, 2, mf->v2, mf->v3, a);
		if (mf->v4) {
			to_edgesort(ed++, 2, 3, mf->v3, mf->v4, a);
			to_edgesort(ed++, 3, 0, mf->v4, mf->v1, a);
		}
		else {
			to_edgesort(ed++, 2, 3, mf->v3, mf->v1, a);
		}
	}

	qsort(edsort, totedge, sizeof(struct edgesort), vergedgesort);

	*totedgesort= totedge;

	return edsort;
}

static void use_mesh_edge_lookup(ObjectRen *obr, DerivedMesh *dm, MEdge *medge, VlakRen *vlr, struct edgesort *edgetable, int totedge)
{
	struct edgesort ed, *edp;
	CustomDataLayer *layer;
	MTFace *mtface, *mtf;
	MCol *mcol, *mc;
	int index, mtfn, mcn;
	char *name;

	if (medge->v1 < medge->v2) {
		ed.v1= medge->v1;
		ed.v2= medge->v2;
	}
	else {
		ed.v1= medge->v2;
		ed.v2= medge->v1;
	}

	edp= bsearch(&ed, edgetable, totedge, sizeof(struct edgesort), vergedgesort);

	/* since edges have different index ordering, we have to duplicate mcol and tface */
	if (edp) {
		mtfn= mcn= 0;

		for (index=0; index<dm->faceData.totlayer; index++) {
			layer= &dm->faceData.layers[index];
			name= layer->name;

			if (layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
				mtface= &((MTFace*)layer->data)[edp->f];
				mtf= RE_vlakren_get_tface(obr, vlr, mtfn++, &name, 1);

				*mtf= *mtface;

				memcpy(mtf->uv[0], mtface->uv[edp->i1], sizeof(float)*2);
				memcpy(mtf->uv[1], mtface->uv[edp->i2], sizeof(float)*2);
				memcpy(mtf->uv[2], mtface->uv[1], sizeof(float)*2);
				memcpy(mtf->uv[3], mtface->uv[1], sizeof(float)*2);
			}
			else if (layer->type == CD_MCOL && mcn < MAX_MCOL) {
				mcol= &((MCol*)layer->data)[edp->f*4];
				mc= RE_vlakren_get_mcol(obr, vlr, mcn++, &name, 1);

				mc[0]= mcol[edp->i1];
				mc[1]= mc[2]= mc[3]= mcol[edp->i2];
			}
		}
	}
}

static void free_camera_inside_volumes(Render *re)
{
	BLI_freelistN(&re->render_volumes_inside);
}

static void init_camera_inside_volumes(Render *re)
{
	ObjectInstanceRen *obi;
	VolumeOb *vo;
	/* coordinates are all in camera space, so camera coordinate is zero. we also
	 * add an offset for the clip start, however note that with clip start it's
	 * actually impossible to do a single 'inside' test, since there will not be
	 * a single point where all camera rays start from, though for small clip start
	 * they will be close together. */
	float co[3] = {0.f, 0.f, -re->clipsta};

	for (vo= re->volumes.first; vo; vo= vo->next) {
		for (obi= re->instancetable.first; obi; obi= obi->next) {
			if (obi->obr == vo->obr) {
				if (point_inside_volume_objectinstance(re, obi, co)) {
					MatInside *mi;

					mi = MEM_mallocN(sizeof(MatInside), "camera inside material");
					mi->ma = vo->ma;
					mi->obi = obi;

					BLI_addtail(&(re->render_volumes_inside), mi);
				}
			}
		}
	}


#if 0 /* debug */
	{
		MatInside *m;
		for (m = re->render_volumes_inside.first; m; m = m->next) {
			printf("matinside: ma: %s\n", m->ma->id.name + 2);
		}
	}
#endif
}

static void add_volume(Render *re, ObjectRen *obr, Material *ma)
{
	struct VolumeOb *vo;

	vo = MEM_mallocN(sizeof(VolumeOb), "volume object");

	vo->ma = ma;
	vo->obr = obr;

	BLI_addtail(&re->volumes, vo);
}

#ifdef WITH_FREESTYLE
static EdgeHash *make_freestyle_edge_mark_hash(DerivedMesh *dm)
{
	EdgeHash *edge_hash= NULL;
	FreestyleEdge *fed;
	MEdge *medge;
	int totedge, a;

	medge = dm->getEdgeArray(dm);
	totedge = dm->getNumEdges(dm);
	fed = dm->getEdgeDataArray(dm, CD_FREESTYLE_EDGE);
	if (fed) {
		edge_hash = BLI_edgehash_new(__func__);
		for (a = 0; a < totedge; a++) {
			if (fed[a].flag & FREESTYLE_EDGE_MARK)
				BLI_edgehash_insert(edge_hash, medge[a].v1, medge[a].v2, medge+a);
		}
	}
	return edge_hash;
}

static bool has_freestyle_edge_mark(EdgeHash *edge_hash, int v1, int v2)
{
	MEdge *medge= BLI_edgehash_lookup(edge_hash, v1, v2);
	return (!medge) ? 0 : 1;
}
#endif

static void init_render_mesh(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Mesh *me;
	MVert *mvert = NULL;
	MFace *mface;
	VlakRen *vlr; //, *vlr1;
	VertRen *ver;
	Material *ma;
	DerivedMesh *dm;
	CustomDataMask mask;
	float xn, yn, zn,  imat[3][3], mat[4][4];  //nor[3],
	float *orco = NULL;
	short (*loop_nors)[4][3] = NULL;
	bool need_orco = false, need_stress = false, need_tangent = false, need_origindex = false;
	bool need_nmap_tangent_concrete = false;
	int a, a1, ok, vertofs;
	int end, totvert = 0;
	bool do_autosmooth = false, do_displace = false;
	bool use_original_normals = false;
	int recalc_normals = 0;	/* false by default */
	int negative_scale;
#ifdef WITH_FREESTYLE
	FreestyleFace *ffa;
#endif

	me= ob->data;

	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	invert_m4_m4(ob->imat, mat);
	copy_m3_m4(imat, ob->imat);
	negative_scale= is_negative_m4(mat);

	need_orco= 0;
	for (a=1; a<=ob->totcol; a++) {
		ma= give_render_material(re, ob, a);
		if (ma) {
			if (ma->texco & (TEXCO_ORCO|TEXCO_STRESS))
				need_orco= 1;
			if (ma->texco & TEXCO_STRESS)
				need_stress= 1;
			/* normalmaps, test if tangents needed, separated from shading */
			if (ma->mode_l & MA_TANGENT_V) {
				need_tangent= 1;
				if (me->mtpoly==NULL)
					need_orco= 1;
			}
			if (ma->mode_l & MA_NORMAP_TANG) {
				if (me->mtpoly==NULL) {
					need_orco= 1;
				}
				need_tangent= 1;
			}
			if (ma->mode2_l & MA_TANGENT_CONCRETE) {
				need_nmap_tangent_concrete = true;
			}
		}
	}

	if (re->flag & R_NEED_TANGENT) {
		/* exception for tangent space baking */
		if (me->mtpoly==NULL) {
			need_orco= 1;
		}
		need_tangent= 1;
	}

	/* check autosmooth and displacement, we then have to skip only-verts optimize
	 * Note: not sure what we want to give higher priority, currently do_displace
	 *       takes precedence over do_autosmooth.
	 */
	do_displace = test_for_displace(re, ob);
	do_autosmooth = ((me->flag & ME_AUTOSMOOTH) != 0) && !do_displace;
	if (do_autosmooth || do_displace)
		timeoffset = 0;

	/* origindex currently used when using autosmooth, or baking to vertex colors. */
	need_origindex = (do_autosmooth || ((re->flag & R_BAKING) && (re->r.bake_flag & R_BAKE_VCOL)));

	mask = CD_MASK_RENDER_INTERNAL;
	if (!timeoffset)
		if (need_orco)
			mask |= CD_MASK_ORCO;

#ifdef WITH_FREESTYLE
	mask |= CD_MASK_ORIGINDEX | CD_MASK_FREESTYLE_EDGE | CD_MASK_FREESTYLE_FACE;
#endif

	if (re->r.scemode & R_VIEWPORT_PREVIEW)
		dm= mesh_create_derived_view(re->scene, ob, mask);
	else
		dm= mesh_create_derived_render(re->scene, ob, mask);
	if (dm==NULL) return;	/* in case duplicated object fails? */

	mvert= dm->getVertArray(dm);
	totvert= dm->getNumVerts(dm);

	if (totvert == 0) {
		dm->release(dm);
		return;
	}

	if (mask & CD_MASK_ORCO) {
		orco = get_object_orco(re, ob);
		if (!orco) {
			orco= dm->getVertDataArray(dm, CD_ORCO);
			if (orco) {
				orco= MEM_dupallocN(orco);
				set_object_orco(re, ob, orco);
			}
		}
	}

	/* attempt to autsmooth on original mesh, only without subsurf */
	if (do_autosmooth && me->totvert==totvert && me->totface==dm->getNumTessFaces(dm))
		use_original_normals= true;
	
	ma= give_render_material(re, ob, 1);


	if (ma->material_type == MA_TYPE_HALO) {
		make_render_halos(re, obr, me, totvert, mvert, ma, orco);
	}
	else {
		const int *index_vert_orig = NULL;
		const int *index_mf_to_mpoly = NULL;
		const int *index_mp_to_orig = NULL;
		if (need_origindex) {
			index_vert_orig = dm->getVertDataArray(dm, CD_ORIGINDEX);
			/* double lookup for faces -> polys */
#ifdef WITH_FREESTYLE
			index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
			index_mp_to_orig = dm->getPolyDataArray(dm, CD_ORIGINDEX);
#endif
		}

		for (a=0; a<totvert; a++, mvert++) {
			ver= RE_findOrAddVert(obr, obr->totvert++);
			copy_v3_v3(ver->co, mvert->co);
			if (do_autosmooth == false) {	/* autosmooth on original unrotated data to prevent differences between frames */
				normal_short_to_float_v3(ver->n, mvert->no);
				mul_m4_v3(mat, ver->co);
				mul_transposed_m3_v3(imat, ver->n);
				normalize_v3(ver->n);
				negate_v3(ver->n);
			}

			if (orco) {
				ver->orco= orco;
				orco+=3;
			}

			if (need_origindex) {
				int *origindex;
				origindex = RE_vertren_get_origindex(obr, ver, 1);

				/* Use orig index array if it's available (e.g. in the presence
				 * of modifiers). */
				if (index_vert_orig)
					*origindex = index_vert_orig[a];
				else
					*origindex = a;
			}
		}
		
		if (!timeoffset) {
			short (*lnp)[4][3] = NULL;
#ifdef WITH_FREESTYLE
			EdgeHash *edge_hash;

			/* create a hash table of Freestyle edge marks */
			edge_hash = make_freestyle_edge_mark_hash(dm);
#endif

			/* store customdata names, because DerivedMesh is freed */
			RE_set_customdata_names(obr, &dm->faceData);

			/* add tangent layers if we need */
			if ((ma->nmap_tangent_names_count && need_nmap_tangent_concrete) || need_tangent) {
				dm->calcLoopTangents(
				        dm, need_tangent,
				        (const char (*)[MAX_NAME])ma->nmap_tangent_names, ma->nmap_tangent_names_count);
				obr->tangent_mask = dm->tangent_mask;
				DM_generate_tangent_tessface_data(dm, need_nmap_tangent_concrete || need_tangent);
			}
			
			/* still to do for keys: the correct local texture coordinate */

			/* faces in order of color blocks */
			vertofs= obr->totvert - totvert;
			for (a1=0; (a1<ob->totcol || (a1==0 && ob->totcol==0)); a1++) {

				ma= give_render_material(re, ob, a1+1);

				/* test for 100% transparent */
				ok = 1;
				if ((ma->alpha == 0.0f) &&
				    (ma->spectra == 0.0f) &&
				    /* No need to test filter here, it's only active with MA_RAYTRANSP and we check against it below. */
				    /* (ma->filter == 0.0f) && */
				    (ma->mode & MA_TRANSP) &&
				    (ma->mode & (MA_RAYTRANSP | MA_RAYMIRROR)) == 0)
				{
					ok = 0;
					/* texture on transparency? */
					for (a=0; a<MAX_MTEX; a++) {
						if (ma->mtex[a] && ma->mtex[a]->tex) {
							if (ma->mtex[a]->mapto & MAP_ALPHA) ok= 1;
						}
					}
				}
				
				/* if wire material, and we got edges, don't do the faces */
				if (ma->material_type == MA_TYPE_WIRE) {
					end= dm->getNumEdges(dm);
					if (end) ok= 0;
				}

				if (ok) {
					end= dm->getNumTessFaces(dm);
					mface= dm->getTessFaceArray(dm);
					if (!loop_nors && do_autosmooth &&
					    (dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL) != NULL))
					{
						lnp = loop_nors = MEM_mallocN(sizeof(*loop_nors) * end, __func__);
					}
#ifdef WITH_FREESTYLE
					index_mf_to_mpoly= dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
					index_mp_to_orig= dm->getPolyDataArray(dm, CD_ORIGINDEX);
					ffa= CustomData_get_layer(&me->pdata, CD_FREESTYLE_FACE);
#endif
					
					for (a=0; a<end; a++, mface++) {
						int v1, v2, v3, v4, flag;
						
						if ( mface->mat_nr==a1 ) {
							float len;
							bool reverse_verts = (negative_scale != 0 && do_autosmooth == false);
							int rev_tab[] = {reverse_verts==0 ? 0 : 2, 1, reverse_verts==0 ? 2 : 0, 3};
							v1= reverse_verts==0 ? mface->v1 : mface->v3;
							v2= mface->v2;
							v3= reverse_verts==0 ? mface->v3 : mface->v1;
							v4= mface->v4;
							flag = do_autosmooth ? ME_SMOOTH : mface->flag & ME_SMOOTH;

							vlr= RE_findOrAddVlak(obr, obr->totvlak++);
							vlr->v1= RE_findOrAddVert(obr, vertofs+v1);
							vlr->v2= RE_findOrAddVert(obr, vertofs+v2);
							vlr->v3= RE_findOrAddVert(obr, vertofs+v3);
							if (v4) vlr->v4 = RE_findOrAddVert(obr, vertofs+v4);
							else vlr->v4 = NULL;

#ifdef WITH_FREESTYLE
							/* Freestyle edge/face marks */
							if (edge_hash) {
								int edge_mark = 0;

								if (has_freestyle_edge_mark(edge_hash, v1, v2)) edge_mark |= R_EDGE_V1V2;
								if (has_freestyle_edge_mark(edge_hash, v2, v3)) edge_mark |= R_EDGE_V2V3;
								if (!v4) {
									if (has_freestyle_edge_mark(edge_hash, v3, v1)) edge_mark |= R_EDGE_V3V1;
								}
								else {
									if (has_freestyle_edge_mark(edge_hash, v3, v4)) edge_mark |= R_EDGE_V3V4;
									if (has_freestyle_edge_mark(edge_hash, v4, v1)) edge_mark |= R_EDGE_V4V1;
								}
								vlr->freestyle_edge_mark= edge_mark;
							}
							if (ffa) {
								int index = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a) : a;
								vlr->freestyle_face_mark= (ffa[index].flag & FREESTYLE_FACE_MARK) ? 1 : 0;
							}
							else {
								vlr->freestyle_face_mark= 0;
							}
#endif

							/* render normals are inverted in render */
							if (use_original_normals) {
								MFace *mf= me->mface+a;
								MVert *mv= me->mvert;
								
								if (vlr->v4)
									len= normal_quad_v3(vlr->n, mv[mf->v4].co, mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co);
								else 
									len= normal_tri_v3(vlr->n, mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co);
							}
							else {
								if (vlr->v4)
									len= normal_quad_v3(vlr->n, vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
								else 
									len= normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);
							}

							vlr->mat= ma;
							vlr->flag= flag;
							vlr->ec= 0; /* mesh edges rendered separately */

							if (len==0) obr->totvlak--;
							else {
								CustomDataLayer *layer;
								MTFace *mtface, *mtf;
								MCol *mcol, *mc;
								int index, mtfn= 0, mcn= 0, mln = 0, vindex;
								char *name;
								int nr_verts = v4!=0 ? 4 : 3;

								for (index=0; index<dm->faceData.totlayer; index++) {
									layer= &dm->faceData.layers[index];
									name= layer->name;
									
									if (layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
										int t;
										mtf= RE_vlakren_get_tface(obr, vlr, mtfn++, &name, 1);
										mtface= (MTFace*)layer->data;
										*mtf = mtface[a];  /* copy face info */
										for (vindex=0; vindex<nr_verts; vindex++)
											for (t=0; t<2; t++)
												mtf->uv[vindex][t]=mtface[a].uv[rev_tab[vindex]][t];
									}
									else if (layer->type == CD_MCOL && mcn < MAX_MCOL) {
										mc= RE_vlakren_get_mcol(obr, vlr, mcn++, &name, 1);
										mcol= (MCol*)layer->data;
										for (vindex=0; vindex<nr_verts; vindex++)
											mc[vindex]=mcol[a*4+rev_tab[vindex]];
									}
									else if (layer->type == CD_TANGENT) {
										if (need_nmap_tangent_concrete || need_tangent) {
											int uv_start = CustomData_get_layer_index(&dm->faceData, CD_MTFACE);
											int uv_index = CustomData_get_named_layer_index(&dm->faceData, CD_MTFACE, layer->name);

											/* if there are no UVs, orco tangents are in first slot */
											int n = (uv_start >= 0 && uv_index >= 0) ? uv_index - uv_start : 0;

											const float *tangent = (const float *) layer->data;
											float *ftang = RE_vlakren_get_nmap_tangent(obr, vlr, n, true);

											for (vindex=0; vindex<nr_verts; vindex++) {
												copy_v4_v4(ftang+vindex*4, tangent+a*16+rev_tab[vindex]*4);
												mul_mat3_m4_v3(mat, ftang+vindex*4);
												normalize_v3(ftang+vindex*4);
											}
										}
									}
									else if (layer->type == CD_TESSLOOPNORMAL && mln < 1) {
										if (loop_nors) {
											const short (*lnors)[4][3] = (const short (*)[4][3])layer->data;
											for (vindex = 0; vindex < 4; vindex++) {
												//print_v3("lnors[a][rev_tab[vindex]]", lnors[a][rev_tab[vindex]]);
												copy_v3_v3_short((short *)lnp[0][vindex], lnors[a][rev_tab[vindex]]);
												/* If we copy loop normals, we are doing autosmooth, so we are still
												 * in object space, no need to multiply with mat!
												 */
											}
											lnp++;
										}
										mln++;
									}
								}

								if (need_origindex) {
									/* Find original index of mpoly for this tessface. Options:
									 * - Modified mesh; two-step look up from tessface -> modified mpoly -> original mpoly
									 * - OR Tesselated mesh; look up from tessface -> mpoly
									 * - OR Failsafe; tessface == mpoly. Could probably assert(false) in this case? */
									int *origindex;
									origindex = RE_vlakren_get_origindex(obr, vlr, 1);
									if (index_mf_to_mpoly && index_mp_to_orig)
										*origindex = DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a);
									else if (index_mf_to_mpoly)
										*origindex = index_mf_to_mpoly[a];
									else
										*origindex = a;
								}
							}
						}
					}
				}
			}

#ifdef WITH_FREESTYLE
			/* release the hash table of Freestyle edge marks */
			if (edge_hash)
				BLI_edgehash_free(edge_hash, NULL);
#endif
			
			/* exception... we do edges for wire mode. potential conflict when faces exist... */
			end= dm->getNumEdges(dm);
			mvert= dm->getVertArray(dm);
			ma= give_render_material(re, ob, 1);
			if (end && (ma->material_type == MA_TYPE_WIRE)) {
				MEdge *medge;
				struct edgesort *edgetable;
				int totedge= 0;
				recalc_normals= 1;
				
				medge= dm->getEdgeArray(dm);
				
				/* we want edges to have UV and vcol too... */
				edgetable= make_mesh_edge_lookup(dm, &totedge);
				
				for (a1=0; a1<end; a1++, medge++) {
					if (medge->flag&ME_EDGERENDER) {
						MVert *v0 = &mvert[medge->v1];
						MVert *v1 = &mvert[medge->v2];

						vlr= RE_findOrAddVlak(obr, obr->totvlak++);
						vlr->v1= RE_findOrAddVert(obr, vertofs+medge->v1);
						vlr->v2= RE_findOrAddVert(obr, vertofs+medge->v2);
						vlr->v3= vlr->v2;
						vlr->v4= NULL;
						
						if (edgetable)
							use_mesh_edge_lookup(obr, dm, medge, vlr, edgetable, totedge);
						
						xn= -(v0->no[0]+v1->no[0]);
						yn= -(v0->no[1]+v1->no[1]);
						zn= -(v0->no[2]+v1->no[2]);
						/* transpose ! */
						vlr->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
						vlr->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
						vlr->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
						normalize_v3(vlr->n);
						
						vlr->mat= ma;
						vlr->flag= 0;
						vlr->ec= ME_V1V2;
					}
				}
				if (edgetable)
					MEM_freeN(edgetable);
			}
		}
	}
	
	if (!timeoffset) {
		if (need_stress)
			calc_edge_stress(re, obr, me);

		if (do_displace) {
			calc_vertexnormals(re, obr, 1, 0, 0);
			displace(re, obr);
			recalc_normals = 0;  /* Already computed by displace! */
		}
		else if (do_autosmooth) {
			recalc_normals = (loop_nors == NULL);  /* Should never happen, but better be safe than sorry. */
			autosmooth(re, obr, mat, loop_nors);
		}

		if (recalc_normals!=0 || need_tangent!=0)
			calc_vertexnormals(re, obr, recalc_normals, need_tangent, need_nmap_tangent_concrete);
	}

	MEM_SAFE_FREE(loop_nors);

	dm->release(dm);
}

/* ------------------------------------------------------------------------- */
/* Lamps and Shadowbuffers													 */
/* ------------------------------------------------------------------------- */

static void initshadowbuf(Render *re, LampRen *lar, float mat[4][4])
{
	struct ShadBuf *shb;
	float viewinv[4][4];
	
	/* if (la->spsi<16) return; */
	
	/* memory alloc */
	shb= (struct ShadBuf *)MEM_callocN(sizeof(struct ShadBuf), "initshadbuf");
	lar->shb= shb;
	
	if (shb==NULL) return;
	
	VECCOPY(shb->co, lar->co); /* int copy */
	
	/* percentage render: keep track of min and max */
	shb->size= (lar->bufsize*re->r.size)/100;
	
	if (shb->size<512) shb->size= 512;
	else if (shb->size > lar->bufsize) shb->size= lar->bufsize;
	
	shb->size &= ~15;	/* make sure its multiples of 16 */
	
	shb->samp= lar->samp;
	shb->soft= lar->soft;
	shb->shadhalostep= lar->shadhalostep;
	
	normalize_m4(mat);
	invert_m4_m4(shb->winmat, mat);	/* winmat is temp */
	
	/* matrix: combination of inverse view and lampmat */
	/* calculate again: the ortho-render has no correct viewinv */
	invert_m4_m4(viewinv, re->viewmat);
	mul_m4_m4m4(shb->viewmat, shb->winmat, viewinv);
	
	/* projection */
	shb->d= lar->clipsta;
	shb->clipend= lar->clipend;
	
	/* bias is percentage, made 2x larger because of correction for angle of incidence */
	/* when a ray is closer to parallel of a face, bias value is increased during render */
	shb->bias= (0.02f*lar->bias)*0x7FFFFFFF;
	
	/* halfway method (average of first and 2nd z) reduces bias issues */
	if (ELEM(lar->buftype, LA_SHADBUF_HALFWAY, LA_SHADBUF_DEEP))
		shb->bias= 0.1f*shb->bias;
	
	shb->compressthresh= lar->compressthresh;
}

void area_lamp_vectors(LampRen *lar)
{
	float xsize= 0.5f*lar->area_size, ysize= 0.5f*lar->area_sizey, multifac;

	/* make it smaller, so area light can be multisampled */
	multifac= 1.0f/sqrtf((float)lar->ray_totsamp);
	xsize *= multifac;
	ysize *= multifac;
	
	/* corner vectors */
	lar->area[0][0]= lar->co[0] - xsize*lar->mat[0][0] - ysize*lar->mat[1][0];
	lar->area[0][1]= lar->co[1] - xsize*lar->mat[0][1] - ysize*lar->mat[1][1];
	lar->area[0][2]= lar->co[2] - xsize*lar->mat[0][2] - ysize*lar->mat[1][2];

	/* corner vectors */
	lar->area[1][0]= lar->co[0] - xsize*lar->mat[0][0] + ysize*lar->mat[1][0];
	lar->area[1][1]= lar->co[1] - xsize*lar->mat[0][1] + ysize*lar->mat[1][1];
	lar->area[1][2]= lar->co[2] - xsize*lar->mat[0][2] + ysize*lar->mat[1][2];

	/* corner vectors */
	lar->area[2][0]= lar->co[0] + xsize*lar->mat[0][0] + ysize*lar->mat[1][0];
	lar->area[2][1]= lar->co[1] + xsize*lar->mat[0][1] + ysize*lar->mat[1][1];
	lar->area[2][2]= lar->co[2] + xsize*lar->mat[0][2] + ysize*lar->mat[1][2];

	/* corner vectors */
	lar->area[3][0]= lar->co[0] + xsize*lar->mat[0][0] - ysize*lar->mat[1][0];
	lar->area[3][1]= lar->co[1] + xsize*lar->mat[0][1] - ysize*lar->mat[1][1];
	lar->area[3][2]= lar->co[2] + xsize*lar->mat[0][2] - ysize*lar->mat[1][2];
	/* only for correction button size, matrix size works on energy */
	lar->areasize= lar->dist*lar->dist/(4.0f*xsize*ysize);
}

/* If lar takes more lamp data, the decoupling will be better. */
static GroupObject *add_render_lamp(Render *re, Object *ob)
{
	Lamp *la= ob->data;
	LampRen *lar;
	GroupObject *go;
	float mat[4][4], angle, xn, yn;
	float vec[3];
	int c;

	/* previewrender sets this to zero... prevent accidents */
	if (la==NULL) return NULL;
	
	/* prevent only shadow from rendering light */
	if (la->mode & LA_ONLYSHADOW)
		if ((re->r.mode & R_SHADOW)==0)
			return NULL;
	
	re->totlamp++;
	
	/* groups is used to unify support for lightgroups, this is the global lightgroup */
	go= MEM_callocN(sizeof(GroupObject), "groupobject");
	BLI_addtail(&re->lights, go);
	go->ob= ob;
	/* lamprens are in own list, for freeing */
	lar= (LampRen *)MEM_callocN(sizeof(LampRen), "lampren");
	BLI_addtail(&re->lampren, lar);
	go->lampren= lar;

	mul_m4_m4m4(mat, re->viewmat, ob->obmat);
	invert_m4_m4(ob->imat, mat);

	copy_m4_m4(lar->lampmat, ob->obmat);
	copy_m3_m4(lar->mat, mat);
	copy_m3_m4(lar->imat, ob->imat);

	lar->bufsize = la->bufsize;
	lar->samp = la->samp;
	lar->buffers= la->buffers;
	if (lar->buffers==0) lar->buffers= 1;
	lar->buftype= la->buftype;
	lar->filtertype= la->filtertype;
	lar->soft = la->soft;
	lar->shadhalostep = la->shadhalostep;
	lar->clipsta = la->clipsta;
	lar->clipend = la->clipend;
	
	lar->bias = la->bias;
	lar->compressthresh = la->compressthresh;

	lar->type= la->type;
	lar->mode= la->mode;

	lar->energy= la->energy;
	if (la->mode & LA_NEG) lar->energy= -lar->energy;

	lar->vec[0]= -mat[2][0];
	lar->vec[1]= -mat[2][1];
	lar->vec[2]= -mat[2][2];
	normalize_v3(lar->vec);
	lar->co[0]= mat[3][0];
	lar->co[1]= mat[3][1];
	lar->co[2]= mat[3][2];
	lar->dist= la->dist;
	lar->haint= la->haint;
	lar->distkw= lar->dist*lar->dist;
	lar->r= lar->energy*la->r;
	lar->g= lar->energy*la->g;
	lar->b= lar->energy*la->b;
	lar->shdwr= la->shdwr;
	lar->shdwg= la->shdwg;
	lar->shdwb= la->shdwb;
	lar->k= la->k;

	/* area */
	lar->ray_samp= la->ray_samp;
	lar->ray_sampy= la->ray_sampy;
	lar->ray_sampz= la->ray_sampz;
	
	lar->area_size= la->area_size;
	lar->area_sizey= la->area_sizey;
	lar->area_sizez= la->area_sizez;

	lar->area_shape= la->area_shape;
	
	/* Annoying, lamp UI does this, but the UI might not have been used? - add here too.
	 * make sure this matches buttons_shading.c's logic */
	if (ELEM(la->type, LA_AREA, LA_SPOT, LA_SUN, LA_LOCAL) && (la->mode & LA_SHAD_RAY))
		if (ELEM(la->type, LA_SPOT, LA_SUN, LA_LOCAL))
			if (la->ray_samp_method == LA_SAMP_CONSTANT) la->ray_samp_method = LA_SAMP_HALTON;
	
	lar->ray_samp_method= la->ray_samp_method;
	lar->ray_samp_type= la->ray_samp_type;
	
	lar->adapt_thresh= la->adapt_thresh;
	lar->sunsky = NULL;
	
	if ( ELEM(lar->type, LA_SPOT, LA_LOCAL)) {
		lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
		lar->area_shape = LA_AREA_SQUARE;
		lar->area_sizey= lar->area_size;
	}
	else if (lar->type==LA_AREA) {
		switch (lar->area_shape) {
		case LA_AREA_SQUARE:
			lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
			lar->ray_sampy= lar->ray_samp;
			lar->area_sizey= lar->area_size;
			break;
		case LA_AREA_RECT:
			lar->ray_totsamp= lar->ray_samp*lar->ray_sampy;
			break;
		case LA_AREA_CUBE:
			lar->ray_totsamp= lar->ray_samp*lar->ray_samp*lar->ray_samp;
			lar->ray_sampy= lar->ray_samp;
			lar->ray_sampz= lar->ray_samp;
			lar->area_sizey= lar->area_size;
			lar->area_sizez= lar->area_size;
			break;
		case LA_AREA_BOX:
			lar->ray_totsamp= lar->ray_samp*lar->ray_sampy*lar->ray_sampz;
			break;
		}

		area_lamp_vectors(lar);
		init_jitter_plane(lar);	 /* subsamples */
	}
	else if (lar->type==LA_SUN) {
		lar->ray_totsamp= lar->ray_samp*lar->ray_samp;
		lar->area_shape = LA_AREA_SQUARE;
		lar->area_sizey= lar->area_size;

		if ((la->sun_effect_type & LA_SUN_EFFECT_SKY) ||
		    (la->sun_effect_type & LA_SUN_EFFECT_AP))
		{
			lar->sunsky = (struct SunSky*)MEM_callocN(sizeof(struct SunSky), "sunskyren");
			lar->sunsky->effect_type = la->sun_effect_type;
		
			copy_v3_v3(vec, ob->obmat[2]);
			normalize_v3(vec);

			InitSunSky(lar->sunsky, la->atm_turbidity, vec, la->horizon_brightness, 
					la->spread, la->sun_brightness, la->sun_size, la->backscattered_light,
					   la->skyblendfac, la->skyblendtype, la->sky_exposure, la->sky_colorspace);
			
			InitAtmosphere(lar->sunsky, la->sun_intensity, 1.0, 1.0, la->atm_inscattering_factor, la->atm_extinction_factor,
					la->atm_distance_factor);
		}
	}
	else lar->ray_totsamp= 0;
	
	lar->spotsi= la->spotsize;
	if (lar->mode & LA_HALO) {
		if (lar->spotsi > DEG2RADF(170.0f)) lar->spotsi = DEG2RADF(170.0f);
	}
	lar->spotsi= cosf(lar->spotsi * 0.5f);
	lar->spotbl= (1.0f-lar->spotsi)*la->spotblend;

	memcpy(lar->mtex, la->mtex, MAX_MTEX*sizeof(void *));

	lar->lay = ob->lay & 0xFFFFFF;  /* higher 8 bits are localview layers */

	lar->falloff_type = la->falloff_type;
	lar->ld1= la->att1;
	lar->ld2= la->att2;
	lar->coeff_const= la->coeff_const;
	lar->coeff_lin= la->coeff_lin;
	lar->coeff_quad= la->coeff_quad;
	lar->curfalloff = curvemapping_copy(la->curfalloff);

	if (lar->curfalloff) {
		/* so threads don't conflict on init */
		curvemapping_initialize(lar->curfalloff);
	}

	if (lar->type==LA_SPOT) {

		normalize_v3(lar->imat[0]);
		normalize_v3(lar->imat[1]);
		normalize_v3(lar->imat[2]);

		xn = saacos(lar->spotsi);
		xn = sinf(xn) / cosf(xn);
		lar->spottexfac= 1.0f/(xn);

		if (lar->mode & LA_ONLYSHADOW) {
			if ((lar->mode & (LA_SHAD_BUF|LA_SHAD_RAY))==0) lar->mode -= LA_ONLYSHADOW;
		}

	}

	/* set flag for spothalo en initvars */
	if ((la->type == LA_SPOT) && (la->mode & LA_HALO) &&
	    (!(la->mode & LA_SHAD_BUF) || la->buftype != LA_SHADBUF_DEEP))
	{
		if (la->haint>0.0f) {
			re->flag |= R_LAMPHALO;

			/* camera position (0, 0, 0) rotate around lamp */
			lar->sh_invcampos[0]= -lar->co[0];
			lar->sh_invcampos[1]= -lar->co[1];
			lar->sh_invcampos[2]= -lar->co[2];
			mul_m3_v3(lar->imat, lar->sh_invcampos);

			/* z factor, for a normalized volume */
			angle= saacos(lar->spotsi);
			xn= lar->spotsi;
			yn = sinf(angle);
			lar->sh_zfac= yn/xn;
			/* pre-scale */
			lar->sh_invcampos[2]*= lar->sh_zfac;

			/* halfway shadow buffer doesn't work for volumetric effects */
			if (ELEM(lar->buftype, LA_SHADBUF_HALFWAY, LA_SHADBUF_DEEP))
				lar->buftype = LA_SHADBUF_REGULAR;

		}
	}
	else if (la->type==LA_HEMI) {
		lar->mode &= ~(LA_SHAD_RAY|LA_SHAD_BUF);
	}

	for (c=0; c<MAX_MTEX; c++) {
		if (la->mtex[c] && la->mtex[c]->tex) {
			if (la->mtex[c]->mapto & LAMAP_COL) 
				lar->mode |= LA_TEXTURE;
			if (la->mtex[c]->mapto & LAMAP_SHAD)
				lar->mode |= LA_SHAD_TEX;

			if (G.is_rendering) {
				if (re->osa) {
					if (la->mtex[c]->tex->type==TEX_IMAGE) lar->mode |= LA_OSATEX;
				}
			}
		}
	}

	/* old code checked for internal render (aka not yafray) */
	{
		/* to make sure we can check ray shadow easily in the render code */
		if (lar->mode & LA_SHAD_RAY) {
			if ( (re->r.mode & R_RAYTRACE)==0)
				lar->mode &= ~LA_SHAD_RAY;
		}
	

		if (re->r.mode & R_SHADOW) {
			
			if (la->type==LA_AREA && (lar->mode & LA_SHAD_RAY) && (lar->ray_samp_method == LA_SAMP_CONSTANT)) {
				init_jitter_plane(lar);
			}
			else if (la->type==LA_SPOT && (lar->mode & LA_SHAD_BUF) ) {
				/* Per lamp, one shadow buffer is made. */
				lar->bufflag= la->bufflag;
				copy_m4_m4(mat, ob->obmat);
				initshadowbuf(re, lar, mat);  /* mat is altered */
			}
			
			
			/* this is the way used all over to check for shadow */
			if (lar->shb || (lar->mode & LA_SHAD_RAY)) {
				LampShadowSample *ls;
				LampShadowSubSample *lss;
				int a, b;

				memset(re->shadowsamplenr, 0, sizeof(re->shadowsamplenr));
				
				lar->shadsamp= MEM_mallocN(re->r.threads*sizeof(LampShadowSample), "lamp shadow sample");
				ls= lar->shadsamp;

				/* shadfacs actually mean light, let's put them to 1 to prevent unitialized accidents */
				for (a=0; a<re->r.threads; a++, ls++) {
					lss= ls->s;
					for (b=0; b<re->r.osa; b++, lss++) {
						lss->samplenr= -1;	/* used to detect whether we store or read */
						lss->shadfac[0]= 1.0f;
						lss->shadfac[1]= 1.0f;
						lss->shadfac[2]= 1.0f;
						lss->shadfac[3]= 1.0f;
					}
				}
			}
		}
	}
	
	return go;
}

static bool is_object_restricted(Render *re, Object *ob)
{
	if (re->r.scemode & R_VIEWPORT_PREVIEW)
		return (ob->restrictflag & OB_RESTRICT_VIEW) != 0;
	else
		return (ob->restrictflag & OB_RESTRICT_RENDER) != 0;
}

static bool is_object_hidden(Render *re, Object *ob)
{
	if (is_object_restricted(re, ob))
		return true;
	
	if (re->r.scemode & R_VIEWPORT_PREVIEW) {
		/* Mesh deform cages and so on mess up the preview. To avoid the problem,
		 * viewport doesn't show mesh object if its draw type is bounding box or wireframe.
		 * Unless it's an active smoke domain!
		 */
		ModifierData *md = NULL;

		if ((md = modifiers_findByType(ob, eModifierType_Smoke)) &&
		    (modifier_isEnabled(re->scene, md, eModifierMode_Realtime)))
		{
			return false;
		}
		return ELEM(ob->dt, OB_BOUNDBOX, OB_WIRE);
	}
	else {
		return false;
	}
}

/* layflag: allows material group to ignore layerflag */
static void add_lightgroup(Render *re, Group *group, int exclusive)
{
	GroupObject *go, *gol;
	
	group->id.tag &= ~LIB_TAG_DOIT;

	/* it's a bit too many loops in loops... but will survive */
	/* note that 'exclusive' will remove it from the global list */
	for (go= group->gobject.first; go; go= go->next) {
		go->lampren= NULL;

		if (is_object_hidden(re, go->ob))
			continue;
		
		if (go->ob->lay & re->lay) {
			if (go->ob && go->ob->type==OB_LAMP) {
				for (gol= re->lights.first; gol; gol= gol->next) {
					if (gol->ob==go->ob) {
						go->lampren= gol->lampren;
						break;
					}
				}
				if (go->lampren==NULL)
					gol= add_render_lamp(re, go->ob);
				if (gol && exclusive) {
					BLI_remlink(&re->lights, gol);
					MEM_freeN(gol);
				}
			}
		}
	}
}

static void set_material_lightgroups(Render *re)
{
	Group *group;
	Material *ma;
	
	/* not for preview render */
	if (re->scene->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))
		return;
	
	for (group= re->main->group.first; group; group=group->id.next)
		group->id.tag |= LIB_TAG_DOIT;
	
	/* it's a bit too many loops in loops... but will survive */
	/* hola! materials not in use...? */
	for (ma= re->main->mat.first; ma; ma=ma->id.next) {
		if (ma->group && (ma->group->id.tag & LIB_TAG_DOIT))
			add_lightgroup(re, ma->group, ma->mode & MA_GROUP_NOLAY);
	}
}

static void set_renderlayer_lightgroups(Render *re, Scene *sce)
{
	SceneRenderLayer *srl;
	
	for (srl= sce->r.layers.first; srl; srl= srl->next) {
		if (srl->light_override)
			add_lightgroup(re, srl->light_override, 0);
	}
}

/* ------------------------------------------------------------------------- */
/* World																	 */
/* ------------------------------------------------------------------------- */

void init_render_world(Render *re)
{
	void *wrld_prev[2] = {
	    re->wrld.aotables,
	    re->wrld.aosphere,
	};

	int a;
	
	if (re->scene && re->scene->world) {
		re->wrld = *(re->scene->world);

		copy_v3_v3(re->grvec, re->viewmat[2]);
		normalize_v3(re->grvec);
		copy_m3_m4(re->imat, re->viewinv);
		
		for (a=0; a<MAX_MTEX; a++)
			if (re->wrld.mtex[a] && re->wrld.mtex[a]->tex) re->wrld.skytype |= WO_SKYTEX;
		
		/* AO samples should be OSA minimum */
		if (re->osa)
			while (re->wrld.aosamp*re->wrld.aosamp < re->osa)
				re->wrld.aosamp++;
		if (!(re->r.mode & R_RAYTRACE) && (re->wrld.ao_gather_method == WO_AOGATHER_RAYTRACE))
			re->wrld.mode &= ~(WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT);
	}
	else {
		memset(&re->wrld, 0, sizeof(World));
		re->wrld.exp= 0.0f;
		re->wrld.range= 1.0f;
		
		/* for mist pass */
		re->wrld.miststa= re->clipsta;
		re->wrld.mistdist= re->clipend-re->clipsta;
		re->wrld.misi= 1.0f;
	}
	
	re->wrld.linfac= 1.0f + powf((2.0f*re->wrld.exp + 0.5f), -10);
	re->wrld.logfac= logf((re->wrld.linfac-1.0f)/re->wrld.linfac) / re->wrld.range;

	/* restore runtime vars, needed for viewport rendering [#36005] */
	re->wrld.aotables = wrld_prev[0];
	re->wrld.aosphere = wrld_prev[1];
}



/* ------------------------------------------------------------------------- */
/* Object Finalization														 */
/* ------------------------------------------------------------------------- */

/* prevent phong interpolation for giving ray shadow errors (terminator problem) */
static void set_phong_threshold(ObjectRen *obr)
{
//	VertRen *ver;
	VlakRen *vlr;
	float thresh= 0.0, dot;
	int tot=0, i;
	
	/* Added check for 'pointy' situations, only dotproducts of 0.9 and larger 
	 * are taken into account. This threshold is meant to work on smooth geometry, not
	 * for extreme cases (ton) */
	
	for (i=0; i<obr->totvlak; i++) {
		vlr= RE_findOrAddVlak(obr, i);
		if ((vlr->flag & R_SMOOTH) && (vlr->flag & R_STRAND)==0) {
			dot= dot_v3v3(vlr->n, vlr->v1->n);
			dot= ABS(dot);
			if (dot>0.9f) {
				thresh+= dot; tot++;
			}
			dot= dot_v3v3(vlr->n, vlr->v2->n);
			dot= ABS(dot);
			if (dot>0.9f) {
				thresh+= dot; tot++;
			}

			dot= dot_v3v3(vlr->n, vlr->v3->n);
			dot= ABS(dot);
			if (dot>0.9f) {
				thresh+= dot; tot++;
			}

			if (vlr->v4) {
				dot= dot_v3v3(vlr->n, vlr->v4->n);
				dot= ABS(dot);
				if (dot>0.9f) {
					thresh+= dot; tot++;
				}
			}
		}
	}
	
	if (tot) {
		thresh/= (float)tot;
		obr->ob->smoothresh= cosf(0.5f*(float)M_PI-saacos(thresh));
	}
}

/* per face check if all samples should be taken.
 * if raytrace or multisample, do always for raytraced material, or when material full_osa set */
static void set_fullsample_trace_flag(Render *re, ObjectRen *obr)
{
	VlakRen *vlr;
	int a, trace, mode, osa;

	osa= re->osa;
	trace= re->r.mode & R_RAYTRACE;
	
	for (a=obr->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(obr, a);
		mode= vlr->mat->mode;

		if (trace && (mode & MA_TRACEBLE))
			vlr->flag |= R_TRACEBLE;
		
		if (osa) {
			if (mode & MA_FULL_OSA) {
				vlr->flag |= R_FULL_OSA;
			}
			else if (trace) {
				if (mode & MA_SHLESS) {
					/* pass */
				}
				else if (vlr->mat->material_type == MA_TYPE_VOLUME) {
					/* pass */
				}
				else if ((mode & MA_RAYMIRROR) || ((mode & MA_TRANSP) && (mode & MA_RAYTRANSP))) {
					/* for blurry reflect/refract, better to take more samples 
					 * inside the raytrace than as OSA samples */
					if ((vlr->mat->gloss_mir == 1.0f) && (vlr->mat->gloss_tra == 1.0f))
						vlr->flag |= R_FULL_OSA;
				}
			}
		}
	}
}

/* split quads for predictable baking
 * dir 1 == (0, 1, 2) (0, 2, 3),  2 == (1, 3, 0) (1, 2, 3)
 */
static void split_quads(ObjectRen *obr, int dir) 
{
	VlakRen *vlr, *vlr1;
	int a;

	for (a=obr->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(obr, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if ((vlr->flag & R_STRAND)==0 && (vlr->mat->material_type != MA_TYPE_WIRE)) {
			
			if (vlr->v4) {

				vlr1= RE_vlakren_copy(obr, vlr);
				vlr1->flag |= R_FACE_SPLIT;
				
				if ( dir==2 ) vlr->flag |= R_DIVIDE_24;
				else vlr->flag &= ~R_DIVIDE_24;

				/* new vertex pointers */
				if (vlr->flag & R_DIVIDE_24) {
					vlr1->v1= vlr->v2;
					vlr1->v2= vlr->v3;
					vlr1->v3= vlr->v4;

					vlr->v3 = vlr->v4;
					
					vlr1->flag |= R_DIVIDE_24;
				}
				else {
					vlr1->v1= vlr->v1;
					vlr1->v2= vlr->v3;
					vlr1->v3= vlr->v4;
					
					vlr1->flag &= ~R_DIVIDE_24;
				}
				vlr->v4 = vlr1->v4 = NULL;
				
#ifdef WITH_FREESTYLE
				/* Freestyle edge marks */
				if (vlr->flag & R_DIVIDE_24) {
					vlr1->freestyle_edge_mark=
						((vlr->freestyle_edge_mark & R_EDGE_V2V3) ? R_EDGE_V1V2 : 0) |
						((vlr->freestyle_edge_mark & R_EDGE_V3V4) ? R_EDGE_V2V3 : 0);
					vlr->freestyle_edge_mark=
						((vlr->freestyle_edge_mark & R_EDGE_V1V2) ? R_EDGE_V1V2 : 0) |
						((vlr->freestyle_edge_mark & R_EDGE_V4V1) ? R_EDGE_V3V1 : 0);
				}
				else {
					vlr1->freestyle_edge_mark=
						((vlr->freestyle_edge_mark & R_EDGE_V3V4) ? R_EDGE_V2V3 : 0) |
						((vlr->freestyle_edge_mark & R_EDGE_V4V1) ? R_EDGE_V3V1 : 0);
					vlr->freestyle_edge_mark=
						((vlr->freestyle_edge_mark & R_EDGE_V1V2) ? R_EDGE_V1V2 : 0) |
						((vlr->freestyle_edge_mark & R_EDGE_V2V3) ? R_EDGE_V2V3 : 0);
				}
#endif

				/* new normals */
				normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);
				normal_tri_v3(vlr1->n, vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
			}
			/* clear the flag when not divided */
			else vlr->flag &= ~R_DIVIDE_24;
		}
	}
}

static void check_non_flat_quads(ObjectRen *obr)
{
	VlakRen *vlr, *vlr1;
	VertRen *v1, *v2, *v3, *v4;
	float nor[3], xn, flen;
	int a;

	for (a=obr->totvlak-1; a>=0; a--) {
		vlr= RE_findOrAddVlak(obr, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if (vlr->v4 && (vlr->flag & R_STRAND)==0 && (vlr->mat->material_type != MA_TYPE_WIRE)) {
			
			/* check if quad is actually triangle */
			v1= vlr->v1;
			v2= vlr->v2;
			v3= vlr->v3;
			v4= vlr->v4;
			sub_v3_v3v3(nor, v1->co, v2->co);
			if ( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
				vlr->v1= v2;
				vlr->v2= v3;
				vlr->v3= v4;
				vlr->v4= NULL;
				vlr->flag |= (R_DIVIDE_24 | R_FACE_SPLIT);
			}
			else {
				sub_v3_v3v3(nor, v2->co, v3->co);
				if ( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
					vlr->v2= v3;
					vlr->v3= v4;
					vlr->v4= NULL;
					vlr->flag |= R_FACE_SPLIT;
				}
				else {
					sub_v3_v3v3(nor, v3->co, v4->co);
					if ( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
						vlr->v4= NULL;
					}
					else {
						sub_v3_v3v3(nor, v4->co, v1->co);
						if ( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
							vlr->v4= NULL;
						}
					}
				}
			}
			
			if (vlr->v4) {
				
				/* Face is divided along edge with the least gradient 		*/
				/* Flagged with R_DIVIDE_24 if divide is from vert 2 to 4 	*/
				/* 		4---3		4---3 */
				/*		|\ 1|	or  |1 /| */
				/*		|0\ |		|/ 0| */
				/*		1---2		1---2 	0 = orig face, 1 = new face */
				
				/* render normals are inverted in render! we calculate normal of single tria here */
				flen= normal_tri_v3(nor, vlr->v4->co, vlr->v3->co, vlr->v1->co);
				if (flen==0.0f) normal_tri_v3(nor, vlr->v4->co, vlr->v2->co, vlr->v1->co);
				
				xn = dot_v3v3(nor, vlr->n);

				if (ABS(xn) < 0.999995f ) {  /* checked on noisy fractal grid */

					float d1, d2;

					vlr1= RE_vlakren_copy(obr, vlr);
					vlr1->flag |= R_FACE_SPLIT;
					
					/* split direction based on vnorms */
					normal_tri_v3(nor, vlr->v1->co, vlr->v2->co, vlr->v3->co);
					d1 = dot_v3v3(nor, vlr->v1->n);

					normal_tri_v3(nor, vlr->v2->co, vlr->v3->co, vlr->v4->co);
					d2 = dot_v3v3(nor, vlr->v2->n);

					if (fabsf(d1) < fabsf(d2) ) vlr->flag |=  R_DIVIDE_24;
					else                        vlr->flag &= ~R_DIVIDE_24;

					/* new vertex pointers */
					if (vlr->flag & R_DIVIDE_24) {
						vlr1->v1= vlr->v2;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;

						vlr->v3 = vlr->v4;
						
						vlr1->flag |= R_DIVIDE_24;
					}
					else {
						vlr1->v1= vlr->v1;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;
						
						vlr1->flag &= ~R_DIVIDE_24;
					}
					vlr->v4 = vlr1->v4 = NULL;
					
					/* new normals */
					normal_tri_v3(vlr->n, vlr->v3->co, vlr->v2->co, vlr->v1->co);
					normal_tri_v3(vlr1->n, vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);

#ifdef WITH_FREESTYLE
					/* Freestyle edge marks */
					if (vlr->flag & R_DIVIDE_24) {
						vlr1->freestyle_edge_mark=
							((vlr->freestyle_edge_mark & R_EDGE_V2V3) ? R_EDGE_V1V2 : 0) |
							((vlr->freestyle_edge_mark & R_EDGE_V3V4) ? R_EDGE_V2V3 : 0);
						vlr->freestyle_edge_mark=
							((vlr->freestyle_edge_mark & R_EDGE_V1V2) ? R_EDGE_V1V2 : 0) |
							((vlr->freestyle_edge_mark & R_EDGE_V4V1) ? R_EDGE_V3V1 : 0);
					}
					else {
						vlr1->freestyle_edge_mark=
							((vlr->freestyle_edge_mark & R_EDGE_V3V4) ? R_EDGE_V2V3 : 0) |
							((vlr->freestyle_edge_mark & R_EDGE_V4V1) ? R_EDGE_V3V1 : 0);
						vlr->freestyle_edge_mark=
							((vlr->freestyle_edge_mark & R_EDGE_V1V2) ? R_EDGE_V1V2 : 0) |
							((vlr->freestyle_edge_mark & R_EDGE_V2V3) ? R_EDGE_V2V3 : 0);
					}
#endif
				}
				/* clear the flag when not divided */
				else vlr->flag &= ~R_DIVIDE_24;
			}
		}
	}
}

static void finalize_render_object(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	VertRen *ver= NULL;
	StrandRen *strand= NULL;
	StrandBound *sbound= NULL;
	float min[3], max[3], smin[3], smax[3];
	int a, b;

	if (obr->totvert || obr->totvlak || obr->tothalo || obr->totstrand) {
		/* the exception below is because displace code now is in init_render_mesh call, 
		 * I will look at means to have autosmooth enabled for all object types
		 * and have it as general postprocess, like displace */
		if (ob->type!=OB_MESH && test_for_displace(re, ob))
			displace(re, obr);
	
		if (!timeoffset) {
			/* phong normal interpolation can cause error in tracing
			 * (terminator problem) */
			ob->smoothresh= 0.0;
			if ((re->r.mode & R_RAYTRACE) && (re->r.mode & R_SHADOW))
				set_phong_threshold(obr);
			
			if (re->flag & R_BAKING && re->r.bake_quad_split != 0) {
				/* Baking lets us define a quad split order */
				split_quads(obr, re->r.bake_quad_split);
			}
			else if (BKE_object_is_animated(re->scene, ob))
				split_quads(obr, 1);
			else {
				if ((re->r.mode & R_SIMPLIFY && re->r.simplify_flag & R_SIMPLE_NO_TRIANGULATE) == 0)
					check_non_flat_quads(obr);
			}
			
			set_fullsample_trace_flag(re, obr);

			/* compute bounding boxes for clipping */
			INIT_MINMAX(min, max);
			for (a=0; a<obr->totvert; a++) {
				if ((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
				else ver++;

				minmax_v3v3_v3(min, max, ver->co);
			}

			if (obr->strandbuf) {
				float width;
				
				/* compute average bounding box of strandpoint itself (width) */
				if (obr->strandbuf->flag & R_STRAND_B_UNITS)
					obr->strandbuf->maxwidth = max_ff(obr->strandbuf->ma->strand_sta, obr->strandbuf->ma->strand_end);
				else
					obr->strandbuf->maxwidth= 0.0f;
				
				width= obr->strandbuf->maxwidth;
				sbound= obr->strandbuf->bound;
				for (b=0; b<obr->strandbuf->totbound; b++, sbound++) {
					
					INIT_MINMAX(smin, smax);

					for (a=sbound->start; a<sbound->end; a++) {
						strand= RE_findOrAddStrand(obr, a);
						strand_minmax(strand, smin, smax, width);
					}

					copy_v3_v3(sbound->boundbox[0], smin);
					copy_v3_v3(sbound->boundbox[1], smax);

					minmax_v3v3_v3(min, max, smin);
					minmax_v3v3_v3(min, max, smax);
				}
			}

			copy_v3_v3(obr->boundbox[0], min);
			copy_v3_v3(obr->boundbox[1], max);
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Database																	 */
/* ------------------------------------------------------------------------- */

static int render_object_type(short type)
{
	return OB_TYPE_SUPPORT_MATERIAL(type);
}

static void find_dupli_instances(Render *re, ObjectRen *obr, DupliObject *dob)
{
	ObjectInstanceRen *obi;
	float imat[4][4], obmat[4][4], obimat[4][4], nmat[3][3];
	int first = 1;

	mul_m4_m4m4(obmat, re->viewmat, obr->obmat);
	invert_m4_m4(imat, obmat);

	/* for objects instanced by dupliverts/faces/particles, we go over the
	 * list of instances to find ones that instance obr, and setup their
	 * matrices and obr pointer */
	for (obi=re->instancetable.last; obi; obi=obi->prev) {
		if (!obi->obr && obi->ob == obr->ob && obi->psysindex == obr->psysindex) {
			obi->obr= obr;

			/* compute difference between object matrix and
			 * object matrix with dupli transform, in viewspace */
			copy_m4_m4(obimat, obi->mat);
			mul_m4_m4m4(obi->mat, obimat, imat);

			copy_m3_m4(nmat, obi->mat);
			invert_m3_m3(obi->nmat, nmat);
			transpose_m3(obi->nmat);

			if (dob) {
				copy_v3_v3(obi->dupliorco, dob->orco);
				obi->dupliuv[0]= dob->uv[0];
				obi->dupliuv[1]= dob->uv[1];
			}

			if (!first) {
				re->totvert += obr->totvert;
				re->totvlak += obr->totvlak;
				re->tothalo += obr->tothalo;
				re->totstrand += obr->totstrand;
			}
			else
				first= 0;
		}
	}
}

static void assign_dupligroup_dupli(Render *re, ObjectInstanceRen *obi, ObjectRen *obr, DupliObject *dob)
{
	float imat[4][4], obmat[4][4], obimat[4][4], nmat[3][3];

	mul_m4_m4m4(obmat, re->viewmat, obr->obmat);
	invert_m4_m4(imat, obmat);

	obi->obr= obr;

	/* compute difference between object matrix and
	 * object matrix with dupli transform, in viewspace */
	copy_m4_m4(obimat, obi->mat);
	mul_m4_m4m4(obi->mat, obimat, imat);

	copy_m3_m4(nmat, obi->mat);
	invert_m3_m3(obi->nmat, nmat);
	transpose_m3(obi->nmat);

	if (dob) {
		copy_v3_v3(obi->dupliorco, dob->orco);
		obi->dupliuv[0]= dob->uv[0];
		obi->dupliuv[1]= dob->uv[1];
	}

	re->totvert += obr->totvert;
	re->totvlak += obr->totvlak;
	re->tothalo += obr->tothalo;
	re->totstrand += obr->totstrand;
}

static ObjectRen *find_dupligroup_dupli(Render *re, Object *ob, int psysindex)
{
	ObjectRen *obr;

	/* if the object is itself instanced, we don't want to create an instance
	 * for it */
	if (ob->transflag & OB_RENDER_DUPLI)
		return NULL;

	/* try to find an object that was already created so we can reuse it
	 * and save memory */
	for (obr=re->objecttable.first; obr; obr=obr->next)
		if (obr->ob == ob && obr->psysindex == psysindex && (obr->flag & R_INSTANCEABLE))
			return obr;
	
	return NULL;
}

static void set_dupli_tex_mat(Render *re, ObjectInstanceRen *obi, DupliObject *dob, float omat[4][4])
{
	/* For duplis we need to have a matrix that transform the coordinate back
	 * to it's original position, without the dupli transforms. We also check
	 * the matrix is actually needed, to save memory on lots of dupliverts for
	 * example */
	static Object *lastob= NULL;
	static int needtexmat= 0;

	/* init */
	if (!re) {
		lastob= NULL;
		needtexmat= 0;
		return;
	}

	/* check if we actually need it */
	if (lastob != dob->ob) {
		Material ***material;
		short a, *totmaterial;

		lastob= dob->ob;
		needtexmat= 0;

		totmaterial= give_totcolp(dob->ob);
		material= give_matarar(dob->ob);

		if (totmaterial && material)
			for (a= 0; a<*totmaterial; a++)
				if ((*material)[a] && (*material)[a]->texco & TEXCO_OBJECT)
					needtexmat= 1;
	}

	if (needtexmat) {
		float imat[4][4];

		obi->duplitexmat= BLI_memarena_alloc(re->memArena, sizeof(float)*4*4);
		invert_m4_m4(imat, dob->mat);
		mul_m4_series(obi->duplitexmat, re->viewmat, omat, imat, re->viewinv);
	}

	copy_v3_v3(obi->dupliorco, dob->orco);
	copy_v2_v2(obi->dupliuv, dob->uv);
}

static void init_render_object_data(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	ParticleSystem *psys;
	int i;

	if (obr->psysindex) {
		if ((!obr->prev || obr->prev->ob != ob || (obr->prev->flag & R_INSTANCEABLE)==0) && ob->type==OB_MESH) {
			/* the emitter mesh wasn't rendered so the modifier stack wasn't
			 * evaluated with render settings */
			DerivedMesh *dm;
			const CustomDataMask mask = CD_MASK_RENDER_INTERNAL;

			if (re->r.scemode & R_VIEWPORT_PREVIEW)
				dm = mesh_create_derived_view(re->scene, ob, mask);
			else
				dm = mesh_create_derived_render(re->scene, ob, mask);
			dm->release(dm);
		}

		for (psys=ob->particlesystem.first, i=0; i<obr->psysindex-1; i++)
			psys= psys->next;

		render_new_particle_system(re, obr, psys, timeoffset);
	}
	else {
		if (ELEM(ob->type, OB_FONT, OB_CURVE))
			init_render_curve(re, obr, timeoffset);
		else if (ob->type==OB_SURF)
			init_render_surf(re, obr, timeoffset);
		else if (ob->type==OB_MESH)
			init_render_mesh(re, obr, timeoffset);
		else if (ob->type==OB_MBALL)
			init_render_mball(re, obr);
	}

	finalize_render_object(re, obr, timeoffset);

	re->totvert += obr->totvert;
	re->totvlak += obr->totvlak;
	re->tothalo += obr->tothalo;
	re->totstrand += obr->totstrand;
}

static void add_render_object(Render *re, Object *ob, Object *par, DupliObject *dob, float omat[4][4], int timeoffset)
{
	ObjectRen *obr;
	ObjectInstanceRen *obi;
	ParticleSystem *psys;
	int show_emitter, allow_render= 1, index, psysindex, i;

	index= (dob)? dob->persistent_id[0]: 0;

	/* It seems that we may generate psys->renderdata recursively in some nasty intricated cases of
	 * several levels of bupliobject (see T51524).
	 * For now, basic rule is, do not restore psys if it was already in 'render state'.
	 * Another, more robust solution could be to add some reference counting to that renderdata... */
	bool psys_has_renderdata = false;

	/* the emitter has to be processed first (render levels of modifiers) */
	/* so here we only check if the emitter should be rendered */
	if (ob->particlesystem.first) {
		show_emitter= 0;
		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			show_emitter += psys->part->draw & PART_DRAW_EMITTER;
			if (!(re->r.scemode & R_VIEWPORT_PREVIEW)) {
				psys_has_renderdata |= (psys->renderdata != NULL);
				psys_render_set(ob, psys, re->viewmat, re->winmat, re->winx, re->winy, timeoffset);
			}
		}

		/* if no psys has "show emitter" selected don't render emitter */
		if (show_emitter == 0)
			allow_render= 0;
	}

	/* one render object for the data itself */
	if (allow_render) {
		obr= RE_addRenderObject(re, ob, par, index, 0, ob->lay);
		if ((dob && !dob->animated) || (ob->transflag & OB_RENDER_DUPLI)) {
			obr->flag |= R_INSTANCEABLE;
			copy_m4_m4(obr->obmat, ob->obmat);
		}
		init_render_object_data(re, obr, timeoffset);

		/* only add instance for objects that have not been used for dupli */
		if (!(ob->transflag & OB_RENDER_DUPLI)) {
			obi = RE_addRenderInstance(re, obr, ob, par, index, 0, NULL, ob->lay, dob);
			if (dob) set_dupli_tex_mat(re, obi, dob, omat);
		}
		else
			find_dupli_instances(re, obr, dob);
			
		for (i=1; i<=ob->totcol; i++) {
			Material* ma = give_render_material(re, ob, i);
			if (ma && ma->material_type == MA_TYPE_VOLUME)
				add_volume(re, obr, ma);
		}
	}

	/* and one render object per particle system */
	if (ob->particlesystem.first) {
		psysindex= 1;
		for (psys=ob->particlesystem.first; psys; psys=psys->next, psysindex++) {
			if (!psys_check_enabled(ob, psys, G.is_rendering))
				continue;
			
			obr= RE_addRenderObject(re, ob, par, index, psysindex, ob->lay);
			if ((dob && !dob->animated) || (ob->transflag & OB_RENDER_DUPLI)) {
				obr->flag |= R_INSTANCEABLE;
				copy_m4_m4(obr->obmat, ob->obmat);
			}
			if (dob)
				psys->flag |= PSYS_USE_IMAT;
			init_render_object_data(re, obr, timeoffset);
			if (!(re->r.scemode & R_VIEWPORT_PREVIEW) && !psys_has_renderdata) {
				psys_render_restore(ob, psys);
			}
			psys->flag &= ~PSYS_USE_IMAT;

			/* only add instance for objects that have not been used for dupli */
			if (!(ob->transflag & OB_RENDER_DUPLI)) {
				obi = RE_addRenderInstance(re, obr, ob, par, index, psysindex, NULL, ob->lay, dob);
				if (dob) set_dupli_tex_mat(re, obi, dob, omat);
			}
			else
				find_dupli_instances(re, obr, dob);
		}
	}
}

/* par = pointer to duplicator parent, needed for object lookup table */
/* index = when duplicater copies same object (particle), the counter */
static void init_render_object(Render *re, Object *ob, Object *par, DupliObject *dob, float omat[4][4], int timeoffset)
{
	static double lasttime= 0.0;
	double time;
	float mat[4][4];

	if (ob->type==OB_LAMP)
		add_render_lamp(re, ob);
	else if (render_object_type(ob->type))
		add_render_object(re, ob, par, dob, omat, timeoffset);
	else {
		mul_m4_m4m4(mat, re->viewmat, ob->obmat);
		invert_m4_m4(ob->imat, mat);
	}
	
	time= PIL_check_seconds_timer();
	if (time - lasttime > 1.0) {
		lasttime= time;
		/* clumsy copying still */
		re->i.totvert= re->totvert;
		re->i.totface= re->totvlak;
		re->i.totstrand= re->totstrand;
		re->i.tothalo= re->tothalo;
		re->i.totlamp= re->totlamp;
		re->stats_draw(re->sdh, &re->i);
	}

	ob->flag |= OB_DONE;
}

void RE_Database_Free(Render *re)
{
	LampRen *lar;

	/* will crash if we try to free empty database */
	if (!re->i.convertdone)
		return;

	/* statistics for debugging render memory usage */
	if ((G.debug & G_DEBUG) && (G.is_rendering)) {
		if ((re->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))==0) {
			BKE_image_print_memlist();
			MEM_printmemlist_stats();
		}
	}

	/* FREE */
	
	for (lar= re->lampren.first; lar; lar= lar->next) {
		freeshadowbuf(lar);
		if (lar->jitter) MEM_freeN(lar->jitter);
		if (lar->shadsamp) MEM_freeN(lar->shadsamp);
		if (lar->sunsky) MEM_freeN(lar->sunsky);
		curvemapping_free(lar->curfalloff);
	}
	
	free_volume_precache(re);
	
	BLI_freelistN(&re->lampren);
	BLI_freelistN(&re->lights);

	free_renderdata_tables(re);

	/* free orco */
	free_mesh_orco_hash(re);

	if (re->main) {
		end_render_materials(re->main);
		end_render_textures(re);
		free_pointdensities(re);
	}
	
	free_camera_inside_volumes(re);
	
	if (re->wrld.aosphere) {
		MEM_freeN(re->wrld.aosphere);
		re->wrld.aosphere= NULL;
		if (re->scene && re->scene->world)
			re->scene->world->aosphere= NULL;
	}
	if (re->wrld.aotables) {
		MEM_freeN(re->wrld.aotables);
		re->wrld.aotables= NULL;
		if (re->scene && re->scene->world)
			re->scene->world->aotables= NULL;
	}
	if (re->r.mode & R_RAYTRACE)
		free_render_qmcsampler(re);
	
	if (re->r.mode & R_RAYTRACE) freeraytree(re);

	free_sss(re);
	free_occ(re);
	free_strand_surface(re);
	
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
	re->i.convertdone = false;

	re->bakebuf= NULL;

	if (re->scene)
		if (re->scene->r.scemode & R_FREE_IMAGE)
			if ((re->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))==0)
				BKE_image_free_all_textures();

	if (re->memArena) {
		BLI_memarena_free(re->memArena);
		re->memArena = NULL;
	}
}

static int allow_render_object(Render *re, Object *ob, int nolamps, int onlyselected, Object *actob)
{
	if (is_object_hidden(re, ob))
		return 0;

	/* Only handle dupli-hiding here if there is no particle systems. Else, let those handle show/noshow. */
	if (!ob->particlesystem.first) {
		if ((ob->transflag & OB_DUPLI) && !(ob->transflag & OB_DUPLIFRAMES)) {
			return 0;
		}
	}
	
	/* don't add non-basic meta objects, ends up having renderobjects with no geometry */
	if (ob->type == OB_MBALL && ob!=BKE_mball_basis_find(re->scene, ob))
		return 0;
	
	if (nolamps && (ob->type==OB_LAMP))
		return 0;
	
	if (onlyselected && (ob!=actob && !(ob->flag & SELECT)))
		return 0;
	
	return 1;
}

static int allow_render_dupli_instance(Render *UNUSED(re), DupliObject *dob, Object *obd)
{
	ParticleSystem *psys;
	Material *ma;
	short a, *totmaterial;

	/* don't allow objects with halos. we need to have
	 * all halo's to sort them globally in advance */
	totmaterial= give_totcolp(obd);

	if (totmaterial) {
		for (a= 0; a<*totmaterial; a++) {
			ma= give_current_material(obd, a + 1);
			if (ma && (ma->material_type == MA_TYPE_HALO))
				return 0;
		}
	}

	for (psys=obd->particlesystem.first; psys; psys=psys->next)
		if (!ELEM(psys->part->ren_as, PART_DRAW_BB, PART_DRAW_LINE, PART_DRAW_PATH, PART_DRAW_OB, PART_DRAW_GR))
			return 0;

	/* don't allow lamp, animated duplis, or radio render */
	return (render_object_type(obd->type) &&
			(!(dob->type == OB_DUPLIGROUP) || !dob->animated));
}

static void dupli_render_particle_set(Render *re, Object *ob, int timeoffset, int level, int enable)
{
	/* ugly function, but we need to set particle systems to their render
	 * settings before calling object_duplilist, to get render level duplis */
	Group *group;
	GroupObject *go;
	ParticleSystem *psys;
	DerivedMesh *dm;

	if (re->r.scemode & R_VIEWPORT_PREVIEW)
		return;

	if (level >= MAX_DUPLI_RECUR)
		return;
	
	if (ob->transflag & OB_DUPLIPARTS) {
		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			if (ELEM(psys->part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
				if (enable)
					psys_render_set(ob, psys, re->viewmat, re->winmat, re->winx, re->winy, timeoffset);
				else
					psys_render_restore(ob, psys);
			}
		}

		if (enable) {
			/* this is to make sure we get render level duplis in groups:
			 * the derivedmesh must be created before init_render_mesh,
			 * since object_duplilist does dupliparticles before that */
			dm = mesh_create_derived_render(re->scene, ob, CD_MASK_RENDER_INTERNAL);
			dm->release(dm);

			for (psys=ob->particlesystem.first; psys; psys=psys->next)
				psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
		}
	}

	if (ob->dup_group==NULL) return;
	group= ob->dup_group;

	for (go= group->gobject.first; go; go= go->next)
		dupli_render_particle_set(re, go->ob, timeoffset, level+1, enable);
}

static int get_vector_renderlayers(Scene *sce)
{
	SceneRenderLayer *srl;
	unsigned int lay= 0;

	for (srl= sce->r.layers.first; srl; srl= srl->next)
		if (srl->passflag & SCE_PASS_VECTOR)
			lay |= srl->lay;

	return lay;
}

static void add_group_render_dupli_obs(Render *re, Group *group, int nolamps, int onlyselected, Object *actob, int timeoffset, int level)
{
	GroupObject *go;
	Object *ob;

	/* simple preventing of too deep nested groups */
	if (level>MAX_DUPLI_RECUR) return;

	/* recursively go into dupligroups to find objects with OB_RENDER_DUPLI
	 * that were not created yet */
	for (go= group->gobject.first; go; go= go->next) {
		ob= go->ob;

		if (ob->flag & OB_DONE) {
			if (ob->transflag & OB_RENDER_DUPLI) {
				if (allow_render_object(re, ob, nolamps, onlyselected, actob)) {
					init_render_object(re, ob, NULL, NULL, NULL, timeoffset);
					ob->transflag &= ~OB_RENDER_DUPLI;

					if (ob->dup_group)
						add_group_render_dupli_obs(re, ob->dup_group, nolamps, onlyselected, actob, timeoffset, level+1);
				}
			}
		}
	}
}

static void database_init_objects(Render *re, unsigned int renderlay, int nolamps, int onlyselected, Object *actob, int timeoffset)
{
	Base *base;
	Object *ob;
	Group *group;
	ObjectInstanceRen *obi;
	Scene *sce_iter;
	int lay, vectorlay;

	/* for duplis we need the Object texture mapping to work as if
	 * untransformed, set_dupli_tex_mat sets the matrix to allow that
	 * NULL is just for init */
	set_dupli_tex_mat(NULL, NULL, NULL, NULL);

	/* loop over all objects rather then using SETLOOPER because we may
	 * reference an mtex-mapped object which isn't rendered or is an
	 * empty in a dupli group. We could scan all render material/lamp/world
	 * mtex's for mapto objects but its easier just to set the
	 * 'imat' / 'imat_ren' on all and unlikely to be a performance hit
	 * See bug: [#28744] - campbell */
	for (ob= re->main->object.first; ob; ob= ob->id.next) {
		float mat[4][4];
		
		/* imat objects has to be done here, since displace can have texture using Object map-input */
		mul_m4_m4m4(mat, re->viewmat, ob->obmat);
		invert_m4_m4(ob->imat_ren, mat);
		copy_m4_m4(ob->imat, ob->imat_ren);
		/* each object should only be rendered once */
		ob->flag &= ~OB_DONE;
		ob->transflag &= ~OB_RENDER_DUPLI;
	}

	for (SETLOOPER(re->scene, sce_iter, base)) {
		ob= base->object;

		/* in the prev/next pass for making speed vectors, avoid creating
		 * objects that are not on a renderlayer with a vector pass, can
		 * save a lot of time in complex scenes */
		vectorlay= get_vector_renderlayers(re->scene);
		lay= (timeoffset)? renderlay & vectorlay: renderlay;

		/* if the object has been restricted from rendering in the outliner, ignore it */
		if (is_object_restricted(re, ob)) continue;

		/* OB_DONE means the object itself got duplicated, so was already converted */
		if (ob->flag & OB_DONE) {
			/* OB_RENDER_DUPLI means instances for it were already created, now
			 * it still needs to create the ObjectRen containing the data */
			if (ob->transflag & OB_RENDER_DUPLI) {
				if (allow_render_object(re, ob, nolamps, onlyselected, actob)) {
					init_render_object(re, ob, NULL, NULL, NULL, timeoffset);
					ob->transflag &= ~OB_RENDER_DUPLI;
				}
			}
		}
		else if ((base->lay & lay) || (ob->type==OB_LAMP && (base->lay & re->lay)) ) {
			if ((ob->transflag & OB_DUPLI) && (ob->type!=OB_MBALL)) {
				DupliObject *dob;
				ListBase *duplilist;
				DupliApplyData *duplilist_apply_data = NULL;
				int i;

				/* create list of duplis generated by this object, particle
				 * system need to have render settings set for dupli particles */
				dupli_render_particle_set(re, ob, timeoffset, 0, 1);
				duplilist = object_duplilist(re->eval_ctx, re->scene, ob);
				duplilist_apply_data = duplilist_apply(ob, NULL, duplilist);
				/* postpone 'dupli_render_particle_set', since RE_addRenderInstance reads
				 * index values from 'dob->persistent_id[0]', referencing 'psys->child' which
				 * may be smaller once the particle system is restored, see: T45563. */

				for (dob= duplilist->first, i = 0; dob; dob= dob->next, ++i) {
					DupliExtraData *dob_extra = &duplilist_apply_data->extra[i];
					Object *obd= dob->ob;

					copy_m4_m4(obd->obmat, dob->mat);

					/* group duplis need to set ob matrices correct, for deform. so no_draw is part handled */
					if (!(obd->transflag & OB_RENDER_DUPLI) && dob->no_draw)
						continue;

					if (is_object_hidden(re, obd))
						continue;

					if (obd->type==OB_MBALL)
						continue;

					if (!allow_render_object(re, obd, nolamps, onlyselected, actob))
						continue;

					if (allow_render_dupli_instance(re, dob, obd)) {
						ParticleSystem *psys;
						ObjectRen *obr = NULL;
						int psysindex;
						float mat[4][4];

						obi=NULL;

						/* instances instead of the actual object are added in two cases, either
						 * this is a duplivert/face/particle, or it is a non-animated object in
						 * a dupligroup that has already been created before */
						if (dob->type != OB_DUPLIGROUP || (obr=find_dupligroup_dupli(re, obd, 0))) {
							mul_m4_m4m4(mat, re->viewmat, dob->mat);
														/* ob = particle system, use that layer */
							obi = RE_addRenderInstance(re, NULL, obd, ob, dob->persistent_id[0], 0, mat, ob->lay, dob);

							/* fill in instance variables for texturing */
							set_dupli_tex_mat(re, obi, dob, dob_extra->obmat);
							if (dob->type != OB_DUPLIGROUP) {
								copy_v3_v3(obi->dupliorco, dob->orco);
								obi->dupliuv[0]= dob->uv[0];
								obi->dupliuv[1]= dob->uv[1];
							}
							else {
								/* for the second case, setup instance to point to the already
								 * created object, and possibly setup instances if this object
								 * itself was duplicated. for the first case find_dupli_instances
								 * will be called later. */
								assign_dupligroup_dupli(re, obi, obr, dob);
								if (obd->transflag & OB_RENDER_DUPLI)
									find_dupli_instances(re, obr, dob);
							}
						}

						/* same logic for particles, each particle system has it's own object, so
						 * need to go over them separately */
						psysindex= 1;
						for (psys=obd->particlesystem.first; psys; psys=psys->next) {
							if (dob->type != OB_DUPLIGROUP || (obr=find_dupligroup_dupli(re, obd, psysindex))) {
								if (obi == NULL)
									mul_m4_m4m4(mat, re->viewmat, dob->mat);
								obi = RE_addRenderInstance(re, NULL, obd, ob, dob->persistent_id[0], psysindex++, mat, obd->lay, dob);

								set_dupli_tex_mat(re, obi, dob, dob_extra->obmat);
								if (dob->type != OB_DUPLIGROUP) {
									copy_v3_v3(obi->dupliorco, dob->orco);
									obi->dupliuv[0]= dob->uv[0];
									obi->dupliuv[1]= dob->uv[1];
								}
								else {
									assign_dupligroup_dupli(re, obi, obr, dob);
									if (obd->transflag & OB_RENDER_DUPLI)
										find_dupli_instances(re, obr, dob);
								}
							}
						}

						if (obi==NULL)
							/* can't instance, just create the object */
							init_render_object(re, obd, ob, dob, dob_extra->obmat, timeoffset);
						
						if (dob->type != OB_DUPLIGROUP) {
							obd->flag |= OB_DONE;
							obd->transflag |= OB_RENDER_DUPLI;
						}
					}
					else
						init_render_object(re, obd, ob, dob, dob_extra->obmat, timeoffset);
					
					if (re->test_break(re->tbh)) break;
				}

				/* restore particle system */
				dupli_render_particle_set(re, ob, timeoffset, 0, false);

				if (duplilist_apply_data) {
					duplilist_restore(duplilist, duplilist_apply_data);
					duplilist_free_apply_data(duplilist_apply_data);
				}
				free_object_duplilist(duplilist);

				if (allow_render_object(re, ob, nolamps, onlyselected, actob))
					init_render_object(re, ob, NULL, NULL, NULL, timeoffset);
			}
			else if (allow_render_object(re, ob, nolamps, onlyselected, actob))
				init_render_object(re, ob, NULL, NULL, NULL, timeoffset);
		}

		if (re->test_break(re->tbh)) break;
	}

	/* objects in groups with OB_RENDER_DUPLI set still need to be created,
	 * since they may not be part of the scene */
	for (group= re->main->group.first; group; group=group->id.next)
		add_group_render_dupli_obs(re, group, nolamps, onlyselected, actob, timeoffset, 0);

	if (!re->test_break(re->tbh))
		RE_makeRenderInstances(re);
}

/* used to be 'rotate scene' */
void RE_Database_FromScene(Render *re, Main *bmain, Scene *scene, unsigned int lay, int use_camera_view)
{
	Scene *sce;
	Object *camera;
	float mat[4][4];
	float amb[3];

	re->main= bmain;
	re->scene= scene;
	re->lay= lay;

	if (re->r.scemode & R_VIEWPORT_PREVIEW)
		re->scene_color_manage = BKE_scene_check_color_management_enabled(scene);
	
	/* scene needs to be set to get camera */
	camera= RE_GetCamera(re);
	
	/* per second, per object, stats print this */
	re->i.infostr= "Preparing Scene data";
	re->i.cfra= scene->r.cfra;
	BLI_strncpy(re->i.scene_name, scene->id.name + 2, sizeof(re->i.scene_name));
	
	/* XXX add test if dbase was filled already? */
	
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "render db arena");
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
	re->lights.first= re->lights.last= NULL;
	re->lampren.first= re->lampren.last= NULL;

	re->i.partsdone = false;	/* signal now in use for previewrender */
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if (re->lay & 0xFF000000)
		lay &= 0xFF000000;
	
	/* applies changes fully */
	if ((re->r.scemode & (R_NO_FRAME_UPDATE|R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))==0) {
		BKE_scene_update_for_newframe(re->eval_ctx, re->main, re->scene, lay);
		render_update_anim_renderdata(re, &re->scene->r);
	}
	
	/* if no camera, viewmat should have been set! */
	if (use_camera_view && camera) {
		/* called before but need to call again in case of lens animation from the
		 * above call to BKE_scene_update_for_newframe, fixes bug. [#22702].
		 * following calls don't depend on 'RE_SetCamera' */
		RE_SetCamera(re, camera);
		RE_GetCameraModelMatrix(re, camera, mat);
		invert_m4(mat);
		RE_SetView(re, mat);

		/* force correct matrix for scaled cameras */
		DAG_id_tag_update_ex(re->main, &camera->id, OB_RECALC_OB);
	}
	
	/* store for incremental render, viewmat rotates dbase */
	copy_m4_m4(re->viewmat_orig, re->viewmat);
	
	init_render_world(re);	/* do first, because of ambient. also requires re->osa set correct */
	if (re->r.mode & R_RAYTRACE) {
		init_render_qmcsampler(re);

		if (re->wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT))
			if (re->wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
				init_ao_sphere(re, &re->wrld);
	}
	
	/* still bad... doing all */
	init_render_textures(re);
	copy_v3_v3(amb, &re->wrld.ambr);
	init_render_materials(re->main, re->r.mode, amb, (re->r.scemode & R_BUTS_PREVIEW) == 0);
	set_node_shader_lamp_loop(shade_material_loop);

	/* MAKE RENDER DATA */
	database_init_objects(re, lay, 0, 0, NULL, 0);
	
	if (!re->test_break(re->tbh)) {
		set_material_lightgroups(re);
		for (sce= re->scene; sce; sce= sce->set)
			set_renderlayer_lightgroups(re, sce);
		
		/* for now some clumsy copying still */
		re->i.totvert= re->totvert;
		re->i.totface= re->totvlak;
		re->i.totstrand= re->totstrand;
		re->i.tothalo= re->tothalo;
		re->i.totlamp= re->totlamp;
		re->stats_draw(re->sdh, &re->i);
	}
}

void RE_Database_Preprocess(Render *re)
{
	if (!re->test_break(re->tbh)) {
		int tothalo;

		tothalo= re->tothalo;
		sort_halos(re, tothalo);
		
		init_camera_inside_volumes(re);
		
		re->i.infostr = IFACE_("Creating Shadowbuffers");
		re->stats_draw(re->sdh, &re->i);

		/* SHADOW BUFFER */
		threaded_makeshadowbufs(re);

		/* old code checked for internal render (aka not yafray) */
		{
			/* raytree */
			if (!re->test_break(re->tbh)) {
				if (re->r.mode & R_RAYTRACE) {
					makeraytree(re);
				}
			}
			/* ENVIRONMENT MAPS */
			if (!re->test_break(re->tbh))
				make_envmaps(re);
				
			/* point density texture */
			if (!re->test_break(re->tbh))
				make_pointdensities(re);
			/* voxel data texture */
			if (!re->test_break(re->tbh))
				make_voxeldata(re);
		}
		
		if (!re->test_break(re->tbh))
			project_renderdata(re, projectverto, (re->r.mode & R_PANORAMA) != 0, 0, 1);
		
		/* Occlusion */
		if ((re->wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && !re->test_break(re->tbh))
			if (re->wrld.ao_gather_method == WO_AOGATHER_APPROX)
				if (re->r.mode & R_SHADOW)
					make_occ_tree(re);

		/* SSS */
		if ((re->r.mode & R_SSS) && !re->test_break(re->tbh))
			make_sss_tree(re);
		
		if (!re->test_break(re->tbh))
			if (re->r.mode & R_RAYTRACE)
				volume_precache(re);
	}
	
	re->i.convertdone = true;

	if (re->test_break(re->tbh))
		RE_Database_Free(re);
	
	re->i.infostr = NULL;
	re->stats_draw(re->sdh, &re->i);
}

/* exported call to recalculate hoco for vertices, when winmat changed */
void RE_DataBase_ApplyWindow(Render *re)
{
	project_renderdata(re, projectverto, 0, 0, 0);
}

/* exported call to rotate render data again, when viewmat changed */
void RE_DataBase_IncrementalView(Render *re, float viewmat[4][4], int restore)
{
	float oldviewinv[4][4], tmat[4][4];
	
	invert_m4_m4(oldviewinv, re->viewmat_orig);
	
	/* we have to correct for the already rotated vertexcoords */
	mul_m4_m4m4(tmat, viewmat, oldviewinv);
	
	copy_m4_m4(re->viewmat, viewmat);
	invert_m4_m4(re->viewinv, re->viewmat);
	
	init_camera_inside_volumes(re);

	env_rotate_scene(re, tmat, !restore);

	/* SSS points distribution depends on view */
	if ((re->r.mode & R_SSS) && !re->test_break(re->tbh))
		make_sss_tree(re);
}


void RE_DataBase_GetView(Render *re, float mat[4][4])
{
	copy_m4_m4(mat, re->viewmat);
}

/* ------------------------------------------------------------------------- */
/* Speed Vectors															 */
/* ------------------------------------------------------------------------- */

static void database_fromscene_vectors(Render *re, Scene *scene, unsigned int lay, int timeoffset)
{
	Object *camera= RE_GetCamera(re);
	float mat[4][4];
	
	re->scene= scene;
	re->lay= lay;
	
	/* XXX add test if dbase was filled already? */
	
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "vector render db arena");
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
	re->i.totface=re->i.totvert=re->i.totstrand=re->i.totlamp=re->i.tothalo= 0;
	re->lights.first= re->lights.last= NULL;
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if (re->lay & 0xFF000000)
		lay &= 0xFF000000;
	
	/* applies changes fully */
	scene->r.cfra += timeoffset;
	BKE_scene_update_for_newframe(re->eval_ctx, re->main, re->scene, lay);
	
	/* if no camera, viewmat should have been set! */
	if (camera) {
		RE_GetCameraModelMatrix(re, camera, mat);
		normalize_m4(mat);
		invert_m4(mat);
		RE_SetView(re, mat);
	}
	
	/* MAKE RENDER DATA */
	database_init_objects(re, lay, 0, 0, NULL, timeoffset);
	
	if (!re->test_break(re->tbh))
		project_renderdata(re, projectverto, (re->r.mode & R_PANORAMA) != 0, 0, 1);

	/* do this in end, particles for example need cfra */
	scene->r.cfra -= timeoffset;
}

/* choose to use static, to prevent giving too many args to this call */
static void speedvector_project(Render *re, float zco[2], const float co[3], const float ho[4])
{
	static float pixelphix=0.0f, pixelphiy=0.0f, zmulx=0.0f, zmuly=0.0f;
	static int pano= 0;
	float div;
	
	/* initialize */
	if (re) {
		pano= re->r.mode & R_PANORAMA;
		
		/* precalculate amount of radians 1 pixel rotates */
		if (pano) {
			/* size of 1 pixel mapped to viewplane coords */
			float psize;

			psize = BLI_rctf_size_x(&re->viewplane) / (float)re->winx;
			/* x angle of a pixel */
			pixelphix = atan(psize / re->clipsta);
			
			psize = BLI_rctf_size_y(&re->viewplane) / (float)re->winy;
			/* y angle of a pixel */
			pixelphiy = atan(psize / re->clipsta);
		}
		zmulx= re->winx/2;
		zmuly= re->winy/2;
		
		return;
	}
	
	/* now map hocos to screenspace, uses very primitive clip still */
	if (ho[3]<0.1f) div= 10.0f;
	else div= 1.0f/ho[3];
	
	/* use cylinder projection */
	if (pano) {
		float vec[3], ang;
		/* angle between (0, 0, -1) and (co) */
		copy_v3_v3(vec, co);

		ang= saacos(-vec[2]/sqrtf(vec[0]*vec[0] + vec[2]*vec[2]));
		if (vec[0]<0.0f) ang= -ang;
		zco[0]= ang/pixelphix + zmulx;
		
		ang= 0.5f*(float)M_PI - saacos(vec[1] / len_v3(vec));
		zco[1]= ang/pixelphiy + zmuly;
		
	}
	else {
		zco[0]= zmulx*(1.0f+ho[0]*div);
		zco[1]= zmuly*(1.0f+ho[1]*div);
	}
}

static void calculate_speedvector(const float vectors[2], int step, float winsq, float winroot, const float co[3], const float ho[4], float speed[4])
{
	float zco[2], len;

	speedvector_project(NULL, zco, co, ho);
	
	zco[0]= vectors[0] - zco[0];
	zco[1]= vectors[1] - zco[1];
	
	/* enable nice masks for hardly moving stuff or float inaccuracy */
	if (zco[0]<0.1f && zco[0]>-0.1f && zco[1]<0.1f && zco[1]>-0.1f ) {
		zco[0]= 0.0f;
		zco[1]= 0.0f;
	}
	
	/* maximize speed for image width, otherwise it never looks good */
	len= zco[0]*zco[0] + zco[1]*zco[1];
	if (len > winsq) {
		len= winroot/sqrtf(len);
		zco[0]*= len;
		zco[1]*= len;
	}
	
	/* note; in main vecblur loop speedvec is negated again */
	if (step) {
		speed[2]= -zco[0];
		speed[3]= -zco[1];
	}
	else {
		speed[0]= zco[0];
		speed[1]= zco[1];
	}
}

static float *calculate_strandsurface_speedvectors(Render *re, ObjectInstanceRen *obi, StrandSurface *mesh)
{
	if (mesh->co && mesh->prevco && mesh->nextco) {
		float winsq= (float)re->winx*(float)re->winy; /* int's can wrap on large images */
		float winroot= sqrtf(winsq);
		float (*winspeed)[4];
		float ho[4], prevho[4], nextho[4], winmat[4][4], vec[2];
		int a;

		if (obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(winmat, re->winmat, obi->mat);
		else
			copy_m4_m4(winmat, re->winmat);

		winspeed= MEM_callocN(sizeof(float)*4*mesh->totvert, "StrandSurfWin");

		for (a=0; a<mesh->totvert; a++) {
			projectvert(mesh->co[a], winmat, ho);

			projectvert(mesh->prevco[a], winmat, prevho);
			speedvector_project(NULL, vec, mesh->prevco[a], prevho);
			calculate_speedvector(vec, 0, winsq, winroot, mesh->co[a], ho, winspeed[a]);

			projectvert(mesh->nextco[a], winmat, nextho);
			speedvector_project(NULL, vec, mesh->nextco[a], nextho);
			calculate_speedvector(vec, 1, winsq, winroot, mesh->co[a], ho, winspeed[a]);
		}

		return (float *)winspeed;
	}

	return NULL;
}

static void calculate_speedvectors(Render *re, ObjectInstanceRen *obi, float *vectors, int step)
{
	ObjectRen *obr= obi->obr;
	VertRen *ver= NULL;
	StrandRen *strand= NULL;
	StrandBuffer *strandbuf;
	StrandSurface *mesh= NULL;
	float *speed, (*winspeed)[4]=NULL, ho[4], winmat[4][4];
	float *co1, *co2, *co3, *co4, w[4];
	float winsq = (float)re->winx * (float)re->winy, winroot = sqrtf(winsq);  /* int's can wrap on large images */
	int a, *face, *index;

	if (obi->flag & R_TRANSFORMED)
		mul_m4_m4m4(winmat, re->winmat, obi->mat);
	else
		copy_m4_m4(winmat, re->winmat);

	if (obr->vertnodes) {
		for (a=0; a<obr->totvert; a++, vectors+=2) {
			if ((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
			else ver++;

			speed= RE_vertren_get_winspeed(obi, ver, 1);
			projectvert(ver->co, winmat, ho);
			calculate_speedvector(vectors, step, winsq, winroot, ver->co, ho, speed);
		}
	}

	if (obr->strandnodes) {
		strandbuf= obr->strandbuf;
		mesh= (strandbuf)? strandbuf->surface: NULL;

		/* compute speed vectors at surface vertices */
		if (mesh)
			winspeed= (float(*)[4])calculate_strandsurface_speedvectors(re, obi, mesh);

		if (winspeed) {
			for (a=0; a<obr->totstrand; a++, vectors+=2) {
				if ((a & 255)==0) strand= obr->strandnodes[a>>8].strand;
				else strand++;

				index= RE_strandren_get_face(obr, strand, 0);
				if (index && *index < mesh->totface) {
					speed= RE_strandren_get_winspeed(obi, strand, 1);

					/* interpolate speed vectors from strand surface */
					face= mesh->face[*index];

					co1 = mesh->co[face[0]];
					co2 = mesh->co[face[1]];
					co3 = mesh->co[face[2]];

					if (face[3]) {
						co4 = mesh->co[face[3]];
						interp_weights_quad_v3(w, co1, co2, co3, co4, strand->vert->co);
					}
					else {
						interp_weights_tri_v3(w, co1, co2, co3, strand->vert->co);
					}

					zero_v4(speed);
					madd_v4_v4fl(speed, winspeed[face[0]], w[0]);
					madd_v4_v4fl(speed, winspeed[face[1]], w[1]);
					madd_v4_v4fl(speed, winspeed[face[2]], w[2]);
					if (face[3])
						madd_v4_v4fl(speed, winspeed[face[3]], w[3]);
				}
			}

			MEM_freeN(winspeed);
		}
	}
}

static int load_fluidsimspeedvectors(Render *re, ObjectInstanceRen *obi, float *vectors, int step)
{
	ObjectRen *obr= obi->obr;
	Object *fsob= obr->ob;
	VertRen *ver= NULL;
	float *speed, div, zco[2], avgvel[4] = {0.0, 0.0, 0.0, 0.0};
	float zmulx= re->winx/2, zmuly= re->winy/2, len;
	float winsq = (float)re->winx * (float)re->winy, winroot= sqrtf(winsq); /* int's can wrap on large images */
	int a, j;
	float hoco[4], ho[4], fsvec[4], camco[4];
	float mat[4][4], winmat[4][4];
	float imat[4][4];
	FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(fsob, eModifierType_Fluidsim);
	FluidsimSettings *fss;
	FluidVertexVelocity *velarray = NULL;
	
	/* only one step needed */
	if (step) return 1;
	
	if (fluidmd)
		fss = fluidmd->fss;
	else
		return 0;
	
	copy_m4_m4(mat, re->viewmat);
	invert_m4_m4(imat, mat);

	/* set first vertex OK */
	if (!fss->meshVelocities) return 0;
	
	if ( obr->totvert != fss->totvert) {
		//fprintf(stderr, "load_fluidsimspeedvectors - modified fluidsim mesh, not using speed vectors (%d,%d)...\n", obr->totvert, fsob->fluidsimSettings->meshSurface->totvert); // DEBUG
		return 0;
	}
	
	velarray = fss->meshVelocities;

	if (obi->flag & R_TRANSFORMED)
		mul_m4_m4m4(winmat, re->winmat, obi->mat);
	else
		copy_m4_m4(winmat, re->winmat);
	
	/* (bad) HACK calculate average velocity */
	/* better solution would be fixing getVelocityAt() in intern/elbeem/intern/solver_util.cpp
	 * so that also small drops/little water volumes return a velocity != 0.
	 * But I had no luck in fixing that function - DG */
	for (a=0; a<obr->totvert; a++) {
		for (j=0;j<3;j++) avgvel[j] += velarray[a].vel[j];
		
	}
	for (j=0;j<3;j++) avgvel[j] /= (float)(obr->totvert);
	
	
	for (a=0; a<obr->totvert; a++, vectors+=2) {
		if ((a & 255)==0)
			ver= obr->vertnodes[a>>8].vert;
		else
			ver++;

		/* get fluid velocity */
		fsvec[3] = 0.0f;
		//fsvec[0] = fsvec[1] = fsvec[2] = fsvec[3] = 0.0; fsvec[2] = 2.0f; // NT fixed test
		for (j=0;j<3;j++) fsvec[j] = velarray[a].vel[j];
		
		/* (bad) HACK insert average velocity if none is there (see previous comment) */
		if ((fsvec[0] == 0.0f) && (fsvec[1] == 0.0f) && (fsvec[2] == 0.0f)) {
			fsvec[0] = avgvel[0];
			fsvec[1] = avgvel[1];
			fsvec[2] = avgvel[2];
		}
		
		/* transform (=rotate) to cam space */
		camco[0] = dot_v3v3(imat[0], fsvec);
		camco[1] = dot_v3v3(imat[1], fsvec);
		camco[2] = dot_v3v3(imat[2], fsvec);

		/* get homogeneous coordinates */
		projectvert(camco, winmat, hoco);
		projectvert(ver->co, winmat, ho);
		
		/* now map hocos to screenspace, uses very primitive clip still */
		/* use ho[3] of original vertex, xy component of vel. direction */
		if (ho[3]<0.1f) div= 10.0f;
		else div= 1.0f/ho[3];
		zco[0]= zmulx*hoco[0]*div;
		zco[1]= zmuly*hoco[1]*div;
		
		/* maximize speed as usual */
		len= zco[0]*zco[0] + zco[1]*zco[1];
		if (len > winsq) {
			len= winroot/sqrtf(len);
			zco[0]*= len; zco[1]*= len;
		}

		speed= RE_vertren_get_winspeed(obi, ver, 1);
		/* set both to the same value */
		speed[0]= speed[2]= zco[0];
		speed[1]= speed[3]= zco[1];
		//if (a < 20) fprintf(stderr,"speed %d %f,%f | camco %f,%f,%f | hoco %f,%f,%f,%f\n", a, speed[0], speed[1], camco[0],camco[1], camco[2], hoco[0],hoco[1], hoco[2],hoco[3]); // NT DEBUG
	}

	return 1;
}

/* makes copy per object of all vectors */
/* result should be that we can free entire database */
static void copy_dbase_object_vectors(Render *re, ListBase *lb)
{
	ObjectInstanceRen *obi, *obilb;
	ObjectRen *obr;
	VertRen *ver= NULL;
	float *vec, ho[4], winmat[4][4];
	int a, totvector;

	for (obi= re->instancetable.first; obi; obi= obi->next) {
		obr= obi->obr;

		obilb= MEM_mallocN(sizeof(ObjectInstanceRen), "ObInstanceVector");
		memcpy(obilb, obi, sizeof(ObjectInstanceRen));
		BLI_addtail(lb, obilb);

		obilb->totvector= totvector= obr->totvert;

		if (totvector > 0) {
			vec= obilb->vectors= MEM_mallocN(2*sizeof(float)*totvector, "vector array");

			if (obi->flag & R_TRANSFORMED)
				mul_m4_m4m4(winmat, re->winmat, obi->mat);
			else
				copy_m4_m4(winmat, re->winmat);

			for (a=0; a<obr->totvert; a++, vec+=2) {
				if ((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
				else ver++;
				
				projectvert(ver->co, winmat, ho);
				speedvector_project(NULL, vec, ver->co, ho);
			}
		}
	}
}

static void free_dbase_object_vectors(ListBase *lb)
{
	ObjectInstanceRen *obi;
	
	for (obi= lb->first; obi; obi= obi->next)
		if (obi->vectors)
			MEM_freeN(obi->vectors);
	BLI_freelistN(lb);
}

void RE_Database_FromScene_Vectors(Render *re, Main *bmain, Scene *sce, unsigned int lay)
{
	ObjectInstanceRen *obi, *oldobi;
	StrandSurface *mesh;
	ListBase *table;
	ListBase oldtable= {NULL, NULL}, newtable= {NULL, NULL};
	ListBase strandsurface;
	int step;
	
	re->i.infostr = IFACE_("Calculating previous frame vectors");
	re->r.mode |= R_SPEED;
	
	speedvector_project(re, NULL, NULL, NULL);	/* initializes projection code */
	
	/* creates entire dbase */
	database_fromscene_vectors(re, sce, lay, -1);
	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &oldtable);
		
	/* free dbase and make the future one */
	strandsurface= re->strandsurface;
	memset(&re->strandsurface, 0, sizeof(ListBase));
	re->i.convertdone = true;
	RE_Database_Free(re);
	re->strandsurface= strandsurface;
	
	if (!re->test_break(re->tbh)) {
		/* creates entire dbase */
		re->i.infostr = IFACE_("Calculating next frame vectors");
		
		database_fromscene_vectors(re, sce, lay, +1);
	}
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &newtable);
	
	/* free dbase and make the real one */
	strandsurface= re->strandsurface;
	memset(&re->strandsurface, 0, sizeof(ListBase));
	re->i.convertdone = true;
	RE_Database_Free(re);
	re->strandsurface= strandsurface;
	
	if (!re->test_break(re->tbh)) {
		RE_Database_FromScene(re, bmain, sce, lay, 1);
		RE_Database_Preprocess(re);
	}
	
	if (!re->test_break(re->tbh)) {
		int vectorlay= get_vector_renderlayers(re->scene);

		for (step= 0; step<2; step++) {
			
			if (step)
				table= &newtable;
			else
				table= &oldtable;
			
			oldobi= table->first;
			for (obi= re->instancetable.first; obi && oldobi; obi= obi->next) {
				int ok= 1;
				FluidsimModifierData *fluidmd;

				if (!(obi->lay & vectorlay))
					continue;

				obi->totvector= obi->obr->totvert;

				/* find matching object in old table */
				if (oldobi->ob!=obi->ob || oldobi->par!=obi->par || oldobi->index!=obi->index || oldobi->psysindex!=obi->psysindex) {
					ok= 0;
					for (oldobi= table->first; oldobi; oldobi= oldobi->next)
						if (oldobi->ob==obi->ob && oldobi->par==obi->par && oldobi->index==obi->index && oldobi->psysindex==obi->psysindex)
							break;
					if (oldobi==NULL)
						oldobi= table->first;
					else
						ok= 1;
				}
				if (ok==0) {
					printf("speed table: missing object %s\n", obi->ob->id.name + 2);
					continue;
				}

				/* NT check for fluidsim special treatment */
				fluidmd = (FluidsimModifierData *)modifiers_findByType(obi->ob, eModifierType_Fluidsim);
				if (fluidmd && fluidmd->fss && (fluidmd->fss->type & OB_FLUIDSIM_DOMAIN)) {
					/* use preloaded per vertex simulation data, only does calculation for step=1 */
					/* NOTE/FIXME - velocities and meshes loaded unnecessarily often during the database_fromscene_vectors calls... */
					load_fluidsimspeedvectors(re, obi, oldobi->vectors, step);
				}
				else {
					/* check if both have same amounts of vertices */
					if (obi->totvector==oldobi->totvector)
						calculate_speedvectors(re, obi, oldobi->vectors, step);
					else
						printf("Warning: object %s has different amount of vertices or strands on other frame\n", obi->ob->id.name + 2);
				}  /* not fluidsim */

				oldobi= oldobi->next;
			}
		}
	}
	
	free_dbase_object_vectors(&oldtable);
	free_dbase_object_vectors(&newtable);

	for (mesh=re->strandsurface.first; mesh; mesh=mesh->next) {
		if (mesh->prevco) {
			MEM_freeN(mesh->prevco);
			mesh->prevco= NULL;
		}
		if (mesh->nextco) {
			MEM_freeN(mesh->nextco);
			mesh->nextco= NULL;
		}
	}
	
	re->i.infostr = NULL;
	re->stats_draw(re->sdh, &re->i);
}


/* ------------------------------------------------------------------------- */
/* Baking																	 */
/* ------------------------------------------------------------------------- */

/* setup for shaded view or bake, so only lamps and materials are initialized */
/* type:
 * RE_BAKE_LIGHT:  for shaded view, only add lamps
 * RE_BAKE_ALL:    for baking, all lamps and objects
 * RE_BAKE_NORMALS:for baking, no lamps and only selected objects
 * RE_BAKE_AO:     for baking, no lamps, but all objects
 * RE_BAKE_TEXTURE:for baking, no lamps, only selected objects
 * RE_BAKE_VERTEX_COLORS:for baking, no lamps, only selected objects
 * RE_BAKE_DISPLACEMENT:for baking, no lamps, only selected objects
 * RE_BAKE_DERIVATIVE:for baking, no lamps, only selected objects
 * RE_BAKE_SHADOW: for baking, only shadows, but all objects
 */
void RE_Database_Baking(Render *re, Main *bmain, Scene *scene, unsigned int lay, const int type, Object *actob)
{
	Object *camera;
	float mat[4][4];
	float amb[3];
	const short onlyselected= !ELEM(type, RE_BAKE_LIGHT, RE_BAKE_ALL, RE_BAKE_SHADOW, RE_BAKE_AO, RE_BAKE_VERTEX_COLORS);
	const short nolamps= ELEM(type, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE, RE_BAKE_VERTEX_COLORS);

	re->main= bmain;
	re->scene= scene;
	re->lay= lay;

	/* renderdata setup and exceptions */
	render_copy_renderdata(&re->r, &scene->r);

	RE_init_threadcount(re);
	
	re->flag |= R_BAKING;
	re->excludeob= actob;
	if (actob)
		re->flag |= R_BAKE_TRACE;

	if (type==RE_BAKE_NORMALS && re->r.bake_normal_space==R_BAKE_SPACE_TANGENT)
		re->flag |= R_NEED_TANGENT;

	if (type==RE_BAKE_VERTEX_COLORS)
		re->flag |=  R_NEED_VCOL;

	if (!actob && ELEM(type, RE_BAKE_LIGHT, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE, RE_BAKE_VERTEX_COLORS)) {
		re->r.mode &= ~R_SHADOW;
		re->r.mode &= ~R_RAYTRACE;
	}
	
	if (!actob && (type==RE_BAKE_SHADOW)) {
		re->r.mode |= R_SHADOW;
	}
	
	/* setup render stuff */
	re->memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "bake db arena");
	
	re->totvlak=re->totvert=re->totstrand=re->totlamp=re->tothalo= 0;
	re->lights.first= re->lights.last= NULL;
	re->lampren.first= re->lampren.last= NULL;

	/* in localview, lamps are using normal layers, objects only local bits */
	if (re->lay & 0xFF000000)
		lay &= 0xFF000000;
	
	camera= RE_GetCamera(re);
	
	/* if no camera, set unit */
	if (camera) {
		normalize_m4_m4(mat, camera->obmat);
		invert_m4(mat);
		RE_SetView(re, mat);
	}
	else {
		unit_m4(mat);
		RE_SetView(re, mat);
	}
	copy_m3_m4(re->imat, re->viewinv);

	/* TODO: deep shadow maps + baking + strands */
	/* strands use the window matrix and view size, there is to correct
	 * window matrix but at least avoids malloc and crash loop [#27807] */
	unit_m4(re->winmat);
	re->winx= re->winy= 256;
	/* done setting dummy values */

	init_render_world(re);	/* do first, because of ambient. also requires re->osa set correct */
	if (re->r.mode & R_RAYTRACE) {
		init_render_qmcsampler(re);
		
		if (re->wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT))
			if (re->wrld.ao_samp_method == WO_AOSAMP_CONSTANT)
				init_ao_sphere(re, &re->wrld);
	}
	
	/* still bad... doing all */
	init_render_textures(re);
	
	copy_v3_v3(amb, &re->wrld.ambr);
	init_render_materials(re->main, re->r.mode, amb, true);
	
	set_node_shader_lamp_loop(shade_material_loop);
	
	/* MAKE RENDER DATA */
	database_init_objects(re, lay, nolamps, onlyselected, actob, 0);

	set_material_lightgroups(re);
	
	/* SHADOW BUFFER */
	if (type!=RE_BAKE_LIGHT)
		if (re->r.mode & R_SHADOW)
			threaded_makeshadowbufs(re);

	/* raytree */
	if (!re->test_break(re->tbh))
		if (re->r.mode & R_RAYTRACE)
			makeraytree(re);
	
	/* point density texture */
	if (!re->test_break(re->tbh))
		make_pointdensities(re);

	/* voxel data texture */
	if (!re->test_break(re->tbh))
		make_voxeldata(re);

	/* occlusion */
	if ((re->wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && !re->test_break(re->tbh))
		if (re->wrld.ao_gather_method == WO_AOGATHER_APPROX)
			if (re->r.mode & R_SHADOW)
				make_occ_tree(re);

	re->i.convertdone = true;
}
