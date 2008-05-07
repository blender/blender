/**
 * $Id: 
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*

meshtools.c: no editmode, tools operating on meshes

int join_mesh(void);

void fasterdraw(void);
void slowerdraw(void);

void sort_faces(void);

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_depsgraph.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_editconstraint.h"

#include "BDR_drawmesh.h" 
#include "BDR_editobject.h" 
#include "BDR_editface.h" 
#include "BDR_sculptmode.h"

#include "BLI_editVert.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"
#include "BLI_rand.h" /* for randome face sorting */

#include "mydevice.h"
#include "blendef.h"

#include "BIF_meshtools.h" /* include ourself for prototypes */

#include "RE_pipeline.h"
#include "RE_shader_ext.h"

#include "PIL_time.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

/* from rendercode.c */
#define VECMUL(dest, f)                  dest[0]*= f; dest[1]*= f; dest[2]*= f

/* * ********************** no editmode!!! *********** */

/* join selected meshes into the active mesh, context sensitive
return 0 if no join is made (error) and 1 of the join is done */
int join_mesh(void)
{
	Base *base, *nextb;
	Object *ob;
	Material **matar, *ma;
	Mesh *me;
	MVert *mvert, *mvertmain;
	MEdge *medge = NULL, *medgemain;
	MFace *mface = NULL, *mfacemain;
	float imat[4][4], cmat[4][4];
	int a, b, totcol, totedge=0, totvert=0, totface=0, ok=0, vertofs, map[MAXMAT];
	int	i, j, index, haskey=0, hasmulti=0, edgeofs, faceofs;
	bDeformGroup *dg, *odg;
	MDeformVert *dvert;
	CustomData vdata, edata, fdata;

	if(G.obedit) return 0;
	
	ob= OBACT;
	if(!ob || ob->type!=OB_MESH) return 0;
	
	if (object_data_is_libdata(ob)) {
		error_libdata();
		return 0;
	}

#ifdef WITH_VERSE
	/* it isn't allowed to join shared object at verse server
	 * this function will be implemented as soon as possible */
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base->object->type==OB_MESH) {
				if(base->object->vnode) {
					haskey= 1;
					break;
				}
			}
		}
		base= base->next;
	}
	if(haskey) {
		error("Can't join meshes shared at verse server");
		return 0;
	}
