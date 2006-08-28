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

/* TODO maybe BIF_gl.h should include string.h? */
#include <string.h>
#include "BIF_gl.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include <string.h>
#include <limits.h>


/**************** DerivedMesh interface functions ****************/
static int cdDM_getNumVerts(DerivedMesh *dm)
{
	return dm->vertData.numElems;
}

static int cdDM_getNumFaces(DerivedMesh *dm)
{
	return dm->faceData.numElems;
}

static int cdDM_getNumEdges(DerivedMesh *dm)
{
	return dm->edgeData.numElems;
}

static void cdDM_getVert(DerivedMesh *dm, int index, MVert *vert_r)
{
	*vert_r = *CDDM_get_vert(dm, index);
}

static void cdDM_getEdge(DerivedMesh *dm, int index, MEdge *edge_r)
{
	*edge_r = *CDDM_get_edge(dm, index);
}

static void cdDM_getFace(DerivedMesh *dm, int index, MFace *face_r)
{
	*face_r = *CDDM_get_face(dm, index);
}

static void cdDM_getVertArray(DerivedMesh *dm, MVert *vert_r)
{
	memcpy(vert_r, CDDM_get_verts(dm), sizeof(*vert_r) * dm->getNumVerts(dm));
}

static void cdDM_getEdgeArray(DerivedMesh *dm, MEdge *edge_r)
{
	memcpy(edge_r, CDDM_get_edges(dm), sizeof(*edge_r) * dm->getNumEdges(dm));
}

static void cdDM_getFaceArray(DerivedMesh *dm, MFace *face_r)
{
	memcpy(face_r, CDDM_get_faces(dm), sizeof(*face_r) * dm->getNumFaces(dm));
}

static void cdDM_foreachMappedVert(
                           DerivedMesh *dm,
                           void (*func)(void *userData, int index, float *co,
                                        float *no_f, short *no_s),
                           void *userData)
{
	int i;
	int maxVerts = dm->getNumVerts(dm);
	MVert *mv = CDDM_get_verts(dm);
	int *index = DM_get_vert_data_layer(dm, LAYERTYPE_ORIGINDEX);

	for(i = 0; i < maxVerts; i++, mv++, index++) {
		if(*index == ORIGINDEX_NONE) continue;

		func(userData, *index, mv->co, NULL, mv->no);
	}
}

static void cdDM_foreachMappedEdge(
                           DerivedMesh *dm,
                           void (*func)(void *userData, int index,
                                        float *v0co, float *v1co),
                           void *userData)
{
	int i;
	int maxEdges = dm->getNumEdges(dm);
	MEdge *med = CDDM_get_edges(dm);
	MVert *mv = CDDM_get_verts(dm);
	int *index = DM_get_edge_data_layer(dm, LAYERTYPE_ORIGINDEX);

	for(i = 0; i < maxEdges; i++, med++, index++) {
		if(*index == ORIGINDEX_NONE) continue;

		func(userData, *index, mv[med->v1].co, mv[med->v2].co);
	}
}

static void cdDM_foreachMappedFaceCenter(
                           DerivedMesh *dm,
                           void (*func)(void *userData, int index,
                                        float *cent, float *no),
                           void *userData)
{
	int i;
	int maxFaces = dm->getNumFaces(dm);
	MFace *mf = CDDM_get_faces(dm);
	MVert *mv = CDDM_get_verts(dm);
	int *index = DM_get_face_data_layer(dm, LAYERTYPE_ORIGINDEX);

	for(i = 0; i < maxFaces; i++, mf++, index++) {
		float cent[3];
		float no[3];

		if(*index == ORIGINDEX_NONE) continue;

		VECCOPY(cent, mv[mf->v1].co);
		VecAddf(cent, cent, mv[mf->v2].co);
		VecAddf(cent, cent, mv[mf->v3].co);

		if (mf->v4) {
			CalcNormFloat4(mv[mf->v1].co, mv[mf->v2].co,
			               mv[mf->v3].co, mv[mf->v4].co, no);
			VecAddf(cent, cent, mv[mf->v4].co);
			VecMulf(cent, 0.25f);
		} else {
			CalcNormFloat(mv[mf->v1].co, mv[mf->v2].co,
			              mv[mf->v3].co, no);
			VecMulf(cent, 0.33333333333f);
		}

		func(userData, *index, cent, no);
	}
}

