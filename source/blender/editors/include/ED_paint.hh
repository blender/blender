/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_view3d_enums.h"

enum class PaintMode : int8_t;
struct bContext;
struct bToolRef;
struct Depsgraph;
struct Image;
struct ImageUser;
struct ImBuf;
struct Main;
struct PaintModeSettings;
struct PaintTileMap;
struct ReportList;
struct Scene;
struct UndoStep;
struct UndoType;
struct wmKeyConfig;
struct wmOperator;

/* `paint_ops.cc` */

void ED_operatortypes_paint();
void ED_operatormacros_paint();
void ED_keymap_paint(wmKeyConfig *keyconf);

/* `paint_image.cc` */

void ED_imapaint_clear_partial_redraw();
void ED_imapaint_dirty_region(
    Image *ima, ImBuf *ibuf, ImageUser *iuser, int x, int y, int w, int h, bool find_old);
void ED_imapaint_bucket_fill(bContext *C, float color[3], wmOperator *op, const int mouse[2]);

/* `paint_image_proj.cc` */

void ED_paint_data_warning(
    ReportList *reports, bool has_uvs, bool has_mat, bool has_tex, bool has_stencil);
/**
 * Make sure that active object has a material,
 * and assign UVs and image layers if they do not exist.
 */
bool ED_paint_proj_mesh_data_check(Scene *scene,
                                   Object *ob,
                                   bool *r_has_uvs,
                                   bool *r_has_mat,
                                   bool *r_has_tex,
                                   bool *r_has_stencil);

/* `image_undo.cc` */

/**
 * The caller is responsible for running #ED_image_undo_push_end,
 * failure to do so causes an invalid state for the undo system.
 */
void ED_image_undo_push_begin(const char *name, PaintMode paint_mode);
void ED_image_undo_push_begin_with_image(const char *name,
                                         Image *image,
                                         ImBuf *ibuf,
                                         ImageUser *iuser);

void ED_image_undo_push_end();
/**
 * Restore painting image to previous state. Used for anchored and drag-dot style brushes.
 */
void ED_image_undo_restore(UndoStep *us);

/** Export for ED_undo_sys. */
void ED_image_undosys_type(UndoType *ut);

void *ED_image_paint_tile_find(PaintTileMap *paint_tile_map,
                               Image *image,
                               ImBuf *ibuf,
                               ImageUser *iuser,
                               int x_tile,
                               int y_tile,
                               unsigned short **r_mask,
                               bool validate);
void *ED_image_paint_tile_push(PaintTileMap *paint_tile_map,
                               Image *image,
                               ImBuf *ibuf,
                               ImBuf **tmpibuf,
                               ImageUser *iuser,
                               int x_tile,
                               int y_tile,
                               unsigned short **r_mask,
                               bool **r_valid,
                               bool use_thread_lock,
                               bool find_prev);
void ED_image_paint_tile_lock_init();
void ED_image_paint_tile_lock_end();

PaintTileMap *ED_image_paint_tile_map_get();

#define ED_IMAGE_UNDO_TILE_BITS 6
#define ED_IMAGE_UNDO_TILE_SIZE (1 << ED_IMAGE_UNDO_TILE_BITS)
#define ED_IMAGE_UNDO_TILE_NUMBER(size) \
  (((size) + ED_IMAGE_UNDO_TILE_SIZE - 1) >> ED_IMAGE_UNDO_TILE_BITS)

/* `paint_curve_undo.cc` */

void ED_paintcurve_undo_push_begin(const char *name);
void ED_paintcurve_undo_push_end(bContext *C);

/** Export for ED_undo_sys. */
void ED_paintcurve_undosys_type(UndoType *ut);

/* `paint_canvas.cc` */

/** Color type of an object can be overridden in sculpt/paint mode. */
eV3DShadingColorType ED_paint_shading_color_override(bContext *C,
                                                     const PaintModeSettings *settings,
                                                     Object *ob,
                                                     eV3DShadingColorType orig_color_type);

/**
 * Does the given tool use a paint canvas.
 *
 * When #tref isn't given the active tool from the context is used.
 */
bool ED_paint_tool_use_canvas(bContext *C, bToolRef *tref);

/** Store the last used tool in the sculpt session. */
void ED_paint_tool_update_sticky_shading_color(bContext *C, Object *ob);

void ED_object_vpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void ED_object_vpaintmode_enter(bContext *C, Depsgraph *depsgraph);
void ED_object_wpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void ED_object_wpaintmode_enter(bContext *C, Depsgraph *depsgraph);

void ED_object_vpaintmode_exit_ex(Object *ob);
void ED_object_vpaintmode_exit(bContext *C);
void ED_object_wpaintmode_exit_ex(Object *ob);
void ED_object_wpaintmode_exit(bContext *C);

void ED_object_texture_paint_mode_enter_ex(Main *bmain,
                                           Scene *scene,
                                           Depsgraph *depsgraph,
                                           Object *ob);
void ED_object_texture_paint_mode_enter(bContext *C);

void ED_object_texture_paint_mode_exit_ex(Main *bmain, Scene *scene, Object *ob);
void ED_object_texture_paint_mode_exit(bContext *C);