#endif

	/* count & check */
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB_BGMODE(base) { /* BGMODE since python can access */
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				totvert+= me->totvert;
				totface+= me->totface;

				if(base->object == ob) ok= 1;

				if(me->key) {
					haskey= 1;
					break;
				}
				if(me->mr) {
					hasmulti= 1;
					break;
				}
			}
		}
		base= base->next;
	}
	
	if(haskey) {
		error("Can't join meshes with vertex keys");
		return 0;
	}
	if(hasmulti) {
		error("Can't join meshes with Multires");
		return 0;
	}
	/* that way the active object is always selected */ 
	if(ok==0) return 0;
	
	if(totvert==0 || totvert>MESH_MAX_VERTS) return 0;

	/* if needed add edges to other meshes */
	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB_BGMODE(base) {
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				totedge += me->totedge;
			}
		}
	}
	
	/* new material indices and material array */
	matar= MEM_callocN(sizeof(void *)*MAXMAT, "join_mesh");
	totcol= ob->totcol;
	
	/* obact materials in new main array, is nicer start! */
	for(a=1; a<=ob->totcol; a++) {
		matar[a-1]= give_current_material(ob, a);
		id_us_plus((ID *)matar[a-1]);
		/* increase id->us : will be lowered later */
	}
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB_BGMODE(base) {
			if(ob!=base->object && base->object->type==OB_MESH) {
				me= base->object->data;

				// Join this object's vertex groups to the base one's
				for (dg=base->object->defbase.first; dg; dg=dg->next){
					/* See if this group exists in the object */
					for (odg=ob->defbase.first; odg; odg=odg->next){
						if (!strcmp(odg->name, dg->name)){
							break;
						}
					}
					if (!odg){
						odg = MEM_callocN (sizeof(bDeformGroup), "join deformGroup");
						memcpy (odg, dg, sizeof(bDeformGroup));
						BLI_addtail(&ob->defbase, odg);
					}

				}
				if (ob->defbase.first && ob->actdef==0)
					ob->actdef=1;

				if(me->totvert) {
					for(a=1; a<=base->object->totcol; a++) {
						ma= give_current_material(base->object, a);
						if(ma) {
							for(b=0; b<totcol; b++) {
								if(ma == matar[b]) break;
							}
							if(b==totcol) {
								matar[b]= ma;
								ma->id.us++;
								totcol++;
							}
							if(totcol>=MAXMAT-1) break;
						}
					}
				}
			}
			if(totcol>=MAXMAT-1) break;
		}
		base= base->next;
	}

	me= ob->data;

	memset(&vdata, 0, sizeof(vdata));
	memset(&edata, 0, sizeof(edata));
	memset(&fdata, 0, sizeof(fdata));
	
	mvert= CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
	medge= CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);
	mface= CustomData_add_layer(&fdata, CD_MFACE, CD_CALLOC, NULL, totface);

	mvertmain= mvert;
	medgemain= medge;
	mfacemain= mface;

	/* inverse transorm all selected meshes in this object */
	Mat4Invert(imat, ob->obmat);

	vertofs= 0;
	edgeofs= 0;
	faceofs= 0;
	base= FIRSTBASE;
	while(base) {
		nextb= base->next;
		if TESTBASELIB_BGMODE(base) {
			if(base->object->type==OB_MESH) {
				
				me= base->object->data;
				
				if(me->totvert) {
					CustomData_merge(&me->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, totvert);
					CustomData_copy_data(&me->vdata, &vdata, 0, vertofs, me->totvert);
					
					dvert= CustomData_get(&vdata, vertofs, CD_MDEFORMVERT);

					/* NEW VERSION */
					if (dvert){
						for (i=0; i<me->totvert; i++){
							for (j=0; j<dvert[i].totweight; j++){
								//	Find the old vertex group
								odg = BLI_findlink (&base->object->defbase, dvert[i].dw[j].def_nr);
								if(odg) {
									//	Search for a match in the new object
									for (dg=ob->defbase.first, index=0; dg; dg=dg->next, index++){
										if (!strcmp(dg->name, odg->name)){
											dvert[i].dw[j].def_nr = index;
											break;
										}
									}
								}
							}
						}
					}

					if(base->object != ob) {
						/* watch this: switch matmul order really goes wrong */
						Mat4MulMat4(cmat, base->object->obmat, imat);
						
						a= me->totvert;
						while(a--) {
							Mat4MulVecfl(cmat, mvert->co);
							mvert++;
						}
					}
					else mvert+= me->totvert;
				}
				if(me->totface) {
				
					/* make mapping for materials */
					memset(map, 0, 4*MAXMAT);
					for(a=1; a<=base->object->totcol; a++) {
						ma= give_current_material(base->object, a);
						if(ma) {
							for(b=0; b<totcol; b++) {
								if(ma == matar[b]) {
									map[a-1]= b;
									break;
								}
							}
						}
					}

					CustomData_merge(&me->fdata, &fdata, CD_MASK_MESH, CD_DEFAULT, totface);
					CustomData_copy_data(&me->fdata, &fdata, 0, faceofs, me->totface);

					for(a=0; a<me->totface; a++, mface++) {
						mface->v1+= vertofs;
						mface->v2+= vertofs;
						mface->v3+= vertofs;
						if(mface->v4) mface->v4+= vertofs;
						
						mface->mat_nr= map[(int)mface->mat_nr];
					}

					faceofs += me->totface;
				}
				
				if(me->totedge) {
					CustomData_merge(&me->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge);
					CustomData_copy_data(&me->edata, &edata, 0, edgeofs, me->totedge);

					for(a=0; a<me->totedge; a++, medge++) {
						medge->v1+= vertofs;
						medge->v2+= vertofs;
					}

					edgeofs += me->totedge;
				}
				
				vertofs += me->totvert;
				
				if(base->object!=ob)
					free_and_unlink_base(base);
			}
		}
		base= nextb;
	}
	
	me= ob->data;
	
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);

	me->totvert= totvert;
	me->totedge= totedge;
	me->totface= totface;
	
	me->vdata= vdata;
	me->edata= edata;
	me->fdata= fdata;

	mesh_update_customdata_pointers(me);
	
	/* old material array */
	for(a=1; a<=ob->totcol; a++) {
		ma= ob->mat[a-1];
		if(ma) ma->id.us--;
	}
	for(a=1; a<=me->totcol; a++) {
		ma= me->mat[a-1];
		if(ma) ma->id.us--;
	}
	if(ob->mat) MEM_freeN(ob->mat);
	if(me->mat) MEM_freeN(me->mat);
	ob->mat= me->mat= 0;
	
	if(totcol) {
		me->mat= matar;
		ob->mat= MEM_callocN(sizeof(void *)*totcol, "join obmatar");
	}
	else MEM_freeN(matar);
	
	ob->totcol= me->totcol= totcol;
	ob->colbits= 0;
	
	/* other mesh users */
	test_object_materials((ID *)me);
	
	DAG_scene_sort(G.scene);	// removed objects, need to rebuild dag before editmode call
	
	enter_editmode(EM_WAITCURSOR);
	exit_editmode(EM_FREEDATA|EM_WAITCURSOR);	// freedata, but no undo
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSSHADING, 0);

	BIF_undo_push("Join Mesh");
	return 1;
}