static DispListMesh *cdDM_convertToDispListMesh(DerivedMesh *dm,
                                                int allowShared)
{
	DispListMesh *dlm = MEM_callocN(sizeof(*dlm),
	                                "cdDM_convertToDispListMesh dlm");

	dlm->totvert = dm->vertData.numElems;
	dlm->totedge = dm->edgeData.numElems;
	dlm->totface = dm->faceData.numElems;
	dlm->mvert = dm->dupVertArray(dm);
	dlm->medge = dm->dupEdgeArray(dm);
	dlm->mface = dm->dupFaceArray(dm);

	dlm->tface = dm->getFaceDataArray(dm, LAYERTYPE_TFACE);
	if(dlm->tface)
		dlm->tface = MEM_dupallocN(dlm->tface);

	dlm->mcol = dm->getFaceDataArray(dm, LAYERTYPE_MCOL);
	if(dlm->mcol)
		dlm->mcol = MEM_dupallocN(dlm->mcol);

	dlm->nors = NULL;
	dlm->dontFreeVerts = dlm->dontFreeOther = dlm->dontFreeNors = 0;

	return dlm;
}

static void cdDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	int i;

	for(i = 0; i < dm->vertData.numElems; i++) {
		DO_MINMAX(CDDM_get_vert(dm, i)->co, min_r, max_r);
	}
}

static void cdDM_getVertCo(DerivedMesh *dm, int index, float co_r[3])
{
	VECCOPY(co_r, CDDM_get_vert(dm, index)->co);
}

static void cdDM_getVertCos(DerivedMesh *dm, float (*cos_r)[3])
{
	int i;
	MVert *mv = CDDM_get_verts(dm);

	for(i = 0; i < dm->vertData.numElems; i++, mv++)
		VECCOPY(cos_r[i], mv->co);
}

static void cdDM_getVertNo(DerivedMesh *dm, int index, float no_r[3])
{
	short *no = CDDM_get_vert(dm, index)->no;

	no_r[0] = no[0] / 32767.f;
	no_r[1] = no[1] / 32767.f;
	no_r[2] = no[2] / 32767.f;
}

static void cdDM_drawVerts(DerivedMesh *dm)
{
	int i;
	MVert *mv = CDDM_get_verts(dm);

	glBegin(GL_POINTS);
	for(i = 0; i < dm->vertData.numElems; i++, mv++)
		glVertex3fv(mv->co);
	glEnd();
}

static void cdDM_drawUVEdges(DerivedMesh *dm)
{
	int i;
	TFace *tf = DM_get_face_data_layer(dm, LAYERTYPE_TFACE);
	MFace *mf = CDDM_get_faces(dm);

	if(tf) {
		glBegin(GL_LINES);
		for(i = 0; i < dm->faceData.numElems; i++, tf++, mf++) {
			if(!(tf->flag&TF_HIDE)) {
				glVertex2fv(tf->uv[0]);
				glVertex2fv(tf->uv[1]);

				glVertex2fv(tf->uv[1]);
				glVertex2fv(tf->uv[2]);

				if(!mf->v4) {
					glVertex2fv(tf->uv[2]);
					glVertex2fv(tf->uv[0]);
				} else {
					glVertex2fv(tf->uv[2]);
					glVertex2fv(tf->uv[3]);

					glVertex2fv(tf->uv[3]);
					glVertex2fv(tf->uv[0]);
				}
			}
		}
		glEnd();
	}
}

static void cdDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	int i;
	MEdge *medge = CDDM_get_edges(dm);
	MVert *mvert = CDDM_get_verts(dm);
		
	glBegin(GL_LINES);
	for(i = 0; i < dm->edgeData.numElems; i++, medge++) {
		if((medge->flag&ME_EDGEDRAW)
		   && (drawLooseEdges || !(medge->flag&ME_LOOSEEDGE))) {
			glVertex3fv(mvert[medge->v1].co);
			glVertex3fv(mvert[medge->v2].co);
		}
	}
	glEnd();
}

static void cdDM_drawLooseEdges(DerivedMesh *dm)
{
	MEdge *medge = CDDM_get_edges(dm);
	MVert *mvert = CDDM_get_verts(dm);
	int i;

	glBegin(GL_LINES);
	for(i = 0; i < dm->edgeData.numElems; i++, medge++) {
		if(medge->flag&ME_LOOSEEDGE) {
			glVertex3fv(mvert[medge->v1].co);
			glVertex3fv(mvert[medge->v2].co);
		}
	}
	glEnd();
}

