/*  effect.c        MIX MODEL
 * 
 *  dec 95
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
#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "DNA_listBase.h"
#include "DNA_effect_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_lattice_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_utildefines.h"
#include "BKE_bad_level_calls.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_effect.h"
#include "BKE_key.h"
#include "BKE_ipo.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_blender.h"
#include "BKE_object.h"
#include "BKE_displist.h"
#include "BKE_lattice.h"


Effect *add_effect(int type)
{
	Effect *eff=0;
	BuildEff *bld;
	PartEff *paf;
	WaveEff *wav;
	int a;
	
	switch(type) {
	case EFF_BUILD:
		bld= MEM_callocN(sizeof(BuildEff), "neweff");
		eff= (Effect *)bld;
		
		bld->sfra= 1.0;
		bld->len= 100.0;
		break;
		
	case EFF_PARTICLE:
		paf= MEM_callocN(sizeof(PartEff), "neweff");
		eff= (Effect *)paf;
		
		paf->sta= 1.0;
		paf->end= 100.0;
		paf->lifetime= 50.0;
		for(a=0; a<PAF_MAXMULT; a++) {
			paf->life[a]= 50.0;
			paf->child[a]= 4;
			paf->mat[a]= 1;
		}
		
		paf->totpart= 1000;
		paf->totkey= 8;
		paf->staticstep= 5;
		paf->defvec[2]= 1.0f;
		paf->nabla= 0.05f;
		
		break;
		
	case EFF_WAVE:
		wav= MEM_callocN(sizeof(WaveEff), "neweff");
		eff= (Effect *)wav;
		
		wav->flag |= (WAV_X+WAV_Y+WAV_CYCL);
		
		wav->height= 0.5f;
		wav->width= 1.5f;
		wav->speed= 0.5f;
		wav->narrow= 1.5f;
		wav->lifetime= 0.0f;
		wav->damp= 10.0f;
		
		break;
	}
	
	eff->type= eff->buttype= type;
	eff->flag |= SELECT;
	
	return eff;
}

void free_effect(Effect *eff)
{
	PartEff *paf;
	
	if(eff->type==EFF_PARTICLE) {
		paf= (PartEff *)eff;
		if(paf->keys) MEM_freeN(paf->keys);
	}
	MEM_freeN(eff);	
}


void free_effects(ListBase *lb)
{
	Effect *eff;
	
	eff= lb->first;
	while(eff) {
		BLI_remlink(lb, eff);
		free_effect(eff);
		eff= lb->first;
	}
}

Effect *copy_effect(Effect *eff) 
{
	Effect *effn;
	
	effn= MEM_dupallocN(eff);
	if(effn->type==EFF_PARTICLE) ((PartEff *)effn)->keys= 0;

	return effn;	
}

void copy_act_effect(Object *ob)
{
	/* return de aktieve eff gekopieerd */
	Effect *effn, *eff;
	
	eff= ob->effect.first;
	while(eff) {
		if(eff->flag & SELECT) {
			
			effn= copy_effect(eff);
			BLI_addtail(&ob->effect, effn);
			
			eff->flag &= ~SELECT;
			return;
			
		}
		eff= eff->next;
	}
	
	/* als tie hier komt: new effect */
	eff= add_effect(EFF_BUILD);
	BLI_addtail(&ob->effect, eff);
			
}

void copy_effects(ListBase *lbn, ListBase *lb)
{
	Effect *eff, *effn;

	lbn->first= lbn->last= 0;

	eff= lb->first;
	while(eff) {
		effn= copy_effect(eff);
		BLI_addtail(lbn, effn);
		
		eff= eff->next;
	}
	
}

void deselectall_eff(Object *ob)
{
	Effect *eff= ob->effect.first;
	
	while(eff) {
		eff->flag &= ~SELECT;
		eff= eff->next;
	}
}

void set_buildvars(Object *ob, int *start, int *end)
{
	BuildEff *bld;
	float ctime;
	
	bld= ob->effect.first;
	while(bld) {
		if(bld->type==EFF_BUILD) {
			ctime= bsystem_time(ob, 0, (float)G.scene->r.cfra, bld->sfra-1.0f);
			if(ctime < 0.0) {
				*end= *start;
			}
			else if(ctime < bld->len) {
				*end= *start+ (int)((*end - *start)*ctime/bld->len);
			}
			
			return;
		}
		bld= bld->next;
	}
}