/* ********************** SORT FACES ******************* */

static void permutate(void *list, int num, int size, int *index)
{
	void *buf;
	int len;
	int i;

	len = num * size;

	buf = MEM_mallocN(len, "permutate");
	memcpy(buf, list, len);
	
	for (i = 0; i < num; i++) {
		memcpy((char *)list + (i * size), (char *)buf + (index[i] * size), size);
	}
	MEM_freeN(buf);
}

/* sort faces on view axis */
static float *face_sort_floats;
static int float_sort(const void *v1, const void *v2)
{
	float x1, x2;
	
	x1 = face_sort_floats[((int *) v1)[0]];
	x2 = face_sort_floats[((int *) v2)[0]];
	
	if( x1 > x2 ) return 1;
	else if( x1 < x2 ) return -1;
	return 0;
}


void sort_faces(void)
{
	Object *ob= OBACT;
	Mesh *me;
	CustomDataLayer *layer;
	int i, *index;
	short event;
	float reverse = 1;
	
	if(!ob) return;
	if(G.obedit) return;
	if(ob->type!=OB_MESH) return;
	if (!G.vd) return;
	
	me= ob->data;
	if(me->totface==0) return;
	
	event = pupmenu(
	"Sort Faces (Ctrl to reverse)%t|"
	"View Axis%x1|"
	"Cursor Distance%x2|"
	"Material%x3|"
	"Selection%x4|"
	"Randomize%x5");
	
	if (event==-1) return;
	
	if(G.qual & LR_CTRLKEY)
		reverse = -1;
	
/*	create index list */
	index = (int *) MEM_mallocN(sizeof(int) * me->totface, "sort faces");
	for (i = 0; i < me->totface; i++) {
		index[i] = i;
	}
	
	face_sort_floats = (float *) MEM_mallocN(sizeof(float) * me->totface, "sort faces float");
	
/* sort index list instead of faces itself 
   and apply this permutation to all face layers */
   
  	if (event == 5) {
		/* Random */
		for(i=0; i<me->totface; i++) {
			face_sort_floats[i] = BLI_frand();
		}
		qsort(index, me->totface, sizeof(int), float_sort);		
	} else {
		MFace *mf;
		float vec[3];
		float mat[4][4];
		float cur[3];
		
		if (event == 1)
			Mat4MulMat4(mat, OBACT->obmat, G.vd->viewmat); /* apply the view matrix to the object matrix */
		else if (event == 2) { /* sort from cursor */
			if( G.vd && G.vd->localview ) {
				VECCOPY(cur, G.vd->cursor);
			} else {
				VECCOPY(cur, G.scene->cursor);
			}
			Mat4Invert(mat, OBACT->obmat);
			Mat4MulVecfl(mat, cur);
		}
		
		mf= me->mface;
		for(i=0; i<me->totface; i++, mf++) {
			
			if (event==3) {
				face_sort_floats[i] = ((float)mf->mat_nr)*reverse;
			} else if (event==4) {
				/*selected first*/
				if (mf->flag & ME_FACE_SEL)	face_sort_floats[i] = 0.0;
				else						face_sort_floats[i] = reverse;
			} else {
				/* find the faces center */
				VECADD(vec, (me->mvert+mf->v1)->co, (me->mvert+mf->v2)->co);
				if (mf->v4) {
					VECADD(vec, vec, (me->mvert+mf->v3)->co);
					VECADD(vec, vec, (me->mvert+mf->v4)->co);
					VECMUL(vec, 0.25f);
				} else {
					VECADD(vec, vec, (me->mvert+mf->v3)->co);
					VECMUL(vec, 1.0f/3.0f);
				} /* done */
				
				if (event == 1) { /* sort on view axis */
					Mat4MulVecfl(mat, vec);
					face_sort_floats[i] = vec[2] * reverse;
				} else { /* distance from cursor*/
					face_sort_floats[i] = VecLenf(cur, vec) * reverse; /* back to front */
				}
			}
		}
		qsort(index, me->totface, sizeof(int), float_sort);
	}
	
	MEM_freeN(face_sort_floats);
	
	for(i = 0; i < me->fdata.totlayer; i++) {
		layer = &me->fdata.layers[i];
		permutate(layer->data, me->totface, CustomData_sizeof(layer->type), index);
	}

	MEM_freeN(index);

	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
}



