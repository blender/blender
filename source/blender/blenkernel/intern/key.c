
/*  key.c      
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

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define KEY_BPOINT		1
#define KEY_BEZTRIPLE	2

	// old defines from DNA_ipo_types.h for data-type
#define IPO_FLOAT		4
#define IPO_BEZTRIPLE	100
#define IPO_BPOINT		101

int slurph_opt= 1;


void free_key(Key *key)
{
	KeyBlock *kb;
	
	BKE_free_animdata((ID *)key);
	
	while( (kb= key->block.first) ) {
		
		if(kb->data) MEM_freeN(kb->data);
		
		BLI_remlink(&key->block, kb);
		MEM_freeN(kb);
	}
	
}

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */

Key *add_key(ID *id)	/* common function */
{
	Key *key;
	char *el;
	
	key= alloc_libblock(&G.main->key, ID_KE, "Key");
	
	key->type= KEY_NORMAL;
	key->from= id;
	
	// XXX the code here uses some defines which will soon be depreceated...
	if( GS(id->name)==ID_ME) {
		el= key->elemstr;
		
		el[0]= 3;
		el[1]= IPO_FLOAT;
		el[2]= 0;
		
		key->elemsize= 12;
	}
	else if( GS(id->name)==ID_LT) {
		el= key->elemstr;
		
		el[0]= 3;
		el[1]= IPO_FLOAT;
		el[2]= 0;
		
		key->elemsize= 12;
	}
	else if( GS(id->name)==ID_CU) {
		el= key->elemstr;
		
		el[0]= 4;
		el[1]= IPO_BPOINT;
		el[2]= 0;
		
		key->elemsize= 16;
	}
	
	return key;
}

Key *copy_key(Key *key)
{
	Key *keyn;
	KeyBlock *kbn, *kb;
	
	if(key==0) return 0;
	
	keyn= copy_libblock(key);
	
#if 0 // XXX old animation system
	keyn->ipo= copy_ipo(key->ipo);
#endif // XXX old animation system
	
	BLI_duplicatelist(&keyn->block, &key->block);
	
	kb= key->block.first;
	kbn= keyn->block.first;
	while(kbn) {
		
		if(kbn->data) kbn->data= MEM_dupallocN(kbn->data);
		if( kb==key->refkey ) keyn->refkey= kbn;
		
		kbn= kbn->next;
		kb= kb->next;
	}
	
	return keyn;
}

void make_local_key(Key *key)
{

    /* - only lib users: do nothing
    * - only local users: set flag
    * - mixed: make copy
    */
    if(key==0) return;
	
	key->id.lib= 0;
	new_id(0, (ID *)key, 0);

#if 0 // XXX old animation system
	make_local_ipo(key->ipo);
#endif // XXX old animation system
}

/* Sort shape keys and Ipo curves after a change.  This assumes that at most
 * one key was moved, which is a valid assumption for the places it's
 * currently being called.
 */

void sort_keys(Key *key)
{
	KeyBlock *kb;
	//short i, adrcode;
	//IpoCurve *icu = NULL;
	KeyBlock *kb2;

	/* locate the key which is out of position */ 
	for (kb= key->block.first; kb; kb= kb->next)
		if ((kb->next) && (kb->pos > kb->next->pos))
			break;

	/* if we find a key, move it */
	if (kb) {
		kb = kb->next; /* next key is the out-of-order one */
		BLI_remlink(&key->block, kb);
		
		/* find the right location and insert before */
		for (kb2=key->block.first; kb2; kb2= kb2->next) {
			if (kb2->pos > kb->pos) {
				BLI_insertlink(&key->block, kb2->prev, kb);
				break;
			}
		}
		
		/* if more than one Ipo curve, see if this key had a curve */
#if 0 // XXX old animation system
		if(key->ipo && key->ipo->curve.first != key->ipo->curve.last ) {
			for(icu= key->ipo->curve.first; icu; icu= icu->next) {
				/* if we find the curve, remove it and reinsert in the 
				 right place */
				if(icu->adrcode==kb->adrcode) {
					IpoCurve *icu2;
					BLI_remlink(&key->ipo->curve, icu);
					for(icu2= key->ipo->curve.first; icu2; icu2= icu2->next) {
						if(icu2->adrcode >= kb2->adrcode) {
							BLI_insertlink(&key->ipo->curve, icu2->prev, icu);
							break;
						}
					}
					break;
				}
			}
		}
		
		/* kb points at the moved key, icu at the moved ipo (if it exists).
		 * go back now and renumber adrcodes */

		/* first new code */
		adrcode = kb2->adrcode;
		for (i = kb->adrcode - adrcode; i >= 0; i--, adrcode++) {
			/* if the next ipo curve matches the current key, renumber it */
			if(icu && icu->adrcode == kb->adrcode ) {
				icu->adrcode = adrcode;
				icu = icu->next;
			}
			/* renumber the shape key */
			kb->adrcode = adrcode;
			kb = kb->next;
		}
#endif // XXX old animation system
	}

	/* new rule; first key is refkey, this to match drawing channels... */
	key->refkey= key->block.first;
}

/**************** do the key ****************/

