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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file manipulator_library_intern.h
 *  \ingroup wm
 */

#ifndef __MANIPULATOR_LIBRARY_INTERN_H__
#define __MANIPULATOR_LIBRARY_INTERN_H__

/* distance around which manipulators respond to input (and get highlighted) */
#define MANIPULATOR_HOTSPOT 14.0f

/**
 * Data for common interactions. Used in manipulator_library_utils.c functions.
 */
typedef struct ManipulatorCommonData {
	int flag;

	float range_fac;      /* factor for arrow min/max distance */
	float offset;

	/* property range for constrained manipulators */
	float range;
	/* min/max value for constrained manipulators */
	float min, max;
} ManipulatorCommonData;

typedef struct ManipulatorInteraction {
	float init_value; /* initial property value */
	float init_mval[2];
	float init_offset;
	float init_matrix_final[4][4];
	float init_matrix_basis[4][4];

	/* offset of last handling step */
	float prev_offset;
	/* Total offset added by precision tweaking.
	 * Needed to allow toggling precision on/off without causing jumps */
	float precision_offset;
} ManipulatorInteraction;

/* ManipulatorCommonData->flag  */
enum {
	MANIPULATOR_CUSTOM_RANGE_SET = (1 << 0),
};


float manipulator_offset_from_value(
        ManipulatorCommonData *data, const float value,
        const bool constrained, const bool inverted);
float manipulator_value_from_offset(
        ManipulatorCommonData *data, ManipulatorInteraction *inter, const float offset,
        const bool constrained, const bool inverted, const bool use_precision);

void manipulator_property_data_update(
        struct wmManipulator *mpr, ManipulatorCommonData *data, wmManipulatorProperty *mpr_prop,
        const bool constrained, const bool inverted);

void  manipulator_property_value_reset(
        bContext *C, const struct wmManipulator *mpr, ManipulatorInteraction *inter, wmManipulatorProperty *mpr_prop);


/* -------------------------------------------------------------------- */

void manipulator_color_get(
        const struct wmManipulator *mpr, const bool highlight,
        float r_color[4]);

bool manipulator_window_project_2d(
        bContext *C, const struct wmManipulator *mpr, const float mval[2], int axis, bool use_offset,
        float r_co[2]);

/* -------------------------------------------------------------------- */
/* Manipulator drawing */

#include "manipulator_geometry.h"

void wm_manipulator_geometryinfo_draw(const struct ManipulatorGeomInfo *info, const bool select, const float color[4]);
void wm_manipulator_vec_draw(
        const float color[4], const float (*verts)[3], uint vert_count,
        uint pos, uint primitive_type);


#endif  /* __MANIPULATOR_LIBRARY_INTERN_H__ */

