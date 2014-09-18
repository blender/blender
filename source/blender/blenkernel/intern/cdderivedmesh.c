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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ben Batt <benbatt@gmail.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implementation of CDDerivedMesh.
 *
 * BKE_cdderivedmesh.h contains the function prototypes for this file.
 *
 */

/** \file blender/blenkernel/intern/cdderivedmesh.c
 *  \ingroup bke
 */

#include "GL/glew.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_utildefines.h"
#include "BLI_stackdefines.h"

#include "BKE_pbvh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_paint.h"
#include "BKE_editmesh.h"
#include "BKE_curve.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h" /* for Curve */

#include "MEM_guardedalloc.h"

#include "GPU_buffers.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

#include <string.h>
#include <limits.h>
#include <math.h>

extern GLubyte stipple_quarttone[128]; /* glutil.c, bad level data */

typedef struct {
	DerivedMesh dm;

	/* these point to data in the DerivedMesh custom data layers,
	 * they are only here for efficiency and convenience **/
	MVert *mvert;
	MEdge *medge;
	MFace *mface;
	MLoop *mloop;
	MPoly *mpoly;

	/* Cached */
	struct PBVH *pbvh;
	bool pbvh_draw;

	/* Mesh connectivity */
	MeshElemMap *pmap;
	int *pmap_mem;
} CDDerivedMesh;

/**************** DerivedMesh interface functions ****************/
static int cdDM_getNumVerts(DerivedMesh *dm)
{
	return dm->numVertData;
}

static int cdDM_getNumEdges(DerivedMesh *dm)
{
	return dm->numEdgeData;
}

static int cdDM_getNumTessFaces(DerivedMesh *dm)
{
	/* uncomment and add a breakpoint on the printf()
	 * to help debug tessfaces issues since BMESH merge. */
#if 0
	if (dm->numTessFaceData == 0 && dm->numPolyData != 0) {
		printf("%s: has no faces!, call DM_ensure_tessface() if you need them\n");
	}
#endif
	return dm->numTessFaceData;
}

static int cdDM_getNumLoops(DerivedMesh *dm)
{
	return dm->numLoopData;
}

static int cdDM_getNumPolys(DerivedMesh *dm)
{
	return dm->numPolyData;
}

static void cdDM_getVert(DerivedMesh *dm, int index, MVert *r_vert)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	*r_vert = cddm->mvert[index];
}

static void cdDM_getEdge(DerivedMesh *dm, int index, MEdge *r_edge)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	*r_edge = cddm->medge[index];
}

static void cdDM_getTessFace(DerivedMesh *dm, int index, MFace *r_face)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	*r_face = cddm->mface[index];
}

static void cdDM_copyVertArray(DerivedMesh *dm, MVert *r_vert)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	memcpy(r_vert, cddm->mvert, sizeof(*r_vert) * dm->numVertData);
}

static void cdDM_copyEdgeArray(DerivedMesh *dm, MEdge *r_edge)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	memcpy(r_edge, cddm->medge, sizeof(*r_edge) * dm->numEdgeData);
}

static void cdDM_copyTessFaceArray(DerivedMesh *dm, MFace *r_face)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	memcpy(r_face, cddm->mface, sizeof(*r_face) * dm->numTessFaceData);
}

static void cdDM_copyLoopArray(DerivedMesh *dm, MLoop *r_loop)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	memcpy(r_loop, cddm->mloop, sizeof(*r_loop) * dm->numLoopData);
}

static void cdDM_copyPolyArray(DerivedMesh *dm, MPoly *r_poly)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	memcpy(r_poly, cddm->mpoly, sizeof(*r_poly) * dm->numPolyData);
}

static void cdDM_getMinMax(DerivedMesh *dm, float r_min[3], float r_max[3])
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	int i;

	if (dm->numVertData) {
		for (i = 0; i < dm->numVertData; i++) {
			minmax_v3v3_v3(r_min, r_max, cddm->mvert[i].co);
		}
	}
	else {
		zero_v3(r_min);
		zero_v3(r_max);
	}
}

static void cdDM_getVertCo(DerivedMesh *dm, int index, float r_co[3])
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;

	copy_v3_v3(r_co, cddm->mvert[index].co);
}

static void cdDM_getVertCos(DerivedMesh *dm, float (*r_cos)[3])
{
	MVert *mv = CDDM_get_verts(dm);
	int i;

	for (i = 0; i < dm->numVertData; i++, mv++)
		copy_v3_v3(r_cos[i], mv->co);
}

static void cdDM_getVertNo(DerivedMesh *dm, int index, float r_no[3])
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	normal_short_to_float_v3(r_no, cddm->mvert[index].no);
}

static const MeshElemMap *cdDM_getPolyMap(Object *ob, DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;

	if (!cddm->pmap && ob->type == OB_MESH) {
		Mesh *me = ob->data;

		BKE_mesh_vert_poly_map_create(&cddm->pmap, &cddm->pmap_mem,
		                     me->mpoly, me->mloop,
		                     me->totvert, me->totpoly, me->totloop);
	}

	return cddm->pmap;
}

static bool check_sculpt_object_deformed(Object *object, bool for_construction)
{
	bool deformed = false;

	/* Active modifiers means extra deformation, which can't be handled correct
	 * on birth of PBVH and sculpt "layer" levels, so use PBVH only for internal brush
	 * stuff and show final DerivedMesh so user would see actual object shape.
	 */
	deformed |= object->sculpt->modifiers_active;

	if (for_construction) {
		deformed |= object->sculpt->kb != NULL;
	}
	else {
		/* As in case with modifiers, we can't synchronize deformation made against
		 * PBVH and non-locked keyblock, so also use PBVH only for brushes and
		 * final DM to give final result to user.
		 */
		deformed |= object->sculpt->kb && (object->shapeflag & OB_SHAPE_LOCK) == 0;
	}

	return deformed;
}

static bool can_pbvh_draw(Object *ob, DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	Mesh *me = ob->data;
	bool deformed = check_sculpt_object_deformed(ob, false);

	if (deformed) {
		return false;
	}

	return cddm->mvert == me->mvert || ob->sculpt->kb;
}

static PBVH *cdDM_getPBVH(Object *ob, DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;

	if (!ob) {
		cddm->pbvh = NULL;
		return NULL;
	}

	if (!ob->sculpt)
		return NULL;

	if (ob->sculpt->pbvh) {
		cddm->pbvh = ob->sculpt->pbvh;
		cddm->pbvh_draw = can_pbvh_draw(ob, dm);
	}

	/* Sculpting on a BMesh (dynamic-topology) gets a special PBVH */
	if (!cddm->pbvh && ob->sculpt->bm) {
		cddm->pbvh = BKE_pbvh_new();
		cddm->pbvh_draw = true;

		BKE_pbvh_build_bmesh(cddm->pbvh, ob->sculpt->bm,
		                     ob->sculpt->bm_smooth_shading,
		                     ob->sculpt->bm_log, ob->sculpt->cd_vert_node_offset,
		                     ob->sculpt->cd_face_node_offset);

		pbvh_show_diffuse_color_set(cddm->pbvh, ob->sculpt->show_diffuse_color);
	}
		

	/* always build pbvh from original mesh, and only use it for drawing if
	 * this derivedmesh is just original mesh. it's the multires subsurf dm
	 * that this is actually for, to support a pbvh on a modified mesh */
	if (!cddm->pbvh && ob->type == OB_MESH) {
		Mesh *me = ob->data;
		bool deformed;

		cddm->pbvh = BKE_pbvh_new();
		cddm->pbvh_draw = can_pbvh_draw(ob, dm);

		BKE_mesh_tessface_ensure(me);
		
		BKE_pbvh_build_mesh(cddm->pbvh, me->mface, me->mvert,
		                    me->totface, me->totvert, &me->vdata);

		pbvh_show_diffuse_color_set(cddm->pbvh, ob->sculpt->show_diffuse_color);

		deformed = check_sculpt_object_deformed(ob, true);

		if (deformed && ob->derivedDeform) {
			DerivedMesh *deformdm = ob->derivedDeform;
			float (*vertCos)[3];
			int totvert;

			totvert = deformdm->getNumVerts(deformdm);
			vertCos = MEM_mallocN(totvert * sizeof(float[3]), "cdDM_getPBVH vertCos");
			deformdm->getVertCos(deformdm, vertCos);
			BKE_pbvh_apply_vertCos(cddm->pbvh, vertCos);
			MEM_freeN(vertCos);
		}
	}

	return cddm->pbvh;
}

/* update vertex normals so that drawing smooth faces works during sculpt
 * TODO: proper fix is to support the pbvh in all drawing modes */
static void cdDM_update_normals_from_pbvh(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	float (*face_nors)[3];

	if (!cddm->pbvh || !cddm->pbvh_draw || !dm->numTessFaceData)
		return;

	face_nors = CustomData_get_layer(&dm->faceData, CD_NORMAL);

	BKE_pbvh_update(cddm->pbvh, PBVH_UpdateNormals, face_nors);
}

static void cdDM_drawVerts(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mv = cddm->mvert;
	int i;

	if (GPU_buffer_legacy(dm)) {
		glBegin(GL_POINTS);
		for (i = 0; i < dm->numVertData; i++, mv++)
			glVertex3fv(mv->co);
		glEnd();
	}
	else {  /* use OpenGL VBOs or Vertex Arrays instead for better, faster rendering */
		GPU_vertex_setup(dm);
		if (!GPU_buffer_legacy(dm)) {
			if (dm->drawObject->tot_triangle_point)
				glDrawArrays(GL_POINTS, 0, dm->drawObject->tot_triangle_point);
			else
				glDrawArrays(GL_POINTS, 0, dm->drawObject->tot_loose_point);
		}
		GPU_buffer_unbind();
	}
}

static void cdDM_drawUVEdges(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MFace *mf = cddm->mface;
	MTFace *tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
	int i;

	if (mf) {
		if (GPU_buffer_legacy(dm)) {
			glBegin(GL_LINES);
			for (i = 0; i < dm->numTessFaceData; i++, mf++, tf++) {
				if (!(mf->flag & ME_HIDE)) {
					glVertex2fv(tf->uv[0]);
					glVertex2fv(tf->uv[1]);

					glVertex2fv(tf->uv[1]);
					glVertex2fv(tf->uv[2]);

					if (!mf->v4) {
						glVertex2fv(tf->uv[2]);
						glVertex2fv(tf->uv[0]);
					}
					else {
						glVertex2fv(tf->uv[2]);
						glVertex2fv(tf->uv[3]);

						glVertex2fv(tf->uv[3]);
						glVertex2fv(tf->uv[0]);
					}
				}
			}
			glEnd();
		}
		else {
			int prevstart = 0;
			int prevdraw = 1;
			int draw = 1;
			int curpos = 0;

			GPU_uvedge_setup(dm);
			if (!GPU_buffer_legacy(dm)) {
				for (i = 0; i < dm->numTessFaceData; i++, mf++) {
					if (!(mf->flag & ME_HIDE)) {
						draw = 1;
					}
					else {
						draw = 0;
					}
					if (prevdraw != draw) {
						if (prevdraw > 0 && (curpos - prevstart) > 0) {
							glDrawArrays(GL_LINES, prevstart, curpos - prevstart);
						}
						prevstart = curpos;
					}
					if (mf->v4) {
						curpos += 8;
					}
					else {
						curpos += 6;
					}
					prevdraw = draw;
				}
				if (prevdraw > 0 && (curpos - prevstart) > 0) {
					glDrawArrays(GL_LINES, prevstart, curpos - prevstart);
				}
			}
			GPU_buffer_unbind();
		}
	}
}

static void cdDM_drawEdges(DerivedMesh *dm, bool drawLooseEdges, bool drawAllEdges)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mvert = cddm->mvert;
	MEdge *medge = cddm->medge;
	int i;

	if (cddm->pbvh && cddm->pbvh_draw &&
	    BKE_pbvh_type(cddm->pbvh) == PBVH_BMESH)
	{
		BKE_pbvh_draw(cddm->pbvh, NULL, NULL, NULL, true);

		return;
	}
	
	if (GPU_buffer_legacy(dm)) {
		DEBUG_VBO("Using legacy code. cdDM_drawEdges\n");
		glBegin(GL_LINES);
		for (i = 0; i < dm->numEdgeData; i++, medge++) {
			if ((drawAllEdges || (medge->flag & ME_EDGEDRAW)) &&
			    (drawLooseEdges || !(medge->flag & ME_LOOSEEDGE)))
			{
				glVertex3fv(mvert[medge->v1].co);
				glVertex3fv(mvert[medge->v2].co);
			}
		}
		glEnd();
	}
	else {  /* use OpenGL VBOs or Vertex Arrays instead for better, faster rendering */
		int prevstart = 0;
		int prevdraw = 1;
		bool draw = true;

		GPU_edge_setup(dm);
		if (!GPU_buffer_legacy(dm)) {
			for (i = 0; i < dm->numEdgeData; i++, medge++) {
				if ((drawAllEdges || (medge->flag & ME_EDGEDRAW)) &&
				    (drawLooseEdges || !(medge->flag & ME_LOOSEEDGE)))
				{
					draw = true;
				}
				else {
					draw = false;
				}
				if (prevdraw != draw) {
					if (prevdraw > 0 && (i - prevstart) > 0) {
						GPU_buffer_draw_elements(dm->drawObject->edges, GL_LINES, prevstart * 2, (i - prevstart) * 2);
					}
					prevstart = i;
				}
				prevdraw = draw;
			}
			if (prevdraw > 0 && (i - prevstart) > 0) {
				GPU_buffer_draw_elements(dm->drawObject->edges, GL_LINES, prevstart * 2, (i - prevstart) * 2);
			}
		}
		GPU_buffer_unbind();
	}
}

static void cdDM_drawLooseEdges(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mvert = cddm->mvert;
	MEdge *medge = cddm->medge;
	int i;

	if (GPU_buffer_legacy(dm)) {
		DEBUG_VBO("Using legacy code. cdDM_drawLooseEdges\n");
		glBegin(GL_LINES);
		for (i = 0; i < dm->numEdgeData; i++, medge++) {
			if (medge->flag & ME_LOOSEEDGE) {
				glVertex3fv(mvert[medge->v1].co);
				glVertex3fv(mvert[medge->v2].co);
			}
		}
		glEnd();
	}
	else {  /* use OpenGL VBOs or Vertex Arrays instead for better, faster rendering */
		int prevstart = 0;
		int prevdraw = 1;
		int draw = 1;

		GPU_edge_setup(dm);
		if (!GPU_buffer_legacy(dm)) {
			for (i = 0; i < dm->numEdgeData; i++, medge++) {
				if (medge->flag & ME_LOOSEEDGE) {
					draw = 1;
				}
				else {
					draw = 0;
				}
				if (prevdraw != draw) {
					if (prevdraw > 0 && (i - prevstart) > 0) {
						GPU_buffer_draw_elements(dm->drawObject->edges, GL_LINES, prevstart * 2, (i - prevstart) * 2);
					}
					prevstart = i;
				}
				prevdraw = draw;
			}
			if (prevdraw > 0 && (i - prevstart) > 0) {
				GPU_buffer_draw_elements(dm->drawObject->edges, GL_LINES, prevstart * 2, (i - prevstart) * 2);
			}
		}
		GPU_buffer_unbind();
	}
}

