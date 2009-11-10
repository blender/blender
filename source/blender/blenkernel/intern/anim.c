/** anim.c
 *
 *
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"
#include "DNA_listBase.h"

#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"

#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_utildefines.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ED_mesh.h"

static void object_duplilist_recursive(ID *id, Scene *scene, Object *ob, ListBase *duplilist, float par_space_mat[][4], int level, int animated);

void free_path(Path *path)
{
	if(path->data) MEM_freeN(path->data);
	MEM_freeN(path);
}


void calc_curvepath(Object *ob)
{
	BevList *bl;
	BevPoint *bevp, *bevpn, *bevpfirst, *bevplast;
	PathPoint *pp;
	Curve *cu;
	Nurb *nu;
	Path *path;
	float *fp, *dist, *maxdist, xyz[3];
	float fac, d=0, fac1, fac2;
	int a, tot, cycl=0;
	

	/* in a path vertices are with equal differences: path->len = number of verts */
	/* NOW WITH BEVELCURVE!!! */
	
	if(ob==NULL || ob->type != OB_CURVE) return;
	cu= ob->data;
	if(cu->editnurb) 
		nu= cu->editnurb->first;
	else 
		nu= cu->nurb.first;
	
	if(cu->path) free_path(cu->path);
	cu->path= NULL;
	
	bl= cu->bev.first;
	if(bl==NULL || !bl->nr) return;

	cu->path=path= MEM_callocN(sizeof(Path), "path");
	
	/* if POLY: last vertice != first vertice */
	cycl= (bl->poly!= -1);
	
	if(cycl) tot= bl->nr;
	else tot= bl->nr-1;
	
	path->len= tot+1;
	/* exception: vector handle paths and polygon paths should be subdivided at least a factor resolu */
	if(path->len<nu->resolu*SEGMENTSU(nu)) path->len= nu->resolu*SEGMENTSU(nu);
	
	dist= (float *)MEM_mallocN((tot+1)*4, "calcpathdist");

		/* all lengths in *dist */
	bevp= bevpfirst= (BevPoint *)(bl+1);
	fp= dist;
	*fp= 0;
	for(a=0; a<tot; a++) {
		fp++;
		if(cycl && a==tot-1)
			VecSubf(xyz, bevpfirst->vec, bevp->vec);
		else
			VecSubf(xyz, (bevp+1)->vec, bevp->vec);
		
		*fp= *(fp-1)+VecLength(xyz);
		bevp++;
	}
	
	path->totdist= *fp;

		/* the path verts  in path->data */
		/* now also with TILT value */
	pp= path->data = (PathPoint *)MEM_callocN(sizeof(PathPoint)*4*path->len, "pathdata"); // XXX - why *4? - in 2.4x each element was 4 and the size was 16, so better leave for now - Campbell
	
	bevp= bevpfirst;
	bevpn= bevp+1;
	bevplast= bevpfirst + (bl->nr-1);
	fp= dist+1;
	maxdist= dist+tot;
	fac= 1.0f/((float)path->len-1.0f);
        fac = fac * path->totdist;

	for(a=0; a<path->len; a++) {
		
		d= ((float)a)*fac;
		
		/* we're looking for location (distance) 'd' in the array */
		while((d>= *fp) && fp<maxdist) {
			fp++;
			if(bevp<bevplast) bevp++;
			bevpn= bevp+1;
			if(bevpn>bevplast) {
				if(cycl) bevpn= bevpfirst;
				else bevpn= bevplast;
			}
		}
		
		fac1= *(fp)- *(fp-1);
		fac2= *(fp)-d;
		fac1= fac2/fac1;
		fac2= 1.0f-fac1;

		VecLerpf(pp->vec, bevp->vec, bevpn->vec, fac2);
		pp->vec[3]= fac1*bevp->alfa + fac2*bevpn->alfa;
		pp->radius= fac1*bevp->radius + fac2*bevpn->radius;
		QuatInterpol(pp->quat, bevp->quat, bevpn->quat, fac2);
		NormalQuat(pp->quat);
		
		pp++;
	}
	
	MEM_freeN(dist);
}

int interval_test(int min, int max, int p1, int cycl)
{
	
	if(cycl) {
		if( p1 < min) 
			p1=  ((p1 -min) % (max-min+1)) + max+1;
		else if(p1 > max)
			p1=  ((p1 -min) % (max-min+1)) + min;
	}
	else {
		if(p1 < min) p1= min;
		else if(p1 > max) p1= max;
	}
	return p1;
}

