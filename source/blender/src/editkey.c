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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"

#include "BKE_utildefines.h"
#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_displist.h"

#include "BIF_editkey.h"
#include "BIF_editview.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_interface.h"

#include "BSE_editipo.h"
#include "BSE_trans_types.h"

#include "BDR_editobject.h"

#include "blendef.h"
#include "mydevice.h"
#include "ipo.h"

extern ListBase editNurb; /* in editcurve.c */

/* temporary storage for slider values */
float meshslidervals[32] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

static IpoCurve *get_key_icu(Key *key, int keynum) {
	/* return the Ipocurve that has the specified
	 * keynum as ardcode -- return NULL if no such 
	 * curve exists.
	 */
    IpoCurve *icu;
	if (!(key->ipo)) {
		key->ipo = get_ipo((ID *)key, ID_KE, 1);
		return NULL;
	}


    for (icu = key->ipo->curve.first; icu ; icu = icu->next) {
        if (!icu->adrcode) continue;
        if (icu->adrcode == keynum) return icu;
    }

    return NULL;
}

static BezTriple *get_bezt_icu_time(IpoCurve *icu, float *frame, float *val) {
	/* this function tries to find a bezier that is within
	 * 0.25 time units from the specified frame. If there
	 * are more than one such beziers, it returns the
	 * closest one.
	 */
	int   i;
	float d, dmin = 0.25, newframe;
	BezTriple *bezt = NULL;
	
	newframe = *frame;

	for (i=0; i<icu->totvert; i++){
		d = fabs(icu->bezt[i].vec[1][0] - *frame);
		if (d < dmin) {
			dmin     = d;
			newframe = icu->bezt[i].vec[1][0];
			*val     = icu->bezt[i].vec[1][1];
			bezt     = icu->bezt + i;
		}
	}

	*frame = newframe;
	return bezt;
}

static void rvk_slider_func(void *voidkey, void *voidkeynum) {
	/* the callback for the rvk sliders ... copies the
	 * value from the temporary array into a bezier at the
	 * right frame on the right ipo curve (creating both the
	 * ipo curve and the bezier if needed).
	 */
	int       *keynum = (int *) voidkeynum;
	Key       *key = (Key *) voidkey;
	float     cfra, rvkval;
	IpoCurve  *icu=NULL;
	BezTriple *bezt=NULL;

	cfra = frame_to_float(CFRA);

	icu    = get_key_icu(key, *keynum);

	if (icu) {
		/* if the ipocurve exists, try to get a bezier
		 * for this frame
		 */
		bezt = get_bezt_icu_time(icu, &cfra, &rvkval);
	}
	else {
		/* create an IpoCurve if one doesn't already
		 * exist.
		 */
		icu = get_ipocurve(key->from, GS(key->from->name), 
						   *keynum, key->ipo);
	}
	
	/* create the bezier triple if one doesn't exist,
	 * otherwise modify it's value
	 */
	if (!bezt) {
		insert_vert_ipo(icu, cfra, meshslidervals[*keynum]);
	}
	else {
		bezt->vec[1][1] = meshslidervals[*keynum];
	}

	/* make sure the Ipo's are properly process and
	 * redraw as necessary
	 */
	sort_time_ipocurve(icu);
	testhandles_ipocurve(icu);

	do_all_ipos();
	do_spec_key(key);
	/* if I'm deformed by a lattice, update my
	 * displists
	 */
	makeDispList(OBACT);

	/* if I'm a lattice, update the displists of
	 * my children
	 */
	if (OBACT->type==OB_LATTICE ) {
		Base *base;

		base= FIRSTBASE;
		while(base) {
			if (base->object->parent == OBACT) {
				makeDispList(base->object);
			}
			base= base->next;
		}
	}
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
	allqueue (REDRAWIPO, 0);

}

static float getrvkval(Key *key, int keynum) {
	/* get the value of the rvk from the
	 * ipo curve at the current time -- return 0
	 * if no ipo curve exists
	 */
	IpoCurve  *icu=NULL;
	BezTriple *bezt=NULL;
	float     rvkval = 0.0;
	float     cfra;

	cfra = frame_to_float(CFRA);
	icu    = get_key_icu(key, keynum);
	if (icu) {
		bezt = get_bezt_icu_time(icu, &cfra, &rvkval);
		if (!bezt) {
			rvkval = eval_icu(icu, cfra);
		}
	}

	return rvkval;

}

