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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_effect_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

///////////////////////////////////
///////////////////////////////////

typedef struct {
	DerivedMesh dm;

	Object *ob;
	float *extverts, *nors;
} MeshDerivedMesh;

static DispListMesh *meshDM_convertToDispListMesh(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->ob->data;
	DispListMesh *dlm = MEM_callocN(sizeof(*dlm), "dlm");

	dlm->totvert = me->totvert;
	dlm->totedge = me->totedge;
	dlm->totface = me->totface;
	dlm->mvert = me->mvert;
	dlm->medge = me->medge;
	dlm->mface = me->mface;
	dlm->tface = me->tface;
	dlm->mcol = me->mcol;
	dlm->nors = mdm->nors;
	dlm->dontFreeVerts = dlm->dontFreeOther = 1;

	if (mdm->extverts) {
		int i;

		dlm->mvert = MEM_dupallocN(dlm->mvert);

		for (i=0; i<dlm->totvert; i++) {
			dlm->mvert[i].co[0] = mdm->extverts[i*3 + 0];
			dlm->mvert[i].co[1] = mdm->extverts[i*3 + 1];
			dlm->mvert[i].co[2] = mdm->extverts[i*3 + 2];
		}

		displistmesh_calc_normals(dlm);
	}

	return dlm;
}

static float *meshDM__getVertCo(MeshDerivedMesh *mdm, int index)
{
	if (mdm->extverts) {
		return mdm->extverts+3*index;
	} else {
		return ((Mesh*) mdm->ob->data)->mvert[index].co;
	}
}

static void meshDM_drawVerts(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->ob->data;
	int a, start=0, end=me->totvert;

	set_buildvars(mdm->ob, &start, &end);

	glBegin(GL_POINTS);
	for(a= start; a<end; a++) {
		glVertex3fv(meshDM__getVertCo(mdm, a));
	}
	glEnd();
}
static void meshDM_drawEdges(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me= mdm->ob->data;
	int a, start= 0, end= me->totface;
	MFace *mface = me->mface;

	set_buildvars(mdm->ob, &start, &end);
	mface+= start;
	
		// edges can't cope with buildvars, draw with
		// faces if build is in use.
	if(me->medge && start==0 && end==me->totface) {
		MEdge *medge= me->medge;
		
		glBegin(GL_LINES);
		for(a=me->totedge; a>0; a--, medge++) {
			if(medge->flag & ME_EDGEDRAW) {
				glVertex3fv(meshDM__getVertCo(mdm, medge->v1));
				glVertex3fv(meshDM__getVertCo(mdm, medge->v2));
			}
		}
		glEnd();
	}
	else {
		glBegin(GL_LINES);
		for(a=start; a<end; a++, mface++) {
			int test= mface->edcode;
			
			if(test) {
				if(test&ME_V1V2){
					glVertex3fv(meshDM__getVertCo(mdm, mface->v1));
					glVertex3fv(meshDM__getVertCo(mdm, mface->v2));
				}

				if(mface->v3) {
					if(test&ME_V2V3){
						glVertex3fv(meshDM__getVertCo(mdm, mface->v2));
						glVertex3fv(meshDM__getVertCo(mdm, mface->v3));
					}

					if (mface->v4) {
						if(test&ME_V3V4){
							glVertex3fv(meshDM__getVertCo(mdm, mface->v3));
							glVertex3fv(meshDM__getVertCo(mdm, mface->v4));
						}
						if(test&ME_V4V1){
							glVertex3fv(meshDM__getVertCo(mdm, mface->v4));
							glVertex3fv(meshDM__getVertCo(mdm, mface->v1));
						}
					} else {
						if(test&ME_V3V1){
							glVertex3fv(meshDM__getVertCo(mdm, mface->v3));
							glVertex3fv(meshDM__getVertCo(mdm, mface->v1));
						}
					}
				}
			}
		}
		glEnd();
	}
}
static void meshDM_drawLooseEdges(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->ob->data;
	MFace *mface= me->mface;
	int a, start=0, end=me->totface;

	set_buildvars(mdm->ob, &start, &end);
	mface+= start;
		
	glBegin(GL_LINES);
	for(a=start; a<end; a++, mface++) {
		if(!mface->v3) {
			glVertex3fv(meshDM__getVertCo(mdm, mface->v3));
			glVertex3fv(meshDM__getVertCo(mdm, mface->v4));
		} 
	}
	glEnd();
}
static void meshDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int))
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->ob->data;
	MVert *mvert= me->mvert;
	MFace *mface= me->mface;
	float *nors = mdm->nors;
	int a, start=0, end=me->totface;
	int glmode=-1, shademodel=-1, matnr=-1, drawCurrentMat=1;

	set_buildvars(mdm->ob, &start, &end);
	mface+= start;
	
