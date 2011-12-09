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
 */

#include "GL/glew.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_math.h"
#include "BLI_pbvh.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"


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

static void emDM_foreachMappedVert(
		DerivedMesh *dm,
		void (*func)(void *userData, int index, float *co, float *no_f, short *no_s),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	int i;

	for (i=0,eve= emdm->em->verts.first; eve; i++,eve=eve->next) {
		if (emdm->vertexCos) {
			func(userData, i, emdm->vertexCos[i], emdm->vertexNos[i], NULL);
		}
		else {
			func(userData, i, eve->co, eve->no, NULL);
		}
	}
}
static void emDM_foreachMappedEdge(
		DerivedMesh *dm,
		void (*func)(void *userData, int index, float *v0co, float *v1co),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next)
			func(userData, i, emdm->vertexCos[(int) eed->v1->tmp.l], emdm->vertexCos[(int) eed->v2->tmp.l]);
	}
	else {
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next)
			func(userData, i, eed->v1->co, eed->v2->co);
	}
}

static void emDM_drawMappedEdges(
		DerivedMesh *dm,
		int (*setDrawOptions)(void *userData, int index),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if (!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(emdm->vertexCos[(int) eed->v1->tmp.l]);
				glVertex3fv(emdm->vertexCos[(int) eed->v2->tmp.l]);
			}
		}
		glEnd();
	}
	else {
		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if (!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(eed->v1->co);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}
static void emDM_drawEdges(
		DerivedMesh *dm,
		int UNUSED(drawLooseEdges),
		int UNUSED(drawAllEdges))
{
	emDM_drawMappedEdges(dm, NULL, NULL);
}

static void emDM_drawMappedEdgesInterp(
		DerivedMesh *dm,
		int (*setDrawOptions)(void *userData, int index),
		void (*setDrawInterpOptions)(void *userData, int index, float t),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if (!setDrawOptions || setDrawOptions(userData, i)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(emdm->vertexCos[(int) eed->v1->tmp.l]);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(emdm->vertexCos[(int) eed->v2->tmp.l]);
			}
		}
		glEnd();
	}
	else {
		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if (!setDrawOptions || setDrawOptions(userData, i)) {
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
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;
	MTFace *tf;

	glBegin(GL_LINES);
	for (efa= emdm->em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&emdm->em->fdata, efa->data, CD_MTFACE);

		if (tf && !(efa->h)) {
			glVertex2fv(tf->uv[0]);
			glVertex2fv(tf->uv[1]);

			glVertex2fv(tf->uv[1]);
			glVertex2fv(tf->uv[2]);

			if (!efa->v4) {
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

static void emDM__calcFaceCent(EditFace *efa, float cent[3], float (*vertexCos)[3])
{
	if (vertexCos) {
		copy_v3_v3(cent, vertexCos[(int) efa->v1->tmp.l]);
		add_v3_v3(cent, vertexCos[(int) efa->v2->tmp.l]);
		add_v3_v3(cent, vertexCos[(int) efa->v3->tmp.l]);
		if (efa->v4) add_v3_v3(cent, vertexCos[(int) efa->v4->tmp.l]);
	}
	else {
		copy_v3_v3(cent, efa->v1->co);
		add_v3_v3(cent, efa->v2->co);
		add_v3_v3(cent, efa->v3->co);
		if (efa->v4) add_v3_v3(cent, efa->v4->co);
	}

	if (efa->v4) {
		mul_v3_fl(cent, 0.25f);
	}
	else {
		mul_v3_fl(cent, 0.33333333333f);
	}
}

static void emDM_foreachMappedFaceCenter(
		DerivedMesh *dm,
		void (*func)(void *userData, int index, float *co, float *no),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	EditFace *efa;
	float cent[3];
	int i;

	if (emdm->vertexCos) {
		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;
	}

	for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
		emDM__calcFaceCent(efa, cent, emdm->vertexCos);
		func(userData, i, cent, emdm->vertexCos?emdm->faceNos[i]:efa->n);
	}
}

/* note, material function is ignored for now. */
static void emDM_drawMappedFaces(
		DerivedMesh *dm,
		int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r),
		int (*setMaterial)(int, void *attribs),
		int (*compareDrawOptions)(void *userData, int cur_index, int next_index),
		void *userData, int UNUSED(useColors))
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;
	int i, draw, flush;
	const int skip_normals= !glIsEnabled(GL_LIGHTING); /* could be passed as an arg */

	/* GL_ZERO is used to detect if drawing has started or not */
	GLenum poly_prev= GL_ZERO;
	GLenum shade_prev= GL_ZERO;

	(void)setMaterial; /* UNUSED */

	/* currently unused -- each original face is handled separately */
	(void)compareDrawOptions;

	if (emdm->vertexCos) {
		/* add direct access */
		float (*vertexCos)[3]= emdm->vertexCos;
		float (*vertexNos)[3]= emdm->vertexNos;
		float (*faceNos)[3]=   emdm->faceNos;
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
			int drawSmooth = (efa->flag & ME_SMOOTH);
			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, i, &drawSmooth);
			if (draw) {
				const GLenum poly_type= efa->v4 ? GL_QUADS:GL_TRIANGLES;
				if (draw==2) { /* enabled with stipple */

					if (poly_prev != GL_ZERO) glEnd();
					poly_prev= GL_ZERO; /* force glBegin */

					glEnable(GL_POLYGON_STIPPLE);
					glPolygonStipple(stipple_quarttone);
				}

				if (skip_normals) {
					if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev= poly_type));
					}
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
					if (poly_type == GL_QUADS) glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
				}
				else {
					const GLenum shade_type= drawSmooth ? GL_SMOOTH : GL_FLAT;
					if (shade_type != shade_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glShadeModel((shade_prev= shade_type)); /* same as below but switch shading */
						glBegin((poly_prev= poly_type));
					}
					else if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev= poly_type));
					}

					if (!drawSmooth) {
						glNormal3fv(faceNos[i]);
						glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
						if (poly_type == GL_QUADS) glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
					else {
						glNormal3fv(vertexNos[(int) efa->v1->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
						glNormal3fv(vertexNos[(int) efa->v2->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
						glNormal3fv(vertexNos[(int) efa->v3->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
						if (poly_type == GL_QUADS) {
							glNormal3fv(vertexNos[(int) efa->v4->tmp.l]);
							glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
						}
					}
				}

				flush= (draw==2);
				if (!skip_normals && !flush && efa->next)
					flush|= efa->mat_nr != efa->next->mat_nr;

				if (flush) {
					glEnd();
					poly_prev= GL_ZERO; /* force glBegin */

					glDisable(GL_POLYGON_STIPPLE);
				}
			}
		}
	}
	else {
		for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
			int drawSmooth = (efa->flag & ME_SMOOTH);
			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, i, &drawSmooth);
			if (draw) {
				const GLenum poly_type= efa->v4 ? GL_QUADS:GL_TRIANGLES;
				if (draw==2) { /* enabled with stipple */

					if (poly_prev != GL_ZERO) glEnd();
					poly_prev= GL_ZERO; /* force glBegin */

					glEnable(GL_POLYGON_STIPPLE);
					glPolygonStipple(stipple_quarttone);
				}

				if (skip_normals) {
					if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev= poly_type));
					}
					glVertex3fv(efa->v1->co);
					glVertex3fv(efa->v2->co);
					glVertex3fv(efa->v3->co);
					if (poly_type == GL_QUADS) glVertex3fv(efa->v4->co);
				}
				else {
					const GLenum shade_type= drawSmooth ? GL_SMOOTH : GL_FLAT;
					if (shade_type != shade_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glShadeModel((shade_prev= shade_type)); /* same as below but switch shading */
						glBegin((poly_prev= poly_type));
					}
					else if (poly_type != poly_prev) {
						if (poly_prev != GL_ZERO) glEnd();
						glBegin((poly_prev= poly_type));
					}

					if (!drawSmooth) {
						glNormal3fv(efa->n);
						glVertex3fv(efa->v1->co);
						glVertex3fv(efa->v2->co);
						glVertex3fv(efa->v3->co);
						if (poly_type == GL_QUADS) glVertex3fv(efa->v4->co);
					}
					else {
						glNormal3fv(efa->v1->no);
						glVertex3fv(efa->v1->co);
						glNormal3fv(efa->v2->no);
						glVertex3fv(efa->v2->co);
						glNormal3fv(efa->v3->no);
						glVertex3fv(efa->v3->co);
						if (poly_type == GL_QUADS) {
							glNormal3fv(efa->v4->no);
							glVertex3fv(efa->v4->co);
						}
					}
				}

				flush= (draw==2);
				if (!skip_normals && !flush && efa->next)
					flush|= efa->mat_nr != efa->next->mat_nr;

				if (flush) {
					glEnd();
					poly_prev= GL_ZERO; /* force glBegin */

					glDisable(GL_POLYGON_STIPPLE);
				}
			}
		}
	}

	/* if non zero we know a face was rendered */
	if (poly_prev != GL_ZERO) glEnd();
}

