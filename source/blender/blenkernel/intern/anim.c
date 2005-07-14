/** anim.c
 *
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "DNA_listBase.h"

#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_view3d_types.h"
#include "DNA_effect_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_anim.h"
#include "BKE_ipo.h"
#include "BKE_object.h"
#include "BKE_displist.h"
#include "BKE_key.h"
#include "BKE_font.h"
#include "BKE_effect.h"

#include "BKE_bad_level_calls.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

ListBase duplilist= {0, 0}; 

void free_path(Path *path)
{
	if(path->data) MEM_freeN(path->data);
	MEM_freeN(path);
}


void calc_curvepath(Object *ob)
{
	BevList *bl;
	BevPoint *bevp, *bevpn, *bevpfirst, *bevplast, *tempbevp;
	Curve *cu;
	Nurb *nu;
	Path *path;
	float *fp, *dist, *maxdist, x, y, z;
	float fac, d=0, fac1, fac2;
	int a, tot, cycl=0;
	float *ft;
	
	/* in a path vertices are with equal differences: path->len = number of verts */
	/* NOW WITH BEVELCURVE!!! */
	
	if(ob==NULL || ob->type != OB_CURVE) return;
	cu= ob->data;
	if(ob==G.obedit) nu= editNurb.first;
	else nu= cu->nurb.first;
	
	if(cu->path) free_path(cu->path);
	cu->path= NULL;
	
	bl= cu->bev.first;
	if(bl==NULL) return;

	cu->path=path= MEM_callocN(sizeof(Path), "path");
	
	/* if POLY: last vertice != first vertice */
	cycl= (bl->poly!= -1);
	
	if(cycl) tot= bl->nr;
	else tot= bl->nr-1;
	
	path->len= tot+1;
	/* exception: vector handle paths and polygon paths should be subdivided at least a factor 6 (or more?) */
	if(path->len<6*nu->pntsu) path->len= 6*nu->pntsu;
	
	dist= (float *)MEM_mallocN((tot+1)*4, "calcpathdist");

		/* all lengths in *dist */
	bevp= bevpfirst= (BevPoint *)(bl+1);
	fp= dist;
	*fp= 0;
	for(a=0; a<tot; a++) {
		fp++;
		if(cycl && a==tot-1) {
			x= bevpfirst->x - bevp->x;
			y= bevpfirst->y - bevp->y;
			z= bevpfirst->z - bevp->z;
		}
		else {
                        tempbevp = bevp+1;
			x= (tempbevp)->x - bevp->x;
			y= (tempbevp)->y - bevp->y;
			z= (tempbevp)->z - bevp->z;
		}
		*fp= *(fp-1)+ (float)sqrt(x*x+y*y+z*z);
		
		bevp++;
	}
	
	path->totdist= *fp;

		/* the path verts  in path->data */
		/* now also with TILT value */
	ft= path->data = (float *)MEM_callocN(16*path->len, "pathdata");
	
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

		ft[0]= fac1*bevp->x+ fac2*(bevpn)->x;
		ft[1]= fac1*bevp->y+ fac2*(bevpn)->y;
		ft[2]= fac1*bevp->z+ fac2*(bevpn)->z;
		ft[3]= fac1*bevp->alfa+ fac2*(bevpn)->alfa;
		
		ft+= 4;

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
int where_on_path(Object *ob, float ctime, float *vec, float *dir)	/* returns OK */
{
	Curve *cu;
	Nurb *nu;
	BevList *bl;
	Path *path;
	float *fp, *p0, *p1, *p2, *p3, fac;
	float data[4];
	int cycl=0, s0, s1, s2, s3;

	if(ob==NULL || ob->type != OB_CURVE) return 0;
	cu= ob->data;
	if(cu->path==NULL || cu->path->data==NULL) {
		printf("no path!\n");
	}
	path= cu->path;
	fp= path->data;
	
	/* test for cyclic */
	bl= cu->bev.first;
	if(bl && bl->poly> -1) cycl= 1;

	ctime *= (path->len-1);
	
	s1= (int)floor(ctime);
	fac= (float)(s1+1)-ctime;

	/* path->len is corected for cyclic */
	s0= interval_test(0, path->len-1-cycl, s1-1, cycl);
	s1= interval_test(0, path->len-1-cycl, s1, cycl);
	s2= interval_test(0, path->len-1-cycl, s1+1, cycl);
	s3= interval_test(0, path->len-1-cycl, s1+2, cycl);

	p0= fp + 4*s0;
	p1= fp + 4*s1;
	p2= fp + 4*s2;
	p3= fp + 4*s3;

	/* note, commented out for follow constraint */
	//if(cu->flag & CU_FOLLOW) {
		
		set_afgeleide_four_ipo(1.0f-fac, data, KEY_BSPLINE);
		
		dir[0]= data[0]*p0[0] + data[1]*p1[0] + data[2]*p2[0] + data[3]*p3[0] ;
		dir[1]= data[0]*p0[1] + data[1]*p1[1] + data[2]*p2[1] + data[3]*p3[1] ;
		dir[2]= data[0]*p0[2] + data[1]*p1[2] + data[2]*p2[2] + data[3]*p3[2] ;
		
		/* make compatible with vectoquat */
		dir[0]= -dir[0];
		dir[1]= -dir[1];
		dir[2]= -dir[2];
	//}
	
	nu= cu->nurb.first;

	/* make sure that first and last frame are included in the vectors here  */
	if((nu->type & 7)==CU_POLY) set_four_ipo(1.0f-fac, data, KEY_LINEAR);
	else if((nu->type & 7)==CU_BEZIER) set_four_ipo(1.0f-fac, data, KEY_LINEAR);
	else if(s0==s1 || p2==p3) set_four_ipo(1.0f-fac, data, KEY_CARDINAL);
	else set_four_ipo(1.0f-fac, data, KEY_BSPLINE);

	vec[0]= data[0]*p0[0] + data[1]*p1[0] + data[2]*p2[0] + data[3]*p3[0] ;
	vec[1]= data[0]*p0[1] + data[1]*p1[1] + data[2]*p2[1] + data[3]*p3[1] ;
	vec[2]= data[0]*p0[2] + data[1]*p1[2] + data[2]*p2[2] + data[3]*p3[2] ;

	vec[3]= data[0]*p0[3] + data[1]*p1[3] + data[2]*p2[3] + data[3]*p3[3] ;

	return 1;
}

