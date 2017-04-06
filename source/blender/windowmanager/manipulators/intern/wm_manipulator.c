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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/wm_manipulator.c
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_manipulator_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_batch.h"
#include "GPU_glew.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"

#include "manipulator_library/manipulator_geometry.h"

/**
 * Main draw call for ManipulatorGeomInfo data
 */
void wm_manipulator_geometryinfo_draw(const ManipulatorGeomInfo *info, const bool select, const float color[4])
{
	/* TODO store the Batches inside the ManipulatorGeomInfo and updated it when geom changes
	 * So we don't need to re-created and discard it every time */

	const bool use_lighting = true || (!select && ((U.manipulator_flag & V3D_SHADED_MANIPULATORS) != 0));
	VertexBuffer *vbo;
	ElementList *el;
	Batch *batch;
	ElementListBuilder elb = {0};

	VertexFormat format = {0};
	unsigned int pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
	unsigned int nor_id;

	if (use_lighting) {
		nor_id = VertexFormat_add_attrib(&format, "nor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
	}

	/* Elements */
	ElementListBuilder_init(&elb, GL_TRIANGLES, info->ntris, info->nverts);
	for (int i = 0; i < info->ntris; ++i) {
		const unsigned short *idx = &info->indices[i * 3];
		add_triangle_vertices(&elb, idx[0], idx[1], idx[2]);
	}
	el = ElementList_build(&elb);

	vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, info->nverts);

	VertexBuffer_fill_attrib(vbo, pos_id, info->verts);

	if (use_lighting) {
		/* Normals are expected to be smooth. */
		VertexBuffer_fill_attrib(vbo, nor_id, info->normals);
	}

	batch = Batch_create(GL_TRIANGLES, vbo, el);
	Batch_set_builtin_program(batch, GPU_SHADER_3D_UNIFORM_COLOR);

	Batch_Uniform4fv(batch, "color", color);

	glEnable(GL_CULL_FACE);
	// glEnable(GL_DEPTH_TEST);

	Batch_draw(batch);

	glDisable(GL_DEPTH_TEST);
	// glDisable(GL_CULL_FACE);


	Batch_discard_all(batch);
}

/* Still unused */
wmManipulator *WM_manipulator_new(
        void (*draw)(const bContext *C, wmManipulator *customdata),
        void (*render_3d_intersection)(const bContext *C, wmManipulator *customdata, int selectionbase),
        int  (*intersect)(bContext *C, const wmEvent *event, wmManipulator *manipulator),
        int  (*handler)(bContext *C, const wmEvent *event, wmManipulator *manipulator, const int flag))
{
	wmManipulator *manipulator = MEM_callocN(sizeof(wmManipulator), "manipulator");

	manipulator->draw = draw;
	manipulator->handler = handler;
	manipulator->intersect = intersect;
	manipulator->render_3d_intersection = render_3d_intersection;

	/* XXX */
	fix_linking_manipulator_arrow();
	fix_linking_manipulator_arrow2d();
	fix_linking_manipulator_cage();
	fix_linking_manipulator_dial();
	fix_linking_manipulator_primitive();

	return manipulator;
}

/**
 * Assign an idname that is unique in \a mgroup to \a manipulator.
 *
 * \param rawname: Name used as basis to define final unique idname.
 */
static void manipulator_unique_idname_set(wmManipulatorGroup *mgroup, wmManipulator *manipulator, const char *rawname)
{
	if (mgroup->type->idname[0]) {
		BLI_snprintf(manipulator->idname, sizeof(manipulator->idname), "%s_%s", mgroup->type->idname, rawname);
	}
	else {
		BLI_strncpy(manipulator->idname, rawname, sizeof(manipulator->idname));
	}

	/* ensure name is unique, append '.001', '.002', etc if not */
	BLI_uniquename(&mgroup->manipulators, manipulator, "Manipulator", '.',
	               offsetof(wmManipulator, idname), sizeof(manipulator->idname));
}

/**
 * Initialize default values and allocate needed memory for members.
 */