/* warning, *vec needs FOUR items! */
/* ctime is normalized range <0-1> */
int where_on_path(Object *ob, float ctime, float *vec, float *dir, float *quat, float *radius)	/* returns OK */
{
	Curve *cu;
	Nurb *nu;
	BevList *bl;
	Path *path;
	PathPoint *pp, *p0, *p1, *p2, *p3;
	float fac;
	float data[4];
	int cycl=0, s0, s1, s2, s3;

	if(ob==NULL || ob->type != OB_CURVE) return 0;
	cu= ob->data;
	if(cu->path==NULL || cu->path->data==NULL) {
		printf("no path!\n");
		return 0;
	}
	path= cu->path;
	pp= path->data;
	
	/* test for cyclic */
	bl= cu->bev.first;
	if (!bl) return 0;
	if (!bl->nr) return 0;
	if(bl->poly> -1) cycl= 1;

	ctime *= (path->len-1);
	
	s1= (int)floor(ctime);
	fac= (float)(s1+1)-ctime;

	/* path->len is corected for cyclic */
	s0= interval_test(0, path->len-1-cycl, s1-1, cycl);
	s1= interval_test(0, path->len-1-cycl, s1, cycl);
	s2= interval_test(0, path->len-1-cycl, s1+1, cycl);
	s3= interval_test(0, path->len-1-cycl, s1+2, cycl);

	p0= pp + s0;
	p1= pp + s1;
	p2= pp + s2;
	p3= pp + s3;

	/* note, commented out for follow constraint */
	//if(cu->flag & CU_FOLLOW) {
		
		key_curve_tangent_weights(1.0f-fac, data, KEY_BSPLINE);
		
		dir[0]= data[0]*p0->vec[0] + data[1]*p1->vec[0] + data[2]*p2->vec[0] + data[3]*p3->vec[0] ;
		dir[1]= data[0]*p0->vec[1] + data[1]*p1->vec[1] + data[2]*p2->vec[1] + data[3]*p3->vec[1] ;
		dir[2]= data[0]*p0->vec[2] + data[1]*p1->vec[2] + data[2]*p2->vec[2] + data[3]*p3->vec[2] ;
		
		/* make compatible with vectoquat */
		dir[0]= -dir[0];
		dir[1]= -dir[1];
		dir[2]= -dir[2];
	//}
	
	nu= cu->nurb.first;

	/* make sure that first and last frame are included in the vectors here  */
	if(nu->type == CU_POLY) key_curve_position_weights(1.0f-fac, data, KEY_LINEAR);
	else if(nu->type == CU_BEZIER) key_curve_position_weights(1.0f-fac, data, KEY_LINEAR);
	else if(s0==s1 || p2==p3) key_curve_position_weights(1.0f-fac, data, KEY_CARDINAL);
	else key_curve_position_weights(1.0f-fac, data, KEY_BSPLINE);

	vec[0]= data[0]*p0->vec[0] + data[1]*p1->vec[0] + data[2]*p2->vec[0] + data[3]*p3->vec[0] ; /* X */
	vec[1]= data[0]*p0->vec[1] + data[1]*p1->vec[1] + data[2]*p2->vec[1] + data[3]*p3->vec[1] ; /* Y */
	vec[2]= data[0]*p0->vec[2] + data[1]*p1->vec[2] + data[2]*p2->vec[2] + data[3]*p3->vec[2] ; /* Z */
	vec[3]= data[0]*p0->vec[3] + data[1]*p1->vec[3] + data[2]*p2->vec[3] + data[3]*p3->vec[3] ; /* Tilt, should not be needed since we have quat still used */
	/* Need to verify the quat interpolation is correct - XXX */

	if (quat) {
		//float totfac, q1[4], q2[4];

		/* checks for totfac are needed when 'fac' is 1.0 key_curve_position_weights can assign zero
		 * to more then one index in data which can give divide by zero error */
/*
		totfac= data[0]+data[1];
		if(totfac>0.000001)	QuatInterpol(q1, p0->quat, p1->quat, data[0] / totfac);
		else				QUATCOPY(q1, p1->quat);

		NormalQuat(q1);

		totfac= data[2]+data[3];
		if(totfac>0.000001)	QuatInterpol(q2, p2->quat, p3->quat, data[2] / totfac);
		else				QUATCOPY(q1, p3->quat);
		NormalQuat(q2);

		totfac = data[0]+data[1]+data[2]+data[3];
		if(totfac>0.000001)	QuatInterpol(quat, q1, q2, (data[0]+data[1]) / totfac);
		else				QUATCOPY(quat, q2);
		NormalQuat(quat);
		*/
		// XXX - find some way to make quat interpolation work correctly, above code fails in rare but nasty cases.
		QUATCOPY(quat, p1->quat);
	}

	if(radius)
		*radius= data[0]*p0->radius + data[1]*p1->radius + data[2]*p2->radius + data[3]*p3->radius;

	return 1;
}

/* ****************** DUPLICATOR ************** */

static DupliObject *new_dupli_object(ListBase *lb, Object *ob, float mat[][4], int lay, int index, int type, int animated)
{
	DupliObject *dob= MEM_callocN(sizeof(DupliObject), "dupliobject");
	
	BLI_addtail(lb, dob);
	dob->ob= ob;
	Mat4CpyMat4(dob->mat, mat);
	Mat4CpyMat4(dob->omat, ob->obmat);
	dob->origlay= ob->lay;
	dob->index= index;
	dob->type= type;
	dob->animated= (type == OB_DUPLIGROUP) && animated;
	ob->lay= lay;
	
	return dob;
}