/* ********************* MESH VERTEX OCTREE LOOKUP ************* */

/* important note; this is unfinished, needs better API for editmode, and custom threshold */

#define MOC_RES			8
#define MOC_NODE_RES	8
#define MOC_THRESH		0.0002f

typedef struct MocNode {
	struct MocNode *next;
	long index[MOC_NODE_RES];
} MocNode;

static int mesh_octree_get_base_offs(float *co, float *offs, float *div)
{
	int vx, vy, vz;
	
	vx= floor( (co[0]-offs[0])/div[0] );
	vy= floor( (co[1]-offs[1])/div[1] );
	vz= floor( (co[2]-offs[2])/div[2] );
	
	CLAMP(vx, 0, MOC_RES-1);
	CLAMP(vy, 0, MOC_RES-1);
	CLAMP(vz, 0, MOC_RES-1);

	return (vx*MOC_RES*MOC_RES) + vy*MOC_RES + vz;
}

static void mesh_octree_add_node(MocNode **bt, long index)
{
	if(*bt==NULL) {
		*bt= MEM_callocN(sizeof(MocNode), "MocNode");
		(*bt)->index[0]= index;
	}
	else {
		int a;
		for(a=0; a<MOC_NODE_RES; a++) {
			if((*bt)->index[a]==index)
				return;
			else if((*bt)->index[a]==0) {
				(*bt)->index[a]= index;
				return;
			}
		}
		mesh_octree_add_node(&(*bt)->next, index);
	}
}

static void mesh_octree_free_node(MocNode **bt)
{
	if( (*bt)->next ) {
		mesh_octree_free_node(&(*bt)->next);
	}
	MEM_freeN(*bt);
}


/* temporal define, just to make nicer code below */
#define MOC_ADDNODE(vx, vy, vz)	mesh_octree_add_node(basetable + ((vx)*MOC_RES*MOC_RES) + (vy)*MOC_RES + (vz), index)