void key_curve_position_weights(float t, float *data, int type)
{
	float t2, t3, fc;
	
	if(type==KEY_LINEAR) {
		data[0]=		  0.0f;
		data[1]= -t		+ 1.0f;
		data[2]= t;
		data[3]= 		  0.0f;
	}
	else if(type==KEY_CARDINAL) {
		t2= t*t;
		t3= t2*t;
		fc= 0.71f;
		
		data[0]= -fc*t3			+ 2.0f*fc*t2		- fc*t;
		data[1]= (2.0f-fc)*t3	+ (fc-3.0f)*t2					+ 1.0f;
		data[2]= (fc-2.0f)*t3	+ (3.0f-2.0f*fc)*t2	+ fc*t;
		data[3]= fc*t3			- fc*t2;
	}
	else if(type==KEY_BSPLINE) {
		t2= t*t;
		t3= t2*t;

		data[0]= -0.16666666f*t3	+ 0.5f*t2	- 0.5f*t	+ 0.16666666f;
		data[1]= 0.5f*t3			- t2					+ 0.6666666f;
		data[2]= -0.5f*t3			+ 0.5f*t2	+ 0.5f*t	+ 0.16666666f;
		data[3]= 0.16666666f*t3;
	}
}

/* first derivative */
void key_curve_tangent_weights(float t, float *data, int type)
{
	float t2, fc;
	
	if(type==KEY_LINEAR) {
		data[0]= 0.0f;
		data[1]= -1.0f;
		data[2]= 1.0f;
		data[3]= 0.0f;
	}
	else if(type==KEY_CARDINAL) {
		t2= t*t;
		fc= 0.71f;
		
		data[0]= -3.0f*fc*t2		+4.0f*fc*t				- fc;
		data[1]= 3.0f*(2.0f-fc)*t2	+2.0f*(fc-3.0f)*t;
		data[2]= 3.0f*(fc-2.0f)*t2	+2.0f*(3.0f-2.0f*fc)*t	+ fc;
		data[3]= 3.0f*fc*t2			-2.0f*fc*t;
	}
	else if(type==KEY_BSPLINE) {
		t2= t*t;

		data[0]= -0.5f*t2	+ t			- 0.5f;
		data[1]= 1.5f*t2	- 2.0f*t;
		data[2]= -1.5f*t2	+ t			+ 0.5f;
		data[3]= 0.5f*t2;
	}
}

/* second derivative */
void key_curve_normal_weights(float t, float *data, int type)
{
	float fc;
	
	if(type==KEY_LINEAR) {
		data[0]= 0.0f;
		data[1]= 0.0f;
		data[2]= 0.0f;
		data[3]= 0.0f;
	}
	else if(type==KEY_CARDINAL) {
		fc= 0.71f;
		
		data[0]= -6.0f*fc*t			+ 4.0f*fc;
		data[1]= 6.0f*(2.0f-fc)*t	+ 2.0f*(fc-3.0f);
		data[2]= 6.0f*(fc-2.0f)*t	+ 2.0f*(3.0f-2.0f*fc);
		data[3]= 6.0f*fc*t			- 2.0f*fc;
	}
	else if(type==KEY_BSPLINE) {
		data[0]= -1.0f*t	+ 1.0f;
		data[1]= 3.0f*t		- 2.0f;
		data[2]= -3.0f*t	+ 1.0f;
		data[3]= 1.0f*t;
	}
}

static int setkeys(float fac, ListBase *lb, KeyBlock *k[], float *t, int cycl)
{
	/* return 1 means k[2] is the position, return 0 means interpolate */
	KeyBlock *k1, *firstkey;
	float d, dpos, ofs=0, lastpos, temp, fval[4];
	short bsplinetype;

	firstkey= lb->first;
	k1= lb->last;
	lastpos= k1->pos;
	dpos= lastpos - firstkey->pos;

	if(fac < firstkey->pos) fac= firstkey->pos;
	else if(fac > k1->pos) fac= k1->pos;

	k1=k[0]=k[1]=k[2]=k[3]= firstkey;
	t[0]=t[1]=t[2]=t[3]= k1->pos;

	/* if(fac<0.0 || fac>1.0) return 1; */

	if(k1->next==0) return 1;

	if(cycl) {	/* pre-sort */
		k[2]= k1->next;
		k[3]= k[2]->next;
		if(k[3]==0) k[3]=k1;
		while(k1) {
			if(k1->next==0) k[0]=k1;
			k1=k1->next;
		}
		k1= k[1];
		t[0]= k[0]->pos;
		t[1]+= dpos;
		t[2]= k[2]->pos + dpos;
		t[3]= k[3]->pos + dpos;
		fac+= dpos;
		ofs= dpos;
		if(k[3]==k[1]) { 
			t[3]+= dpos; 
			ofs= 2.0f*dpos;
		}
		if(fac<t[1]) fac+= dpos;
		k1= k[3];
	}
	else {		/* pre-sort */
		k[2]= k1->next;
		t[2]= k[2]->pos;
		k[3]= k[2]->next;
		if(k[3]==0) k[3]= k[2];
		t[3]= k[3]->pos;
		k1= k[3];
	}
	
	while( t[2]<fac ) {	/* find correct location */
		if(k1->next==0) {
			if(cycl) {
				k1= firstkey;
				ofs+= dpos;
			}
			else if(t[2]==t[3]) break;
		}
		else k1= k1->next;

		t[0]= t[1]; 
		k[0]= k[1];
		t[1]= t[2]; 
		k[1]= k[2];
		t[2]= t[3]; 
		k[2]= k[3];
		t[3]= k1->pos+ofs; 
		k[3]= k1;

		if(ofs>2.1+lastpos) break;
	}
	
	bsplinetype= 0;
	if(k[1]->type==KEY_BSPLINE || k[2]->type==KEY_BSPLINE) bsplinetype= 1;


	if(cycl==0) {
		if(bsplinetype==0) {	/* B spline doesn't go through the control points */
			if(fac<=t[1]) {		/* fac for 1st key */
				t[2]= t[1];
				k[2]= k[1];
				return 1;
			}
			if(fac>=t[2] ) {	/* fac after 2nd key */
				return 1;
			}
		}
		else if(fac>t[2]) {	/* last key */
			fac= t[2];
			k[3]= k[2];
			t[3]= t[2];
		}
	}

	d= t[2]-t[1];
	if(d==0.0) {
		if(bsplinetype==0) {
			return 1;	/* both keys equal */
		}
	}
	else d= (fac-t[1])/d;

	/* interpolation */
	
	key_curve_position_weights(d, t, k[1]->type);

	if(k[1]->type != k[2]->type) {
		key_curve_position_weights(d, fval, k[2]->type);
		
		temp= 1.0f-d;
		t[0]= temp*t[0]+ d*fval[0];
		t[1]= temp*t[1]+ d*fval[1];
		t[2]= temp*t[2]+ d*fval[2];
		t[3]= temp*t[3]+ d*fval[3];
	}

	return 0;

}

