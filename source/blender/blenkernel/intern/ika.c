
/*  ika.c      MIXED MODEL
 * 
 *  april 96
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
#include <stdlib.h>

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_ika_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
/* functions */
#include "BKE_blender.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_ika.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Let's go! */
#define TOLER 0.000076
#define CLAMP(a, b, c) if((a)<(b)) (a)=(b); else if((a)>(c)) (a)=(c) 


void unlink_ika(Ika *ika)
{
	/* loskoppelen: */
	

}

/* niet Ika zelf vrijgeven */
void free_ika(Ika *ika)
{

	/* unimplemented!!! */
	unlink_ika(ika); 
	
	BLI_freelistN(&ika->limbbase);
	
	if(ika->def) MEM_freeN(ika->def);
}

Ika *add_ika()
{
	Ika *ika;
	
	ika= alloc_libblock(&G.main->ika, ID_IK, "Ika");
	ika->flag = IK_GRABEFF | IK_XYCONSTRAINT;

	ika->xyconstraint= 0.5f;
	ika->mem= 0.3f;
	ika->iter= 6;
	
	return ika;
}

Ika *copy_ika(Ika *ika)
{
	Ika *ikan;
	
	ikan= copy_libblock(ika);
	
	duplicatelist(&ikan->limbbase, &ika->limbbase);

	ikan->def= MEM_dupallocN(ikan->def);
	
	return ikan;
}

