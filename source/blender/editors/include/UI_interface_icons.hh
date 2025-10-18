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

enum eAlertIcon {
  ALERT_ICON_NONE = -1,
  ALERT_ICON_WARNING = 0,
  ALERT_ICON_QUESTION = 1,
  ALERT_ICON_ERROR = 2,
  ALERT_ICON_INFO = 3,
  ALERT_ICON_MAX,
};

ImBuf *UI_icon_alert_imbuf_get(eAlertIcon icon, float size);

/**
 * Resizable Icons for Blender
 */
void UI_icons_init();

bool UI_icon_get_theme_color(int icon_id, unsigned char color[4]);

/**
 * Render a #PreviewImage for the data block.
 *
 * Note that if an ID doesn't support jobs for preview creation, \a use_job will be ignored.
 */
void UI_icon_render_id(
    const bContext *C, Scene *scene, ID *id, enum eIconSizes size, bool use_job);

/**
 * Render the data block into the provided #PreviewImage.
 */
void UI_icon_render_id_ex(const bContext *C,
                          Scene *scene,
                          ID *id_to_render,
                          const enum eIconSizes size,
                          const bool use_job,
                          PreviewImage *r_preview_image);

/**
 * Render size for preview images and icons
 */
int UI_icon_preview_to_render_size(enum eIconSizes size);

/**
 * Draws icon with DPI scale factor.
 */
void UI_icon_draw(float x, float y, int icon_id);
void UI_icon_draw_alpha(float x, float y, int icon_id, float alpha);
void UI_icon_draw_preview(float x, float y, int icon_id, float aspect, float alpha, int size);

void UI_icon_draw_ex(float x,
                     float y,
                     int icon_id,
                     float aspect,
                     float alpha,
                     float desaturate,
                     const uchar mono_color[4],
                     bool mono_border,
                     const IconTextOverlay *text_overlay,
                     const bool inverted = false);

ImBuf *UI_svg_icon_bitmap(uint icon_id, float size, bool multicolor = false);

void UI_icons_free();
void UI_icons_free_drawinfo(void *drawinfo);

PreviewImage *UI_icon_to_preview(int icon_id);

int UI_icon_from_rnaptr(const bContext *C, PointerRNA *ptr, int rnaicon, bool big);
int UI_icon_from_idcode(int idcode);
int UI_icon_from_library(const ID *id);
int UI_icon_from_object_mode(int mode);
int UI_icon_from_object_type(const Object *object);
int UI_icon_color_from_collection(const Collection *collection);

void UI_icon_text_overlay_init_from_count(IconTextOverlay *text_overlay,
                                          const int icon_indicator_number);