static void cdDM_drawFacesSolid(DerivedMesh *dm,
                                float (*partial_redraw_planes)[4],
                                bool UNUSED(fast), DMSetMaterial setMaterial)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mvert = cddm->mvert;
	MFace *mface = cddm->mface;
	const float *nors = dm->getTessFaceDataArray(dm, CD_NORMAL);
	short (*lnors)[4][3] = dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL);
	int a, glmode = -1, shademodel = -1, matnr = -1, drawCurrentMat = 1;

	if (cddm->pbvh && cddm->pbvh_draw) {
		if (dm->numTessFaceData) {
			float (*face_nors)[3] = CustomData_get_layer(&dm->faceData, CD_NORMAL);

			BKE_pbvh_draw(cddm->pbvh, partial_redraw_planes, face_nors,
			              setMaterial, false);
			glShadeModel(GL_FLAT);
		}

		return;
	}

	if (GPU_buffer_legacy(dm)) {
		DEBUG_VBO("Using legacy code. cdDM_drawFacesSolid\n");
		glBegin(glmode = GL_QUADS);
		for (a = 0; a < dm->numTessFaceData; a++, mface++) {
			int new_glmode, new_matnr, new_shademodel;

			new_glmode = mface->v4 ? GL_QUADS : GL_TRIANGLES;
			new_matnr = mface->mat_nr + 1;
			new_shademodel = (lnors || (mface->flag & ME_SMOOTH)) ? GL_SMOOTH : GL_FLAT;


			if ((new_glmode != glmode) || (new_shademodel != shademodel) ||
			    (setMaterial && (new_matnr != matnr)))
			{
				glEnd();

				if (setMaterial) {
					drawCurrentMat = setMaterial(matnr = new_matnr, NULL);
				}

				glShadeModel(shademodel = new_shademodel);
				glBegin(glmode = new_glmode);
			}
			
			if (drawCurrentMat) {
				if (lnors) {
					glNormal3sv((const GLshort *)lnors[0][0]);
					glVertex3fv(mvert[mface->v1].co);
					glNormal3sv((const GLshort *)lnors[0][1]);
					glVertex3fv(mvert[mface->v2].co);
					glNormal3sv((const GLshort *)lnors[0][2]);
					glVertex3fv(mvert[mface->v3].co);
					if (mface->v4) {
						glNormal3sv((const GLshort *)lnors[0][3]);
						glVertex3fv(mvert[mface->v4].co);
					}
				}
				else if (shademodel == GL_FLAT) {
					if (nors) {
						glNormal3fv(nors);
					}
					else {
						/* TODO make this better (cache facenormals as layer?) */
						float nor[3];
						if (mface->v4) {
							normal_quad_v3(nor, mvert[mface->v1].co, mvert[mface->v2].co, mvert[mface->v3].co, mvert[mface->v4].co);
						}
						else {
							normal_tri_v3(nor, mvert[mface->v1].co, mvert[mface->v2].co, mvert[mface->v3].co);
						}
						glNormal3fv(nor);
					}
					glVertex3fv(mvert[mface->v1].co);
					glVertex3fv(mvert[mface->v2].co);
					glVertex3fv(mvert[mface->v3].co);
					if (mface->v4) {
						glVertex3fv(mvert[mface->v4].co);
					}
				}
				else {  /* shademodel == GL_SMOOTH */
					glNormal3sv(mvert[mface->v1].no);
					glVertex3fv(mvert[mface->v1].co);
					glNormal3sv(mvert[mface->v2].no);
					glVertex3fv(mvert[mface->v2].co);
					glNormal3sv(mvert[mface->v3].no);
					glVertex3fv(mvert[mface->v3].co);
					if (mface->v4) {
						glNormal3sv(mvert[mface->v4].no);
						glVertex3fv(mvert[mface->v4].co);
					}
				}
			}

			if (nors)
				nors += 3;
			if (lnors)
				lnors++;
		}
		glEnd();
	}
	else {  /* use OpenGL VBOs or Vertex Arrays instead for better, faster rendering */
		GPU_vertex_setup(dm);
		GPU_normal_setup(dm);
		if (!GPU_buffer_legacy(dm)) {
			glShadeModel(GL_SMOOTH);
			for (a = 0; a < dm->drawObject->totmaterial; a++) {
				if (!setMaterial || setMaterial(dm->drawObject->materials[a].mat_nr + 1, NULL)) {
					glDrawArrays(GL_TRIANGLES, dm->drawObject->materials[a].start,
					             dm->drawObject->materials[a].totpoint);
				}
			}
		}
		GPU_buffer_unbind();
	}

	glShadeModel(GL_FLAT);
}

static void cdDM_drawFacesTex_common(DerivedMesh *dm,
                                     DMSetDrawOptionsTex drawParams,
                                     DMSetDrawOptions drawParamsMapped,
                                     DMCompareDrawOptions compareDrawOptions,
                                     void *userData, DMDrawFlag uvflag)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mv = cddm->mvert;
	MFace *mf = DM_get_tessface_data_layer(dm, CD_MFACE);
	const float *nors = dm->getTessFaceDataArray(dm, CD_NORMAL);
	short (*lnors)[4][3] = dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL);
	MTFace *tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
	MCol *mcol;
	int i, orig;
	int colType, startFace = 0;
	bool use_tface = (uvflag & DM_DRAW_USE_ACTIVE_UV) != 0;

	/* double lookup */
	const int *index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	const int *index_mp_to_orig  = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}

	/* TODO: not entirely correct, but currently dynamic topology will
	 *       destroy UVs anyway, so textured display wouldn't work anyway
	 *
	 *       this will do more like solid view with lights set up for
	 *       textured view, but object itself will be displayed gray
	 *       (the same as it'll display without UV maps in textured view)
	 */
	if (cddm->pbvh && cddm->pbvh_draw && BKE_pbvh_type(cddm->pbvh) == PBVH_BMESH) {
		if (dm->numTessFaceData) {
			GPU_set_tpage(NULL, false, false);
			BKE_pbvh_draw(cddm->pbvh, NULL, NULL, NULL, false);
		}

		return;
	}

	colType = CD_TEXTURE_MCOL;
	mcol = dm->getTessFaceDataArray(dm, colType);
	if (!mcol) {
		colType = CD_PREVIEW_MCOL;
		mcol = dm->getTessFaceDataArray(dm, colType);
	}
	if (!mcol) {
		colType = CD_MCOL;
		mcol = dm->getTessFaceDataArray(dm, colType);
	}

	cdDM_update_normals_from_pbvh(dm);

	if (GPU_buffer_legacy(dm)) {
		int mat_nr_cache = -1;
		MTFace *tf_base = DM_get_tessface_data_layer(dm, CD_MTFACE);
		MTFace *tf_stencil_base = NULL;
		MTFace *tf_stencil = NULL;

		if (uvflag & DM_DRAW_USE_TEXPAINT_UV) {
			int stencil = CustomData_get_stencil_layer(&dm->faceData, CD_MTFACE);
			tf_stencil_base = CustomData_get_layer_n(&dm->faceData, CD_MTFACE, stencil);
		}

		DEBUG_VBO("Using legacy code. cdDM_drawFacesTex_common\n");
		for (i = 0; i < dm->numTessFaceData; i++, mf++) {
			MVert *mvert;
			DMDrawOption draw_option;
			unsigned char *cp = NULL;

			if (uvflag & DM_DRAW_USE_TEXPAINT_UV) {
				if (mf->mat_nr != mat_nr_cache) {
					tf_base = DM_paint_uvlayer_active_get(dm, mf->mat_nr);

					mat_nr_cache = mf->mat_nr;
				}
			}

			tf = tf_base ? tf_base + i : NULL;
			tf_stencil = tf_stencil_base ? tf_stencil_base + i : NULL;

			if (drawParams) {
				draw_option = drawParams(use_tface ? tf : NULL, (mcol != NULL), mf->mat_nr);
			}
			else {
				if (index_mf_to_mpoly) {
					orig = DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, i);
					if (orig == ORIGINDEX_NONE) {
						/* XXX, this is not really correct
						 * it will draw the previous faces context for this one when we don't know its settings.
						 * but better then skipping it altogether. - campbell */
						draw_option = DM_DRAW_OPTION_NORMAL;
					}
					else if (drawParamsMapped) {
						draw_option = drawParamsMapped(userData, orig);
					}
					else {
						if (nors) {
							nors += 3;
						}
						continue;
					}
				}
				else if (drawParamsMapped) {
					draw_option = drawParamsMapped(userData, i);
				}
				else {
					if (nors) {
						nors += 3;
					}
					continue;
				}
			}
			
			if (draw_option != DM_DRAW_OPTION_SKIP) {
				if (draw_option != DM_DRAW_OPTION_NO_MCOL && mcol)
					cp = (unsigned char *) &mcol[i * 4];

				if (!(lnors || (mf->flag & ME_SMOOTH))) {
					if (nors) {
						glNormal3fv(nors);
					}
					else {
						float nor[3];
						if (mf->v4) {
							normal_quad_v3(nor, mv[mf->v1].co, mv[mf->v2].co, mv[mf->v3].co, mv[mf->v4].co);
						}
						else {
							normal_tri_v3(nor, mv[mf->v1].co, mv[mf->v2].co, mv[mf->v3].co);
						}
						glNormal3fv(nor);
					}
				}

				glBegin(mf->v4 ? GL_QUADS : GL_TRIANGLES);
				if (tf) glTexCoord2fv(tf->uv[0]);
				if (tf_stencil) glMultiTexCoord2fv(GL_TEXTURE2, tf->uv[0]);
				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				mvert = &mv[mf->v1];
				if (lnors) glNormal3sv((const GLshort *)lnors[0][0]);
				else if (mf->flag & ME_SMOOTH) glNormal3sv(mvert->no);
				glVertex3fv(mvert->co);

				if (tf) glTexCoord2fv(tf->uv[1]);
				if (tf_stencil) glMultiTexCoord2fv(GL_TEXTURE2, tf->uv[1]);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				mvert = &mv[mf->v2];
				if (lnors) glNormal3sv((const GLshort *)lnors[0][1]);
				else if (mf->flag & ME_SMOOTH) glNormal3sv(mvert->no);
				glVertex3fv(mvert->co);

				if (tf) glTexCoord2fv(tf->uv[2]);
				if (tf_stencil) glMultiTexCoord2fv(GL_TEXTURE2, tf->uv[2]);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				mvert = &mv[mf->v3];
				if (lnors) glNormal3sv((const GLshort *)lnors[0][2]);
				else if (mf->flag & ME_SMOOTH) glNormal3sv(mvert->no);
				glVertex3fv(mvert->co);

				if (mf->v4) {
					if (tf) glTexCoord2fv(tf->uv[3]);
					if (tf_stencil) glMultiTexCoord2fv(GL_TEXTURE2, tf->uv[3]);
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					mvert = &mv[mf->v4];
					if (lnors) glNormal3sv((const GLshort *)lnors[0][3]);
					else if (mf->flag & ME_SMOOTH) glNormal3sv(mvert->no);
					glVertex3fv(mvert->co);
				}
				glEnd();
			}
			
			if (nors)
				nors += 3;
			if (lnors)
				lnors++;
		}
	}
	else { /* use OpenGL VBOs or Vertex Arrays instead for better, faster rendering */
		GPU_vertex_setup(dm);
		GPU_normal_setup(dm);
		if (uvflag & DM_DRAW_USE_TEXPAINT_UV)
			GPU_texpaint_uv_setup(dm);
		else
			GPU_uv_setup(dm);
		if (mcol) {
			GPU_color_setup(dm, colType);
		}

		if (!GPU_buffer_legacy(dm)) {
			int tottri = dm->drawObject->tot_triangle_point / 3;
			int next_actualFace = dm->drawObject->triangle_to_mface[0];

			glShadeModel(GL_SMOOTH);
			/* lastFlag = 0; */ /* UNUSED */
			for (i = 0; i < tottri; i++) {
				int actualFace = next_actualFace;
				DMDrawOption draw_option = DM_DRAW_OPTION_NORMAL;
				int flush = 0;

				if (i != tottri - 1)
					next_actualFace = dm->drawObject->triangle_to_mface[i + 1];

				if (drawParams) {
					draw_option = drawParams(use_tface && tf ? &tf[actualFace] : NULL, (mcol != NULL), mf[actualFace].mat_nr);
				}
				else {
					if (index_mf_to_mpoly) {
						orig = DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, actualFace);
						if (orig == ORIGINDEX_NONE) {
							/* XXX, this is not really correct
							 * it will draw the previous faces context for this one when we don't know its settings.
							 * but better then skipping it altogether. - campbell */
							draw_option = DM_DRAW_OPTION_NORMAL;
						}
						else if (drawParamsMapped) {
							draw_option = drawParamsMapped(userData, orig);
						}
					}
					else if (drawParamsMapped) {
						draw_option = drawParamsMapped(userData, actualFace);
					}
				}

				/* flush buffer if current triangle isn't drawable or it's last triangle */
				flush = (draw_option == DM_DRAW_OPTION_SKIP) || (i == tottri - 1);

				if (!flush && compareDrawOptions) {
					/* also compare draw options and flush buffer if they're different
					 * need for face selection highlight in edit mode */
					flush |= compareDrawOptions(userData, actualFace, next_actualFace) == 0;
				}

				if (flush) {
					int first = startFace * 3;
					/* Add one to the length if we're drawing at the end of the array */
					int count = (i - startFace + (draw_option != DM_DRAW_OPTION_SKIP ? 1 : 0)) * 3;

					if (count) {
						if (mcol && draw_option != DM_DRAW_OPTION_NO_MCOL)
							GPU_color_switch(1);
						else
							GPU_color_switch(0);

						glDrawArrays(GL_TRIANGLES, first, count);
					}

					startFace = i + 1;
				}
			}
		}

		GPU_buffer_unbind();
		glShadeModel(GL_FLAT);
	}
}

static void cdDM_drawFacesTex(DerivedMesh *dm,
                              DMSetDrawOptionsTex setDrawOptions,
                              DMCompareDrawOptions compareDrawOptions,
                              void *userData, DMDrawFlag uvflag)
{
	cdDM_drawFacesTex_common(dm, setDrawOptions, NULL, compareDrawOptions, userData, uvflag);
}