static void cdDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int))
{
	int a;
	int glmode = -1, shademodel = -1, matnr = -1, drawCurrentMat = 1;
	MFace *mface = CDDM_get_faces(dm);
	MVert *mvert = CDDM_get_verts(dm);

#define PASSVERT(index) {						\
	if(shademodel == GL_SMOOTH) {				\
		short *no = mvert[index].no;			\
		glNormal3sv(no);						\
	}											\
	glVertex3fv(mvert[index].co);	\
}

	glBegin(glmode = GL_QUADS);
	for(a = 0; a < dm->faceData.numElems; a++, mface++) {
		int new_glmode, new_matnr, new_shademodel;

		new_glmode = mface->v4?GL_QUADS:GL_TRIANGLES;
		new_matnr = mface->mat_nr + 1;
		new_shademodel = (mface->flag & ME_SMOOTH)?GL_SMOOTH:GL_FLAT;
		
		if(new_glmode != glmode || new_matnr != matnr
		   || new_shademodel != shademodel) {
			glEnd();

			drawCurrentMat = setMaterial(matnr = new_matnr);

			glShadeModel(shademodel = new_shademodel);
			glBegin(glmode = new_glmode);
		} 
		
		if(drawCurrentMat) {
			/* TODO make this better (cache facenormals as layer?) */
			if(shademodel == GL_FLAT) {
				float nor[3];
				if(mface->v4) {
					CalcNormFloat4(mvert[mface->v1].co, mvert[mface->v2].co,
					               mvert[mface->v3].co, mvert[mface->v4].co,
					               nor);
				} else {
					CalcNormFloat(mvert[mface->v1].co, mvert[mface->v2].co,
					              mvert[mface->v3].co, nor);
				}
				glNormal3fv(nor);
			}

			PASSVERT(mface->v1);
			PASSVERT(mface->v2);
			PASSVERT(mface->v3);
			if(mface->v4) {
				PASSVERT(mface->v4);
			}
		}
	}
	glEnd();

	glShadeModel(GL_FLAT);
#undef PASSVERT
}

static void cdDM_drawFacesColored(DerivedMesh *dm, int useTwoSided, unsigned char *col1, unsigned char *col2)
{
	int a, glmode;
	unsigned char *cp1, *cp2;
	MFace *mface = CDDM_get_faces(dm);
	MVert *mvert = CDDM_get_verts(dm);

	cp1 = col1;
	if(col2) {
		cp2 = col2;
	} else {
		cp2 = NULL;
		useTwoSided = 0;
	}

	/* there's a conflict here... twosided colors versus culling...? */
	/* defined by history, only texture faces have culling option */
	/* we need that as mesh option builtin, next to double sided lighting */
	if(col1 && col2)
		glEnable(GL_CULL_FACE);
	
	glShadeModel(GL_SMOOTH);
	glBegin(glmode = GL_QUADS);
	for(a = 0; a < dm->faceData.numElems; a++, mface++, cp1 += 16) {
		int new_glmode = mface->v4?GL_QUADS:GL_TRIANGLES;

		if(new_glmode != glmode) {
			glEnd();
			glBegin(glmode = new_glmode);
		}
			
		glColor3ub(cp1[3], cp1[2], cp1[1]);
		glVertex3fv(mvert[mface->v1].co);
		glColor3ub(cp1[7], cp1[6], cp1[5]);
		glVertex3fv(mvert[mface->v2].co);
		glColor3ub(cp1[11], cp1[10], cp1[9]);
		glVertex3fv(mvert[mface->v3].co);
		if(mface->v4) {
			glColor3ub(cp1[15], cp1[14], cp1[13]);
			glVertex3fv(mvert[mface->v4].co);
		}
			
		if(useTwoSided) {
			glColor3ub(cp2[11], cp2[10], cp2[9]);
			glVertex3fv(mvert[mface->v3].co );
			glColor3ub(cp2[7], cp2[6], cp2[5]);
			glVertex3fv(mvert[mface->v2].co );
			glColor3ub(cp2[3], cp2[2], cp2[1]);
			glVertex3fv(mvert[mface->v1].co );
			if(mface->v4) {
				glColor3ub(cp2[15], cp2[14], cp2[13]);
				glVertex3fv(mvert[mface->v4].co );
			}
		}
		if(col2) cp2 += 16;
	}
	glEnd();

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
}