static void mesh_octree_add_nodes(MocNode **basetable, float *co, float *offs, float *div, long index)
{
	float fx, fy, fz;
	int vx, vy, vz;
	
	if (isnan(co[0]) || !finite(co[0]) ||
		isnan(co[1]) || !finite(co[1]) ||
		isnan(co[2]) || !finite(co[2])
	) {
		return;
	}
	
	fx= (co[0]-offs[0])/div[0];
	fy= (co[1]-offs[1])/div[1];
	fz= (co[2]-offs[2])/div[2];
	CLAMP(fx, 0.0f, MOC_RES-MOC_THRESH);
	CLAMP(fy, 0.0f, MOC_RES-MOC_THRESH);
	CLAMP(fz, 0.0f, MOC_RES-MOC_THRESH);
	
	vx= floor(fx);
	vy= floor(fy);
	vz= floor(fz);
	
	MOC_ADDNODE(vx, vy, vz);
	
	if( vx>0 )
		if( fx-((float)vx)-MOC_THRESH < 0.0f)
			MOC_ADDNODE(vx-1, vy, vz);
	if( vx<MOC_RES-2 )
		if( fx-((float)vx)+MOC_THRESH > 1.0f)
			MOC_ADDNODE(vx+1, vy, vz);

	if( vy>0 )
		if( fy-((float)vy)-MOC_THRESH < 0.0f) 
			MOC_ADDNODE(vx, vy-1, vz);
	if( vy<MOC_RES-2 )
		if( fy-((float)vy)+MOC_THRESH > 1.0f) 
			MOC_ADDNODE(vx, vy+1, vz);

	if( vz>0 )
		if( fz-((float)vz)-MOC_THRESH < 0.0f) 
			MOC_ADDNODE(vx, vy, vz-1);
	if( vz<MOC_RES-2 )
		if( fz-((float)vz)+MOC_THRESH > 1.0f) 
			MOC_ADDNODE(vx, vy, vz+1);
	
}

static long mesh_octree_find_index(MocNode **bt, float (*orco)[3], MVert *mvert, float *co)
{
	float *vec;
	int a;
	
	if(*bt==NULL)
		return -1;
	
	for(a=0; a<MOC_NODE_RES; a++) {
		if((*bt)->index[a]) {
			/* does mesh verts and editmode, code looks potential dangerous, octree should really be filled OK! */
			if(orco) {
				vec= orco[(*bt)->index[a]-1];
				if(FloatCompare(vec, co, MOC_THRESH))
					return (*bt)->index[a]-1;
			}
			else if(mvert) {
				vec= (mvert+(*bt)->index[a]-1)->co;
				if(FloatCompare(vec, co, MOC_THRESH))
					return (*bt)->index[a]-1;
			}
			else {
				EditVert *eve= (EditVert *)((*bt)->index[a]);
				if(FloatCompare(eve->co, co, MOC_THRESH))
					return (*bt)->index[a];
			}
		}
		else return -1;
	}
	if( (*bt)->next)
		return mesh_octree_find_index(&(*bt)->next, orco, mvert, co);
	
	return -1;
}

static struct {
	MocNode **table;
	float offs[3], div[3];
	float (*orco)[3];
	float orcoloc[3];
} MeshOctree = {NULL, {0, 0, 0}, {0, 0, 0}, NULL};