static void cdDM_drawMappedFaces(DerivedMesh *dm,
                                 DMSetDrawOptions setDrawOptions,
                                 DMSetMaterial setMaterial,
                                 DMCompareDrawOptions compareDrawOptions,
                                 void *userData, DMDrawFlag flag)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mv = cddm->mvert;
	MFace *mf = cddm->mface;
	MCol *mcol;
	const float *nors = DM_get_tessface_data_layer(dm, CD_NORMAL);
	short (*lnors)[4][3] = dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL);
	int colType, useColors = flag & DM_DRAW_USE_COLORS;
	int i, orig;


	/* double lookup */
	const int *index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	const int *index_mp_to_orig  = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}


	colType = CD_ID_MCOL;
	mcol = DM_get_tessface_data_layer(dm, colType);
	if (!mcol) {
		colType = CD_PREVIEW_MCOL;
		mcol = DM_get_tessface_data_layer(dm, colType);
	}
	if (!mcol) {
		colType = CD_MCOL;
		mcol = DM_get_tessface_data_layer(dm, colType);
	}

	cdDM_update_normals_from_pbvh(dm);

	/* back-buffer always uses legacy since VBO's would need the
	 * color array temporarily overwritten for drawing, then reset. */
	if (GPU_buffer_legacy(dm) || G.f & G_BACKBUFSEL) {
		DEBUG_VBO("Using legacy code. cdDM_drawMappedFaces\n");
		for (i = 0; i < dm->numTessFaceData; i++, mf++) {
			int drawSmooth = ((flag & DM_DRAW_ALWAYS_SMOOTH) || lnors) ? 1 : (mf->flag & ME_SMOOTH);
			DMDrawOption draw_option = DM_DRAW_OPTION_NORMAL;

			orig = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, i) : i;
			
			if (orig == ORIGINDEX_NONE)
				draw_option = setMaterial(mf->mat_nr + 1, NULL);
			else if (setDrawOptions != NULL)
				draw_option = setDrawOptions(userData, orig);

			if (draw_option != DM_DRAW_OPTION_SKIP) {
				unsigned char *cp = NULL;

				if (draw_option == DM_DRAW_OPTION_STIPPLE) {
					glEnable(GL_POLYGON_STIPPLE);
					glPolygonStipple(stipple_quarttone);
				}

				if (useColors && mcol)
					cp = (unsigned char *)&mcol[i * 4];

				/* no need to set shading mode to flat because
				 *  normals are already used to change shading */
				glShadeModel(GL_SMOOTH);
				glBegin(mf->v4 ? GL_QUADS : GL_TRIANGLES);

				if (lnors) {
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glNormal3sv((const GLshort *)lnors[0][0]);
					glVertex3fv(mv[mf->v1].co);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glNormal3sv((const GLshort *)lnors[0][1]);
					glVertex3fv(mv[mf->v2].co);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3sv((const GLshort *)lnors[0][2]);
					glVertex3fv(mv[mf->v3].co);
					if (mf->v4) {
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glNormal3sv((const GLshort *)lnors[0][3]);
						glVertex3fv(mv[mf->v4].co);
					}
				}
				else if (!drawSmooth) {
					if (nors) {
						glNormal3fv(nors);
					}
					else {
						float nor[3];
						if (mf->v4) {
							normal_quad_v3(nor, mv[mf->v1].co, mv[mf->v2].co, mv[mf->v3].co, mv[mf->v4].co);
						}
						else {
							normal_tri_v3(nor, mv[mf->v1].co, mv[mf->v2].co, mv[mf->v3].co);
						}
						glNormal3fv(nor);
					}

					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glVertex3fv(mv[mf->v1].co);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glVertex3fv(mv[mf->v2].co);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glVertex3fv(mv[mf->v3].co);
					if (mf->v4) {
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(mv[mf->v4].co);
					}
				}
				else {
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glNormal3sv(mv[mf->v1].no);
					glVertex3fv(mv[mf->v1].co);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glNormal3sv(mv[mf->v2].no);
					glVertex3fv(mv[mf->v2].co);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3sv(mv[mf->v3].no);
					glVertex3fv(mv[mf->v3].co);
					if (mf->v4) {
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glNormal3sv(mv[mf->v4].no);
						glVertex3fv(mv[mf->v4].co);
					}
				}

				glEnd();

				if (draw_option == DM_DRAW_OPTION_STIPPLE)
					glDisable(GL_POLYGON_STIPPLE);
			}
			
			if (nors)
				nors += 3;
			if (lnors)
				lnors++;
		}
	}
	else { /* use OpenGL VBOs or Vertex Arrays instead for better, faster rendering */
		int prevstart = 0;
		GPU_vertex_setup(dm);
		GPU_normal_setup(dm);
		if (useColors && mcol) {
			GPU_color_setup(dm, colType);
		}
		if (!GPU_buffer_legacy(dm)) {
			int tottri = dm->drawObject->tot_triangle_point / 3;
			glShadeModel(GL_SMOOTH);
			
			if (tottri == 0) {
				/* avoid buffer problems in following code */
			}
			if (setDrawOptions == NULL) {
				/* just draw the entire face array */
				glDrawArrays(GL_TRIANGLES, 0, (tottri) * 3);
			}
			else {
				/* we need to check if the next material changes */
				int next_actualFace = dm->drawObject->triangle_to_mface[0];
				
				for (i = 0; i < tottri; i++) {
					//int actualFace = dm->drawObject->triangle_to_mface[i];
					int actualFace = next_actualFace;
					MFace *mface = mf + actualFace;
					/*int drawSmooth = (flag & DM_DRAW_ALWAYS_SMOOTH) ? 1 : (mface->flag & ME_SMOOTH);*/ /* UNUSED */
					DMDrawOption draw_option = DM_DRAW_OPTION_NORMAL;
					int flush = 0;

					if (i != tottri - 1)
						next_actualFace = dm->drawObject->triangle_to_mface[i + 1];

					orig = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, actualFace) : actualFace;

					if (orig == ORIGINDEX_NONE)
						draw_option = setMaterial(mface->mat_nr + 1, NULL);
					else if (setDrawOptions != NULL)
						draw_option = setDrawOptions(userData, orig);

					if (draw_option == DM_DRAW_OPTION_STIPPLE) {
						glEnable(GL_POLYGON_STIPPLE);
						glPolygonStipple(stipple_quarttone);
					}
	
					/* Goal is to draw as long of a contiguous triangle
					 * array as possible, so draw when we hit either an
					 * invisible triangle or at the end of the array */

					/* flush buffer if current triangle isn't drawable or it's last triangle... */
					flush = (ELEM(draw_option, DM_DRAW_OPTION_SKIP, DM_DRAW_OPTION_STIPPLE)) || (i == tottri - 1);

					/* ... or when material setting is dissferent  */
					flush |= mf[actualFace].mat_nr != mf[next_actualFace].mat_nr;

					if (!flush && compareDrawOptions) {
						flush |= compareDrawOptions(userData, actualFace, next_actualFace) == 0;
					}

					if (flush) {
						int first = prevstart * 3;
						/* Add one to the length if we're drawing at the end of the array */
						int count = (i - prevstart + (draw_option != DM_DRAW_OPTION_SKIP ? 1 : 0)) * 3;

						if (count)
							glDrawArrays(GL_TRIANGLES, first, count);

						prevstart = i + 1;

						if (draw_option == DM_DRAW_OPTION_STIPPLE)
							glDisable(GL_POLYGON_STIPPLE);
					}
				}
			}

			glShadeModel(GL_FLAT);
		}
		GPU_buffer_unbind();
	}
}

static void cdDM_drawMappedFacesTex(DerivedMesh *dm,
                                    DMSetDrawOptions setDrawOptions,
                                    DMCompareDrawOptions compareDrawOptions,
                                    void *userData, DMDrawFlag flag)
{
	cdDM_drawFacesTex_common(dm, NULL, setDrawOptions, compareDrawOptions, userData, flag);
}

static void cddm_draw_attrib_vertex(DMVertexAttribs *attribs, MVert *mvert, int a, int index, int vert,
                                    short (*lnor)[3], int smoothnormal)
{
	const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	int b;

	/* orco texture coordinates */
	if (attribs->totorco) {
		/*const*/ float (*array)[3] = attribs->orco.array;
		const float *orco = (array) ? array[index] : zero;

		if (attribs->orco.gl_texco)
			glTexCoord3fv(orco);
		else
			glVertexAttrib3fvARB(attribs->orco.gl_index, orco);
	}

	/* uv texture coordinates */
	for (b = 0; b < attribs->tottface; b++) {
		const float *uv;

		if (attribs->tface[b].array) {
			MTFace *tf = &attribs->tface[b].array[a];
			uv = tf->uv[vert];
		}
		else {
			uv = zero;
		}

		if (attribs->tface[b].gl_texco)
			glTexCoord2fv(uv);
		else
			glVertexAttrib2fvARB(attribs->tface[b].gl_index, uv);
	}

	/* vertex colors */
	for (b = 0; b < attribs->totmcol; b++) {
		GLubyte col[4];

		if (attribs->mcol[b].array) {
			MCol *cp = &attribs->mcol[b].array[a * 4 + vert];
			col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
		}
		else {
			col[0] = 0; col[1] = 0; col[2] = 0; col[3] = 0;
		}

		glVertexAttrib4ubvARB(attribs->mcol[b].gl_index, col);
	}

	/* tangent for normal mapping */
	if (attribs->tottang) {
		/*const*/ float (*array)[4] = attribs->tang.array;
		const float *tang = (array) ? array[a * 4 + vert] : zero;
		glVertexAttrib4fvARB(attribs->tang.gl_index, tang);
	}

	/* vertex normal */
	if (lnor) {
		glNormal3sv((const GLshort *)lnor);
	}
	else if (smoothnormal) {
		glNormal3sv(mvert[index].no);
	}

	/* vertex coordinate */
	glVertex3fv(mvert[index].co);
}

