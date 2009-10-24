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
#include "ED_mesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

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

static void insert_meshkey(Scene *scene, Object *ob)
{
	Mesh *me= ob->data;
	Key *key= me->key;;
	KeyBlock *kb;
	int newkey= 0;

	if(key == NULL) {
		key= me->key= add_key((ID *)me);
		key->type= KEY_RELATIVE;
		newkey= 1;
	}
	
	kb= add_keyblock(scene, key);
	
	if(newkey) {
		/* create from mesh */
		mesh_to_key(me, kb);
	}
	else {
		/* copy from current values */
		kb->data= do_ob_key(scene, ob);
		kb->totelem= me->totvert;
	}
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

static void insert_lattkey(Scene *scene, Object *ob)
{
	Lattice *lt= ob->data;
	Key *key= lt->key;
	KeyBlock *kb;
	int newkey= 0;
	
	if(key==NULL) {
		key= lt->key= add_key( (ID *)lt);
		key->type= KEY_RELATIVE;
	}

	kb= add_keyblock(scene, key);
	
	if(newkey) {
		/* create from lattice */
		latt_to_key(lt, kb);
	}
	else {
		/* copy from current values */
		kb->totelem= lt->pntsu*lt->pntsv*lt->pntsw;
		kb->data= do_ob_key(scene, ob);
	}
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


static void insert_curvekey(Scene *scene, Object *ob)
{
	Curve *cu= ob->data;
	Key *key= cu->key;
	KeyBlock *kb;
	ListBase *lb= (cu->editnurb)? cu->editnurb: &cu->nurb;
	int newkey= 0;
	
	if(key==NULL) {
		key= cu->key= add_key( (ID *)cu);
		key->type = KEY_RELATIVE;
		newkey= 1;
	}
	
	kb= add_keyblock(scene, key);
	
	if(newkey) {
		/* create from curve */
		curve_to_key(cu, kb, lb);
	}
	else {
		/* copy from current values */
		kb->totelem= count_curveverts(lb);
		kb->data= do_ob_key(scene, ob);
	}

}

/*********************** add shape key ***********************/

static void ED_object_shape_key_add(bContext *C, Scene *scene, Object *ob)
{
	Key *key;

	if(ob->type==OB_MESH) insert_meshkey(scene, ob);
	else if ELEM(ob->type, OB_CURVE, OB_SURF) insert_curvekey(scene, ob);
	else if(ob->type==OB_LATTICE) insert_lattkey(scene, ob);

	key= ob_get_key(ob);
	ob->shapenr= BLI_countlist(&key->block);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
}

/*********************** remove shape key ***********************/

static int ED_object_shape_key_remove(bContext *C, Object *ob)
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

static int ED_object_shape_key_mirror(bContext *C, Scene *scene, Object *ob)
{
	KeyBlock *kb;
	Key *key;

	key= ob_get_key(ob);
	if(key==NULL)
		return 0;
	
	kb= BLI_findlink(&key->block, ob->shapenr-1);

	if(kb) {
		int i1, i2;
		float *fp1, *fp2;
		float tvec[3];
		char *tag_elem= MEM_callocN(sizeof(char) * kb->totelem, "shape_key_mirror");


		if(ob->type==OB_MESH) {
			Mesh *me= ob->data;
			MVert *mv;

			mesh_octree_table(ob, NULL, NULL, 's');

			for(i1=0, mv=me->mvert; i1<me->totvert; i1++, mv++) {
				i2= mesh_get_x_mirror_vert(ob, i1);
				if(i2==i1) {
					fp1= ((float *)kb->data) + i1*3;
					fp1[0] = -fp1[0];
					tag_elem[i1]= 1;
				}
				else if(i2 != -1) {
					if(tag_elem[i1]==0 && tag_elem[i2]==0) {
						fp1= ((float *)kb->data) + i1*3;
						fp2= ((float *)kb->data) + i2*3;

						VECCOPY(tvec,	fp1);
						VECCOPY(fp1,	fp2);
						VECCOPY(fp2,	tvec);

						/* flip x axis */
						fp1[0] = -fp1[0];
						fp2[0] = -fp2[0];
					}
					tag_elem[i1]= tag_elem[i2]= 1;
				}
			}

			mesh_octree_table(ob, NULL, NULL, 'e');
		}
		/* todo, other types? */

		MEM_freeN(tag_elem);
	}
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return 1;
}

/********************** shape key operators *********************/

static int shape_key_mode_poll(bContext *C)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	ID *data= (ob)? ob->data: NULL;
	return (ob && !ob->id.lib && data && !data->lib && ob->mode != OB_MODE_EDIT);
}

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
	ot->poll= shape_key_mode_poll;
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
	ot->poll= shape_key_mode_poll;
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

static int shape_key_mirror_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ED_object_shape_key_mirror(C, scene, ob))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mirror Shape Key";
	ot->idname= "OBJECT_OT_shape_key_mirror";

	/* api callbacks */
	ot->poll= shape_key_mode_poll;
	ot->exec= shape_key_mirror_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int shape_key_move_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	int type= RNA_enum_get(op->ptr, "type");
	Key *key= ob_get_key(ob);

	if(key) {
		KeyBlock *kb, *kb_other;
		kb= BLI_findlink(&key->block, ob->shapenr-1);

		if(type==-1) {
			/* move back */
			if(kb->prev) {
				kb_other= kb->prev;
				BLI_remlink(&key->block, kb);
				BLI_insertlinkbefore(&key->block, kb_other, kb);
				ob->shapenr--;
			}
		}
		else {
			/* move next */
			if(kb->next) {
				kb_other= kb->next;
				BLI_remlink(&key->block, kb);
				BLI_insertlinkafter(&key->block, kb_other, kb);
				ob->shapenr++;
			}
		}
	}

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{-1, "UP", 0, "Up", ""},
		{1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name= "Move Shape Key";
	ot->idname= "OBJECT_OT_shape_key_move";

	/* api callbacks */
	ot->poll= shape_key_mode_poll;
	ot->exec= shape_key_move_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

