/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <zlib.h>

#include "PIL_time.h"

#include "MEM_guardedalloc.h"

#include "DNA_effect_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h" // N_T
#include "DNA_scene_types.h" // N_T

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"
#include "LBM_fluidsim.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"
#include "BKE_key.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

///////////////////////////////////
///////////////////////////////////

typedef struct {
	DerivedMesh dm;

	Object *ob;
	Mesh *me;
	MVert *verts;
	float *nors;
	MCol *wpaintMCol;

	int freeNors, freeVerts;
} MeshDerivedMesh;

static DispListMesh *meshDM_convertToDispListMesh(DerivedMesh *dm, int allowShared)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	DispListMesh *dlm = MEM_callocN(sizeof(*dlm), "dlm");

	dlm->totvert = me->totvert;
	dlm->totedge = me->totedge;
	dlm->totface = me->totface;
	dlm->mvert = mdm->verts;
	dlm->medge = me->medge;
	dlm->mface = me->mface;
	dlm->tface = me->tface;
	dlm->mcol = me->mcol;
	dlm->nors = mdm->nors;
	dlm->dontFreeVerts = dlm->dontFreeOther = dlm->dontFreeNors = 1;

	if (!allowShared) {
		dlm->mvert = MEM_dupallocN(dlm->mvert);
		if (dlm->nors) dlm->nors = MEM_dupallocN(dlm->nors);

		dlm->dontFreeVerts = dlm->dontFreeNors = 0;
	}

	return dlm;
}

static void meshDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	int i;

	if (me->totvert) {
		for (i=0; i<me->totvert; i++) {
			DO_MINMAX(mdm->verts[i].co, min_r, max_r);
		}
	} else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}

static void meshDM_getVertCos(DerivedMesh *dm, float (*cos_r)[3])
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	int i;

	for (i=0; i<me->totvert; i++) {
		cos_r[i][0] = mdm->verts[i].co[0];
		cos_r[i][1] = mdm->verts[i].co[1];
		cos_r[i][2] = mdm->verts[i].co[2];
	}
}

static void meshDM_getVertCo(DerivedMesh *dm, int index, float co_r[3])
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;

	VECCOPY(co_r, mdm->verts[index].co);
}

static void meshDM_getVertNo(DerivedMesh *dm, int index, float no_r[3])
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	short *no = mdm->verts[index].no;

	no_r[0] = no[0]/32767.f;
	no_r[1] = no[1]/32767.f;
	no_r[2] = no[2]/32767.f;
}

static void meshDM_drawVerts(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	int i;

	glBegin(GL_POINTS);
	for(i=0; i<me->totvert; i++) {
		glVertex3fv(mdm->verts[i].co);
	}
	glEnd();
}
static void meshDM_drawUVEdges(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	int i;

	if (me->tface) {
		glBegin(GL_LINES);
		for (i=0; i<me->totface; i++) {
			TFace *tf = &me->tface[i];

			if (!(tf->flag&TF_HIDE)) {
				glVertex2fv(tf->uv[0]);
				glVertex2fv(tf->uv[1]);

				glVertex2fv(tf->uv[1]);
				glVertex2fv(tf->uv[2]);

				if (!me->mface[i].v4) {
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
static void meshDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me= mdm->me;
	MEdge *medge= me->medge;
	int i;
		
	glBegin(GL_LINES);
	for(i=0; i<me->totedge; i++, medge++) {
		if ((medge->flag&ME_EDGEDRAW) && (drawLooseEdges || !(medge->flag&ME_LOOSEEDGE))) {
			glVertex3fv(mdm->verts[medge->v1].co);
			glVertex3fv(mdm->verts[medge->v2].co);
		}
	}
	glEnd();
}
static void meshDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me= mdm->me;
	int i;
		
	glBegin(GL_LINES);
	for (i=0; i<me->totedge; i++) {
		if (!setDrawOptions || setDrawOptions(userData, i)) {
			glVertex3fv(mdm->verts[me->medge[i].v1].co);
			glVertex3fv(mdm->verts[me->medge[i].v2].co);
		}
	}
	glEnd();
}
static void meshDM_drawLooseEdges(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me= mdm->me;
	MEdge *medge= me->medge;
	int i;

	glBegin(GL_LINES);
	for (i=0; i<me->totedge; i++, medge++) {
		if (medge->flag&ME_LOOSEEDGE) {
			glVertex3fv(mdm->verts[medge->v1].co);
			glVertex3fv(mdm->verts[medge->v2].co);
		}
	}
	glEnd();
}
static void meshDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int))
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	MVert *mvert= mdm->verts;
	MFace *mface= me->mface;
	float *nors = mdm->nors;
	int a;
	int glmode=-1, shademodel=-1, matnr=-1, drawCurrentMat=1;

#define PASSVERT(index) {						\
	if (shademodel==GL_SMOOTH) {				\
		short *no = mvert[index].no;			\
		glNormal3sv(no);						\
	}											\
	glVertex3fv(mvert[index].co);	\
}

	glBegin(glmode=GL_QUADS);
	for(a=0; a<me->totface; a++, mface++, nors+=3) {
		int new_glmode, new_matnr, new_shademodel;
			
		new_glmode = mface->v4?GL_QUADS:GL_TRIANGLES;
		new_matnr = mface->mat_nr+1;
		new_shademodel = (!(me->flag&ME_AUTOSMOOTH) && (mface->flag & ME_SMOOTH))?GL_SMOOTH:GL_FLAT;
		
		if (new_glmode!=glmode || new_matnr!=matnr || new_shademodel!=shademodel) {
			glEnd();

			drawCurrentMat = setMaterial(matnr=new_matnr);

			glShadeModel(shademodel=new_shademodel);
			glBegin(glmode=new_glmode);
		} 
		
		if (drawCurrentMat) {
			if(shademodel==GL_FLAT) 
				glNormal3fv(nors);

			PASSVERT(mface->v1);
			PASSVERT(mface->v2);
			PASSVERT(mface->v3);
			if (mface->v4) {
				PASSVERT(mface->v4);
			}
		}
	}
	glEnd();

	glShadeModel(GL_FLAT);
#undef PASSVERT
}