static void cdDM_drawFacesTex_common(DerivedMesh *dm,
               int (*drawParams)(TFace *tface, int matnr),
               int (*drawParamsMapped)(void *userData, int index),
               void *userData) 
{
	int i;
	MFace *mf = CDDM_get_faces(dm);
	TFace *tf = DM_get_face_data_layer(dm, LAYERTYPE_TFACE);
	MVert *mv = CDDM_get_verts(dm);
	int *index = DM_get_face_data_layer(dm, LAYERTYPE_ORIGINDEX);

	for(i = 0; i < dm->faceData.numElems; i++, mf++, index++) {
		MVert *mvert;
		int flag;
		unsigned char *cp = NULL;

		if(drawParams)
			if(tf) flag = drawParams(&tf[i], mf->mat_nr);
			else flag = drawParams(NULL, mf->mat_nr);
		else if(*index != ORIGINDEX_NONE)
			flag = drawParamsMapped(userData, *index);
		else
			flag = 0;

		if(flag == 0) {
			continue;
		} else if(flag == 1) {
			if(tf) {
				cp = (unsigned char *)tf[i].col;
			} else {
				cp = DM_get_face_data(dm, i, LAYERTYPE_MCOL);
			}
		}

		/* TODO make this better (cache facenormals as layer?) */
		if(!(mf->flag&ME_SMOOTH)) {
			float nor[3];
			if(mf->v4) {
				CalcNormFloat4(mv[mf->v1].co, mv[mf->v2].co,
							   mv[mf->v3].co, mv[mf->v4].co, nor);
			} else {
				CalcNormFloat(mv[mf->v1].co, mv[mf->v2].co,
							  mv[mf->v3].co, nor);
			}
			glNormal3fv(nor);
		}

		glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);
		if(tf) glTexCoord2fv(tf[i].uv[0]);
		if(cp) glColor3ub(cp[3], cp[2], cp[1]);
		mvert = &mv[mf->v1];
		if(mf->flag&ME_SMOOTH) glNormal3sv(mvert->no);
		glVertex3fv(mvert->co);
			
		if(tf) glTexCoord2fv(tf[i].uv[1]);
		if(cp) glColor3ub(cp[7], cp[6], cp[5]);
		mvert = &mv[mf->v2];
		if(mf->flag&ME_SMOOTH) glNormal3sv(mvert->no);
		glVertex3fv(mvert->co);

		if(tf) glTexCoord2fv(tf[i].uv[2]);
		if(cp) glColor3ub(cp[11], cp[10], cp[9]);
		mvert = &mv[mf->v3];
		if(mf->flag&ME_SMOOTH) glNormal3sv(mvert->no);
		glVertex3fv(mvert->co);

		if(mf->v4) {
			if(tf) glTexCoord2fv(tf[i].uv[3]);
			if(cp) glColor3ub(cp[15], cp[14], cp[13]);
			mvert = &mv[mf->v4];
			if(mf->flag&ME_SMOOTH) glNormal3sv(mvert->no);
			glVertex3fv(mvert->co);
		}
		glEnd();
	}
}

static void cdDM_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(TFace *tface, int matnr))
{
	cdDM_drawFacesTex_common(dm, setDrawOptions, NULL, NULL);
}

static void cdDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors)
{
	int i;
	MFace *mf = CDDM_get_faces(dm);
	MVert *mv = CDDM_get_verts(dm);
	int *index = DM_get_face_data_layer(dm, LAYERTYPE_ORIGINDEX);
	TFace *tf = DM_get_face_data_layer(dm, LAYERTYPE_TFACE);
	MCol *mc = DM_get_face_data_layer(dm, LAYERTYPE_MCOL);

	for(i = 0; i < dm->faceData.numElems; i++, mf++, index++) {
		int drawSmooth = (mf->flag & ME_SMOOTH);

		if(setDrawOptions && *index == ORIGINDEX_NONE) continue;

		if(!setDrawOptions || setDrawOptions(userData, *index, &drawSmooth)) {
			unsigned char *cp = NULL;

			if(useColors) {
				if(tf) {
					cp = (unsigned char *)tf[i].col;
				} else if(mc) {
					cp = (unsigned char *)&mc[i * 4];
				}
			}

			glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
			glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);

			if(!drawSmooth) {
				/* TODO make this better (cache facenormals as layer?) */
				float nor[3];
				if(mf->v4) {
					CalcNormFloat4(mv[mf->v1].co, mv[mf->v2].co,
								   mv[mf->v3].co, mv[mf->v4].co, nor);
				} else {
					CalcNormFloat(mv[mf->v1].co, mv[mf->v2].co,
								  mv[mf->v3].co, nor);
				}
				glNormal3fv(nor);

				if(cp) glColor3ub(cp[3], cp[2], cp[1]);
				glVertex3fv(mv[mf->v1].co);
				if(cp) glColor3ub(cp[7], cp[6], cp[5]);
				glVertex3fv(mv[mf->v2].co);
				if(cp) glColor3ub(cp[11], cp[10], cp[9]);
				glVertex3fv(mv[mf->v3].co);
				if(mf->v4) {
					if(cp) glColor3ub(cp[15], cp[14], cp[13]);
					glVertex3fv(mv[mf->v4].co);
				}
			} else {
				if(cp) glColor3ub(cp[3], cp[2], cp[1]);
				glNormal3sv(mv[mf->v1].no);
				glVertex3fv(mv[mf->v1].co);
				if(cp) glColor3ub(cp[7], cp[6], cp[5]);
				glNormal3sv(mv[mf->v2].no);
				glVertex3fv(mv[mf->v2].co);
				if(cp) glColor3ub(cp[11], cp[10], cp[9]);
				glNormal3sv(mv[mf->v3].no);
				glVertex3fv(mv[mf->v3].co);
				if(mf->v4) {
					if(cp) glColor3ub(cp[15], cp[14], cp[13]);
					glNormal3sv(mv[mf->v4].no);
					glVertex3fv(mv[mf->v4].co);
				}
			}

			glEnd();
		}
	}
}