/* mode is 's' start, or 'e' end, or 'u' use */
/* if end, ob can be NULL */
long mesh_octree_table(Object *ob, float *co, char mode)
{
	MocNode **bt;
	
	if(mode=='u') {		/* use table */
		if(MeshOctree.table==NULL)
			mesh_octree_table(ob, NULL, 's');
	   
		if(MeshOctree.table) {
			Mesh *me= ob->data;
			bt= MeshOctree.table + mesh_octree_get_base_offs(co, MeshOctree.offs, MeshOctree.div);
			if(ob==G.obedit)
				return mesh_octree_find_index(bt, NULL, NULL, co);
			else
				return mesh_octree_find_index(bt, MeshOctree.orco, me->mvert, co);
		}
		return -1;
	}
	else if(mode=='s') {	/* start table */
		Mesh *me= ob->data;
		float min[3], max[3];

		/* we compute own bounding box and don't reuse ob->bb because
		 * we are using the undeformed coordinates*/
		INIT_MINMAX(min, max);

		if(ob==G.obedit) {
			EditVert *eve;
			
			for(eve= G.editMesh->verts.first; eve; eve= eve->next)
				DO_MINMAX(eve->co, min, max)
		}
		else {		
			MVert *mvert;
			float *co;
			int a, totvert;
			
			MeshOctree.orco= mesh_getRefKeyCos(me, &totvert);
			mesh_get_texspace(me, MeshOctree.orcoloc, NULL, NULL);
			
			for(a=0, mvert= me->mvert; a<me->totvert; a++, mvert++) {
				co= (MeshOctree.orco)? MeshOctree.orco[a]: mvert->co;
				DO_MINMAX(co, min, max);
			}
		}
		
		/* for quick unit coordinate calculus */
		VECCOPY(MeshOctree.offs, min);
		MeshOctree.offs[0]-= MOC_THRESH;		/* we offset it 1 threshold unit extra */
		MeshOctree.offs[1]-= MOC_THRESH;
		MeshOctree.offs[2]-= MOC_THRESH;
		
		VecSubf(MeshOctree.div, max, min);
		MeshOctree.div[0]+= 2*MOC_THRESH;	/* and divide with 2 threshold unit more extra (try 8x8 unit grid on paint) */
		MeshOctree.div[1]+= 2*MOC_THRESH;
		MeshOctree.div[2]+= 2*MOC_THRESH;
		
		VecMulf(MeshOctree.div, 1.0f/MOC_RES);
		if(MeshOctree.div[0]==0.0f) MeshOctree.div[0]= 1.0f;
		if(MeshOctree.div[1]==0.0f) MeshOctree.div[1]= 1.0f;
		if(MeshOctree.div[2]==0.0f) MeshOctree.div[2]= 1.0f;
			
		if(MeshOctree.table) /* happens when entering this call without ending it */
			mesh_octree_table(ob, co, 'e');
		
		MeshOctree.table= MEM_callocN(MOC_RES*MOC_RES*MOC_RES*sizeof(void *), "sym table");
		
		if(ob==G.obedit) {
			EditVert *eve;

			for(eve= G.editMesh->verts.first; eve; eve= eve->next) {
				mesh_octree_add_nodes(MeshOctree.table, eve->co, MeshOctree.offs, MeshOctree.div, (long)(eve));
			}
		}
		else {		
			MVert *mvert;
			float *co;
			int a;
			
			for(a=0, mvert= me->mvert; a<me->totvert; a++, mvert++) {
				co= (MeshOctree.orco)? MeshOctree.orco[a]: mvert->co;
				mesh_octree_add_nodes(MeshOctree.table, co, MeshOctree.offs, MeshOctree.div, a+1);
			}
		}
	}
	else if(mode=='e') { /* end table */
		if(MeshOctree.table) {
			int a;
			
			for(a=0, bt=MeshOctree.table; a<MOC_RES*MOC_RES*MOC_RES; a++, bt++) {
				if(*bt) mesh_octree_free_node(bt);
			}
			MEM_freeN(MeshOctree.table);
			MeshOctree.table= NULL;
		}
		if(MeshOctree.orco) {
			MEM_freeN(MeshOctree.orco);
			MeshOctree.orco= NULL;
		}
	}
	return 0;
}

int mesh_get_x_mirror_vert(Object *ob, int index)
{
	Mesh *me= ob->data;
	MVert *mvert;
	float vec[3];
	
	if(MeshOctree.orco) {
		float *loc= MeshOctree.orcoloc;

		vec[0]= -(MeshOctree.orco[index][0] + loc[0]) - loc[0];
		vec[1]= MeshOctree.orco[index][1];
		vec[2]= MeshOctree.orco[index][2];
	}
	else {
		mvert= me->mvert+index;
		vec[0]= -mvert->co[0];
		vec[1]= mvert->co[1];
		vec[2]= mvert->co[2];
	}
	
	return mesh_octree_table(ob, vec, 'u');
}

EditVert *editmesh_get_x_mirror_vert(Object *ob, float *co)
{
	float vec[3];
	long poinval;
	
	/* ignore nan verts */
	if (isnan(co[0]) || !finite(co[0]) ||
		isnan(co[1]) || !finite(co[1]) ||
		isnan(co[2]) || !finite(co[2])
	   )
		return NULL;
	
	vec[0]= -co[0];
	vec[1]= co[1];
	vec[2]= co[2];
	
	poinval= mesh_octree_table(ob, vec, 'u');
	if(poinval != -1)
		return (EditVert *)(poinval);
	return NULL;
}

static unsigned int mirror_facehash(void *ptr)
{
	MFace *mf= ptr;
	int v0, v1;

	if(mf->v4) {
		v0= MIN4(mf->v1, mf->v2, mf->v3, mf->v4);
		v1= MAX4(mf->v1, mf->v2, mf->v3, mf->v4);
	}
	else {
		v0= MIN3(mf->v1, mf->v2, mf->v3);
		v1= MAX3(mf->v1, mf->v2, mf->v3);
	}

	return ((v0*39)^(v1*31));
}

