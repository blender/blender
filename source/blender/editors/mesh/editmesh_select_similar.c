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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_select_similar.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_math_bits.h"
#include "BLI_rand.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_paint.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "UI_resources.h"

#include "bmesh_tools.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h"  /* own include */

/* use bmesh operator flags for a few operators */
#define BMO_ELE_TAG 1

/* -------------------------------------------------------------------- */
/** \name Select Similar (Vert/Edge/Face) Operator
 * \{ */

static const EnumPropertyItem prop_similar_compare_types[] = {
	{SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
	{SIM_CMP_GT, "GREATER", 0, "Greater", ""},
	{SIM_CMP_LT, "LESS", 0, "Less", ""},

	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem prop_similar_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Adjacent Faces", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},
	{SIMVERT_EDGE, "EDGE", 0, "Amount of connecting edges", ""},

	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Faces Around an Edge", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_BEVEL, "BEVEL", 0, "Bevel", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},
#ifdef WITH_FREESTYLE
	{SIMEDGE_FREESTYLE, "FREESTYLE_EDGE", 0, "Freestyle Edge Marks", ""},
#endif

	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_SIDES, "SIDES", 0, "Polygon Sides", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},
	{SIMFACE_SMOOTH, "SMOOTH", 0, "Flat/Smooth", ""},
	{SIMFACE_FACEMAP, "FACE_MAP", 0, "Face-Map", ""},
#ifdef WITH_FREESTYLE
	{SIMFACE_FREESTYLE, "FREESTYLE_FACE", 0, "Freestyle Face Marks", ""},
#endif

	{0, NULL, 0, NULL, NULL}
};

/* selects new faces/edges/verts based on the existing selection */

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_faces faces=%hf type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

/* ***************************************************** */

/* EDGE GROUP */

/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_edges edges=%he type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "edges.out", BM_EDGE, BM_ELEM_SELECT, true);
	EDBM_selectmode_flush(em);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

/* ********************************* */

/*
 * VERT GROUP
 * mode 1: same normal
 * mode 2: same number of face users
 * mode 3: same vertex groups
 */
static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;
	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_verts verts=%hv type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_selectmode_flush(em);

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

static int edbm_select_similar_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "threshold");

	const int type = RNA_enum_get(op->ptr, "type");

	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, ts->select_thresh);
	}
	else {
		ts->select_thresh = RNA_property_float_get(op->ptr, prop);
	}

	if      (type < 100) return similar_vert_select_exec(C, op);
	else if (type < 200) return similar_edge_select_exec(C, op);
	else                 return similar_face_select_exec(C, op);
}

static const EnumPropertyItem *select_similar_type_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
        bool *r_free)
{
	Object *obedit;

	if (!C) /* needed for docs and i18n tools */
		return prop_similar_types;

	obedit = CTX_data_edit_object(C);

	if (obedit && obedit->type == OB_MESH) {
		EnumPropertyItem *item = NULL;
		int a, totitem = 0;
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		if (em->selectmode & SCE_SELECT_VERTEX) {
			for (a = SIMVERT_NORMAL; a < SIMEDGE_LENGTH; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			for (a = SIMEDGE_LENGTH; a < SIMFACE_MATERIAL; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_FACE) {
#ifdef WITH_FREESTYLE
			const int a_end = SIMFACE_FREESTYLE;
#else
			const int a_end = SIMFACE_FACEMAP;
#endif
			for (a = SIMFACE_MATERIAL; a <= a_end; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		RNA_enum_item_end(&item, &totitem);

		*r_free = true;

		return item;
	}

	return prop_similar_types;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "MESH_OT_select_similar";
	ot->description = "Select similar vertices, edges or faces by property types";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_select_similar_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);

	RNA_def_enum(ot->srna, "compare", prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");

	RNA_def_float(ot->srna, "threshold", 0.0f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}

/** \} */