/* ***************** PARTICLES ***************** */

Particle *new_particle(PartEff *paf)
{
	static Particle *pa;
	static int cur;

	/* afspraak: als paf->keys==0: alloc */	
	if(paf->keys==0) {
		pa= paf->keys= MEM_callocN( paf->totkey*paf->totpart*sizeof(Particle), "particlekeys" );
		cur= 0;
	}
	else {
		if(cur && cur<paf->totpart) pa+=paf->totkey;
		cur++;
	}
	return pa;
}

PartEff *give_parteff(Object *ob)
{
	PartEff *paf;
	
	paf= ob->effect.first;
	while(paf) {
		if(paf->type==EFF_PARTICLE) return paf;
		paf= paf->next;
	}
	return 0;
}

void where_is_particle(PartEff *paf, Particle *pa, float ctime, float *vec)
{
	Particle *p[4];
	float dt, t[4];
	int a;
	
	if(paf->totkey==1) {
		VECCOPY(vec, pa->co);
		return;
	}
	
	/* eerst op zoek naar de eerste particlekey */
	a= (int)((paf->totkey-1)*(ctime-pa->time)/pa->lifetime);
	if(a>=paf->totkey) a= paf->totkey-1;
	
	pa+= a;
	
	if(a>0) p[0]= pa-1; else p[0]= pa;
	p[1]= pa;
	
	if(a+1<paf->totkey) p[2]= pa+1; else p[2]= pa;
	if(a+2<paf->totkey) p[3]= pa+2; else p[3]= p[2];
	
	if(p[1]==p[2]) dt= 0.0;
	else dt= (ctime-p[1]->time)/(p[2]->time - p[1]->time);

	if(paf->flag & PAF_BSPLINE) set_four_ipo(dt, t, KEY_BSPLINE);
	else set_four_ipo(dt, t, KEY_CARDINAL);

	vec[0]= t[0]*p[0]->co[0] + t[1]*p[1]->co[0] + t[2]*p[2]->co[0] + t[3]*p[3]->co[0];
	vec[1]= t[0]*p[0]->co[1] + t[1]*p[1]->co[1] + t[2]*p[2]->co[1] + t[3]*p[3]->co[1];
	vec[2]= t[0]*p[0]->co[2] + t[1]*p[1]->co[2] + t[2]*p[2]->co[2] + t[3]*p[3]->co[2];

}


void particle_tex(MTex *mtex, PartEff *paf, float *co, float *no)
{				
	extern float Tin, Tr, Tg, Tb;
	float old;
	
	externtex(mtex, co);
	
	if(paf->texmap==PAF_TEXINT) {
		Tin*= paf->texfac;
		no[0]+= Tin*paf->defvec[0];
		no[1]+= Tin*paf->defvec[1];
		no[2]+= Tin*paf->defvec[2];
	}
	else if(paf->texmap==PAF_TEXRGB) {
		no[0]+= (Tr-0.5f)*paf->texfac;
		no[1]+= (Tg-0.5f)*paf->texfac;
		no[2]+= (Tb-0.5f)*paf->texfac;
	}
	else {	/* PAF_TEXGRAD */
		
		old= Tin;
		co[0]+= paf->nabla;
		externtex(mtex, co);
		no[0]+= (old-Tin)*paf->texfac;
		
		co[0]-= paf->nabla;
		co[1]+= paf->nabla;
		externtex(mtex, co);
		no[1]+= (old-Tin)*paf->texfac;
		
		co[1]-= paf->nabla;
		co[2]+= paf->nabla;
		externtex(mtex, co);
		no[2]+= (old-Tin)*paf->texfac;
		
	}
}