static void emDM_drawFacesTex_common(
		DerivedMesh *dm,
		int (*drawParams)(MTFace *tface, int has_mcol, int matnr),
		int (*drawParamsMapped)(void *userData, int index),
		int (*compareDrawOptions)(void *userData, int cur_index, int next_index),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	float (*vertexCos)[3]= emdm->vertexCos;
	float (*vertexNos)[3]= emdm->vertexNos;
	EditFace *efa;
	int i;

	(void) compareDrawOptions;

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	if (vertexCos) {
		EditVert *eve;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
			MTFace *tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			MCol *mcol= CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			unsigned char *cp= NULL;
			int drawSmooth= (efa->flag & ME_SMOOTH);
			int flag;

			if (drawParams)
				flag= drawParams(tf, (mcol != NULL), efa->mat_nr);
			else if (drawParamsMapped)
				flag= drawParamsMapped(userData, i);
			else
				flag= 1;

			if (flag != 0) { /* flag 0 == the face is hidden or invisible */

				/* we always want smooth here since otherwise vertex colors dont interpolate */
				if (mcol) {
					if (flag==1) {
						cp= (unsigned char*)mcol;
					}
				}
				else {
					glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				}

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(emdm->faceNos[i]);

					if (tf) glTexCoord2fv(tf->uv[0]);
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);

					if (tf) glTexCoord2fv(tf->uv[1]);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);

					if (tf) glTexCoord2fv(tf->uv[2]);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);

					if (efa->v4) {
						if (tf) glTexCoord2fv(tf->uv[3]);
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
				}
				else {
					if (tf) glTexCoord2fv(tf->uv[0]);
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glNormal3fv(vertexNos[(int) efa->v1->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);

					if (tf) glTexCoord2fv(tf->uv[1]);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glNormal3fv(vertexNos[(int) efa->v2->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);

					if (tf) glTexCoord2fv(tf->uv[2]);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3fv(vertexNos[(int) efa->v3->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);

					if (efa->v4) {
						if (tf) glTexCoord2fv(tf->uv[3]);
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glNormal3fv(vertexNos[(int) efa->v4->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
				}
				glEnd();
			}
		}
	}
	else {
		for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
			MTFace *tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			MCol *mcol= CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			unsigned char *cp= NULL;
			int drawSmooth= (efa->flag & ME_SMOOTH);
			int flag;

			if (drawParams)
				flag= drawParams(tf, (mcol != NULL), efa->mat_nr);
			else if (drawParamsMapped)
				flag= drawParamsMapped(userData, i);
			else
				flag= 1;

			if (flag != 0) { /* flag 0 == the face is hidden or invisible */

				/* we always want smooth here since otherwise vertex colors dont interpolate */
				if (mcol) {
					if (flag==1) {
						cp= (unsigned char*)mcol;
					}
				}
				else {
					glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				}

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->n);

					if (tf) glTexCoord2fv(tf->uv[0]);
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glVertex3fv(efa->v1->co);

					if (tf) glTexCoord2fv(tf->uv[1]);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glVertex3fv(efa->v2->co);

					if (tf) glTexCoord2fv(tf->uv[2]);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glVertex3fv(efa->v3->co);

					if (efa->v4) {
						if (tf) glTexCoord2fv(tf->uv[3]);
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(efa->v4->co);
					}
				}
				else {
					if (tf) glTexCoord2fv(tf->uv[0]);
					if (cp) glColor3ub(cp[3], cp[2], cp[1]);
					glNormal3fv(efa->v1->no);
					glVertex3fv(efa->v1->co);

					if (tf) glTexCoord2fv(tf->uv[1]);
					if (cp) glColor3ub(cp[7], cp[6], cp[5]);
					glNormal3fv(efa->v2->no);
					glVertex3fv(efa->v2->co);

					if (tf) glTexCoord2fv(tf->uv[2]);
					if (cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3fv(efa->v3->no);
					glVertex3fv(efa->v3->co);

					if (efa->v4) {
						if (tf) glTexCoord2fv(tf->uv[3]);
						if (cp) glColor3ub(cp[15], cp[14], cp[13]);
						glNormal3fv(efa->v4->no);
						glVertex3fv(efa->v4->co);
					}
				}
				glEnd();
			}
		}
	}
}

