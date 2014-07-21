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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/editderivedmesh.c
 *  \ingroup bke
 *
 * basic design:
 *
 * the bmesh derivedmesh exposes the mesh as triangles.  it stores pointers
 * to three loops per triangle.  the derivedmesh stores a cache of tessellations
 * for each face.  this cache will smartly update as needed (though at first
 * it'll simply be more brute force).  keeping track of face/edge counts may
 * be a small problem.
 *
 * this won't be the most efficient thing, considering that internal edges and
 * faces of tessellations are exposed.  looking up an edge by index in particular
 * is likely to be a little slow.
 */

#include "GL/glew.h"

#include "BLI_math.h"
#include "BLI_jitter.h"
#include "BLI_bitmap.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "GPU_extensions.h"

extern GLubyte stipple_quarttone[128]; /* glutil.c, bad level data */

static void bmdm_get_tri_colpreview(BMLoop *ls[3], MLoopCol *lcol[3], unsigned char(*color_vert_array)[4]);

typedef struct EditDerivedBMesh {
	DerivedMesh dm;

	BMEditMesh *em;

	/** when set, \a vertexNos, polyNos are lazy initialized */
	const float (*vertexCos)[3];

	/** lazy initialize (when \a vertexCos is set) */
	float const (*vertexNos)[3];
	float const (*polyNos)[3];
	/** also lazy init but dont depend on \a vertexCos */
	const float (*polyCos)[3];
} EditDerivedBMesh;

/* -------------------------------------------------------------------- */
/* Lazy initialize datastructures */

static void emDM_ensurePolyNormals(EditDerivedBMesh *bmdm);

static void emDM_ensureVertNormals(EditDerivedBMesh *bmdm)
{
	if (bmdm->vertexCos && (bmdm->vertexNos == NULL)) {

		BMesh *bm = bmdm->em->bm;
		const float (*vertexCos)[3], (*polyNos)[3];
		float (*vertexNos)[3];

		/* calculate vertex normals from poly normals */
		emDM_ensurePolyNormals(bmdm);

		BM_mesh_elem_index_ensure(bm, BM_FACE);

		polyNos = bmdm->polyNos;
		vertexCos = bmdm->vertexCos;
		vertexNos = MEM_callocN(sizeof(*vertexNos) * bm->totvert, __func__);

		BM_verts_calc_normal_vcos(bm, polyNos, vertexCos, vertexNos);

		bmdm->vertexNos = (const float (*)[3])vertexNos;
	}
}

static void emDM_ensurePolyNormals(EditDerivedBMesh *bmdm)
{
	if (bmdm->vertexCos && (bmdm->polyNos == NULL)) {
		BMesh *bm = bmdm->em->bm;
		const float (*vertexCos)[3];
		float (*polyNos)[3];

		BMFace *efa;
		BMIter fiter;
		int i;

		BM_mesh_elem_index_ensure(bm, BM_VERT);

		polyNos = MEM_mallocN(sizeof(*polyNos) * bm->totface, __func__);

		vertexCos = bmdm->vertexCos;

		BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
			BM_elem_index_set(efa, i); /* set_inline */
			BM_face_calc_normal_vcos(bm, efa, polyNos[i], vertexCos);
		}
		bm->elem_index_dirty &= ~BM_FACE;

		bmdm->polyNos = (const float (*)[3])polyNos;
	}
}

static void emDM_ensurePolyCenters(EditDerivedBMesh *bmdm)
{
	if (bmdm->polyCos == NULL) {
		BMesh *bm = bmdm->em->bm;
		float (*polyCos)[3];

		BMFace *efa;
		BMIter fiter;
		int i;

		polyCos = MEM_mallocN(sizeof(*polyCos) * bm->totface, __func__);

		if (bmdm->vertexCos) {
			const float (*vertexCos)[3];
			vertexCos = bmdm->vertexCos;

			BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
				BM_face_calc_center_mean_vcos(bm, efa, polyCos[i], vertexCos);
			}
		}
		else {
			BM_ITER_MESH_INDEX (efa, &fiter, bm, BM_FACES_OF_MESH, i) {
				BM_face_calc_center_mean(efa, polyCos[i]);
			}
		}

		bmdm->polyCos = (const float (*)[3])polyCos;
	}
}

static void emDM_calcNormals(DerivedMesh *dm)
{
	/* Nothing to do: normals are already calculated and stored on the
	 * BMVerts and BMFaces */
	dm->dirty &= ~DM_DIRTY_NORMALS;
}

static void emDM_calcLoopNormals(DerivedMesh *dm, const float split_angle)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	const float (*vertexCos)[3], (*vertexNos)[3], (*polyNos)[3];
	float (*loopNos)[3];

	/* calculate loop normals from poly and vertex normals */
	emDM_ensureVertNormals(bmdm);
	emDM_ensurePolyNormals(bmdm);
	dm->dirty &= ~DM_DIRTY_NORMALS;

	vertexCos = bmdm->vertexCos;
	vertexNos = bmdm->vertexNos;
	polyNos = bmdm->polyNos;

	loopNos = dm->getLoopDataArray(dm, CD_NORMAL);
	if (!loopNos) {
		DM_add_loop_layer(dm, CD_NORMAL, CD_CALLOC, NULL);
		loopNos = dm->getLoopDataArray(dm, CD_NORMAL);
	}

	BM_loops_calc_normal_vcos(bm, vertexCos, vertexNos, polyNos, split_angle, loopNos);
}

static void emDM_recalcTessellation(DerivedMesh *UNUSED(dm))
{
	/* do nothing */
}

static void emDM_foreachMappedVert(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float co[3], const float no_f[3], const short no_s[3]),
        void *userData,
        DMForeachFlag flag)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMVert *eve;
	BMIter iter;
	int i;

	if (bmdm->vertexCos) {
		const float (*vertexCos)[3] = bmdm->vertexCos;
		const float (*vertexNos)[3];

		if (flag & DM_FOREACH_USE_NORMAL) {
			emDM_ensureVertNormals(bmdm);
			vertexNos = bmdm->vertexNos;
		}
		else {
			vertexNos = NULL;
		}

		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			const float *no = (flag & DM_FOREACH_USE_NORMAL) ? vertexNos[i] : NULL;
			func(userData, i, vertexCos[i], no, NULL);
		}
	}
	else {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			const float *no = (flag & DM_FOREACH_USE_NORMAL) ? eve->no : NULL;
			func(userData, i, eve->co, no, NULL);
		}
	}
}
static void emDM_foreachMappedEdge(DerivedMesh *dm,
                                   void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
                                   void *userData)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMEdge *eed;
	BMIter iter;
	int i;

	if (bmdm->vertexCos) {

		BM_mesh_elem_index_ensure(bm, BM_VERT);

		BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
			func(userData, i,
			     bmdm->vertexCos[BM_elem_index_get(eed->v1)],
			     bmdm->vertexCos[BM_elem_index_get(eed->v2)]);
		}
	}
	else {
		BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
			func(userData, i, eed->v1->co, eed->v2->co);
		}
	}
}

static void emDM_drawMappedEdges(DerivedMesh *dm,
                                 DMSetDrawOptions setDrawOptions,
                                 void *userData)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMEdge *eed;
	BMIter iter;
	int i;

	if (bmdm->vertexCos) {

		BM_mesh_elem_index_ensure(bm, BM_VERT);

		glBegin(GL_LINES);
		BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
			if (!setDrawOptions || (setDrawOptions(userData, i) != DM_DRAW_OPTION_SKIP)) {
				glVertex3fv(bmdm->vertexCos[BM_elem_index_get(eed->v1)]);
				glVertex3fv(bmdm->vertexCos[BM_elem_index_get(eed->v2)]);
			}
		}
		glEnd();
	}
	else {
		glBegin(GL_LINES);
		BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
			if (!setDrawOptions || (setDrawOptions(userData, i) != DM_DRAW_OPTION_SKIP)) {
				glVertex3fv(eed->v1->co);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}
static void emDM_drawEdges(DerivedMesh *dm,
                           bool UNUSED(drawLooseEdges),
                           bool UNUSED(drawAllEdges))
{
	emDM_drawMappedEdges(dm, NULL, NULL);
}

static void emDM_drawMappedEdgesInterp(DerivedMesh *dm,
                                       DMSetDrawOptions setDrawOptions,
                                       DMSetDrawInterpOptions setDrawInterpOptions,
                                       void *userData)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMEdge *eed;
	BMIter iter;
	int i;

	if (bmdm->vertexCos) {

		BM_mesh_elem_index_ensure(bm, BM_VERT);

		glBegin(GL_LINES);
		BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
			if (!setDrawOptions || (setDrawOptions(userData, i) != DM_DRAW_OPTION_SKIP)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(bmdm->vertexCos[BM_elem_index_get(eed->v1)]);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(bmdm->vertexCos[BM_elem_index_get(eed->v2)]);
			}
		}
		glEnd();
	}
	else {
		glBegin(GL_LINES);
		BM_ITER_MESH_INDEX (eed, &iter, bm, BM_EDGES_OF_MESH, i) {
			if (!setDrawOptions || (setDrawOptions(userData, i) != DM_DRAW_OPTION_SKIP)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(eed->v1->co);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}

static void emDM_drawUVEdges(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMFace *efa;
	BMIter iter;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	if (UNLIKELY(cd_loop_uv_offset == -1)) {
		return;
	}

	glBegin(GL_LINES);
	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_iter, *l_first;
		const float *uv, *uv_prev;

		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
			continue;

		l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
		uv_prev = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter->prev, cd_loop_uv_offset))->uv;
		do {
			uv = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset))->uv;
			glVertex2fv(uv);
			glVertex2fv(uv_prev);
			uv_prev = uv;
		} while ((l_iter = l_iter->next) != l_first);
	}
	glEnd();
}