static void group_duplilist(ListBase *lb, Scene *scene, Object *ob, int level, int animated)
{
	DupliObject *dob;
	Group *group;
	GroupObject *go;
	float mat[4][4], tmat[4][4];
	
	if(ob->dup_group==NULL) return;
	group= ob->dup_group;
	
	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;
	
	/* handles animated groups, and */
	/* we need to check update for objects that are not in scene... */
	group_handle_recalc_and_update(scene, ob, group);
	animated= animated || group_is_animated(ob, group);
	
	for(go= group->gobject.first; go; go= go->next) {
		/* note, if you check on layer here, render goes wrong... it still deforms verts and uses parent imat */
		if(go->ob!=ob) {
			
			/* Group Dupli Offset, should apply after everything else */
			if (group->dupli_ofs[0] || group->dupli_ofs[1] || group->dupli_ofs[2]) {
				Mat4CpyMat4(tmat, go->ob->obmat);
				VecSubf(tmat[3], tmat[3], group->dupli_ofs);
				Mat4MulMat4(mat, tmat, ob->obmat);
			} else {
				Mat4MulMat4(mat, go->ob->obmat, ob->obmat);
			}
			
			dob= new_dupli_object(lb, go->ob, mat, ob->lay, 0, OB_DUPLIGROUP, animated);
			dob->no_draw= (dob->origlay & group->layer)==0;
			
			if(go->ob->transflag & OB_DUPLI) {
				Mat4CpyMat4(dob->ob->obmat, dob->mat);
				object_duplilist_recursive((ID *)group, scene, go->ob, lb, ob->obmat, level+1, animated);
				Mat4CpyMat4(dob->ob->obmat, dob->omat);
			}
		}
	}
}

static void frames_duplilist(ListBase *lb, Scene *scene, Object *ob, int level, int animated)
{
	extern int enable_cu_speed;	/* object.c */
	Object copyob;
	DupliObject *dob;
	int cfrao, ok;
	
	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;
	
	cfrao= scene->r.cfra;
	if(ob->parent==NULL && ob->track==NULL && ob->ipo==NULL && ob->constraints.first==NULL) return;

	if(ob->transflag & OB_DUPLINOSPEED) enable_cu_speed= 0;
	copyob= *ob;	/* store transform info */

	for(scene->r.cfra= ob->dupsta; scene->r.cfra<=ob->dupend; scene->r.cfra++) {

		ok= 1;
		if(ob->dupoff) {
			ok= scene->r.cfra - ob->dupsta;
			ok= ok % (ob->dupon+ob->dupoff);
			if(ok < ob->dupon) ok= 1;
			else ok= 0;
		}
		if(ok) {
#if 0 // XXX old animation system
			do_ob_ipo(scene, ob);
#endif // XXX old animation system
			where_is_object_time(scene, ob, (float)scene->r.cfra);
			dob= new_dupli_object(lb, ob, ob->obmat, ob->lay, scene->r.cfra, OB_DUPLIFRAMES, animated);
			Mat4CpyMat4(dob->omat, copyob.obmat);
		}
	}

	*ob= copyob;	/* restore transform info */
	scene->r.cfra= cfrao;
	enable_cu_speed= 1;
}

struct vertexDupliData {
	ID *id; /* scene or group, for recursive loops */
	int level;
	int animated;
	ListBase *lb;
	float pmat[4][4];
	float obmat[4][4]; /* Only used for dupliverts inside dupligroups, where the ob->obmat is modified */
	Scene *scene;
	Object *ob, *par;
	float (*orco)[3];
};

static void vertex_dupli__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	DupliObject *dob;
	struct vertexDupliData *vdd= userData;
	float vec[3], q2[4], mat[3][3], tmat[4][4], obmat[4][4];
	
	VECCOPY(vec, co);
	Mat4MulVecfl(vdd->pmat, vec);
	VecSubf(vec, vec, vdd->pmat[3]);
	VecAddf(vec, vec, vdd->obmat[3]);
	
	Mat4CpyMat4(obmat, vdd->obmat);
	VECCOPY(obmat[3], vec);
	
	if(vdd->par->transflag & OB_DUPLIROT) {
		if(no_f) {
			vec[0]= -no_f[0]; vec[1]= -no_f[1]; vec[2]= -no_f[2];
		}
		else if(no_s) {
			vec[0]= -no_s[0]; vec[1]= -no_s[1]; vec[2]= -no_s[2];
		}
		
		vectoquat(vec, vdd->ob->trackflag, vdd->ob->upflag, q2);
		
		QuatToMat3(q2, mat);
		Mat4CpyMat4(tmat, obmat);
		Mat4MulMat43(obmat, tmat, mat);
	}
	dob= new_dupli_object(vdd->lb, vdd->ob, obmat, vdd->par->lay, index, OB_DUPLIVERTS, vdd->animated);
	if(vdd->orco)
		VECCOPY(dob->orco, vdd->orco[index]);
	
	if(vdd->ob->transflag & OB_DUPLI) {
		float tmpmat[4][4];
		Mat4CpyMat4(tmpmat, vdd->ob->obmat);
		Mat4CpyMat4(vdd->ob->obmat, obmat); /* pretend we are really this mat */
		object_duplilist_recursive((ID *)vdd->id, vdd->scene, vdd->ob, vdd->lb, obmat, vdd->level+1, vdd->animated);
		Mat4CpyMat4(vdd->ob->obmat, tmpmat);
	}
}

