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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/WM_manipulator_library.h
 *  \ingroup wm
 *
 * \name Generic Manipulator Library
 *
 * Only included in WM_api.h and lower level files.
 */


#ifndef __WM_MANIPULATOR_LIBRARY_H__
#define __WM_MANIPULATOR_LIBRARY_H__

struct wmManipulatorGroup;


/* -------------------------------------------------------------------- */
/* 3D Arrow Manipulator */

enum {
	MANIPULATOR_ARROW_STYLE_NORMAL        =  1,
	MANIPULATOR_ARROW_STYLE_NO_AXIS       = (1 << 1),
	MANIPULATOR_ARROW_STYLE_CROSS         = (1 << 2),
	MANIPULATOR_ARROW_STYLE_INVERTED      = (1 << 3), /* inverted offset during interaction - if set it also sets constrained below */
	MANIPULATOR_ARROW_STYLE_CONSTRAINED   = (1 << 4), /* clamp arrow interaction to property width */
	MANIPULATOR_ARROW_STYLE_BOX           = (1 << 5), /* use a box for the arrowhead */
	MANIPULATOR_ARROW_STYLE_CONE          = (1 << 6),
};

/* slots for properties */
enum {
	ARROW_SLOT_OFFSET_WORLD_SPACE = 0
};

struct wmManipulator *MANIPULATOR_arrow_new(struct wmManipulatorGroup *mgroup, const char *name, const int style);
void MANIPULATOR_arrow_set_direction(struct wmManipulator *manipulator, const float direction[3]);
void MANIPULATOR_arrow_set_up_vector(struct wmManipulator *manipulator, const float direction[3]);
void MANIPULATOR_arrow_set_line_len(struct wmManipulator *manipulator, const float len);
void MANIPULATOR_arrow_set_ui_range(struct wmManipulator *manipulator, const float min, const float max);
void MANIPULATOR_arrow_set_range_fac(struct wmManipulator *manipulator, const float range_fac);
void MANIPULATOR_arrow_cone_set_aspect(struct wmManipulator *manipulator, const float aspect[2]);


/* -------------------------------------------------------------------- */
/* 2D Arrow Manipulator */

struct wmManipulator *MANIPULATOR_arrow2d_new(struct wmManipulatorGroup *mgroup, const char *name);
void MANIPULATOR_arrow2d_set_angle(struct wmManipulator *manipulator, const float rot_fac);
void MANIPULATOR_arrow2d_set_line_len(struct wmManipulator *manipulator, const float len);


/* -------------------------------------------------------------------- */
/* Cage Manipulator */

enum {
	MANIPULATOR_RECT_TRANSFORM_STYLE_TRANSLATE       =  1,       /* Manipulator translates */
	MANIPULATOR_RECT_TRANSFORM_STYLE_ROTATE          = (1 << 1), /* Manipulator rotates */
	MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE           = (1 << 2), /* Manipulator scales */
	MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM   = (1 << 3), /* Manipulator scales uniformly */
};

enum {
	RECT_TRANSFORM_SLOT_OFFSET = 0,
	RECT_TRANSFORM_SLOT_SCALE = 1
};

struct wmManipulator *MANIPULATOR_rect_transform_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style);
void MANIPULATOR_rect_transform_set_dimensions(
        struct wmManipulator *manipulator, const float width, const float height);


/* -------------------------------------------------------------------- */
/* Dial Manipulator */

enum {
	MANIPULATOR_DIAL_STYLE_RING = 0,
	MANIPULATOR_DIAL_STYLE_RING_CLIPPED = 1,
	MANIPULATOR_DIAL_STYLE_RING_FILLED = 2,
};

struct wmManipulator *MANIPULATOR_dial_new(struct wmManipulatorGroup *mgroup, const char *name, const int style);
void MANIPULATOR_dial_set_up_vector(struct wmManipulator *manipulator, const float direction[3]);


/* -------------------------------------------------------------------- */
/* Facemap Manipulator */

struct wmManipulator *MANIPULATOR_facemap_new(
        struct wmManipulatorGroup *mgroup, const char *name, const int style,
        struct Object *ob, const int facemap);
struct bFaceMap *MANIPULATOR_facemap_get_fmap(struct wmManipulator *manipulator);


/* -------------------------------------------------------------------- */
/* Primitive Manipulator */

enum {
	MANIPULATOR_PRIMITIVE_STYLE_PLANE = 0,
};

struct wmManipulator *MANIPULATOR_primitive_new(struct wmManipulatorGroup *mgroup, const char *name, const int style);
void MANIPULATOR_primitive_set_direction(struct wmManipulator *manipulator, const float direction[3]);
void MANIPULATOR_primitive_set_up_vector(struct wmManipulator *manipulator, const float direction[3]);

#endif  /* __WM_MANIPULATOR_LIBRARY_H__ */