static void emDM_foreachMappedLoop(
        DerivedMesh *dm,
        void (*func)(void *userData, int vertex_index, int face_index, const float co[3], const float no[3]),
        void *userData,
        DMForeachFlag flag)
{
	/* We can't use dm->getLoopDataLayout(dm) here, we want to always access dm->loopData, EditDerivedBMesh would
	 * return loop data from bmesh itself. */
	const float (*lnors)[3] = (flag & DM_FOREACH_USE_NORMAL) ? DM_get_loop_data_layer(dm, CD_NORMAL) : NULL;

	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMFace *efa;
	BMIter iter;

	const float (*vertexCos)[3] = bmdm->vertexCos;
	int f_idx;

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, f_idx) {
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
		do {
			const BMVert *eve = l_iter->v;
			const int v_idx = BM_elem_index_get(eve);
			const float *no = lnors ? *lnors++ : NULL;
			func(userData, v_idx, f_idx, vertexCos ? vertexCos[v_idx] : eve->co, no);
		} while ((l_iter = l_iter->next) != l_first);
	}
}

static void emDM_foreachMappedFaceCenter(
        DerivedMesh *dm,
        void (*func)(void *userData, int index, const float co[3], const float no[3]),
        void *userData,
        DMForeachFlag flag)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	const float (*polyNos)[3];
	const float (*polyCos)[3];
	BMFace *efa;
	BMIter iter;
	int i;

	emDM_ensurePolyCenters(bmdm);
	polyCos = bmdm->polyCos;  /* always set */

	if (flag & DM_FOREACH_USE_NORMAL) {
		emDM_ensurePolyNormals(bmdm);
		polyNos = bmdm->polyNos;  /* maybe NULL */
	}
	else {
		polyNos = NULL;
	}

	if (polyNos) {
		BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
			const float *no = polyNos[i];
			func(userData, i, polyCos[i], no);
		}
	}
	else {
		BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
			const float *no = (flag & DM_FOREACH_USE_NORMAL) ? efa->no : NULL;
			func(userData, i, polyCos[i], no);
		}
	}
}

static void emDM_drawMappedFaces(DerivedMesh *dm,
                                 DMSetDrawOptions setDrawOptions,
                                 DMSetMaterial setMaterial,
                                 DMCompareDrawOptions compareDrawOptions,
                                 void *userData,
                                 DMDrawFlag flag)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMEditMesh *em = bmdm->em;
	BMesh *bm = em->bm;
	BMFace *efa;
	struct BMLoop *(*looptris)[3] = bmdm->em->looptris;
	const int tottri = bmdm->em->tottri;
	const int lasttri = tottri - 1; /* compare agasint this a lot */
	DMDrawOption draw_option;
	int i, flush;
	const int skip_normals = !glIsEnabled(GL_LIGHTING); /* could be passed as an arg */

	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
	MLoopCol *lcol[3] = {NULL} /* , dummylcol = {0} */;
	unsigned char(*color_vert_array)[4] = em->derivedVertColor;
	unsigned char(*color_face_array)[4] = em->derivedFaceColor;
	bool has_vcol_preview = (color_vert_array != NULL) && !skip_normals;
	bool has_fcol_preview = (color_face_array != NULL) && !skip_normals;
	bool has_vcol_any = has_vcol_preview;

	/* GL_ZERO is used to detect if drawing has started or not */
	GLenum poly_prev = GL_ZERO;
	GLenum shade_prev = GL_ZERO;

	(void)setMaterial; /* UNUSED */

	/* currently unused -- each original face is handled separately */
	(void)compareDrawOptions;

	/* call again below is ok */
	if (has_vcol_preview) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}
	if (has_fcol_preview) {
		BM_mesh_elem_index_ensure(bm, BM_FACE);
	}
	if (has_vcol_preview || has_fcol_preview) {
		flag |= DM_DRAW_ALWAYS_SMOOTH;
		glDisable(GL_LIGHTING);  /* grr */
	}

	if (bmdm->vertexCos) {
		/* add direct access */
		const float (*vertexCos)[3] = bmdm->vertexCos;
		const float (*vertexNos)[3];
		const float (*polyNos)[3];

		if (skip_normals) {
			vertexNos = NULL;
			polyNos = NULL;
		}
		else {
			emDM_ensureVertNormals(bmdm);
			emDM_ensurePolyNormals(bmdm);
			vertexNos = bmdm->vertexNos;
			polyNos = bmdm->polyNos;
		}

		BM_mesh_elem_index_ensure(bm, lnors ? BM_VERT | BM_FACE | BM_LOOP : BM_VERT | BM_FACE);

		for (i = 0; i < tottri; i++) {
			BMLoop **ltri = looptris[i];
			int drawSmooth;

			efa = ltri[0]->f;
			drawSmooth = lnors || ((flag & DM_DRAW_ALWAYS_SMOOTH) ? 1 : BM_elem_flag_test(efa, BM_ELEM_SMOOTH));

			draw_option = (!setDrawOptions ?
			               DM_DRAW_OPTION_NORMAL :
			               setDrawOptions(userData, BM_elem_index_get(efa)));
			if (draw_option != DM_DRAW_OPTION_SKIP) {
				const GLenum poly_type = GL_TRIANGLES; /* BMESH NOTE, this is odd but keep it for now to match trunk */
				if (draw_option == DM_DRAW_OPTION_STIPPLE) { /* enabled with stipple */

					if (poly_prev != GL_ZERO) glEnd();
					poly_prev = GL_ZERO; /* force glBegin */

					glEnable(GL_POLYGON_STIPPLE);
					glPolygonStipple(stipple_quarttone);
				}

				if      (has_vcol_preview) bmdm_get_tri_colpreview(ltri, lcol, color_vert_array);
				else if (has_fcol_preview) glColor3ubv((const GLubyte *)&(color_face_array[BM_elem_index_get(efa)]));
				if (skip_normals) {
					if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev = poly_type)); /* BMesh: will always be GL_TRIANGLES */
					}
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[0]->v)]);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[1]->v)]);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[2]->v)]);
				}
				else {
					const GLenum shade_type = drawSmooth ? GL_SMOOTH : GL_FLAT;
					if (shade_type != shade_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glShadeModel((shade_prev = shade_type)); /* same as below but switch shading */
						glBegin((poly_prev = poly_type)); /* BMesh: will always be GL_TRIANGLES */
					}
					if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev = poly_type)); /* BMesh: will always be GL_TRIANGLES */
					}

					if (!drawSmooth) {
						glNormal3fv(polyNos[BM_elem_index_get(efa)]);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[0]->v)]);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[1]->v)]);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[2]->v)]);
					}
					else {
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[0])]);
						else glNormal3fv(vertexNos[BM_elem_index_get(ltri[0]->v)]);
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[0]->v)]);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[1])]);
						else glNormal3fv(vertexNos[BM_elem_index_get(ltri[1]->v)]);
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[1]->v)]);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[2])]);
						else glNormal3fv(vertexNos[BM_elem_index_get(ltri[2]->v)]);
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[2]->v)]);
					}
				}

				flush = (draw_option == DM_DRAW_OPTION_STIPPLE);
				if (!skip_normals && !flush && (i != lasttri))
					flush |= efa->mat_nr != looptris[i + 1][0]->f->mat_nr;  /* TODO, make this neater */

				if (flush) {
					glEnd();
					poly_prev = GL_ZERO; /* force glBegin */

					glDisable(GL_POLYGON_STIPPLE);
				}
			}
		}
	}
	else {
		BM_mesh_elem_index_ensure(bm, lnors ? BM_FACE | BM_LOOP : BM_FACE);

		for (i = 0; i < tottri; i++) {
			BMLoop **ltri = looptris[i];
			int drawSmooth;

			efa = ltri[0]->f;
			drawSmooth = lnors || ((flag & DM_DRAW_ALWAYS_SMOOTH) ? 1 : BM_elem_flag_test(efa, BM_ELEM_SMOOTH));

			draw_option = (!setDrawOptions ?
			               DM_DRAW_OPTION_NORMAL :
			               setDrawOptions(userData, BM_elem_index_get(efa)));
			if (draw_option != DM_DRAW_OPTION_SKIP) {
				const GLenum poly_type = GL_TRIANGLES; /* BMESH NOTE, this is odd but keep it for now to match trunk */
				if (draw_option == DM_DRAW_OPTION_STIPPLE) { /* enabled with stipple */

					if (poly_prev != GL_ZERO) glEnd();
					poly_prev = GL_ZERO; /* force glBegin */

					glEnable(GL_POLYGON_STIPPLE);
					glPolygonStipple(stipple_quarttone);
				}

				if      (has_vcol_preview) bmdm_get_tri_colpreview(ltri, lcol, color_vert_array);
				else if (has_fcol_preview) glColor3ubv((const GLubyte *)&(color_face_array[BM_elem_index_get(efa)]));

				if (skip_normals) {
					if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev = poly_type)); /* BMesh: will always be GL_TRIANGLES */
					}
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
					glVertex3fv(ltri[0]->v->co);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
					glVertex3fv(ltri[1]->v->co);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
					glVertex3fv(ltri[2]->v->co);
				}
				else {
					const GLenum shade_type = drawSmooth ? GL_SMOOTH : GL_FLAT;
					if (shade_type != shade_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glShadeModel((shade_prev = shade_type)); /* same as below but switch shading */
						glBegin((poly_prev = poly_type)); /* BMesh: will always be GL_TRIANGLES */
					}
					if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev = poly_type)); /* BMesh: will always be GL_TRIANGLES */
					}

					if (!drawSmooth) {
						glNormal3fv(efa->no);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
						glVertex3fv(ltri[0]->v->co);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
						glVertex3fv(ltri[1]->v->co);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
						glVertex3fv(ltri[2]->v->co);
					}
					else {
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[0])]);
						else glNormal3fv(ltri[0]->v->no);
						glVertex3fv(ltri[0]->v->co);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[1])]);
						else glNormal3fv(ltri[1]->v->no);
						glVertex3fv(ltri[1]->v->co);
						if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[2])]);
						else glNormal3fv(ltri[2]->v->no);
						glVertex3fv(ltri[2]->v->co);
					}
				}

				flush = (draw_option == DM_DRAW_OPTION_STIPPLE);
				if (!skip_normals && !flush && (i != lasttri)) {
					flush |= efa->mat_nr != looptris[i + 1][0]->f->mat_nr; /* TODO, make this neater */
				}

				if (flush) {
					glEnd();
					poly_prev = GL_ZERO; /* force glBegin */

					glDisable(GL_POLYGON_STIPPLE);
				}
			}
		}
	}

	/* if non zero we know a face was rendered */
	if (poly_prev != GL_ZERO) glEnd();
}