void make_rvk_slider(uiBlock *block, Key *key, int keynum,
					 int x, int y, int w, int h)
{
	/* create a slider for the rvk */
	uiBut         *but;
	KeyBlock   *kb;
	float min, max;
	int i;

	/* dang, need to pass a pointer to int to uiButSetFunc
	 * that is on the heap, not the stack ... hence this
	 * kludgy static array
	 */
	static int keynums[] = {0,1,2,3,4,5,6,7,
							8,9,10,11,12,13,14,15,
							16,17,18,19,20,21,22,23,
							24,25,26,27,28,29,30,31};

	meshslidervals[keynum] = getrvkval(key, keynum);

	kb= key->block.first;
	for (i=0; i<keynum; ++i) kb = kb->next; 

	if ( (kb->slidermin >= kb->slidermax) ) {
		kb->slidermin = 0.0;
		kb->slidermax = 1.0;
	}

	min = (kb->slidermin < meshslidervals[keynum]) ? 
		kb->slidermin: meshslidervals[keynum];

	max = (kb->slidermax > meshslidervals[keynum]) ? 
		kb->slidermax: meshslidervals[keynum];

	but=uiDefButF(block, NUMSLI, REDRAWVIEW3D, "",
				  x, y , w, h,
				  meshslidervals+keynum, min, max, 10, 2,
				  "Slider to control rvk");
	uiButSetFunc(but, rvk_slider_func, key, keynums+keynum);
	// no hilite, the winmatrix is not correct later on...
	uiButSetFlag(but, UI_NO_HILITE);

}

static void default_key_ipo(Key *key)
{
	IpoCurve *icu;
	BezTriple *bezt;
	
	key->ipo= add_ipo("KeyIpo", ID_KE);
	
	icu= MEM_callocN(sizeof(IpoCurve), "ipocurve");
			
	icu->blocktype= ID_KE;
	icu->adrcode= KEY_SPEED;
	icu->flag= IPO_VISIBLE+IPO_SELECT;
	set_icu_vars(icu);
	
	BLI_addtail( &(key->ipo->curve), icu);
	
	icu->bezt= bezt= MEM_callocN(2*sizeof(BezTriple), "defaultipo");
	icu->totvert= 2;
	
	bezt->hide= IPO_BEZ;
	bezt->f1=bezt->f2= bezt->f3= SELECT;
	bezt->h1= bezt->h2= HD_AUTO;
	bezt++;
	bezt->vec[1][0]= 100.0;
	bezt->vec[1][1]= 1.0;
	bezt->hide= IPO_BEZ;
	bezt->f1=bezt->f2= bezt->f3= SELECT;
	bezt->h1= bezt->h2= HD_AUTO;
	
	calchandles_ipocurve(icu);
}

	

/* **************************************** */

void mesh_to_key(Mesh *me, KeyBlock *kb)
{
	MVert *mvert;
	float *fp;
	int a;
	
	if(me->totvert==0) return;
	
	if(kb->data) MEM_freeN(kb->data);
	
	kb->data= MEM_callocN(me->key->elemsize*me->totvert, "kb->data");
	kb->totelem= me->totvert;
	
	mvert= me->mvert;
	fp= kb->data;
	for(a=0; a<kb->totelem; a++, fp+=3, mvert++) {
		VECCOPY(fp, mvert->co);
		
	}
}

void key_to_mesh(KeyBlock *kb, Mesh *me)
{
	MVert *mvert;
	float *fp;
	int a, tot;
	
	mvert= me->mvert;
	fp= kb->data;
	
	tot= MIN2(kb->totelem, me->totvert);
	
	for(a=0; a<tot; a++, fp+=3, mvert++) {
		VECCOPY(mvert->co, fp);
	}
}