static void cdDM_drawMappedFacesGLSL(DerivedMesh *dm,
                                     DMSetMaterial setMaterial,
                                     DMSetDrawOptions setDrawOptions,
                                     void *userData)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	GPUVertexAttribs gattribs;
	DMVertexAttribs attribs;
	MVert *mvert = cddm->mvert;
	MFace *mface = cddm->mface;
	/* MTFace *tf = dm->getTessFaceDataArray(dm, CD_MTFACE); */ /* UNUSED */
	float (*nors)[3] = dm->getTessFaceDataArray(dm, CD_NORMAL);
	short (*lnors)[4][3] = dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL);
	int a, b, matnr, new_matnr;
	bool do_draw;
	int orig;

	/* double lookup */
	const int *index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	const int *index_mp_to_orig  = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}

	/* TODO: same as for solid draw, not entirely correct, but works fine for now,
	 *       will skip using textures (dyntopo currently destroys UV anyway) and
	 *       works fine for matcap
	 */
	if (cddm->pbvh && cddm->pbvh_draw && BKE_pbvh_type(cddm->pbvh) == PBVH_BMESH) {
		if (dm->numTessFaceData) {
			setMaterial(1, &gattribs);
			BKE_pbvh_draw(cddm->pbvh, NULL, NULL, NULL, false);
		}

		return;
	}

	cdDM_update_normals_from_pbvh(dm);

	matnr = -1;
	do_draw = false;

	glShadeModel(GL_SMOOTH);

	if (GPU_buffer_legacy(dm) || setDrawOptions != NULL) {
		DEBUG_VBO("Using legacy code. cdDM_drawMappedFacesGLSL\n");
		memset(&attribs, 0, sizeof(attribs));

		glBegin(GL_QUADS);

		for (a = 0; a < dm->numTessFaceData; a++, mface++) {
			const int smoothnormal = lnors || (mface->flag & ME_SMOOTH);
			short (*ln1)[3] = NULL, (*ln2)[3] = NULL, (*ln3)[3] = NULL, (*ln4)[3] = NULL;
			new_matnr = mface->mat_nr + 1;

			if (new_matnr != matnr) {
				glEnd();

				do_draw = setMaterial(matnr = new_matnr, &gattribs);
				if (do_draw)
					DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);

				glBegin(GL_QUADS);
			}

			if (!do_draw) {
				continue;
			}
			else if (setDrawOptions) {
				orig = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a) : a;

				if (orig == ORIGINDEX_NONE) {
					/* since the material is set by setMaterial(), faces with no
					 * origin can be assumed to be generated by a modifier */ 
					
					/* continue */
				}
				else if (setDrawOptions(userData, orig) == DM_DRAW_OPTION_SKIP)
					continue;
			}

			if (!smoothnormal) {
				if (nors) {
					glNormal3fv(nors[a]);
				}
				else {
					/* TODO ideally a normal layer should always be available */
					float nor[3];
					if (mface->v4) {
						normal_quad_v3(nor, mvert[mface->v1].co, mvert[mface->v2].co, mvert[mface->v3].co, mvert[mface->v4].co);
					}
					else {
						normal_tri_v3(nor, mvert[mface->v1].co, mvert[mface->v2].co, mvert[mface->v3].co);
					}
					glNormal3fv(nor);
				}
			}
			else if (lnors) {
				ln1 = &lnors[a][0];
				ln2 = &lnors[a][1];
				ln3 = &lnors[a][2];
				ln4 = &lnors[a][3];
			}

			cddm_draw_attrib_vertex(&attribs, mvert, a, mface->v1, 0, ln1, smoothnormal);
			cddm_draw_attrib_vertex(&attribs, mvert, a, mface->v2, 1, ln2, smoothnormal);
			cddm_draw_attrib_vertex(&attribs, mvert, a, mface->v3, 2, ln3, smoothnormal);

			if (mface->v4)
				cddm_draw_attrib_vertex(&attribs, mvert, a, mface->v4, 3, ln4, smoothnormal);
			else
				cddm_draw_attrib_vertex(&attribs, mvert, a, mface->v3, 2, ln3, smoothnormal);
		}
		glEnd();
	}
	else {
		GPUBuffer *buffer = NULL;
		const char *varray = NULL;
		int numdata = 0, elementsize = 0, offset;
		int start = 0, numfaces = 0 /* , prevdraw = 0 */ /* UNUSED */, curface = 0;
		int i;

		MFace *mf = mface;
		GPUAttrib datatypes[GPU_MAX_ATTRIB]; /* TODO, messing up when switching materials many times - [#21056]*/
		memset(&attribs, 0, sizeof(attribs));

		GPU_vertex_setup(dm);
		GPU_normal_setup(dm);

		if (!GPU_buffer_legacy(dm)) {
			for (i = 0; i < dm->drawObject->tot_triangle_point / 3; i++) {

				a = dm->drawObject->triangle_to_mface[i];

				mface = mf + a;
				new_matnr = mface->mat_nr + 1;

				if (new_matnr != matnr) {
					numfaces = curface - start;
					if (numfaces > 0) {

						if (do_draw) {

							if (numdata != 0) {

								GPU_buffer_unlock(buffer);

								GPU_interleaved_attrib_setup(buffer, datatypes, numdata);
							}

							glDrawArrays(GL_TRIANGLES, start * 3, numfaces * 3);

							if (numdata != 0) {

								GPU_buffer_free(buffer);

								buffer = NULL;
							}

						}
					}
					numdata = 0;
					start = curface;
					/* prevdraw = do_draw; */ /* UNUSED */
					do_draw = setMaterial(matnr = new_matnr, &gattribs);
					if (do_draw) {
						DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);

						if (attribs.totorco && attribs.orco.array) {
							datatypes[numdata].index = attribs.orco.gl_index;
							datatypes[numdata].size = 3;
							datatypes[numdata].type = GL_FLOAT;
							numdata++;
						}
						for (b = 0; b < attribs.tottface; b++) {
							if (attribs.tface[b].array) {
								datatypes[numdata].index = attribs.tface[b].gl_index;
								datatypes[numdata].size = 2;
								datatypes[numdata].type = GL_FLOAT;
								numdata++;
							}
						}
						for (b = 0; b < attribs.totmcol; b++) {
							if (attribs.mcol[b].array) {
								datatypes[numdata].index = attribs.mcol[b].gl_index;
								datatypes[numdata].size = 4;
								datatypes[numdata].type = GL_UNSIGNED_BYTE;
								numdata++;
							}
						}
						if (attribs.tottang && attribs.tang.array) {
							datatypes[numdata].index = attribs.tang.gl_index;
							datatypes[numdata].size = 4;
							datatypes[numdata].type = GL_FLOAT;
							numdata++;
						}
						if (numdata != 0) {
							elementsize = GPU_attrib_element_size(datatypes, numdata);
							buffer = GPU_buffer_alloc(elementsize * dm->drawObject->tot_triangle_point);
							if (buffer == NULL) {
								GPU_buffer_unbind();
								dm->drawObject->legacy = 1;
								return;
							}
							varray = GPU_buffer_lock_stream(buffer);
							if (varray == NULL) {
								GPU_buffer_unbind();
								GPU_buffer_free(buffer);
								dm->drawObject->legacy = 1;
								return;
							}
						}
						else {
							/* if the buffer was set, don't use it again.
							 * prevdraw was assumed true but didnt run so set to false - [#21036] */
							/* prevdraw = 0; */ /* UNUSED */
							buffer = NULL;
						}
					}
				}

				if (do_draw && numdata != 0) {
					offset = 0;
					if (attribs.totorco && attribs.orco.array) {
						copy_v3_v3((float *)&varray[elementsize * curface * 3], (float *)attribs.orco.array[mface->v1]);
						copy_v3_v3((float *)&varray[elementsize * curface * 3 + elementsize], (float *)attribs.orco.array[mface->v2]);
						copy_v3_v3((float *)&varray[elementsize * curface * 3 + elementsize * 2], (float *)attribs.orco.array[mface->v3]);
						offset += sizeof(float) * 3;
					}
					for (b = 0; b < attribs.tottface; b++) {
						if (attribs.tface[b].array) {
							MTFace *tf = &attribs.tface[b].array[a];
							copy_v2_v2((float *)&varray[elementsize * curface * 3 + offset], tf->uv[0]);
							copy_v2_v2((float *)&varray[elementsize * curface * 3 + offset + elementsize], tf->uv[1]);

							copy_v2_v2((float *)&varray[elementsize * curface * 3 + offset + elementsize * 2], tf->uv[2]);
							offset += sizeof(float) * 2;
						}
					}
					for (b = 0; b < attribs.totmcol; b++) {
						if (attribs.mcol[b].array) {
							MCol *cp = &attribs.mcol[b].array[a * 4 + 0];
							GLubyte col[4];
							col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
							copy_v4_v4_char((char *)&varray[elementsize * curface * 3 + offset], (char *)col);
							cp = &attribs.mcol[b].array[a * 4 + 1];
							col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
							copy_v4_v4_char((char *)&varray[elementsize * curface * 3 + offset + elementsize], (char *)col);
							cp = &attribs.mcol[b].array[a * 4 + 2];
							col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
							copy_v4_v4_char((char *)&varray[elementsize * curface * 3 + offset + elementsize * 2], (char *)col);
							offset += sizeof(unsigned char) * 4;
						}
					}
					if (attribs.tottang && attribs.tang.array) {
						const float *tang = attribs.tang.array[a * 4 + 0];
						copy_v4_v4((float *)&varray[elementsize * curface * 3 + offset], tang);
						tang = attribs.tang.array[a * 4 + 1];
						copy_v4_v4((float *)&varray[elementsize * curface * 3 + offset + elementsize], tang);
						tang = attribs.tang.array[a * 4 + 2];
						copy_v4_v4((float *)&varray[elementsize * curface * 3 + offset + elementsize * 2], tang);
						offset += sizeof(float) * 4;
					}
					(void)offset;
				}
				curface++;
				if (mface->v4) {
					if (do_draw && numdata != 0) {
						offset = 0;
						if (attribs.totorco && attribs.orco.array) {
							copy_v3_v3((float *)&varray[elementsize * curface * 3], (float *)attribs.orco.array[mface->v3]);
							copy_v3_v3((float *)&varray[elementsize * curface * 3 + elementsize], (float *)attribs.orco.array[mface->v4]);
							copy_v3_v3((float *)&varray[elementsize * curface * 3 + elementsize * 2], (float *)attribs.orco.array[mface->v1]);
							offset += sizeof(float) * 3;
						}
						for (b = 0; b < attribs.tottface; b++) {
							if (attribs.tface[b].array) {
								MTFace *tf = &attribs.tface[b].array[a];
								copy_v2_v2((float *)&varray[elementsize * curface * 3 + offset], tf->uv[2]);
								copy_v2_v2((float *)&varray[elementsize * curface * 3 + offset + elementsize], tf->uv[3]);
								copy_v2_v2((float *)&varray[elementsize * curface * 3 + offset + elementsize * 2], tf->uv[0]);
								offset += sizeof(float) * 2;
							}
						}
						for (b = 0; b < attribs.totmcol; b++) {
							if (attribs.mcol[b].array) {
								MCol *cp = &attribs.mcol[b].array[a * 4 + 2];
								GLubyte col[4];
								col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
								copy_v4_v4_char((char *)&varray[elementsize * curface * 3 + offset], (char *)col);
								cp = &attribs.mcol[b].array[a * 4 + 3];
								col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
								copy_v4_v4_char((char *)&varray[elementsize * curface * 3 + offset + elementsize], (char *)col);
								cp = &attribs.mcol[b].array[a * 4 + 0];
								col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
								copy_v4_v4_char((char *)&varray[elementsize * curface * 3 + offset + elementsize * 2], (char *)col);
								offset += sizeof(unsigned char) * 4;
							}
						}
						if (attribs.tottang && attribs.tang.array) {
							const float *tang = attribs.tang.array[a * 4 + 2];
							copy_v4_v4((float *)&varray[elementsize * curface * 3 + offset], tang);
							tang = attribs.tang.array[a * 4 + 3];
							copy_v4_v4((float *)&varray[elementsize * curface * 3 + offset + elementsize], tang);
							tang = attribs.tang.array[a * 4 + 0];
							copy_v4_v4((float *)&varray[elementsize * curface * 3 + offset + elementsize * 2], tang);
							offset += sizeof(float) * 4;
						}
						(void)offset;
					}
					curface++;
					i++;
				}
			}
			numfaces = curface - start;
			if (numfaces > 0) {
				if (do_draw) {
					if (numdata != 0) {
						GPU_buffer_unlock(buffer);
						GPU_interleaved_attrib_setup(buffer, datatypes, numdata);
					}
					glDrawArrays(GL_TRIANGLES, start * 3, (curface - start) * 3);
				}
			}
			GPU_buffer_unbind();
		}
		GPU_buffer_free(buffer);
	}

	glShadeModel(GL_FLAT);
}

static void cdDM_drawFacesGLSL(DerivedMesh *dm, DMSetMaterial setMaterial)
{
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void cdDM_drawMappedFacesMat(DerivedMesh *dm,
                                    void (*setMaterial)(void *userData, int matnr, void *attribs),
                                    bool (*setFace)(void *userData, int index), void *userData)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	GPUVertexAttribs gattribs;
	DMVertexAttribs attribs;
	MVert *mvert = cddm->mvert;
	MFace *mf = cddm->mface;
	float (*nors)[3] = dm->getTessFaceDataArray(dm, CD_NORMAL);
	short (*lnors)[4][3] = dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL);
	int a, matnr, new_matnr;
	int orig;

	/* double lookup */
	const int *index_mf_to_mpoly = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	const int *index_mp_to_orig  = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	if (index_mf_to_mpoly == NULL) {
		index_mp_to_orig = NULL;
	}

	/* TODO: same as for solid draw, not entirely correct, but works fine for now,
	 *       will skip using textures (dyntopo currently destroys UV anyway) and
	 *       works fine for matcap
	 */
	if (cddm->pbvh && cddm->pbvh_draw && BKE_pbvh_type(cddm->pbvh) == PBVH_BMESH) {
		if (dm->numTessFaceData) {
			setMaterial(userData, 1, &gattribs);
			BKE_pbvh_draw(cddm->pbvh, NULL, NULL, NULL, false);
		}

		return;
	}

	cdDM_update_normals_from_pbvh(dm);

	matnr = -1;

	glShadeModel(GL_SMOOTH);

	memset(&attribs, 0, sizeof(attribs));

	glBegin(GL_QUADS);

	for (a = 0; a < dm->numTessFaceData; a++, mf++) {
		const int smoothnormal = lnors || (mf->flag & ME_SMOOTH);
		short (*ln1)[3] = NULL, (*ln2)[3] = NULL, (*ln3)[3] = NULL, (*ln4)[3] = NULL;

		/* material */
		new_matnr = mf->mat_nr + 1;

		if (new_matnr != matnr) {
			glEnd();

			setMaterial(userData, matnr = new_matnr, &gattribs);
			DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);

			glBegin(GL_QUADS);
		}

		/* skipping faces */
		if (setFace) {
			orig = (index_mf_to_mpoly) ? DM_origindex_mface_mpoly(index_mf_to_mpoly, index_mp_to_orig, a) : a;

			if (orig != ORIGINDEX_NONE && !setFace(userData, orig))
				continue;
		}

		/* smooth normal */
		if (!smoothnormal) {
			if (nors) {
				glNormal3fv(nors[a]);
			}
			else {
				/* TODO ideally a normal layer should always be available */
				float nor[3];

				if (mf->v4)
					normal_quad_v3(nor, mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co, mvert[mf->v4].co);
				else
					normal_tri_v3(nor, mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co);

				glNormal3fv(nor);
			}
		}
		else if (lnors) {
			ln1 = &lnors[a][0];
			ln2 = &lnors[a][1];
			ln3 = &lnors[a][2];
			ln4 = &lnors[a][3];
		}

		/* vertices */
		cddm_draw_attrib_vertex(&attribs, mvert, a, mf->v1, 0, ln1, smoothnormal);
		cddm_draw_attrib_vertex(&attribs, mvert, a, mf->v2, 1, ln2, smoothnormal);
		cddm_draw_attrib_vertex(&attribs, mvert, a, mf->v3, 2, ln3, smoothnormal);

		if (mf->v4)
			cddm_draw_attrib_vertex(&attribs, mvert, a, mf->v4, 3, ln4, smoothnormal);
		else
			cddm_draw_attrib_vertex(&attribs, mvert, a, mf->v3, 2, ln3, smoothnormal);
	}
	glEnd();

	glShadeModel(GL_FLAT);
}

static void cdDM_drawMappedEdges(DerivedMesh *dm, DMSetDrawOptions setDrawOptions, void *userData)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *vert = cddm->mvert;
	MEdge *edge = cddm->medge;
	int i, orig, *index = DM_get_edge_data_layer(dm, CD_ORIGINDEX);

	glBegin(GL_LINES);
	for (i = 0; i < dm->numEdgeData; i++, edge++) {
		if (index) {
			orig = *index++;
			if (setDrawOptions && orig == ORIGINDEX_NONE) continue;
		}
		else
			orig = i;

		if (!setDrawOptions || (setDrawOptions(userData, orig) != DM_DRAW_OPTION_SKIP)) {
			glVertex3fv(vert[edge->v1].co);
			glVertex3fv(vert[edge->v2].co);
		}
	}
	glEnd();
}

static void cdDM_foreachMappedVert(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float co[3], const float no_f[3], const short no_s[3]),
        void *userData,
        DMForeachFlag flag)
{
	MVert *mv = CDDM_get_verts(dm);
	const int *index = DM_get_vert_data_layer(dm, CD_ORIGINDEX);
	int i;

	if (index) {
		for (i = 0; i < dm->numVertData; i++, mv++) {
			const short *no = (flag & DM_FOREACH_USE_NORMAL) ? mv->no : NULL;
			const int orig = *index++;
			if (orig == ORIGINDEX_NONE) continue;
			func(userData, orig, mv->co, NULL, no);
		}
	}
	else {
		for (i = 0; i < dm->numVertData; i++, mv++) {
			const short *no = (flag & DM_FOREACH_USE_NORMAL) ? mv->no : NULL;
			func(userData, i, mv->co, NULL, no);
		}
	}
}

static void cdDM_foreachMappedEdge(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
        void *userData)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *) dm;
	MVert *mv = cddm->mvert;
	MEdge *med = cddm->medge;
	int i, orig, *index = DM_get_edge_data_layer(dm, CD_ORIGINDEX);

	for (i = 0; i < dm->numEdgeData; i++, med++) {
		if (index) {
			orig = *index++;
			if (orig == ORIGINDEX_NONE) continue;
			func(userData, orig, mv[med->v1].co, mv[med->v2].co);
		}
		else
			func(userData, i, mv[med->v1].co, mv[med->v2].co);
	}
}

static void cdDM_foreachMappedLoop(
        DerivedMesh *dm,
        void (*func)(void *userData, int vertex_index, int face_index, const float co[3], const float no[3]),
        void *userData,
        DMForeachFlag flag)
{
	/* We can't use dm->getLoopDataLayout(dm) here, we want to always access dm->loopData, EditDerivedBMesh would
	 * return loop data from bmesh itself. */
	const float (*lnors)[3] = (flag & DM_FOREACH_USE_NORMAL) ? DM_get_loop_data_layer(dm, CD_NORMAL) : NULL;

	const MVert *mv = CDDM_get_verts(dm);
	const MLoop *ml = CDDM_get_loops(dm);
	const MPoly *mp = CDDM_get_polys(dm);
	const int *v_index = DM_get_vert_data_layer(dm, CD_ORIGINDEX);
	const int *f_index = DM_get_poly_data_layer(dm, CD_ORIGINDEX);
	int p_idx, i;

	for (p_idx = 0; p_idx < dm->numPolyData; ++p_idx, ++mp) {
		for (i = 0; i < mp->totloop; ++i, ++ml) {
			const int v_idx = v_index ? v_index[ml->v] : ml->v;
			const int f_idx = f_index ? f_index[p_idx] : p_idx;
			const float *no = lnors ? *lnors++ : NULL;
			if (!ELEM(ORIGINDEX_NONE, v_idx, f_idx)) {
				func(userData, v_idx, f_idx, mv[ml->v].co, no);
			}
		}
	}
}

