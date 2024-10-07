/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#pragma once

#include "DNA_vec_types.h"

#include "ED_numinput.hh"

#define DEPTH_INVALID 1.0f

/* internal exports only */
struct Material;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;
struct tGPspoint;

struct GHash;
struct RNG;

struct ARegion;
struct Brush;
struct Scene;
struct View2D;
struct View3D;
struct ViewDepths;
struct wmOperatorType;
struct wmWindow;

struct Depsgraph;

struct EnumPropertyItem;
struct PointerRNA;
struct PropertyRNA;

/* ***************************************************** */
/* Internal API */

/* Stroke Coordinates API ------------------------------ */
/* `gpencil_utils.cc` */

struct GP_SpaceConversion {
  Scene *scene;
  Object *ob;
  bGPdata *gpd;
  bGPDlayer *gpl;

  ScrArea *area;
  ARegion *region;
  View2D *v2d;

  const rctf *subrect; /* for using the camera rect within the 3d view */
  rctf subrect_data;

  float mat[4][4]; /* transform matrix on the strokes (introduced in [b770964]) */
};

/**
 * Check whether a given stroke segment is inside a circular brush
 *
 * \param mval: The current screen-space coordinates (midpoint) of the brush
 * \param rad: The radius of the brush
 *
 * \param x0, y0: The screen-space x and y coordinates of the start of the stroke segment
 * \param x1, y1: The screen-space x and y coordinates of the end of the stroke segment
 */
bool gpencil_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1);

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screen-space (2D)
 *
 * \param[out] r_x: The screen-space x-coordinate of the point
 * \param[out] r_y: The screen-space y-coordinate of the point
 *
 * \warning This assumes that the caller has already checked
 * whether the stroke in question can be drawn.
 */
void gpencil_point_to_xy(const GP_SpaceConversion *gsc,
                         const bGPDstroke *gps,
                         const bGPDspoint *pt,
                         int *r_x,
                         int *r_y);

/* Copy/Paste Buffer --------------------------------- */
/* `gpencil_edit.cc` */

/**
 * list of #bGPDstroke instances
 *
 * \note is exposed within the editors/gpencil module so that other tools can use it too.
 */
extern ListBase gpencil_strokes_copypastebuf;

/* ***************************************************** */
/* Operator Defines */

/* annotations ------ */

void GPENCIL_OT_annotate(wmOperatorType *ot);
void GPENCIL_OT_annotation_add(wmOperatorType *ot);
void GPENCIL_OT_data_unlink(wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_add(wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_remove(wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_move(wmOperatorType *ot);
void GPENCIL_OT_annotation_active_frame_delete(wmOperatorType *ot);

/* Paint Modes for operator */
enum eGPencil_PaintModes {
  GP_PAINTMODE_DRAW = 0,
  GP_PAINTMODE_ERASER,
  GP_PAINTMODE_DRAW_STRAIGHT,
  GP_PAINTMODE_DRAW_POLY,
  GP_PAINTMODE_SET_CP,
};

/* chunk size for gp-session buffer (the total size is a multiple of this number) */
#define GP_STROKE_BUFFER_CHUNK 2048

/* undo stack ---------- */

void gpencil_undo_init(bGPdata *gpd);
void gpencil_undo_push(bGPdata *gpd);
void gpencil_undo_finish();