static void flerp(int aantal, float *in, float *f0, float *f1, float *f2, float *f3, float *t)	
{
	int a;

	for(a=0; a<aantal; a++) {
		in[a]= t[0]*f0[a]+t[1]*f1[a]+t[2]*f2[a]+t[3]*f3[a];
	}
}

static void rel_flerp(int aantal, float *in, float *ref, float *out, float fac)
{
	int a;
	
	for(a=0; a<aantal; a++) {
		in[a]-= fac*(ref[a]-out[a]);
	}
}

static void cp_key(int start, int end, int tot, char *poin, Key *key, KeyBlock *k, float *weights, int mode)
{
	float ktot = 0.0, kd = 0.0;
	int elemsize, poinsize = 0, a, *ofsp, ofs[32], flagflo=0;
	char *k1, *kref;
	char *cp, elemstr[8];

	if(key->from==NULL) return;

	if( GS(key->from->name)==ID_ME ) {
		ofs[0]= sizeof(MVert);
		ofs[1]= 0;
		poinsize= ofs[0];
	}
	else if( GS(key->from->name)==ID_LT ) {
		ofs[0]= sizeof(BPoint);
		ofs[1]= 0;
		poinsize= ofs[0];
	}
	else if( GS(key->from->name)==ID_CU ) {
		if(mode==KEY_BPOINT) ofs[0]= sizeof(BPoint);
		else ofs[0]= sizeof(BezTriple);
		
		ofs[1]= 0;
		poinsize= ofs[0];
	}


	if(end>tot) end= tot;
	
	k1= k->data;
	kref= key->refkey->data;
	
	if(tot != k->totelem) {
		ktot= 0.0;
		flagflo= 1;
		if(k->totelem) {
			kd= k->totelem/(float)tot;
		}
		else return;
	}

	/* this exception is needed for slurphing */
	if(start!=0) {
		
		poin+= poinsize*start;
		
		if(flagflo) {
			ktot+= start*kd;
			a= (int)floor(ktot);
			if(a) {
				ktot-= a;
				k1+= a*key->elemsize;
			}
		}
		else k1+= start*key->elemsize;
	}	
	
	if(mode==KEY_BEZTRIPLE) {
		elemstr[0]= 1;
		elemstr[1]= IPO_BEZTRIPLE;
		elemstr[2]= 0;
	}
	
	/* just do it here, not above! */
	elemsize= key->elemsize;
	if(mode==KEY_BEZTRIPLE) elemsize*= 3;

	for(a=start; a<end; a++) {
		cp= key->elemstr;
		if(mode==KEY_BEZTRIPLE) cp= elemstr;

		ofsp= ofs;
		
		while( cp[0] ) {
			
			switch(cp[1]) {
			case IPO_FLOAT:
				
				if(weights) {
					memcpy(poin, kref, sizeof(float)*cp[0]);
					if(*weights!=0.0f)
						rel_flerp(cp[0], (float *)poin, (float *)kref, (float *)k1, *weights);
					weights++;
				}
				else 
					memcpy(poin, k1, sizeof(float)*cp[0]);

				poin+= ofsp[0];

				break;
			case IPO_BPOINT:
				memcpy(poin, k1, 3*sizeof(float));
				memcpy(poin+4*sizeof(float), k1+3*sizeof(float), sizeof(float));
				
				poin+= ofsp[0];				

				break;
			case IPO_BEZTRIPLE:
				memcpy(poin, k1, sizeof(float)*10);
				poin+= ofsp[0];	

				break;
			}
			
			cp+= 2; ofsp++;
		}
		
		/* are we going to be nasty? */
		if(flagflo) {
			ktot+= kd;
			while(ktot>=1.0) {
				ktot-= 1.0;
				k1+= elemsize;
				kref+= elemsize;
			}
		}
		else {
			k1+= elemsize;
			kref+= elemsize;
		}
		
		if(mode==KEY_BEZTRIPLE) a+=2;
	}
}