static void meshDM_drawFacesColored(DerivedMesh *dm, int useTwoSide, unsigned char *col1, unsigned char *col2)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me= mdm->me;
	MFace *mface= me->mface;
	int a, glmode;
	unsigned char *cp1, *cp2;

	cp1= col1;
	if(col2) {
		cp2= col2;
	} else {
		cp2= NULL;
		useTwoSide= 0;
	}

	/* there's a conflict here... twosided colors versus culling...? */
	/* defined by history, only texture faces have culling option */
	/* we need that as mesh option builtin, next to double sided lighting */
	if(col1 && col2)
		glEnable(GL_CULL_FACE);
	
	glShadeModel(GL_SMOOTH);
	glBegin(glmode=GL_QUADS);
	for(a=0; a<me->totface; a++, mface++, cp1+= 16) {
		int new_glmode= mface->v4?GL_QUADS:GL_TRIANGLES;

		if (new_glmode!=glmode) {
			glEnd();
			glBegin(glmode= new_glmode);
		}
			
		glColor3ub(cp1[3], cp1[2], cp1[1]);
		glVertex3fv( mdm->verts[mface->v1].co );
		glColor3ub(cp1[7], cp1[6], cp1[5]);
		glVertex3fv( mdm->verts[mface->v2].co );
		glColor3ub(cp1[11], cp1[10], cp1[9]);
		glVertex3fv( mdm->verts[mface->v3].co );
		if(mface->v4) {
			glColor3ub(cp1[15], cp1[14], cp1[13]);
			glVertex3fv( mdm->verts[mface->v4].co );
		}
			
		if(useTwoSide) {
			glColor3ub(cp2[11], cp2[10], cp2[9]);
			glVertex3fv( mdm->verts[mface->v3].co );
			glColor3ub(cp2[7], cp2[6], cp2[5]);
			glVertex3fv( mdm->verts[mface->v2].co );
			glColor3ub(cp2[3], cp2[2], cp2[1]);
			glVertex3fv( mdm->verts[mface->v1].co );
			if(mface->v4) {
				glColor3ub(cp2[15], cp2[14], cp2[13]);
				glVertex3fv( mdm->verts[mface->v4].co );
			}
		}
		if(col2) cp2+= 16;
	}
	glEnd();

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
}
static void meshDM_drawFacesTex(DerivedMesh *dm, int (*setDrawParams)(TFace *tf, int matnr)) 
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	MVert *mvert= mdm->verts;
	MFace *mface= me->mface;
	TFace *tface = me->tface;
	float *nors = mdm->nors;
	int i;

	for (i=0; i<me->totface; i++) {
		MFace *mf= &mface[i];
		TFace *tf = tface?&tface[i]:NULL;
		int flag;
		unsigned char *cp= NULL;
		
		flag = setDrawParams(tf, mf->mat_nr);

		if (flag==0) {
			continue;
		} else if (flag==1) {
			if (mdm->wpaintMCol) {
				cp= (unsigned char*) &mdm->wpaintMCol[i*4];
			} else if (tf) {
				cp= (unsigned char*) tf->col;
			} else if (me->mcol) {
				cp= (unsigned char*) &me->mcol[i*4];
			}
		}

		if (!(mf->flag&ME_SMOOTH)) {
			glNormal3fv(&nors[i*3]);
		}

		glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);
		if (tf) glTexCoord2fv(tf->uv[0]);
		if (cp) glColor3ub(cp[3], cp[2], cp[1]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v1].no);
		glVertex3fv(mvert[mf->v1].co);
			
		if (tf) glTexCoord2fv(tf->uv[1]);
		if (cp) glColor3ub(cp[7], cp[6], cp[5]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v2].no);
		glVertex3fv(mvert[mf->v2].co);

		if (tf) glTexCoord2fv(tf->uv[2]);
		if (cp) glColor3ub(cp[11], cp[10], cp[9]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v3].no);
		glVertex3fv(mvert[mf->v3].co);

		if(mf->v4) {
			if (tf) glTexCoord2fv(tf->uv[3]);
			if (cp) glColor3ub(cp[15], cp[14], cp[13]);
			if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v4].no);
			glVertex3fv(mvert[mf->v4].co);
		}
		glEnd();
	}
}
static void meshDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors) 
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;
	MVert *mvert= mdm->verts;
	MFace *mface= me->mface;
	float *nors= mdm->nors;
	int i;

	for (i=0; i<me->totface; i++) {
		MFace *mf= &mface[i];
		int drawSmooth = 1;

		if (!setDrawOptions || setDrawOptions(userData, i, &drawSmooth)) {
			unsigned char *cp = NULL;

			if (useColors) {
				if (mdm->wpaintMCol) {
					cp= (unsigned char*) &mdm->wpaintMCol[i*4];
				} else if (me->tface) {
					cp= (unsigned char*) me->tface[i].col;
				} else if (me->mcol) {
					cp= (unsigned char*) &me->mcol[i*4];
				}
			}

			glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
			glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);

			if (!drawSmooth) {
				glNormal3fv(&nors[i*3]);

				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				glVertex3fv(mvert[mf->v1].co);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				glVertex3fv(mvert[mf->v2].co);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				glVertex3fv(mvert[mf->v3].co);
				if(mf->v4) {
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					glVertex3fv(mvert[mf->v4].co);
				}
			} else {
				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				glNormal3sv(mvert[mf->v1].no);
				glVertex3fv(mvert[mf->v1].co);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				glNormal3sv(mvert[mf->v2].no);
				glVertex3fv(mvert[mf->v2].co);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				glNormal3sv(mvert[mf->v3].no);
				glVertex3fv(mvert[mf->v3].co);
				if(mf->v4) {
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					glNormal3sv(mvert[mf->v4].no);
					glVertex3fv(mvert[mf->v4].co);
				}
			}

			glEnd();
		}
	}
}
static int meshDM_getNumVerts(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;

	return me->totvert;
}
static int meshDM_getNumFaces(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->me;

	return me->totface;
}

static void meshDM_release(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;

	if (mdm->wpaintMCol) MEM_freeN(mdm->wpaintMCol);
	if (mdm->freeNors) MEM_freeN(mdm->nors);
	if (mdm->freeVerts) MEM_freeN(mdm->verts);
	MEM_freeN(mdm);
}

static DerivedMesh *getMeshDerivedMesh(Mesh *me, Object *ob, float (*vertCos)[3])
{
	MeshDerivedMesh *mdm = MEM_callocN(sizeof(*mdm), "mdm");

	mdm->dm.getMinMax = meshDM_getMinMax;

	mdm->dm.convertToDispListMesh = meshDM_convertToDispListMesh;
	mdm->dm.getNumVerts = meshDM_getNumVerts;
	mdm->dm.getNumFaces = meshDM_getNumFaces;

	mdm->dm.getVertCos = meshDM_getVertCos;
	mdm->dm.getVertCo = meshDM_getVertCo;
	mdm->dm.getVertNo = meshDM_getVertNo;

	mdm->dm.drawVerts = meshDM_drawVerts;

	mdm->dm.drawUVEdges = meshDM_drawUVEdges;
	mdm->dm.drawEdges = meshDM_drawEdges;
	mdm->dm.drawLooseEdges = meshDM_drawLooseEdges;
	
	mdm->dm.drawFacesSolid = meshDM_drawFacesSolid;
	mdm->dm.drawFacesColored = meshDM_drawFacesColored;
	mdm->dm.drawFacesTex = meshDM_drawFacesTex;
	mdm->dm.drawMappedFaces = meshDM_drawMappedFaces;

	mdm->dm.drawMappedEdges = meshDM_drawMappedEdges;
	mdm->dm.drawMappedFaces = meshDM_drawMappedFaces;

	mdm->dm.release = meshDM_release;

		/* Works in conjunction with hack during modifier calc */
	if ((G.f & G_WEIGHTPAINT) && ob==(G.scene->basact?G.scene->basact->object:NULL)) {
		mdm->wpaintMCol = MEM_dupallocN(me->mcol);
	}

	mdm->ob = ob;
	mdm->me = me;
	mdm->verts = me->mvert;
	mdm->nors = NULL;
	mdm->freeNors = 0;
	mdm->freeVerts = 0;

	if (vertCos) {
		int i;

		mdm->verts = MEM_mallocN(sizeof(*mdm->verts)*me->totvert, "deformedVerts");
		for (i=0; i<me->totvert; i++) {
			VECCOPY(mdm->verts[i].co, vertCos[i]);
		}
		mesh_calc_normals(mdm->verts, me->totvert, me->mface, me->totface, &mdm->nors);
		mdm->freeNors = 1;
		mdm->freeVerts = 1;
	} else {
			// XXX this is kinda hacky because we shouldn't really be editing
			// the mesh here, however, we can't just call mesh_build_faceNormals(ob)
			// because in the case when a key is applied to a mesh the vertex normals
			// would never be correctly computed.
		mesh_calc_normals(mdm->verts, me->totvert, me->mface, me->totface, &mdm->nors);
		mdm->freeNors = 1;
	}

	return (DerivedMesh*) mdm;
}

///

typedef struct {
	DerivedMesh dm;

	EditMesh *em;
	float (*vertexCos)[3];
	float (*vertexNos)[3];
	float (*faceNos)[3];
} EditMeshDerivedMesh;

static void emDM_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	int i;

	for (i=0,eve= emdm->em->verts.first; eve; i++,eve=eve->next) {
		if (emdm->vertexCos) {
			func(userData, i, emdm->vertexCos[i], emdm->vertexNos[i], NULL);
		} else {
			func(userData, i, eve->co, eve->no, NULL);
		}
	}
}
static void emDM_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve, *preveve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next)
			func(userData, i, emdm->vertexCos[(int) eed->v1->prev], emdm->vertexCos[(int) eed->v2->prev]);
		for (preveve=NULL, eve=emdm->em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;
	} else {
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next)
			func(userData, i, eed->v1->co, eed->v2->co);
	}
}
static void emDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve, *preveve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;

		glBegin(GL_LINES);
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(emdm->vertexCos[(int) eed->v1->prev]);
				glVertex3fv(emdm->vertexCos[(int) eed->v2->prev]);
			}
		}
		glEnd();

		for (preveve=NULL, eve=emdm->em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;
	} else {
		glBegin(GL_LINES);
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(eed->v1->co);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}
static void emDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	emDM_drawMappedEdges(dm, NULL, NULL);
}
static void emDM_drawMappedEdgesInterp(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void (*setDrawInterpOptions)(void *userData, int index, float t), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve, *preveve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;

		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(emdm->vertexCos[(int) eed->v1->prev]);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(emdm->vertexCos[(int) eed->v2->prev]);
			}
		}
		glEnd();

		for (preveve=NULL, eve=emdm->em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;
	} else {
		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(eed->v1->co);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}
