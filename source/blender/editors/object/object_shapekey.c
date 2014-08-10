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
#include "BKE_lattice.h"
#include "BKE_curve.h"

#include "BLI_sys_types.h" // for intptr_t support

#include "ED_object.h"
#include "ED_mesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/*********************** add shape key ***********************/

static void ED_object_shape_key_add(bContext *C, Scene *scene, Object *ob, const bool from_mix)
{
	KeyBlock *kb;
	if ((kb = BKE_object_insert_shape_key(scene, ob, NULL, from_mix))) {
		Key *key = BKE_key_from_object(ob);
		/* for absolute shape keys, new keys may not be added last */
		ob->shapenr = BLI_findindex(&key->block, kb) + 1;

		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
}

/*********************** remove shape key ***********************/

static bool ED_object_shape_key_remove_all(Main *bmain, Object *ob)
{
	Key *key;

	key = BKE_key_from_object(ob);
	if (key == NULL)
		return false;

	switch (GS(key->from->name)) {
		case ID_ME: ((Mesh *)key->from)->key    = NULL; break;
		case ID_CU: ((Curve *)key->from)->key   = NULL; break;
		case ID_LT: ((Lattice *)key->from)->key = NULL; break;
	}

	BKE_libblock_free_us(bmain, key);

	return true;
}

static bool ED_object_shape_key_remove(Main *bmain, Object *ob)
{
	KeyBlock *kb, *rkb;
	Key *key;

	key = BKE_key_from_object(ob);
	if (key == NULL)
		return false;

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
						BKE_key_convert_to_mesh(key->refkey, ob->data);
						break;
					case OB_CURVE:
					case OB_SURF:
						BKE_key_convert_to_curve(key->refkey, ob->data, BKE_curve_nurbs_get(ob->data));
						break;
					case OB_LATTICE:
						BKE_key_convert_to_lattice(key->refkey, ob->data);
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
		switch (GS(key->from->name)) {
			case ID_ME: ((Mesh *)key->from)->key    = NULL; break;
			case ID_CU: ((Curve *)key->from)->key   = NULL; break;
			case ID_LT: ((Lattice *)key->from)->key = NULL; break;
		}

		BKE_libblock_free_us(bmain, key);
	}

	return true;
}

static bool object_shape_key_mirror(bContext *C, Object *ob,
                                    int *r_totmirr, int *r_totfail, bool use_topology)
{
	KeyBlock *kb;
	Key *key;
	int totmirr = 0, totfail = 0;

	*r_totmirr = *r_totfail = 0;

	key = BKE_key_from_object(ob);
	if (key == NULL)
		return 0;
	
	kb = BLI_findlink(&key->block, ob->shapenr - 1);

	if (kb) {
		char *tag_elem = MEM_callocN(sizeof(char) * kb->totelem, "shape_key_mirror");


		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			MVert *mv;
			int i1, i2;
			float *fp1, *fp2;
			float tvec[3];

			ED_mesh_mirror_spatial_table(ob, NULL, NULL, 's');

			for (i1 = 0, mv = me->mvert; i1 < me->totvert; i1++, mv++) {
				i2 = mesh_get_x_mirror_vert(ob, i1, use_topology);
				if (i2 == i1) {
					fp1 = ((float *)kb->data) + i1 * 3;
					fp1[0] = -fp1[0];
					tag_elem[i1] = 1;
					totmirr++;
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
						totmirr++;
					}
					tag_elem[i1] = tag_elem[i2] = 1;
				}
				else {
					totfail++;
				}
			}

			ED_mesh_mirror_spatial_table(ob, NULL, NULL, 'e');
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

			/* if (lt->editlatt) lt = lt->editlatt->latt; */

			for (w = 0; w < lt->pntsw; w++) {
				for (v = 0; v < lt->pntsv; v++) {
					for (u = 0; u < pntsu_half; u++) {
						int u_inv = (lt->pntsu - 1) - u;
						float tvec[3];
						if (u == u_inv) {
							i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
							fp1 = ((float *)kb->data) + i1 * 3;
							fp1[0] = -fp1[0];
							totmirr++;
						}
						else {
							i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
							i2 = BKE_lattice_index_from_uvw(lt, u_inv, v, w);

							fp1 = ((float *)kb->data) + i1 * 3;
							fp2 = ((float *)kb->data) + i2 * 3;

							copy_v3_v3(tvec, fp1);
							copy_v3_v3(fp1, fp2);
							copy_v3_v3(fp2, tvec);
							fp1[0] = -fp1[0];
							fp2[0] = -fp2[0];
							totmirr++;
						}
					}
				}
			}
		}

		MEM_freeN(tag_elem);
	}
	
	*r_totmirr = totmirr;
	*r_totfail = totfail;

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