void insert_meshkey(Mesh *me, short offline)
{
	Key *key;
	KeyBlock *kb, *kkb;
	float curpos;
	short rel;

	if(me->key==0) {
		me->key= add_key( (ID *)me);

		if (!offline) /* interactive */
			rel = pupmenu("Insert Vertex Keys %t|"
										"Relative keys %x1|Absolute keys %x2");
		else /* we were called from a script */
			rel = offline;

		switch (rel) {
		case 1:
			me->key->type = KEY_RELATIVE;
			break;
		default:
			default_key_ipo(me->key);
			break;
		}
	}
	key= me->key;
	
	kb= MEM_callocN(sizeof(KeyBlock), "Keyblock");
	BLI_addtail(&key->block, kb);
	kb->type= KEY_CARDINAL;
	
	curpos= bsystem_time(0, 0, (float)CFRA, 0.0);
	if(calc_ipo_spec(me->key->ipo, KEY_SPEED, &curpos)==0) {
		curpos /= 100.0;
	}
	kb->pos= curpos;
	
	key->totkey++;
	if(key->totkey==1) key->refkey= kb;
	
	mesh_to_key(me, kb);
	
	sort_keys(me->key);

	/* curent active: */
	kkb= key->block.first;
	while(kkb) {
		kkb->flag &= ~SELECT;
		if(kkb==kb) kkb->flag |= SELECT;
		
		kkb= kkb->next;
	}
}

/* ******************** */

void latt_to_key(Lattice *lt, KeyBlock *kb)
{
	BPoint *bp;
	float *fp;
	int a, tot;
	
	tot= lt->pntsu*lt->pntsv*lt->pntsw;
	if(tot==0) return;
	
	if(kb->data) MEM_freeN(kb->data);
	
	kb->data= MEM_callocN(lt->key->elemsize*tot, "kb->data");
	kb->totelem= tot;
	
	bp= lt->def;
	fp= kb->data;
	for(a=0; a<kb->totelem; a++, fp+=3, bp++) {
		VECCOPY(fp, bp->vec);
	}
}

void key_to_latt(KeyBlock *kb, Lattice *lt)
{
	BPoint *bp;
	float *fp;
	int a, tot;
	
	bp= lt->def;
	fp= kb->data;
	
	tot= lt->pntsu*lt->pntsv*lt->pntsw;
	tot= MIN2(kb->totelem, tot);
	
	for(a=0; a<tot; a++, fp+=3, bp++) {
		VECCOPY(bp->vec, fp);
	}
	
}

void insert_lattkey(Lattice *lt)
{
	Key *key;
	KeyBlock *kb, *kkb;
	float curpos;
	
	if(lt->key==0) {
		lt->key= add_key( (ID *)lt);
		default_key_ipo(lt->key);
	}
	key= lt->key;
	
	kb= MEM_callocN(sizeof(KeyBlock), "Keyblock");
	BLI_addtail(&key->block, kb);
	kb->type= KEY_CARDINAL;
	
	curpos= bsystem_time(0, 0, (float)CFRA, 0.0);
	if(calc_ipo_spec(lt->key->ipo, KEY_SPEED, &curpos)==0) {
		curpos /= 100.0;
	}
	kb->pos= curpos;
	
	key->totkey++;
	if(key->totkey==1) key->refkey= kb;
	
	latt_to_key(lt, kb);
	
	sort_keys(lt->key);

	/* curent active: */
	kkb= key->block.first;
	while(kkb) {
		kkb->flag &= ~SELECT;
		if(kkb==kb) kkb->flag |= SELECT;
		
		kkb= kkb->next;
	}
}

/* ******************************** */

void curve_to_key(Curve *cu, KeyBlock *kb, ListBase *nurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float *fp;
	int a, tot;
	
	/* count */
	tot= count_curveverts(nurb);
	if(tot==0) return;
	
	if(kb->data) MEM_freeN(kb->data);
	
	kb->data= MEM_callocN(cu->key->elemsize*tot, "kb->data");
	kb->totelem= tot;
	
	nu= nurb->first;
	fp= kb->data;
	while(nu) {
		
		if(nu->bezt) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				VECCOPY(fp, bezt->vec[0]);
				fp+= 3;
				VECCOPY(fp, bezt->vec[1]);
				fp+= 3;
				VECCOPY(fp, bezt->vec[2]);
				fp+= 3;
				fp+= 3;	/* alphas */
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				VECCOPY(fp, bp->vec);
				fp[3]= bp->alfa;
				
				fp+= 4;
				bp++;
			}
		}
		nu= nu->next;
	}
}