static void emDM__calcFaceCent(EditFace *efa, float cent[3], float (*vertexCos)[3])
{
	if (vertexCos) {
		VECCOPY(cent, vertexCos[(int) efa->v1->prev]);
		VecAddf(cent, cent, vertexCos[(int) efa->v2->prev]);
		VecAddf(cent, cent, vertexCos[(int) efa->v3->prev]);
		if (efa->v4) VecAddf(cent, cent, vertexCos[(int) efa->v4->prev]);
	} else {
		VECCOPY(cent, efa->v1->co);
		VecAddf(cent, cent, efa->v2->co);
		VecAddf(cent, cent, efa->v3->co);
		if (efa->v4) VecAddf(cent, cent, efa->v4->co);
	}

	if (efa->v4) {
		VecMulf(cent, 0.25f);
	} else {
		VecMulf(cent, 0.33333333333f);
	}
}
static void emDM_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve, *preveve;
	EditFace *efa;
	float cent[3];
	int i;

	if (emdm->vertexCos) {
		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;
	}

	for(i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
		emDM__calcFaceCent(efa, cent, emdm->vertexCos);
		func(userData, i, cent, emdm->vertexCos?emdm->faceNos[i]:efa->n);
	}

	if (emdm->vertexCos) {
		for (preveve=NULL, eve=emdm->em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;
	}
}
static void emDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve, *preveve;
		int drawSmooth = 1;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;

		for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
			if(!setDrawOptions || setDrawOptions(userData, i, &drawSmooth)) {
				glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(emdm->faceNos[i]);
					glVertex3fv(emdm->vertexCos[(int) efa->v1->prev]);
					glVertex3fv(emdm->vertexCos[(int) efa->v2->prev]);
					glVertex3fv(emdm->vertexCos[(int) efa->v3->prev]);
					if(efa->v4) glVertex3fv(emdm->vertexCos[(int) efa->v4->prev]);
				} else {
					glNormal3fv(emdm->vertexNos[(int) efa->v1->prev]);
					glVertex3fv(emdm->vertexCos[(int) efa->v1->prev]);
					glNormal3fv(emdm->vertexNos[(int) efa->v2->prev]);
					glVertex3fv(emdm->vertexCos[(int) efa->v2->prev]);
					glNormal3fv(emdm->vertexNos[(int) efa->v3->prev]);
					glVertex3fv(emdm->vertexCos[(int) efa->v3->prev]);
					if(efa->v4) {
						glNormal3fv(emdm->vertexNos[(int) efa->v4->prev]);
						glVertex3fv(emdm->vertexCos[(int) efa->v4->prev]);
					}
				}
				glEnd();
			}
		}

		for (preveve=NULL, eve=emdm->em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;
	} else {
		int drawSmooth = 1;

		for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
			if(!setDrawOptions || setDrawOptions(userData, i, &drawSmooth)) {
				glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->n);
					glVertex3fv(efa->v1->co);
					glVertex3fv(efa->v2->co);
					glVertex3fv(efa->v3->co);
					if(efa->v4) glVertex3fv(efa->v4->co);
				} else {
					glNormal3fv(efa->v1->no);
					glVertex3fv(efa->v1->co);
					glNormal3fv(efa->v2->no);
					glVertex3fv(efa->v2->co);
					glNormal3fv(efa->v3->no);
					glVertex3fv(efa->v3->co);
					if(efa->v4) {
						glNormal3fv(efa->v4->no);
						glVertex3fv(efa->v4->co);
					}
				}
				glEnd();
			}
		}
	}
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
			} else {
				DO_MINMAX(eve->co, min_r, max_r);
			}
		}
	} else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}
static int emDM_getNumVerts(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->verts);
}
static int emDM_getNumFaces(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->faces);
}

static void emDM_release(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	if (emdm->vertexCos) {
		MEM_freeN(emdm->vertexCos);
		MEM_freeN(emdm->vertexNos);
		MEM_freeN(emdm->faceNos);
	}

	MEM_freeN(emdm);
}

static DerivedMesh *getEditMeshDerivedMesh(EditMesh *em, float (*vertexCos)[3])
{
	EditMeshDerivedMesh *emdm = MEM_callocN(sizeof(*emdm), "emdm");

	emdm->dm.getMinMax = emDM_getMinMax;

	emdm->dm.getNumVerts = emDM_getNumVerts;
	emdm->dm.getNumFaces = emDM_getNumFaces;
	emdm->dm.foreachMappedVert = emDM_foreachMappedVert;
	emdm->dm.foreachMappedEdge = emDM_foreachMappedEdge;
	emdm->dm.foreachMappedFaceCenter = emDM_foreachMappedFaceCenter;

	emdm->dm.drawEdges = emDM_drawEdges;
	emdm->dm.drawMappedEdges = emDM_drawMappedEdges;
	emdm->dm.drawMappedEdgesInterp = emDM_drawMappedEdgesInterp;
	emdm->dm.drawMappedFaces = emDM_drawMappedFaces;

	emdm->dm.release = emDM_release;
	
	emdm->em = em;
	emdm->vertexCos = vertexCos;

	if (vertexCos) {
		EditVert *eve, *preveve;
		EditFace *efa;
		int totface = BLI_countlist(&em->faces);
		int i;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->prev = (EditVert*) i++;

		emdm->vertexNos = MEM_callocN(sizeof(*emdm->vertexNos)*i, "emdm_vno");
		emdm->faceNos = MEM_mallocN(sizeof(*emdm->faceNos)*totface, "emdm_vno");

		for(i=0, efa= em->faces.first; efa; i++, efa=efa->next) {
			float *v1 = vertexCos[(int) efa->v1->prev];
			float *v2 = vertexCos[(int) efa->v2->prev];
			float *v3 = vertexCos[(int) efa->v3->prev];
			float *no = emdm->faceNos[i];
			
			if(efa->v4) {
				float *v4 = vertexCos[(int) efa->v3->prev];

				CalcNormFloat4(v1, v2, v3, v4, no);
				VecAddf(emdm->vertexNos[(int) efa->v4->prev], emdm->vertexNos[(int) efa->v4->prev], no);
			}
			else {
				CalcNormFloat(v1, v2, v3, no);
			}

			VecAddf(emdm->vertexNos[(int) efa->v1->prev], emdm->vertexNos[(int) efa->v1->prev], no);
			VecAddf(emdm->vertexNos[(int) efa->v2->prev], emdm->vertexNos[(int) efa->v2->prev], no);
			VecAddf(emdm->vertexNos[(int) efa->v3->prev], emdm->vertexNos[(int) efa->v3->prev], no);
		}

		for(i=0, eve= em->verts.first; eve; i++, eve=eve->next) {
			float *no = emdm->vertexNos[i];

			if (Normalise(no)==0.0) {
				VECCOPY(no, vertexCos[i]);
				Normalise(no);
			}
		}

		for (preveve=NULL, eve=emdm->em->verts.first; eve; preveve=eve, eve= eve->next)
			eve->prev = preveve;
	}

	return (DerivedMesh*) emdm;
}

///

typedef struct {
	DerivedMesh dm;

	DispListMesh *dlm;
} SSDerivedMesh;

static void ssDM_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	int i, index=-1;

	for (i=0; i<dlm->totvert; i++) {
		MVert *mv = &dlm->mvert[i];

		if (mv->flag&ME_VERT_STEPINDEX) index++;

		if (index!=-1) {
			func(userData, index, mv->co, NULL, mv->no);
		}
	}
}
static void ssDM_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	int i, index=-1;

	for (i=0; i<dlm->totedge; i++) {
		MEdge *med = &dlm->medge[i];

		if (med->flag&ME_EDGE_STEPINDEX) index++;

		if (index!=-1) {
			func(userData, index, dlm->mvert[med->v1].co, dlm->mvert[med->v2].co);
		}
	}
}
static void ssDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) 
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	int i, index=-1;

	glBegin(GL_LINES);
	for(i=0; i<dlm->totedge; i++) {
		MEdge *med = &dlm->medge[i];

		if (med->flag&ME_EDGE_STEPINDEX) index++;

		if (index!=-1 && (!setDrawOptions || setDrawOptions(userData, index))) {
			glVertex3fv(dlm->mvert[med->v1].co);
			glVertex3fv(dlm->mvert[med->v2].co);
		}
	}
	glEnd();
}