static void bmdm_get_tri_uv(BMLoop *ltri[3], MLoopUV *luv[3], const int cd_loop_uv_offset)
{
	luv[0] = BM_ELEM_CD_GET_VOID_P(ltri[0], cd_loop_uv_offset);
	luv[1] = BM_ELEM_CD_GET_VOID_P(ltri[1], cd_loop_uv_offset);
	luv[2] = BM_ELEM_CD_GET_VOID_P(ltri[2], cd_loop_uv_offset);
}

static void bmdm_get_tri_col(BMLoop *ltri[3], MLoopCol *lcol[3], const int cd_loop_color_offset)
{
	lcol[0] = BM_ELEM_CD_GET_VOID_P(ltri[0], cd_loop_color_offset);
	lcol[1] = BM_ELEM_CD_GET_VOID_P(ltri[1], cd_loop_color_offset);
	lcol[2] = BM_ELEM_CD_GET_VOID_P(ltri[2], cd_loop_color_offset);
}

static void bmdm_get_tri_colpreview(BMLoop *ls[3], MLoopCol *lcol[3], unsigned char(*color_vert_array)[4])
{
	lcol[0] = (MLoopCol *)color_vert_array[BM_elem_index_get(ls[0]->v)];
	lcol[1] = (MLoopCol *)color_vert_array[BM_elem_index_get(ls[1]->v)];
	lcol[2] = (MLoopCol *)color_vert_array[BM_elem_index_get(ls[2]->v)];
}

static void emDM_drawFacesTex_common(DerivedMesh *dm,
                                     DMSetDrawOptionsTex drawParams,
                                     DMSetDrawOptions drawParamsMapped,
                                     DMCompareDrawOptions compareDrawOptions,
                                     void *userData)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMEditMesh *em = bmdm->em;
	BMesh *bm = em->bm;
	struct BMLoop *(*looptris)[3] = em->looptris;
	BMFace *efa;
	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
	MLoopUV *luv[3], dummyluv = {{0}};
	MLoopCol *lcol[3] = {NULL} /* , dummylcol = {0} */;
	const int cd_loop_uv_offset    = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_loop_color_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPCOL);
	const int cd_poly_tex_offset   = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);
	unsigned char(*color_vert_array)[4] = em->derivedVertColor;
	bool has_uv   = (cd_loop_uv_offset    != -1);
	bool has_vcol_preview = (color_vert_array != NULL);
	bool has_vcol = (cd_loop_color_offset != -1) && (has_vcol_preview == false);
	bool has_vcol_any = (has_vcol_preview || has_vcol);
	int i;

	(void) compareDrawOptions;

	luv[0] = luv[1] = luv[2] = &dummyluv;

	// dummylcol.r = dummylcol.g = dummylcol.b = dummylcol.a = 255;  /* UNUSED */

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	BM_mesh_elem_index_ensure(bm, BM_FACE);

	/* call again below is ok */
	if (has_vcol_preview) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}

	if (bmdm->vertexCos) {
		/* add direct access */
		const float (*vertexCos)[3] = bmdm->vertexCos;
		const float (*vertexNos)[3];
		const float (*polyNos)[3];

		emDM_ensureVertNormals(bmdm);
		emDM_ensurePolyNormals(bmdm);
		vertexNos = bmdm->vertexNos;
		polyNos = bmdm->polyNos;

		BM_mesh_elem_index_ensure(bm, lnors ? BM_LOOP | BM_VERT : BM_VERT);

		for (i = 0; i < em->tottri; i++) {
			BMLoop **ltri = looptris[i];
			MTexPoly *tp = (cd_poly_tex_offset != -1) ? BM_ELEM_CD_GET_VOID_P(ltri[0]->f, cd_poly_tex_offset) : NULL;
			MTFace mtf = {{{0}}};
			/*unsigned char *cp = NULL;*/ /*UNUSED*/
			int drawSmooth = lnors || BM_elem_flag_test(ltri[0]->f, BM_ELEM_SMOOTH);
			DMDrawOption draw_option;

			efa = ltri[0]->f;

			if (cd_poly_tex_offset != -1) {
				ME_MTEXFACE_CPY(&mtf, tp);
			}

			if (drawParams)
				draw_option = drawParams(&mtf, has_vcol, efa->mat_nr);
			else if (drawParamsMapped)
				draw_option = drawParamsMapped(userData, BM_elem_index_get(efa));
			else
				draw_option = DM_DRAW_OPTION_NORMAL;

			if (draw_option != DM_DRAW_OPTION_SKIP) {

				if      (has_uv)            bmdm_get_tri_uv(ltri,  luv,  cd_loop_uv_offset);
				if      (has_vcol)          bmdm_get_tri_col(ltri, lcol, cd_loop_color_offset);
				else if (has_vcol_preview)  bmdm_get_tri_colpreview(ltri, lcol, color_vert_array);

				glBegin(GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(polyNos[BM_elem_index_get(efa)]);

					glTexCoord2fv(luv[0]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[0]->v)]);

					glTexCoord2fv(luv[1]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[1]->v)]);

					glTexCoord2fv(luv[2]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[2]->v)]);
				}
				else {
					glTexCoord2fv(luv[0]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[0])]);
					else glNormal3fv(vertexNos[BM_elem_index_get(ltri[0]->v)]);
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[0]->v)]);

					glTexCoord2fv(luv[1]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[1])]);
					else glNormal3fv(vertexNos[BM_elem_index_get(ltri[1]->v)]);
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[1]->v)]);

					glTexCoord2fv(luv[2]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[2])]);
					else glNormal3fv(vertexNos[BM_elem_index_get(ltri[2]->v)]);
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[2]->v)]);
				}
				glEnd();
			}
		}
	}
	else {
		BM_mesh_elem_index_ensure(bm, lnors ? BM_LOOP | BM_VERT : BM_VERT);

		for (i = 0; i < em->tottri; i++) {
			BMLoop **ltri = looptris[i];
			MTexPoly *tp = (cd_poly_tex_offset != -1) ? BM_ELEM_CD_GET_VOID_P(ltri[0]->f, cd_poly_tex_offset) : NULL;
			MTFace mtf = {{{0}}};
			/*unsigned char *cp = NULL;*/ /*UNUSED*/
			int drawSmooth = lnors || BM_elem_flag_test(ltri[0]->f, BM_ELEM_SMOOTH);
			DMDrawOption draw_option;

			efa = ltri[0]->f;

			if (cd_poly_tex_offset != -1) {
				ME_MTEXFACE_CPY(&mtf, tp);
			}

			if (drawParams)
				draw_option = drawParams(&mtf, has_vcol, efa->mat_nr);
			else if (drawParamsMapped)
				draw_option = drawParamsMapped(userData, BM_elem_index_get(efa));
			else
				draw_option = DM_DRAW_OPTION_NORMAL;

			if (draw_option != DM_DRAW_OPTION_SKIP) {

				if      (has_uv)            bmdm_get_tri_uv(ltri,  luv,  cd_loop_uv_offset);
				if      (has_vcol)          bmdm_get_tri_col(ltri, lcol, cd_loop_color_offset);
				else if (has_vcol_preview)  bmdm_get_tri_colpreview(ltri, lcol, color_vert_array);

				glBegin(GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->no);

					glTexCoord2fv(luv[0]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
					glVertex3fv(ltri[0]->v->co);

					glTexCoord2fv(luv[1]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
					glVertex3fv(ltri[1]->v->co);

					glTexCoord2fv(luv[2]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
					glVertex3fv(ltri[2]->v->co);
				}
				else {
					glTexCoord2fv(luv[0]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[0]->r));
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[0])]);
					else glNormal3fv(ltri[0]->v->no);
					glVertex3fv(ltri[0]->v->co);

					glTexCoord2fv(luv[1]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[1]->r));
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[1])]);
					else glNormal3fv(ltri[1]->v->no);
					glVertex3fv(ltri[1]->v->co);

					glTexCoord2fv(luv[2]->uv);
					if (has_vcol_any) glColor3ubv((const GLubyte *)&(lcol[2]->r));
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[2])]);
					else glNormal3fv(ltri[2]->v->no);
					glVertex3fv(ltri[2]->v->co);
				}
				glEnd();
			}
		}
	}

	glShadeModel(GL_FLAT);
}