static void vertex_duplilist(ListBase *lb, ID *id, Scene *scene, Object *par, float par_space_mat[][4], int level, int animated)
{
	Object *ob, *ob_iter;
	Mesh *me= par->data;
	Base *base = NULL;
	DerivedMesh *dm;
	struct vertexDupliData vdd;
	Scene *sce = NULL;
	Group *group = NULL;
	GroupObject * go = NULL;
	EditMesh *em;
	float vec[3], no[3], pmat[4][4];
	int lay, totvert, a, oblay;
	
	Mat4CpyMat4(pmat, par->obmat);
	
	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;
	
	em = BKE_mesh_get_editmesh(me);
	
	if(em) {
		dm= editmesh_get_derived_cage(scene, par, em, CD_MASK_BAREMESH);
		BKE_mesh_end_editmesh(me, em);
	} else
		dm= mesh_get_derived_deform(scene, par, CD_MASK_BAREMESH);
	
	if(G.rendering) {
		vdd.orco= (float(*)[3])get_mesh_orco_verts(par);
		transform_mesh_orco_verts(me, vdd.orco, me->totvert, 0);
	}
	else
		vdd.orco= NULL;
	
	totvert = dm->getNumVerts(dm);

	/* having to loop on scene OR group objects is NOT FUN */
	if (GS(id->name) == ID_SCE) {
		sce = (Scene *)id;
		lay= sce->lay;
		base= sce->base.first;
	} else {
		group = (Group *)id;
		lay= group->layer;
		go = group->gobject.first;
	}
	
	/* Start looping on Scene OR Group objects */
	while (base || go) { 
		if (sce) {
			ob_iter= base->object;
			oblay = base->lay;
		} else {
			ob_iter= go->ob;
			oblay = ob_iter->lay;
		}
		
		if (lay & oblay && scene->obedit!=ob_iter) {
			ob=ob_iter->parent;
			while(ob) {
				if(ob==par) {
					ob = ob_iter;
	/* End Scene/Group object loop, below is generic */
					
					
					/* par_space_mat - only used for groups so we can modify the space dupli's are in
					   when par_space_mat is NULL ob->obmat can be used instead of ob__obmat
					*/
					if(par_space_mat)
						Mat4MulMat4(vdd.obmat, ob->obmat, par_space_mat);
					else
						Mat4CpyMat4(vdd.obmat, ob->obmat);

					vdd.id= id;
					vdd.level= level;
					vdd.animated= animated;
					vdd.lb= lb;
					vdd.ob= ob;
					vdd.scene= scene;
					vdd.par= par;
					Mat4CpyMat4(vdd.pmat, pmat);
					
					/* mballs have a different dupli handling */
					if(ob->type!=OB_MBALL) ob->flag |= OB_DONE;	/* doesnt render */

					if(par->mode & OB_MODE_EDIT) {
						dm->foreachMappedVert(dm, vertex_dupli__mapFunc, (void*) &vdd);
					}
					else {
						for(a=0; a<totvert; a++) {
							dm->getVertCo(dm, a, vec);
							dm->getVertNo(dm, a, no);
							
							vertex_dupli__mapFunc(&vdd, a, vec, no, NULL);
						}
					}
					
					break;
				}
				ob= ob->parent;
			}
		}
		if (sce)	base= base->next;	/* scene loop */
		else		go= go->next;		/* group loop */
	}

	if(vdd.orco)
		MEM_freeN(vdd.orco);
	dm->release(dm);
}

