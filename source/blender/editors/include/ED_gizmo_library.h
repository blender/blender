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

/** \file ED_gizmo_library.h
 *  \ingroup wm
 *
 * \name Generic Gizmos.
 *
 * This is exposes pre-defined gizmos for re-use.
 */


#ifndef __ED_GIZMO_LIBRARY_H__
#define __ED_GIZMO_LIBRARY_H__

/* initialize gizmos */
void ED_gizmotypes_arrow_2d(void);
void ED_gizmotypes_arrow_3d(void);
void ED_gizmotypes_button_2d(void);
void ED_gizmotypes_cage_2d(void);
void ED_gizmotypes_cage_3d(void);
void ED_gizmotypes_dial_3d(void);
void ED_gizmotypes_grab_3d(void);
void ED_gizmotypes_facemap_3d(void);
void ED_gizmotypes_primitive_3d(void);
void ED_gizmotypes_blank_3d(void);

struct bContext;
struct Object;
struct Scene;
struct wmGizmo;
struct wmGizmoGroup;


/* -------------------------------------------------------------------- */
/* Shape Presets
 *
 * Intended to be called by custom draw functions.
 */

/* gizmo_library_presets.c */
void ED_gizmo_draw_preset_box(
        const struct wmGizmo *gz, float mat[4][4], int select_id);
void ED_gizmo_draw_preset_arrow(
        const struct wmGizmo *gz, float mat[4][4], int axis, int select_id);
void ED_gizmo_draw_preset_circle(
        const struct wmGizmo *gz, float mat[4][4], int axis, int select_id);
void ED_gizmo_draw_preset_facemap(
        const struct bContext *C, const struct wmGizmo *gz, struct Scene *scene,
        struct Object *ob,  const int facemap, int select_id);


/* -------------------------------------------------------------------- */
/* 3D Arrow Gizmo */

enum {
	ED_GIZMO_ARROW_STYLE_NORMAL        = 0,
	ED_GIZMO_ARROW_STYLE_CROSS         = 1,
	ED_GIZMO_ARROW_STYLE_BOX           = 2,
	ED_GIZMO_ARROW_STYLE_CONE          = 3,
};

/* transform */
enum {
	/* inverted offset during interaction - if set it also sets constrained below */
	ED_GIZMO_ARROW_XFORM_FLAG_INVERTED      = (1 << 3),
	/* clamp arrow interaction to property width */
	ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED   = (1 << 4),
};

/* draw_options */
enum {
	/* Show arrow stem. */
	ED_GIZMO_ARROW_DRAW_FLAG_STEM      = (1 << 0),
};

void ED_gizmo_arrow3d_set_ui_range(struct wmGizmo *gz, const float min, const float max);
void ED_gizmo_arrow3d_set_range_fac(struct wmGizmo *gz, const float range_fac);

/* -------------------------------------------------------------------- */
/* 2D Arrow Gizmo */

/* none */

/* -------------------------------------------------------------------- */
/* Cage Gizmo */

enum {
	ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE        = (1 << 0), /* Translates */
	ED_GIZMO_CAGE2D_XFORM_FLAG_ROTATE           = (1 << 1), /* Rotates */
	ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE            = (1 << 2), /* Scales */
	ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_UNIFORM    = (1 << 3), /* Scales uniformly */
	ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_SIGNED     = (1 << 4), /* Negative scale allowed */
};

/* draw_style */
enum {
	ED_GIZMO_CAGE2D_STYLE_BOX = 0,
	ED_GIZMO_CAGE2D_STYLE_CIRCLE = 1,
};

/* draw_options */
enum {
	/** Draw a central handle (instead of having the entire area selectable)
	 * Needed for large rectangles that we don't want to swallow all events. */
	ED_GIZMO_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE = (1 << 0),
};

/** #wmGizmo.highlight_part */
enum {
	ED_GIZMO_CAGE2D_PART_TRANSLATE     = 0,
	ED_GIZMO_CAGE2D_PART_SCALE_MIN_X   = 1,
	ED_GIZMO_CAGE2D_PART_SCALE_MAX_X   = 2,
	ED_GIZMO_CAGE2D_PART_SCALE_MIN_Y   = 3,
	ED_GIZMO_CAGE2D_PART_SCALE_MAX_Y   = 4,
	/* Corners */
	ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MIN_Y = 5,
	ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MAX_Y = 6,
	ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MIN_Y = 7,
	ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MAX_Y = 8,

	ED_GIZMO_CAGE2D_PART_ROTATE = 9,
};

/** #wmGizmo.highlight_part */
enum {
	/* ordered min/mid/max so we can loop over values (MIN/MID/MAX) on each axis. */
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z = 0,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MID_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MID_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MID_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MAX_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MAX_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MAX_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MIN_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MIN_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MIN_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MID_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MID_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MID_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MAX_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MAX_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MAX_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MIN_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MIN_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MIN_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MID_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MID_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MID_Y_MAX_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MIN_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MID_Z,
	ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z,

	ED_GIZMO_CAGE3D_PART_TRANSLATE,

	ED_GIZMO_CAGE3D_PART_ROTATE,
};

/* -------------------------------------------------------------------- */
/* Dial Gizmo */

/* draw_options */
enum {
	ED_GIZMO_DIAL_DRAW_FLAG_NOP               = 0,
	ED_GIZMO_DIAL_DRAW_FLAG_CLIP              = (1 << 0),
	ED_GIZMO_DIAL_DRAW_FLAG_FILL              = (1 << 1),
	ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR      = (1 << 2),
	ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y     = (1 << 3),
};

/* -------------------------------------------------------------------- */
/* Grab Gizmo */

/* draw_options */
enum {
	ED_GIZMO_GRAB_DRAW_FLAG_NOP               = 0,
	/* only for solid shapes */
	ED_GIZMO_GRAB_DRAW_FLAG_FILL              = (1 << 0),
	ED_GIZMO_GRAB_DRAW_FLAG_ALIGN_VIEW        = (1 << 1),
};

enum {
	ED_GIZMO_GRAB_STYLE_RING_2D = 0,
	ED_GIZMO_GRAB_STYLE_CROSS_2D = 1,
};

/* -------------------------------------------------------------------- */
/* Button Gizmo */

enum {
	ED_GIZMO_BUTTON_SHOW_OUTLINE = (1 << 0),
	/**
	 * Draw a line from the origin to the offset (similar to an arrow)
	 * sometimes needed to show what the button edits.
	 */
	ED_GIZMO_BUTTON_SHOW_HELPLINE = (1 << 1),
};


/* -------------------------------------------------------------------- */
/* Primitive Gizmo */

enum {
	ED_GIZMO_PRIMITIVE_STYLE_PLANE = 0,
};

#endif  /* __ED_GIZMO_LIBRARY_H__ */