static int mirror_facerotation(MFace *a, MFace *b)
{
	if(b->v4) {
		if(a->v1==b->v1 && a->v2==b->v2 && a->v3==b->v3 && a->v4==b->v4)
			return 0;
		else if(a->v4==b->v1 && a->v1==b->v2 && a->v2==b->v3 && a->v3==b->v4)
			return 1;
		else if(a->v3==b->v1 && a->v4==b->v2 && a->v1==b->v3 && a->v2==b->v4)
			return 2;
		else if(a->v2==b->v1 && a->v3==b->v2 && a->v4==b->v3 && a->v1==b->v4)
			return 3;
	}
	else {
		if(a->v1==b->v1 && a->v2==b->v2 && a->v3==b->v3)
			return 0;
		else if(a->v3==b->v1 && a->v1==b->v2 && a->v2==b->v3)
			return 1;
		else if(a->v2==b->v1 && a->v3==b->v2 && a->v1==b->v3)
			return 2;
	}
	
	return -1;
}

static int mirror_facecmp(void *a, void *b)
{
	return (mirror_facerotation((MFace*)a, (MFace*)b) == -1);
}

int *mesh_get_x_mirror_faces(Object *ob)
{
	Mesh *me= ob->data;
	MVert *mv, *mvert= me->mvert;
	MFace mirrormf, *mf, *hashmf, *mface= me->mface;
	GHash *fhash;
	int *mirrorverts, *mirrorfaces;
	int a;

	mirrorverts= MEM_callocN(sizeof(int)*me->totvert, "MirrorVerts");
	mirrorfaces= MEM_callocN(sizeof(int)*2*me->totface, "MirrorFaces");

	mesh_octree_table(ob, NULL, 's');

	for(a=0, mv=mvert; a<me->totvert; a++, mv++)
		mirrorverts[a]= mesh_get_x_mirror_vert(ob, a);

	mesh_octree_table(ob, NULL, 'e');

	fhash= BLI_ghash_new(mirror_facehash, mirror_facecmp);
	for(a=0, mf=mface; a<me->totface; a++, mf++)
		BLI_ghash_insert(fhash, mf, mf);

	for(a=0, mf=mface; a<me->totface; a++, mf++) {
		mirrormf.v1= mirrorverts[mf->v3];
		mirrormf.v2= mirrorverts[mf->v2];
		mirrormf.v3= mirrorverts[mf->v1];
		mirrormf.v4= (mf->v4)? mirrorverts[mf->v4]: 0;

		hashmf= BLI_ghash_lookup(fhash, &mirrormf);
		if(hashmf) {
			mirrorfaces[a*2]= hashmf - mface;
			mirrorfaces[a*2+1]= mirror_facerotation(&mirrormf, hashmf);
		}
		else
			mirrorfaces[a*2]= -1;
	}

	BLI_ghash_free(fhash, NULL, NULL);
	MEM_freeN(mirrorverts);
	
	return mirrorfaces;
}

/* ****************** render BAKING ********************** */

/* threaded break test */
static volatile int g_break= 0;
static int thread_break(void)
{
	return g_break;
}

static ScrArea *biggest_image_area(void)
{
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			size= sa->winx*sa->winy;
			if(sa->winx > 10 && sa->winy > 10 && size > maxsize) {
				maxsize= size;
				big= sa;
			}
		}
	}
	return big;
}


typedef struct BakeRender {
	Render *re;
	struct Object *actob;
	int event, tot, ready;
} BakeRender;

static void *do_bake_render(void *bake_v)
{
	BakeRender *bkr= bake_v;
	
	bkr->tot= RE_bake_shade_all_selected(bkr->re, bkr->event, bkr->actob);
	bkr->ready= 1;
	
	return NULL;
}


void objects_bake_render_menu(void)
{
	short event;

	event= pupmenu("Bake Selected Meshes %t|Full Render %x1|Ambient Occlusion %x2|Normals %x3|Texture Only %x4|Displacement %x5");
	if (event < 1) return;
	objects_bake_render_ui(event);
}