void cp_cu_key(Curve *cu, KeyBlock *kb, int start, int end)
{
	Nurb *nu;
	int a, step = 0, tot, a1, a2;
	char *poin;

	tot= count_curveverts(&cu->nurb);
	nu= cu->nurb.first;
	a= 0;
	while(nu) {
		if(nu->bp) {
			
			step= nu->pntsu*nu->pntsv;
			
			/* exception because keys prefer to work with complete blocks */
			poin= (char *)nu->bp->vec;
			poin -= a*sizeof(BPoint);
			
			a1= MAX2(a, start);
			a2= MIN2(a+step, end);
			
			if(a1<a2) cp_key(a1, a2, tot, poin, cu->key, kb, NULL, KEY_BPOINT);
		}
		else if(nu->bezt) {
			
			step= 3*nu->pntsu;
			
			poin= (char *)nu->bezt->vec;
			poin -= a*sizeof(BezTriple);

			a1= MAX2(a, start);
			a2= MIN2(a+step, end);

			if(a1<a2) cp_key(a1, a2, tot, poin, cu->key, kb, NULL, KEY_BEZTRIPLE);
			
		}
		a+= step;
		nu=nu->next;
	}
}


void do_rel_key(int start, int end, int tot, char *basispoin, Key *key, int mode)
{
	KeyBlock *kb;
	int *ofsp, ofs[3], elemsize, b;
	char *cp, *poin, *reffrom, *from, elemstr[8];
	
	if(key->from==NULL) return;
	
	if (G.f & G_DEBUG) printf("do_rel_key() \n");
	
	if( GS(key->from->name)==ID_ME ) {
		ofs[0]= sizeof(MVert);
		ofs[1]= 0;
	}
	else if( GS(key->from->name)==ID_LT ) {
		ofs[0]= sizeof(BPoint);
		ofs[1]= 0;
	}
	else if( GS(key->from->name)==ID_CU ) {
		if(mode==KEY_BPOINT) ofs[0]= sizeof(BPoint);
		else ofs[0]= sizeof(BezTriple);
		
		ofs[1]= 0;
	}
	
	if(end>tot) end= tot;
	
	/* in case of beztriple */
	elemstr[0]= 1;				/* nr of ipofloats */
	elemstr[1]= IPO_BEZTRIPLE;
	elemstr[2]= 0;

	/* just here, not above! */
	elemsize= key->elemsize;
	if(mode==KEY_BEZTRIPLE) elemsize*= 3;

	/* step 1 init */
	cp_key(start, end, tot, basispoin, key, key->refkey, NULL, mode);
	
	/* step 2: do it */
	
	for(kb=key->block.first; kb; kb=kb->next) {
		if(kb!=key->refkey) {
			float icuval= kb->curval;
			
			if (G.f & G_DEBUG) printf("\tdo rel key %s : %s = %f \n", key->id.name+2, kb->name, icuval);
			
			/* only with value, and no difference allowed */
			if(!(kb->flag & KEYBLOCK_MUTE) && icuval!=0.0f && kb->totelem==tot) {
				KeyBlock *refb;
				float weight, *weights= kb->weights;
				
				if (G.f & G_DEBUG) printf("\t\tnot skipped \n");
				
				poin= basispoin;
				from= kb->data;
				/* reference now can be any block */
				refb= BLI_findlink(&key->block, kb->relative);
				if(refb==NULL) continue;
				reffrom= refb->data;
				
				poin+= start*ofs[0];
				reffrom+= key->elemsize*start;	// key elemsize yes!
				from+= key->elemsize*start;
				
				for(b=start; b<end; b++) {
				
					if(weights) 
						weight= *weights * icuval;
					else
						weight= icuval;
					
					cp= key->elemstr;	
					if(mode==KEY_BEZTRIPLE) cp= elemstr;
					
					ofsp= ofs;
					
					while( cp[0] ) {	/* cp[0]==amount */
						
						switch(cp[1]) {
						case IPO_FLOAT:
							rel_flerp(cp[0], (float *)poin, (float *)reffrom, (float *)from, weight);
							
							break;
						case IPO_BPOINT:
							rel_flerp(3, (float *)poin, (float *)reffrom, (float *)from, icuval);
							rel_flerp(1, (float *)(poin+16), (float *)(reffrom+16), (float *)(from+16), icuval);
			
							break;
						case IPO_BEZTRIPLE:
							rel_flerp(9, (float *)poin, (float *)reffrom, (float *)from, icuval);
			
							break;
						}
						
						poin+= ofsp[0];				
						
						cp+= 2;
						ofsp++;
					}
					
					reffrom+= elemsize;
					from+= elemsize;
					
					if(mode==KEY_BEZTRIPLE) b+= 2;
					if(weights) weights++;
				}
			}
		}
	}
}


