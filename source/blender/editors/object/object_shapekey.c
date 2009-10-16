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
 * Contributor(s): Blender Foundation, shapekey support
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
#include "BKE_context.h"
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

#include "BLO_sys_types.h" // for intptr_t support

#include "ED_object.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

#if 0 // XXX old animation system
static void default_key_ipo(Scene *scene, Key *key)
{
	IpoCurve *icu;
	BezTriple *bezt;
	
	key->ipo= add_ipo(scene, "KeyIpo", ID_KE);
	
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
#endif // XXX old animation system
	

/************************* Mesh ************************/

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

static KeyBlock *add_keyblock(Scene *scene, Key *key)
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
	
		// XXX this is old anim system stuff? (i.e. the 'index' of the shapekey)
	kb->adrcode= tot-1;
	
	key->totkey++;
	if(key->totkey==1) key->refkey= kb;
	
	kb->slidermin= 0.0f;
	kb->slidermax= 1.0f;
	
	// XXX kb->pos is the confusing old horizontal-line RVK crap in old IPO Editor...
	if(key->type == KEY_RELATIVE) 
		kb->pos= curpos+0.1;
	else {
#if 0 // XXX old animation system
		curpos= bsystem_time(scene, 0, (float)CFRA, 0.0);
		if(calc_ipo_spec(key->ipo, KEY_SPEED, &curpos)==0) {
			curpos /= 100.0;
		}
		kb->pos= curpos;
		
		sort_keys(key);
#endif // XXX old animation system
	}
	return kb;
}

void insert_meshkey(Scene *scene, Mesh *me, short rel)
{
	Key *key;
	KeyBlock *kb;

	if(me->key==NULL) {
		me->key= add_key( (ID *)me);

		if(rel)
			me->key->type = KEY_RELATIVE;
//		else
//			default_key_ipo(scene, me->key); // XXX old animation system
	}
	key= me->key;
	
	kb= add_keyblock(scene, key);
	
	mesh_to_key(me, kb);
}

/************************* Lattice ************************/

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
void insert_lattkey(Scene *scene, Lattice *lt, short rel)
{
	Key *key;
	KeyBlock *kb;
	
	if(lt->key==NULL) {
		lt->key= add_key( (ID *)lt);
//		default_key_ipo(scene, lt->key); // XXX old animation system
	}
	key= lt->key;
	
	kb= add_keyblock(scene, key);
	
	latt_to_key(lt, kb);
}

/************************* Curve ************************/

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


void insert_curvekey(Scene *scene, Curve *cu, short rel) 
{
	Key *key;
	KeyBlock *kb;
	
	if(cu->key==NULL) {
		cu->key= add_key( (ID *)cu);

		if(rel)
			cu->key->type = KEY_RELATIVE;
//		else
//			default_key_ipo(scene, cu->key);	// XXX old animation system
	}
	key= cu->key;
	
	kb= add_keyblock(scene, key);
	
	if(cu->editnurb->first) curve_to_key(cu, kb, cu->editnurb);
	else curve_to_key(cu, kb, &cu->nurb);
}

/*********************** add shape key ***********************/

void ED_object_shape_key_add(bContext *C, Scene *scene, Object *ob)
{
	Key *key;

	if(ob->type==OB_MESH) insert_meshkey(scene, ob->data, 1);
	else if ELEM(ob->type, OB_CURVE, OB_SURF) insert_curvekey(scene, ob->data, 1);
	else if(ob->type==OB_LATTICE) insert_lattkey(scene, ob->data, 1);

	key= ob_get_key(ob);
	ob->shapenr= BLI_countlist(&key->block);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
}

/*********************** remove shape key ***********************/

int ED_object_shape_key_remove(bContext *C, Object *ob)
{
	Main *bmain= CTX_data_main(C);
	KeyBlock *kb, *rkb;
	Key *key;
	//IpoCurve *icu;

	key= ob_get_key(ob);
	if(key==NULL)
		return 0;
	
	kb= BLI_findlink(&key->block, ob->shapenr-1);

	if(kb) {
		for(rkb= key->block.first; rkb; rkb= rkb->next)
			if(rkb->relative == ob->shapenr-1)
				rkb->relative= 0;

		BLI_remlink(&key->block, kb);
		key->totkey--;
		if(key->refkey== kb)
			key->refkey= key->block.first;
			
		if(kb->data) MEM_freeN(kb->data);
		MEM_freeN(kb);
		
		for(kb= key->block.first; kb; kb= kb->next)
			if(kb->adrcode>=ob->shapenr)
				kb->adrcode--;
		
#if 0 // XXX old animation system
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
#endif // XXX old animation system		
		
		if(ob->shapenr>1) ob->shapenr--;
	}
	
	if(key->totkey==0) {
		if(GS(key->from->name)==ID_ME) ((Mesh *)key->from)->key= NULL;
		else if(GS(key->from->name)==ID_CU) ((Curve *)key->from)->key= NULL;
		else if(GS(key->from->name)==ID_LT) ((Lattice *)key->from)->key= NULL;

		free_libblock_us(&(bmain->key), key);
	}
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return 1;
}

/********************** shape key operators *********************/

static int shape_key_poll(bContext *C)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	ID *data= (ob)? ob->data: NULL;
	return (ob && !ob->id.lib && data && !data->lib);
}

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	ED_object_shape_key_add(C, scene, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Shape Key";
	ot->name= "Add shape key to the object.";
	ot->idname= "OBJECT_OT_shape_key_add";
	
	/* api callbacks */
	ot->poll= shape_key_poll;
	ot->exec= shape_key_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int shape_key_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ED_object_shape_key_remove(C, ob))
		return OPERATOR_CANCELLED;
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Shape Key";
	ot->name= "Remove shape key from the object.";
	ot->idname= "OBJECT_OT_shape_key_remove";
	
	/* api callbacks */
	ot->poll= shape_key_poll;
	ot->exec= shape_key_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int shape_key_clear_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Key *key= ob_get_key(ob);
	KeyBlock *kb= ob_get_keyblock(ob);

	if(!key || !kb)
		return OPERATOR_CANCELLED;
	
	for(kb=key->block.first; kb; kb=kb->next)
		kb->curval= 0.0f;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Shape Keys";
	ot->description= "Clear weights for all shape keys.";
	ot->idname= "OBJECT_OT_shape_key_clear";
	
	/* api callbacks */
	ot->poll= shape_key_poll;
	ot->exec= shape_key_clear_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