static void emDM_drawFacesTex(DerivedMesh *dm,
                              DMSetDrawOptionsTex setDrawOptions,
                              DMCompareDrawOptions compareDrawOptions,
                              void *userData, DMDrawFlag UNUSED(flag))
{
	emDM_drawFacesTex_common(dm, setDrawOptions, NULL, compareDrawOptions, userData);
}

static void emDM_drawMappedFacesTex(DerivedMesh *dm,
                                    DMSetDrawOptions setDrawOptions,
                                    DMCompareDrawOptions compareDrawOptions,
                                    void *userData, DMDrawFlag UNUSED(flag))
{
	emDM_drawFacesTex_common(dm, NULL, setDrawOptions, compareDrawOptions, userData);
}

/**
 * \note
 *
 * For UV's:
 *   const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, attribs->tface[i].em_offset);
 *
 * This is intentionally different to calling:
 *   CustomData_bmesh_get_n(&bm->ldata, loop->head.data, CD_MLOOPUV, i);
 *
 * ... because the material may use layer names to select different UV's
 * see: [#34378]
 */
static void emdm_pass_attrib_vertex_glsl(const DMVertexAttribs *attribs, const BMLoop *loop, const int index_in_face)
{
	BMVert *eve = loop->v;
	int i;
	const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	if (attribs->totorco) {
		int index = BM_elem_index_get(eve);
		const float *orco = (attribs->orco.array) ? attribs->orco.array[index] : zero;

		if (attribs->orco.gl_texco)
			glTexCoord3fv(orco);
		else
			glVertexAttrib3fvARB(attribs->orco.gl_index, orco);
	}
	for (i = 0; i < attribs->tottface; i++) {
		const float *uv;

		if (attribs->tface[i].em_offset != -1) {
			const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, attribs->tface[i].em_offset);
			uv = luv->uv;
		}
		else {
			uv = zero;
		}

		if (attribs->tface[i].gl_texco)
			glTexCoord2fv(uv);
		else
			glVertexAttrib2fvARB(attribs->tface[i].gl_index, uv);
	}
	for (i = 0; i < attribs->totmcol; i++) {
		GLubyte col[4];
		if (attribs->mcol[i].em_offset != -1) {
			const MLoopCol *cp = BM_ELEM_CD_GET_VOID_P(loop, attribs->mcol[i].em_offset);
			col[0] = cp->b; col[1] = cp->g; col[2] = cp->r; col[3] = cp->a;
		}
		else {
			col[0] = 0; col[1] = 0; col[2] = 0; col[3] = 0;
		}
		glVertexAttrib4ubvARB(attribs->mcol[i].gl_index, col);
	}
	if (attribs->tottang) {
		int index = i * 4 + index_in_face;
		const float *tang = (attribs->tang.array) ? attribs->tang.array[index] : zero;
		glVertexAttrib4fvARB(attribs->tang.gl_index, tang);
	}
}

static void emDM_drawMappedFacesGLSL(DerivedMesh *dm,
                                     DMSetMaterial setMaterial,
                                     DMSetDrawOptions setDrawOptions,
                                     void *userData)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMEditMesh *em = bmdm->em;
	BMesh *bm = em->bm;
	struct BMLoop *(*looptris)[3] = em->looptris;
	/* add direct access */
	const float (*vertexCos)[3] = bmdm->vertexCos;
	const float (*vertexNos)[3];
	const float (*polyNos)[3];
	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);

	BMFace *efa;
	DMVertexAttribs attribs;
	GPUVertexAttribs gattribs;

	int i, matnr, new_matnr, fi;
	bool do_draw;

	do_draw = false;
	matnr = -1;

	memset(&attribs, 0, sizeof(attribs));

	emDM_ensureVertNormals(bmdm);
	emDM_ensurePolyNormals(bmdm);
	vertexNos = bmdm->vertexNos;
	polyNos = bmdm->polyNos;

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);
	BM_mesh_elem_index_ensure(bm, lnors ? BM_VERT | BM_FACE | BM_LOOP : BM_VERT | BM_FACE);

	for (i = 0; i < em->tottri; i++) {
		BMLoop **ltri = looptris[i];
		int drawSmooth;

		efa = ltri[0]->f;

		if (setDrawOptions && (setDrawOptions(userData, BM_elem_index_get(efa)) == DM_DRAW_OPTION_SKIP))
			continue;

		new_matnr = efa->mat_nr + 1;
		if (new_matnr != matnr) {
			if (matnr != -1)
				glEnd();

			do_draw = setMaterial(matnr = new_matnr, &gattribs);
			if (do_draw)
				DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);

			glBegin(GL_TRIANGLES);
		}

		if (do_draw) {

			/* draw face */
			drawSmooth = lnors || BM_elem_flag_test(efa, BM_ELEM_SMOOTH);

			if (!drawSmooth) {
				if (vertexCos) {
					glNormal3fv(polyNos[BM_elem_index_get(efa)]);
					for (fi = 0; fi < 3; fi++) {
						emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
						glVertex3fv(vertexCos[BM_elem_index_get(ltri[fi]->v)]);
					}
				}
				else {
					glNormal3fv(efa->no);
					for (fi = 0; fi < 3; fi++) {
						emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
						glVertex3fv(ltri[fi]->v->co);
					}
				}
			}
			else {
				if (vertexCos) {
					for (fi = 0; fi < 3; fi++) {
						const int j = BM_elem_index_get(ltri[fi]->v);
						emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[fi])]);
						else glNormal3fv(vertexNos[j]);
						glVertex3fv(vertexCos[j]);
					}
				}
				else {
					for (fi = 0; fi < 3; fi++) {
						emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
						if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[fi])]);
						else glNormal3fv(ltri[fi]->v->no);
						glVertex3fv(ltri[fi]->v->co);
					}
				}
			}
		}
	}

	if (matnr != -1) {
		glEnd();
	}
}

static void emDM_drawFacesGLSL(DerivedMesh *dm,
                               int (*setMaterial)(int matnr, void *attribs))
{
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void emDM_drawMappedFacesMat(DerivedMesh *dm,
                                    void (*setMaterial)(void *userData, int matnr, void *attribs),
                                    bool (*setFace)(void *userData, int index), void *userData)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMEditMesh *em = bmdm->em;
	BMesh *bm = em->bm;
	struct BMLoop *(*looptris)[3] = em->looptris;
	const float (*vertexCos)[3] = bmdm->vertexCos;
	const float (*vertexNos)[3];
	const float (*polyNos)[3];
	const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);
	BMFace *efa;
	DMVertexAttribs attribs = {{{NULL}}};
	GPUVertexAttribs gattribs;
	int i, matnr, new_matnr, fi;

	matnr = -1;

	emDM_ensureVertNormals(bmdm);
	emDM_ensurePolyNormals(bmdm);

	vertexNos = bmdm->vertexNos;
	polyNos = bmdm->polyNos;

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	BM_mesh_elem_index_ensure(bm, lnors ? BM_VERT | BM_FACE | BM_LOOP : BM_VERT | BM_FACE);

	for (i = 0; i < em->tottri; i++) {
		BMLoop **ltri = looptris[i];
		int drawSmooth;

		efa = ltri[0]->f;

		/* face hiding */
		if (setFace && !setFace(userData, BM_elem_index_get(efa)))
			continue;

		/* material */
		new_matnr = efa->mat_nr + 1;
		if (new_matnr != matnr) {
			if (matnr != -1)
				glEnd();

			setMaterial(userData, matnr = new_matnr, &gattribs);
			DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);

			glBegin(GL_TRIANGLES);
		}

		/* draw face */
		drawSmooth = lnors || BM_elem_flag_test(efa, BM_ELEM_SMOOTH);

		if (!drawSmooth) {
			if (vertexCos) {
				glNormal3fv(polyNos[BM_elem_index_get(efa)]);
				for (fi = 0; fi < 3; fi++) {
					emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
					glVertex3fv(vertexCos[BM_elem_index_get(ltri[fi]->v)]);
				}
			}
			else {
				glNormal3fv(efa->no);
				for (fi = 0; fi < 3; fi++) {
					emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
					glVertex3fv(ltri[fi]->v->co);
				}
			}
		}
		else {
			if (vertexCos) {
				for (fi = 0; fi < 3; fi++) {
					const int j = BM_elem_index_get(ltri[fi]->v);
					emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[fi])]);
					else glNormal3fv(vertexNos[j]);
					glVertex3fv(vertexCos[j]);
				}
			}
			else {
				for (fi = 0; fi < 3; fi++) {
					emdm_pass_attrib_vertex_glsl(&attribs, ltri[fi], fi);
					if (lnors) glNormal3fv(lnors[BM_elem_index_get(ltri[fi])]);
					else glNormal3fv(ltri[fi]->v->no);
					glVertex3fv(ltri[fi]->v->co);
				}
			}
		}
	}

	if (matnr != -1) {
		glEnd();
	}
}