static void emDM_drawFacesTex(
		DerivedMesh *dm,
		int (*setDrawOptions)(MTFace *tface, int has_mcol, int matnr),
		int (*compareDrawOptions)(void *userData, int cur_index, int next_index),
		void *userData)
{
	emDM_drawFacesTex_common(dm, setDrawOptions, NULL, compareDrawOptions, userData);
}

static void emDM_drawMappedFacesTex(
		DerivedMesh *dm,
		int (*setDrawOptions)(void *userData, int index),
		int (*compareDrawOptions)(void *userData, int cur_index, int next_index),
		void *userData)
{
	emDM_drawFacesTex_common(dm, NULL, setDrawOptions, compareDrawOptions, userData);
}

static void emDM_drawMappedFacesGLSL(
		DerivedMesh *dm,
		int (*setMaterial)(int, void *attribs),
		int (*setDrawOptions)(void *userData, int index),
		void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	float (*vertexCos)[3]= emdm->vertexCos;
	float (*vertexNos)[3]= emdm->vertexNos;
	EditVert *eve;
	EditFace *efa;
	DMVertexAttribs attribs= {{{0}}};
	GPUVertexAttribs gattribs;
	/* int tfoffset; */ /* UNUSED */
	int i, b, matnr, new_matnr, dodraw /* , layer */ /* UNUSED */;

	dodraw = 0;
	matnr = -1;

	/* layer = CustomData_get_layer_index(&em->fdata, CD_MTFACE); */ /* UNUSED */
	/* tfoffset = (layer == -1)? -1: em->fdata.layers[layer].offset; */ /* UNUSED */

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	for (i=0,eve=em->verts.first; eve; eve= eve->next)
		eve->tmp.l = (intptr_t) i++;

#define PASSATTRIB(efa, eve, vert) {											\
	if (attribs.totorco) {														\
		float *orco = attribs.orco.array[eve->tmp.l];							\
		glVertexAttrib3fvARB(attribs.orco.glIndex, orco);						\
	}																			\
	for (b = 0; b < attribs.tottface; b++) {									\
		MTFace *_tf = (MTFace*)((char*)efa->data + attribs.tface[b].emOffset);	\
		glVertexAttrib2fvARB(attribs.tface[b].glIndex, _tf->uv[vert]);			\
	}																			\
	for (b = 0; b < attribs.totmcol; b++) {										\
		MCol *cp = (MCol*)((char*)efa->data + attribs.mcol[b].emOffset);		\
		GLubyte col[4];															\
		col[0]= cp->b; col[1]= cp->g; col[2]= cp->r; col[3]= cp->a;				\
		glVertexAttrib4ubvARB(attribs.mcol[b].glIndex, col);					\
	}																			\
	if (attribs.tottang) {														\
		float *tang = attribs.tang.array[i*4 + vert];							\
		glVertexAttrib4fvARB(attribs.tang.glIndex, tang);						\
	}																			\
}

	for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
		int drawSmooth= (efa->flag & ME_SMOOTH);

		if (setDrawOptions && !setDrawOptions(userData, i))
			continue;

		new_matnr = efa->mat_nr + 1;
		if (new_matnr != matnr) {
			dodraw = setMaterial(matnr = new_matnr, &gattribs);
			if (dodraw)
				DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);
		}

		if (dodraw) {
			glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
			if (!drawSmooth) {
				if (vertexCos) glNormal3fv(emdm->faceNos[i]);
				else glNormal3fv(efa->n);

				PASSATTRIB(efa, efa->v1, 0);
				if (vertexCos) glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
				else glVertex3fv(efa->v1->co);

				PASSATTRIB(efa, efa->v2, 1);
				if (vertexCos) glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
				else glVertex3fv(efa->v2->co);

				PASSATTRIB(efa, efa->v3, 2);
				if (vertexCos) glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
				else glVertex3fv(efa->v3->co);

				if (efa->v4) {
					PASSATTRIB(efa, efa->v4, 3);
					if (vertexCos) glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					else glVertex3fv(efa->v4->co);
				}
			}
			else {
				PASSATTRIB(efa, efa->v1, 0);
				if (vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v1->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
				}
				else {
					glNormal3fv(efa->v1->no);
					glVertex3fv(efa->v1->co);
				}

				PASSATTRIB(efa, efa->v2, 1);
				if (vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v2->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
				}
				else {
					glNormal3fv(efa->v2->no);
					glVertex3fv(efa->v2->co);
				}

				PASSATTRIB(efa, efa->v3, 2);
				if (vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v3->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
				}
				else {
					glNormal3fv(efa->v3->no);
					glVertex3fv(efa->v3->co);
				}

				if (efa->v4) {
					PASSATTRIB(efa, efa->v4, 3);
					if (vertexCos) {
						glNormal3fv(vertexNos[(int) efa->v4->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
					else {
						glNormal3fv(efa->v4->no);
						glVertex3fv(efa->v4->co);
					}
				}
			}
			glEnd();
		}
	}
#undef PASSATTRIB
}

static void emDM_drawFacesGLSL(
		DerivedMesh *dm,
		int (*setMaterial)(int, void *attribs))
{
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void emDM_drawMappedFacesMat(
		DerivedMesh *dm,
		void (*setMaterial)(void *userData, int, void *attribs),
		int (*setFace)(void *userData, int index), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	float (*vertexCos)[3]= emdm->vertexCos;
	float (*vertexNos)[3]= emdm->vertexNos;
	EditVert *eve;
	EditFace *efa;
	DMVertexAttribs attribs= {{{0}}};
	GPUVertexAttribs gattribs;
	int i, b, matnr, new_matnr;

	matnr = -1;

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	for (i=0,eve=em->verts.first; eve; eve= eve->next)
		eve->tmp.l = (intptr_t) i++;

#define PASSATTRIB(efa, eve, vert) {											\
	if (attribs.totorco) {														\
		float *orco = attribs.orco.array[eve->tmp.l];							\
		if (attribs.orco.glTexco)												\
			glTexCoord3fv(orco);												\
		else																	\
			glVertexAttrib3fvARB(attribs.orco.glIndex, orco);					\
	}																			\
	for (b = 0; b < attribs.tottface; b++) {									\
		MTFace *_tf = (MTFace*)((char*)efa->data + attribs.tface[b].emOffset);	\
		if (attribs.tface[b].glTexco)											\
			glTexCoord2fv(_tf->uv[vert]);										\
		else																	\
			glVertexAttrib2fvARB(attribs.tface[b].glIndex, _tf->uv[vert]);		\
	}																			\
	for (b = 0; b < attribs.totmcol; b++) {										\
		MCol *cp = (MCol*)((char*)efa->data + attribs.mcol[b].emOffset);		\
		GLubyte col[4];															\
		col[0]= cp->b; col[1]= cp->g; col[2]= cp->r; col[3]= cp->a;				\
		glVertexAttrib4ubvARB(attribs.mcol[b].glIndex, col);					\
	}																			\
	if (attribs.tottang) {														\
		float *tang = attribs.tang.array[i*4 + vert];							\
		glVertexAttrib4fvARB(attribs.tang.glIndex, tang);						\
	}																			\
}

	for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
		int drawSmooth= (efa->flag & ME_SMOOTH);

		/* face hiding */
		if (setFace && !setFace(userData, i))
			continue;

		/* material */
		new_matnr = efa->mat_nr + 1;
		if (new_matnr != matnr) {
			setMaterial(userData, matnr = new_matnr, &gattribs);
			DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);
		}

		/* face */
		glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
		if (!drawSmooth) {
			if (vertexCos) glNormal3fv(emdm->faceNos[i]);
			else glNormal3fv(efa->n);

			PASSATTRIB(efa, efa->v1, 0);
			if (vertexCos) glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
			else glVertex3fv(efa->v1->co);

			PASSATTRIB(efa, efa->v2, 1);
			if (vertexCos) glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
			else glVertex3fv(efa->v2->co);

			PASSATTRIB(efa, efa->v3, 2);
			if (vertexCos) glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
			else glVertex3fv(efa->v3->co);

			if (efa->v4) {
				PASSATTRIB(efa, efa->v4, 3);
				if (vertexCos) glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
				else glVertex3fv(efa->v4->co);
			}
		}
		else {
			PASSATTRIB(efa, efa->v1, 0);
			if (vertexCos) {
				glNormal3fv(vertexNos[(int) efa->v1->tmp.l]);
				glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
			}
			else {
				glNormal3fv(efa->v1->no);
				glVertex3fv(efa->v1->co);
			}

			PASSATTRIB(efa, efa->v2, 1);
			if (vertexCos) {
				glNormal3fv(vertexNos[(int) efa->v2->tmp.l]);
				glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
			}
			else {
				glNormal3fv(efa->v2->no);
				glVertex3fv(efa->v2->co);
			}

			PASSATTRIB(efa, efa->v3, 2);
			if (vertexCos) {
				glNormal3fv(vertexNos[(int) efa->v3->tmp.l]);
				glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
			}
			else {
				glNormal3fv(efa->v3->no);
				glVertex3fv(efa->v3->co);
			}

			if (efa->v4) {
				PASSATTRIB(efa, efa->v4, 3);
				if (vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v4->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
				}
				else {
					glNormal3fv(efa->v4->no);
					glVertex3fv(efa->v4->co);
				}
			}
		}
		glEnd();
	}