void make_particle_keys(int depth, int nr, PartEff *paf, Particle *part, float *force, int deform, MTex *mtex)
{
	Particle *pa, *opa = NULL;
	float damp, deltalife;
	int b, rt1, rt2;
	
	damp= 1.0f-paf->damp;
	pa= part;
	
	/* startsnelheid: random */
	if(paf->randfac!=0.0) {
		pa->no[0]+= (float)(paf->randfac*( BLI_drand() -0.5));
		pa->no[1]+= (float)(paf->randfac*( BLI_drand() -0.5));
		pa->no[2]+= (float)(paf->randfac*( BLI_drand() -0.5));
	}
	
	/* startsnelheid: texture */
	if(mtex && paf->texfac!=0.0) {
		particle_tex(mtex, paf, pa->co, pa->no);
	}
	
	/* keys */
	if(paf->totkey>1) {
		
		deltalife= pa->lifetime/(paf->totkey-1);
		opa= pa;
		pa++;
		
		b= paf->totkey-1;
		while(b--) {
			/* nieuwe tijd */
			pa->time= opa->time+deltalife;
			
			/* nieuwe plek */
			pa->co[0]= opa->co[0] + deltalife*opa->no[0];
			pa->co[1]= opa->co[1] + deltalife*opa->no[1];
			pa->co[2]= opa->co[2] + deltalife*opa->no[2];
			
			/* nieuwe snelheid */
			pa->no[0]= opa->no[0] + deltalife*force[0];
			pa->no[1]= opa->no[1] + deltalife*force[1];
			pa->no[2]= opa->no[2] + deltalife*force[2];

			/* snelheid: texture */
			if(mtex && paf->texfac!=0.0) {
				particle_tex(mtex, paf, pa->co, pa->no);
			}
			if(damp!=1.0) {
				pa->no[0]*= damp;
				pa->no[1]*= damp;
				pa->no[2]*= damp;
			}
	
			opa= pa;
			pa++;
			/* opa wordt onderin ook gebruikt */
		}
	}

	if(deform) {
		/* alle keys deformen */
		pa= part;
		b= paf->totkey;
		while(b--) {
			calc_latt_deform(pa->co);
			pa++;
		}
	}
	
	/* de grote vermenigvuldiging */
	if(depth<PAF_MAXMULT && paf->mult[depth]!=0.0) {
		
		/* uit gemiddeld 'mult' deel van de particles ontstaan 'child' nieuwe */
		damp = (float)nr;
		rt1= (int)(damp*paf->mult[depth]);
		rt2= (int)((damp+1.0)*paf->mult[depth]);
		if(rt1!=rt2) {
			
			for(b=0; b<paf->child[depth]; b++) {
				pa= new_particle(paf);
				*pa= *opa;
				pa->lifetime= paf->life[depth];
				if(paf->randlife!=0.0) {
					pa->lifetime*= 1.0f+ (float)(paf->randlife*( BLI_drand() - 0.5));
				}
				pa->mat_nr= paf->mat[depth];
				
				make_particle_keys(depth+1, b, paf, pa, force, deform, mtex);
			}
		}
	}
}

void init_mv_jit(float *jit, int num)
{
	float *jit2, x, rad1, rad2, rad3;
	int i;

	if(num==0) return;

	rad1= (float)(1.0/sqrt((float)num));
	rad2= (float)(1.0/((float)num));
	rad3= (float)sqrt((float)num)/((float)num);

	BLI_srand(31415926 + num);
	x= 0;
	for(i=0; i<2*num; i+=2) {
	
		jit[i]= x+ (float)(rad1*(0.5-BLI_drand()));
		jit[i+1]= ((float)i/2)/num +(float)(rad1*(0.5-BLI_drand()));
		
		jit[i]-= (float)floor(jit[i]);
		jit[i+1]-= (float)floor(jit[i+1]);
		
		x+= rad3;
		x -= (float)floor(x);
	}

	jit2= MEM_mallocN(12 + 2*sizeof(float)*num, "initjit");

	for (i=0 ; i<4 ; i++) {
		RE_jitterate1(jit, jit2, num, rad1);
		RE_jitterate1(jit, jit2, num, rad1);
		RE_jitterate2(jit, jit2, num, rad2);
	}
	MEM_freeN(jit2);
}