void key_to_curve(KeyBlock *kb, Curve  *cu, ListBase *nurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float *fp;
	int a, tot;
	
	nu= nurb->first;
	fp= kb->data;
	
	tot= count_curveverts(nurb);

	tot= MIN2(kb->totelem, tot);
	
	while(nu && tot>0) {
		
		if(nu->bezt) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a-- && tot>0) {
				VECCOPY(bezt->vec[0], fp);
				fp+= 3;
				VECCOPY(bezt->vec[1], fp);
				fp+= 3;
				VECCOPY(bezt->vec[2], fp);
				fp+= 3;
				fp+= 3;	/* alphas */
				
				tot-= 3;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a-- && tot>0) {
				VECCOPY(bp->vec, fp);
				bp->alfa= fp[3];
				
				fp+= 4;
				tot--;
				bp++;
			}
		}
		nu= nu->next;
	}
}



void insert_curvekey(Curve *cu)
{
	Key *key;
	KeyBlock *kb, *kkb;
	float curpos;
	
	if(cu->key==0) {
		cu->key= add_key( (ID *)cu);
		default_key_ipo(cu->key);
	}
	key= cu->key;
	
	kb= MEM_callocN(sizeof(KeyBlock), "Keyblock");
	BLI_addtail(&key->block, kb);
	kb->type= KEY_CARDINAL;
	
	curpos= bsystem_time(0, 0, (float)CFRA, 0.0);
	if(calc_ipo_spec(cu->key->ipo, KEY_SPEED, &curpos)==0) {
		curpos /= 100.0;
	}
	kb->pos= curpos;
	
	key->totkey++;
	if(key->totkey==1) key->refkey= kb;
	
	if(editNurb.first) curve_to_key(cu, kb, &editNurb);
	else curve_to_key(cu, kb, &cu->nurb);
	
	sort_keys(cu->key);

	/* curent active: */
	kkb= key->block.first;
	while(kkb) {
		kkb->flag &= ~SELECT;
		if(kkb==kb) kkb->flag |= SELECT;
		
		kkb= kkb->next;
	}
}


/* ******************** */

Key *give_current_key(Object *ob)
{
	Mesh *me;
	Curve *cu;
	Lattice *lt;
	
	if(ob->type==OB_MESH) {
		me= ob->data;
		return me->key;
	}
	else if ELEM(ob->type, OB_CURVE, OB_SURF) {
		cu= ob->data;
		return cu->key;
	}
	else if(ob->type==OB_LATTICE) {
		lt= ob->data;
		return lt->key;
	}
	return 0;
}

void showkeypos(Key *key, KeyBlock *kb)
{
	Object *ob;
	Mesh *me;
	Lattice *lt;
	Curve *cu;
	int tot;
	
	/* from ipo */
	ob= OBACT;
	if(ob==0) return;
	
	if(key == give_current_key(ob)) {
		
		if(ob->type==OB_MESH) {
			me= ob->data;

			cp_key(0, me->totvert, me->totvert, (char *)me->mvert->co, me->key, kb, 0);

			make_displists_by_obdata(me);
		}
		else if(ob->type==OB_LATTICE) {
			lt= ob->data;
			tot= lt->pntsu*lt->pntsv*lt->pntsw;
			
			cp_key(0, tot, tot, (char *)lt->def->vec, lt->key, kb, 0);

			make_displists_by_parent(ob);
		}
		else if ELEM(ob->type, OB_CURVE, OB_SURF) {
			cu= ob->data;
			tot= count_curveverts(&cu->nurb);
			cp_cu_key(cu, kb, 0, tot);

			make_displists_by_obdata(cu);
		}
		
		allqueue(REDRAWVIEW3D, 0);
	}
}

void deselectall_key(void)
{
	KeyBlock *kb;
	Key *key;
	
	if(G.sipo->blocktype!=ID_KE) return;
	key= (Key *)G.sipo->from;
	if(key==0) return;
	
	kb= key->block.first;
	while(kb) {
		kb->flag &= ~SELECT;
		kb= kb->next;
	}
}