static void emDM_getMinMax(DerivedMesh *dm, float r_min[3], float r_max[3])
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMVert *eve;
	BMIter iter;
	int i;

	if (bm->totvert) {
		if (bmdm->vertexCos) {
			BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
				minmax_v3v3_v3(r_min, r_max, bmdm->vertexCos[i]);
			}
		}
		else {
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				minmax_v3v3_v3(r_min, r_max, eve->co);
			}
		}
	}
	else {
		zero_v3(r_min);
		zero_v3(r_max);
	}
}
static int emDM_getNumVerts(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return bmdm->em->bm->totvert;
}

static int emDM_getNumEdges(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return bmdm->em->bm->totedge;
}

static int emDM_getNumTessFaces(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return bmdm->em->tottri;
}

static int emDM_getNumLoops(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return bmdm->em->bm->totloop;
}

static int emDM_getNumPolys(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return bmdm->em->bm->totface;
}

static void bmvert_to_mvert(BMesh *bm, BMVert *ev, MVert *r_vert)
{
	const float *f;

	copy_v3_v3(r_vert->co, ev->co);

	normal_float_to_short_v3(r_vert->no, ev->no);

	r_vert->flag = BM_vert_flag_to_mflag(ev);

	if ((f = CustomData_bmesh_get(&bm->vdata, ev->head.data, CD_BWEIGHT))) {
		r_vert->bweight = (unsigned char)((*f) * 255.0f);
	}
}

static void emDM_getVert(DerivedMesh *dm, int index, MVert *r_vert)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMVert *ev;

	if (UNLIKELY(index < 0 || index >= bm->totvert)) {
		BLI_assert(!"error in emDM_getVert");
		return;
	}

	BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
	ev = bm->vtable[index];  /* should be BM_vert_at_index() */
	// ev = BM_vert_at_index(bm, index); /* warning, does list loop, _not_ ideal */

	bmvert_to_mvert(bm, ev, r_vert);
	if (bmdm->vertexCos)
		copy_v3_v3(r_vert->co, bmdm->vertexCos[index]);
}

static void emDM_getVertCo(DerivedMesh *dm, int index, float r_co[3])
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;

	if (UNLIKELY(index < 0 || index >= bm->totvert)) {
		BLI_assert(!"error in emDM_getVertCo");
		return;
	}

	if (bmdm->vertexCos) {
		copy_v3_v3(r_co, bmdm->vertexCos[index]);
	}
	else {
		BMVert *ev;

		BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
		ev = bm->vtable[index];  /* should be BM_vert_at_index() */
		// ev = BM_vert_at_index(bm, index); /* warning, does list loop, _not_ ideal */
		copy_v3_v3(r_co, ev->co);
	}
}

static void emDM_getVertNo(DerivedMesh *dm, int index, float r_no[3])
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;

	if (UNLIKELY(index < 0 || index >= bm->totvert)) {
		BLI_assert(!"error in emDM_getVertNo");
		return;
	}


	if (bmdm->vertexCos) {
		emDM_ensureVertNormals(bmdm);
		copy_v3_v3(r_no, bmdm->vertexNos[index]);
	}
	else {
		BMVert *ev;

		BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
		ev = bm->vtable[index];  /* should be BM_vert_at_index() */
		// ev = BM_vert_at_index(bm, index); /* warning, does list loop, _not_ ideal */
		copy_v3_v3(r_no, ev->no);
	}
}

static void emDM_getPolyNo(DerivedMesh *dm, int index, float r_no[3])
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;

	if (UNLIKELY(index < 0 || index >= bm->totface)) {
		BLI_assert(!"error in emDM_getPolyNo");
		return;
	}

	if (bmdm->vertexCos) {
		emDM_ensurePolyNormals(bmdm);
		copy_v3_v3(r_no, bmdm->polyNos[index]);
	}
	else {
		BMFace *efa;

		BLI_assert((bm->elem_table_dirty & BM_FACE) == 0);
		efa = bm->ftable[index];  /* should be BM_vert_at_index() */
		// efa = BM_face_at_index(bm, index); /* warning, does list loop, _not_ ideal */
		copy_v3_v3(r_no, efa->no);
	}
}

static void emDM_getEdge(DerivedMesh *dm, int index, MEdge *r_edge)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMEdge *e;
	const float *f;

	if (UNLIKELY(index < 0 || index >= bm->totedge)) {
		BLI_assert(!"error in emDM_getEdge");
		return;
	}

	BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
	e = bm->etable[index];  /* should be BM_edge_at_index() */
	// e = BM_edge_at_index(bm, index); /* warning, does list loop, _not_ ideal */

	r_edge->flag = BM_edge_flag_to_mflag(e);

	r_edge->v1 = BM_elem_index_get(e->v1);
	r_edge->v2 = BM_elem_index_get(e->v2);

	if ((f = CustomData_bmesh_get(&bm->edata, e->head.data, CD_BWEIGHT))) {
		r_edge->bweight = (unsigned char)((*f) * 255.0f);
	}
	if ((f = CustomData_bmesh_get(&bm->edata, e->head.data, CD_CREASE))) {
		r_edge->crease = (unsigned char)((*f) * 255.0f);
	}
}

static void emDM_getTessFace(DerivedMesh *dm, int index, MFace *r_face)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMFace *ef;
	BMLoop **ltri;

	if (UNLIKELY(index < 0 || index >= bmdm->em->tottri)) {
		BLI_assert(!"error in emDM_getTessFace");
		return;
	}

	ltri = bmdm->em->looptris[index];

	ef = ltri[0]->f;

	r_face->mat_nr = (unsigned char) ef->mat_nr;
	r_face->flag = BM_face_flag_to_mflag(ef);

	r_face->v1 = BM_elem_index_get(ltri[0]->v);
	r_face->v2 = BM_elem_index_get(ltri[1]->v);
	r_face->v3 = BM_elem_index_get(ltri[2]->v);
	r_face->v4 = 0;

	test_index_face(r_face, NULL, 0, 3);
}

static void emDM_copyVertArray(DerivedMesh *dm, MVert *r_vert)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMVert *eve;
	BMIter iter;
	const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);

	if (bmdm->vertexCos) {
		int i;

		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			copy_v3_v3(r_vert->co, bmdm->vertexCos[i]);
			normal_float_to_short_v3(r_vert->no, eve->no);
			r_vert->flag = BM_vert_flag_to_mflag(eve);

			r_vert->bweight = (cd_vert_bweight_offset != -1) ? BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eve, cd_vert_bweight_offset) : 0;

			r_vert++;
		}
	}
	else {
		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			copy_v3_v3(r_vert->co, eve->co);
			normal_float_to_short_v3(r_vert->no, eve->no);
			r_vert->flag = BM_vert_flag_to_mflag(eve);

			r_vert->bweight = (cd_vert_bweight_offset != -1) ? BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eve, cd_vert_bweight_offset) : 0;

			r_vert++;
		}
	}
}

static void emDM_copyEdgeArray(DerivedMesh *dm, MEdge *r_edge)
{
	BMesh *bm = ((EditDerivedBMesh *)dm)->em->bm;
	BMEdge *eed;
	BMIter iter;

	const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
	const int cd_edge_crease_offset  = CustomData_get_offset(&bm->edata, CD_CREASE);

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
		r_edge->v1 = BM_elem_index_get(eed->v1);
		r_edge->v2 = BM_elem_index_get(eed->v2);

		r_edge->flag = BM_edge_flag_to_mflag(eed);

		r_edge->crease  = (cd_edge_crease_offset  != -1) ? BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eed, cd_edge_crease_offset)  : 0;
		r_edge->bweight = (cd_edge_bweight_offset != -1) ? BM_ELEM_CD_GET_FLOAT_AS_UCHAR(eed, cd_edge_bweight_offset) : 0;

		r_edge++;
	}
}