static void do_key(int start, int end, int tot, char *poin, Key *key, KeyBlock **k, float *t, int mode)
{
	float k1tot = 0.0, k2tot = 0.0, k3tot = 0.0, k4tot = 0.0;
	float k1d = 0.0, k2d = 0.0, k3d = 0.0, k4d = 0.0;
	int a, ofs[32], *ofsp;
	int flagdo= 15, flagflo=0, elemsize, poinsize=0;
	char *k1, *k2, *k3, *k4;
	char *cp, elemstr[8];;

	if(key->from==0) return;

	if (G.f & G_DEBUG) printf("do_key() \n");
	
	if( GS(key->from->name)==ID_ME ) {
		ofs[0]= sizeof(MVert);
		ofs[1]= 0;
		poinsize= ofs[0];
	}
	else if( GS(key->from->name)==ID_LT ) {
		ofs[0]= sizeof(BPoint);
		ofs[1]= 0;
		poinsize= ofs[0];
	}
	else if( GS(key->from->name)==ID_CU ) {
		if(mode==KEY_BPOINT) ofs[0]= sizeof(BPoint);
		else ofs[0]= sizeof(BezTriple);
		
		ofs[1]= 0;
		poinsize= ofs[0];
	}
	
	if(end>tot) end= tot;

	k1= k[0]->data;
	k2= k[1]->data;
	k3= k[2]->data;
	k4= k[3]->data;

	/*  test for more or less points (per key!) */
	if(tot != k[0]->totelem) {
		k1tot= 0.0;
		flagflo |= 1;
		if(k[0]->totelem) {
			k1d= k[0]->totelem/(float)tot;
		}
		else flagdo -= 1;
	}
	if(tot != k[1]->totelem) {
		k2tot= 0.0;
		flagflo |= 2;
		if(k[0]->totelem) {
			k2d= k[1]->totelem/(float)tot;
		}
		else flagdo -= 2;
	}
	if(tot != k[2]->totelem) {
		k3tot= 0.0;
		flagflo |= 4;
		if(k[0]->totelem) {
			k3d= k[2]->totelem/(float)tot;
		}
		else flagdo -= 4;
	}
	if(tot != k[3]->totelem) {
		k4tot= 0.0;
		flagflo |= 8;
		if(k[0]->totelem) {
			k4d= k[3]->totelem/(float)tot;
		}
		else flagdo -= 8;
	}

		/* this exception needed for slurphing */
	if(start!=0) {

		poin+= poinsize*start;
		
		if(flagdo & 1) {
			if(flagflo & 1) {
				k1tot+= start*k1d;
				a= (int)floor(k1tot);
				if(a) {
					k1tot-= a;
					k1+= a*key->elemsize;
				}
			}
			else k1+= start*key->elemsize;
		}
		if(flagdo & 2) {
			if(flagflo & 2) {
				k2tot+= start*k2d;
				a= (int)floor(k2tot);
				if(a) {
					k2tot-= a;
					k2+= a*key->elemsize;
				}
			}
			else k2+= start*key->elemsize;
		}
		if(flagdo & 4) {
			if(flagflo & 4) {
				k3tot+= start*k3d;
				a= (int)floor(k3tot);
				if(a) {
					k3tot-= a;
					k3+= a*key->elemsize;
				}
			}
			else k3+= start*key->elemsize;
		}
		if(flagdo & 8) {
			if(flagflo & 8) {
				k4tot+= start*k4d;
				a= (int)floor(k4tot);
				if(a) {
					k4tot-= a;
					k4+= a*key->elemsize;
				}
			}
			else k4+= start*key->elemsize;
		}

	}

	/* in case of beztriple */
	elemstr[0]= 1;				/* nr of ipofloats */
	elemstr[1]= IPO_BEZTRIPLE;
	elemstr[2]= 0;

	/* only here, not above! */
	elemsize= key->elemsize;
	if(mode==KEY_BEZTRIPLE) elemsize*= 3;

	for(a=start; a<end; a++) {
	
		cp= key->elemstr;	
		if(mode==KEY_BEZTRIPLE) cp= elemstr;
		
		ofsp= ofs;
		
		while( cp[0] ) {	/* cp[0]==amount */
			
			switch(cp[1]) {
			case IPO_FLOAT:
				flerp(cp[0], (float *)poin, (float *)k1, (float *)k2, (float *)k3, (float *)k4, t);
				poin+= ofsp[0];				

				break;
			case IPO_BPOINT:
				flerp(3, (float *)poin, (float *)k1, (float *)k2, (float *)k3, (float *)k4, t);
				flerp(1, (float *)(poin+16), (float *)(k1+12), (float *)(k2+12), (float *)(k3+12), (float *)(k4+12), t);
				
				poin+= ofsp[0];				

				break;
			case IPO_BEZTRIPLE:
				flerp(9, (void *)poin, (void *)k1, (void *)k2, (void *)k3, (void *)k4, t);
				flerp(1, (float *)(poin+36), (float *)(k1+36), (float *)(k2+36), (float *)(k3+36), (float *)(k4+36), t);
				poin+= ofsp[0];				

				break;
			}
			

			cp+= 2;
			ofsp++;
		}
		/* lets do it the difficult way: when keys have a different size */
		if(flagdo & 1) {
			if(flagflo & 1) {
				k1tot+= k1d;
				while(k1tot>=1.0) {
					k1tot-= 1.0;
					k1+= elemsize;
				}
			}
			else k1+= elemsize;
		}
		if(flagdo & 2) {
			if(flagflo & 2) {
				k2tot+= k2d;
				while(k2tot>=1.0) {
					k2tot-= 1.0;
					k2+= elemsize;
				}
			}
			else k2+= elemsize;
		}
		if(flagdo & 4) {
			if(flagflo & 4) {
				k3tot+= k3d;
				while(k3tot>=1.0) {
					k3tot-= 1.0;
					k3+= elemsize;
				}
			}
			else k3+= elemsize;
		}
		if(flagdo & 8) {
			if(flagflo & 8) {
				k4tot+= k4d;
				while(k4tot>=1.0) {
					k4tot-= 1.0;
					k4+= elemsize;
				}
			}
			else k4+= elemsize;
		}
		
		if(mode==KEY_BEZTRIPLE) a+= 2;
	}
}