void delete_key(void)
{
	KeyBlock *kb, *kbn;
	Key *key;
	
	if(G.sipo->blocktype!=ID_KE) return;

	if(okee("Erase selected keys")==0) return;
	
	key= (Key *)G.sipo->from;
	if(key==0) return;
	
	kb= key->block.first;
	while(kb) {
		kbn= kb->next;
		if(kb->flag & SELECT) {
			BLI_remlink(&key->block, kb);
			key->totkey--;
			if(key->refkey== kb) key->refkey= key->block.first;
			
			if(kb->data) MEM_freeN(kb->data);
			MEM_freeN(kb);
			
		}
		kb= kbn;
	}
	
	if(key->totkey==0) {
		if(GS(key->from->name)==ID_ME) ((Mesh *)key->from)->key= 0;
		else if(GS(key->from->name)==ID_CU) ((Curve *)key->from)->key= 0;
		else if(GS(key->from->name)==ID_LT) ((Lattice *)key->from)->key= 0;

		free_libblock_us(&(G.main->key), key);
		scrarea_queue_headredraw(curarea);	/* ipo remove too */
	}
	else do_spec_key(key);
	
	allqueue(REDRAWVIEW3D, 0);
	scrarea_queue_winredraw(curarea);
}

void move_keys(void)
{
	Key *key;
	KeyBlock *kb;
	TransVert *transmain, *tv;
	float div, dy, vec[3], dvec[3];
	int a, tot=0, afbreek=0, firsttime= 1;
	unsigned short event = 0;
	short mval[2], val, xo, yo;
	char str[32];
	
	if(G.sipo->blocktype!=ID_KE) return;
	
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if(G.sipo->editipo==0) return;

	key= (Key *)G.sipo->from;
	if(key==0) return;
	
	/* which keys are involved */
	kb= key->block.first;
	while(kb) {
		if(kb->flag & SELECT) tot++;
		kb= kb->next;
	}
	
	if(tot==0) return;	
	
	tv=transmain= MEM_callocN(tot*sizeof(TransVert), "transmain");
	kb= key->block.first;
	while(kb) {
		if(kb->flag & SELECT) {
			tv->loc= &kb->pos;
			tv->oldloc[0]= kb->pos;
			tv++;
		}
		kb= kb->next;
	}
	
	getmouseco_areawin(mval);
	xo= mval[0];
	yo= mval[1];
	dvec[0]=dvec[1]=dvec[2]= 0.0; 
	

	while(afbreek==0) {
		getmouseco_areawin(mval);
		if(mval[0]!=xo || mval[1]!=yo || firsttime) {
			firsttime= 0;
			
			dy= (float)(mval[1]- yo);

			div= (float)(G.v2d->mask.ymax-G.v2d->mask.ymin);
			dvec[1]+= (G.v2d->cur.ymax-G.v2d->cur.ymin)*(dy)/div;
			
			VECCOPY(vec, dvec);

			apply_keyb_grid(vec, 0.0, 1.0, 0.1, U.flag & USER_AUTOGRABGRID);
			apply_keyb_grid(vec+1, 0.0, 1.0, 0.1, U.flag & USER_AUTOGRABGRID);

			tv= transmain;
			for(a=0; a<tot; a++, tv++) {
				tv->loc[0]= tv->oldloc[0]+vec[1];
			}
			
			sprintf(str, "Y: %.3f  ", vec[1]);
			headerprint(str);
			
			xo= mval[0];
			yo= mval[1];
				
			force_draw();
		}
		else BIF_wait_for_statechange();
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case LEFTMOUSE:
				case SPACEKEY:
					afbreek= 1;
					break;
				default:
					arrows_move_cursor(event);
				}
			}
		}
	}
	
	if(event==ESCKEY) {
		tv= transmain;
		for(a=0; a<tot; a++, tv++) {
			tv->loc[0]= tv->oldloc[0];
		}
	}
	
	sort_keys(key);
	do_spec_key(key);
	
	/* for boundbox */
	editipo_changed(G.sipo, 0);

	MEM_freeN(transmain);	
	allqueue(REDRAWVIEW3D, 0);
	scrarea_queue_redraw(curarea);
}