static void emDM_copyTessFaceArray(DerivedMesh *dm, MFace *r_face)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	struct BMLoop *(*looptris)[3] = bmdm->em->looptris;
	BMFace *ef;
	int i;

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	for (i = 0; i < bmdm->em->tottri; i++, r_face++) {
		BMLoop **ltri = looptris[i];
		ef = ltri[0]->f;

		r_face->mat_nr = (unsigned char) ef->mat_nr;

		r_face->flag = BM_face_flag_to_mflag(ef);
		r_face->edcode = 0;

		r_face->v1 = BM_elem_index_get(ltri[0]->v);
		r_face->v2 = BM_elem_index_get(ltri[1]->v);
		r_face->v3 = BM_elem_index_get(ltri[2]->v);
		r_face->v4 = 0;

		test_index_face(r_face, NULL, 0, 3);
	}
}

static void emDM_copyLoopArray(DerivedMesh *dm, MLoop *r_loop)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMIter iter;
	BMFace *efa;

	BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE);

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_iter, *l_first;
		l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
		do {
			r_loop->v = BM_elem_index_get(l_iter->v);
			r_loop->e = BM_elem_index_get(l_iter->e);
			r_loop++;
		} while ((l_iter = l_iter->next) != l_first);
	}
}

static void emDM_copyPolyArray(DerivedMesh *dm, MPoly *r_poly)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMIter iter;
	BMFace *efa;
	int i;

	i = 0;
	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		r_poly->flag = BM_face_flag_to_mflag(efa);
		r_poly->loopstart = i;
		r_poly->totloop = efa->len;
		r_poly->mat_nr = efa->mat_nr;

		r_poly++;
		i += efa->len;
	}
}

static void *emDM_getTessFaceDataArray(DerivedMesh *dm, int type)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	void *datalayer;

	datalayer = DM_get_tessface_data_layer(dm, type);
	if (datalayer)
		return datalayer;

	/* layers are store per face for editmesh, we convert to a temporary
	 * data layer array in the derivedmesh when these are requested */
	if (type == CD_MTFACE || type == CD_MCOL) {
		const int type_from = (type == CD_MTFACE) ? CD_MTEXPOLY : CD_MLOOPCOL;
		int index;
		const char *data, *bmdata;
		index = CustomData_get_layer_index(&bm->pdata, type_from);

		if (index != -1) {
			/* offset = bm->pdata.layers[index].offset; */ /* UNUSED */
			BMLoop *(*looptris)[3] = bmdm->em->looptris;
			const int size = CustomData_sizeof(type);
			int i, j;

			DM_add_tessface_layer(dm, type, CD_CALLOC, NULL);
			index = CustomData_get_layer_index(&dm->faceData, type);
			dm->faceData.layers[index].flag |= CD_FLAG_TEMPORARY;

			data = datalayer = DM_get_tessface_data_layer(dm, type);

			if (type == CD_MTFACE) {
				const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
				const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);

				for (i = 0; i < bmdm->em->tottri; i++, data += size) {
					BMFace *efa = looptris[i][0]->f;

					// bmdata = CustomData_bmesh_get(&bm->pdata, efa->head.data, CD_MTEXPOLY);
					bmdata = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

					ME_MTEXFACE_CPY(((MTFace *)data), ((MTexPoly *)bmdata));
					for (j = 0; j < 3; j++) {
						// bmdata = CustomData_bmesh_get(&bm->ldata, looptris[i][j]->head.data, CD_MLOOPUV);
						bmdata = BM_ELEM_CD_GET_VOID_P(looptris[i][j], cd_loop_uv_offset);
						copy_v2_v2(((MTFace *)data)->uv[j], ((MLoopUV *)bmdata)->uv);
					}
				}
			}
			else {
				const int cd_loop_color_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPCOL);
				for (i = 0; i < bmdm->em->tottri; i++, data += size) {
					for (j = 0; j < 3; j++) {
						// bmdata = CustomData_bmesh_get(&bm->ldata, looptris[i][j]->head.data, CD_MLOOPCOL);
						bmdata = BM_ELEM_CD_GET_VOID_P(looptris[i][j], cd_loop_color_offset);
						MESH_MLOOPCOL_TO_MCOL(((MLoopCol *)bmdata), (((MCol *)data) + j));
					}
				}
			}
		}
	}

	/* Special handling for CD_TESSLOOPNORMAL, we generate it on demand as well. */
	if (type == CD_TESSLOOPNORMAL) {
		const float (*lnors)[3] = dm->getLoopDataArray(dm, CD_NORMAL);

		if (lnors) {
			BMLoop *(*looptris)[3] = bmdm->em->looptris;
			short (*tlnors)[4][3], (*tlnor)[4][3];
			int index, i, j;

			DM_add_tessface_layer(dm, type, CD_CALLOC, NULL);
			index = CustomData_get_layer_index(&dm->faceData, type);
			dm->faceData.layers[index].flag |= CD_FLAG_TEMPORARY;

			tlnor = tlnors = DM_get_tessface_data_layer(dm, type);

			BM_mesh_elem_index_ensure(bm, BM_LOOP);

			for (i = 0; i < bmdm->em->tottri; i++, tlnor++, looptris++) {
				for (j = 0; j < 3; j++) {
					normal_float_to_short_v3((*tlnor)[j], lnors[BM_elem_index_get((*looptris)[j])]);
				}
			}
		}
	}

	return datalayer;
}

static void emDM_getVertCos(DerivedMesh *dm, float (*r_cos)[3])
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->em->bm;
	BMVert *eve;
	BMIter iter;
	int i;

	if (bmdm->vertexCos) {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			copy_v3_v3(r_cos[i], bmdm->vertexCos[i]);
		}
	}
	else {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			copy_v3_v3(r_cos[i], eve->co);
		}
	}
}

static void emDM_release(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	if (DM_release(dm)) {
		if (bmdm->vertexCos) {
			MEM_freeN((void *)bmdm->vertexCos);
			if (bmdm->vertexNos) {
				MEM_freeN((void *)bmdm->vertexNos);
			}
			if (bmdm->polyNos) {
				MEM_freeN((void *)bmdm->polyNos);
			}
		}

		if (bmdm->polyCos) {
			MEM_freeN((void *)bmdm->polyCos);
		}

		MEM_freeN(bmdm);
	}
}

static CustomData *bmDm_getVertDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return &bmdm->em->bm->vdata;
}

static CustomData *bmDm_getEdgeDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return &bmdm->em->bm->edata;
}

static CustomData *bmDm_getTessFaceDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return &bmdm->dm.faceData;
}

static CustomData *bmDm_getLoopDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return &bmdm->em->bm->ldata;
}

static CustomData *bmDm_getPolyDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;

	return &bmdm->em->bm->pdata;
}


DerivedMesh *getEditDerivedBMesh(BMEditMesh *em,
                                 Object *UNUSED(ob),
                                 float (*vertexCos)[3])
{
	EditDerivedBMesh *bmdm = MEM_callocN(sizeof(*bmdm), __func__);
	BMesh *bm = em->bm;
	const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
	const int cd_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);

	bmdm->em = em;

	DM_init((DerivedMesh *)bmdm, DM_TYPE_EDITBMESH, bm->totvert,
	        bm->totedge, em->tottri, bm->totloop, bm->totface);

	/* could also get from the objects mesh directly */
	bmdm->dm.cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

	bmdm->dm.getVertCos = emDM_getVertCos;
	bmdm->dm.getMinMax = emDM_getMinMax;

	bmdm->dm.getVertDataLayout = bmDm_getVertDataLayout;
	bmdm->dm.getEdgeDataLayout = bmDm_getEdgeDataLayout;
	bmdm->dm.getTessFaceDataLayout = bmDm_getTessFaceDataLayout;
	bmdm->dm.getLoopDataLayout = bmDm_getLoopDataLayout;
	bmdm->dm.getPolyDataLayout = bmDm_getPolyDataLayout;

	bmdm->dm.getNumVerts = emDM_getNumVerts;
	bmdm->dm.getNumEdges = emDM_getNumEdges;
	bmdm->dm.getNumTessFaces = emDM_getNumTessFaces;
	bmdm->dm.getNumLoops = emDM_getNumLoops;
	bmdm->dm.getNumPolys = emDM_getNumPolys;

	bmdm->dm.getVert = emDM_getVert;
	bmdm->dm.getVertCo = emDM_getVertCo;
	bmdm->dm.getVertNo = emDM_getVertNo;
	bmdm->dm.getPolyNo = emDM_getPolyNo;
	bmdm->dm.getEdge = emDM_getEdge;
	bmdm->dm.getTessFace = emDM_getTessFace;
	bmdm->dm.copyVertArray = emDM_copyVertArray;
	bmdm->dm.copyEdgeArray = emDM_copyEdgeArray;
	bmdm->dm.copyTessFaceArray = emDM_copyTessFaceArray;
	bmdm->dm.copyLoopArray = emDM_copyLoopArray;
	bmdm->dm.copyPolyArray = emDM_copyPolyArray;

	bmdm->dm.getTessFaceDataArray = emDM_getTessFaceDataArray;

	bmdm->dm.calcNormals = emDM_calcNormals;
	bmdm->dm.calcLoopNormals = emDM_calcLoopNormals;
	bmdm->dm.recalcTessellation = emDM_recalcTessellation;

	bmdm->dm.foreachMappedVert = emDM_foreachMappedVert;
	bmdm->dm.foreachMappedLoop = emDM_foreachMappedLoop;
	bmdm->dm.foreachMappedEdge = emDM_foreachMappedEdge;
	bmdm->dm.foreachMappedFaceCenter = emDM_foreachMappedFaceCenter;

	bmdm->dm.drawEdges = emDM_drawEdges;
	bmdm->dm.drawMappedEdges = emDM_drawMappedEdges;
	bmdm->dm.drawMappedEdgesInterp = emDM_drawMappedEdgesInterp;
	bmdm->dm.drawMappedFaces = emDM_drawMappedFaces;
	bmdm->dm.drawMappedFacesTex = emDM_drawMappedFacesTex;
	bmdm->dm.drawMappedFacesGLSL = emDM_drawMappedFacesGLSL;
	bmdm->dm.drawMappedFacesMat = emDM_drawMappedFacesMat;
	bmdm->dm.drawFacesTex = emDM_drawFacesTex;
	bmdm->dm.drawFacesGLSL = emDM_drawFacesGLSL;
	bmdm->dm.drawUVEdges = emDM_drawUVEdges;

	bmdm->dm.release = emDM_release;

	bmdm->vertexCos = (const float (*)[3])vertexCos;
	bmdm->dm.deformedOnly = (vertexCos != NULL);

	if (cd_dvert_offset != -1) {
		BMIter iter;
		BMVert *eve;
		int i;

		DM_add_vert_layer(&bmdm->dm, CD_MDEFORMVERT, CD_CALLOC, NULL);

		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			DM_set_vert_data(&bmdm->dm, i, CD_MDEFORMVERT,
			                 BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
		}
	}

	if (cd_skin_offset != -1) {
		BMIter iter;
		BMVert *eve;
		int i;

		DM_add_vert_layer(&bmdm->dm, CD_MVERT_SKIN, CD_CALLOC, NULL);

		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			DM_set_vert_data(&bmdm->dm, i, CD_MVERT_SKIN,
			                 BM_ELEM_CD_GET_VOID_P(eve, cd_skin_offset));
		}
	}

	return (DerivedMesh *)bmdm;
}



