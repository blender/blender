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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joseph Eagar, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_loopcut.c
 *  \ingroup edmesh
 */

#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_string.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_unit.h"

#include "BIF_gl.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_numinput.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mesh_intern.h"  /* own include */

#define SUBD_SMOOTH_MAX 4.0f
#define SUBD_CUTS_MAX 500

/* ringsel operator */

/* struct for properties used while drawing */
typedef struct RingSelOpData {
	ARegion *ar;        /* region that ringsel was activated in */
	void *draw_handle;  /* for drawing preview loop */
	
	float (*edges)[2][3];
	int totedge;

	float (*points)[3];
	int totpoint;

	ViewContext vc;

	Object *ob;
	BMEditMesh *em;
	BMEdge *eed;
	NumInput num;

	bool extend;
	bool do_cut;
} RingSelOpData;

/* modal loop selection drawing callback */
static void ringsel_draw(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	View3D *v3d = CTX_wm_view3d(C);
	RingSelOpData *lcd = arg;
	
	if ((lcd->totedge > 0) || (lcd->totpoint > 0)) {
		if (v3d && v3d->zbuf)
			glDisable(GL_DEPTH_TEST);

		glPushMatrix();
		glMultMatrixf(lcd->ob->obmat);

		glColor3ub(255, 0, 255);
		if (lcd->totedge > 0) {
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, lcd->edges);
			glDrawArrays(GL_LINES, 0, lcd->totedge * 2);
			glDisableClientState(GL_VERTEX_ARRAY);
		}

		if (lcd->totpoint > 0) {
			glPointSize(3.0f);

			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, lcd->points);
			glDrawArrays(GL_POINTS, 0, lcd->totpoint);
			glDisableClientState(GL_VERTEX_ARRAY);

			glPointSize(1.0f);
		}

		glPopMatrix();
		if (v3d && v3d->zbuf)
			glEnable(GL_DEPTH_TEST);
	}
}

/* given two opposite edges in a face, finds the ordering of their vertices so
 * that cut preview lines won't cross each other */
static void edgering_find_order(BMEdge *lasteed, BMEdge *eed,
                                BMVert *lastv1, BMVert *v[2][2])
{
	BMIter liter;
	BMLoop *l, *l2;
	int rev;

	l = eed->l;

	/* find correct order for v[1] */
	if (!(BM_edge_in_face(eed, l->f) && BM_edge_in_face(lasteed, l->f))) {
		BM_ITER_ELEM (l, &liter, l, BM_LOOPS_OF_LOOP) {
			if (BM_edge_in_face(eed, l->f) && BM_edge_in_face(lasteed, l->f))
				break;
		}
	}
	
	/* this should never happen */
	if (!l) {
		v[0][0] = eed->v1;
		v[0][1] = eed->v2;
		v[1][0] = lasteed->v1;
		v[1][1] = lasteed->v2;
		return;
	}
	
	l2 = BM_loop_other_edge_loop(l, eed->v1);
	rev = (l2 == l->prev);
	while (l2->v != lasteed->v1 && l2->v != lasteed->v2) {
		l2 = rev ? l2->prev : l2->next;
	}

	if (l2->v == lastv1) {
		v[0][0] = eed->v1;
		v[0][1] = eed->v2;
	}
	else {
		v[0][0] = eed->v2;
		v[0][1] = eed->v1;
	}
}

static void edgering_vcos_get(DerivedMesh *dm, BMVert *v[2][2], float r_cos[2][2][3])
{
	if (dm) {
		int j, k;
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 2; k++) {
				dm->getVertCo(dm, BM_elem_index_get(v[j][k]), r_cos[j][k]);
			}
		}
	}
	else {
		int j, k;
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 2; k++) {
				copy_v3_v3(r_cos[j][k], v[j][k]->co);
			}
		}
	}
}

static void edgering_vcos_get_pair(DerivedMesh *dm, BMVert *v[2], float r_cos[2][3])
{
	if (dm) {
		int j;
		for (j = 0; j < 2; j++) {
			dm->getVertCo(dm, BM_elem_index_get(v[j]), r_cos[j]);
		}
	}
	else {
		int j;
		for (j = 0; j < 2; j++) {
			copy_v3_v3(r_cos[j], v[j]->co);
		}
	}
}

static void edgering_preview_free(RingSelOpData *lcd)
{
	MEM_SAFE_FREE(lcd->edges);
	lcd->totedge = 0;

	MEM_SAFE_FREE(lcd->points);
	lcd->totpoint = 0;
}

