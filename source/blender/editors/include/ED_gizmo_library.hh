/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Generic Gizmos.
 *
 * This is exposes predefined gizmos for re-use.
 */

#pragma once

#include "DNA_scene_types.h"

/* initialize gizmos */
void ED_gizmotypes_arrow_3d(void);
void ED_gizmotypes_button_2d(void);
void ED_gizmotypes_cage_2d(void);
void ED_gizmotypes_cage_3d(void);
void ED_gizmotypes_dial_3d(void);
void ED_gizmotypes_move_3d(void);
void ED_gizmotypes_facemap_3d(void);
void ED_gizmotypes_preselect_3d(void);
void ED_gizmotypes_primitive_3d(void);
void ED_gizmotypes_blank_3d(void);
void ED_gizmotypes_snap_3d(void);

struct Object;
struct bContext;
struct wmGizmo;

/* -------------------------------------------------------------------- */
/* Shape Presets
 *
 * Intended to be called by custom draw functions.
 */

/* `gizmo_library_presets.cc` */

void ED_gizmo_draw_preset_box(const struct wmGizmo *gz, const float mat[4][4], int select_id);
void ED_gizmo_draw_preset_arrow(const struct wmGizmo *gz,
                                const float mat[4][4],
                                int axis,
                                int select_id);
void ED_gizmo_draw_preset_circle(const struct wmGizmo *gz,
                                 const float mat[4][4],
                                 int axis,
                                 int select_id);

/* -------------------------------------------------------------------- */
/* 3D Arrow Gizmo */

enum {
  ED_GIZMO_ARROW_STYLE_NORMAL = 0,
  ED_GIZMO_ARROW_STYLE_CROSS = 1,
  ED_GIZMO_ARROW_STYLE_BOX = 2,
  ED_GIZMO_ARROW_STYLE_CONE = 3,
  ED_GIZMO_ARROW_STYLE_PLANE = 4,
};

/* transform */
enum {
  /* inverted offset during interaction - if set it also sets constrained below */
  ED_GIZMO_ARROW_XFORM_FLAG_INVERTED = (1 << 3),
  /* clamp arrow interaction to property width */
  ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED = (1 << 4),
};

/* draw_options */
enum {
  /* Show arrow stem. */
  ED_GIZMO_ARROW_DRAW_FLAG_STEM = (1 << 0),
  ED_GIZMO_ARROW_DRAW_FLAG_ORIGIN = (1 << 1),
};

/**
 * Define a custom property UI range.
 *
 * \note Needs to be called before #WM_gizmo_target_property_def_rna!
 */
void ED_gizmo_arrow3d_set_ui_range(struct wmGizmo *gz, float min, float max);
/**
 * Define a custom factor for arrow min/max distance.
 *
 * \note Needs to be called before #WM_gizmo_target_property_def_rna!
 */
void ED_gizmo_arrow3d_set_range_fac(struct wmGizmo *gz, float range_fac);

/* -------------------------------------------------------------------- */
/* Cage Gizmo */

enum {
  ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE = (1 << 0),     /* Translates */
  ED_GIZMO_CAGE_XFORM_FLAG_ROTATE = (1 << 1),        /* Rotates */
  ED_GIZMO_CAGE_XFORM_FLAG_SCALE = (1 << 2),         /* Scales */
  ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM = (1 << 3), /* Scales uniformly */
  ED_GIZMO_CAGE_XFORM_FLAG_SCALE_SIGNED = (1 << 4),  /* Negative scale allowed */
};

/* draw_style */
enum {
  /* Display the hover region (edge or corner) of the underlying bounding box. */
  ED_GIZMO_CAGE2D_STYLE_BOX = 0,
  /* Display the bounding box plus dots on four corners while hovering, usually used for
   * transforming a 2D shape. */
  ED_GIZMO_CAGE2D_STYLE_BOX_TRANSFORM,
  /* Display the bounding circle while hovering. */
  ED_GIZMO_CAGE2D_STYLE_CIRCLE,
};