static void ssDM_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	int i, index=-1;

	for (i=0; i<dlm->totface; i++) {
		MFace *mf = &dlm->mface[i];

		if (mf->flag&ME_FACE_STEPINDEX) index++;

		if(index!=-1) {
			float cent[3];
			float no[3];

			VECCOPY(cent, dlm->mvert[mf->v1].co);
			VecAddf(cent, cent, dlm->mvert[mf->v2].co);
			VecAddf(cent, cent, dlm->mvert[mf->v3].co);

			if (mf->v4) {
				CalcNormFloat4(dlm->mvert[mf->v1].co, dlm->mvert[mf->v2].co, dlm->mvert[mf->v3].co, dlm->mvert[mf->v4].co, no);
				VecAddf(cent, cent, dlm->mvert[mf->v4].co);
				VecMulf(cent, 0.25f);
			} else {
				CalcNormFloat(dlm->mvert[mf->v1].co, dlm->mvert[mf->v2].co, dlm->mvert[mf->v3].co, no);
				VecMulf(cent, 0.33333333333f);
			}

			func(userData, index, cent, no);
		}
	}
}
static void ssDM_drawVerts(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MVert *mvert= dlm->mvert;
	int i;

	bglBegin(GL_POINTS);
	for (i=0; i<dlm->totvert; i++) {
		bglVertex3fv(mvert[i].co);
	}
	bglEnd();
}
static void ssDM_drawUVEdges(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	int i;

	if (dlm->tface) {
		glBegin(GL_LINES);
		for (i=0; i<dlm->totface; i++) {
			TFace *tf = &dlm->tface[i];

			if (!(tf->flag&TF_HIDE)) {
				glVertex2fv(tf->uv[0]);
				glVertex2fv(tf->uv[1]);

				glVertex2fv(tf->uv[1]);
				glVertex2fv(tf->uv[2]);

				if (!dlm->mface[i].v4) {
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
static void ssDM_drawLooseEdges(DerivedMesh *dm) 
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MVert *mvert = dlm->mvert;
	MEdge *medge= dlm->medge;
	int i;

	glBegin(GL_LINES);
	for (i=0; i<dlm->totedge; i++, medge++) {
		if (medge->flag&ME_LOOSEEDGE) {
			glVertex3fv(mvert[medge->v1].co); 
			glVertex3fv(mvert[medge->v2].co);
		}
	}
	glEnd();
}
static void ssDM_drawEdges(DerivedMesh *dm, int drawLooseEdges) 
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MVert *mvert= dlm->mvert;
	MEdge *medge= dlm->medge;
	int i;
	
	glBegin(GL_LINES);
	for (i=0; i<dlm->totedge; i++, medge++) {
		if ((medge->flag&ME_EDGEDRAW) && (drawLooseEdges || !(medge->flag&ME_LOOSEEDGE))) {
			glVertex3fv(mvert[medge->v1].co); 
			glVertex3fv(mvert[medge->v2].co);
		}
	}
	glEnd();
}
static void ssDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int))
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	float *nors = dlm->nors;
	int glmode=-1, shademodel=-1, matnr=-1, drawCurrentMat=1;
	int i;

#define PASSVERT(ind) {						\
	if (shademodel==GL_SMOOTH)				\
		glNormal3sv(dlm->mvert[(ind)].no);	\
	glVertex3fv(dlm->mvert[(ind)].co);		\
}

	glBegin(glmode=GL_QUADS);
	for (i=0; i<dlm->totface; i++) {
		MFace *mf= &dlm->mface[i];
		int new_glmode = mf->v4?GL_QUADS:GL_TRIANGLES;
		int new_shademodel = (mf->flag&ME_SMOOTH)?GL_SMOOTH:GL_FLAT;
		int new_matnr = mf->mat_nr+1;
		
		if(new_glmode!=glmode || new_shademodel!=shademodel || new_matnr!=matnr) {
			glEnd();

			drawCurrentMat = setMaterial(matnr=new_matnr);

			glShadeModel(shademodel=new_shademodel);
			glBegin(glmode=new_glmode);
		}
		
		if (drawCurrentMat) {
			if (shademodel==GL_FLAT)
				glNormal3fv(&nors[i*3]);
				
			PASSVERT(mf->v1);
			PASSVERT(mf->v2);
			PASSVERT(mf->v3);
			if (mf->v4)
				PASSVERT(mf->v4);
		}
	}
	glEnd();
	
#undef PASSVERT
}
static void ssDM_drawFacesColored(DerivedMesh *dm, int useTwoSided, unsigned char *vcols1, unsigned char *vcols2)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	int i, lmode;
	
	glShadeModel(GL_SMOOTH);
	if (vcols2) {
		glEnable(GL_CULL_FACE);
	} else {
		useTwoSided = 0;
	}
		
#define PASSVERT(vidx, fidx) {					\
	unsigned char *col= &colbase[fidx*4];		\
	glColor3ub(col[3], col[2], col[1]);			\
	glVertex3fv(dlm->mvert[(vidx)].co);			\
}

	glBegin(lmode= GL_QUADS);
	for (i=0; i<dlm->totface; i++) {
		MFace *mf= &dlm->mface[i];
		int nmode= mf->v4?GL_QUADS:GL_TRIANGLES;
		unsigned char *colbase= &vcols1[i*16];
		
		if (nmode!=lmode) {
			glEnd();
			glBegin(lmode= nmode);
		}
		
		PASSVERT(mf->v1, 0);
		PASSVERT(mf->v2, 1);
		PASSVERT(mf->v3, 2);
		if (mf->v4)
			PASSVERT(mf->v4, 3);
		
		if (useTwoSided) {
			unsigned char *colbase= &vcols2[i*16];

			if (mf->v4)
				PASSVERT(mf->v4, 3);
			PASSVERT(mf->v3, 2);
			PASSVERT(mf->v2, 1);
			PASSVERT(mf->v1, 0);
		}
	}
	glEnd();

	if (vcols2)
		glDisable(GL_CULL_FACE);
	
#undef PASSVERT
}
static void ssDM_drawFacesTex(DerivedMesh *dm, int (*setDrawParams)(TFace *tf, int matnr)) 
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MVert *mvert= dlm->mvert;
	MFace *mface= dlm->mface;
	TFace *tface = dlm->tface;
	float *nors = dlm->nors;
	int a;
	
	for (a=0; a<dlm->totface; a++) {
		MFace *mf= &mface[a];
		TFace *tf = tface?&tface[a]:NULL;
		int flag;
		unsigned char *cp= NULL;
		
		flag = setDrawParams(tf, mf->mat_nr);

		if (flag==0) {
			continue;
		} else if (flag==1) {
			if (tf) {
				cp= (unsigned char*) tf->col;
			} else if (dlm->mcol) {
				cp= (unsigned char*) &dlm->mcol[a*4];
			}
		}

		if (!(mf->flag&ME_SMOOTH)) {
			glNormal3fv(&nors[a*3]);
		}

		glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);
		if (tf) glTexCoord2fv(tf->uv[0]);
		if (cp) glColor3ub(cp[3], cp[2], cp[1]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v1].no);
		glVertex3fv((mvert+mf->v1)->co);
			
		if (tf) glTexCoord2fv(tf->uv[1]);
		if (cp) glColor3ub(cp[7], cp[6], cp[5]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v2].no);
		glVertex3fv((mvert+mf->v2)->co);

		if (tf) glTexCoord2fv(tf->uv[2]);
		if (cp) glColor3ub(cp[11], cp[10], cp[9]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v3].no);
		glVertex3fv((mvert+mf->v3)->co);

		if(mf->v4) {
			if (tf) glTexCoord2fv(tf->uv[3]);
			if (cp) glColor3ub(cp[15], cp[14], cp[13]);
			if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v4].no);
			glVertex3fv((mvert+mf->v4)->co);
		}
		glEnd();
	}
}
static void ssDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors) 
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MVert *mvert= dlm->mvert;
	MFace *mface= dlm->mface;
	float *nors = dlm->nors;
	int i, index=-1;

	for (i=0; i<dlm->totface; i++) {
		MFace *mf = &mface[i];
		int drawSmooth = 1;

		if (mf->flag&ME_FACE_STEPINDEX) index++;

		if (index!=-1 && (!setDrawOptions || setDrawOptions(userData, index, &drawSmooth))) {
			unsigned char *cp = NULL;

			if (useColors) {
				if (dlm->tface) {
					cp= (unsigned char*) dlm->tface[i].col;
				} else if (dlm->mcol) {
					cp= (unsigned char*) &dlm->mcol[i*4];
				}
			}

			glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
			glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);

			if (!drawSmooth) {
				glNormal3fv(&nors[i*3]);

				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				glVertex3fv(mvert[mf->v1].co);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				glVertex3fv(mvert[mf->v2].co);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				glVertex3fv(mvert[mf->v3].co);
				if(mf->v4) {
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					glVertex3fv(mvert[mf->v4].co);
				}
			} else {
				if (cp) glColor3ub(cp[3], cp[2], cp[1]);
				glNormal3sv(mvert[mf->v1].no);
				glVertex3fv(mvert[mf->v1].co);
				if (cp) glColor3ub(cp[7], cp[6], cp[5]);
				glNormal3sv(mvert[mf->v2].no);
				glVertex3fv(mvert[mf->v2].co);
				if (cp) glColor3ub(cp[11], cp[10], cp[9]);
				glNormal3sv(mvert[mf->v3].no);
				glVertex3fv(mvert[mf->v3].co);
				if(mf->v4) {
					if (cp) glColor3ub(cp[15], cp[14], cp[13]);
					glNormal3sv(mvert[mf->v4].no);
					glVertex3fv(mvert[mf->v4].co);
				}
			}

			glEnd();
		}
	}
}
static void ssDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	int i;

	if (ssdm->dlm->totvert) {
		for (i=0; i<ssdm->dlm->totvert; i++) {
			DO_MINMAX(ssdm->dlm->mvert[i].co, min_r, max_r);
		}
	} else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}

