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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_manipulator_library.h
 *  \ingroup wm
 *
 * \name Generic Manipulators.
 *
 * This is exposes pre-defined manipulators for re-use.
 */


#ifndef __ED_MANIPULATOR_LIBRARY_H__
#define __ED_MANIPULATOR_LIBRARY_H__

/* initialize manipulators */
void ED_manipulatortypes_arrow_2d(void);
void ED_manipulatortypes_arrow_3d(void);
void ED_manipulatortypes_cage_2d(void);
void ED_manipulatortypes_dial_3d(void);
void ED_manipulatortypes_grab_3d(void);
void ED_manipulatortypes_facemap_3d(void);
void ED_manipulatortypes_primitive_3d(void);

struct wmManipulator;
struct wmManipulatorGroup;


/* -------------------------------------------------------------------- */
/* Shape Presets
 *
 * Intended to be called by custom draw functions.
 */

/* manipulator_library_presets.c */
void ED_manipulator_draw_preset_box(
        const struct wmManipulator *mpr, float mat[4][4], int select_id);
void ED_manipulator_draw_preset_arrow(
        const struct wmManipulator *mpr, float mat[4][4], int axis, int select_id);
void ED_manipulator_draw_preset_circle(
        const struct wmManipulator *mpr, float mat[4][4], int axis, int select_id);
void ED_manipulator_draw_preset_facemap(
        const struct wmManipulator *mpr, struct Scene *scene, struct Object *ob,  const int facemap, int select_id);


/* -------------------------------------------------------------------- */
/* 3D Arrow Manipulator */

enum {
	ED_MANIPULATOR_ARROW_STYLE_NORMAL        = 0,
	ED_MANIPULATOR_ARROW_STYLE_CROSS         = 1,
	ED_MANIPULATOR_ARROW_STYLE_BOX           = 2,
	ED_MANIPULATOR_ARROW_STYLE_CONE          = 3,
};

enum {
	/* inverted offset during interaction - if set it also sets constrained below */
	ED_MANIPULATOR_ARROW_STYLE_INVERTED      = (1 << 3),
	/* clamp arrow interaction to property width */
	ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED   = (1 << 4),
};

void ED_manipulator_arrow3d_set_ui_range(struct wmManipulator *mpr, const float min, const float max);
void ED_manipulator_arrow3d_set_range_fac(struct wmManipulator *mpr, const float range_fac);

/* -------------------------------------------------------------------- */
/* 2D Arrow Manipulator */

/* none */

/* -------------------------------------------------------------------- */
/* Cage Manipulator */

enum {
	ED_MANIPULATOR_RECT_TRANSFORM_FLAG_TRANSLATE        = (1 << 0), /* Manipulator translates */
	ED_MANIPULATOR_RECT_TRANSFORM_FLAG_ROTATE           = (1 << 1), /* Manipulator rotates */
	ED_MANIPULATOR_RECT_TRANSFORM_FLAG_SCALE            = (1 << 2), /* Manipulator scales */
	ED_MANIPULATOR_RECT_TRANSFORM_FLAG_SCALE_UNIFORM    = (1 << 3), /* Manipulator scales uniformly */
};

/* -------------------------------------------------------------------- */
/* Dial Manipulator */

/* draw_options */
enum {
	ED_MANIPULATOR_DIAL_DRAW_FLAG_NOP               = 0,
	ED_MANIPULATOR_DIAL_DRAW_FLAG_CLIP              = (1 << 0),
	ED_MANIPULATOR_DIAL_DRAW_FLAG_FILL              = (1 << 1),
	ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_MIRROR      = (1 << 2),
	ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_START_Y     = (1 << 3),
};

/* -------------------------------------------------------------------- */
/* Grab Manipulator */

/* draw_options */
enum {
	ED_MANIPULATOR_GRAB_DRAW_FLAG_NOP               = 0,
	ED_MANIPULATOR_GRAB_DRAW_FLAG_FILL              = (1 << 0),
};

enum {
	ED_MANIPULATOR_GRAB_STYLE_RING = 0,
};


/* -------------------------------------------------------------------- */
/* Primitive Manipulator */

enum {
	ED_MANIPULATOR_PRIMITIVE_STYLE_PLANE = 0,
};

#endif  /* __ED_MANIPULATOR_LIBRARY_H__ */