static void manipulator_init(wmManipulator *manipulator)
{
	const float col_default[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	manipulator->user_scale = 1.0f;
	manipulator->line_width = 1.0f;

	/* defaults */
	copy_v4_v4(manipulator->col, col_default);
	copy_v4_v4(manipulator->col_hi, col_default);

	/* create at least one property for interaction */
	if (manipulator->max_prop == 0) {
		manipulator->max_prop = 1;
	}

	manipulator->props = MEM_callocN(sizeof(PropertyRNA *) * manipulator->max_prop, "manipulator->props");
	manipulator->ptr = MEM_callocN(sizeof(PointerRNA) * manipulator->max_prop, "manipulator->ptr");
}

/**
 * Register \a manipulator.
 *
 * \param name: name used to create a unique idname for \a manipulator in \a mgroup
 */
void wm_manipulator_register(wmManipulatorGroup *mgroup, wmManipulator *manipulator, const char *name)
{
	manipulator_init(manipulator);
	manipulator_unique_idname_set(mgroup, manipulator, name);
	wm_manipulatorgroup_manipulator_register(mgroup, manipulator);
}

/**
 * Free \a manipulator and unlink from \a manipulatorlist.
 * \a manipulatorlist is allowed to be NULL.
 */
void WM_manipulator_delete(ListBase *manipulatorlist, wmManipulatorMap *mmap, wmManipulator *manipulator, bContext *C)
{
	if (manipulator->state & WM_MANIPULATOR_HIGHLIGHT) {
		wm_manipulatormap_set_highlighted_manipulator(mmap, C, NULL, 0);
	}
	if (manipulator->state & WM_MANIPULATOR_ACTIVE) {
		wm_manipulatormap_set_active_manipulator(mmap, C, NULL, NULL);
	}
	if (manipulator->state & WM_MANIPULATOR_SELECTED) {
		wm_manipulator_deselect(mmap, manipulator);
	}

	if (manipulator->opptr.data) {
		WM_operator_properties_free(&manipulator->opptr);
	}
	MEM_freeN(manipulator->props);
	MEM_freeN(manipulator->ptr);

	if (manipulatorlist)
		BLI_remlink(manipulatorlist, manipulator);
	MEM_freeN(manipulator);
}

wmManipulatorGroup *wm_manipulator_get_parent_group(const wmManipulator *manipulator)
{
	return manipulator->mgroup;
}


/* -------------------------------------------------------------------- */
/** \name Manipulator Creation API
 *
 * API for defining data on manipulator creation.
 *
 * \{ */

void WM_manipulator_set_property(wmManipulator *manipulator, const int slot, PointerRNA *ptr, const char *propname)
{
	if (slot < 0 || slot >= manipulator->max_prop) {
		fprintf(stderr, "invalid index %d when binding property for manipulator type %s\n", slot, manipulator->idname);
		return;
	}

	/* if manipulator evokes an operator we cannot use it for property manipulation */
	manipulator->opname = NULL;
	manipulator->ptr[slot] = *ptr;
	manipulator->props[slot] = RNA_struct_find_property(ptr, propname);

	if (manipulator->prop_data_update)
		manipulator->prop_data_update(manipulator, slot);
}

PointerRNA *WM_manipulator_set_operator(wmManipulator *manipulator, const char *opname)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);

	if (ot) {
		manipulator->opname = opname;

		if (manipulator->opptr.data) {
			WM_operator_properties_free(&manipulator->opptr);
		}
		WM_operator_properties_create_ptr(&manipulator->opptr, ot);

		return &manipulator->opptr;
	}
	else {
		fprintf(stderr, "Error binding operator to manipulator: operator %s not found!\n", opname);
	}

	return NULL;
}


void WM_manipulator_set_custom_handler(
        struct wmManipulator *manipulator,
        int (*handler)(struct bContext *, const wmEvent *, struct wmManipulator *, const int))
{
	manipulator->handler = handler;
}

/**
 * \brief Set manipulator select callback.
 *
 * Callback is called when manipulator gets selected/deselected.
 */
void WM_manipulator_set_func_select(wmManipulator *manipulator, wmManipulatorSelectFunc select)
{
	BLI_assert(manipulator->mgroup->type->flag & WM_MANIPULATORGROUPTYPE_SELECTABLE);
	manipulator->select = select;
}

void WM_manipulator_set_origin(wmManipulator *manipulator, const float origin[3])
{
	copy_v3_v3(manipulator->origin, origin);
}

void WM_manipulator_set_offset(wmManipulator *manipulator, const float offset[3])
{
	copy_v3_v3(manipulator->offset, offset);
}

void WM_manipulator_set_flag(wmManipulator *manipulator, const int flag, const bool enable)
{
	if (enable) {
		manipulator->flag |= flag;
	}
	else {
		manipulator->flag &= ~flag;
	}
}

void WM_manipulator_set_scale(wmManipulator *manipulator, const float scale)
{
	manipulator->user_scale = scale;
}

void WM_manipulator_set_line_width(wmManipulator *manipulator, const float line_width)
{
	manipulator->line_width = line_width;
}

/**
 * Set manipulator rgba colors.
 *
 * \param col  Normal state color.
 * \param col_hi  Highlighted state color.
 */
void WM_manipulator_set_colors(wmManipulator *manipulator, const float col[4], const float col_hi[4])
{
	copy_v4_v4(manipulator->col, col);
	copy_v4_v4(manipulator->col_hi, col_hi);
}