static void ssDM_getVertCos(DerivedMesh *dm, float (*cos_r)[3])
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	int i;

	for (i=0; i<ssdm->dlm->totvert; i++) {
		cos_r[i][0] = ssdm->dlm->mvert[i].co[0];
		cos_r[i][1] = ssdm->dlm->mvert[i].co[1];
		cos_r[i][2] = ssdm->dlm->mvert[i].co[2];
	}
}

static int ssDM_getNumVerts(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;

	return ssdm->dlm->totvert;
}
static int ssDM_getNumFaces(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;

	return ssdm->dlm->totface;
}

static DispListMesh *ssDM_convertToDispListMesh(DerivedMesh *dm, int allowShared)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;

	if (allowShared) {
		return displistmesh_copyShared(ssdm->dlm);
	} else {
		return displistmesh_copy(ssdm->dlm);
	}
}

static void ssDM_release(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;

	displistmesh_free(ssdm->dlm);

	MEM_freeN(dm);
}

DerivedMesh *derivedmesh_from_displistmesh(DispListMesh *dlm, float (*vertexCos)[3])
{
	SSDerivedMesh *ssdm = MEM_callocN(sizeof(*ssdm), "ssdm");

	ssdm->dm.getMinMax = ssDM_getMinMax;

	ssdm->dm.getNumVerts = ssDM_getNumVerts;
	ssdm->dm.getNumFaces = ssDM_getNumFaces;
	ssdm->dm.convertToDispListMesh = ssDM_convertToDispListMesh;

	ssdm->dm.getVertCos = ssDM_getVertCos;

	ssdm->dm.drawVerts = ssDM_drawVerts;

	ssdm->dm.drawUVEdges = ssDM_drawUVEdges;
	ssdm->dm.drawEdges = ssDM_drawEdges;
	ssdm->dm.drawLooseEdges = ssDM_drawLooseEdges;
	
	ssdm->dm.drawFacesSolid = ssDM_drawFacesSolid;
	ssdm->dm.drawFacesColored = ssDM_drawFacesColored;
	ssdm->dm.drawFacesTex = ssDM_drawFacesTex;
	ssdm->dm.drawMappedFaces = ssDM_drawMappedFaces;

		/* EM functions */
	
	ssdm->dm.foreachMappedVert = ssDM_foreachMappedVert;
	ssdm->dm.foreachMappedEdge = ssDM_foreachMappedEdge;
	ssdm->dm.foreachMappedFaceCenter = ssDM_foreachMappedFaceCenter;
	
	ssdm->dm.drawMappedEdges = ssDM_drawMappedEdges;
	ssdm->dm.drawMappedEdgesInterp = NULL; // no way to implement this one
	
	ssdm->dm.release = ssDM_release;
	
	ssdm->dlm = dlm;

	if (vertexCos) {
		int i;

		for (i=0; i<dlm->totvert; i++) {
			VECCOPY(dlm->mvert[i].co, vertexCos[i]);
		}

		if (dlm->nors && !dlm->dontFreeNors) {
			MEM_freeN(dlm->nors);
			dlm->nors = 0;
		}

		mesh_calc_normals(dlm->mvert, dlm->totvert, dlm->mface, dlm->totface, &dlm->nors);
	}

	return (DerivedMesh*) ssdm;
}

/***/


DerivedMesh *mesh_create_derived_for_modifier(Object *ob, ModifierData *md)
{
	Mesh *me = ob->data;
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *dm;

	if (!(md->mode&eModifierMode_Realtime)) return NULL;
	if (mti->isDisabled && mti->isDisabled(md)) return NULL;

	if (mti->type==eModifierTypeType_OnlyDeform) {
		int numVerts;
		float (*deformedVerts)[3] = mesh_getVertexCos(me, &numVerts);

		mti->deformVerts(md, ob, NULL, deformedVerts, numVerts);
		
		dm = getMeshDerivedMesh(me, ob, deformedVerts);
		MEM_freeN(deformedVerts);
	} else {
		dm = mti->applyModifier(md, ob, NULL, NULL, 0, 0);
	}

	return dm;
}

static void mesh_calc_modifiers(Object *ob, float (*inputVertexCos)[3], DerivedMesh **deform_r, DerivedMesh **final_r, int useRenderParams, int useDeform)
{
	Mesh *me = ob->data;
	ModifierData *md= modifiers_getVirtualModifierList(ob);
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm;
	int numVerts = me->totvert;

	modifiers_clearErrors(ob);

	if (deform_r) *deform_r = NULL;
	*final_r = NULL;

	if (useDeform) {
		if(do_ob_key(ob))	/* shape key makes deform verts */
			deformedVerts = mesh_getVertexCos(me, &numVerts);
		
			/* Apply all leading deforming modifiers */
		for (; md; md=md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			if (!(md->mode&(1<<useRenderParams))) continue;
			if (mti->isDisabled && mti->isDisabled(md)) continue;

			if (mti->type==eModifierTypeType_OnlyDeform) {
				if (!deformedVerts) deformedVerts = mesh_getVertexCos(me, &numVerts);
				mti->deformVerts(md, ob, NULL, deformedVerts, numVerts);
			} else {
				break;
			}
		}

			/* Result of all leading deforming modifiers is cached for
			 * places that wish to use the original mesh but with deformed
			 * coordinates (vpaint, etc.)
			 */
		if (deform_r) *deform_r = getMeshDerivedMesh(me, ob, deformedVerts);
	} else {
		deformedVerts = inputVertexCos;
	}

	/* N_T 
	 * i dont know why, but somehow the if(useDeform) part
	 * is necessary to get anything displayed
	 */ 
	if((G.obedit!=ob) && (ob->fluidsimFlag & OB_FLUIDSIM_ENABLE)) {
		if(ob->fluidsimSettings->type & OB_FLUIDSIM_DOMAIN) {
			*final_r = getFluidsimDerivedMesh(ob,useRenderParams, NULL,NULL);					
			if(*final_r) return;
		}
	}

		/* Now apply all remaining modifiers. If useDeform is off then skip
		 * OnlyDeform ones. 
		 */
	dm = NULL;
	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!(md->mode&(1<<useRenderParams))) continue;
		if (mti->type==eModifierTypeType_OnlyDeform && !useDeform) continue;
		if ((mti->flags&eModifierTypeFlag_RequiresOriginalData) && dm) {
			modifier_setError(md, "Internal error, modifier requires original data (bad stack position).");
			continue;
		}
		if (mti->isDisabled && mti->isDisabled(md)) continue;

			/* How to apply modifier depends on (a) what we already have as
			 * a result of previous modifiers (could be a DerivedMesh or just
			 * deformed vertices) and (b) what type the modifier is.
			 */

		if (mti->type==eModifierTypeType_OnlyDeform) {
				/* No existing verts to deform, need to build them. */
			if (!deformedVerts) {
				if (dm) {
						/* Deforming a derived mesh, read the vertex locations out of the mesh and
						 * deform them. Once done with this run of deformers verts will be written back.
						 */
					numVerts = dm->getNumVerts(dm);
					deformedVerts = MEM_mallocN(sizeof(*deformedVerts)*numVerts, "dfmv");
					dm->getVertCos(dm, deformedVerts);
				} else {
					deformedVerts = mesh_getVertexCos(me, &numVerts);
				}
			}

			mti->deformVerts(md, ob, dm, deformedVerts, numVerts);
		} else {
				/* There are 4 cases here (have deform? have dm?) but they all are handled
				 * by the modifier apply function, which will also free the DerivedMesh if
				 * it exists.
				 */
			DerivedMesh *ndm = mti->applyModifier(md, ob, dm, deformedVerts, useRenderParams, !inputVertexCos);

			if (ndm) {
				if (dm) dm->release(dm);

				dm = ndm;

				if (deformedVerts) {
					if (deformedVerts!=inputVertexCos) {
						MEM_freeN(deformedVerts);
					}
					deformedVerts = NULL;
				}
			} 
		}
	}

		/* Yay, we are done. If we have a DerivedMesh and deformed vertices need to apply
		 * these back onto the DerivedMesh. If we have no DerivedMesh then we need to build
		 * one.
		 */
	if (dm && deformedVerts) {
		DispListMesh *dlm = dm->convertToDispListMesh(dm, 0);

		dm->release(dm);

		*final_r = derivedmesh_from_displistmesh(dlm, deformedVerts);
	} else if (dm) {
		*final_r = dm;
	} else {
		*final_r = getMeshDerivedMesh(me, ob, deformedVerts);
	}

	if (deformedVerts && deformedVerts!=inputVertexCos) {
		MEM_freeN(deformedVerts);
	}
}