static Object *new_dupli_object(ListBase *lb, Object *ob, Object *par)
{
	Object *newob;
	
	newob= MEM_mallocN(sizeof(Object), "newobj dupli");
	memcpy(newob, ob, sizeof(Object));
	newob->flag |= OB_FROMDUPLI;
	newob->id.newid= (ID *)par;	/* store duplicator */
	
	/* only basis-ball gets displist */
	if(newob->type==OB_MBALL) newob->disp.first= newob->disp.last= NULL;

	if(ob!=par) {	// dupliverts, particle
		newob->parent= NULL;
		newob->track= NULL;
	}	
	BLI_addtail(lb, newob);
	
	return newob;
}

void frames_duplilist(Object *ob)
{
	extern int enable_cu_speed;	/* object.c */
	Object *newob;
	int cfrao, ok;
	
	cfrao= G.scene->r.cfra;
	if(ob->parent==NULL && ob->track==NULL && ob->ipo==NULL && ob->constraints.first==NULL) return;

	if(ob->transflag & OB_DUPLINOSPEED) enable_cu_speed= 0;

	for(G.scene->r.cfra= ob->dupsta; G.scene->r.cfra<=ob->dupend; G.scene->r.cfra++) {

		ok= 1;
		if(ob->dupoff) {
			ok= G.scene->r.cfra - ob->dupsta;
			ok= ok % (ob->dupon+ob->dupoff);
			if(ok < ob->dupon) ok= 1;
			else ok= 0;
		}
		if(ok) {
			newob= new_dupli_object(&duplilist, ob, ob);

			do_ob_ipo(newob);
			where_is_object_time(newob, (float)G.scene->r.cfra);
		}
	}

	G.scene->r.cfra= cfrao;
	enable_cu_speed= 1;
	do_ob_ipo(ob);
}


void vertex_duplilist(Scene *sce, Object *par)
{
	Object *ob, *newob;
	Base *base;
	MVert *mvert;
	Mesh *me;
	DispList *dl;
	float *extverts=NULL, vec[3], pvec[3], pmat[4][4], mat[3][3], tmat[4][4];
	float *q2;
	int lay, totvert, a;
	
	Mat4CpyMat4(pmat, par->obmat);
	
	Mat4One(tmat);
	
	lay= G.scene->lay;
	
	dl= find_displist(&par->disp, DL_VERTS);
	if(dl) extverts= dl->verts;

	base= sce->base.first;
	while(base) {

		if(base->object->type>0 && (lay & base->lay) && G.obedit!=base->object) {
			ob= base->object->parent;
			while(ob) {
				if(ob==par) {
				
					ob= base->object;
					/* mballs have a different dupli handling */
					if(ob->type!=OB_MBALL) ob->flag |= OB_DONE;	/* doesnt render */
					
					me= par->data;
					mvert= me->mvert;
					mvert+= (me->totvert-1);
					VECCOPY(pvec, mvert->co);
					Mat4MulVecfl(pmat, pvec);

					mvert= me->mvert;
					totvert= me->totvert;

					for(a=0; a<totvert; a++, mvert++) {
					
						/* calc the extra offset for children (wrt. centre parent)  */
						if(extverts) {
							VECCOPY(vec, extverts+3*a);
						}
						else {
							VECCOPY(vec, mvert->co);
						}

						Mat4MulVecfl(pmat, vec);
						VecSubf(vec, vec, pmat[3]);
						VecAddf(vec, vec, ob->obmat[3]);
						
						newob= new_dupli_object(&duplilist, ob, par);
						VECCOPY(newob->obmat[3], vec);
						
						if(par->transflag & OB_DUPLIROT) {
							VECCOPY(vec, mvert->no);
							vec[0]= -vec[0]; vec[1]= -vec[1]; vec[2]= -vec[2];
							
							q2= vectoquat(vec, ob->trackflag, ob->upflag);
				
							QuatToMat3(q2, mat);
							Mat4CpyMat4(tmat, newob->obmat);
							Mat4MulMat43(newob->obmat, tmat, mat);
						}
						
						VECCOPY(pvec, vec);
					
					}
					break;
				}
				ob= ob->parent;
			}
		}
		base= base->next;
	}
}