static void cdDM_drawMappedFacesTex(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	cdDM_drawFacesTex_common(dm, NULL, setDrawOptions, userData);
}

static void cdDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	int i;
	int *index = DM_get_edge_data_layer(dm, LAYERTYPE_ORIGINDEX);
	MEdge *edge = CDDM_get_edges(dm);
	MVert *vert = CDDM_get_verts(dm);

	glBegin(GL_LINES);
	for(i = 0; i < dm->edgeData.numElems; i++, edge++, index++) {

		if(setDrawOptions && *index == ORIGINDEX_NONE) continue;

		if(!setDrawOptions || setDrawOptions(userData, *index)) {
			glVertex3fv(vert[edge->v1].co);
			glVertex3fv(vert[edge->v2].co);
		}
	}
	glEnd();
}

static void cdDM_release(DerivedMesh *dm)
{
	CustomData_free(&dm->vertData);
	CustomData_free(&dm->edgeData);
	CustomData_free(&dm->faceData);

	MEM_freeN(dm);
}


/**************** CDDM interface functions ****************/
static DerivedMesh *cdDM_create(const char *desc)
{
	DerivedMesh *dm;

	dm = MEM_callocN(sizeof(*dm), desc);

	dm->getMinMax = cdDM_getMinMax;

	dm->convertToDispListMesh = cdDM_convertToDispListMesh;

	dm->getNumVerts = cdDM_getNumVerts;
	dm->getNumFaces = cdDM_getNumFaces;
	dm->getNumEdges = cdDM_getNumEdges;

	dm->getVert = cdDM_getVert;
	dm->getEdge = cdDM_getEdge;
	dm->getFace = cdDM_getFace;
	dm->getVertArray = cdDM_getVertArray;
	dm->getEdgeArray = cdDM_getEdgeArray;
	dm->getFaceArray = cdDM_getFaceArray;
	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getFaceData = DM_get_face_data;
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getFaceDataArray = DM_get_face_data_layer;

	dm->getVertCos = cdDM_getVertCos;
	dm->getVertCo = cdDM_getVertCo;
	dm->getVertNo = cdDM_getVertNo;

	dm->drawVerts = cdDM_drawVerts;

	dm->drawUVEdges = cdDM_drawUVEdges;
	dm->drawEdges = cdDM_drawEdges;
	dm->drawLooseEdges = cdDM_drawLooseEdges;
	dm->drawMappedEdges = cdDM_drawMappedEdges;

	dm->drawFacesSolid = cdDM_drawFacesSolid;
	dm->drawFacesColored = cdDM_drawFacesColored;
	dm->drawFacesTex = cdDM_drawFacesTex;
	dm->drawMappedFaces = cdDM_drawMappedFaces;
	dm->drawMappedFacesTex = cdDM_drawMappedFacesTex;

	dm->foreachMappedVert = cdDM_foreachMappedVert;
	dm->foreachMappedEdge = cdDM_foreachMappedEdge;
	dm->foreachMappedFaceCenter = cdDM_foreachMappedFaceCenter;

	dm->release = cdDM_release;

	return dm;
}

DerivedMesh *CDDM_new(int numVerts, int numEdges, int numFaces)
{
	DerivedMesh *dm = cdDM_create("CDDM_new dm");
	DM_init(dm, numVerts, numEdges, numFaces);

	CustomData_add_layer(&dm->vertData, LAYERTYPE_MVERT, LAYERFLAG_NOCOPY,
	                     NULL);
	CustomData_add_layer(&dm->edgeData, LAYERTYPE_MEDGE, LAYERFLAG_NOCOPY,
	                     NULL);
	CustomData_add_layer(&dm->faceData, LAYERTYPE_MFACE, LAYERFLAG_NOCOPY,
	                     NULL);

	return dm;
}