#define PASSVERT(index, punoBit) {				\
	if (shademodel==GL_SMOOTH) {				\
		short *no = (mvert+index)->no;			\
		if (mface->puno&punoBit) {				\
			glNormal3s(-no[0], -no[1], -no[2]); \
		} else {								\
			glNormal3sv(no);					\
		}										\
	}											\
	glVertex3fv(meshDM__getVertCo(mdm,index));	\
}

	glBegin(glmode=GL_QUADS);
	for(a=start; a<end; a++, mface++, nors+=3) {
		if(mface->v3) {
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

				PASSVERT(mface->v1, ME_FLIPV1);
				PASSVERT(mface->v2, ME_FLIPV2);
				PASSVERT(mface->v3, ME_FLIPV3);
				if (mface->v4) {
					PASSVERT(mface->v4, ME_FLIPV4);
				}
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
	Object *ob= mdm->ob;
	Mesh *me= ob->data;
	MFace *mface= me->mface;
	int a, glmode, start=0, end=me->totface;
	unsigned char *cp1, *cp2;

	set_buildvars(ob, &start, &end);
	mface+= start;
	col1+= 4*start;
	if(col2) col2+= 4*start;
	
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
	for(a=start; a<end; a++, mface++, cp1+= 16) {
		if(mface->v3) {
			int new_glmode= mface->v4?GL_QUADS:GL_TRIANGLES;

			if (new_glmode!=glmode) {
				glEnd();
				glBegin(glmode= new_glmode);
			}
				
			glColor3ub(cp1[3], cp1[2], cp1[1]);
			glVertex3fv( meshDM__getVertCo(mdm,mface->v1) );
			glColor3ub(cp1[7], cp1[6], cp1[5]);
			glVertex3fv( meshDM__getVertCo(mdm,mface->v2) );
			glColor3ub(cp1[11], cp1[10], cp1[9]);
			glVertex3fv( meshDM__getVertCo(mdm,mface->v3) );
			if(mface->v4) {
				glColor3ub(cp1[15], cp1[14], cp1[13]);
				glVertex3fv( meshDM__getVertCo(mdm,mface->v4) );
			}
				
			if(useTwoSide) {
				glColor3ub(cp2[11], cp2[10], cp2[9]);
				glVertex3fv( meshDM__getVertCo(mdm,mface->v3) );
				glColor3ub(cp2[7], cp2[6], cp2[5]);
				glVertex3fv( meshDM__getVertCo(mdm,mface->v2) );
				glColor3ub(cp2[3], cp2[2], cp2[1]);
				glVertex3fv( meshDM__getVertCo(mdm,mface->v1) );
				if(mface->v4) {
					glColor3ub(cp2[15], cp2[14], cp2[13]);
					glVertex3fv( meshDM__getVertCo(mdm,mface->v4) );
				}
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
	Mesh *me = mdm->ob->data;
	MVert *mvert= me->mvert;
	MFace *mface= me->mface;
	TFace *tface = me->tface;
	float *nors = mdm->nors;
	int a, start=0, end=me->totface;

	set_buildvars(mdm->ob, &start, &end);
	
	for (a=start; a<end; a++) {
		MFace *mf= &mface[a];
		TFace *tf = tface?&tface[a]:NULL;
		unsigned char *cp= NULL;
		
		if(mf->v3==0) continue;
		if(tf && ((tf->flag&TF_HIDE) || (tf->mode&TF_INVISIBLE))) continue;

		if (setDrawParams(tf, mf->mat_nr)) {
			if (tf) {
				cp= (unsigned char *) tf->col;
			} else if (me->mcol) {
				cp= (unsigned char *) &me->mcol[a*4];
			}
		}

		if (!(mf->flag&ME_SMOOTH)) {
			glNormal3fv(&nors[a*3]);
		}

		glBegin(mf->v4?GL_QUADS:GL_TRIANGLES);
		if (tf) glTexCoord2fv(tf->uv[0]);
		if (cp) glColor3ub(cp[3], cp[2], cp[1]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v1].no);
		glVertex3fv(meshDM__getVertCo(mdm, mf->v1));
			
		if (tf) glTexCoord2fv(tf->uv[1]);
		if (cp) glColor3ub(cp[7], cp[6], cp[5]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v2].no);
		glVertex3fv(meshDM__getVertCo(mdm, mf->v2));

		if (tf) glTexCoord2fv(tf->uv[2]);
		if (cp) glColor3ub(cp[11], cp[10], cp[9]);
		if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v3].no);
		glVertex3fv(meshDM__getVertCo(mdm, mf->v3));

		if(mf->v4) {
			if (tf) glTexCoord2fv(tf->uv[3]);
			if (cp) glColor3ub(cp[15], cp[14], cp[13]);
			if (mf->flag&ME_SMOOTH) glNormal3sv(mvert[mf->v4].no);
			glVertex3fv(meshDM__getVertCo(mdm, mf->v4));
		}
		glEnd();
	}
}
static int meshDM_getNumVerts(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->ob->data;

	return me->totvert;
}
static int meshDM_getNumFaces(DerivedMesh *dm)
{
	MeshDerivedMesh *mdm = (MeshDerivedMesh*) dm;
	Mesh *me = mdm->ob->data;

	return me->totface;
}