void give_mesh_mvert(Mesh *me, int nr, float *co, short *no)
{
	static float *jit=0;
	static int jitlevel=1;
	MVert *mvert;
	MFace *mface;
	float u, v, *v1, *v2, *v3, *v4;
	int curface, curjit;
	short *n1, *n2, *n3, *n4;
	
	/* signal */
	if(me==0) {
		if(jit) MEM_freeN(jit);
		jit= 0;
		return;
	}
	
	if(me->totface==0 || nr<me->totvert) {
		mvert= me->mvert + (nr % me->totvert);
		VECCOPY(co, mvert->co);
		VECCOPY(no, mvert->no);
	}
	else {
		
		nr-= me->totvert;
		
		if(jit==0) {
			jitlevel= nr/me->totface;
			if(jitlevel==0) jitlevel= 1;
			if(jitlevel>100) jitlevel= 100;
			
			jit= MEM_callocN(2+ jitlevel*2*sizeof(float), "jit");
			init_mv_jit(jit, jitlevel);
			
		}

		curjit= nr/me->totface;
		curjit= curjit % jitlevel;

		curface= nr % me->totface;
		
		mface= me->mface;
		mface+= curface;
		
		v1= (me->mvert+(mface->v1))->co;
		v2= (me->mvert+(mface->v2))->co;
		n1= (me->mvert+(mface->v1))->no;
		n2= (me->mvert+(mface->v2))->no;
		if(mface->v3==0) {
			v3= (me->mvert+(mface->v2))->co;
			v4= (me->mvert+(mface->v1))->co;
			n3= (me->mvert+(mface->v2))->no;
			n4= (me->mvert+(mface->v1))->no;
		}
		else if(mface->v4==0) {
			v3= (me->mvert+(mface->v3))->co;
			v4= (me->mvert+(mface->v1))->co;
			n3= (me->mvert+(mface->v3))->no;
			n4= (me->mvert+(mface->v1))->no;
		}
		else {
			v3= (me->mvert+(mface->v3))->co;
			v4= (me->mvert+(mface->v4))->co;
			n3= (me->mvert+(mface->v3))->no;
			n4= (me->mvert+(mface->v4))->no;
		}

		u= jit[2*curjit];
		v= jit[2*curjit+1];

		co[0]= (float)((1.0-u)*(1.0-v)*v1[0] + (1.0-u)*(v)*v2[0] + (u)*(v)*v3[0] + (u)*(1.0-v)*v4[0]);
		co[1]= (float)((1.0-u)*(1.0-v)*v1[1] + (1.0-u)*(v)*v2[1] + (u)*(v)*v3[1] + (u)*(1.0-v)*v4[1]);
		co[2]= (float)((1.0-u)*(1.0-v)*v1[2] + (1.0-u)*(v)*v2[2] + (u)*(v)*v3[2] + (u)*(1.0-v)*v4[2]);
		
		no[0]= (short)((1.0-u)*(1.0-v)*n1[0] + (1.0-u)*(v)*n2[0] + (u)*(v)*n3[0] + (u)*(1.0-v)*n4[0]);
		no[1]= (short)((1.0-u)*(1.0-v)*n1[1] + (1.0-u)*(v)*n2[1] + (u)*(v)*n3[1] + (u)*(1.0-v)*n4[1]);
		no[2]= (short)((1.0-u)*(1.0-v)*n1[2] + (1.0-u)*(v)*n2[2] + (u)*(v)*n3[2] + (u)*(1.0-v)*n4[2]);
		
	}
}