static float *get_weights_array(Object *ob, char *vgroup)
{
	bDeformGroup *curdef;
	MDeformVert *dvert= NULL;
	int totvert= 0, index= 0;
	
	/* no vgroup string set? */
	if(vgroup[0]==0) return NULL;
	
	/* gather dvert and totvert */
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		dvert= me->dvert;
		totvert= me->totvert;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		dvert= lt->dvert;
		totvert= lt->pntsu*lt->pntsv*lt->pntsw;
	}
	
	if(dvert==NULL) return NULL;
	
	/* find the group (weak loop-in-loop) */
	for (curdef = ob->defbase.first; curdef; curdef=curdef->next, index++)
		if (!strcmp(curdef->name, vgroup))
			break;

	if(curdef) {
		float *weights;
		int i, j;
		
		weights= MEM_callocN(totvert*sizeof(float), "weights");
		
		for (i=0; i < totvert; i++, dvert++) {
			for(j=0; j<dvert->totweight; j++) {
				if (dvert->dw[j].def_nr == index) {
					weights[i]= dvert->dw[j].weight;
					break;
				}
			}
		}
		return weights;
	}
	return NULL;
}

static int do_mesh_key(Scene *scene, Object *ob, Mesh *me)
{
	KeyBlock *k[4];
	float cfra, ctime, t[4], delta, loc[3], size[3];
	int a, flag = 0, step;
	
	if(me->totvert==0) return 0;
	if(me->key==NULL) return 0;
	if(me->key->block.first==NULL) return 0;
	
	/* prevent python from screwing this up? anyhoo, the from pointer could be dropped */
	me->key->from= (ID *)me;
	
	if (G.f & G_DEBUG) printf("do mesh key ob:%s me:%s ke:%s \n", ob->id.name+2, me->id.name+2, me->key->id.name+2);
	
	if(me->key->slurph && me->key->type!=KEY_RELATIVE ) {
		if (G.f & G_DEBUG) printf("\tslurph key\n");
		
		delta= me->key->slurph;
		delta/= me->totvert;
		
		step= 1;
		if(me->totvert>100 && slurph_opt) {
			step= me->totvert/50;
			delta*= step;
			/* in do_key and cp_key the case a>tot is handled */
		}
		
		cfra= (float)scene->r.cfra;
		
		for(a=0; a<me->totvert; a+=step, cfra+= delta) {
			
			ctime= bsystem_time(scene, 0, cfra, 0.0); // xxx  ugly cruft!
#if 0 // XXX old animation system
			if(calc_ipo_spec(me->key->ipo, KEY_SPEED, &ctime)==0) {
				ctime /= 100.0;
				CLAMP(ctime, 0.0, 1.0);
			}
#endif // XXX old animation system
			// XXX for now... since speed curve cannot be directly ported yet
			ctime /= 100.0f;
			CLAMP(ctime, 0.0f, 1.0f); // XXX for compat, we use this, but this clamping was confusing
		
			flag= setkeys(ctime, &me->key->block, k, t, 0);
			if(flag==0) {
				
				do_key(a, a+step, me->totvert, (char *)me->mvert->co, me->key, k, t, 0);
			}
			else {
				cp_key(a, a+step, me->totvert, (char *)me->mvert->co, me->key, k[2], NULL, 0);
			}
		}
		
		if(flag && k[2]==me->key->refkey) tex_space_mesh(me);
		else boundbox_mesh(me, loc, size);
	}
	else {
		if(me->key->type==KEY_RELATIVE) {
			KeyBlock *kb;
			
			if (G.f & G_DEBUG) printf("\tdo relative \n");
			
			for(kb= me->key->block.first; kb; kb= kb->next)
				kb->weights= get_weights_array(ob, kb->vgroup);

			do_rel_key(0, me->totvert, me->totvert, (char *)me->mvert->co, me->key, 0);
			
			for(kb= me->key->block.first; kb; kb= kb->next) {
				if(kb->weights) MEM_freeN(kb->weights);
				kb->weights= NULL;
			}
		}
		else {
			if (G.f & G_DEBUG) printf("\tdo absolute \n");
			
			ctime= bsystem_time(scene, ob, (float)scene->r.cfra, 0.0f); // xxx old cruft
			
#if 0 // XXX old animation system
			if(calc_ipo_spec(me->key->ipo, KEY_SPEED, &ctime)==0) {
				ctime /= 100.0;
				CLAMP(ctime, 0.0, 1.0);
			}
#endif // XXX old animation system
			// XXX for now... since speed curve cannot be directly ported yet
			ctime /= 100.0f;
			CLAMP(ctime, 0.0f, 1.0f); // XXX for compat, we use this, but this clamping was confusing
			
			flag= setkeys(ctime, &me->key->block, k, t, 0);
			if(flag==0) {
				do_key(0, me->totvert, me->totvert, (char *)me->mvert->co, me->key, k, t, 0);
			}
			else {
				cp_key(0, me->totvert, me->totvert, (char *)me->mvert->co, me->key, k[2], NULL, 0);
			}
			
			if(flag && k[2]==me->key->refkey) tex_space_mesh(me);
			else boundbox_mesh(me, loc, size);
		}
	}
	return 1;
}