static int shape_key_mode_exists_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	/* same as shape_key_mode_poll */
	return (ob && !ob->id.lib && data && !data->lib && ob->mode != OB_MODE_EDIT) &&
	       /* check a keyblock exists */
	       (BKE_keyblock_from_object(ob) != NULL);
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
	const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");

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
	RNA_def_boolean(ot->srna, "from_mix", true, "From Mix", "Create the new shape key from the existing mix of keys");
}

static int shape_key_remove_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = ED_object_context(C);
	bool changed = false;

	if (RNA_boolean_get(op->ptr, "all")) {
		changed = ED_object_shape_key_remove_all(bmain, ob);
	}
	else {
		changed = ED_object_shape_key_remove(bmain, ob);
	}

	if (changed) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_shape_key_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Shape Key";
	ot->idname = "OBJECT_OT_shape_key_remove";
	ot->description = "Remove shape key from the object";
	
	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->poll = shape_key_mode_exists_poll;
	ot->exec = shape_key_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Remove all shape keys");
}

static int shape_key_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb = BKE_keyblock_from_object(ob);

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
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb = BKE_keyblock_from_object(ob);
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

static int shape_key_mirror_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	int totmirr = 0, totfail = 0;
	bool use_topology = RNA_boolean_get(op->ptr, "use_topology");

	if (!object_shape_key_mirror(C, ob, &totmirr, &totfail, use_topology))
		return OPERATOR_CANCELLED;

	ED_mesh_report_mirror(op, totmirr, totfail);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mirror Shape Key";
	ot->idname = "OBJECT_OT_shape_key_mirror";
	ot->description = "Mirror the current shape key along the local X axis";

	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_mirror_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "use_topology", 0, "Topology Mirror",
	                "Use topology based mirroring (for when both sides of mesh have matching, unique topology)");
}


static int shape_key_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	Key *key = BKE_key_from_object(ob);

	if (!key) {
		return OPERATOR_CANCELLED;
	}

	{
		KeyBlock *kb, *kb_other, *kb_iter;
		const int type = RNA_enum_get(op->ptr, "type");
		const int shape_tot = BLI_countlist(&key->block);
		const int shapenr_act = ob->shapenr - 1;
		const int shapenr_swap = (shape_tot + shapenr_act + type) % shape_tot;

		kb = BLI_findlink(&key->block, shapenr_act);
		if (!kb || shape_tot == 1) {
			return OPERATOR_CANCELLED;
		}

		if (type == -1) {
			/* move back */
			kb_other = kb->prev;
			BLI_remlink(&key->block, kb);
			BLI_insertlinkbefore(&key->block, kb_other, kb);
		}
		else {
			/* move next */
			kb_other = kb->next;
			BLI_remlink(&key->block, kb);
			BLI_insertlinkafter(&key->block, kb_other, kb);
		}

		ob->shapenr = shapenr_swap + 1;

		/* for relative shape keys */
		if (kb_other) {
			for (kb_iter = key->block.first; kb_iter; kb_iter = kb_iter->next) {
				if (kb_iter->relative == shapenr_act) {
					kb_iter->relative = shapenr_swap;
				}
				else if (kb_iter->relative == shapenr_swap) {
					kb_iter->relative = shapenr_act;
				}
			}
		}
		/* First key became last, or vice-versa, we have to change all keys' relative value. */
		else {
			for (kb_iter = key->block.first; kb_iter; kb_iter = kb_iter->next) {
				if (kb_iter->relative == shapenr_act) {
					kb_iter->relative = shapenr_swap;
				}
				else {
					kb_iter->relative += type;
				}
			}
		}

		/* for absolute shape keys */
		if (kb_other) {
			SWAP(float, kb_other->pos, kb->pos);
		}
		/* First key became last, or vice-versa, we have to change all keys' pos value. */
		else {
			float pos = kb->pos;
			if (type == -1) {
				for (kb_iter = key->block.first; kb_iter; kb_iter = kb_iter->next) {
					SWAP(float, kb_iter->pos, pos);
				}
			}
			else {
				for (kb_iter = key->block.last; kb_iter; kb_iter = kb_iter->prev) {
					SWAP(float, kb_iter->pos, pos);
				}
			}
		}

		/* First key is refkey, matches interface and BKE_key_sort */
		key->refkey = key->block.first;
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
	ot->description = "Move the active shape key up/down in the list";

	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