void objects_bake_render(short event, char **error_msg)
{
	Object *actob= OBACT;
	int active= G.scene->r.bake_flag & R_BAKE_TO_ACTIVE;
	short prev_r_raytrace= 0, prev_wo_amb_occ= 0;
	
	if(event==0) event= G.scene->r.bake_mode;
	
	if(G.scene->r.renderer!=R_INTERN) {	 
		*error_msg = "Bake only supported for Internal Renderer";
		return;
	}	 
	
	if(active && !actob) {
		*error_msg = "No active object";
		return;
	}
	
	if(event>0) {
		Render *re= RE_NewRender("_Bake View_");
		ScrArea *area= biggest_image_area();
		ListBase threads;
		BakeRender bkr;
		int timer=0, tot, sculptmode= G.f & G_SCULPTMODE;

		if(sculptmode) set_sculptmode();
		
		if(event==1) event= RE_BAKE_ALL;
		else if(event==2) event= RE_BAKE_AO;
		else if(event==3) event= RE_BAKE_NORMALS;
		else if(event==4) event= RE_BAKE_TEXTURE;
		else event= RE_BAKE_DISPLACEMENT;

		if(event==RE_BAKE_AO) {
			if(G.scene->world==NULL) {
				*error_msg = "No world set up";
				return;
			}

			/* If raytracing or AO is disabled, switch it on temporarily for baking. */
			prev_wo_amb_occ = (G.scene->world->mode & WO_AMB_OCC) != 0;
			G.scene->world->mode |= WO_AMB_OCC;
		}
		if(event==RE_BAKE_AO || active) {
			prev_r_raytrace = (G.scene->r.mode & R_RAYTRACE) != 0;
			G.scene->r.mode |= R_RAYTRACE;
		}
		
		waitcursor(1);
		RE_test_break_cb(re, thread_break);
		g_break= 0;
		G.afbreek= 0;	/* blender_test_break uses this global */
		
		RE_Database_Baking(re, G.scene, event, (active)? actob: NULL);
		
		/* baking itself is threaded, cannot use test_break in threads. we also update optional imagewindow */
	
		BLI_init_threads(&threads, do_bake_render, 1);
		bkr.re= re;
		bkr.event= event;
		bkr.ready= 0;
		bkr.actob= (active)? actob: NULL;
		BLI_insert_thread(&threads, &bkr);
		
		while(bkr.ready==0) {
			PIL_sleep_ms(50);
			if(bkr.ready)
				break;
			
			if (!G.background) {
				g_break= blender_test_break();
				
				timer++;
				if(area && timer==20) {
					Image *ima= RE_bake_shade_get_image();
					if(ima) ((SpaceImage *)area->spacedata.first)->image= ima;
					scrarea_do_windraw(area);
					myswapbuffers();	
					timer= 0;
				}
			}
		}
		BLI_end_threads(&threads);
		tot= bkr.tot;
		
		RE_Database_Free(re);
		waitcursor(0);
		
		if(tot==0) *error_msg = "No Images found to bake to";
		else {
			Image *ima;
			/* force OpenGL reload and mipmap recalc */
			for(ima= G.main->image.first; ima; ima= ima->id.next) {
				if(ima->ok==IMA_OK_LOADED) {
					ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
					if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY)) {
						free_realtime_image(ima); 
						imb_freemipmapImBuf(ibuf);
					}
				}
			}
		}
		
		/* restore raytrace and AO */
		if(event==RE_BAKE_AO)
			if(prev_wo_amb_occ == 0)
				G.scene->world->mode &= ~WO_AMB_OCC;

		if(event==RE_BAKE_AO || active)
			if(prev_r_raytrace == 0)
				G.scene->r.mode &= ~R_RAYTRACE;
		
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
		
		if(sculptmode) set_sculptmode();
		
	}
}

/* all selected meshes with UV maps are rendered for current scene visibility */
void objects_bake_render_ui(short event)
{
	char *error_msg = NULL;
	int is_editmode = (G.obedit!=NULL);
	
	/* Deal with editmode, this is a bit clunky but since UV's are in editmode, users are likely to bake from their */
	if (is_editmode) exit_editmode(0);
	
	objects_bake_render(event, &error_msg);
	
	if (is_editmode) enter_editmode(0);
	
	if (error_msg)
		error(error_msg);
}