static void cdDM_foreachMappedFaceCenter(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float cent[3], const float no[3]),
        void *userData,
        DMForeachFlag flag)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	MVert *mvert = cddm->mvert;
	MPoly *mp;
	MLoop *ml;
	int i, orig, *index;

	index = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX);
	mp = cddm->mpoly;
	for (i = 0; i < dm->numPolyData; i++, mp++) {
		float cent[3];
		float *no, _no[3];

		if (index) {
			orig = *index++;
			if (orig == ORIGINDEX_NONE) continue;
		}
		else {
			orig = i;
		}
		
		ml = &cddm->mloop[mp->loopstart];
		BKE_mesh_calc_poly_center(mp, ml, mvert, cent);

		if (flag & DM_FOREACH_USE_NORMAL) {
			BKE_mesh_calc_poly_normal(mp, ml, mvert, (no = _no));
		}
		else {
			no = NULL;
		}

		func(userData, orig, cent, no);
	}

}

void CDDM_recalc_tessellation_ex(DerivedMesh *dm, const bool do_face_nor_cpy)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	dm->numTessFaceData = BKE_mesh_recalc_tessellation(&dm->faceData, &dm->loopData, &dm->polyData,
	                                                   cddm->mvert,
	                                                   dm->numTessFaceData, dm->numLoopData, dm->numPolyData,
	                                                   do_face_nor_cpy);

	cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);

	/* Tessellation recreated faceData, and the active layer indices need to get re-propagated
	 * from loops and polys to faces */
	CustomData_bmesh_update_active_layers(&dm->faceData, &dm->polyData, &dm->loopData);
}

void CDDM_recalc_tessellation(DerivedMesh *dm)
{
	CDDM_recalc_tessellation_ex(dm, true);
}

static void cdDM_free_internal(CDDerivedMesh *cddm)
{
	if (cddm->pmap) MEM_freeN(cddm->pmap);
	if (cddm->pmap_mem) MEM_freeN(cddm->pmap_mem);
}

static void cdDM_release(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	if (DM_release(dm)) {
		cdDM_free_internal(cddm);
		MEM_freeN(cddm);
	}
}

/**************** CDDM interface functions ****************/
static CDDerivedMesh *cdDM_create(const char *desc)
{
	CDDerivedMesh *cddm;
	DerivedMesh *dm;

	cddm = MEM_callocN(sizeof(*cddm), desc);
	dm = &cddm->dm;

	dm->getMinMax = cdDM_getMinMax;

	dm->getNumVerts = cdDM_getNumVerts;
	dm->getNumEdges = cdDM_getNumEdges;
	dm->getNumTessFaces = cdDM_getNumTessFaces;
	dm->getNumLoops = cdDM_getNumLoops;
	dm->getNumPolys = cdDM_getNumPolys;

	dm->getVert = cdDM_getVert;
	dm->getEdge = cdDM_getEdge;
	dm->getTessFace = cdDM_getTessFace;

	dm->copyVertArray = cdDM_copyVertArray;
	dm->copyEdgeArray = cdDM_copyEdgeArray;
	dm->copyTessFaceArray = cdDM_copyTessFaceArray;
	dm->copyLoopArray = cdDM_copyLoopArray;
	dm->copyPolyArray = cdDM_copyPolyArray;

	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getTessFaceData = DM_get_tessface_data;
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getTessFaceDataArray = DM_get_tessface_data_layer;

	dm->calcNormals = CDDM_calc_normals;
	dm->calcLoopNormals = CDDM_calc_loop_normals;
	dm->recalcTessellation = CDDM_recalc_tessellation;

	dm->getVertCos = cdDM_getVertCos;
	dm->getVertCo = cdDM_getVertCo;
	dm->getVertNo = cdDM_getVertNo;

	dm->getPBVH = cdDM_getPBVH;
	dm->getPolyMap = cdDM_getPolyMap;

	dm->drawVerts = cdDM_drawVerts;

	dm->drawUVEdges = cdDM_drawUVEdges;
	dm->drawEdges = cdDM_drawEdges;
	dm->drawLooseEdges = cdDM_drawLooseEdges;
	dm->drawMappedEdges = cdDM_drawMappedEdges;

	dm->drawFacesSolid = cdDM_drawFacesSolid;
	dm->drawFacesTex = cdDM_drawFacesTex;
	dm->drawFacesGLSL = cdDM_drawFacesGLSL;
	dm->drawMappedFaces = cdDM_drawMappedFaces;
	dm->drawMappedFacesTex = cdDM_drawMappedFacesTex;
	dm->drawMappedFacesGLSL = cdDM_drawMappedFacesGLSL;
	dm->drawMappedFacesMat = cdDM_drawMappedFacesMat;

	dm->foreachMappedVert = cdDM_foreachMappedVert;
	dm->foreachMappedEdge = cdDM_foreachMappedEdge;
	dm->foreachMappedLoop = cdDM_foreachMappedLoop;
	dm->foreachMappedFaceCenter = cdDM_foreachMappedFaceCenter;

	dm->release = cdDM_release;

	return cddm;
}

DerivedMesh *CDDM_new(int numVerts, int numEdges, int numTessFaces, int numLoops, int numPolys)
{
	CDDerivedMesh *cddm = cdDM_create("CDDM_new dm");
	DerivedMesh *dm = &cddm->dm;

	DM_init(dm, DM_TYPE_CDDM, numVerts, numEdges, numTessFaces, numLoops, numPolys);

	CustomData_add_layer(&dm->vertData, CD_ORIGINDEX, CD_CALLOC, NULL, numVerts);
	CustomData_add_layer(&dm->edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);
	CustomData_add_layer(&dm->faceData, CD_ORIGINDEX, CD_CALLOC, NULL, numTessFaces);
	CustomData_add_layer(&dm->polyData, CD_ORIGINDEX, CD_CALLOC, NULL, numPolys);

	CustomData_add_layer(&dm->vertData, CD_MVERT, CD_CALLOC, NULL, numVerts);
	CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_CALLOC, NULL, numEdges);
	CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL, numTessFaces);
	CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL, numLoops);
	CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL, numPolys);

	cddm->mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);
	cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);
	cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);
	cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
	cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

	return dm;
}

DerivedMesh *CDDM_from_mesh(Mesh *mesh)
{
	CDDerivedMesh *cddm = cdDM_create(__func__);
	DerivedMesh *dm = &cddm->dm;
	CustomDataMask mask = CD_MASK_MESH & (~CD_MASK_MDISPS);
	int alloctype;

	/* this does a referenced copy, with an exception for fluidsim */

	DM_init(dm, DM_TYPE_CDDM, mesh->totvert, mesh->totedge, mesh->totface,
	        mesh->totloop, mesh->totpoly);

	dm->deformedOnly = 1;
	dm->cd_flag = mesh->cd_flag;

	alloctype = CD_REFERENCE;

	CustomData_merge(&mesh->vdata, &dm->vertData, mask, alloctype,
	                 mesh->totvert);
	CustomData_merge(&mesh->edata, &dm->edgeData, mask, alloctype,
	                 mesh->totedge);
	CustomData_merge(&mesh->fdata, &dm->faceData, mask | CD_MASK_ORIGINDEX, alloctype,
	                 mesh->totface);
	CustomData_merge(&mesh->ldata, &dm->loopData, mask, alloctype,
	                 mesh->totloop);
	CustomData_merge(&mesh->pdata, &dm->polyData, mask, alloctype,
	                 mesh->totpoly);

	cddm->mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);
	cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);
	cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
	cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);
	cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);

	/* commented since even when CD_ORIGINDEX was first added this line fails
	 * on the default cube, (after editmode toggle too) - campbell */
#if 0
	BLI_assert(CustomData_has_layer(&cddm->dm.faceData, CD_ORIGINDEX));
#endif

	return dm;
}

DerivedMesh *CDDM_from_curve(Object *ob)
{
	ListBase disp = {NULL, NULL};

	if (ob->curve_cache) {
		disp = ob->curve_cache->disp;
	}

	return CDDM_from_curve_displist(ob, &disp);
}

DerivedMesh *CDDM_from_curve_displist(Object *ob, ListBase *dispbase)
{
	Curve *cu = (Curve *) ob->data;
	DerivedMesh *dm;
	CDDerivedMesh *cddm;
	MVert *allvert;
	MEdge *alledge;
	MLoop *allloop;
	MPoly *allpoly;
	MLoopUV *alluv = NULL;
	int totvert, totedge, totloop, totpoly;
	bool use_orco_uv = (cu->flag & CU_UV_ORCO) != 0;

	if (BKE_mesh_nurbs_displist_to_mdata(ob, dispbase, &allvert, &totvert, &alledge,
	                                     &totedge, &allloop, &allpoly, (use_orco_uv) ? &alluv : NULL,
	                                     &totloop, &totpoly) != 0)
	{
		/* Error initializing mdata. This often happens when curve is empty */
		return CDDM_new(0, 0, 0, 0, 0);
	}

	dm = CDDM_new(totvert, totedge, 0, totloop, totpoly);
	dm->deformedOnly = 1;
	dm->dirty |= DM_DIRTY_NORMALS;

	cddm = (CDDerivedMesh *)dm;

	memcpy(cddm->mvert, allvert, totvert * sizeof(MVert));
	memcpy(cddm->medge, alledge, totedge * sizeof(MEdge));
	memcpy(cddm->mloop, allloop, totloop * sizeof(MLoop));
	memcpy(cddm->mpoly, allpoly, totpoly * sizeof(MPoly));

	if (alluv) {
		const char *uvname = "Orco";
		CustomData_add_layer_named(&cddm->dm.polyData, CD_MTEXPOLY, CD_DEFAULT, NULL, totpoly, uvname);
		CustomData_add_layer_named(&cddm->dm.loopData, CD_MLOOPUV, CD_ASSIGN, alluv, totloop, uvname);
	}

	MEM_freeN(allvert);
	MEM_freeN(alledge);
	MEM_freeN(allloop);
	MEM_freeN(allpoly);

	return dm;
}

static void loops_to_customdata_corners(BMesh *bm, CustomData *facedata,
                                        int cdindex, const BMLoop *l3[3],
                                        int numCol, int numTex)
{
	const BMLoop *l;
	BMFace *f = l3[0]->f;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j, hasPCol = CustomData_has_layer(&bm->ldata, CD_PREVIEW_MLOOPCOL);

	for (i = 0; i < numTex; i++) {
		texface = CustomData_get_n(facedata, CD_MTFACE, cdindex, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->head.data, CD_MTEXPOLY, i);
		
		ME_MTEXFACE_CPY(texface, texpoly);
	
		for (j = 0; j < 3; j++) {
			l = l3[j];
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
			copy_v2_v2(texface->uv[j], mloopuv->uv);
		}
	}

	for (i = 0; i < numCol; i++) {
		mcol = CustomData_get_n(facedata, CD_MCOL, cdindex, i);
		
		for (j = 0; j < 3; j++) {
			l = l3[j];
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPCOL, i);
			MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
		}
	}

	if (hasPCol) {
		mcol = CustomData_get(facedata, cdindex, CD_PREVIEW_MCOL);

		for (j = 0; j < 3; j++) {
			l = l3[j];
			mloopcol = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_PREVIEW_MLOOPCOL);
			MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
		}
	}
}

/* used for both editbmesh and bmesh */
static DerivedMesh *cddm_from_bmesh_ex(struct BMesh *bm, const bool use_mdisps,
                                       /* EditBMesh vars for use_tessface */
                                       const bool use_tessface,
                                       const int em_tottri, const BMLoop *(*em_looptris)[3]
                                       )
{
	DerivedMesh *dm = CDDM_new(bm->totvert,
	                           bm->totedge,
	                           use_tessface ? em_tottri : 0,
	                           bm->totloop,
	                           bm->totface);

	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	BMIter iter;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	MVert *mvert = cddm->mvert;
	MEdge *medge = cddm->medge;
	MFace *mface = cddm->mface;
	MLoop *mloop = cddm->mloop;
	MPoly *mpoly = cddm->mpoly;
	int numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);
	int numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	int *index, add_orig;
	CustomDataMask mask;
	unsigned int i, j;
	
	const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
	const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
	const int cd_edge_crease_offset  = CustomData_get_offset(&bm->edata, CD_CREASE);
	
	dm->deformedOnly = 1;
	
	/* don't add origindex layer if one already exists */
	add_orig = !CustomData_has_layer(&bm->pdata, CD_ORIGINDEX);

	mask = use_mdisps ? CD_MASK_DERIVEDMESH | CD_MASK_MDISPS : CD_MASK_DERIVEDMESH;
	
	/* don't process shapekeys, we only feed them through the modifier stack as needed,
	 * e.g. for applying modifiers or the like*/
	mask &= ~CD_MASK_SHAPEKEY;
	CustomData_merge(&bm->vdata, &dm->vertData, mask,
	                 CD_CALLOC, dm->numVertData);
	CustomData_merge(&bm->edata, &dm->edgeData, mask,
	                 CD_CALLOC, dm->numEdgeData);
	CustomData_merge(&bm->ldata, &dm->loopData, mask,
	                 CD_CALLOC, dm->numLoopData);
	CustomData_merge(&bm->pdata, &dm->polyData, mask,
	                 CD_CALLOC, dm->numPolyData);

	/* add tessellation mface layers */
	if (use_tessface) {
		CustomData_from_bmeshpoly(&dm->faceData, &dm->polyData, &dm->loopData, em_tottri);
	}

	index = dm->getVertDataArray(dm, CD_ORIGINDEX);

	BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
		MVert *mv = &mvert[i];

		copy_v3_v3(mv->co, eve->co);

		BM_elem_index_set(eve, i); /* set_inline */

		normal_float_to_short_v3(mv->no, eve->no);

		mv->flag = BM_vert_flag_to_mflag(eve);

		if (cd_vert_bweight_offset != -1) mv->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eve, cd_vert_bweight_offset);

		if (add_orig) *index++ = i;

		CustomData_from_bmesh_block(&bm->vdata, &dm->vertData, eve->head.data, i);
	}
	bm->elem_index_dirty &= ~BM_VERT;

	index = dm->getEdgeDataArray(dm, CD_ORIGINDEX);
	BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
		MEdge *med = &medge[i];

		BM_elem_index_set(eed, i); /* set_inline */

		med->v1 = BM_elem_index_get(eed->v1);
		med->v2 = BM_elem_index_get(eed->v2);

		med->flag = BM_edge_flag_to_mflag(eed);

		/* handle this differently to editmode switching,
		 * only enable draw for single user edges rather then calculating angle */
		if ((med->flag & ME_EDGEDRAW) == 0) {
			if (eed->l && eed->l == eed->l->radial_next) {
				med->flag |= ME_EDGEDRAW;
			}
		}

		if (cd_edge_crease_offset  != -1) med->crease  = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eed, cd_edge_crease_offset);
		if (cd_edge_bweight_offset != -1) med->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eed, cd_edge_bweight_offset);

		CustomData_from_bmesh_block(&bm->edata, &dm->edgeData, eed->head.data, i);
		if (add_orig) *index++ = i;
	}
	bm->elem_index_dirty &= ~BM_EDGE;

	/* avoid this where possiblem, takes extra memory */
	if (use_tessface) {

		BM_mesh_elem_index_ensure(bm, BM_FACE);

		index = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
		for (i = 0; i < dm->numTessFaceData; i++) {
			MFace *mf = &mface[i];
			const BMLoop **l = em_looptris[i];
			efa = l[0]->f;

			mf->v1 = BM_elem_index_get(l[0]->v);
			mf->v2 = BM_elem_index_get(l[1]->v);
			mf->v3 = BM_elem_index_get(l[2]->v);
			mf->v4 = 0;
			mf->mat_nr = efa->mat_nr;
			mf->flag = BM_face_flag_to_mflag(efa);

			/* map mfaces to polygons in the same cddm intentionally */
			*index++ = BM_elem_index_get(efa);

			loops_to_customdata_corners(bm, &dm->faceData, i, l, numCol, numTex);
			test_index_face(mf, &dm->faceData, i, 3);
		}
	}
	
	index = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX);
	j = 0;
	BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
		BMLoop *l_iter;
		BMLoop *l_first;
		MPoly *mp = &mpoly[i];

		BM_elem_index_set(efa, i); /* set_inline */

		mp->totloop = efa->len;
		mp->flag = BM_face_flag_to_mflag(efa);
		mp->loopstart = j;
		mp->mat_nr = efa->mat_nr;

		l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
		do {
			mloop->v = BM_elem_index_get(l_iter->v);
			mloop->e = BM_elem_index_get(l_iter->e);
			CustomData_from_bmesh_block(&bm->ldata, &dm->loopData, l_iter->head.data, j);

			BM_elem_index_set(l_iter, j); /* set_inline */

			j++;
			mloop++;
		} while ((l_iter = l_iter->next) != l_first);

		CustomData_from_bmesh_block(&bm->pdata, &dm->polyData, efa->head.data, i);

		if (add_orig) *index++ = i;
	}
	bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);

	dm->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

	return dm;
}