DerivedMesh *CDDM_from_mesh(Mesh *mesh)
{
	DerivedMesh *dm = CDDM_new(mesh->totvert, mesh->totedge, mesh->totface);
	int i;

	if(mesh->msticky)
		CustomData_add_layer(&dm->vertData, LAYERTYPE_MSTICKY, 0, NULL);
	if(mesh->dvert)
		CustomData_add_layer(&dm->vertData, LAYERTYPE_MDEFORMVERT, 0, NULL);

	if(mesh->tface)
		CustomData_add_layer(&dm->faceData, LAYERTYPE_TFACE, 0, NULL);
	if(mesh->mcol)
		CustomData_add_layer(&dm->faceData, LAYERTYPE_MCOL, 0, NULL);

	for(i = 0; i < mesh->totvert; ++i) {
		DM_set_vert_data(dm, i, LAYERTYPE_MVERT, &mesh->mvert[i]);
		if(mesh->msticky)
			DM_set_vert_data(dm, i, LAYERTYPE_MSTICKY, &mesh->msticky[i]);
		if(mesh->dvert)
			DM_set_vert_data(dm, i, LAYERTYPE_MDEFORMVERT, &mesh->dvert[i]);

		DM_set_vert_data(dm, i, LAYERTYPE_ORIGINDEX, &i);
	}

	for(i = 0; i < mesh->totedge; ++i) {
		DM_set_edge_data(dm, i, LAYERTYPE_MEDGE, &mesh->medge[i]);

		DM_set_edge_data(dm, i, LAYERTYPE_ORIGINDEX, &i);
	}

	for(i = 0; i < mesh->totface; ++i) {
		DM_set_face_data(dm, i, LAYERTYPE_MFACE, &mesh->mface[i]);
		if(mesh->tface)
			DM_set_face_data(dm, i, LAYERTYPE_TFACE, &mesh->tface[i]);
		if(mesh->mcol)
			DM_set_face_data(dm, i, LAYERTYPE_MCOL, &mesh->mcol[i * 4]);

		DM_set_face_data(dm, i, LAYERTYPE_ORIGINDEX, &i);
	}

	return dm;
}

DerivedMesh *CDDM_from_editmesh(EditMesh *em, Mesh *me)
{
	DerivedMesh *dm = CDDM_new(BLI_countlist(&em->verts),
	                           BLI_countlist(&em->edges),
	                           BLI_countlist(&em->faces));
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int i;
	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = CDDM_get_edges(dm);
	MFace *mface = CDDM_get_faces(dm);
	int *index;

	/* this maps from vert pointer to vert index */
	GHash *vertHash = BLI_ghash_new(BLI_ghashutil_ptrhash,
	                                BLI_ghashutil_ptrcmp);

	for(i = 0, eve = em->verts.first; eve; eve = eve->next, ++i)
		BLI_ghash_insert(vertHash, eve, (void *)i);

	if(me->msticky)
		CustomData_add_layer(&dm->vertData, LAYERTYPE_MDEFORMVERT, 0, NULL);
	if(me->dvert)
		CustomData_add_layer(&dm->vertData, LAYERTYPE_MDEFORMVERT, 0, NULL);

	if(me->tface)
		CustomData_add_layer(&dm->faceData, LAYERTYPE_TFACE, 0, NULL);

	/* Need to be able to mark loose edges */
	for(eed = em->edges.first; eed; eed = eed->next) {
		eed->f2 = 0;
	}
	for(efa = em->faces.first; efa; efa = efa->next) {
		efa->e1->f2 = 1;
		efa->e2->f2 = 1;
		efa->e3->f2 = 1;
		if(efa->e4) efa->e4->f2 = 1;
	}

	index = dm->getVertDataArray(dm, LAYERTYPE_ORIGINDEX);
	for(i = 0, eve = em->verts.first; i < dm->vertData.numElems;
	    i++, eve = eve->next, index++) {
		MVert *mv = &mvert[i];

		VECCOPY(mv->co, eve->co);

		mv->no[0] = eve->no[0] * 32767.0;
		mv->no[1] = eve->no[1] * 32767.0;
		mv->no[2] = eve->no[2] * 32767.0;

		mv->mat_nr = 0;
		mv->flag = 0;

		*index = i;

		if(me->msticky && eve->keyindex != -1)
			DM_set_vert_data(dm, i, LAYERTYPE_MSTICKY,
			                 &me->msticky[eve->keyindex]);
		if(me->dvert && eve->keyindex != -1)
			DM_set_vert_data(dm, i, LAYERTYPE_MDEFORMVERT,
			                 &me->dvert[eve->keyindex]);
	}

	index = dm->getEdgeDataArray(dm, LAYERTYPE_ORIGINDEX);
	for(i = 0, eed = em->edges.first; i < dm->edgeData.numElems;
	    i++, eed = eed->next, index++) {
		MEdge *med = &medge[i];

		med->v1 = (int) BLI_ghash_lookup(vertHash, eed->v1);
		med->v2 = (int) BLI_ghash_lookup(vertHash, eed->v2);
		med->crease = (unsigned char) (eed->crease * 255.0f);
		med->flag = ME_EDGEDRAW|ME_EDGERENDER;
		
		if(eed->seam) med->flag |= ME_SEAM;
		if(eed->sharp) med->flag |= ME_SHARP;
		if(!eed->f2) med->flag |= ME_LOOSEEDGE;

		*index = i;
	}

	index = dm->getFaceDataArray(dm, LAYERTYPE_ORIGINDEX);
	for(i = 0, efa = em->faces.first; i < dm->faceData.numElems;
	    i++, efa = efa->next, index++) {
		MFace *mf = &mface[i];

		mf->v1 = (int) BLI_ghash_lookup(vertHash, efa->v1);
		mf->v2 = (int) BLI_ghash_lookup(vertHash, efa->v2);
		mf->v3 = (int) BLI_ghash_lookup(vertHash, efa->v3);
		mf->v4 = efa->v4 ? (int)BLI_ghash_lookup(vertHash, efa->v4) : 0;
		mf->mat_nr = efa->mat_nr;
		mf->flag = efa->flag;
		test_index_face(mf, NULL, NULL, efa->v4?4:3);

		*index = i;

		if(me->tface)
			DM_set_face_data(dm, i, LAYERTYPE_TFACE, &efa->tf);
	}

	BLI_ghash_free(vertHash, NULL, NULL);

	return dm;
}