void build_particle_system(Object *ob)
{
	Object *par;
	PartEff *paf;
	Particle *pa;
	Mesh *me;
	MVert *mvert;
	MTex *mtexmove=0;
	Material *ma;
	float framelenont, ftime, dtime, force[3], imat[3][3], vec[3];
	float fac, prevobmat[4][4], sfraont, co[3];
	int deform=0, a, cur, cfraont, cfralast, totpart;
	short no[3];
	
	if(ob->type!=OB_MESH) return;
	me= ob->data;
	if(me->totvert==0) return;
	
	ma= give_current_material(ob, 1);
	if(ma) {
		mtexmove= ma->mtex[7];
	}
	
	paf= give_parteff(ob);
	if(paf==0) return;

	waitcursor(1);
	
	disable_speed_curve(1);
	
	/* alle particles genereren */
	if(paf->keys) MEM_freeN(paf->keys);
	paf->keys= 0;
	new_particle(paf);	

	cfraont= G.scene->r.cfra;
	cfralast= -1000;
	framelenont= G.scene->r.framelen;
	G.scene->r.framelen= 1.0;
	sfraont= ob->sf;
	ob->sf= 0.0;
	
	/* mult generaties? */
	totpart= paf->totpart;
	for(a=0; a<PAF_MAXMULT; a++) {
		if(paf->mult[a]!=0.0) {
			/* interessante formule! opdezewijze is na 'x' generaties het totale aantal paf->totpart */
			totpart= (int)(totpart / (1.0+paf->mult[a]*paf->child[a]));
		}
		else break;
	}

	ftime= paf->sta;
	dtime= (paf->end - paf->sta)/totpart;
	
	/* hele hiera onthouden */
	par= ob;
	while(par) {
		pushdata(par, sizeof(Object));
		par= par->parent;
	}

	/* alles op eerste frame zetten */
	G.scene->r.cfra= cfralast= (int)floor(ftime);
	par= ob;
	while(par) {
		/* do_ob_ipo(par); */
		do_ob_key(par);
		par= par->parent;
	}
	do_mat_ipo(ma);
	
	if((paf->flag & PAF_STATIC)==0) {
		where_is_object(ob);
		Mat4CpyMat4(prevobmat, ob->obmat);
		Mat4Invert(ob->imat, ob->obmat);
		Mat3CpyMat4(imat, ob->imat);
	}
	else {
		Mat4One(prevobmat);
		Mat3One(imat);
	}
	
	BLI_srand(paf->seed);
	
	/* gaat anders veuls te hard */
	force[0]= paf->force[0]*0.05f;
	force[1]= paf->force[1]*0.05f;
	force[2]= paf->force[2]*0.05f;
	
	deform= (ob->parent && ob->parent->type==OB_LATTICE);
	if(deform) init_latt_deform(ob->parent, 0);
	
	/* init */
	give_mesh_mvert(me, totpart, co, no);
	
	for(a=0; a<totpart; a++, ftime+=dtime) {
		
		pa= new_particle(paf);
		pa->time= ftime;
		
		/* ob op juiste tijd zetten */
		
		if((paf->flag & PAF_STATIC)==0) {
		
			cur= (int)floor(ftime) + 1 ;		/* + 1 heeft een reden: (obmat/prevobmat) anders beginnen b.v. komeetstaartjes te laat */
			if(cfralast != cur) {
				G.scene->r.cfra= cfralast= cur;
	
				/* later bijgevoegd: blur? */
				bsystem_time(ob, ob->parent, (float)G.scene->r.cfra, 0.0);
				
				par= ob;
				while(par) {
					/* do_ob_ipo(par); */
					par->ctime= -1234567.0;
					do_ob_key(par);
					par= par->parent;
				}
				do_mat_ipo(ma);
				Mat4CpyMat4(prevobmat, ob->obmat);
				where_is_object(ob);
				Mat4Invert(ob->imat, ob->obmat);
				Mat3CpyMat4(imat, ob->imat);
			}
		}
		/* coordinaat ophalen */
		if(paf->flag & PAF_FACE) give_mesh_mvert(me, a, co, no);
		else {
			mvert= me->mvert + (a % me->totvert);
			VECCOPY(co, mvert->co);
			VECCOPY(no, mvert->no);
		}
		
		VECCOPY(pa->co, co);
		
		if(paf->flag & PAF_STATIC);
		else {
			Mat4MulVecfl(ob->obmat, pa->co);
		
			VECCOPY(vec, co);
			Mat4MulVecfl(prevobmat, vec);
			
			/* eerst even startsnelheid: object */
			VecSubf(pa->no, pa->co, vec);
			VecMulf(pa->no, paf->obfac);
			
			/* nu juiste interframe co berekenen */	
			fac= (ftime- (float)floor(ftime));
			pa->co[0]= fac*pa->co[0] + (1.0f-fac)*vec[0];
			pa->co[1]= fac*pa->co[1] + (1.0f-fac)*vec[1];
			pa->co[2]= fac*pa->co[2] + (1.0f-fac)*vec[2];
		}
		
		/* startsnelheid: normaal */
		if(paf->normfac!=0.0) {
			/* sp= mvert->no; */
				/* transpose ! */
			vec[0]= imat[0][0]*no[0] + imat[0][1]*no[1] + imat[0][2]*no[2];
			vec[1]= imat[1][0]*no[0] + imat[1][1]*no[1] + imat[1][2]*no[2];
			vec[2]= imat[2][0]*no[0] + imat[2][1]*no[1] + imat[2][2]*no[2];		
		
			Normalise(vec);
			VecMulf(vec, paf->normfac);
			VecAddf(pa->no, pa->no, vec);
		}
		pa->lifetime= paf->lifetime;
		if(paf->randlife!=0.0) {
			pa->lifetime*= 1.0f+ (float)(paf->randlife*( BLI_drand() - 0.5));
		}
		pa->mat_nr= 1;
		
		make_particle_keys(0, a, paf, pa, force, deform, mtexmove);
	}
	
	if(deform) end_latt_deform();
		
	/* restore */
	G.scene->r.cfra= cfraont;
	G.scene->r.framelen= framelenont;
	give_mesh_mvert(0, 0, 0, 0);


	/* hele hiera terug */
	par= ob;
	while(par) {
		popfirst(par);
		/* geen ob->ipo doen: insertkey behouden */
		do_ob_key(par);
		par= par->parent;
	}

	/* restore: NA popfirst */
	ob->sf= sfraont;

	disable_speed_curve(0);

	waitcursor(0);

}