static vec3f *editmesh_getVertexCos(EditMesh *em, int *numVerts_r)
{
	int i, numVerts = *numVerts_r = BLI_countlist(&em->verts);
	float (*cos)[3];
	EditVert *eve;

	cos = MEM_mallocN(sizeof(*cos)*numVerts, "vertexcos");
	for (i=0,eve=em->verts.first; i<numVerts; i++,eve=eve->next) {
		VECCOPY(cos[i], eve->co);
	}

	return (vec3f *)cos;
}

static void editmesh_calc_modifiers(DerivedMesh **cage_r, DerivedMesh **final_r)
{
	Object *ob = G.obedit;
	EditMesh *em = G.editMesh;
	ModifierData *md;
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm;
	int i, numVerts, cageIndex = modifiers_getCageIndex(ob, NULL);

	modifiers_clearErrors(ob);

	if (cage_r && cageIndex==-1) {
		*cage_r = getEditMeshDerivedMesh(em, NULL);
	}

	dm = NULL;
	for (i=0,md= ob->modifiers.first; md; i++,md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!(md->mode&eModifierMode_Realtime)) continue;
		if (!(md->mode&eModifierMode_Editmode)) continue;
		if ((mti->flags&eModifierTypeFlag_RequiresOriginalData) && dm) {
			modifier_setError(md, "Internal error, modifier requires original data (bad stack position).");
			continue;
		}
		if (mti->isDisabled && mti->isDisabled(md)) continue;
		if (!(mti->flags&eModifierTypeFlag_SupportsEditmode)) continue;

			/* How to apply modifier depends on (a) what we already have as
			 * a result of previous modifiers (could be a DerivedMesh or just
			 * deformed vertices) and (b) what type the modifier is.
			 */

		if (mti->type==eModifierTypeType_OnlyDeform) {
				/* No existing verts to deform, need to build them. */
			if (!deformedVerts) {
				if (dm) {
						/* Deforming a derived mesh, read the vertex locations out of the mesh and
						 * deform them. Once done with this run of deformers verts will be written back.
						 */
					numVerts = dm->getNumVerts(dm);
					deformedVerts = MEM_mallocN(sizeof(*deformedVerts)*numVerts, "dfmv");
					dm->getVertCos(dm, deformedVerts);
				} else {
					deformedVerts = editmesh_getVertexCos(em, &numVerts);
				}
			}

			mti->deformVertsEM(md, ob, em, dm, deformedVerts, numVerts);
		} else {
				/* There are 4 cases here (have deform? have dm?) but they all are handled
				 * by the modifier apply function, which will also free the DerivedMesh if
				 * it exists.
				 */
			DerivedMesh *ndm = mti->applyModifierEM(md, ob, em, dm, deformedVerts);

			if (ndm) {
				if (dm && (!cage_r || dm!=*cage_r)) dm->release(dm);

				dm = ndm;

				if (deformedVerts) {
					MEM_freeN(deformedVerts);
					deformedVerts = NULL;
				}
			}
		}

		if (cage_r && i==cageIndex) {
			if (dm && deformedVerts) {
				DispListMesh *dlm;

				dlm = dm->convertToDispListMesh(dm, 0);

				*cage_r = derivedmesh_from_displistmesh(dlm, deformedVerts);
			} else if (dm) {
				*cage_r = dm;
			} else {
				*cage_r = getEditMeshDerivedMesh(em, deformedVerts?MEM_dupallocN(deformedVerts):NULL);
			}
		}
	}

		/* Yay, we are done. If we have a DerivedMesh and deformed vertices need to apply
		 * these back onto the DerivedMesh. If we have no DerivedMesh then we need to build
		 * one.
		 */
	if (dm && deformedVerts) {
		DispListMesh *dlm = dm->convertToDispListMesh(dm, 0);

		if (!cage_r || dm!=*cage_r) dm->release(dm);

		*final_r = derivedmesh_from_displistmesh(dlm, deformedVerts);
		MEM_freeN(deformedVerts);
	} else if (dm) {
		*final_r = dm;
	} else {
		*final_r = getEditMeshDerivedMesh(em, deformedVerts);
	}
}

/***/


	/* Something of a hack, at the moment deal with weightpaint
	 * by tucking into colors during modifier eval, only in
	 * wpaint mode. Works ok but need to make sure recalc
	 * happens on enter/exit wpaint.
	 */

static void weight_to_rgb(float input, float *fr, float *fg, float *fb)
{
	float blend;
	
	blend= ((input/2.0f)+0.5f);
	
	if (input<=0.25f){	// blue->cyan
		*fr= 0.0f;
		*fg= blend*input*4.0f;
		*fb= blend;
	}
	else if (input<=0.50f){	// cyan->green
		*fr= 0.0f;
		*fg= blend;
		*fb= blend*(1.0f-((input-0.25f)*4.0f)); 
	}
	else if (input<=0.75){	// green->yellow
		*fr= blend * ((input-0.50f)*4.0f);
		*fg= blend;
		*fb= 0.0f;
	}
	else if (input<=1.0){ // yellow->red
		*fr= blend;
		*fg= blend * (1.0f-((input-0.75f)*4.0f)); 
		*fb= 0.0f;
	}
}
static void calc_weightpaint_vert_color(Object *ob, int vert, unsigned char *col)
{
	Mesh *me = ob->data;
	float fr, fg, fb, input = 0.0f;
	int i;

	if (me->dvert) {
		for (i=0; i<me->dvert[vert].totweight; i++)
			if (me->dvert[vert].dw[i].def_nr==ob->actdef-1)
				input+=me->dvert[vert].dw[i].weight;		
	}

	CLAMP(input, 0.0f, 1.0f);
	
	weight_to_rgb(input, &fr, &fg, &fb);
	
	col[3] = (unsigned char)(fr * 255.0f);
	col[2] = (unsigned char)(fg * 255.0f);
	col[1] = (unsigned char)(fb * 255.0f);
	col[0] = 255;
}
static unsigned char *calc_weightpaint_colors(Object *ob) 
{
	Mesh *me = ob->data;
	MFace *mf = me->mface;
	unsigned char *wtcol;
	int i;
	
	wtcol = MEM_callocN (sizeof (unsigned char) * me->totface*4*4, "weightmap");
	
	memset(wtcol, 0x55, sizeof (unsigned char) * me->totface*4*4);
	for (i=0; i<me->totface; i++, mf++){
		calc_weightpaint_vert_color(ob, mf->v1, &wtcol[(i*4 + 0)*4]); 
		calc_weightpaint_vert_color(ob, mf->v2, &wtcol[(i*4 + 1)*4]); 
		calc_weightpaint_vert_color(ob, mf->v3, &wtcol[(i*4 + 2)*4]); 
		if (mf->v4)
			calc_weightpaint_vert_color(ob, mf->v4, &wtcol[(i*4 + 3)*4]); 
	}
	
	return wtcol;
}

static void clear_mesh_caches(Object *ob)
{
	Mesh *me= ob->data;

		/* also serves as signal to remake texspace */
	if (me->bb) {
		MEM_freeN(me->bb);
		me->bb = NULL;
	}

	freedisplist(&ob->disp);

	if (ob->derivedFinal) {
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal= NULL;
	}
	if (ob->derivedDeform) {
		ob->derivedDeform->release(ob->derivedDeform);
		ob->derivedDeform= NULL;
	}
}

static void mesh_build_data(Object *ob)
{
	Mesh *me = ob->data;
	float min[3], max[3];

	if(ob->flag&OB_FROMDUPLI) return;

	clear_mesh_caches(ob);

	if( (G.f & G_WEIGHTPAINT) && ob==(G.scene->basact?G.scene->basact->object:NULL)) {
		MCol *mcol = me->mcol;
		TFace *tface =  me->tface;

		me->tface = NULL;
		me->mcol = (MCol*) calc_weightpaint_colors(ob);

		mesh_calc_modifiers(ob, NULL, &ob->derivedDeform, &ob->derivedFinal, 0, 1);

		MEM_freeN(me->mcol);
		me->mcol = mcol;
		me->tface = tface;
	} else {
		mesh_calc_modifiers(ob, NULL, &ob->derivedDeform, &ob->derivedFinal, 0, 1);
	}

	INIT_MINMAX(min, max);

	ob->derivedFinal->getMinMax(ob->derivedFinal, min, max);

	boundbox_set_from_min_max(mesh_get_bb(ob->data), min, max);
}

static void editmesh_build_data(void)
{
	float min[3], max[3];

	EditMesh *em = G.editMesh;

	clear_mesh_caches(G.obedit);

	if (em->derivedFinal) {
		if (em->derivedFinal!=em->derivedCage) {
			em->derivedFinal->release(em->derivedFinal);
		}
		em->derivedFinal = NULL;
	}
	if (em->derivedCage) {
		em->derivedCage->release(em->derivedCage);
		em->derivedCage = NULL;
	}

	editmesh_calc_modifiers(&em->derivedCage, &em->derivedFinal);

	INIT_MINMAX(min, max);

	em->derivedFinal->getMinMax(em->derivedFinal, min, max);

	boundbox_set_from_min_max(mesh_get_bb(G.obedit->data), min, max);
}