DerivedMesh *CDDM_copy(DerivedMesh *source)
{
	DerivedMesh *dest = CDDM_from_template(source,
	                                       source->vertData.numElems,
	                                       source->edgeData.numElems,
	                                       source->faceData.numElems);

	CustomData_copy_data(&source->vertData, &dest->vertData, 0, 0,
	                     source->vertData.numElems);
	CustomData_copy_data(&source->edgeData, &dest->edgeData, 0, 0,
	                     source->edgeData.numElems);
	CustomData_copy_data(&source->faceData, &dest->faceData, 0, 0,
	                     source->faceData.numElems);

	/* copy vert/face/edge data from source */
	source->getVertArray(source, CDDM_get_verts(dest));
	source->getEdgeArray(source, CDDM_get_edges(dest));
	source->getFaceArray(source, CDDM_get_faces(dest));

	return dest;
}

DerivedMesh *CDDM_from_template(DerivedMesh *source,
                                int numVerts, int numEdges, int numFaces)
{
	DerivedMesh *dest = cdDM_create("CDDM_from_template dest");
	DM_from_template(dest, source, numVerts, numEdges, numFaces);

	/* if no vert/face/edge layers in custom data, add them */
	if(!CDDM_get_verts(dest))
		CustomData_add_layer(&dest->vertData, LAYERTYPE_MVERT,
		                     LAYERFLAG_NOCOPY, NULL);
	if(!CDDM_get_edges(dest))
		CustomData_add_layer(&dest->edgeData, LAYERTYPE_MEDGE,
		                     LAYERFLAG_NOCOPY, NULL);
	if(!CDDM_get_faces(dest))
		CustomData_add_layer(&dest->faceData, LAYERTYPE_MFACE,
		                     LAYERFLAG_NOCOPY, NULL);

	return dest;
}

void CDDM_apply_vert_coords(DerivedMesh *dm, float (*vertCoords)[3])
{
	int i;
	MVert *vert = CDDM_get_verts(dm);

	for(i = 0; i < dm->vertData.numElems; ++i, ++vert)
		VECCOPY(vert->co, vertCoords[i]);
}