/* -------------------------------------------------------------------- */
/* StatVis Functions */

static void axis_from_enum_v3(float v[3], const char axis)
{
	zero_v3(v);
	if (axis < 3) v[axis]     =  1.0f;
	else          v[axis - 3] = -1.0f;
}

static void statvis_calc_overhang(
        BMEditMesh *em,
        const float (*polyNos)[3],
        /* values for calculating */
        const float min, const float max, const char axis,
        /* result */
        unsigned char (*r_face_colors)[4])
{
	BMIter iter;
	BMesh *bm = em->bm;
	BMFace *f;
	float dir[3];
	int index;
	const float minmax_irange = 1.0f / (max - min);
	bool is_max;

	/* fallback */
	unsigned char col_fallback[4] = {64, 64, 64, 255}; /* gray */
	unsigned char col_fallback_max[4] = {0,  0,  0,  255}; /* max color */

	BLI_assert(min <= max);

	axis_from_enum_v3(dir, axis);

	if (LIKELY(em->ob)) {
		mul_transposed_mat3_m4_v3(em->ob->obmat, dir);
		normalize_v3(dir);
	}

	/* fallback max */
	{
		float fcol[3];
		weight_to_rgb(fcol, 1.0f);
		rgb_float_to_uchar(col_fallback_max, fcol);
	}

	/* now convert into global space */
	BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, index) {
		float fac = angle_normalized_v3v3(polyNos ? polyNos[index] : f->no, dir) / (float)M_PI;

		/* remap */
		if ((is_max = (fac <= max)) && (fac >= min)) {
			float fcol[3];
			fac = (fac - min) * minmax_irange;
			fac = 1.0f - fac;
			CLAMP(fac, 0.0f, 1.0f);
			weight_to_rgb(fcol, fac);
			rgb_float_to_uchar(r_face_colors[index], fcol);
		}
		else {
			unsigned char *fallback = is_max ? col_fallback_max : col_fallback;
			copy_v4_v4_char((char *)r_face_colors[index], (const char *)fallback);
		}
	}
}

/* so we can use jitter values for face interpolation */
static void uv_from_jitter_v2(float uv[2])
{
	uv[0] += 0.5f;
	uv[1] += 0.5f;
	if (uv[0] + uv[1] > 1.0f) {
		uv[0] = 1.0f - uv[0];
		uv[1] = 1.0f - uv[1];
	}

	CLAMP(uv[0], 0.0f, 1.0f);
	CLAMP(uv[1], 0.0f, 1.0f);
}

static void statvis_calc_thickness(
        BMEditMesh *em,
        const float (*vertexCos)[3],
        /* values for calculating */
        const float min, const float max, const int samples,
        /* result */
        unsigned char (*r_face_colors)[4])
{
	const float eps_offset = 0.00002f;  /* values <= 0.00001 give errors */
	float *face_dists = (float *)r_face_colors;  /* cheating */
	const bool use_jit = samples < 32;
	float jit_ofs[32][2];
	BMesh *bm = em->bm;
	const int tottri = em->tottri;
	const float minmax_irange = 1.0f / (max - min);
	int i;

	struct BMLoop *(*looptris)[3] = em->looptris;

	/* fallback */
	const char col_fallback[4] = {64, 64, 64, 255};

	struct BMBVHTree *bmtree;

	BLI_assert(min <= max);

	fill_vn_fl(face_dists, em->bm->totface, max);

	if (use_jit) {
		int j;
		BLI_assert(samples < 32);
		BLI_jitter_init(jit_ofs, samples);

		for (j = 0; j < samples; j++) {
			uv_from_jitter_v2(jit_ofs[j]);
		}
	}

	BM_mesh_elem_index_ensure(bm, BM_FACE);
	if (vertexCos) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}

	bmtree = BKE_bmbvh_new_from_editmesh(em, 0, vertexCos, false);

	for (i = 0; i < tottri; i++) {
		BMFace *f_hit;
		BMLoop **ltri = looptris[i];
		const int index = BM_elem_index_get(ltri[0]->f);
		const float *cos[3];
		float ray_co[3];
		float ray_no[3];

		if (vertexCos) {
			cos[0] = vertexCos[BM_elem_index_get(ltri[0]->v)];
			cos[1] = vertexCos[BM_elem_index_get(ltri[1]->v)];
			cos[2] = vertexCos[BM_elem_index_get(ltri[2]->v)];
		}
		else {
			cos[0] = ltri[0]->v->co;
			cos[1] = ltri[1]->v->co;
			cos[2] = ltri[2]->v->co;
		}

		normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

#define FACE_RAY_TEST_ANGLE \
		f_hit = BKE_bmbvh_ray_cast(bmtree, ray_co, ray_no, 0.0f, \
		                           &dist, NULL, NULL); \
		if (f_hit && dist < face_dists[index]) { \
			float angle_fac = fabsf(dot_v3v3(ltri[0]->f->no, f_hit->no)); \
			angle_fac = 1.0f - angle_fac; \
			angle_fac = angle_fac * angle_fac * angle_fac; \
			angle_fac = 1.0f - angle_fac; \
			dist /= angle_fac; \
			if (dist < face_dists[index]) { \
				face_dists[index] = dist; \
			} \
		} (void)0

		if (use_jit) {
			int j;
			for (j = 0; j < samples; j++) {
				float dist = face_dists[index];
				interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
				madd_v3_v3fl(ray_co, ray_no, eps_offset);

				FACE_RAY_TEST_ANGLE;
			}
		}
		else {
			float dist = face_dists[index];
			mid_v3_v3v3v3(ray_co, cos[0], cos[1], cos[2]);
			madd_v3_v3fl(ray_co, ray_no, eps_offset);

			FACE_RAY_TEST_ANGLE;
		}
	}

	BKE_bmbvh_free(bmtree);

	/* convert floats into color! */
	for (i = 0; i < bm->totface; i++) {
		float fac = face_dists[i];

		/* important not '<=' */
		if (fac < max) {
			float fcol[3];
			fac = (fac - min) * minmax_irange;
			fac = 1.0f - fac;
			CLAMP(fac, 0.0f, 1.0f);
			weight_to_rgb(fcol, fac);
			rgb_float_to_uchar(r_face_colors[i], fcol);
		}
		else {
			copy_v4_v4_char((char *)r_face_colors[i], (const char *)col_fallback);
		}
	}
}