#undef PASSATTRIB
}

static void emDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	int i;

	if (emdm->em->verts.first) {
		for (i=0,eve= emdm->em->verts.first; eve; i++,eve= eve->next) {
			if (emdm->vertexCos) {
				DO_MINMAX(emdm->vertexCos[i], min_r, max_r);
			}
			else {
				DO_MINMAX(eve->co, min_r, max_r);
			}
		}
	}
	else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}
static int emDM_getNumVerts(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->verts);
}

static int emDM_getNumEdges(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->edges);
}

static int emDM_getNumFaces(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->faces);
}

static void emDM_getVertCos(DerivedMesh *dm, float (*cos_r)[3])
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	int i;

	for (i=0,eve= emdm->em->verts.first; eve; i++,eve=eve->next) {
		if (emdm->vertexCos) {
			copy_v3_v3(cos_r[i], emdm->vertexCos[i]);
		}
		else {
			copy_v3_v3(cos_r[i], eve->co);
		}
	}
}

static void emDM_getVert(DerivedMesh *dm, int index, MVert *vert_r)
{
	EditVert *ev = ((EditMeshDerivedMesh *)dm)->em->verts.first;
	int i;

	for (i = 0; i < index; ++i) ev = ev->next;

	copy_v3_v3(vert_r->co, ev->co);

	normal_float_to_short_v3(vert_r->no, ev->no);

	/* TODO what to do with vert_r->flag? */
	vert_r->bweight = (unsigned char) (ev->bweight*255.0f);
}