struct DerivedMesh *CDDM_from_bmesh(struct BMesh *bm, const bool use_mdisps)
{
	return cddm_from_bmesh_ex(bm, use_mdisps, false,
	                          /* these vars are for editmesh only */
	                          0, NULL);
}

DerivedMesh *CDDM_from_editbmesh(BMEditMesh *em, const bool use_mdisps, const bool use_tessface)
{
	return cddm_from_bmesh_ex(em->bm, use_mdisps,
	                          /* editmesh */
	                          use_tessface, em->tottri, (const BMLoop *(*)[3])em->looptris);
}

static DerivedMesh *cddm_copy_ex(DerivedMesh *source, int faces_from_tessfaces)
{
	CDDerivedMesh *cddm = cdDM_create("CDDM_copy cddm");
	DerivedMesh *dm = &cddm->dm;
	int numVerts = source->numVertData;
	int numEdges = source->numEdgeData;
	int numTessFaces = source->numTessFaceData;
	int numLoops = source->numLoopData;
	int numPolys = source->numPolyData;

	/* ensure these are created if they are made on demand */
	source->getVertDataArray(source, CD_ORIGINDEX);
	source->getEdgeDataArray(source, CD_ORIGINDEX);
	source->getTessFaceDataArray(source, CD_ORIGINDEX);
	source->getPolyDataArray(source, CD_ORIGINDEX);

	/* this initializes dm, and copies all non mvert/medge/mface layers */
	DM_from_template(dm, source, DM_TYPE_CDDM, numVerts, numEdges, numTessFaces,
	                 numLoops, numPolys);
	dm->deformedOnly = source->deformedOnly;
	dm->cd_flag = source->cd_flag;
	dm->dirty = source->dirty;

	CustomData_copy_data(&source->vertData, &dm->vertData, 0, 0, numVerts);
	CustomData_copy_data(&source->edgeData, &dm->edgeData, 0, 0, numEdges);
	CustomData_copy_data(&source->faceData, &dm->faceData, 0, 0, numTessFaces);

	/* now add mvert/medge/mface layers */
	cddm->mvert = source->dupVertArray(source);
	cddm->medge = source->dupEdgeArray(source);
	cddm->mface = source->dupTessFaceArray(source);

	CustomData_add_layer(&dm->vertData, CD_MVERT, CD_ASSIGN, cddm->mvert, numVerts);
	CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_ASSIGN, cddm->medge, numEdges);
	CustomData_add_layer(&dm->faceData, CD_MFACE, CD_ASSIGN, cddm->mface, numTessFaces);
	
	if (!faces_from_tessfaces)
		DM_DupPolys(source, dm);
	else
		CDDM_tessfaces_to_faces(dm);

	cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
	cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

	return dm;
}

DerivedMesh *CDDM_copy(DerivedMesh *source)
{
	return cddm_copy_ex(source, 0);
}

DerivedMesh *CDDM_copy_from_tessface(DerivedMesh *source)
{
	return cddm_copy_ex(source, 1);
}

/* note, the CD_ORIGINDEX layers are all 0, so if there is a direct
 * relationship between mesh data this needs to be set by the caller. */
DerivedMesh *CDDM_from_template(DerivedMesh *source,
                                int numVerts, int numEdges, int numTessFaces,
                                int numLoops, int numPolys)
{
	CDDerivedMesh *cddm = cdDM_create("CDDM_from_template dest");
	DerivedMesh *dm = &cddm->dm;

	/* ensure these are created if they are made on demand */
	source->getVertDataArray(source, CD_ORIGINDEX);
	source->getEdgeDataArray(source, CD_ORIGINDEX);
	source->getTessFaceDataArray(source, CD_ORIGINDEX);
	source->getPolyDataArray(source, CD_ORIGINDEX);

	/* this does a copy of all non mvert/medge/mface layers */
	DM_from_template(dm, source, DM_TYPE_CDDM, numVerts, numEdges, numTessFaces, numLoops, numPolys);

	/* now add mvert/medge/mface layers */
	CustomData_add_layer(&dm->vertData, CD_MVERT, CD_CALLOC, NULL, numVerts);
	CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_CALLOC, NULL, numEdges);
	CustomData_add_layer(&dm->faceData, CD_MFACE, CD_CALLOC, NULL, numTessFaces);
	CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_CALLOC, NULL, numLoops);
	CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_CALLOC, NULL, numPolys);

	if (!CustomData_get_layer(&dm->vertData, CD_ORIGINDEX))
		CustomData_add_layer(&dm->vertData, CD_ORIGINDEX, CD_CALLOC, NULL, numVerts);
	if (!CustomData_get_layer(&dm->edgeData, CD_ORIGINDEX))
		CustomData_add_layer(&dm->edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);
	if (!CustomData_get_layer(&dm->faceData, CD_ORIGINDEX))
		CustomData_add_layer(&dm->faceData, CD_ORIGINDEX, CD_CALLOC, NULL, numTessFaces);

	cddm->mvert = CustomData_get_layer(&dm->vertData, CD_MVERT);
	cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);
	cddm->mface = CustomData_get_layer(&dm->faceData, CD_MFACE);
	cddm->mloop = CustomData_get_layer(&dm->loopData, CD_MLOOP);
	cddm->mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);

	return dm;
}

void CDDM_apply_vert_coords(DerivedMesh *dm, float (*vertCoords)[3])
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	MVert *vert;
	int i;

	/* this will just return the pointer if it wasn't a referenced layer */
	vert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);
	cddm->mvert = vert;

	for (i = 0; i < dm->numVertData; ++i, ++vert)
		copy_v3_v3(vert->co, vertCoords[i]);

	cddm->dm.dirty |= DM_DIRTY_NORMALS;
}

void CDDM_apply_vert_normals(DerivedMesh *dm, short (*vertNormals)[3])
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	MVert *vert;
	int i;

	/* this will just return the pointer if it wasn't a referenced layer */
	vert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);
	cddm->mvert = vert;

	for (i = 0; i < dm->numVertData; ++i, ++vert)
		copy_v3_v3_short(vert->no, vertNormals[i]);

	cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

void CDDM_calc_normals_mapping_ex(DerivedMesh *dm, const bool only_face_normals)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	float (*face_nors)[3] = NULL;

	if (dm->numVertData == 0) {
		cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
		return;
	}

	/* now we skip calculating vertex normals for referenced layer,
	 * no need to duplicate verts.
	 * WATCH THIS, bmesh only change!,
	 * need to take care of the side effects here - campbell */
#if 0
	/* we don't want to overwrite any referenced layers */
	cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);
#endif


	if (dm->numTessFaceData == 0) {
		/* No tessellation on this mesh yet, need to calculate one.
		 *
		 * Important not to update face normals from polys since it
		 * interferes with assigning the new normal layer in the following code.
		 */
		CDDM_recalc_tessellation_ex(dm, false);
	}
	else {
		/* A tessellation already exists, it should always have a CD_ORIGINDEX */
		BLI_assert(CustomData_has_layer(&dm->faceData, CD_ORIGINDEX));
		CustomData_free_layers(&dm->faceData, CD_NORMAL, dm->numTessFaceData);
	}

	face_nors = MEM_mallocN(sizeof(*face_nors) * dm->numTessFaceData, "face_nors");

	/* calculate face normals */
	BKE_mesh_calc_normals_mapping_ex(cddm->mvert, dm->numVertData, CDDM_get_loops(dm), CDDM_get_polys(dm),
	                                 dm->numLoopData, dm->numPolyData, NULL, cddm->mface, dm->numTessFaceData,
	                                 CustomData_get_layer(&dm->faceData, CD_ORIGINDEX), face_nors,
	                                 only_face_normals);

	CustomData_add_layer(&dm->faceData, CD_NORMAL, CD_ASSIGN, face_nors, dm->numTessFaceData);

	cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

void CDDM_calc_normals_mapping(DerivedMesh *dm)
{
	/* use this to skip calculating normals on original vert's, this may need to be changed */
	const bool only_face_normals = CustomData_is_referenced_layer(&dm->vertData, CD_MVERT);

	CDDM_calc_normals_mapping_ex(dm, only_face_normals);
}

#if 0
/* bmesh note: this matches what we have in trunk */
void CDDM_calc_normals(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	float (*poly_nors)[3];

	if (dm->numVertData == 0) return;

	/* we don't want to overwrite any referenced layers */
	cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);

	/* fill in if it exists */
	poly_nors = CustomData_get_layer(&dm->polyData, CD_NORMAL);
	if (!poly_nors) {
		poly_nors = CustomData_add_layer(&dm->polyData, CD_NORMAL, CD_CALLOC, NULL, dm->numPolyData);
	}

	BKE_mesh_calc_normals_poly(cddm->mvert, dm->numVertData, CDDM_get_loops(dm), CDDM_get_polys(dm),
	                               dm->numLoopData, dm->numPolyData, poly_nors, false);

	cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}
#else

/* poly normal layer is now only for final display */
void CDDM_calc_normals(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	/* we don't want to overwrite any referenced layers */
	cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);

	BKE_mesh_calc_normals_poly(cddm->mvert, dm->numVertData, CDDM_get_loops(dm), CDDM_get_polys(dm),
	                           dm->numLoopData, dm->numPolyData, NULL, false);

	cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

#endif

void CDDM_calc_loop_normals(DerivedMesh *dm, const float split_angle)
{
	MVert *mverts = dm->getVertArray(dm);
	MEdge *medges = dm->getEdgeArray(dm);
	MLoop *mloops = dm->getLoopArray(dm);
	MPoly *mpolys = dm->getPolyArray(dm);

	CustomData *ldata, *pdata;

	float (*lnors)[3];
	float (*pnors)[3];

	const int numVerts = dm->getNumVerts(dm);
	const int numEdges = dm->getNumEdges(dm);
	const int numLoops = dm->getNumLoops(dm);
	const int numPolys = dm->getNumPolys(dm);

	ldata = dm->getLoopDataLayout(dm);
	if (CustomData_has_layer(ldata, CD_NORMAL)) {
		lnors = CustomData_get_layer(ldata, CD_NORMAL);
	}
	else {
		lnors = CustomData_add_layer(ldata, CD_NORMAL, CD_CALLOC, NULL, numLoops);
	}

	/* Compute poly (always needed) and vert normals. */
	/* Note we can't use DM_ensure_normals, since it won't keep computed poly nors... */
	pdata = dm->getPolyDataLayout(dm);
	pnors = CustomData_get_layer(pdata, CD_NORMAL);
	if (!pnors) {
		pnors = CustomData_add_layer(pdata, CD_NORMAL, CD_CALLOC, NULL, numPolys);
	}
	BKE_mesh_calc_normals_poly(mverts, numVerts, mloops, mpolys, numLoops, numPolys, pnors,
	                           (dm->dirty & DM_DIRTY_NORMALS) ? false : true);

	dm->dirty &= ~DM_DIRTY_NORMALS;

	BKE_mesh_normals_loop_split(mverts, numVerts, medges, numEdges, mloops, lnors, numLoops,
	                            mpolys, pnors, numPolys, split_angle);
}


void CDDM_calc_normals_tessface(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	float (*face_nors)[3];

	if (dm->numVertData == 0) return;

	/* we don't want to overwrite any referenced layers */
	cddm->mvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, dm->numVertData);

	/* fill in if it exists */
	face_nors = CustomData_get_layer(&dm->faceData, CD_NORMAL);
	if (!face_nors) {
		face_nors = CustomData_add_layer(&dm->faceData, CD_NORMAL, CD_CALLOC, NULL, dm->numTessFaceData);
	}

	BKE_mesh_calc_normals_tessface(cddm->mvert, dm->numVertData,
	                               cddm->mface, dm->numTessFaceData, face_nors);

	cddm->dm.dirty &= ~DM_DIRTY_NORMALS;
}

#if 1

/**
 * Poly compare with vtargetmap
 * Function used by #CDDM_merge_verts.
 * The function compares poly_source after applying vtargetmap, with poly_target.
 * The two polys are identical if they share the same vertices in the same order, or in reverse order,
 * but starting position loopstart may be different.
 * The function is called with direct_reverse=1 for same order (i.e. same normal),
 * and may be called again with direct_reverse=-1 for reverse order.
 * \return 1 if polys are identical,  0 if polys are different.
 */