static void statvis_calc_intersect(
        BMEditMesh *em,
        const float (*vertexCos)[3],
        /* result */
        unsigned char (*r_face_colors)[4])
{
	BMIter iter;
	BMesh *bm = em->bm;
	BMEdge *e;
	int index;

	/* fallback */
	// const char col_fallback[4] = {64, 64, 64, 255};

	struct BMBVHTree *bmtree;

	memset(r_face_colors, 64, sizeof(int) * em->bm->totface);

	BM_mesh_elem_index_ensure(bm, BM_FACE);
	if (vertexCos) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}

	bmtree = BKE_bmbvh_new_from_editmesh(em, 0, vertexCos, false);

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BMFace *f_hit;
		float cos[2][3];
		float cos_mid[3];
		float ray_no[3];

		if (e->l == NULL)
			continue;

		if (vertexCos) {
			copy_v3_v3(cos[0], vertexCos[BM_elem_index_get(e->v1)]);
			copy_v3_v3(cos[1], vertexCos[BM_elem_index_get(e->v2)]);
		}
		else {
			copy_v3_v3(cos[0], e->v1->co);
			copy_v3_v3(cos[1], e->v2->co);
		}

		mid_v3_v3v3(cos_mid, cos[0], cos[1]);
		sub_v3_v3v3(ray_no, cos[1], cos[0]);

		f_hit = BKE_bmbvh_find_face_segment(bmtree, cos[0], cos[1],
		                                    NULL, NULL, NULL);

		if (f_hit) {
			BMLoop *l_iter, *l_first;
			float fcol[3];

			index = BM_elem_index_get(f_hit);
			weight_to_rgb(fcol, 1.0f);
			rgb_float_to_uchar(r_face_colors[index], fcol);

			l_iter = l_first = e->l;
			do {
				index = BM_elem_index_get(l_iter->f);
				weight_to_rgb(fcol, 1.0f);
				rgb_float_to_uchar(r_face_colors[index], fcol);
			} while ((l_iter = l_iter->radial_next) != l_first);
		}

	}

	BKE_bmbvh_free(bmtree);
}

static void statvis_calc_distort(
        BMEditMesh *em,
        const float (*vertexCos)[3], const float (*polyNos)[3],
        /* values for calculating */
        const float min, const float max,
        /* result */
        unsigned char (*r_face_colors)[4])
{
	BMIter iter;
	BMesh *bm = em->bm;
	BMFace *f;
	const float *f_no;
	int index;
	const float minmax_irange = 1.0f / (max - min);

	/* fallback */
	const char col_fallback[4] = {64, 64, 64, 255};

	/* now convert into global space */
	BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, index) {
		float fac;

		if (f->len == 3) {
			fac = -1.0f;
		}
		else {
			BMLoop *l_iter, *l_first;
			if (vertexCos) {
				f_no = polyNos[index];
			}
			else {
				f_no = f->no;
			}

			fac = 0.0f;
			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				float no_corner[3];
				if (vertexCos) {
					normal_tri_v3(no_corner,
					              vertexCos[BM_elem_index_get(l_iter->prev->v)],
					              vertexCos[BM_elem_index_get(l_iter->v)],
					              vertexCos[BM_elem_index_get(l_iter->next->v)]);
				}
				else {
					BM_loop_calc_face_normal(l_iter, no_corner);
				}
				/* simple way to detect (what is most likely) concave */
				if (dot_v3v3(f_no, no_corner) < 0.0f) {
					negate_v3(no_corner);
				}
				fac = max_ff(fac, angle_normalized_v3v3(f_no, no_corner));
			} while ((l_iter = l_iter->next) != l_first);
			fac *= 2.0f;
		}

		/* remap */
		if (fac >= min) {
			float fcol[3];
			fac = (fac - min) * minmax_irange;
			CLAMP(fac, 0.0f, 1.0f);
			weight_to_rgb(fcol, fac);
			rgb_float_to_uchar(r_face_colors[index], fcol);
		}
		else {
			copy_v4_v4_char((char *)r_face_colors[index], (const char *)col_fallback);
		}
	}
}

static void statvis_calc_sharp(
        BMEditMesh *em,
        const float (*vertexCos)[3],
        /* values for calculating */
        const float min, const float max,
        /* result */
        unsigned char (*r_vert_colors)[4])
{
	float *vert_angles = (float *)r_vert_colors;  /* cheating */
	BMIter iter;
	BMesh *bm = em->bm;
	BMEdge *e;
	//float f_no[3];
	const float minmax_irange = 1.0f / (max - min);
	int i;

	/* fallback */
	const char col_fallback[4] = {64, 64, 64, 255};

	(void)vertexCos;  /* TODO */

	fill_vn_fl(vert_angles, em->bm->totvert, -M_PI);

	/* first assign float values to verts */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		float angle = BM_edge_calc_face_angle_signed(e);
		float *col1 = &vert_angles[BM_elem_index_get(e->v1)];
		float *col2 = &vert_angles[BM_elem_index_get(e->v2)];
		*col1 = max_ff(*col1, angle);
		*col2 = max_ff(*col2, angle);
	}

	/* convert floats into color! */
	for (i = 0; i < bm->totvert; i++) {
		float fac = vert_angles[i];

		/* important not '<=' */
		if (fac > min) {
			float fcol[3];
			fac = (fac - min) * minmax_irange;
			CLAMP(fac, 0.0f, 1.0f);
			weight_to_rgb(fcol, fac);
			rgb_float_to_uchar(r_vert_colors[i], fcol);
		}
		else {
			copy_v4_v4_char((char *)r_vert_colors[i], (const char *)col_fallback);
		}
	}
}

void BKE_editmesh_statvis_calc(BMEditMesh *em, DerivedMesh *dm,
                               MeshStatVis *statvis)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BLI_assert(dm == NULL || dm->type == DM_TYPE_EDITBMESH);

	switch (statvis->type) {
		case SCE_STATVIS_OVERHANG:
		{
			BKE_editmesh_color_ensure(em, BM_FACE);
			statvis_calc_overhang(
			            em, bmdm ? bmdm->polyNos : NULL,
			            statvis->overhang_min / (float)M_PI,
			            statvis->overhang_max / (float)M_PI,
			            statvis->overhang_axis,
			            em->derivedFaceColor);
			break;
		}
		case SCE_STATVIS_THICKNESS:
		{
			const float scale = 1.0f / mat4_to_scale(em->ob->obmat);
			BKE_editmesh_color_ensure(em, BM_FACE);
			statvis_calc_thickness(
			            em, bmdm ? bmdm->vertexCos : NULL,
			            statvis->thickness_min * scale,
			            statvis->thickness_max * scale,
			            statvis->thickness_samples,
			            em->derivedFaceColor);
			break;
		}
		case SCE_STATVIS_INTERSECT:
		{
			BKE_editmesh_color_ensure(em, BM_FACE);
			statvis_calc_intersect(
			            em, bmdm ? bmdm->vertexCos : NULL,
			            em->derivedFaceColor);
			break;
		}
		case SCE_STATVIS_DISTORT:
		{
			BKE_editmesh_color_ensure(em, BM_FACE);

			if (bmdm)
				emDM_ensurePolyNormals(bmdm);

			statvis_calc_distort(
			        em, bmdm ? bmdm->vertexCos : NULL, bmdm ? bmdm->polyNos : NULL,
			        statvis->distort_min,
			        statvis->distort_max,
			        em->derivedFaceColor);
			break;
		}
		case SCE_STATVIS_SHARP:
		{
			BKE_editmesh_color_ensure(em, BM_VERT);
			statvis_calc_sharp(
			        em, bmdm ? bmdm->vertexCos : NULL,
			        statvis->sharp_min,
			        statvis->sharp_max,
			        /* in this case they are vertex colors */
			        em->derivedVertColor);
			break;
		}
	}
}



/* -------------------------------------------------------------------- */
/* Editmesh Vert Coords */

struct CageUserData {
	int totvert;
	float (*cos_cage)[3];
	BLI_bitmap *visit_bitmap;
};

static void cage_mapped_verts_callback(void *userData, int index, const float co[3],
                                       const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	struct CageUserData *data = userData;

	if ((index >= 0 && index < data->totvert) && (!BLI_BITMAP_TEST(data->visit_bitmap, index))) {
		BLI_BITMAP_ENABLE(data->visit_bitmap, index);
		copy_v3_v3(data->cos_cage[index], co);
	}
}

float (*BKE_editmesh_vertexCos_get(BMEditMesh *em, Scene *scene, int *r_numVerts))[3]
{
	DerivedMesh *cage, *final;
	BLI_bitmap *visit_bitmap;
	struct CageUserData data;
	float (*cos_cage)[3];

	cage = editbmesh_get_derived_cage_and_final(scene, em->ob, em, &final, CD_MASK_BAREMESH);
	cos_cage = MEM_callocN(sizeof(*cos_cage) * em->bm->totvert, "bmbvh cos_cage");

	/* when initializing cage verts, we only want the first cage coordinate for each vertex,
	 * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate */
	visit_bitmap = BLI_BITMAP_NEW(em->bm->totvert, __func__);

	data.totvert = em->bm->totvert;
	data.cos_cage = cos_cage;
	data.visit_bitmap = visit_bitmap;

	cage->foreachMappedVert(cage, cage_mapped_verts_callback, &data, DM_FOREACH_NOP);

	MEM_freeN(visit_bitmap);

	if (r_numVerts) {
		*r_numVerts = em->bm->totvert;
	}

	return cos_cage;
}