static DerivedMesh *getMeshDerivedMesh(Object *ob, float *extverts, float *nors)
{
	MeshDerivedMesh *mdm = MEM_callocN(sizeof(*mdm), "dm");

	mdm->dm.convertToDispListMesh = meshDM_convertToDispListMesh;
	mdm->dm.getNumVerts = meshDM_getNumVerts;
	mdm->dm.getNumFaces = meshDM_getNumFaces;

	mdm->dm.drawVerts = meshDM_drawVerts;

	mdm->dm.drawEdges = meshDM_drawEdges;
	mdm->dm.drawMappedEdges = meshDM_drawEdges;
	mdm->dm.drawLooseEdges = meshDM_drawLooseEdges;

	mdm->dm.drawFacesSolid = meshDM_drawFacesSolid;
	mdm->dm.drawFacesColored = meshDM_drawFacesColored;
	mdm->dm.drawFacesTex = meshDM_drawFacesTex;

	mdm->dm.release = (void(*)(DerivedMesh*)) MEM_freeN;
	
	mdm->ob = ob;
	mdm->extverts = extverts;
	mdm->nors = nors;

	return (DerivedMesh*) mdm;
}

///

typedef struct {
	DerivedMesh dm;

	EditMesh *em;
} EditMeshDerivedMesh;