static int cddm_poly_compare(MLoop *mloop_array, MPoly *mpoly_source, MPoly *mpoly_target, const int *vtargetmap, const int direct_reverse)
{
	int vert_source, first_vert_source, vert_target;
	int i_loop_source;
	int i_loop_target, i_loop_target_start, i_loop_target_offset, i_loop_target_adjusted;
	bool compare_completed = false;
	bool same_loops = false;

	MLoop *mloop_source, *mloop_target;

	BLI_assert(direct_reverse == 1 || direct_reverse == -1);

	i_loop_source = 0;
	mloop_source = mloop_array + mpoly_source->loopstart;
	vert_source = mloop_source->v;

	if (vtargetmap[vert_source] != -1) {
		vert_source = vtargetmap[vert_source];
	}
	else {
		/* All source loop vertices should be mapped */
		BLI_assert(false);
	}

	/* Find same vertex within mpoly_target's loops */
	mloop_target = mloop_array + mpoly_target->loopstart;
	for (i_loop_target = 0; i_loop_target < mpoly_target->totloop; i_loop_target++, mloop_target++) {
		if (mloop_target->v == vert_source) {
			break;
		}
	}

	/* If same vertex not found, then polys cannot be equal */
	if (i_loop_target >= mpoly_target->totloop) {
		return false;
	}

	/* Now mloop_source and m_loop_target have one identical vertex */
	/* mloop_source is at position 0, while m_loop_target has advanced to find identical vertex */
	/* Go around the loop and check that all vertices match in same order */
	/* Skipping source loops when consecutive source vertices are mapped to same target vertex */

	i_loop_target_start = i_loop_target;
	i_loop_target_offset = 0;
	first_vert_source = vert_source;

	compare_completed = false;
	same_loops = false;

	while (!compare_completed) {

		vert_target = mloop_target->v;

		/* First advance i_loop_source, until it points to different vertex, after mapping applied */
		do {
			i_loop_source++;

			if (i_loop_source == mpoly_source->totloop) {
				/* End of loops for source, must match end of loop for target.  */
				if (i_loop_target_offset == mpoly_target->totloop - 1) {
					compare_completed = true;
					same_loops = true;
					break;  /* Polys are identical */
				}
				else {
					compare_completed = true;
					same_loops = false;
					break;  /* Polys are different */
				}
			}

			mloop_source++;
			vert_source = mloop_source->v;

			if (vtargetmap[vert_source] != -1) {
				vert_source = vtargetmap[vert_source];
			}
			else {
				/* All source loop vertices should be mapped */
				BLI_assert(false);
			}

		} while (vert_source == vert_target);

		if (compare_completed) {
			break;
		}

		/* Now advance i_loop_target as well */
		i_loop_target_offset++;

		if (i_loop_target_offset == mpoly_target->totloop) {
			/* End of loops for target only, that means no match */
			/* except if all remaining source vertices are mapped to first target */
			for (; i_loop_source < mpoly_source->totloop; i_loop_source++, mloop_source++) {
				vert_source = vtargetmap[mloop_source->v];
				if (vert_source != first_vert_source) {
					compare_completed = true;
					same_loops = false;
					break;
				}
			}
			if (!compare_completed) {
				same_loops = true;
			}
			break;
		}

		/* Adjust i_loop_target for cycling around and for direct/reverse order defined by delta = +1 or -1 */
		i_loop_target_adjusted = (i_loop_target_start + direct_reverse * i_loop_target_offset) % mpoly_target->totloop;
		if (i_loop_target_adjusted < 0) {
			i_loop_target_adjusted += mpoly_target->totloop;
		}
		mloop_target = mloop_array + mpoly_target->loopstart + i_loop_target_adjusted;
		vert_target = mloop_target->v;

		if (vert_target != vert_source) {
			same_loops = false;  /* Polys are different */
			break;
		}
	}
	return same_loops;
}

/* Utility stuff for using GHash with polys */

typedef struct PolyKey {
	int poly_index;   /* index of the MPoly within the derived mesh */
	int totloops;     /* number of loops in the poly */
	unsigned int hash_sum;  /* Sum of all vertices indices */
	unsigned int hash_xor;  /* Xor of all vertices indices */
} PolyKey;


static unsigned int poly_gset_hash_fn(const void *key)
{
	const PolyKey *pk = key;
	return pk->hash_sum;
}

static int poly_gset_compare_fn(const void *k1, const void *k2)
{
	const PolyKey *pk1 = k1;
	const PolyKey *pk2 = k2;
	if ((pk1->hash_sum == pk2->hash_sum) &&
	    (pk1->hash_xor == pk2->hash_xor) &&
	    (pk1->totloops == pk2->totloops))
	{
		/* Equality - note that this does not mean equality of polys */
		return 0;
	}
	else {
		return 1;
	}
}

/**
 * Merge Verts
 *
 * This frees dm, and returns a new one.
 *
 * \param vtargetmap  The table that maps vertices to target vertices.  a value of -1
 * indicates a vertex is a target, and is to be kept.
 * This array is aligned with 'dm->numVertData'
 *
 * \param tot_vtargetmap  The number of non '-1' values in vtargetmap. (not the size)
 *
 * \param merge_mode enum with two modes.
 * - #CDDM_MERGE_VERTS_DUMP_IF_MAPPED
 * When called by the Mirror Modifier,
 * In this mode it skips any faces that have all vertices merged (to avoid creating pairs
 * of faces sharing the same set of vertices)
 * - #CDDM_MERGE_VERTS_DUMP_IF_EQUAL
 * When called by the Array Modifier,
 * In this mode, faces where all vertices are merged are double-checked,
 * to see whether all target vertices actually make up a poly already.
 * Indeed it could be that all of a poly's vertices are merged,
 * but merged to vertices that do not make up a single poly,
 * in which case the original poly should not be dumped.
 * Actually this later behavior could apply to the Mirror Modifier as well, but the additional checks are
 * costly and not necessary in the case of mirror, because each vertex is only merged to its own mirror.
 *
 * \note #CDDM_recalc_tessellation has to run on the returned DM if you want to access tessfaces.
 */
DerivedMesh *CDDM_merge_verts(DerivedMesh *dm, const int *vtargetmap, const int tot_vtargetmap, const int merge_mode)
{
// #define USE_LOOPS
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	CDDerivedMesh *cddm2 = NULL;

	const int totvert = dm->numVertData;
	const int totedge = dm->numEdgeData;
	const int totloop = dm->numLoopData;
	const int totpoly = dm->numPolyData;

	const int totvert_final = totvert - tot_vtargetmap;

	MVert *mv, *mvert = MEM_mallocN(sizeof(*mvert) * totvert_final, __func__);
	int *oldv         = MEM_mallocN(sizeof(*oldv)  * totvert_final, __func__);
	int *newv         = MEM_mallocN(sizeof(*newv)  * totvert, __func__);
	STACK_DECLARE(mvert);
	STACK_DECLARE(oldv);

	MEdge *med, *medge = MEM_mallocN(sizeof(*medge) * totedge, __func__);
	int *olde          = MEM_mallocN(sizeof(*olde)  * totedge, __func__);
	int *newe          = MEM_mallocN(sizeof(*newe)  * totedge, __func__);
	STACK_DECLARE(medge);
	STACK_DECLARE(olde);

	MLoop *ml, *mloop = MEM_mallocN(sizeof(*mloop) * totloop, __func__);
	int *oldl         = MEM_mallocN(sizeof(*oldl)  * totloop, __func__);
#ifdef USE_LOOPS
	int newl          = MEM_mallocN(sizeof(*newl)  * totloop, __func__);
#endif
	STACK_DECLARE(mloop);
	STACK_DECLARE(oldl);

	MPoly *mp, *mpoly = MEM_mallocN(sizeof(*medge) * totpoly, __func__);
	int *oldp         = MEM_mallocN(sizeof(*oldp)  * totpoly, __func__);
	STACK_DECLARE(mpoly);
	STACK_DECLARE(oldp);

	EdgeHash *ehash = BLI_edgehash_new_ex(__func__, totedge);

	int i, j, c;

	PolyKey *poly_keys;
	GSet *poly_gset = NULL;

	STACK_INIT(oldv, totvert_final);
	STACK_INIT(olde, totedge);
	STACK_INIT(oldl, totloop);
	STACK_INIT(oldp, totpoly);

	STACK_INIT(mvert, totvert_final);
	STACK_INIT(medge, totedge);
	STACK_INIT(mloop, totloop);
	STACK_INIT(mpoly, totpoly);

	/* fill newl with destination vertex indices */
	mv = cddm->mvert;
	c = 0;
	for (i = 0; i < totvert; i++, mv++) {
		if (vtargetmap[i] == -1) {
			STACK_PUSH(oldv, i);
			STACK_PUSH(mvert, *mv);
			newv[i] = c++;
		}
		else {
			/* dummy value */
			newv[i] = 0;
		}
	}
	
	/* now link target vertices to destination indices */
	for (i = 0; i < totvert; i++) {
		if (vtargetmap[i] != -1) {
			newv[i] = newv[vtargetmap[i]];
		}
	}

	/* Don't remap vertices in cddm->mloop, because we need to know the original
	 * indices in order to skip faces with all vertices merged.
	 * The "update loop indices..." section further down remaps vertices in mloop.
	 */

	/* now go through and fix edges and faces */
	med = cddm->medge;
	c = 0;
	for (i = 0; i < totedge; i++, med++) {
		const unsigned int v1 = (vtargetmap[med->v1] != -1) ? vtargetmap[med->v1] : med->v1;
		const unsigned int v2 = (vtargetmap[med->v2] != -1) ? vtargetmap[med->v2] : med->v2;
		if (LIKELY(v1 != v2)) {
			void **eh_p = BLI_edgehash_lookup_p(ehash, v1, v2);

			if (eh_p) {
				newe[i] = GET_INT_FROM_POINTER(*eh_p);
			}
			else {
				STACK_PUSH(olde, i);
				STACK_PUSH(medge, *med);
				newe[i] = c;
				BLI_edgehash_insert(ehash, v1, v2, SET_INT_IN_POINTER(c));
				c++;
			}
		}
		else {
			newe[i] = -1;
		}
	}
	
	if (merge_mode == CDDM_MERGE_VERTS_DUMP_IF_EQUAL) {
		/* In this mode, we need to determine,  whenever a poly' vertices are all mapped */
		/* if the targets already make up a poly, in which case the new poly is dropped */
		/* This poly equality check is rather complex.   We use a BLI_ghash to speed it up with a first level check */
		PolyKey *mpgh;
		poly_keys = MEM_mallocN(sizeof(PolyKey) * totpoly, __func__);
		poly_gset = BLI_gset_new_ex(poly_gset_hash_fn, poly_gset_compare_fn, __func__, totpoly);
		/* Duplicates allowed because our compare function is not pure equality */
		BLI_gset_flag_set(poly_gset, GHASH_FLAG_ALLOW_DUPES);

		mp = cddm->mpoly;
		mpgh = poly_keys;
		for (i = 0; i < totpoly; i++, mp++, mpgh++) {
			mpgh->poly_index = i;
			mpgh->totloops = mp->totloop;
			ml = cddm->mloop + mp->loopstart;
			mpgh->hash_sum = mpgh->hash_xor = 0;
			for (j = 0; j < mp->totloop; j++, ml++) {
				mpgh->hash_sum += ml->v;
				mpgh->hash_xor ^= ml->v;
			}
			BLI_gset_insert(poly_gset, mpgh);
		}

		if (cddm->pmap) {
			MEM_freeN(cddm->pmap);
			MEM_freeN(cddm->pmap_mem);
		}
		/* Can we optimise by reusing an old pmap ?  How do we know an old pmap is stale ?  */
		/* When called by MOD_array.c, the cddm has just been created, so it has no valid pmap.   */
		BKE_mesh_vert_poly_map_create(&cddm->pmap, &cddm->pmap_mem,
		                              cddm->mpoly, cddm->mloop,
		                              totvert, totpoly, totloop);
	}  /* done preparing for fast poly compare */


	mp = cddm->mpoly;
	for (i = 0; i < totpoly; i++, mp++) {
		MPoly *mp_new;
		
		ml = cddm->mloop + mp->loopstart;

		/* check faces with all vertices merged */
		{
			bool all_vertices_merged = true;

			for (j = 0; j < mp->totloop; j++, ml++) {
				if (vtargetmap[ml->v] == -1) {
					all_vertices_merged = false;
					break;
				}
			}

			if (UNLIKELY(all_vertices_merged)) {
				if (merge_mode == CDDM_MERGE_VERTS_DUMP_IF_MAPPED) {
					/* In this mode, all vertices merged is enough to dump face */
					continue;
				}
				else if (merge_mode == CDDM_MERGE_VERTS_DUMP_IF_EQUAL) {
					/* Additional condition for face dump:  target vertices must make up an identical face */
					/* The test has 2 steps:  (1) first step is fast ghash lookup, but not failproof       */
					/*                        (2) second step is thorough but more costly poly compare     */
					int i_poly, v_target, v_prev;
					bool found = false;
					PolyKey pkey;

					/* Use poly_gset for fast (although not 100% certain) identification of same poly */
					/* First, make up a poly_summary structure */
					ml = cddm->mloop + mp->loopstart;
					pkey.hash_sum = pkey.hash_xor = 0;
					pkey.totloops = 0;
					v_prev = vtargetmap[(ml + mp->totloop -1)->v];  /* since it loops around, the prev of first is the last */
					for (j = 0; j < mp->totloop; j++, ml++) {
						v_target = vtargetmap[ml->v];   /* Cannot be -1, they are all mapped */
						if (v_target == v_prev) {
							/* consecutive vertices in loop map to the same target:  discard */
							/* but what about last to first ? */
							continue;
						}
						pkey.hash_sum += v_target;
						pkey.hash_xor ^= v_target;
						pkey.totloops++;
						v_prev = v_target;
					}
					if (BLI_gset_haskey(poly_gset, &pkey)) {

						/* There might be a poly that matches this one.
						 * We could just leave it there and say there is, and do a "continue".
						 * ... but we are checking whether there is an exact poly match.
						 * It's not so costly in terms of CPU since it's very rare, just a lot of complex code.
						 */

						/* Consider current loop again */
						ml = cddm->mloop + mp->loopstart;
						/* Consider the target of the loop's first vert */
						v_target = vtargetmap[ml->v];
						/* Now see if v_target belongs to a poly that shares all vertices with source poly,
						 * in same order, or reverse order */

						for (i_poly = 0; i_poly < cddm->pmap[v_target].count; i_poly++) {
							MPoly *target_poly = cddm->mpoly + *(cddm->pmap[v_target].indices + i_poly);

							if (cddm_poly_compare(cddm->mloop, mp, target_poly, vtargetmap, +1) ||
							    cddm_poly_compare(cddm->mloop, mp, target_poly, vtargetmap, -1))
							{
								found = true;
								break;
							}
						}
						if (found) {
							/* Current poly's vertices are mapped to a poly that is strictly identical */
							/* Current poly is dumped */
							continue;
						}
					}
				}
			}
		}


		/* Here either the poly's vertices were not all merged
		 * or they were all merged, but targets do not make up an identical poly,
		 * the poly is retained.
		 */
		ml = cddm->mloop + mp->loopstart;

		c = 0;
		for (j = 0; j < mp->totloop; j++, ml++) {
			unsigned int v1, v2;

			med = cddm->medge + ml->e;
			v1 = (vtargetmap[med->v1] != -1) ? vtargetmap[med->v1] : med->v1;
			v2 = (vtargetmap[med->v2] != -1) ? vtargetmap[med->v2] : med->v2;
			if (LIKELY(v1 != v2)) {
#ifdef USE_LOOPS
				newl[j + mp->loopstart] = STACK_SIZE(mloop);
#endif
				STACK_PUSH(oldl, j + mp->loopstart);
				STACK_PUSH(mloop, *ml);
				c++;
			}
		}

		if (UNLIKELY(c == 0)) {
			continue;
		}

		mp_new = STACK_PUSH_RET_PTR(mpoly);
		*mp_new = *mp;
		mp_new->totloop = c;
		mp_new->loopstart = STACK_SIZE(mloop) - c;
		
		STACK_PUSH(oldp, i);
	}  /* end of the loop that tests polys   */


	if (poly_gset) {
		// printf("hash quality %.6f\n", BLI_gset_calc_quality(poly_gset));

		BLI_gset_free(poly_gset, NULL);
		MEM_freeN(poly_keys);
	}
	
	/*create new cddm*/
	cddm2 = (CDDerivedMesh *) CDDM_from_template((DerivedMesh *)cddm, STACK_SIZE(mvert), STACK_SIZE(medge), 0, STACK_SIZE(mloop), STACK_SIZE(mpoly));
	
	/*update edge indices and copy customdata*/
	med = medge;
	for (i = 0; i < cddm2->dm.numEdgeData; i++, med++) {
		if (newv[med->v1] != -1)
			med->v1 = newv[med->v1];
		if (newv[med->v2] != -1)
			med->v2 = newv[med->v2];
		
		CustomData_copy_data(&dm->edgeData, &cddm2->dm.edgeData, olde[i], i, 1);
	}
	
	/*update loop indices and copy customdata*/
	ml = mloop;
	for (i = 0; i < cddm2->dm.numLoopData; i++, ml++) {
		if (newe[ml->e] != -1)
			ml->e = newe[ml->e];
		if (newv[ml->v] != -1)
			ml->v = newv[ml->v];
			
		CustomData_copy_data(&dm->loopData, &cddm2->dm.loopData, oldl[i], i, 1);
	}
	
	/*copy vertex customdata*/
	mv = mvert;
	for (i = 0; i < cddm2->dm.numVertData; i++, mv++) {
		CustomData_copy_data(&dm->vertData, &cddm2->dm.vertData, oldv[i], i, 1);
	}
	
	/*copy poly customdata*/
	mp = mpoly;
	for (i = 0; i < cddm2->dm.numPolyData; i++, mp++) {
		CustomData_copy_data(&dm->polyData, &cddm2->dm.polyData, oldp[i], i, 1);
	}
	
	/*copy over data.  CustomData_add_layer can do this, need to look it up.*/
	memcpy(cddm2->mvert, mvert, sizeof(MVert) * STACK_SIZE(mvert));
	memcpy(cddm2->medge, medge, sizeof(MEdge) * STACK_SIZE(medge));
	memcpy(cddm2->mloop, mloop, sizeof(MLoop) * STACK_SIZE(mloop));
	memcpy(cddm2->mpoly, mpoly, sizeof(MPoly) * STACK_SIZE(mpoly));

	MEM_freeN(mvert);
	MEM_freeN(medge);
	MEM_freeN(mloop);
	MEM_freeN(mpoly);

	MEM_freeN(newv);
	MEM_freeN(newe);
#ifdef USE_LOOPS
	MEM_freeN(newl);
#endif

	MEM_freeN(oldv);
	MEM_freeN(olde);
	MEM_freeN(oldl);
	MEM_freeN(oldp);;

	BLI_edgehash_free(ehash, NULL);

	/*free old derivedmesh*/
	dm->needsFree = 1;
	dm->release(dm);
	
	return (DerivedMesh *)cddm2;
}
#endif