static void emDM_getEdge(DerivedMesh *dm, int index, MEdge *edge_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditEdge *ee = em->edges.first;
	EditVert *ev, *v1, *v2;
	int i;

	for (i = 0; i < index; ++i) ee = ee->next;

	edge_r->crease = (unsigned char) (ee->crease*255.0f);
	edge_r->bweight = (unsigned char) (ee->bweight*255.0f);
	/* TODO what to do with edge_r->flag? */
	edge_r->flag = ME_EDGEDRAW|ME_EDGERENDER;
	if (ee->seam) edge_r->flag |= ME_SEAM;
	if (ee->sharp) edge_r->flag |= ME_SHARP;
#if 0
	/* this needs setup of f2 field */
	if (!ee->f2) edge_r->flag |= ME_LOOSEEDGE;
#endif

	/* goddamn, we have to search all verts to find indices */
	v1 = ee->v1;
	v2 = ee->v2;
	for (i = 0, ev = em->verts.first; v1 || v2; i++, ev = ev->next) {
		if (ev == v1) {
			edge_r->v1 = i;
			v1 = NULL;
		}
		if (ev == v2) {
			edge_r->v2 = i;
			v2 = NULL;
		}
	}
}

static void emDM_getFace(DerivedMesh *dm, int index, MFace *face_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditFace *ef = em->faces.first;
	EditVert *ev, *v1, *v2, *v3, *v4;
	int i;

	for (i = 0; i < index; ++i) ef = ef->next;

	face_r->mat_nr = ef->mat_nr;
	face_r->flag = ef->flag;

	/* goddamn, we have to search all verts to find indices */
	v1 = ef->v1;
	v2 = ef->v2;
	v3 = ef->v3;
	v4 = ef->v4;
	if (!v4) face_r->v4 = 0;

	for (i = 0, ev = em->verts.first; v1 || v2 || v3 || v4;
		i++, ev = ev->next) {
		if (ev == v1) {
			face_r->v1 = i;
			v1 = NULL;
		}
		if (ev == v2) {
			face_r->v2 = i;
			v2 = NULL;
		}
		if (ev == v3) {
			face_r->v3 = i;
			v3 = NULL;
		}
		if (ev == v4) {
			face_r->v4 = i;
			v4 = NULL;
		}
	}

	test_index_face(face_r, NULL, 0, ef->v4?4:3);
}