static void face_duplilist(ListBase *lb, ID *id, Scene *scene, Object *par, float par_space_mat[][4], int level, int animated)
{
	Object *ob, *ob_iter;
	Base *base = NULL;
	DupliObject *dob;
	DerivedMesh *dm;
	Mesh *me= par->data;
	MTFace *mtface;
	MFace *mface;
	MVert *mvert;
	float pmat[4][4], imat[3][3], (*orco)[3] = NULL, w;
	int lay, oblay, totface, a;
	Scene *sce = NULL;
	Group *group = NULL;
	GroupObject *go = NULL;
	EditMesh *em;
	float ob__obmat[4][4]; /* needed for groups where the object matrix needs to be modified */
	
	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;
	
	Mat4CpyMat4(pmat, par->obmat);
	
	em = BKE_mesh_get_editmesh(me);
	if(em) {
		int totvert;
		
		dm= editmesh_get_derived_cage(scene, par, em, CD_MASK_BAREMESH);
		
		totface= dm->getNumFaces(dm);
		mface= MEM_mallocN(sizeof(MFace)*totface, "mface temp");
		dm->copyFaceArray(dm, mface);
		totvert= dm->getNumVerts(dm);
		mvert= MEM_mallocN(sizeof(MVert)*totvert, "mvert temp");
		dm->copyVertArray(dm, mvert);

		BKE_mesh_end_editmesh(me, em);
	}
	else {
		dm = mesh_get_derived_deform(scene, par, CD_MASK_BAREMESH);
		
		totface= dm->getNumFaces(dm);
		mface= dm->getFaceArray(dm);
		mvert= dm->getVertArray(dm);
	}

	if(G.rendering) {

		orco= (float(*)[3])get_mesh_orco_verts(par);
		transform_mesh_orco_verts(me, orco, me->totvert, 0);
		mtface= me->mtface;
	}
	else {
		orco= NULL;
		mtface= NULL;
	}
	
	/* having to loop on scene OR group objects is NOT FUN */
	if (GS(id->name) == ID_SCE) {
		sce = (Scene *)id;
		lay= sce->lay;
		base= sce->base.first;
	} else {
		group = (Group *)id;
		lay= group->layer;
		go = group->gobject.first;
	}
	
	/* Start looping on Scene OR Group objects */
	while (base || go) { 
		if (sce) {
			ob_iter= base->object;
			oblay = base->lay;
		} else {
			ob_iter= go->ob;
			oblay = ob_iter->lay;
		}
		
		if (lay & oblay && scene->obedit!=ob_iter) {
			ob=ob_iter->parent;
			while(ob) {
				if(ob==par) {
					ob = ob_iter;
	/* End Scene/Group object loop, below is generic */
					
					/* par_space_mat - only used for groups so we can modify the space dupli's are in
					   when par_space_mat is NULL ob->obmat can be used instead of ob__obmat
					*/
					if(par_space_mat)
						Mat4MulMat4(ob__obmat, ob->obmat, par_space_mat);
					else
						Mat4CpyMat4(ob__obmat, ob->obmat);
					
					Mat3CpyMat4(imat, ob->parentinv);
						
					/* mballs have a different dupli handling */
					if(ob->type!=OB_MBALL) ob->flag |= OB_DONE;	/* doesnt render */

					for(a=0; a<totface; a++) {
						int mv1 = mface[a].v1;
						int mv2 = mface[a].v2;
						int mv3 = mface[a].v3;
						int mv4 = mface[a].v4;
						float *v1= mvert[mv1].co;
						float *v2= mvert[mv2].co;
						float *v3= mvert[mv3].co;
						float *v4= (mv4)? mvert[mv4].co: NULL;
						float cent[3], quat[4], mat[3][3], mat3[3][3], tmat[4][4], obmat[4][4];

						/* translation */
						if(v4)
							CalcCent4f(cent, v1, v2, v3, v4);
						else
							CalcCent3f(cent, v1, v2, v3);
						Mat4MulVecfl(pmat, cent);
						
						VecSubf(cent, cent, pmat[3]);
						VecAddf(cent, cent, ob__obmat[3]);
						
						Mat4CpyMat4(obmat, ob__obmat);
						
						VECCOPY(obmat[3], cent);
						
						/* rotation */
						triatoquat(v1, v2, v3, quat);
						QuatToMat3(quat, mat);
						
						/* scale */
						if(par->transflag & OB_DUPLIFACES_SCALE) {
							float size= v4? AreaQ3Dfl(v1, v2, v3, v4): AreaT3Dfl(v1, v2, v3);
							size= sqrt(size) * par->dupfacesca;
							Mat3MulFloat(mat[0], size);
						}
						
						Mat3CpyMat3(mat3, mat);
						Mat3MulMat3(mat, imat, mat3);
						
						Mat4CpyMat4(tmat, obmat);
						Mat4MulMat43(obmat, tmat, mat);
						
						dob= new_dupli_object(lb, ob, obmat, lay, a, OB_DUPLIFACES, animated);
						if(G.rendering) {
							w= (mv4)? 0.25f: 1.0f/3.0f;

							if(orco) {
								VECADDFAC(dob->orco, dob->orco, orco[mv1], w);
								VECADDFAC(dob->orco, dob->orco, orco[mv2], w);
								VECADDFAC(dob->orco, dob->orco, orco[mv3], w);
								if(mv4)
									VECADDFAC(dob->orco, dob->orco, orco[mv4], w);
							}

							if(mtface) {
								dob->uv[0] += w*mtface[a].uv[0][0];
								dob->uv[1] += w*mtface[a].uv[0][1];
								dob->uv[0] += w*mtface[a].uv[1][0];
								dob->uv[1] += w*mtface[a].uv[1][1];
								dob->uv[0] += w*mtface[a].uv[2][0];
								dob->uv[1] += w*mtface[a].uv[2][1];

								if(mv4) {
									dob->uv[0] += w*mtface[a].uv[3][0];
									dob->uv[1] += w*mtface[a].uv[3][1];
								}
							}
						}
						
						if(ob->transflag & OB_DUPLI) {
							float tmpmat[4][4];
							Mat4CpyMat4(tmpmat, ob->obmat);
							Mat4CpyMat4(ob->obmat, obmat); /* pretend we are really this mat */
							object_duplilist_recursive((ID *)id, scene, ob, lb, ob->obmat, level+1, animated);
							Mat4CpyMat4(ob->obmat, tmpmat);
						}
					}
					
					break;
				}
				ob= ob->parent;
			}
		}
		if (sce)	base= base->next;	/* scene loop */
		else		go= go->next;		/* group loop */
	}
	
	if(par->mode & OB_MODE_EDIT) {
		MEM_freeN(mface);
		MEM_freeN(mvert);
	}

	if(orco)
		MEM_freeN(orco);
	
	dm->release(dm);
}