static void edgering_preview_calc_edges(RingSelOpData *lcd, DerivedMesh *dm, const int previewlines)
{
	BMesh *bm = lcd->em->bm;
	BMWalker walker;
	BMEdge *eed_start = lcd->eed;
	BMEdge *eed, *eed_last;
	BMVert *v[2][2] = {{NULL}}, *v_last;
	float (*edges)[2][3] = NULL;
	BLI_array_declare(edges);
	int i, tot = 0;

	BMW_init(&walker, bm, BMW_EDGERING,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_FLAG_TEST_HIDDEN,
	         BMW_NIL_LAY);

	v_last   = NULL;
	eed_last = NULL;

	for (eed = eed_start = BMW_begin(&walker, eed_start); eed; eed = BMW_step(&walker)) {
		if (eed_last) {
			if (v_last) {
				v[1][0] = v[0][0];
				v[1][1] = v[0][1];
			}
			else {
				v[1][0] = eed_last->v1;
				v[1][1] = eed_last->v2;
				v_last  = eed_last->v1;
			}

			edgering_find_order(eed_last, eed, v_last, v);
			v_last = v[0][0];

			BLI_array_grow_items(edges, previewlines);

			for (i = 1; i <= previewlines; i++) {
				const float fac = (i / ((float)previewlines + 1));
				float v_cos[2][2][3];

				edgering_vcos_get(dm, v, v_cos);

				interp_v3_v3v3(edges[tot][0], v_cos[0][0], v_cos[0][1], fac);
				interp_v3_v3v3(edges[tot][1], v_cos[1][0], v_cos[1][1], fac);
				tot++;
			}
		}
		eed_last = eed;
	}

	if ((eed_last != eed_start) &&
#ifdef BMW_EDGERING_NGON
	    BM_edge_share_face_check(eed_last, eed_start)
#else
	    BM_edge_share_quad_check(eed_last, eed_start)
#endif
	    )
	{
		v[1][0] = v[0][0];
		v[1][1] = v[0][1];

		edgering_find_order(eed_last, eed_start, v_last, v);

		BLI_array_grow_items(edges, previewlines);

		for (i = 1; i <= previewlines; i++) {
			const float fac = (i / ((float)previewlines + 1));
			float v_cos[2][2][3];

			if (!v[0][0] || !v[0][1] || !v[1][0] || !v[1][1]) {
				continue;
			}

			edgering_vcos_get(dm, v, v_cos);

			interp_v3_v3v3(edges[tot][0], v_cos[0][0], v_cos[0][1], fac);
			interp_v3_v3v3(edges[tot][1], v_cos[1][0], v_cos[1][1], fac);
			tot++;
		}
	}

	BMW_end(&walker);
	lcd->edges = edges;
	lcd->totedge = tot;
}

static void edgering_preview_calc_points(RingSelOpData *lcd, DerivedMesh *dm, const int previewlines)
{
	float v_cos[2][3];
	float (*points)[3];
	int i, tot = 0;

	if (dm) {
		BM_mesh_elem_table_ensure(lcd->em->bm, BM_VERT);
	}

	points = MEM_mallocN(sizeof(*lcd->points) * previewlines, __func__);

	edgering_vcos_get_pair(dm, &lcd->eed->v1, v_cos);

	for (i = 1; i <= previewlines; i++) {
		const float fac = (i / ((float)previewlines + 1));
		interp_v3_v3v3(points[tot], v_cos[0], v_cos[1], fac);
		tot++;
	}

	lcd->points = points;
	lcd->totpoint = previewlines;
}

static void edgering_preview_calc(RingSelOpData *lcd, const int previewlines)
{
	DerivedMesh *dm;

	BLI_assert(lcd->eed != NULL);

	edgering_preview_free(lcd);

	dm = EDBM_mesh_deform_dm_get(lcd->em);
	if (dm) {
		BM_mesh_elem_table_ensure(lcd->em->bm, BM_VERT);
	}

	if (BM_edge_is_wire(lcd->eed)) {
		edgering_preview_calc_points(lcd, dm, previewlines);
	}
	else {
		edgering_preview_calc_edges(lcd, dm, previewlines);
	}
}