static void emDM_copyVertArray(DerivedMesh *dm, MVert *vert_r)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *ev = emdm->em->verts.first;
	int i;

	for (i=0; ev; ev = ev->next, ++vert_r, ++i) {
		if (emdm->vertexCos)
			copy_v3_v3(vert_r->co, emdm->vertexCos[i]);
		else
			copy_v3_v3(vert_r->co, ev->co);

		normal_float_to_short_v3(vert_r->no, ev->no);

		/* TODO what to do with vert_r->flag? */
		vert_r->flag = 0;
		vert_r->bweight = (unsigned char) (ev->bweight*255.0f);
	}
}

static void emDM_copyEdgeArray(DerivedMesh *dm, MEdge *edge_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditEdge *ee = em->edges.first;
	EditVert *ev;
	int i;

	/* store vertex indices in tmp union */
	for (ev = em->verts.first, i = 0; ev; ev = ev->next, ++i)
		ev->tmp.l = (intptr_t) i;

	for ( ; ee; ee = ee->next, ++edge_r) {
		edge_r->crease = (unsigned char) (ee->crease*255.0f);
		edge_r->bweight = (unsigned char) (ee->bweight*255.0f);
		/* TODO what to do with edge_r->flag? */
		edge_r->flag = ME_EDGEDRAW|ME_EDGERENDER;
		if (ee->seam) edge_r->flag |= ME_SEAM;
		if (ee->sharp) edge_r->flag |= ME_SHARP;
#if 0
		/* this needs setup of f2 field */
		if (!ee->f2) edge_r->flag |= ME_LOOSEEDGE;
#endif

		edge_r->v1 = (int)ee->v1->tmp.l;
		edge_r->v2 = (int)ee->v2->tmp.l;
	}
}