/** \} */ // Manipulator Creation API


/* -------------------------------------------------------------------- */

/**
 * Remove \a manipulator from selection.
 * Reallocates memory for selected manipulators so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_manipulator_deselect(wmManipulatorMap *mmap, wmManipulator *manipulator)
{
	if (!mmap->mmap_context.selected_manipulator)
		return false;

	wmManipulator ***sel = &mmap->mmap_context.selected_manipulator;
	int *tot_selected = &mmap->mmap_context.tot_selected;
	bool changed = false;

	/* caller should check! */
	BLI_assert(manipulator->state & WM_MANIPULATOR_SELECTED);

	/* remove manipulator from selected_manipulators array */
	for (int i = 0; i < (*tot_selected); i++) {
		if ((*sel)[i] == manipulator) {
			for (int j = i; j < ((*tot_selected) - 1); j++) {
				(*sel)[j] = (*sel)[j + 1];
			}
			changed = true;
			break;
		}
	}

	/* update array data */
	if ((*tot_selected) <= 1) {
		wm_manipulatormap_selected_delete(mmap);
	}
	else {
		*sel = MEM_reallocN(*sel, sizeof(**sel) * (*tot_selected));
		(*tot_selected)--;
	}

	manipulator->state &= ~WM_MANIPULATOR_SELECTED;
	return changed;
}

/**
 * Add \a manipulator to selection.
 * Reallocates memory for selected manipulators so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_manipulator_select(bContext *C, wmManipulatorMap *mmap, wmManipulator *manipulator)
{
	wmManipulator ***sel = &mmap->mmap_context.selected_manipulator;
	int *tot_selected = &mmap->mmap_context.tot_selected;

	if (!manipulator || (manipulator->state & WM_MANIPULATOR_SELECTED))
		return false;

	(*tot_selected)++;

	*sel = MEM_reallocN(*sel, sizeof(wmManipulator *) * (*tot_selected));
	(*sel)[(*tot_selected) - 1] = manipulator;

	manipulator->state |= WM_MANIPULATOR_SELECTED;
	if (manipulator->select) {
		manipulator->select(C, manipulator, SEL_SELECT);
	}
	wm_manipulatormap_set_highlighted_manipulator(mmap, C, manipulator, manipulator->highlighted_part);

	return true;
}

void wm_manipulator_calculate_scale(wmManipulator *manipulator, const bContext *C)
{
	const RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float scale = 1.0f;

	if (manipulator->mgroup->type->flag & WM_MANIPULATORGROUPTYPE_SCALE_3D) {
		if (rv3d /*&& (U.manipulator_flag & V3D_DRAW_MANIPULATOR) == 0*/) { /* UserPref flag might be useful for later */
			if (manipulator->get_final_position) {
				float position[3];

				manipulator->get_final_position(manipulator, position);
				scale = ED_view3d_pixel_size(rv3d, position) * (float)U.manipulator_scale;
			}
			else {
				scale = ED_view3d_pixel_size(rv3d, manipulator->origin) * (float)U.manipulator_scale;
			}
		}
		else {
			scale = U.manipulator_scale * 0.02f;
		}
	}

	manipulator->scale = scale * manipulator->user_scale;
}

static void manipulator_update_prop_data(wmManipulator *manipulator)
{
	/* manipulator property might have been changed, so update manipulator */
	if (manipulator->props && manipulator->prop_data_update) {
		for (int i = 0; i < manipulator->max_prop; i++) {
			if (manipulator->props[i]) {
				manipulator->prop_data_update(manipulator, i);
			}
		}
	}
}

void wm_manipulator_update(wmManipulator *manipulator, const bContext *C, const bool refresh_map)
{
	if (refresh_map) {
		manipulator_update_prop_data(manipulator);
	}
	wm_manipulator_calculate_scale(manipulator, C);
}

bool wm_manipulator_is_visible(wmManipulator *manipulator)
{
	if (manipulator->flag & WM_MANIPULATOR_HIDDEN) {
		return false;
	}
	if ((manipulator->state & WM_MANIPULATOR_ACTIVE) &&
	    !(manipulator->flag & (WM_MANIPULATOR_DRAW_ACTIVE | WM_MANIPULATOR_DRAW_VALUE)))
	{
		/* don't draw while active (while dragging) */
		return false;
	}
	if ((manipulator->flag & WM_MANIPULATOR_DRAW_HOVER) &&
	    !(manipulator->state & WM_MANIPULATOR_HIGHLIGHT) &&
	    !(manipulator->state & WM_MANIPULATOR_SELECTED)) /* still draw selected manipulators */
	{
		/* only draw on mouse hover */
		return false;
	}

	return true;
}