static void do_cu_key(Curve *cu, KeyBlock **k, float *t)
{
	Nurb *nu;
	int a, step = 0, tot;
	char *poin;
	
	tot= count_curveverts(&cu->nurb);
	nu= cu->nurb.first;
	a= 0;
	
	while(nu) {
		if(nu->bp) {
			
			step= nu->pntsu*nu->pntsv;
			
			/* exception because keys prefer to work with complete blocks */
			poin= (char *)nu->bp->vec;
			poin -= a*sizeof(BPoint);
			
			do_key(a, a+step, tot, poin, cu->key, k, t, KEY_BPOINT);
		}
		else if(nu->bezt) {
			
			step= 3*nu->pntsu;
			
			poin= (char *)nu->bezt->vec;
			poin -= a*sizeof(BezTriple);

			do_key(a, a+step, tot, poin, cu->key, k, t, KEY_BEZTRIPLE);
			
		}
		a+= step;
		nu=nu->next;
	}
}

static void do_rel_cu_key(Curve *cu, float ctime)
{
	Nurb *nu;
	int a, step = 0, tot;
	char *poin;
	
	tot= count_curveverts(&cu->nurb);
	nu= cu->nurb.first;
	a= 0;
	while(nu) {
		if(nu->bp) {
			
			step= nu->pntsu*nu->pntsv;
			
			/* exception because keys prefer to work with complete blocks */
			poin= (char *)nu->bp->vec;
			poin -= a*sizeof(BPoint);
			
			do_rel_key(a, a+step, tot, poin, cu->key, KEY_BPOINT);
		}
		else if(nu->bezt) {
			
			step= 3*nu->pntsu;
			
			poin= (char *)nu->bezt->vec;
			poin -= a*sizeof(BezTriple);

			do_rel_key(a, a+step, tot, poin, cu->key, KEY_BEZTRIPLE);
		}
		a+= step;
		
		nu=nu->next;
	}
}

static int do_curve_key(Scene *scene, Curve *cu)
{
	KeyBlock *k[4];
	float cfra, ctime, t[4], delta;
	int a, flag = 0, step = 0, tot;
	
	tot= count_curveverts(&cu->nurb);
	
	if(tot==0) return 0;
	if(cu->key==NULL) return 0;
	if(cu->key->block.first==NULL) return 0;
	
	if(cu->key->slurph) {
		delta= cu->key->slurph;
		delta/= tot;
		
		step= 1;
		if(tot>100 && slurph_opt) {
			step= tot/50;
			delta*= step;
			/* in do_key and cp_key the case a>tot has been handled */
		}
		
		cfra= (float)scene->r.cfra;
		
		for(a=0; a<tot; a+=step, cfra+= delta) {
			ctime= bsystem_time(scene, 0, cfra, 0.0f); // XXX old cruft
#if 0 // XXX old animation system
			if(calc_ipo_spec(cu->key->ipo, KEY_SPEED, &ctime)==0) {
				ctime /= 100.0;
				CLAMP(ctime, 0.0, 1.0);
			}
#endif // XXX old animation system
		
			flag= setkeys(ctime, &cu->key->block, k, t, 0);
			if(flag==0) {
				
				/* do_key(a, a+step, tot, (char *)cu->mvert->co, cu->key, k, t, 0); */
			}
			else {
				/* cp_key(a, a+step, tot, (char *)cu->mvert->co, cu->key, k[2],0); */
			}
		}

		if(flag && k[2]==cu->key->refkey) tex_space_curve(cu);
		
		
	}
	else {
		
		ctime= bsystem_time(scene, NULL, (float)scene->r.cfra, 0.0);
		
		if(cu->key->type==KEY_RELATIVE) {
			do_rel_cu_key(cu, ctime);
		}
		else {
#if 0 // XXX old animation system
			if(calc_ipo_spec(cu->key->ipo, KEY_SPEED, &ctime)==0) {
				ctime /= 100.0;
				CLAMP(ctime, 0.0, 1.0);
			}
#endif // XXX old animation system
			
			flag= setkeys(ctime, &cu->key->block, k, t, 0);
			
			if(flag==0) do_cu_key(cu, k, t);
			else cp_cu_key(cu, k[2], 0, tot);
					
			if(flag && k[2]==cu->key->refkey) tex_space_curve(cu);
		}
	}
	
	return 1;
}

static int do_latt_key(Scene *scene, Object *ob, Lattice *lt)
{
	KeyBlock *k[4];
	float delta, cfra, ctime, t[4];
	int a, tot, flag;
	
	if(lt->key==NULL) return 0;
	if(lt->key->block.first==NULL) return 0;

	tot= lt->pntsu*lt->pntsv*lt->pntsw;

	if(lt->key->slurph) {
		delta= lt->key->slurph;
		delta/= (float)tot;
		
		cfra= (float)scene->r.cfra;
		
		for(a=0; a<tot; a++, cfra+= delta) {
			
			ctime= bsystem_time(scene, 0, cfra, 0.0); // XXX old cruft
#if 0 // XXX old animation system
			if(calc_ipo_spec(lt->key->ipo, KEY_SPEED, &ctime)==0) {
				ctime /= 100.0;
				CLAMP(ctime, 0.0, 1.0);
			}
#endif // XXX old animation system
		
			flag= setkeys(ctime, &lt->key->block, k, t, 0);
			if(flag==0) {
				
				do_key(a, a+1, tot, (char *)lt->def->vec, lt->key, k, t, 0);
			}
			else {
				cp_key(a, a+1, tot, (char *)lt->def->vec, lt->key, k[2], NULL, 0);
			}
		}		
	}
	else {
		ctime= bsystem_time(scene, NULL, (float)scene->r.cfra, 0.0);
	
		if(lt->key->type==KEY_RELATIVE) {
			KeyBlock *kb;
			
			for(kb= lt->key->block.first; kb; kb= kb->next)
				kb->weights= get_weights_array(ob, kb->vgroup);
			
			do_rel_key(0, tot, tot, (char *)lt->def->vec, lt->key, 0);
			
			for(kb= lt->key->block.first; kb; kb= kb->next) {
				if(kb->weights) MEM_freeN(kb->weights);
				kb->weights= NULL;
			}
		}
		else {
#if 0 // XXX old animation system
			if(calc_ipo_spec(lt->key->ipo, KEY_SPEED, &ctime)==0) {
				ctime /= 100.0;
				CLAMP(ctime, 0.0, 1.0);
			}
#endif // XXX old animation system
			
			flag= setkeys(ctime, &lt->key->block, k, t, 0);
			if(flag==0) {
				do_key(0, tot, tot, (char *)lt->def->vec, lt->key, k, t, 0);
			}
			else {
				cp_key(0, tot, tot, (char *)lt->def->vec, lt->key, k[2], NULL, 0);
			}
		}
	}
	
	if(lt->flag & LT_OUTSIDE) outside_lattice(lt);
	
	return 1;
}