static void new_particle_duplilist(ListBase *lb, ID *id, Scene *scene, Object *par, float par_space_mat[][4], ParticleSystem *psys, int level, int animated)
{
	GroupObject *go;
	Object *ob=0, **oblist=0, obcopy, *obcopylist=0;
	DupliObject *dob;
	ParticleDupliWeight *dw;
	ParticleSimulationData sim = {scene, par, psys, psys_get_modifier(par, psys)};
	ParticleSettings *part;
	ParticleData *pa;
	ChildParticle *cpa=0;
	ParticleKey state;
	ParticleCacheKey *cache;
	float ctime, pa_time, scale = 1.0f;
	float tmat[4][4], mat[4][4], pamat[4][4], vec[3], size=0.0;
	float (*obmat)[4], (*oldobmat)[4];
	int lay, a, b, counter, hair = 0;
	int totpart, totchild, totgroup=0, pa_num;

	if(psys==0) return;
	
	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;
	
	part=psys->part;

	if(part==0)
		return;

	if(!psys_check_enabled(par, psys))
		return;

	ctime = bsystem_time(scene, par, (float)scene->r.cfra, 0.0);

	totpart = psys->totpart;
	totchild = psys->totchild;

	BLI_srandom(31415926 + psys->seed);
	
	lay= scene->lay;
	if((psys->renderdata || part->draw_as==PART_DRAW_REND) &&
		((part->ren_as == PART_DRAW_OB && part->dup_ob) ||
		(part->ren_as == PART_DRAW_GR && part->dup_group && part->dup_group->gobject.first))) {

		psys_check_group_weights(part);

		/* if we have a hair particle system, use the path cache */
		if(part->type == PART_HAIR) {
			if(psys->flag & PSYS_HAIR_DONE)
				hair= (totchild == 0 || psys->childcache) && psys->pathcache;
			if(!hair)
				return;
			
			/* we use cache, update totchild according to cached data */
			totchild = psys->totchildcache;
			totpart = psys->totcached;
		}

		psys->lattice = psys_get_lattice(&sim);

		/* gather list of objects or single object */
		if(part->ren_as==PART_DRAW_GR) {
			group_handle_recalc_and_update(scene, par, part->dup_group);

			if(part->draw & PART_DRAW_COUNT_GR) {
				for(dw=part->dupliweights.first; dw; dw=dw->next)
					totgroup += dw->count;
			}
			else {
				for(go=part->dup_group->gobject.first; go; go=go->next)
					totgroup++;
			}

			/* we also copy the actual objects to restore afterwards, since
			 * where_is_object_time will change the object which breaks transform */
			oblist = MEM_callocN(totgroup*sizeof(Object *), "dupgroup object list");
			obcopylist = MEM_callocN(totgroup*sizeof(Object), "dupgroup copy list");

			
			if(part->draw & PART_DRAW_COUNT_GR && totgroup) {
				dw = part->dupliweights.first;

				for(a=0; a<totgroup; dw=dw->next) {
					for(b=0; b<dw->count; b++, a++) {
						oblist[a] = dw->ob;
						obcopylist[a] = *dw->ob;
					}
				}
			}
			else {
				go = part->dup_group->gobject.first;
				for(a=0; a<totgroup; a++, go=go->next) {
					oblist[a] = go->ob;
					obcopylist[a] = *go->ob;
				}
			}
		}
		else {
			ob = part->dup_ob;
			obcopy = *ob;
		}

		if(totchild==0 || part->draw & PART_DRAW_PARENT)
			a = 0;
		else
			a = totpart;

		for(pa=psys->particles,counter=0; a<totpart+totchild; a++,pa++,counter++) {
			if(a<totpart) {
				/* handle parent particle */
				if(pa->flag & (PARS_UNEXIST+PARS_NO_DISP))
					continue;

				pa_num = pa->num;
				pa_time = pa->time;
				size = pa->size;
			}
			else {
				/* handle child particle */
				cpa = &psys->child[a - totpart];

				pa_num = a;
				pa_time = psys->particles[cpa->parent].time;
				size = psys_get_child_size(psys, cpa, ctime, 0);
			}

			if(part->ren_as==PART_DRAW_GR) {
				/* for groups, pick the object based on settings */
				if(part->draw&PART_DRAW_RAND_GR)
					b= BLI_rand() % totgroup;
				else if(part->from==PART_FROM_PARTICLE)
					b= pa_num % totgroup;
				else
					b= a % totgroup;

				ob = oblist[b];
				obmat = oblist[b]->obmat;
				oldobmat = obcopylist[b].obmat;
			}
			else {
				obmat= ob->obmat;
				oldobmat= obcopy.obmat;
			}

			if(hair) {
				/* hair we handle separate and compute transform based on hair keys */
				if(a < totpart) {
					cache = psys->pathcache[a];
					psys_get_dupli_path_transform(&sim, pa, 0, cache, pamat, &scale);
				}
				else {
					cache = psys->childcache[a-totpart];
					psys_get_dupli_path_transform(&sim, 0, cpa, cache, pamat, &scale);
				}

				VECCOPY(pamat[3], cache->co);
				pamat[3][3]= 1.0f;
				
			}
			else {
				/* first key */
				state.time = ctime;
				if(psys_get_particle_state(&sim, a, &state, 0) == 0)
					continue;

				QuatToMat4(state.rot, pamat);
				VECCOPY(pamat[3], state.co);
				pamat[3][3]= 1.0f;
			}

			if(part->ren_as==PART_DRAW_GR && psys->part->draw & PART_DRAW_WHOLE_GR) {
				for(go= part->dup_group->gobject.first, b=0; go; go= go->next, b++) {
					Mat4MulMat4(tmat, oblist[b]->obmat, pamat);
					Mat4MulFloat3((float *)tmat, size*scale);
					if(par_space_mat)
						Mat4MulMat4(mat, tmat, par_space_mat);
					else
						Mat4CpyMat4(mat, tmat);

					dob= new_dupli_object(lb, go->ob, mat, par->lay, counter, OB_DUPLIPARTS, animated);
					Mat4CpyMat4(dob->omat, obcopylist[b].obmat);
					if(G.rendering)
						psys_get_dupli_texture(par, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
				}
			}
			else {
				/* to give ipos in object correct offset */
				where_is_object_time(scene, ob, ctime-pa_time);

				VECCOPY(vec, obmat[3]);
				obmat[3][0] = obmat[3][1] = obmat[3][2] = 0.0f;
				
				Mat4CpyMat4(mat, pamat);

				Mat4MulMat4(tmat, obmat, mat);
				Mat4MulFloat3((float *)tmat, size*scale);

				if(part->draw & PART_DRAW_GLOBAL_OB)
					VECADD(tmat[3], tmat[3], vec);

				if(par_space_mat)
					Mat4MulMat4(mat, tmat, par_space_mat);
				else
					Mat4CpyMat4(mat, tmat);

				dob= new_dupli_object(lb, ob, mat, ob->lay, counter, OB_DUPLIPARTS, animated);
				Mat4CpyMat4(dob->omat, oldobmat);
				if(G.rendering)
					psys_get_dupli_texture(par, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
			}
		}

		/* restore objects since they were changed in where_is_object_time */
		if(part->ren_as==PART_DRAW_GR) {
			for(a=0; a<totgroup; a++)
				*(oblist[a])= obcopylist[a];
		}
		else
			*ob= obcopy;
	}

	/* clean up */
	if(oblist)
		MEM_freeN(oblist);
	if(obcopylist)
		MEM_freeN(obcopylist);

	if(psys->lattice) {
		end_latt_deform(psys->lattice);
		psys->lattice = NULL;
	}
}

static Object *find_family_object(Object **obar, char *family, char ch)
{
	Object *ob;
	int flen;
	
	if( obar[(int)ch] ) return obar[(int)ch];
	
	flen= strlen(family);
	
	ob= G.main->object.first;
	while(ob) {
		if( ob->id.name[flen+2]==ch ) {
			if( strncmp(ob->id.name+2, family, flen)==0 ) break;
		}
		ob= ob->id.next;
	}
	
	obar[(int)ch]= ob;
	
	return ob;
}


static void font_duplilist(ListBase *lb, Scene *scene, Object *par, int level, int animated)
{
	Object *ob, *obar[256];
	Curve *cu;
	struct chartrans *ct, *chartransdata;
	float vec[3], obmat[4][4], pmat[4][4], fsize, xof, yof;
	int slen, a;
	
	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;
	
	Mat4CpyMat4(pmat, par->obmat);
	
	/* in par the family name is stored, use this to find the other objects */
	
	chartransdata= BKE_text_to_curve(scene, par, FO_DUPLI);
	if(chartransdata==0) return;
	
	memset(obar, 0, 256*sizeof(void *));
	
	cu= par->data;
	slen= strlen(cu->str);
	fsize= cu->fsize;
	xof= cu->xof;
	yof= cu->yof;
	
	ct= chartransdata;
	
	for(a=0; a<slen; a++, ct++) {
		
		ob= find_family_object(obar, cu->family, cu->str[a]);
		if(ob) {
			vec[0]= fsize*(ct->xof - xof);
			vec[1]= fsize*(ct->yof - yof);
			vec[2]= 0.0;
			
			Mat4MulVecfl(pmat, vec);
			
			Mat4CpyMat4(obmat, par->obmat);
			VECCOPY(obmat[3], vec);
			
			new_dupli_object(lb, ob, obmat, par->lay, a, OB_DUPLIVERTS, animated);
		}
	}
	
	MEM_freeN(chartransdata);
}

/* ***************************** */
static void object_duplilist_recursive(ID *id, Scene *scene, Object *ob, ListBase *duplilist, float par_space_mat[][4], int level, int animated)
{	
	if((ob->transflag & OB_DUPLI)==0)
		return;
	
	/* Should the dupli's be generated for this object? - Respect restrict flags */
	if (G.rendering) {
		if (ob->restrictflag & OB_RESTRICT_RENDER) {
			return;
		}
	} else {
		if (ob->restrictflag & OB_RESTRICT_VIEW) {
			return;
		}
	}

	if(ob->transflag & OB_DUPLIPARTS) {
		ParticleSystem *psys = ob->particlesystem.first;
		for(; psys; psys=psys->next)
			new_particle_duplilist(duplilist, id, scene, ob, par_space_mat, psys, level+1, animated);
	}
	else if(ob->transflag & OB_DUPLIVERTS) {
		if(ob->type==OB_MESH) {
			vertex_duplilist(duplilist, id, scene, ob, par_space_mat, level+1, animated);
		}
		else if(ob->type==OB_FONT) {
			if (GS(id->name)==ID_SCE) { /* TODO - support dupligroups */
				font_duplilist(duplilist, scene, ob, level+1, animated);
			}
		}
	}
	else if(ob->transflag & OB_DUPLIFACES) {
		if(ob->type==OB_MESH)
			face_duplilist(duplilist, id, scene, ob, par_space_mat, level+1, animated);
	}
	else if(ob->transflag & OB_DUPLIFRAMES) {
		if (GS(id->name)==ID_SCE) { /* TODO - support dupligroups */
			frames_duplilist(duplilist, scene, ob, level+1, animated);
		}
	} else if(ob->transflag & OB_DUPLIGROUP) {
		DupliObject *dob;
		
		group_duplilist(duplilist, scene, ob, level+1, animated); /* now recursive */

		if (level==0) {
			for(dob= duplilist->first; dob; dob= dob->next)
				if(dob->type == OB_DUPLIGROUP)
					Mat4CpyMat4(dob->ob->obmat, dob->mat);
		}
	}
}

/* Returns a list of DupliObject
 * note; group dupli's already set transform matrix. see note in group_duplilist() */
ListBase *object_duplilist(Scene *sce, Object *ob)
{
	ListBase *duplilist= MEM_mallocN(sizeof(ListBase), "duplilist");
	duplilist->first= duplilist->last= NULL;
	object_duplilist_recursive((ID *)sce, sce, ob, duplilist, NULL, 0, 0);
	return duplilist;
}

void free_object_duplilist(ListBase *lb)
{
	DupliObject *dob;
	
	for(dob= lb->first; dob; dob= dob->next) {
		dob->ob->lay= dob->origlay;
		Mat4CpyMat4(dob->ob->obmat, dob->omat);
	}
	
	BLI_freelistN(lb);
	MEM_freeN(lb);
}

int count_duplilist(Object *ob)
{
	if(ob->transflag & OB_DUPLI) {
		if(ob->transflag & OB_DUPLIVERTS) {
			if(ob->type==OB_MESH) {
				if(ob->transflag & OB_DUPLIVERTS) {
					ParticleSystem *psys = ob->particlesystem.first;
					int pdup=0;

					for(; psys; psys=psys->next)
						pdup += psys->totpart;

					if(pdup==0){
						Mesh *me= ob->data;
						return me->totvert;
					}
					else
						return pdup;
				}
			}
		}
		else if(ob->transflag & OB_DUPLIFRAMES) {
			int tot= ob->dupend - ob->dupsta; 
			tot/= (ob->dupon+ob->dupoff);
			return tot*ob->dupon;
		}
	}
	return 1;
}