static void emDM_getMappedVertCoEM(DerivedMesh *dm, void *vert, float co_r[3])
{
	EditVert *eve = vert;

	co_r[0] = eve->co[0];
	co_r[1] = eve->co[1];
	co_r[2] = eve->co[2];
}
static void emDM_drawMappedVertsEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditVert *vert), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;

	bglBegin(GL_POINTS);
	for(eve= emdm->em->verts.first; eve; eve= eve->next) {
		if(!setDrawOptions || setDrawOptions(userData, eve))
			bglVertex3fv(eve->co);
	}
	bglEnd();		
}
static void emDM_drawMappedEdgeEM(DerivedMesh *dm, void *edge)
{
	EditEdge *eed = edge;

	glBegin(GL_LINES);
	glVertex3fv(eed->v1->co);
	glVertex3fv(eed->v2->co);
	glEnd();
}
static void emDM_drawMappedEdgesEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditEdge *edge), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;

	glBegin(GL_LINES);
	for(eed= emdm->em->edges.first; eed; eed= eed->next) {
		if(!setDrawOptions || setDrawOptions(userData, eed)) {
			glVertex3fv(eed->v1->co);
			glVertex3fv(eed->v2->co);
		}
	}
	glEnd();
}
static void emDM_drawMappedEdgesInterpEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditEdge *edge), void (*setDrawInterpOptions)(void *userData, EditEdge *edge, float t), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;

	glBegin(GL_LINES);
	for(eed= emdm->em->edges.first; eed; eed= eed->next) {
		if(!setDrawOptions || setDrawOptions(userData, eed)) {
			setDrawInterpOptions(userData, eed, 0.0);
			glVertex3fv(eed->v1->co);
			setDrawInterpOptions(userData, eed, 1.0);
			glVertex3fv(eed->v2->co);
		}
	}
	glEnd();
}
static void emDM_drawMappedFacesEM(DerivedMesh *dm, int (*setDrawOptions)(void *userData, EditFace *face), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;

	for (efa= emdm->em->faces.first; efa; efa= efa->next) {
		if(!setDrawOptions || setDrawOptions(userData, efa)) {
			glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
			glVertex3fv(efa->v1->co);
			glVertex3fv(efa->v2->co);
			glVertex3fv(efa->v3->co);
			if(efa->v4) glVertex3fv(efa->v4->co);
			glEnd();
		}
	}
}
static void emDM_drawFacesSolid(DerivedMesh *dm, int (*setMaterial)(int))
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;

	for (efa= emdm->em->faces.first; efa; efa= efa->next) {
		if(efa->h==0) {
			if (setMaterial(efa->mat_nr+1)) {
				glNormal3fv(efa->n);
				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				glVertex3fv(efa->v1->co);
				glVertex3fv(efa->v2->co);
				glVertex3fv(efa->v3->co);
				if(efa->v4) glVertex3fv(efa->v4->co);
				glEnd();
			}
		}
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

static DerivedMesh *getEditMeshDerivedMesh(EditMesh *em)
{
	EditMeshDerivedMesh *emdm = MEM_callocN(sizeof(*emdm), "dm");

	emdm->dm.getNumVerts = emDM_getNumVerts;
	emdm->dm.getNumFaces = emDM_getNumFaces;
	emdm->dm.getMappedVertCoEM = emDM_getMappedVertCoEM;

	emdm->dm.drawMappedVertsEM = emDM_drawMappedVertsEM;

	emdm->dm.drawMappedEdgeEM = emDM_drawMappedEdgeEM;
	emdm->dm.drawMappedEdgesEM = emDM_drawMappedEdgesEM;
	emdm->dm.drawMappedEdgesInterpEM = emDM_drawMappedEdgesInterpEM;
	
	emdm->dm.drawFacesSolid = emDM_drawFacesSolid;
	emdm->dm.drawMappedFacesEM = emDM_drawMappedFacesEM;

	emdm->dm.release = (void(*)(DerivedMesh*)) MEM_freeN;
	
	emdm->em = em;

	return (DerivedMesh*) emdm;
}

///

typedef struct {
	DerivedMesh dm;

	DispListMesh *dlm;
} SSDerivedMesh;

static void ssDM_drawMappedEdges(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MEdge *medge= dlm->medge;
	MVert *mvert= dlm->mvert;
	int a;
	
	glBegin(GL_LINES);
	for (a=0; a<dlm->totedge; a++, medge++) {
		if (medge->flag&ME_EDGEDRAW) {
			glVertex3fv(mvert[medge->v1].co); 
			glVertex3fv(mvert[medge->v2].co);
		}
	}
	glEnd();
}

static void ssDM_drawLooseEdges(DerivedMesh *dm)
{
	/* Can't implement currently */ 
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
static void ssDM_drawEdges(DerivedMesh *dm) 
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;
	DispListMesh *dlm = ssdm->dlm;
	MVert *mvert= dlm->mvert;
	int i;

	if (dlm->medge) {
		MEdge *medge= dlm->medge;
	
		glBegin(GL_LINES);
		for (i=0; i<dlm->totedge; i++, medge++) {
			glVertex3fv(mvert[medge->v1].co); 
			glVertex3fv(mvert[medge->v2].co);
		}
		glEnd();
	} else {
		MFace *mface= dlm->mface;

		glBegin(GL_LINES);
		for (i=0; i<dlm->totface; i++, mface++) {
			glVertex3fv(mvert[mface->v1].co);
			glVertex3fv(mvert[mface->v2].co);

			if (mface->v3) {
				glVertex3fv(mvert[mface->v2].co);
				glVertex3fv(mvert[mface->v3].co);

				glVertex3fv(mvert[mface->v3].co);
				if (mface->v4) {
					glVertex3fv(mvert[mface->v4].co);

					glVertex3fv(mvert[mface->v4].co);
				}
				glVertex3fv(mvert[mface->v1].co);
			}
		}
		glEnd();
	}
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
		
		if (mf->v3) {
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
		
		if (mf->v3) {
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
		unsigned char *cp= NULL;
		
		if(mf->v3==0) continue;
		if(tf && ((tf->flag&TF_HIDE) || (tf->mode&TF_INVISIBLE))) continue;

		if (setDrawParams(tf, mf->mat_nr)) {
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

static DispListMesh *ssDM_convertToDispListMesh(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;

	return displistmesh_copy(ssdm->dlm);
}

static void ssDM_release(DerivedMesh *dm)
{
	SSDerivedMesh *ssdm = (SSDerivedMesh*) dm;

	displistmesh_free(ssdm->dlm);

	MEM_freeN(dm);
}

DerivedMesh *derivedmesh_from_displistmesh(DispListMesh *dlm)
{
	SSDerivedMesh *ssdm = MEM_callocN(sizeof(*ssdm), "dm");

	ssdm->dm.getNumVerts = ssDM_getNumVerts;
	ssdm->dm.getNumFaces = ssDM_getNumFaces;
	ssdm->dm.convertToDispListMesh = ssDM_convertToDispListMesh;

	ssdm->dm.drawVerts = ssDM_drawVerts;

	ssdm->dm.drawEdges = ssDM_drawEdges;
	ssdm->dm.drawMappedEdges = ssDM_drawMappedEdges;
	ssdm->dm.drawLooseEdges = ssDM_drawLooseEdges;

	ssdm->dm.drawFacesSolid = ssDM_drawFacesSolid;
	ssdm->dm.drawFacesColored = ssDM_drawFacesColored;
	ssdm->dm.drawFacesTex = ssDM_drawFacesTex;

	ssdm->dm.release = ssDM_release;
	
	ssdm->dlm = dlm;

	return (DerivedMesh*) ssdm;
}

///

static void build_mesh_data(Object *ob, int inEditMode)
{
	Mesh *me = ob->data;

	if ((me->flag&ME_SUBSURF) && me->subdiv) {
		if(inEditMode && !G.editMesh->derived) {
			makeDispListMesh(ob);
		} else if (!inEditMode && !me->derived) {
			makeDispListMesh(ob);
		}
	}

	if(!me->disp.first || !((DispList*) me->disp.first)->nors) {
		addnormalsDispList(ob, &me->disp);
	}
}

DerivedMesh *mesh_get_derived(Object *ob)
{
	Mesh *me= ob->data;

	if ((me->flag&ME_SUBSURF) && me->subdiv) {
		build_mesh_data(ob, G.obedit && me==G.obedit->data);

		if(G.obedit && me==G.obedit->data) {
			return G.editMesh->derived;
		} else {
			return me->derived;
		}
	} 

	return NULL;
}

DerivedMesh *mesh_get_derived_final(Object *ob)
{
	Mesh *me= ob->data;

	build_mesh_data(ob, G.obedit && me==G.obedit->data);

	if ((me->flag&ME_SUBSURF) && me->subdiv) {
		if(G.obedit && me==G.obedit->data) {
			return G.editMesh->derived;
		} else {
			return me->derived;
		}
	} else {
		DispList *dl;
		DispList *meDL;

		dl = find_displist(&ob->disp, DL_VERTS);
		meDL = me->disp.first;
		return getMeshDerivedMesh(ob, dl?dl->verts:NULL, meDL?meDL->nors:NULL);
	}

	return NULL;
}

DerivedMesh *mesh_get_derived_render(Object *ob, int *needsFree)
{
	Mesh *me= ob->data;

	if ((me->flag&ME_SUBSURF) && me->subdivr) {
		if (me->subdiv==me->subdivr) {
			*needsFree = 0;

				// Don't reuse cache in editmode, we need to guarantee
				// index order of result and the incremental syncing messes
				// with this (could be fixed). - zr
			if(!(G.obedit && me==G.obedit->data)) {
				return me->derived;
			}
		} 

		*needsFree = 1;
		if(G.obedit && me==G.obedit->data) {
			return subsurf_make_derived_from_editmesh(G.editMesh, me->subdivr, me->subsurftype, NULL);
		} else {
			return subsurf_make_derived_from_mesh(me, me->subdivr);
		}
	}

	return NULL;
}

DerivedMesh *mesh_get_base_derived(Object *ob)
{
	Mesh *me= ob->data;

	build_mesh_data(ob, G.obedit && me==G.obedit->data);

		/* Build's extverts, nors */
	if (G.obedit && me==G.obedit->data) {
		return getEditMeshDerivedMesh(G.editMesh);
	} else {
		DispList *dl = find_displist(&ob->disp, DL_VERTS);
		DispList *meDL = me->disp.first;

		return getMeshDerivedMesh(ob, dl?dl->verts:NULL, meDL?meDL->nors:NULL);
	}
}

DerivedMesh *mesh_get_cage_derived(struct Object *ob, int *needsFree)
{
	Mesh *me= ob->data;
	DerivedMesh *dm = NULL;

	*needsFree = 0;

	if (me->flag&ME_OPT_EDGES) {
		dm = mesh_get_derived(ob);
	}
	if (!dm) {
		*needsFree = 1;
		dm = mesh_get_base_derived(ob);
	}

	return dm;
}