static void edgering_select(RingSelOpData *lcd)
{
	BMEditMesh *em = lcd->em;
	BMEdge *eed_start = lcd->eed;
	BMWalker walker;
	BMEdge *eed;
	
	if (!eed_start)
		return;

	if (!lcd->extend) {
		EDBM_flag_disable_all(lcd->em, BM_ELEM_SELECT);
	}

	BMW_init(&walker, em->bm, BMW_EDGERING,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_FLAG_TEST_HIDDEN,
	         BMW_NIL_LAY);

	for (eed = BMW_begin(&walker, eed_start); eed; eed = BMW_step(&walker)) {
		BM_edge_select_set(em->bm, eed, true);
	}
	BMW_end(&walker);
}

static void ringsel_find_edge(RingSelOpData *lcd, const int previewlines)
{
	if (lcd->eed) {
		edgering_preview_calc(lcd, previewlines);
	}
	else {
		edgering_preview_free(lcd);
	}
}

static void ringsel_finish(bContext *C, wmOperator *op)
{
	RingSelOpData *lcd = op->customdata;
	const int cuts = RNA_int_get(op->ptr, "number_cuts");
	const float smoothness = 0.292f * RNA_float_get(op->ptr, "smoothness");
	const int smooth_falloff = RNA_enum_get(op->ptr, "falloff");
#ifdef BMW_EDGERING_NGON
	const bool use_only_quads = false;
#else
	const bool use_only_quads = false;
#endif

	if (lcd->eed) {
		BMEditMesh *em = lcd->em;
		BMVert *v_eed_orig[2] = {lcd->eed->v1, lcd->eed->v2};

		edgering_select(lcd);
		
		if (lcd->do_cut) {
			const bool is_macro = (op->opm != NULL);
			/* a single edge (rare, but better support) */
			const bool is_single = (BM_edge_is_wire(lcd->eed));
			const int seltype = is_single ? SUBDIV_SELECT_INNER : SUBDIV_SELECT_LOOPCUT;

			/* Enable gridfill, so that intersecting loopcut works as one would expect.
			 * Note though that it will break edgeslide in this specific case.
			 * See [#31939]. */
			BM_mesh_esubdivide(em->bm, BM_ELEM_SELECT,
			                   smoothness, smooth_falloff, true,
			                   0.0f, 0.0f,
			                   cuts, seltype, SUBD_CORNER_PATH, 0, true,
			                   use_only_quads, 0);

			/* when used in a macro tessface is already re-recalculated */
			EDBM_update_generic(em, (is_macro == false), true);

			if (is_single) {
				/* de-select endpoints */
				BM_vert_select_set(em->bm, v_eed_orig[0], false);
				BM_vert_select_set(em->bm, v_eed_orig[1], false);

				EDBM_selectmode_flush_ex(lcd->em, SCE_SELECT_VERTEX);
			}
			/* we cant slide multiple edges in vertex select mode */
			else if (is_macro && (cuts > 1) && (em->selectmode & SCE_SELECT_VERTEX)) {
				EDBM_selectmode_disable(lcd->vc.scene, em, SCE_SELECT_VERTEX, SCE_SELECT_EDGE);
			}
			/* force edge slide to edge select mode in in face select mode */
			else if (EDBM_selectmode_disable(lcd->vc.scene, em, SCE_SELECT_FACE, SCE_SELECT_EDGE)) {
				/* pass, the change will flush selection */
			}
			else {
				/* else flush explicitly */
				EDBM_selectmode_flush(lcd->em);
			}
		}
		else {
			/* XXX Is this piece of code ever used now? Simple loop select is now
			 *     in editmesh_select.c (around line 1000)... */
			/* sets as active, useful for other tools */
			if (em->selectmode & SCE_SELECT_VERTEX)
				BM_select_history_store(em->bm, lcd->eed->v1);  /* low priority TODO, get vertrex close to mouse */
			if (em->selectmode & SCE_SELECT_EDGE)
				BM_select_history_store(em->bm, lcd->eed);
			
			EDBM_selectmode_flush(lcd->em);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, lcd->ob->data);
		}
	}
}

/* called when modal loop selection is done... */
static void ringsel_exit(bContext *UNUSED(C), wmOperator *op)
{
	RingSelOpData *lcd = op->customdata;

	/* deactivate the extra drawing stuff in 3D-View */
	ED_region_draw_cb_exit(lcd->ar->type, lcd->draw_handle);
	
	edgering_preview_free(lcd);

	ED_region_tag_redraw(lcd->ar);

	/* free the custom data */
	MEM_freeN(lcd);
	op->customdata = NULL;
}


