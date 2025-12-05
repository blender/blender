/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#include "BLI_sys_types.h"

/* Required for #eIconSizes. */
#include "DNA_ID_enums.h"

struct Collection;
struct ID;
struct ImBuf;
struct Object;
struct PointerRNA;
struct PreviewImage;
struct Scene;
struct bContext;

namespace blender::ui {

struct IconTextOverlay {
  char text[5];
  uchar color[4] = {0};
};

#define UI_NO_ICON_OVERLAY_TEXT NULL

#define ICON_DEFAULT_HEIGHT 16
#define ICON_DEFAULT_WIDTH 16

#define ICON_DEFAULT_HEIGHT_TOOLBAR 32

#define ICON_DEFAULT_HEIGHT_SCALE ((int)(UI_UNIT_Y * 0.8f))
#define ICON_DEFAULT_WIDTH_SCALE ((int)(UI_UNIT_X * 0.8f))

#define PREVIEW_DEFAULT_HEIGHT 128

#define PREVIEW_DRAG_DRAW_SIZE 96.0f

enum class AlertIcon : int8_t {
  None = -1,
  Warning = 0,
  Question = 1,
  Error = 2,
  Info = 3,
  Max,
};

ImBuf *icon_alert_imbuf_get(AlertIcon icon, float size);

/**
 * Resizable Icons for Blender
 */
void icons_init();

bool icon_get_theme_color(int icon_id, unsigned char color[4]);

/**
 * Render a #PreviewImage for the data block.
 *
 * Note that if an ID doesn't support jobs for preview creation, \a use_job will be ignored.
 */
void icon_render_id(const bContext *C, Scene *scene, ID *id, enum eIconSizes size, bool use_job);

/**
 * Render the data block into the provided #PreviewImage.
 */
void icon_render_id_ex(const bContext *C,
                       Scene *scene,
                       ID *id_to_render,
                       const enum eIconSizes size,
                       const bool use_job,
                       PreviewImage *r_preview_image);

/**
 * Render size for preview images and icons
 */
int icon_preview_to_render_size(enum eIconSizes size);

/**
 * Draws icon with DPI scale factor.
 */
void icon_draw(float x, float y, int icon_id);
void icon_draw_alpha(float x, float y, int icon_id, float alpha);
void icon_draw_preview(float x, float y, int icon_id, float aspect, float alpha, int size);

void icon_draw_ex(float x,
                  float y,
                  int icon_id,
                  float aspect,
                  float alpha,
                  float desaturate,
                  const uchar mono_color[4],
                  bool mono_border,
                  const IconTextOverlay *text_overlay,
                  const bool inverted = false);

ImBuf *svg_icon_bitmap(uint icon_id, float size, bool multicolor = false);

void icons_free();
void icons_free_drawinfo(void *drawinfo);

PreviewImage *icon_to_preview(int icon_id);

int icon_from_rnaptr(const bContext *C, PointerRNA *ptr, int rnaicon, bool big);
int icon_from_idcode(int idcode);
int icon_from_library(const ID *id);
int icon_from_object_mode(int mode);
int icon_from_object_type(const Object *object);
int icon_color_from_collection(const Collection *collection);

void icon_text_overlay_init_from_count(IconTextOverlay *text_overlay,
                                       const int icon_indicator_number);
}  // namespace blender::ui