/* returns 1 when key applied */
int do_ob_key(Scene *scene, Object *ob)
{
	Key *key= ob_get_key(ob);
	
	if(key==NULL)
		return 0;
		
	if(ob->shapeflag & OB_SHAPE_LOCK) {
		KeyBlock *kb= BLI_findlink(&key->block, ob->shapenr-1);
		
		if (G.f & G_DEBUG) printf("ob %s, key %s locked \n", ob->id.name+2, key->id.name+2);
		
		if(kb && (kb->flag & KEYBLOCK_MUTE))
			kb= key->refkey;

		if(kb==NULL) {
			kb= key->block.first;
			ob->shapenr= 1;
		}
		
		if(ob->type==OB_MESH) {
			Mesh *me= ob->data;
			float *weights= get_weights_array(ob, kb->vgroup);

			cp_key(0, me->totvert, me->totvert, (char *)me->mvert->co, key, kb, weights, 0);
			
			if(weights) MEM_freeN(weights);
		}
		else if(ob->type==OB_LATTICE) {
			Lattice *lt= ob->data;
			float *weights= get_weights_array(ob, kb->vgroup);
			int tot= lt->pntsu*lt->pntsv*lt->pntsw;
			
			cp_key(0, tot, tot, (char *)lt->def->vec, key, kb, weights, 0);
			
			if(weights) MEM_freeN(weights);
		}
		else if ELEM(ob->type, OB_CURVE, OB_SURF) {
			Curve *cu= ob->data;
			int tot= count_curveverts(&cu->nurb);
			
			cp_cu_key(cu, kb, 0, tot);
		}
		return 1;
	}
	else {
		/* do shapekey local drivers */
		float ctime= (float)scene->r.cfra; // XXX this needs to be checked
		
		if (G.f & G_DEBUG) 
			printf("ob %s - do shapekey (%s) drivers \n", ob->id.name+2, key->id.name+2);
		BKE_animsys_evaluate_animdata(&key->id, key->adt, ctime, ADT_RECALC_DRIVERS);
		
		if(ob->type==OB_MESH) return do_mesh_key(scene, ob, ob->data);
		else if(ob->type==OB_CURVE) return do_curve_key(scene, ob->data);
		else if(ob->type==OB_SURF) return do_curve_key(scene, ob->data);
		else if(ob->type==OB_LATTICE) return do_latt_key(scene, ob, ob->data);
	}
	
	return 0;
}

Key *ob_get_key(Object *ob)
{
	if(ob==NULL) return NULL;
	
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		return me->key;
	}
	else if ELEM(ob->type, OB_CURVE, OB_SURF) {
		Curve *cu= ob->data;
		return cu->key;
	}
	else if(ob->type==OB_LATTICE) {
		Lattice *lt= ob->data;
		return lt->key;
	}
	return NULL;
}

/* only the active keyblock */
KeyBlock *ob_get_keyblock(Object *ob) 
{
	Key *key= ob_get_key(ob);
	
	if (key) {
		KeyBlock *kb= BLI_findlink(&key->block, ob->shapenr-1);
		return kb;
	}

	return NULL;
}

/* get the appropriate KeyBlock given an index */
KeyBlock *key_get_keyblock(Key *key, int index)
{
	KeyBlock *kb;
	int i;
	
	if (key) {
		kb= key->block.first;
		
		for (i= 1; i < key->totkey; i++) {
			kb= kb->next;
			
			if (index==i)
				return kb;
		}
	}
	
	return NULL;
}

/* get the appropriate KeyBlock given a name to search for */
KeyBlock *key_get_named_keyblock(Key *key, const char name[])
{
	KeyBlock *kb;
	
	if (key && name) {
		for (kb= key->block.first; kb; kb= kb->next) {
			if (strcmp(name, kb->name)==0)
				return kb;
		}
	}
	
	return NULL;
}

/* Get RNA-Path for 'value' setting of the given ShapeKey 
 * NOTE: the user needs to free the returned string once they're finishe with it
 */
char *key_get_curValue_rnaPath(Key *key, KeyBlock *kb)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	
	/* sanity checks */
	if ELEM(NULL, key, kb)
		return NULL;
	
	/* create the RNA pointer */
	RNA_pointer_create(key, &RNA_ShapeKey, kb, &ptr);
	/* get pointer to the property too */
	prop= RNA_struct_find_property(&ptr, "value");
	
	/* return the path */
	return RNA_path_from_ID_to_property(&ptr, prop);
}