/* called when modal loop selection gets set up... */
static int ringsel_init(bContext *C, wmOperator *op, bool do_cut)
{
	RingSelOpData *lcd;
	Scene *scene = CTX_data_scene(C);

	/* alloc new customdata */
	lcd = op->customdata = MEM_callocN(sizeof(RingSelOpData), "ringsel Modal Op Data");
	
	/* assign the drawing handle for drawing preview line... */
	lcd->ar = CTX_wm_region(C);
	lcd->draw_handle = ED_region_draw_cb_activate(lcd->ar->type, ringsel_draw, lcd, REGION_DRAW_POST_VIEW);
	lcd->ob = CTX_data_edit_object(C);
	lcd->em = BKE_editmesh_from_object(lcd->ob);
	lcd->extend = do_cut ? false : RNA_boolean_get(op->ptr, "extend");
	lcd->do_cut = do_cut;
	
	initNumInput(&lcd->num);
	lcd->num.idx_max = 1;
	lcd->num.val_flag[0] |= NUM_NO_NEGATIVE | NUM_NO_FRACTION;
	/* No specific flags for smoothness. */
	lcd->num.unit_sys = scene->unit.system;
	lcd->num.unit_type[0] = B_UNIT_NONE;
	lcd->num.unit_type[1] = B_UNIT_NONE;

	/* XXX, temp, workaround for [#	] */
	EDBM_mesh_ensure_valid_dm_hack(scene, lcd->em);

	em_setup_viewcontext(C, &lcd->vc);

	ED_region_tag_redraw(lcd->ar);

	return 1;
}

static void ringcut_cancel(bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	ringsel_exit(C, op);
}

static void loopcut_update_edge(RingSelOpData *lcd, BMEdge *e, const int previewlines)
{
	if (e != lcd->eed) {
		lcd->eed = e;
		ringsel_find_edge(lcd, previewlines);
	}
}

static void loopcut_mouse_move(RingSelOpData *lcd, const int previewlines)
{
	float dist = ED_view3d_select_dist_px();
	BMEdge *e = EDBM_edge_find_nearest(&lcd->vc, &dist);
	loopcut_update_edge(lcd, e, previewlines);
}

/* called by both init() and exec() */
static int loopcut_init(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool is_interactive = (event != NULL);
	Object *obedit = CTX_data_edit_object(C);
	RingSelOpData *lcd;

	if (modifiers_isDeformedByLattice(obedit) || modifiers_isDeformedByArmature(obedit))
		BKE_report(op->reports, RPT_WARNING, "Loop cut does not work well on deformed edit mesh display");

	view3d_operator_needs_opengl(C);

	/* for re-execution, check edge index is in range before we setup ringsel */
	if (is_interactive == false) {
		const int e_index = RNA_int_get(op->ptr, "edge_index");
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (UNLIKELY((e_index == -1) || (e_index >= em->bm->totedge))) {
			return OPERATOR_CANCELLED;
		}
	}

	if (!ringsel_init(C, op, true))
		return OPERATOR_CANCELLED;

	/* add a modal handler for this operator - handles loop selection */
	if (is_interactive) {
		WM_event_add_modal_handler(C, op);
	}

	lcd = op->customdata;

	if (is_interactive) {
		copy_v2_v2_int(lcd->vc.mval, event->mval);
		loopcut_mouse_move(lcd, is_interactive ? 1 : 0);
	}
	else {
		const int e_index = RNA_int_get(op->ptr, "edge_index");
		BMEdge *e;
		BM_mesh_elem_table_ensure(lcd->em->bm, BM_EDGE);
		e = BM_edge_at_index(lcd->em->bm, e_index);
		loopcut_update_edge(lcd, e, 0);
	}

#ifdef USE_LOOPSLIDE_HACK
	/* for use in macro so we can restore, HACK */
	{
		Scene *scene = CTX_data_scene(C);
		ToolSettings *settings = scene->toolsettings;
		const int mesh_select_mode[3] = {
		    (settings->selectmode & SCE_SELECT_VERTEX) != 0,
		    (settings->selectmode & SCE_SELECT_EDGE)   != 0,
		    (settings->selectmode & SCE_SELECT_FACE)   != 0,
		};

		RNA_boolean_set_array(op->ptr, "mesh_select_mode_init", mesh_select_mode);
	}
#endif

	if (is_interactive) {
		ScrArea *sa = CTX_wm_area(C);
		ED_area_headerprint(sa, IFACE_("Select a ring to be cut, use mouse-wheel or page-up/down for number of cuts, "
		                               "hold Alt for smooth"));
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		ringsel_finish(C, op);
		ringsel_exit(C, op);
		return OPERATOR_FINISHED;
	}
}

