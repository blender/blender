/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, shapekey support
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_shapekey.c
 *  \ingroup edobj
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
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_curve.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "ED_object.h"
#include "ED_mesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/*********************** add shape key ***********************/

static void ED_object_shape_key_add(bContext *C, Scene *scene, Object *ob, int from_mix)
{
	KeyBlock *kb;
	if ((kb = BKE_object_insert_shape_key(scene, ob, NULL, from_mix))) {
		Key *key = ob_get_key(ob);
		/* for absolute shape keys, new keys may not be added last */
		ob->shapenr = BLI_findindex(&key->block, kb) + 1;

		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
}

/*********************** remove shape key ***********************/

static int ED_object_shape_key_remove(bContext *C, Object *ob)
{
	Main *bmain = CTX_data_main(C);
	KeyBlock *kb, *rkb;
	Key *key;
	//IpoCurve *icu;

	key = ob_get_key(ob);
	if (key == NULL)
		return 0;
	
	kb = BLI_findlink(&key->block, ob->shapenr - 1);

	if (kb) {
		for (rkb = key->block.first; rkb; rkb = rkb->next)
			if (rkb->relative == ob->shapenr - 1)
				rkb->relative = 0;

		BLI_remlink(&key->block, kb);
		key->totkey--;
		if (key->refkey == kb) {
			key->refkey = key->block.first;

			if (key->refkey) {
				/* apply new basis key on original data */
				switch (ob->type) {
					case OB_MESH:
						key_to_mesh(key->refkey, ob->data);
						break;
					case OB_CURVE:
					case OB_SURF:
						key_to_curve(key->refkey, ob->data, BKE_curve_nurbs_get(ob->data));
						break;
					case OB_LATTICE:
						key_to_latt(key->refkey, ob->data);
						break;
				}
			}
		}
			
		if (kb->data) MEM_freeN(kb->data);
		MEM_freeN(kb);

		if (ob->shapenr > 1) {
			ob->shapenr--;
		}
	}
	
	if (key->totkey == 0) {
		if (GS(key->from->name) == ID_ME) ((Mesh *)key->from)->key = NULL;
		else if (GS(key->from->name) == ID_CU) ((Curve *)key->from)->key = NULL;
		else if (GS(key->from->name) == ID_LT) ((Lattice *)key->from)->key = NULL;

		BKE_libblock_free_us(&(bmain->key), key);
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return 1;
}

static int object_shape_key_mirror(bContext *C, Object *ob)
{
	KeyBlock *kb;
	Key *key;

	key = ob_get_key(ob);
	if (key == NULL)
		return 0;
	
	kb = BLI_findlink(&key->block, ob->shapenr - 1);

	if (kb) {
		int i1, i2;
		float *fp1, *fp2;
		float tvec[3];
		char *tag_elem = MEM_callocN(sizeof(char) * kb->totelem, "shape_key_mirror");


		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			MVert *mv;

			mesh_octree_table(ob, NULL, NULL, 's');

			for (i1 = 0, mv = me->mvert; i1 < me->totvert; i1++, mv++) {
				i2 = mesh_get_x_mirror_vert(ob, i1);
				if (i2 == i1) {
					fp1 = ((float *)kb->data) + i1 * 3;
					fp1[0] = -fp1[0];
					tag_elem[i1] = 1;
				}
				else if (i2 != -1) {
					if (tag_elem[i1] == 0 && tag_elem[i2] == 0) {
						fp1 = ((float *)kb->data) + i1 * 3;
						fp2 = ((float *)kb->data) + i2 * 3;

						copy_v3_v3(tvec,    fp1);
						copy_v3_v3(fp1, fp2);
						copy_v3_v3(fp2, tvec);

						/* flip x axis */
						fp1[0] = -fp1[0];
						fp2[0] = -fp2[0];
					}
					tag_elem[i1] = tag_elem[i2] = 1;
				}
			}

			mesh_octree_table(ob, NULL, NULL, 'e');
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = ob->data;
			int i1, i2;
			float *fp1, *fp2;
			int u, v, w;
			/* half but found up odd value */
			const int pntsu_half = (lt->pntsu / 2) + (lt->pntsu % 2);

			/* currently editmode isn't supported by mesh so
			 * ignore here for now too */

			/* if (lt->editlatt) lt= lt->editlatt->latt; */

			for (w = 0; w < lt->pntsw; w++) {
				for (v = 0; v < lt->pntsv; v++) {
					for (u = 0; u < pntsu_half; u++) {
						int u_inv = (lt->pntsu - 1) - u;
						float tvec[3];
						if (u == u_inv) {
							i1 = LT_INDEX(lt, u, v, w);
							fp1 = ((float *)kb->data) + i1 * 3;
							fp1[0] = -fp1[0];
						}
						else {
							i1 = LT_INDEX(lt, u, v, w);
							i2 = LT_INDEX(lt, u_inv, v, w);

							fp1 = ((float *)kb->data) + i1 * 3;
							fp2 = ((float *)kb->data) + i2 * 3;

							copy_v3_v3(tvec, fp1);
							copy_v3_v3(fp1, fp2);
							copy_v3_v3(fp2, tvec);
							fp1[0] = -fp1[0];
							fp2[0] = -fp2[0];
						}
					}
				}
			}
		}

		MEM_freeN(tag_elem);
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return 1;
}

/********************** shape key operators *********************/

static int shape_key_mode_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && data && !data->lib && ob->mode != OB_MODE_EDIT);
}