/* ************* WAVE **************** */

void calc_wave_deform(WaveEff *wav, float ctime, float *co)
{
	/* co is in lokale coords */
	float lifefac, x, y, amplit;
	
	/* mag eigenlijk niet voorkomen */
	if((wav->flag & (WAV_X+WAV_Y))==0) return;	

	lifefac= wav->height;
	
	if( wav->lifetime!=0.0) {
		x= ctime - wav->timeoffs;
		if(x>wav->lifetime) {
			
			lifefac= x-wav->lifetime;
			
			if(lifefac > wav->damp) lifefac= 0.0;
			else lifefac= (float)(wav->height*(1.0 - sqrt(lifefac/wav->damp)));
		}
	}
	if(lifefac==0.0) return;

	x= co[0]-wav->startx;
	y= co[1]-wav->starty;

	if(wav->flag & WAV_X) {
		if(wav->flag & WAV_Y) amplit= (float)sqrt( (x*x + y*y));
		else amplit= x;
	}
	else amplit= y;
	
	/* zo maaktie mooie cirkels */
	amplit-= (ctime-wav->timeoffs)*wav->speed;

	if(wav->flag & WAV_CYCL) {
		amplit = (float)fmod(amplit-wav->width, 2.0*wav->width) + wav->width;
	}

	/* GAUSSIAN */
	
	if(amplit> -wav->width && amplit<wav->width) {
	
		amplit = amplit*wav->narrow;
		amplit= (float)(1.0/exp(amplit*amplit) - wav->minfac);

		co[2]+= lifefac*amplit;
	}
}

void object_wave(Object *ob)
{
	WaveEff *wav;
	DispList *dl;
	Mesh *me;
	MVert *mvert;
	float *fp, ctime;
	int a, first;
	
	/* is er een mave */
	wav= ob->effect.first;
	while(wav) {
		if(wav->type==EFF_WAVE) break;
		wav= wav->next;
	}
	if(wav==0) return;
	
	if(ob->type==OB_MESH) {

		ctime= bsystem_time(ob, 0, (float)G.scene->r.cfra, 0.0);
		first= 1;
		
		me= ob->data;
		dl= find_displist_create(&ob->disp, DL_VERTS);

		if(dl->verts) MEM_freeN(dl->verts);
		dl->nr= me->totvert;
		dl->verts= MEM_mallocN(3*4*me->totvert, "wave");

		wav= ob->effect.first;
		while(wav) {
			if(wav->type==EFF_WAVE) {
				
				/* voorberekenen */
				wav->minfac= (float)(1.0/exp(wav->width*wav->narrow*wav->width*wav->narrow));
				if(wav->damp==0) wav->damp= 10.0f;
				
				mvert= me->mvert;
				fp= dl->verts;
				
				for(a=0; a<me->totvert; a++, mvert++, fp+=3) {
					if(first) VECCOPY(fp, mvert->co);
					calc_wave_deform(wav, ctime, fp);
				}
				first= 0;
			}
			wav= wav->next;
		}
	}
}