void makeDispListMesh(Object *ob)
{
	if (ob==G.obedit) {
		editmesh_build_data();
	} else {
		mesh_build_data(ob);

		build_particle_system(ob);
	}
}

/***/

DerivedMesh *mesh_get_derived_final(Object *ob, int *needsFree_r)
{
	if (!ob->derivedFinal) {
		mesh_build_data(ob);
	}

	*needsFree_r = 0;
	return ob->derivedFinal;
}

DerivedMesh *mesh_get_derived_deform(Object *ob, int *needsFree_r)
{
	if (!ob->derivedDeform) {
		mesh_build_data(ob);
	} 

	*needsFree_r = 0;
	return ob->derivedDeform;
}

DerivedMesh *mesh_create_derived_render(Object *ob)
{
	DerivedMesh *final;

	mesh_calc_modifiers(ob, NULL, NULL, &final, 1, 1);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform(Object *ob, float (*vertCos)[3])
{
	DerivedMesh *final;

	mesh_calc_modifiers(ob, vertCos, NULL, &final, 0, 0);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform_render(Object *ob, float (*vertCos)[3])
{
	DerivedMesh *final;

	mesh_calc_modifiers(ob, vertCos, NULL, &final, 1, 0);

	return final;
}

/***/

DerivedMesh *editmesh_get_derived_cage_and_final(DerivedMesh **final_r, int *cageNeedsFree_r, int *finalNeedsFree_r)
{
	*cageNeedsFree_r = *finalNeedsFree_r = 0;

	if (!G.editMesh->derivedCage)
		editmesh_build_data();

	*final_r = G.editMesh->derivedFinal;
	return G.editMesh->derivedCage;
}

DerivedMesh *editmesh_get_derived_cage(int *needsFree_r)
{
	*needsFree_r = 0;

	if (!G.editMesh->derivedCage)
		editmesh_build_data();

	return G.editMesh->derivedCage;
}

DerivedMesh *editmesh_get_derived_base(void)
{
	return getEditMeshDerivedMesh(G.editMesh, NULL);
}


/* ***************************** fluidsim derived mesh ***************************** */

typedef struct {
	MeshDerivedMesh mdm;

	/* release whole mesh? */
	char freeMesh;
} FluidsimDerivedMesh;

#ifdef WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif


static void fluidsimDM_release(DerivedMesh *dm)
{
	FluidsimDerivedMesh *fsdm = (FluidsimDerivedMesh*) dm;
	if(fsdm->freeMesh) {
		// similar to free_mesh(fsdm->mdm.me) , but no things like unlink...
		if(fsdm->mdm.me->mvert) MEM_freeN(fsdm->mdm.me->mvert);
		if(fsdm->mdm.me->medge) MEM_freeN(fsdm->mdm.me->medge);
		if(fsdm->mdm.me->mface) MEM_freeN(fsdm->mdm.me->mface);
		MEM_freeN(fsdm->mdm.me);
	}

	if (fsdm->mdm.freeNors) MEM_freeN(fsdm->mdm.nors);
	if (fsdm->mdm.freeVerts) MEM_freeN(fsdm->mdm.verts);
	MEM_freeN(fsdm);
}

DerivedMesh *getFluidsimDerivedMesh(Object *srcob, int useRenderParams, float *extverts, float *nors) {
	//fprintf(stderr,"getFluidsimDerivedMesh call (obid '%s', rp %d)\n", srcob->id.name, useRenderParams); // debug
	int i;
	Mesh *mesh = NULL; // srcob->ata; 
	FluidsimDerivedMesh *fsdm;
	MeshDerivedMesh *mdm = NULL;
	float (*vertCos)[3];
	int displaymode = 0;
	int curFrame = G.scene->r.cfra - 1; /* start with 0 */
	char filename[FILE_MAXFILE],filepath[FILE_MAXFILE+FILE_MAXDIR];
	char curWd[FILE_MAXDIR];

	if(!useRenderParams) {
		displaymode = srcob->fluidsimSettings->guiDisplayMode;
	} else {
		displaymode = srcob->fluidsimSettings->renderDisplayMode;
	}
	
	//fprintf(stderr,"getFluidsimDerivedMesh call (obid '%s', rp %d, dm %d)\n", srcob->id.name, useRenderParams, displaymode); // debug
	if((displaymode==1) || (G.obedit==srcob)) {
		mesh = srcob->data;			
		return getMeshDerivedMesh(mesh , srcob, NULL);
	} 

	// init preview frame
	if(displaymode==2) {
		// use preview
		snprintf(filename,FILE_MAXFILE,"%s_surface_preview_%04d.bobj.gz", srcob->fluidsimSettings->surfdataPrefix, curFrame);
	} else {
		// load final mesh
		snprintf(filename,FILE_MAXFILE,"%s_surface_final_%04d.bobj.gz", srcob->fluidsimSettings->surfdataPrefix, curFrame);
	}
	BLI_getwdN(curWd);
	BLI_make_file_string(G.sce, filepath, srcob->fluidsimSettings->surfdataDir, filename);
	
	//fprintf(stderr,"getFluidsimDerivedMesh call (obid '%s', rp %d, dm %d) %s \n", srcob->id.name, useRenderParams, displaymode, filepath); // debug
	mesh = readBobjgz(filepath, (Mesh*)(srcob->data) );
	if(!mesh) {
		// display org. object upon failure
		mesh = srcob->data;			
		return getMeshDerivedMesh(mesh , srcob, NULL);
	}

	if((mesh)&&(mesh->totvert>0)) {
		make_edges(mesh, 0);	// 0 = make all edges draw
		// force all edge draw 	 
		for(i=0;i<mesh->totedge;i++) { 
			//mesh->medge[i].flag = ME_EDGEDRAW; 
			//fprintf(stderr,"me %d = %d\n",i,mesh->medge[i].flag);
		}
	}

	// WARNING copied from getMeshDerivedMesh
	fsdm = MEM_callocN(sizeof(*fsdm), "getFluidsimDerivedMesh_fsdm");
	fsdm->freeMesh = 1;
	mdm = &fsdm->mdm;
	vertCos = NULL;

	mdm->dm.getMinMax = meshDM_getMinMax;
	mdm->dm.convertToDispListMesh = meshDM_convertToDispListMesh;
	mdm->dm.getNumVerts = meshDM_getNumVerts;
	mdm->dm.getNumFaces = meshDM_getNumFaces;
	mdm->dm.getVertCos = meshDM_getVertCos;
	mdm->dm.getVertCo = meshDM_getVertCo;
	mdm->dm.getVertNo = meshDM_getVertNo;
	mdm->dm.drawVerts = meshDM_drawVerts;
	mdm->dm.drawUVEdges = meshDM_drawUVEdges;
	mdm->dm.drawEdges = meshDM_drawEdges;
	mdm->dm.drawLooseEdges = meshDM_drawLooseEdges;
	mdm->dm.drawFacesSolid = meshDM_drawFacesSolid;
	mdm->dm.drawFacesColored = meshDM_drawFacesColored;
	mdm->dm.drawFacesTex = meshDM_drawFacesTex;
	mdm->dm.drawMappedFaces = meshDM_drawMappedFaces;
	mdm->dm.drawMappedEdges = meshDM_drawMappedEdges;
	mdm->dm.drawMappedFaces = meshDM_drawMappedFaces;

	// use own release function
	mdm->dm.release = fluidsimDM_release;
	
	mdm->ob = srcob;
	mdm->me = mesh;
	mdm->verts = mesh->mvert;
	mdm->nors = NULL;
	mdm->freeNors = 0;
	mdm->freeVerts = 0;
	
	/* if (vertCos) { not needed for fluid meshes... */
	// XXX this is kinda ... see getMeshDerivedMesh
	mesh_calc_normals(mdm->verts, mdm->me->totvert, mdm->me->mface, mdm->me->totface, &mdm->nors);
	mdm->freeNors = 1;
	return (DerivedMesh*) mdm;
}


/* ***************************** bobj file handling ***************************** */

/* write .bobj.gz file for a mesh object */

void writeBobjgz(char *filename, struct Object *ob) 
{
	const int debugBobjWrite = 0;
	int wri,i,j;
	float wrf;
	gzFile gzf;
	DispListMesh *dlm = NULL;
	DerivedMesh *dm;
	float vec[3];
	float rotmat[3][3];
	MFace *mface = NULL;

	if(!ob->data || (ob->type!=OB_MESH)) {
		fprintf(stderr,"Writing GZ_BOBJ Invalid object %s ...\n", ob->id.name); 
		return;
	}
	if((ob->size[0]<0.0) || (ob->size[0]<0.0) || (ob->size[0]<0.0) ) {
		fprintf(stderr,"\nfluidSim::writeBobjgz:: Warning object %s has negative scaling - check triangle ordering...?\n\n", ob->id.name); 
	}

	if(debugBobjWrite) fprintf(stderr,"Writing GZ_BOBJ '%s' ... ",filename);
	gzf = gzopen(filename, "wb9");
	if (!gzf) {
		fprintf(stderr,"writeBobjgz::error - Unable to open file for writing '%s'\n", filename);
		return;
	}

	dm = mesh_create_derived_render(ob);
	dlm = dm->convertToDispListMesh(dm, 1);
	mface = dlm->mface;

	if(sizeof(wri)!=4) { fprintf(stderr,"Writing GZ_BOBJ, Invalid int size %d...\n", wri); return; } // paranoia check
	wri = dlm->totvert;
	gzwrite(gzf, &wri, sizeof(wri));
	for(i=0; i<wri;i++) {
		VECCOPY(vec, dlm->mvert[i].co); /* get transformed point */
		Mat4MulVecfl(ob->obmat, vec);
		for(j=0; j<3; j++) {
			wrf = vec[j]; 
			gzwrite(gzf, &wrf, sizeof( wrf )); 
		}
	}

	// should be the same as Vertices.size
	wri = dlm->totvert;
	gzwrite(gzf, &wri, sizeof(wri));
	EulToMat3(ob->rot, rotmat);
	for(i=0; i<wri;i++) {
		VECCOPY(vec, dlm->mvert[i].no);
		// FIXME divide? mv->no[0]= (short)(no[0]*32767.0);
		Mat3MulVecfl(rotmat, vec); 
		Normalise(vec);
		for(j=0; j<3; j++) {
			wrf = vec[j];
			gzwrite(gzf, &wrf, sizeof( wrf )); 
		}
	}

	
	/* compute no. of triangles */
	wri = 0;
	for(i=0; i<dlm->totface; i++) {
		wri++;
		if(mface[i].v4) { wri++; }
	}
	gzwrite(gzf, &wri, sizeof(wri));
	for(i=0; i<dlm->totface; i++) {

		int face[4];
		face[0] = mface[i].v1;
		face[1] = mface[i].v2;
		face[2] = mface[i].v3;
		face[3] = mface[i].v4;
		//fprintf(stderr,"F %s %d = %d,%d,%d,%d \n",ob->id.name, i, face[0],face[1],face[2],face[3] ); 

		gzwrite(gzf, &(face[0]), sizeof( face[0] )); 
		gzwrite(gzf, &(face[1]), sizeof( face[1] )); 
		gzwrite(gzf, &(face[2]), sizeof( face[2] )); 
		if(face[3]) { 
			gzwrite(gzf, &(face[0]), sizeof( face[0] )); 
			gzwrite(gzf, &(face[2]), sizeof( face[2] )); 
			gzwrite(gzf, &(face[3]), sizeof( face[3] )); 
		}
	}
	
	gzclose( gzf );
	if(dlm) displistmesh_free(dlm);
	dm->release(dm);

	if(debugBobjWrite) fprintf(stderr,"done. #Vertices: %d, #Triangles: %d\n", dlm->totvert, dlm->totface );
}

/* read .bobj.gz file into a fluidsimDerivedMesh struct */
Mesh* readBobjgz(char *filename, Mesh *orgmesh) //, fluidsimDerivedMesh *fsdm)
{
	int wri,i,j;
	float wrf;
	Mesh *newmesh; 
	const int debugBobjRead = 0;
	// init data from old mesh (materials,flags)
	MFace *origMFace = &((MFace*) orgmesh->mface)[0];
	int mat_nr = origMFace->mat_nr;
	int flag = origMFace->flag;
	MFace *fsface = NULL;
	int gotBytes;
	gzFile gzf;

	if(!orgmesh) return NULL;

	// similar to copy_mesh
	newmesh = MEM_dupallocN(orgmesh);
	newmesh->mat= orgmesh->mat;

	newmesh->mvert= NULL;
	newmesh->medge= NULL;
	newmesh->mface= NULL;
	newmesh->tface= NULL;
	newmesh->dface= NULL;

	newmesh->dvert = NULL;

	newmesh->mcol= NULL;
	newmesh->msticky= NULL;
	newmesh->texcomesh= NULL;

	newmesh->key= NULL;
	newmesh->totface = 0;
	newmesh->totvert = 0;
	newmesh->totedge = 0;
	newmesh->medge = NULL;


	if(debugBobjRead) fprintf(stderr,"Reading '%s' GZ_BOBJ... ",filename);
	gzf = gzopen(filename, "rb");
	// gzf = fopen(filename, "rb");
	// debug: fread(b,c,1,a) = gzread(a,b,c)
	if (!gzf) {
		//fprintf(stderr,"readBobjgz::error - Unable to open file for reading '%s'\n", filename); // DEBUG
		MEM_freeN(newmesh);
		return NULL;
	}

	//if(sizeof(wri)!=4) { fprintf(stderr,"Reading GZ_BOBJ, Invalid int size %d...\n", wri); return NULL; } // paranoia check
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	newmesh->totvert = wri;
	newmesh->mvert = MEM_callocN(sizeof(MVert)*newmesh->totvert, "fluidsimDerivedMesh_bobjvertices");
	if(debugBobjRead) fprintf(stderr,"#vertices %d ", newmesh->totvert); //DEBUG
	for(i=0; i<newmesh->totvert;i++) {
		//if(debugBobjRead) fprintf(stderr,"V %d = ",i);
		for(j=0; j<3; j++) {
			gotBytes = gzread(gzf, &wrf, sizeof( wrf )); 
			newmesh->mvert[i].co[j] = wrf;
			//if(debugBobjRead) fprintf(stderr,"%25.20f ", wrf);
		}
		//if(debugBobjRead) fprintf(stderr,"\n");
	}

	// should be the same as Vertices.size
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	if(wri != newmesh->totvert) {
		// complain #vertices has to be equal to #normals, reset&abort
		MEM_freeN(newmesh->mvert);
		MEM_freeN(newmesh);
		fprintf(stderr,"Reading GZ_BOBJ, #normals=%d, #vertices=%d, aborting...\n", wri,newmesh->totvert );
		return NULL;
	}
	for(i=0; i<newmesh->totvert;i++) {
		for(j=0; j<3; j++) {
			gotBytes = gzread(gzf, &wrf, sizeof( wrf )); 
			newmesh->mvert[i].no[j] = wrf*32767.0;
		}
	}

	
	/* compute no. of triangles */
	gotBytes = gzread(gzf, &wri, sizeof(wri));
	newmesh->totface = wri;
	newmesh->mface = MEM_callocN(sizeof(MFace)*newmesh->totface, "fluidsimDerivedMesh_bobjfaces");
	if(debugBobjRead) fprintf(stderr,"#faces %d ", newmesh->totface); // DEBUG
	fsface = newmesh->mface;
	for(i=0; i<newmesh->totface; i++) {
		int face[4];

		gotBytes = gzread(gzf, &(face[0]), sizeof( face[0] )); 
		gotBytes = gzread(gzf, &(face[1]), sizeof( face[1] )); 
		gotBytes = gzread(gzf, &(face[2]), sizeof( face[2] )); 
		face[3] = 0;

		fsface[i].v1 = face[0];
		fsface[i].v2 = face[1];
		fsface[i].v3 = face[2];
		fsface[i].v4 = face[3];
	}

	/*if(debugBobjRead) {
		for(i=0; i<newmesh->totvert; i++) { fprintf(stderr,"V %d = %f,%f,%f \n",i, newmesh->mvert[i].co[0],newmesh->mvert[i].co[1],newmesh->mvert[i].co[2] ); }
		for(i=0; i<newmesh->totface; i++) { fprintf(stderr,"F %d = %d,%d,%d,%d \n",i, fsface[i].v1,fsface[i].v2,fsface[i].v3,fsface[i].v4); }
	} // debug */
	// correct triangles with v3==0 for blender, cycle verts
	for(i=0; i<newmesh->totface; i++) {
		if(!fsface[i].v3) {
			int temp = fsface[i].v1;
			fsface[i].v1 = fsface[i].v2;
			fsface[i].v2 = fsface[i].v3;
			fsface[i].v3 = temp;
		}
	}
	
	gzclose( gzf );
	for(i=0;i<newmesh->totface;i++) { 
		fsface[i].mat_nr = mat_nr;
		fsface[i].flag = flag;
		fsface[i].edcode = ME_V1V2 | ME_V2V3 | ME_V3V1;
		//fprintf(stderr,"%d : %d,%d,%d\n", i,fsface[i].mat_nr, fsface[i].flag, fsface[i].edcode );
	}

	if(debugBobjRead) fprintf(stderr," done\n");
	return newmesh;
}