static void emDM_copyFaceArray(DerivedMesh *dm, MFace *face_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditFace *ef = em->faces.first;
	EditVert *ev;
	int i;

	/* store vertexes indices in tmp union */
	for (ev = em->verts.first, i = 0; ev; ev = ev->next, ++i)
		ev->tmp.l = (intptr_t) i;

	for ( ; ef; ef = ef->next, ++face_r) {
		face_r->mat_nr = ef->mat_nr;
		face_r->flag = ef->flag;

		face_r->v1 = (int)ef->v1->tmp.l;
		face_r->v2 = (int)ef->v2->tmp.l;
		face_r->v3 = (int)ef->v3->tmp.l;
		if (ef->v4) face_r->v4 = (int)ef->v4->tmp.l;
		else face_r->v4 = 0;

		test_index_face(face_r, NULL, 0, ef->v4?4:3);
	}
}

static void *emDM_getFaceDataArray(DerivedMesh *dm, int type)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	EditFace *efa;
	char *data, *emdata;
	void *datalayer;
	int index, size;

	datalayer = DM_get_face_data_layer(dm, type);
	if (datalayer)
		return datalayer;

	/* layers are store per face for editmesh, we convert to a temporary
	 * data layer array in the derivedmesh when these are requested */
	if (type == CD_MTFACE || type == CD_MCOL) {
		index = CustomData_get_layer_index(&em->fdata, type);

		if (index != -1) {
			/* int offset = em->fdata.layers[index].offset; */ /* UNUSED */
			size = CustomData_sizeof(type);

			DM_add_face_layer(dm, type, CD_CALLOC, NULL);
			index = CustomData_get_layer_index(&dm->faceData, type);
			dm->faceData.layers[index].flag |= CD_FLAG_TEMPORARY;

			data = datalayer = DM_get_face_data_layer(dm, type);
			for (efa=em->faces.first; efa; efa=efa->next, data+=size) {
				emdata = CustomData_em_get(&em->fdata, efa->data, type);
				memcpy(data, emdata, size);
			}
		}
	}

	return datalayer;
}

static void emDM_release(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	if (DM_release(dm)) {
		if (emdm->vertexCos) {
			MEM_freeN(emdm->vertexCos);
			MEM_freeN(emdm->vertexNos);
			MEM_freeN(emdm->faceNos);
		}

		MEM_freeN(emdm);
	}
}