void CDDM_calc_edges_tessface(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	CustomData edgeData;
	EdgeSetIterator *ehi;
	MFace *mf = cddm->mface;
	MEdge *med;
	EdgeSet *eh;
	int i, *index, numEdges, numFaces = dm->numTessFaceData;

	eh = BLI_edgeset_new_ex(__func__, BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(numFaces));

	for (i = 0; i < numFaces; i++, mf++) {
		BLI_edgeset_add(eh, mf->v1, mf->v2);
		BLI_edgeset_add(eh, mf->v2, mf->v3);
		
		if (mf->v4) {
			BLI_edgeset_add(eh, mf->v3, mf->v4);
			BLI_edgeset_add(eh, mf->v4, mf->v1);
		}
		else {
			BLI_edgeset_add(eh, mf->v3, mf->v1);
		}
	}

	numEdges = BLI_edgeset_size(eh);

	/* write new edges into a temporary CustomData */
	CustomData_reset(&edgeData);
	CustomData_add_layer(&edgeData, CD_MEDGE, CD_CALLOC, NULL, numEdges);
	CustomData_add_layer(&edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);

	med = CustomData_get_layer(&edgeData, CD_MEDGE);
	index = CustomData_get_layer(&edgeData, CD_ORIGINDEX);

	for (ehi = BLI_edgesetIterator_new(eh), i = 0;
	     BLI_edgesetIterator_isDone(ehi) == false;
	     BLI_edgesetIterator_step(ehi), i++, med++, index++)
	{
		BLI_edgesetIterator_getKey(ehi, &med->v1, &med->v2);

		med->flag = ME_EDGEDRAW | ME_EDGERENDER;
		*index = ORIGINDEX_NONE;
	}
	BLI_edgesetIterator_free(ehi);

	/* free old CustomData and assign new one */
	CustomData_free(&dm->edgeData, dm->numEdgeData);
	dm->edgeData = edgeData;
	dm->numEdgeData = numEdges;

	cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);

	BLI_edgeset_free(eh);
}

/* warning, this uses existing edges but CDDM_calc_edges_tessface() doesn't */
void CDDM_calc_edges(DerivedMesh *dm)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	CustomData edgeData;
	EdgeHashIterator *ehi;
	MPoly *mp = cddm->mpoly;
	MLoop *ml;
	MEdge *med, *origmed;
	EdgeHash *eh;
	unsigned int eh_reserve;
	int v1, v2;
	const int *eindex;
	int i, j, *index;
	const int numFaces = dm->numPolyData;
	const int numLoops = dm->numLoopData;
	int numEdges = dm->numEdgeData;

	eindex = DM_get_edge_data_layer(dm, CD_ORIGINDEX);
	med = cddm->medge;

	eh_reserve = max_ii(med ? numEdges : 0, BLI_EDGEHASH_SIZE_GUESS_FROM_LOOPS(numLoops));
	eh = BLI_edgehash_new_ex(__func__, eh_reserve);
	if (med) {
		for (i = 0; i < numEdges; i++, med++) {
			BLI_edgehash_insert(eh, med->v1, med->v2, SET_INT_IN_POINTER(i + 1));
		}
	}

	for (i = 0; i < numFaces; i++, mp++) {
		ml = cddm->mloop + mp->loopstart;
		for (j = 0; j < mp->totloop; j++, ml++) {
			v1 = ml->v;
			v2 = ME_POLY_LOOP_NEXT(cddm->mloop, mp, j)->v;
			BLI_edgehash_reinsert(eh, v1, v2, NULL);
		}
	}

	numEdges = BLI_edgehash_size(eh);

	/* write new edges into a temporary CustomData */
	CustomData_reset(&edgeData);
	CustomData_add_layer(&edgeData, CD_MEDGE, CD_CALLOC, NULL, numEdges);
	CustomData_add_layer(&edgeData, CD_ORIGINDEX, CD_CALLOC, NULL, numEdges);

	origmed = cddm->medge;
	med = CustomData_get_layer(&edgeData, CD_MEDGE);
	index = CustomData_get_layer(&edgeData, CD_ORIGINDEX);

	for (ehi = BLI_edgehashIterator_new(eh), i = 0;
	     BLI_edgehashIterator_isDone(ehi) == false;
	     BLI_edgehashIterator_step(ehi), ++i, ++med, ++index)
	{
		BLI_edgehashIterator_getKey(ehi, &med->v1, &med->v2);
		j = GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));

		if (j == 0) {
			med->flag = ME_EDGEDRAW | ME_EDGERENDER;
			*index = ORIGINDEX_NONE;
		}
		else {
			med->flag = ME_EDGEDRAW | ME_EDGERENDER | origmed[j - 1].flag;
			*index = eindex[j - 1];
		}

		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(i));
	}
	BLI_edgehashIterator_free(ehi);

	/* free old CustomData and assign new one */
	CustomData_free(&dm->edgeData, dm->numEdgeData);
	dm->edgeData = edgeData;
	dm->numEdgeData = numEdges;

	cddm->medge = CustomData_get_layer(&dm->edgeData, CD_MEDGE);

	mp = cddm->mpoly;
	for (i = 0; i < numFaces; i++, mp++) {
		ml = cddm->mloop + mp->loopstart;
		for (j = 0; j < mp->totloop; j++, ml++) {
			v1 = ml->v;
			v2 = ME_POLY_LOOP_NEXT(cddm->mloop, mp, j)->v;
			ml->e = GET_INT_FROM_POINTER(BLI_edgehash_lookup(eh, v1, v2));
		}
	}

	BLI_edgehash_free(eh, NULL);
}

void CDDM_lower_num_verts(DerivedMesh *dm, int numVerts)
{
	BLI_assert(numVerts >= 0);
	if (numVerts < dm->numVertData)
		CustomData_free_elem(&dm->vertData, numVerts, dm->numVertData - numVerts);

	dm->numVertData = numVerts;
}

void CDDM_lower_num_edges(DerivedMesh *dm, int numEdges)
{
	BLI_assert(numEdges >= 0);
	if (numEdges < dm->numEdgeData)
		CustomData_free_elem(&dm->edgeData, numEdges, dm->numEdgeData - numEdges);

	dm->numEdgeData = numEdges;
}

void CDDM_lower_num_tessfaces(DerivedMesh *dm, int numTessFaces)
{
	BLI_assert(numTessFaces >= 0);
	if (numTessFaces < dm->numTessFaceData)
		CustomData_free_elem(&dm->faceData, numTessFaces, dm->numTessFaceData - numTessFaces);

	dm->numTessFaceData = numTessFaces;
}

void CDDM_lower_num_loops(DerivedMesh *dm, int numLoops)
{
	BLI_assert(numLoops >= 0);
	if (numLoops < dm->numLoopData)
		CustomData_free_elem(&dm->loopData, numLoops, dm->numLoopData - numLoops);

	dm->numLoopData = numLoops;
}

void CDDM_lower_num_polys(DerivedMesh *dm, int numPolys)
{
	BLI_assert(numPolys >= 0);
	if (numPolys < dm->numPolyData)
		CustomData_free_elem(&dm->polyData, numPolys, dm->numPolyData - numPolys);

	dm->numPolyData = numPolys;
}

/* mesh element access functions */

MVert *CDDM_get_vert(DerivedMesh *dm, int index)
{
	return &((CDDerivedMesh *)dm)->mvert[index];
}

MEdge *CDDM_get_edge(DerivedMesh *dm, int index)
{
	return &((CDDerivedMesh *)dm)->medge[index];
}

MFace *CDDM_get_tessface(DerivedMesh *dm, int index)
{
	return &((CDDerivedMesh *)dm)->mface[index];
}

MLoop *CDDM_get_loop(DerivedMesh *dm, int index)
{
	return &((CDDerivedMesh *)dm)->mloop[index];
}

MPoly *CDDM_get_poly(DerivedMesh *dm, int index)
{
	return &((CDDerivedMesh *)dm)->mpoly[index];
}

/* array access functions */

MVert *CDDM_get_verts(DerivedMesh *dm)
{
	return ((CDDerivedMesh *)dm)->mvert;
}

MEdge *CDDM_get_edges(DerivedMesh *dm)
{
	return ((CDDerivedMesh *)dm)->medge;
}

MFace *CDDM_get_tessfaces(DerivedMesh *dm)
{
	return ((CDDerivedMesh *)dm)->mface;
}

MLoop *CDDM_get_loops(DerivedMesh *dm)
{
	return ((CDDerivedMesh *)dm)->mloop;
}

MPoly *CDDM_get_polys(DerivedMesh *dm)
{
	return ((CDDerivedMesh *)dm)->mpoly;
}

void CDDM_tessfaces_to_faces(DerivedMesh *dm)
{
	/* converts mfaces to mpolys/mloops */
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	BKE_mesh_convert_mfaces_to_mpolys_ex(NULL, &cddm->dm.faceData, &cddm->dm.loopData, &cddm->dm.polyData,
	                                     cddm->dm.numEdgeData, cddm->dm.numTessFaceData,
	                                     cddm->dm.numLoopData, cddm->dm.numPolyData,
	                                     cddm->medge, cddm->mface,
	                                     &cddm->dm.numLoopData, &cddm->dm.numPolyData,
	                                     &cddm->mloop, &cddm->mpoly);
}

void CDDM_set_mvert(DerivedMesh *dm, MVert *mvert)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;
	
	if (!CustomData_has_layer(&dm->vertData, CD_MVERT))
		CustomData_add_layer(&dm->vertData, CD_MVERT, CD_ASSIGN, mvert, dm->numVertData);
				
	cddm->mvert = mvert;
}

void CDDM_set_medge(DerivedMesh *dm, MEdge *medge)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	if (!CustomData_has_layer(&dm->edgeData, CD_MEDGE))
		CustomData_add_layer(&dm->edgeData, CD_MEDGE, CD_ASSIGN, medge, dm->numEdgeData);

	cddm->medge = medge;
}

void CDDM_set_mface(DerivedMesh *dm, MFace *mface)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	if (!CustomData_has_layer(&dm->faceData, CD_MFACE))
		CustomData_add_layer(&dm->faceData, CD_MFACE, CD_ASSIGN, mface, dm->numTessFaceData);

	cddm->mface = mface;
}

void CDDM_set_mloop(DerivedMesh *dm, MLoop *mloop)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	if (!CustomData_has_layer(&dm->loopData, CD_MLOOP))
		CustomData_add_layer(&dm->loopData, CD_MLOOP, CD_ASSIGN, mloop, dm->numLoopData);

	cddm->mloop = mloop;
}

void CDDM_set_mpoly(DerivedMesh *dm, MPoly *mpoly)
{
	CDDerivedMesh *cddm = (CDDerivedMesh *)dm;

	if (!CustomData_has_layer(&dm->polyData, CD_MPOLY))
		CustomData_add_layer(&dm->polyData, CD_MPOLY, CD_ASSIGN, mpoly, dm->numPolyData);

	cddm->mpoly = mpoly;
}