enum {
  ED_GIZMO_CAGE3D_STYLE_BOX = 0,
  /* TODO: rename */
  ED_GIZMO_CAGE3D_STYLE_CIRCLE = 1,
};

/* draw_options */
enum {
  /** Draw a central handle (instead of having the entire area selectable)
   * Needed for large rectangles that we don't want to swallow all events. */
  ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE = (1 << 0),
};

/** #wmGizmo.highlight_part */
enum {
  ED_GIZMO_CAGE2D_PART_TRANSLATE = 0,

  ED_GIZMO_CAGE2D_PART_SCALE,
  /* Edges */
  ED_GIZMO_CAGE2D_PART_SCALE_MIN_X,
  ED_GIZMO_CAGE2D_PART_SCALE_MAX_X,
  ED_GIZMO_CAGE2D_PART_SCALE_MIN_Y,
  ED_GIZMO_CAGE2D_PART_SCALE_MAX_Y,
  /* Corners */
  ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MIN_Y,
  ED_GIZMO_CAGE2D_PART_SCALE_MIN_X_MAX_Y,
  ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MIN_Y,
  ED_GIZMO_CAGE2D_PART_SCALE_MAX_X_MAX_Y,

  ED_GIZMO_CAGE2D_PART_ROTATE,
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
  ED_GIZMO_DIAL_DRAW_FLAG_NOP = 0,
  ED_GIZMO_DIAL_DRAW_FLAG_CLIP = (1 << 0),
  ED_GIZMO_DIAL_DRAW_FLAG_FILL = (1 << 1),
  ED_GIZMO_DIAL_DRAW_FLAG_FILL_SELECT = (1 << 2),
  ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR = (1 << 3),
  ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y = (1 << 4),
  /* Always show the angle value as an arc in the dial. */
  ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE = (1 << 5),
};

/* -------------------------------------------------------------------- */
/* Move Gizmo */

/* draw_options */
enum {
  ED_GIZMO_MOVE_DRAW_FLAG_NOP = 0,
  /* only for solid shapes */
  ED_GIZMO_MOVE_DRAW_FLAG_FILL = (1 << 0),
  ED_GIZMO_MOVE_DRAW_FLAG_FILL_SELECT = (1 << 1),
  ED_GIZMO_MOVE_DRAW_FLAG_ALIGN_VIEW = (1 << 2),
};

enum {
  ED_GIZMO_MOVE_STYLE_RING_2D = 0,
  ED_GIZMO_MOVE_STYLE_CROSS_2D = 1,
};

/* -------------------------------------------------------------------- */
/* Button Gizmo */

enum {
  ED_GIZMO_BUTTON_SHOW_OUTLINE = (1 << 0),
  ED_GIZMO_BUTTON_SHOW_BACKDROP = (1 << 1),
  /**
   * Draw a line from the origin to the offset (similar to an arrow)
   * sometimes needed to show what the button edits.
   */
  ED_GIZMO_BUTTON_SHOW_HELPLINE = (1 << 2),
};

/* -------------------------------------------------------------------- */
/* Primitive Gizmo */

enum {
  ED_GIZMO_PRIMITIVE_STYLE_PLANE = 0,
  ED_GIZMO_PRIMITIVE_STYLE_CIRCLE,
  ED_GIZMO_PRIMITIVE_STYLE_ANNULUS,
};

/* -------------------------------------------------------------------- */
/* Specific gizmos utils */

/* `snap3d_gizmo.cc` */

struct SnapObjectContext *ED_gizmotypes_snap_3d_context_ensure(struct Scene *scene,
                                                               struct wmGizmo *gz);

void ED_gizmotypes_snap_3d_flag_set(struct wmGizmo *gz, int flag);

bool ED_gizmotypes_snap_3d_is_enabled(const struct wmGizmo *gz);

void ED_gizmotypes_snap_3d_data_get(const struct bContext *C,
                                    struct wmGizmo *gz,
                                    float r_loc[3],
                                    float r_nor[3],
                                    int r_elem_index[3],
                                    eSnapMode *r_snap_elem);