static int ringcut_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	return loopcut_init(C, op, event);
}

static int loopcut_exec(bContext *C, wmOperator *op)
{
	return loopcut_init(C, op, NULL);
}

static int loopcut_finish(RingSelOpData *lcd, bContext *C, wmOperator *op)
{
	/* finish */
	ED_region_tag_redraw(lcd->ar);
	ED_area_headerprint(CTX_wm_area(C), NULL);

	if (lcd->eed) {
		/* set for redo */
		BM_mesh_elem_index_ensure(lcd->em->bm, BM_EDGE);
		RNA_int_set(op->ptr, "edge_index", BM_elem_index_get(lcd->eed));

		/* execute */
		ringsel_finish(C, op);
		ringsel_exit(C, op);
	}
	else {
		ringcut_cancel(C, op);
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

static int loopcut_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	float smoothness = RNA_float_get(op->ptr, "smoothness");
	int cuts = RNA_int_get(op->ptr, "number_cuts");
	RingSelOpData *lcd = op->customdata;
	bool show_cuts = false;
	const bool has_numinput = hasNumInput(&lcd->num);

	view3d_operator_needs_opengl(C);

	/* using the keyboard to input the number of cuts */
	/* Modal numinput active, try to handle numeric inputs first... */
	if (event->val == KM_PRESS && has_numinput && handleNumInput(C, &lcd->num, event)) {
		float values[2] = {(float)cuts, smoothness};
		applyNumInput(&lcd->num, values);

		/* allow zero so you can backspace and type in a value
		 * otherwise 1 as minimum would make more sense */
		cuts = CLAMPIS(values[0], 0, SUBD_CUTS_MAX);
		smoothness = CLAMPIS(values[1], -SUBD_SMOOTH_MAX, SUBD_SMOOTH_MAX);

		RNA_int_set(op->ptr, "number_cuts", cuts);
		ringsel_find_edge(lcd, cuts);
		show_cuts = true;
		RNA_float_set(op->ptr, "smoothness", smoothness);

		ED_region_tag_redraw(lcd->ar);
	}
	else {
		bool handled = false;
		switch (event->type) {
			case RETKEY:
			case PADENTER:
			case LEFTMOUSE: /* confirm */ // XXX hardcoded
				if (event->val == KM_PRESS)
					return loopcut_finish(lcd, C, op);
				
				ED_region_tag_redraw(lcd->ar);
				handled = true;
				break;
			case RIGHTMOUSE: /* abort */ // XXX hardcoded
				ED_region_tag_redraw(lcd->ar);
				ringsel_exit(C, op);
				ED_area_headerprint(CTX_wm_area(C), NULL);

				return OPERATOR_CANCELLED;
			case ESCKEY:
				if (event->val == KM_RELEASE) {
					/* cancel */
					ED_region_tag_redraw(lcd->ar);
					ED_area_headerprint(CTX_wm_area(C), NULL);
					
					ringcut_cancel(C, op);
					return OPERATOR_CANCELLED;
				}
				
				ED_region_tag_redraw(lcd->ar);
				handled = true;
				break;
			case PADPLUSKEY:
			case PAGEUPKEY:
			case WHEELUPMOUSE:  /* change number of cuts */
				if (event->val == KM_RELEASE)
					break;
				if (event->alt == 0) {
					cuts++;
					cuts = CLAMPIS(cuts, 0, SUBD_CUTS_MAX);
					RNA_int_set(op->ptr, "number_cuts", cuts);
					ringsel_find_edge(lcd, cuts);
					show_cuts = true;
				}
				else {
					smoothness = min_ff(smoothness + 0.05f, SUBD_SMOOTH_MAX);
					RNA_float_set(op->ptr, "smoothness", smoothness);
					show_cuts = true;
				}
				
				ED_region_tag_redraw(lcd->ar);
				handled = true;
				break;
			case PADMINUS:
			case PAGEDOWNKEY:
			case WHEELDOWNMOUSE:  /* change number of cuts */
				if (event->val == KM_RELEASE)
					break;

				if (event->alt == 0) {
					cuts = max_ii(cuts - 1, 1);
					RNA_int_set(op->ptr, "number_cuts", cuts);
					ringsel_find_edge(lcd, cuts);
					show_cuts = true;
				}
				else {
					smoothness = max_ff(smoothness - 0.05f, -SUBD_SMOOTH_MAX);
					RNA_float_set(op->ptr, "smoothness", smoothness);
					show_cuts = true;
				}
				
				ED_region_tag_redraw(lcd->ar);
				handled = true;
				break;
			case MOUSEMOVE:  /* mouse moved somewhere to select another loop */
				if (!has_numinput) {
					lcd->vc.mval[0] = event->mval[0];
					lcd->vc.mval[1] = event->mval[1];
					loopcut_mouse_move(lcd, cuts);

					ED_region_tag_redraw(lcd->ar);
					handled = true;
				}
				break;
		}

		/* Modal numinput inactive, try to handle numeric inputs last... */
		if (!handled && event->val == KM_PRESS && handleNumInput(C, &lcd->num, event)) {
			float values[2] = {(float)cuts, smoothness};
			applyNumInput(&lcd->num, values);

			/* allow zero so you can backspace and type in a value
			 * otherwise 1 as minimum would make more sense */
			cuts = CLAMPIS(values[0], 0, SUBD_CUTS_MAX);
			smoothness = CLAMPIS(values[1], -SUBD_SMOOTH_MAX, SUBD_SMOOTH_MAX);

			RNA_int_set(op->ptr, "number_cuts", cuts);
			ringsel_find_edge(lcd, cuts);
			show_cuts = true;
			RNA_float_set(op->ptr, "smoothness", smoothness);

			ED_region_tag_redraw(lcd->ar);
		}
	}

	if (show_cuts) {
		Scene *sce = CTX_data_scene(C);
		char buf[64 + NUM_STR_REP_LEN * 2];
		char str_rep[NUM_STR_REP_LEN * 2];
		if (hasNumInput(&lcd->num)) {
			outputNumInput(&lcd->num, str_rep, &sce->unit);
		}
		else {
			BLI_snprintf(str_rep, NUM_STR_REP_LEN, "%d", cuts);
			BLI_snprintf(str_rep + NUM_STR_REP_LEN, NUM_STR_REP_LEN, "%.2f", smoothness);
		}
		BLI_snprintf(buf, sizeof(buf), IFACE_("Number of Cuts: %s, Smooth: %s (Alt)"),
		             str_rep, str_rep + NUM_STR_REP_LEN);
		ED_area_headerprint(CTX_wm_area(C), buf);
	}
	
	/* keep going until the user confirms */
	return OPERATOR_RUNNING_MODAL;
}

/* for bmesh this tool is in bmesh_select.c */
#if 0

void MESH_OT_edgering_select(wmOperatorType *ot)
{
	/* description */
	ot->name = "Edge Ring Select";
	ot->idname = "MESH_OT_edgering_select";
	ot->description = "Select an edge ring";
	
	/* callbacks */
	ot->invoke = ringsel_invoke;
	ot->poll = ED_operator_editmesh_region_view3d; 
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
}

#endif

void MESH_OT_loopcut(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* description */
	ot->name = "Loop Cut";
	ot->idname = "MESH_OT_loopcut";
	ot->description = "Add a new loop between existing loops";
	
	/* callbacks */
	ot->invoke = ringcut_invoke;
	ot->exec = loopcut_exec;
	ot->modal = loopcut_modal;
	ot->cancel = ringcut_cancel;
	ot->poll = ED_operator_editmesh_region_view3d;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* properties */
	prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, INT_MAX, "Number of Cuts", "", 1, 100);
	/* avoid re-using last var because it can cause _very_ high poly meshes and annoy users (or worse crash) */
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_float(ot->srna, "smoothness", 0.0f, -FLT_MAX, FLT_MAX,
	                     "Smoothness", "Smoothness factor", -SUBD_SMOOTH_MAX, SUBD_SMOOTH_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_property(ot->srna, "falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, proportional_falloff_curve_only_items);
	RNA_def_property_enum_default(prop, PROP_ROOT);
	RNA_def_property_ui_text(prop, "Falloff", "Falloff type the feather");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */

	prop = RNA_def_int(ot->srna, "edge_index", -1, -1, INT_MAX, "Number of Cuts", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);

#ifdef USE_LOOPSLIDE_HACK
	prop = RNA_def_boolean_array(ot->srna, "mesh_select_mode_init", 3, NULL, "", "");
	RNA_def_property_flag(prop, PROP_HIDDEN);
#endif
}
