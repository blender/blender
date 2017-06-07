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
void ED_manipulatortypes_facemap_3d(void);
void ED_manipulatortypes_primitive_3d(void);

struct wmManipulatorGroup;

/* -------------------------------------------------------------------- */
/* 3D Arrow Manipulator */

enum {
	ED_MANIPULATOR_ARROW_STYLE_NORMAL        =  1,
	ED_MANIPULATOR_ARROW_STYLE_NO_AXIS       = (1 << 1),
	ED_MANIPULATOR_ARROW_STYLE_CROSS         = (1 << 2),
	/* inverted offset during interaction - if set it also sets constrained below */
	ED_MANIPULATOR_ARROW_STYLE_INVERTED      = (1 << 3),
	/* clamp arrow interaction to property width */
	ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED   = (1 << 4),
	/* use a box for the arrowhead */
	ED_MANIPULATOR_ARROW_STYLE_BOX           = (1 << 5),
	ED_MANIPULATOR_ARROW_STYLE_CONE          = (1 << 6),
};

/* slots for properties */
enum {
	ED_MANIPULATOR_ARROW_SLOT_OFS_WORLD_SPACE = 0
};

struct wmManipulator *ED_manipulator_arrow3d_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style);
void ED_manipulator_arrow3d_set_direction(struct wmManipulator *manipulator, const float direction[3]);
void ED_manipulator_arrow3d_set_up_vector(struct wmManipulator *manipulator, const float direction[3]);
void ED_manipulator_arrow3d_set_line_len(struct wmManipulator *manipulator, const float len);
void ED_manipulator_arrow3d_set_ui_range(struct wmManipulator *manipulator, const float min, const float max);
void ED_manipulator_arrow3d_set_range_fac(struct wmManipulator *manipulator, const float range_fac);
void ED_manipulator_arrow3d_cone_set_aspect(struct wmManipulator *manipulator, const float aspect[2]);


/* -------------------------------------------------------------------- */
/* 2D Arrow Manipulator */

struct wmManipulator *ED_manipulator_arrow2d_new(struct wmManipulatorGroup *mgroup, const char *name);
void ED_manipulator_arrow2d_set_angle(struct wmManipulator *manipulator, const float rot_fac);
void ED_manipulator_arrow2d_set_line_len(struct wmManipulator *manipulator, const float len);


/* -------------------------------------------------------------------- */
/* Cage Manipulator */

enum {
	ED_MANIPULATOR_RECT_TRANSFORM_STYLE_TRANSLATE       =  1,       /* Manipulator translates */
	ED_MANIPULATOR_RECT_TRANSFORM_STYLE_ROTATE          = (1 << 1), /* Manipulator rotates */
	ED_MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE           = (1 << 2), /* Manipulator scales */
	ED_MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM   = (1 << 3), /* Manipulator scales uniformly */
};

enum {
	ED_MANIPULATOR_RECT_TX_SLOT_OFFSET = 0,
	ED_MANIPULATOR_RECT_TX_SLOT_SCALE = 1
};

struct wmManipulator *ED_manipulator_rect_transform_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style);
void ED_manipulator_rect_transform_set_dimensions(
        struct wmManipulator *manipulator, const float width, const float height);


/* -------------------------------------------------------------------- */
/* Dial Manipulator */

enum {
	ED_MANIPULATOR_DIAL_STYLE_RING = 0,
	ED_MANIPULATOR_DIAL_STYLE_RING_CLIPPED = 1,
	ED_MANIPULATOR_DIAL_STYLE_RING_FILLED = 2,
};

struct wmManipulator *ED_manipulator_dial3d_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style);
void ED_manipulator_dial3d_set_up_vector(
        struct wmManipulator *manipulator, const float direction[3]);


/* -------------------------------------------------------------------- */
/* Facemap Manipulator */

struct wmManipulator *ED_manipulator_facemap_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style,
        struct Object *ob, const int facemap);
struct bFaceMap *ED_manipulator_facemap_get_fmap(struct wmManipulator *manipulator);


/* -------------------------------------------------------------------- */
/* Primitive Manipulator */

enum {
	ED_MANIPULATOR_PRIMITIVE_STYLE_PLANE = 0,
};

struct wmManipulator *ED_manipulator_primitive3d_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style);
void ED_manipulator_primitive3d_set_direction(struct wmManipulator *manipulator, const float direction[3]);
void ED_manipulator_primitive3d_set_up_vector(struct wmManipulator *manipulator, const float direction[3]);

#endif  /* __ED_MANIPULATOR_LIBRARY_H__ */
