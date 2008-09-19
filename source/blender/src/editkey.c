/**
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

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

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

#include "BLO_sys_types.h" // for intptr_t support

extern ListBase editNurb; /* in editcurve.c */

/* temporary storage for slider values */
/* pretty bad static stuff... is secured in drawaction.c though */
float meshslidervals[256] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
							0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

static IpoCurve *get_key_icu(Ipo *ipo, int keynum) 
{
	/* return the Ipocurve that has the specified
	 * keynum as ardcode -- return NULL if no such 
	 * curve exists.
	 */
    IpoCurve *icu;
	
	if (!ipo) 
		return NULL;

    for (icu = ipo->curve.first; icu ; icu = icu->next) {
        if (!icu->adrcode) continue;
        if (icu->adrcode == keynum) return icu;
    }

    return NULL;
}

BezTriple *get_bezt_icu_time(IpoCurve *icu, float *frame, float *val) 
{
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

static void rvk_slider_func(void *voidob, void *voidkeynum) 
{
	/* the callback for the rvk sliders ... copies the
	 * value from the temporary array into a bezier at the
	 * right frame on the right ipo curve (creating both the
	 * ipo curve and the bezier if needed).
	 */
	Object *ob= voidob;
	IpoCurve  *icu=NULL;
	BezTriple *bezt=NULL;
	float cfra, rvkval;
	int keynum = (intptr_t) voidkeynum;

	cfra = frame_to_float(CFRA);

	/* ipo on action or ob? */
	if(ob->ipoflag & OB_ACTION_KEY)
		icu = verify_ipocurve(&ob->id, ID_KE, "Shape", NULL, NULL, keynum);
	else 
		icu = verify_ipocurve(&ob->id, ID_KE, NULL, NULL, NULL, keynum);

	if (icu) {
		/* if the ipocurve exists, try to get a bezier
		 * for this frame
		 */
		bezt = get_bezt_icu_time(icu, &cfra, &rvkval);
	
		/* create the bezier triple if one doesn't exist,
		 * otherwise modify it's value
		 */
		if (bezt == NULL) {
			insert_vert_icu(icu, cfra, meshslidervals[keynum], 0);
		}
		else {
			bezt->vec[1][1] = meshslidervals[keynum];
		}

		/* make sure the Ipo's are properly process and
		 * redraw as necessary
		 */
		sort_time_ipocurve(icu);
		testhandles_ipocurve(icu);
		
		ob->shapeflag &= ~OB_SHAPE_TEMPLOCK;
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}
	else error("Cannot edit this Shape Key");
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
	allqueue (REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
}

static float getrvkval(Ipo *ipo, int keynum) 
{
	/* get the value of the rvk from the
	 * ipo curve at the current time -- return 0
	 * if no ipo curve exists
	 */
	IpoCurve  *icu=NULL;
	BezTriple *bezt=NULL;
	float     rvkval = 0.0;
	float     cfra;

	cfra = frame_to_float(CFRA);
	icu    = get_key_icu(ipo, keynum);
	if (icu) {
		bezt = get_bezt_icu_time(icu, &cfra, &rvkval);
		if (!bezt) {
			rvkval = eval_icu(icu, cfra);
		}
	}

	return rvkval;

}

void make_rvk_slider(uiBlock *block, Object *ob, int keynum,
					 int x, int y, int w, int h, char *tip)
{
	/* create a slider for the rvk */
	uiBut *but;
	Ipo *ipo= NULL;
	Key *key= ob_get_key(ob);
	KeyBlock   *kb;
	float min, max;
	int i;
	
	if(key==NULL) return;
	
	/* ipo on action or ob? */
	if(ob->ipoflag & OB_ACTION_KEY) {
		if(ob->action) {
			bActionChannel *achan;
			
			achan= get_action_channel(ob->action, "Shape");
			if(achan) ipo= achan->ipo;
		}
	}
	else ipo= key->ipo;
	
	/* global array */
	meshslidervals[keynum] = getrvkval(ipo, keynum);

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
				  meshslidervals+keynum, min, max, 10, 2, tip);
	
	uiButSetFunc(but, rvk_slider_func, ob, (void *)(intptr_t)keynum);
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
	icu->flag= IPO_VISIBLE|IPO_SELECT|IPO_AUTO_HORIZ;
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

static KeyBlock *add_keyblock(Key *key)
{
	KeyBlock *kb;
	float curpos= -0.1;
	int tot;
	
	kb= key->block.last;
	if(kb) curpos= kb->pos;
	
	kb= MEM_callocN(sizeof(KeyBlock), "Keyblock");
	BLI_addtail(&key->block, kb);
	kb->type= KEY_CARDINAL;
	tot= BLI_countlist(&key->block);
	if(tot==1) strcpy(kb->name, "Basis");
	else sprintf(kb->name, "Key %d", tot-1);
	kb->adrcode= tot-1;
	
	key->totkey++;
	if(key->totkey==1) key->refkey= kb;
	
	
	if(key->type == KEY_RELATIVE) 
		kb->pos= curpos+0.1;
	else {
		curpos= bsystem_time(0, (float)CFRA, 0.0);
		if(calc_ipo_spec(key->ipo, KEY_SPEED, &curpos)==0) {
			curpos /= 100.0;
		}
		kb->pos= curpos;
		
		sort_keys(key);
	}
	return kb;
}

void insert_meshkey(Mesh *me, short rel)
{
	Key *key;
	KeyBlock *kb;

	if(me->key==NULL) {
		me->key= add_key( (ID *)me);

		if(rel)
			me->key->type = KEY_RELATIVE;
		else
			default_key_ipo(me->key);
	}
	key= me->key;
	
	kb= add_keyblock(key);
	
	mesh_to_key(me, kb);
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

/* exported to python... hrms, should not, use object levels! (ton) */
void insert_lattkey(Lattice *lt, short rel)
{
	Key *key;
	KeyBlock *kb;
	
	if(lt->key==NULL) {
		lt->key= add_key( (ID *)lt);
		default_key_ipo(lt->key);
	}
	key= lt->key;
	
	kb= add_keyblock(key);
	
	latt_to_key(lt, kb);
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
				fp[0]= bezt->alfa;
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
				bezt->alfa= fp[0];
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


void insert_curvekey(Curve *cu, short rel) 
{
	Key *key;
	KeyBlock *kb;
	
	if(cu->key==NULL) {
		cu->key= add_key( (ID *)cu);

		if (rel)
			cu->key->type = KEY_RELATIVE;
		else
			default_key_ipo(cu->key);
	}
	key= cu->key;
	
	kb= add_keyblock(key);
	
	if(editNurb.first) curve_to_key(cu, kb, &editNurb);
	else curve_to_key(cu, kb, &cu->nurb);
}


/* ******************** */

void insert_shapekey(Object *ob)
{
	if(get_mesh(ob) && get_mesh(ob)->mr) {
		error("Cannot create shape keys on a multires mesh.");
	}
	else {
		Key *key;
	
		if(ob->type==OB_MESH) insert_meshkey(ob->data, 1);
		else if ELEM(ob->type, OB_CURVE, OB_SURF) insert_curvekey(ob->data, 1);
		else if(ob->type==OB_LATTICE) insert_lattkey(ob->data, 1);
	
		key= ob_get_key(ob);
		ob->shapenr= BLI_countlist(&key->block);
	
		BIF_undo_push("Add Shapekey");
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
}

void delete_key(Object *ob)
{
	KeyBlock *kb, *rkb;
	Key *key;
	IpoCurve *icu;
	
	key= ob_get_key(ob);
	if(key==NULL) return;
	
	kb= BLI_findlink(&key->block, ob->shapenr-1);

	if(kb) {
		for(rkb= key->block.first; rkb; rkb= rkb->next)
			if(rkb->relative == ob->shapenr-1)
				rkb->relative= 0;

		BLI_remlink(&key->block, kb);
		key->totkey--;
		if(key->refkey== kb) key->refkey= key->block.first;
			
		if(kb->data) MEM_freeN(kb->data);
		MEM_freeN(kb);
		
		for(kb= key->block.first; kb; kb= kb->next) {
			if(kb->adrcode>=ob->shapenr)
				kb->adrcode--;
		}
		
		if(key->ipo) {
			
			for(icu= key->ipo->curve.first; icu; icu= icu->next) {
				if(icu->adrcode==ob->shapenr-1) {
					BLI_remlink(&key->ipo->curve, icu);
					free_ipo_curve(icu);
					break;
				}
			}
			for(icu= key->ipo->curve.first; icu; icu= icu->next) 
				if(icu->adrcode>=ob->shapenr)
					icu->adrcode--;
		}		
		
		if(ob->shapenr>1) ob->shapenr--;
	}
	
	if(key->totkey==0) {
		if(GS(key->from->name)==ID_ME) ((Mesh *)key->from)->key= NULL;
		else if(GS(key->from->name)==ID_CU) ((Curve *)key->from)->key= NULL;
		else if(GS(key->from->name)==ID_LT) ((Lattice *)key->from)->key= NULL;

		free_libblock_us(&(G.main->key), key);
		scrarea_queue_headredraw(curarea);	/* ipo remove too */
	}
	
	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	
	BIF_undo_push("Delete Shapekey");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
}

void move_keys(Object *ob)
{
	Key *key;
	KeyBlock *kb;
	float div, dy, oldpos, vec[3], dvec[3];
	int afbreek=0, firsttime= 1;
	unsigned short event = 0;
	short mval[2], val, xo, yo;
	char str[32];
	
	if(G.sipo->blocktype!=ID_KE) return;
	
	if(G.sipo->ipo && G.sipo->ipo->id.lib) return;
	if(G.sipo->editipo==NULL) return;

	key= ob_get_key(ob);
	if(key==NULL) return;
	
	/* which kb is involved */
	kb= BLI_findlink(&key->block, ob->shapenr-1);
	if(kb==NULL) return;	
	
	oldpos= kb->pos;
	
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

			kb->pos= oldpos+vec[1];
			
			sprintf(str, "Y: %.3f  ", vec[1]);
			headerprint(str);
			
			xo= mval[0];
			yo= mval[1];
				
			force_draw(0);
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
		kb->pos= oldpos;
	}
	
	sort_keys(key);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	/* for boundbox */
	editipo_changed(G.sipo, 0);

	BIF_undo_push("Move Shapekey(s)");
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	scrarea_queue_redraw(curarea);
}