static int shape_key_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && data && !data->lib);
}

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	int from_mix = RNA_boolean_get(op->ptr, "from_mix");

	ED_object_shape_key_add(C, scene, ob, from_mix);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Shape Key";
	ot->idname = "OBJECT_OT_shape_key_add";
	ot->description = "Add shape key to the object";
	
	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "from_mix", 1, "From Mix", "Create the new shape key from the existing mix of keys");
}

static int shape_key_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	if (!ED_object_shape_key_remove(C, ob))
		return OPERATOR_CANCELLED;
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Shape Key";
	ot->idname = "OBJECT_OT_shape_key_remove";
	ot->description = "Remove shape key from the object";
	
	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int shape_key_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Key *key = ob_get_key(ob);
	KeyBlock *kb = ob_get_keyblock(ob);

	if (!key || !kb)
		return OPERATOR_CANCELLED;
	
	for (kb = key->block.first; kb; kb = kb->next)
		kb->curval = 0.0f;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Shape Keys";
	ot->description = "Clear weights for all shape keys";
	ot->idname = "OBJECT_OT_shape_key_clear";
	
	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_clear_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* starting point and step size could be optional */
static int shape_key_retime_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Key *key = ob_get_key(ob);
	KeyBlock *kb = ob_get_keyblock(ob);
	float cfra = 0.0f;

	if (!key || !kb)
		return OPERATOR_CANCELLED;

	for (kb = key->block.first; kb; kb = kb->next)
		kb->pos = (cfra += 0.1f);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_retime(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Re-Time Shape Keys";
	ot->description = "Resets the timing for absolute shape keys";
	ot->idname = "OBJECT_OT_shape_key_retime";

	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_retime_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int shape_key_mirror_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	if (!object_shape_key_mirror(C, ob))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mirror Shape Key";
	ot->idname = "OBJECT_OT_shape_key_mirror";

	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_mirror_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int shape_key_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	int type = RNA_enum_get(op->ptr, "type");
	Key *key = ob_get_key(ob);

	if (key) {
		KeyBlock *kb, *kb_other;
		int shapenr_act = ob->shapenr - 1;
		int shapenr_swap = shapenr_act + type;
		kb = BLI_findlink(&key->block, shapenr_act);

		if ((type == -1 && kb->prev == NULL) || (type == 1 && kb->next == NULL)) {
			return OPERATOR_CANCELLED;
		}

		for (kb_other = key->block.first; kb_other; kb_other = kb_other->next) {
			if (kb_other->relative == shapenr_act) {
				kb_other->relative += type;
			}
			else if (kb_other->relative == shapenr_swap) {
				kb_other->relative -= type;
			}
		}

		if (type == -1) {
			/* move back */
			kb_other = kb->prev;
			BLI_remlink(&key->block, kb);
			BLI_insertlinkbefore(&key->block, kb_other, kb);
			ob->shapenr--;
		}
		else {
			/* move next */
			kb_other = kb->next;
			BLI_remlink(&key->block, kb);
			BLI_insertlinkafter(&key->block, kb_other, kb);
			ob->shapenr++;
		}

		SWAP(float, kb_other->pos, kb->pos) /* for absolute shape keys */
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

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
	ot->name = "Move Shape Key";
	ot->idname = "OBJECT_OT_shape_key_move";

	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