void make_local_ika(Ika *ika)
{
	Object *ob;
	Ika *ikan;
	int local=0, lib=0;
	
	/* - zijn er alleen lib users: niet doen
	 * - zijn er alleen locale users: flag zetten
	 * - mixed: copy
	 */
	
	if(ika->id.lib==0) return;
	if(ika->id.us==1) {
		ika->id.lib= 0;
		ika->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ika, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==ika) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		ika->id.lib= 0;
		ika->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ika, 0);
	}
	else if(local && lib) {
		ikan= copy_ika(ika);
		ikan->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==ika) {
				
				if(ob->id.lib==0) {
					ob->data= ikan;
					ikan->id.us++;
					ika->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

int count_limbs(Object *ob)
{
	int tot=0;
	Ika *ika;
	Limb *li;
	
	if(ob->type!=OB_IKA) return 0;
	ika= ob->data;
	
	li= ika->limbbase.first;
	while(li) {
		tot++;
		li= li->next;
	}
	return tot;
}

/* ************************************************** */


/* aan hand van eff[] de len en alpha */
void calc_limb(Limb *li)
{
	Limb *prev= li;
	float vec[2], alpha= 0.0;
	
	/* alpha van 'parents' */
	while( (prev=prev->prev) ) {
	  alpha+= prev->alpha;
	}
	
	if(li->prev) {
		vec[0]= -li->prev->eff[0];
		vec[1]= -li->prev->eff[1];
	}
	else vec[0]= vec[1]= 0.0;
	
	vec[0]+= li->eff[0];
	vec[1]+= li->eff[1];

	li->alpha= (float)atan2(vec[1], vec[0]) - alpha;
	li->len= (float)sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

}

/* aan hand van len en alpha worden de eindpunten berekend */
void calc_ika(Ika *ika, Limb *li)
{
	float alpha=0.0, co, si;

	if(li) {		
		Limb *prev= li;
		while((prev=prev->prev)) {
		  alpha+= prev->alpha;
		}
	}
	else li= ika->limbbase.first;

	while(li) {
		if(li->alpha != li->alpha) li->alpha= 0.0f;	/* NaN patch */

		alpha+= li->alpha;

		co= (float)cos(alpha);
		si= (float)sin(alpha);
		
		li->eff[0]= co*li->len;
		li->eff[1]= si*li->len;
		
		if(li->prev) {
			li->eff[0]+= li->prev->eff[0];
			li->eff[1]+= li->prev->eff[1];
		}
		
		if(li->next==0) {
			ika->eff[0]= li->eff[0];
			ika->eff[1]= li->eff[1];
		}
		
		li= li->next;
	}
}

void init_defstate_ika(Object *ob)
{
	Ika *ika;
	Limb *li;
	
	ika= ob->data;
	ika->totx= 0.0;
	ika->toty= 0.0;
	li= ika->limbbase.first;
	
	calc_ika(ika, 0);	/* correcte eindpunten */
	
	while(li) {
		li->alphao= li->alpha;
		li->leno= li->len;
		
		li= li->next;
	}
	ika->eff[2]= 0.0;
	VecMat4MulVecfl(ika->effg, ob->obmat, ika->eff);
}

void itterate_limb(Ika *ika, Limb *li)
{
	float da, n1[2], n2[2], len1, len2;
	
	if(li->prev) {
		n1[0]= ika->eff[0] - li->prev->eff[0];
		n1[1]= ika->eff[1] - li->prev->eff[1];
		n2[0]= ika->effn[0] - li->prev->eff[0];
		n2[1]= ika->effn[1] - li->prev->eff[1];
	}
	else {
		n1[0]= ika->eff[0];
		n1[1]= ika->eff[1];
		n2[0]= ika->effn[0];
		n2[1]= ika->effn[1];
	}
	len1= (float)sqrt(n1[0]*n1[0] + n1[1]*n1[1]);
	len2= (float)sqrt(n2[0]*n2[0] + n2[1]*n2[1]);

	da= (1.0f-li->fac)*saacos( (n1[0]*n2[0]+n1[1]*n2[1])/(len1*len2) );

	if(n1[0]*n2[1] < n1[1]*n2[0]) da= -da;
	
	li->alpha+= da;
	
}

void rotate_ika(Object *ob, Ika *ika)
{
	Limb *li;
	float len2, da, n1[2], n2[2];
	
	/* terug roteren */
	euler_rot(ob->rot, -ika->toty, 'y');
	ika->toty= 0.0;
	
	where_is_object(ob);
	
	Mat4Invert(ob->imat, ob->obmat);
	VecMat4MulVecfl(ika->effn, ob->imat, ika->effg);
	
	li= ika->limbbase.last;
	if(li==0) return;
	
	n1[0]= ika->eff[0];
	n2[0]= ika->effn[0];
	n2[1]= ika->effn[2];
	
	len2= (float)sqrt(n2[0]*n2[0] + n2[1]*n2[1]);
	
	if(len2>TOLER) {
		da= (n2[0])/(len2);
		if(n1[0]<0.0) da= -da;
		
		/* als de x comp bijna nul is kan dit gebeuren */
		if(da<=-1.0+TOLER || da>=1.0) ;
		else {
		
			da= saacos( da );
			if(n1[0]*n2[1] > 0.0) da= -da;
	
			euler_rot(ob->rot, da, 'y');
			ika->toty= da;
		}
	}
}

void rotate_ika_xy(Object *ob, Ika *ika)
{
	Limb *li;
	float ang, da, n1[3], n2[3], axis[3], quat[4];
	
	/* terug roteren */
	euler_rot(ob->rot, -ika->toty, 'y');
	euler_rot(ob->rot, -ika->totx, 'x');
	
	where_is_object(ob);

	Mat4Invert(ob->imat, ob->obmat);
	VecMat4MulVecfl(ika->effn, ob->imat, ika->effg);
	
	li= ika->limbbase.last;
	if(li==0) return;
	
	/* ika->eff = old situation */
	/* ika->effn = desired situation */
	
	*(n1)= *(ika->effn);
	*(n1+1)= *(ika->effn+1);
	*(n1+2)= 0.0;

	*(n2)= *(ika->effn);
	*(n2+1)= *(ika->effn+1);
	*(n2+2)= *(ika->effn+2);
	
	Normalise(n1);
	Normalise(n2);

	ang= n1[0]*n2[0] + n1[1]*n2[1] + n1[2]*n2[2];
	ang= saacos(ang);
		
	if(ang<-0.0000001 || ang>0.00000001) {
		Crossf(axis, n1, n2);
		Normalise(axis);
		quat[0]= (float)cos(0.5*ang);
		da= (float)sin(0.5*ang);
		quat[1]= da*axis[0];
		quat[2]= da*axis[1];
		quat[3]= da*axis[2];
	
		QuatToEul(quat, axis);

		ika->totx= axis[0];
		CLAMP(ika->totx, -ika->xyconstraint, ika->xyconstraint);
		ika->toty= axis[1];
		CLAMP(ika->toty, -ika->xyconstraint, ika->xyconstraint);
	}

	euler_rot(ob->rot, ika->totx, 'x');
	euler_rot(ob->rot, ika->toty, 'y');
}

void itterate_ika(Object *ob)
{
	Ika *ika;
	Limb *li;
	int it = 0;

	ika= ob->data;
	if((ika->flag & IK_GRABEFF)==0) return;

	disable_where_script(1);
	/* memory: grote tijdsprongen afvangen */
	it= abs(ika->lastfra - G.scene->r.cfra);
	ika->lastfra= G.scene->r.cfra;
	if(it>10) {
		
		/* one itteration extra */
		itterate_ika(ob);
	}
	else {
		li= ika->limbbase.first;
		while(li) {
			li->alpha= (1.0f-ika->mem)*li->alpha + ika->mem*li->alphao;
			if(li->fac==1.0f) li->fac= 0.05f;	/* oude files: kan weg in juni 96 */
			li= li->next;
		}
	}
	calc_ika(ika, 0);
	
	/* effector heeft parent? */
	if(ika->parent) {
		
		if(ika->partype==PAROBJECT) {
			if(ika->parent->ctime != (float) G.scene->r.cfra) where_is_object(ika->parent);
			*(ika->effg)= *(ika->parent->obmat[3]);
			*(ika->effg+1)= *(ika->parent->obmat[3]+1);
			*(ika->effg+2)= *(ika->parent->obmat[3]+2);			
		}
		else {
			what_does_parent1(ika->parent, ika->partype, ika->par1, 0, 0);
			*(ika->effg)= *(workob.obmat[3]);
			*(ika->effg+1)= *(workob.obmat[3]+1);
			*(ika->effg+2)= *(workob.obmat[3]+2);			
		}
	}


	/* y-as goed draaien */
	if(ika->flag & IK_XYCONSTRAINT) 
		rotate_ika_xy(ob, ika);
	else
		rotate_ika(ob, ika);

	it= ika->iter;
	while(it--) {
		
		where_is_object(ob);
		Mat4Invert(ob->imat, ob->obmat);
		VecMat4MulVecfl(ika->effn, ob->imat, ika->effg);
		/* forward: dan gaan ook de eerste limbs */
		li= ika->limbbase.first;
		while(li) {
			
			itterate_limb(ika, li);
			
			/* zet je calc_ika() buiten deze lus: lange kettingen instabiel */
			calc_ika(ika, li);

			li= li->next;
		}

		where_is_object(ob);
		Mat4Invert(ob->imat, ob->obmat);
		VecMat4MulVecfl(ika->effn, ob->imat, ika->effg);

		/* backward */
		li= ika->limbbase.last;
		while(li) {
			
			itterate_limb(ika, li);
			
			/* zet je calc_ika() buiten deze lus: lange kettingen instabiel */
			calc_ika(ika, li);

			li= li->prev;
		}
	}

	disable_where_script(0);
}


void do_all_ikas()
{
	Base *base = 0;
	
	base= G.scene->base.first;
	while(base) {
		
		if(base->object->type==OB_IKA) itterate_ika(base->object);

		base= base->next;
	}
}

void do_all_visible_ikas()
{
	Base *base = 0;
	
	base= G.scene->base.first;
	while(base) {
		if(base->lay & G.scene->lay) {
			if(base->object->type==OB_IKA) itterate_ika(base->object);
		}
		base= base->next;
	}
}

/* ******************** DEFORM ************************ */


void init_skel_deform(Object *par, Object *ob)
{
	Deform *def;
	Ika *ika;
	int a;
	
	/*  deform:
	 * 
	 *  ob_vec * ob_obmat * def_imat (weight fie) * def_obmat * ob_imat = ob_vec'
	 *   
	 *           <----- premat ---->                <---- postmat ---->
	 */
	
	if(par->type!=OB_IKA) return;
	
	Mat4Invert(ob->imat, ob->obmat);

	ika= par->data;
	a= ika->totdef;
	def= ika->def;
	while(a--) {
		
		what_does_parent1(def->ob, def->partype, def->par1, def->par2, def->par3);
		
		Mat4MulMat4(def->premat, ob->obmat, def->imat);
		Mat4MulMat4(def->postmat, workob.obmat, ob->imat);

		def++;
	}
}


void calc_skel_deform(Ika *ika, float *co)
{
	Deform *def;
	int a;
	float totw=0.0, weight, fac, len, vec[3], totvec[3];
	
	def= ika->def;
	if(def==0) return;
	a= ika->totdef;
	totvec[0]=totvec[1]=totvec[2]= 0.0;
	
	while(a--) {
		
		VecMat4MulVecfl(vec, def->premat, co);
		
		len= (float)sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
		
		if(def->vec[0]==0.0f) len= 2.0f*len;
		else len= len + (float)sqrt( (vec[0]+def->vec[0])*(vec[0]+def->vec[0]) + vec[1]*vec[1] + vec[2]*vec[2]);
		
		/* def->vec[0]= len limb */
		
		weight= 1.0f/(0.001f+len);
		weight*= weight;
		weight*= weight;
		weight*= def->fac;

		len -= def->vec[0];

		if(def->dist != 0.0) {
			if(len >= def->dist) {
				weight= 0.0;
			}
			else {
				fac= (def->dist - len)/def->dist;
				weight*= fac;
			}
		}
		if(weight > 0.0) {
			Mat4MulVecfl(def->postmat, vec);
			
			VecMulf(vec, weight);
			VecAddf(totvec, totvec, vec);
		
			totw+= weight;
		}
		def++;
	}
	
	if(totw==0.0) return;
	
	co[0]= totvec[0]/totw;
	co[1]= totvec[1]/totw;
	co[2]= totvec[2]/totw;
	
}