void particle_duplilist(Scene *sce, Object *par, PartEff *paf)
{
	Object *ob, *newob;
	Base *base;
	Particle *pa;
	float ctime, vec1[3];
	float vec[3], tmat[4][4], mat[3][3];
	float *q2;
	int lay, a;
	
	pa= paf->keys;
	if(pa==0) {
		build_particle_system(par);
		pa= paf->keys;
		if(pa==0) return;
	}
	
	ctime= bsystem_time(par, 0, (float)G.scene->r.cfra, 0.0);

	lay= G.scene->lay;

	base= sce->base.first;
	while(base) {
		
		if(base->object->type>0 && (base->lay & lay) && G.obedit!=base->object) {
			ob= base->object->parent;
			while(ob) {
				if(ob==par) {
				
					ob= base->object;
					
					pa= paf->keys;
					for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {
						
						if(paf->flag & PAF_STATIC) {
							float mtime;
							
							where_is_particle(paf, pa, pa->time, vec1);
							mtime= pa->time+pa->lifetime;
							
							for(ctime= pa->time; ctime<mtime; ctime+=paf->staticstep) {
								newob= new_dupli_object(&duplilist, ob, par);
								
								/* make sure hair grows until the end.. */ 
								if(ctime>pa->time+pa->lifetime) ctime= pa->time+pa->lifetime;
								
								/* to give ipos in object correct offset */
								where_is_object_time(newob, ctime-pa->time);

								where_is_particle(paf, pa, ctime, vec);	// makes sure there's always a vec
								Mat4MulVecfl(par->obmat, vec);
								
								if(paf->stype==PAF_VECT) {
									where_is_particle(paf, pa, ctime+1.0, vec1); // makes sure there's always a vec
									Mat4MulVecfl(par->obmat, vec1);

									VecSubf(vec1, vec1, vec);
									q2= vectoquat(vec1, ob->trackflag, ob->upflag);
									
									QuatToMat3(q2, mat);
									Mat4CpyMat4(tmat, newob->obmat);
									Mat4MulMat43(newob->obmat, tmat, mat);
								}
								
								VECCOPY(newob->obmat[3], vec);
							}
						}
						else { // non static particles
							   
							if(ctime > pa->time) {
								if(ctime < pa->time+pa->lifetime) {
									newob= new_dupli_object(&duplilist, ob, par);

									/* to give ipos in object correct offset */
									where_is_object_time(newob, ctime-pa->time);
									
									where_is_particle(paf, pa, ctime, vec);
									if(paf->stype==PAF_VECT) {
										where_is_particle(paf, pa, ctime+1.0f, vec1);
										
										VecSubf(vec1, vec1, vec);
										q2= vectoquat(vec1, ob->trackflag, ob->upflag);
							
										QuatToMat3(q2, mat);
										Mat4CpyMat4(tmat, newob->obmat);
										Mat4MulMat43(newob->obmat, tmat, mat);
									}

									VECCOPY(newob->obmat[3], vec);
								}
							}
						}						
					}
					break;
				}
				ob= ob->parent;
			}
		}
		base= base->next;
	}
}


void free_duplilist()
{
	Object *ob;
	
	while( (ob= duplilist.first) ) {
		BLI_remlink(&duplilist, ob);
		MEM_freeN(ob);
	}
	
}

void make_duplilist(Scene *sce, Object *ob)
{
	PartEff *paf;
	
	if(ob->transflag & OB_DUPLI) {
		if(ob->transflag & OB_DUPLIVERTS) {
			if(ob->type==OB_MESH) {
				if(ob->transflag & OB_DUPLIVERTS) {
					if( (paf=give_parteff(ob)) ) particle_duplilist(sce, ob, paf);
					else vertex_duplilist(sce, ob);
				}
			}
			else if(ob->type==OB_FONT) {
				font_duplilist(ob);
			}
		}
		else if(ob->transflag & OB_DUPLIFRAMES) frames_duplilist(ob);
	}
}