DerivedMesh *editmesh_get_derived(
		EditMesh *em,
		float (*vertexCos)[3])
{
	EditMeshDerivedMesh *emdm = MEM_callocN(sizeof(*emdm), "emdm");

	DM_init(&emdm->dm, DM_TYPE_EDITMESH, BLI_countlist(&em->verts),
					 BLI_countlist(&em->edges), BLI_countlist(&em->faces));

	emdm->dm.getMinMax = emDM_getMinMax;

	emdm->dm.getNumVerts = emDM_getNumVerts;
	emdm->dm.getNumEdges = emDM_getNumEdges;
	emdm->dm.getNumFaces = emDM_getNumFaces;

	emdm->dm.getVertCos = emDM_getVertCos;

	emdm->dm.getVert = emDM_getVert;
	emdm->dm.getEdge = emDM_getEdge;
	emdm->dm.getFace = emDM_getFace;
	emdm->dm.copyVertArray = emDM_copyVertArray;
	emdm->dm.copyEdgeArray = emDM_copyEdgeArray;
	emdm->dm.copyFaceArray = emDM_copyFaceArray;
	emdm->dm.getFaceDataArray = emDM_getFaceDataArray;

	emdm->dm.foreachMappedVert = emDM_foreachMappedVert;
	emdm->dm.foreachMappedEdge = emDM_foreachMappedEdge;
	emdm->dm.foreachMappedFaceCenter = emDM_foreachMappedFaceCenter;

	emdm->dm.drawEdges = emDM_drawEdges;
	emdm->dm.drawMappedEdges = emDM_drawMappedEdges;
	emdm->dm.drawMappedEdgesInterp = emDM_drawMappedEdgesInterp;
	emdm->dm.drawMappedFaces = emDM_drawMappedFaces;
	emdm->dm.drawMappedFacesTex = emDM_drawMappedFacesTex;
	emdm->dm.drawMappedFacesGLSL = emDM_drawMappedFacesGLSL;
	emdm->dm.drawFacesTex = emDM_drawFacesTex;
	emdm->dm.drawFacesGLSL = emDM_drawFacesGLSL;
	emdm->dm.drawMappedFacesMat = emDM_drawMappedFacesMat;
	emdm->dm.drawUVEdges = emDM_drawUVEdges;

	emdm->dm.release = emDM_release;

	emdm->em = em;
	emdm->vertexCos = vertexCos;

	if (CustomData_has_layer(&em->vdata, CD_MDEFORMVERT)) {
		EditVert *eve;
		int i;

		DM_add_vert_layer(&emdm->dm, CD_MDEFORMVERT, CD_CALLOC, NULL);

		for (eve = em->verts.first, i = 0; eve; eve = eve->next, ++i)
			DM_set_vert_data(&emdm->dm, i, CD_MDEFORMVERT,
							 CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT));
	}

	if (vertexCos) {
		EditVert *eve;
		EditFace *efa;
		int totface = BLI_countlist(&em->faces);
		int i;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		emdm->vertexNos = MEM_callocN(sizeof(*emdm->vertexNos)*i, "emdm_vno");
		emdm->faceNos = MEM_mallocN(sizeof(*emdm->faceNos)*totface, "emdm_vno");

		for (i=0, efa= em->faces.first; efa; i++, efa=efa->next) {
			float *v1 = vertexCos[(int) efa->v1->tmp.l];
			float *v2 = vertexCos[(int) efa->v2->tmp.l];
			float *v3 = vertexCos[(int) efa->v3->tmp.l];
			float *no = emdm->faceNos[i];

			if (efa->v4) {
				float *v4 = vertexCos[(int) efa->v4->tmp.l];

				normal_quad_v3( no,v1, v2, v3, v4);
				add_v3_v3(emdm->vertexNos[(int) efa->v4->tmp.l], no);
			}
			else {
				normal_tri_v3( no,v1, v2, v3);
			}

			add_v3_v3(emdm->vertexNos[(int) efa->v1->tmp.l], no);
			add_v3_v3(emdm->vertexNos[(int) efa->v2->tmp.l], no);
			add_v3_v3(emdm->vertexNos[(int) efa->v3->tmp.l], no);
		}

		for (i=0, eve= em->verts.first; eve; i++, eve=eve->next) {
			float *no = emdm->vertexNos[i];
			/* following Mesh convention; we use vertex coordinate itself
			 * for normal in this case */
			if (normalize_v3(no) == 0.0f) {
				normalize_v3_v3(no, vertexCos[i]);
			}
		}
	}

	return (DerivedMesh*) emdm;
}