/* adapted from mesh_calc_normals */
void CDDM_calc_normals(DerivedMesh *dm)
{
	float (*temp_nors)[3];
	float (*face_nors)[3];
	int i;
	int numVerts = dm->getNumVerts(dm);
	int numFaces = dm->getNumFaces(dm);
	MFace *mf;
	MVert *mv = CDDM_get_verts(dm);

	if(!mv) return;

	temp_nors = MEM_callocN(numVerts * sizeof(*temp_nors),
	                        "CDDM_calc_normals temp_nors");
	face_nors = MEM_mallocN(numFaces * sizeof(*face_nors),
	                        "CDDM_calc_normals face_nors");

	mf = CDDM_get_faces(dm);
	for(i = 0; i < numFaces; i++, mf++) {
		float *f_no = face_nors[i];

		if(mf->v4)
			CalcNormFloat4(mv[mf->v1].co, mv[mf->v2].co,
			               mv[mf->v3].co, mv[mf->v4].co, f_no);
		else
			CalcNormFloat(mv[mf->v1].co, mv[mf->v2].co,
			              mv[mf->v3].co, f_no);
		
		VecAddf(temp_nors[mf->v1], temp_nors[mf->v1], f_no);
		VecAddf(temp_nors[mf->v2], temp_nors[mf->v2], f_no);
		VecAddf(temp_nors[mf->v3], temp_nors[mf->v3], f_no);
		if(mf->v4)
			VecAddf(temp_nors[mf->v4], temp_nors[mf->v4], f_no);
	}

	for(i = 0; i < numVerts; i++, mv++) {
		float *no = temp_nors[i];
		
		if (Normalise(no) == 0.0) {
			VECCOPY(no, mv->co);
			Normalise(no);
		}

		mv->no[0] = (short)(no[0] * 32767.0);
		mv->no[1] = (short)(no[1] * 32767.0);
		mv->no[2] = (short)(no[2] * 32767.0);
	}
	
	MEM_freeN(temp_nors);

	/* TODO maybe cache face normals here? */
	MEM_freeN(face_nors);
}

void CDDM_calc_edges(DerivedMesh *dm)
{
	CustomData edgeData;
	EdgeHash *eh = BLI_edgehash_new();
	EdgeHashIterator *ehi;
	int i;
	int maxFaces = dm->getNumFaces(dm);
	MFace *mf = CDDM_get_faces(dm);
	MEdge *med;

	for (i = 0; i < maxFaces; i++, mf++) {
		if (!BLI_edgehash_haskey(eh, mf->v1, mf->v2))
			BLI_edgehash_insert(eh, mf->v1, mf->v2, NULL);
		if (!BLI_edgehash_haskey(eh, mf->v2, mf->v3))
			BLI_edgehash_insert(eh, mf->v2, mf->v3, NULL);
		
		if (mf->v4) {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v4))
				BLI_edgehash_insert(eh, mf->v3, mf->v4, NULL);
			if (!BLI_edgehash_haskey(eh, mf->v4, mf->v1))
				BLI_edgehash_insert(eh, mf->v4, mf->v1, NULL);
		} else {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v1))
				BLI_edgehash_insert(eh, mf->v3, mf->v1, NULL);
		}
	}

	CustomData_from_template(&dm->edgeData, &edgeData, BLI_edgehash_size(eh));

	if(!CustomData_get_layer(&edgeData, LAYERTYPE_MEDGE))
		CustomData_add_layer(&edgeData, LAYERTYPE_MEDGE,
		                     LAYERFLAG_NOCOPY, NULL);

	ehi = BLI_edgehashIterator_new(eh);
	med = CustomData_get_layer(&edgeData, LAYERTYPE_MEDGE);
	for(i = 0; !BLI_edgehashIterator_isDone(ehi);
	    BLI_edgehashIterator_step(ehi), ++i, ++med) {
		BLI_edgehashIterator_getKey(ehi, &med->v1, &med->v2);

		med->flag = ME_EDGEDRAW|ME_EDGERENDER;
	}
	BLI_edgehashIterator_free(ehi);

	CustomData_free(&dm->edgeData);
	dm->edgeData = edgeData;

	BLI_edgehash_free(eh, NULL);
}

void CDDM_set_num_verts(DerivedMesh *dm, int numVerts)
{
	CustomData_set_num_elems(&dm->vertData, numVerts);
}

void CDDM_set_num_edges(DerivedMesh *dm, int numEdges)
{
	CustomData_set_num_elems(&dm->edgeData, numEdges);
}

void CDDM_set_num_faces(DerivedMesh *dm, int numFaces)
{
	CustomData_set_num_elems(&dm->faceData, numFaces);
}

MVert *CDDM_get_vert(DerivedMesh *dm, int index)
{
	return CustomData_get(&dm->vertData, index, LAYERTYPE_MVERT);
}

MEdge *CDDM_get_edge(DerivedMesh *dm, int index)
{
	return CustomData_get(&dm->edgeData, index, LAYERTYPE_MEDGE);
}

MFace *CDDM_get_face(DerivedMesh *dm, int index)
{
	return CustomData_get(&dm->faceData, index, LAYERTYPE_MFACE);
}

MVert *CDDM_get_verts(DerivedMesh *dm)
{
	return CustomData_get_layer(&dm->vertData, LAYERTYPE_MVERT);
}

MEdge *CDDM_get_edges(DerivedMesh *dm)
{
	return CustomData_get_layer(&dm->edgeData, LAYERTYPE_MEDGE);
}

MFace *CDDM_get_faces(DerivedMesh *dm)
{
	return CustomData_get_layer(&dm->faceData, LAYERTYPE_MFACE);
}

