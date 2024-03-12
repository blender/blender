/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "BLI_sys_types.h"
#include "DNA_listBase.h"

struct ImBuf;
struct OCIO_ConstCPUProcessorRc;
typedef struct OCIO_ConstCPUProcessorRc *OCIO_ConstCPUProcessorRcPtr;

extern float imbuf_luma_coefficients[3];
extern float imbuf_scene_linear_to_xyz[3][3];
extern float imbuf_xyz_to_scene_linear[3][3];
extern float imbuf_scene_linear_to_aces[3][3];
extern float imbuf_aces_to_scene_linear[3][3];
extern float imbuf_scene_linear_to_rec709[3][3];
extern float imbuf_rec709_to_scene_linear[3][3];

#define MAX_COLORSPACE_NAME 64
#define MAX_COLORSPACE_DESCRIPTION 512

struct ColorSpace {
  ColorSpace *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
  char description[MAX_COLORSPACE_DESCRIPTION];

  OCIO_ConstCPUProcessorRcPtr *to_scene_linear;
  OCIO_ConstCPUProcessorRcPtr *from_scene_linear;

  char (*aliases)[MAX_COLORSPACE_NAME];
  int num_aliases;

  bool is_invertible;
  bool is_data;

  /* Additional info computed only when needed since it's not cheap. */
  struct {
    bool cached;
    bool is_srgb;
    bool is_scene_linear;
  } info;
};

struct ColorManagedDisplay {
  ColorManagedDisplay *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
  ListBase views; /* LinkData.data -> ColorManagedView */

  OCIO_ConstCPUProcessorRcPtr *to_scene_linear;
  OCIO_ConstCPUProcessorRcPtr *from_scene_linear;
};

struct ColorManagedView {
  ColorManagedView *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
};

struct ColorManagedLook {
  ColorManagedLook *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
  char ui_name[MAX_COLORSPACE_NAME];
  char view[MAX_COLORSPACE_NAME];
  char process_space[MAX_COLORSPACE_NAME];
  bool is_noop;
};

/* ** Initialization / De-initialization ** */

void colormanagement_init();
void colormanagement_exit();

void colormanage_cache_free(ImBuf *ibuf);

const char *colormanage_display_get_default_name();
ColorManagedDisplay *colormanage_display_get_default();
ColorManagedDisplay *colormanage_display_add(const char *name);
ColorManagedDisplay *colormanage_display_get_named(const char *name);
ColorManagedDisplay *colormanage_display_get_indexed(int index);

const char *colormanage_view_get_default_name(const ColorManagedDisplay *display);
ColorManagedView *colormanage_view_get_default(const ColorManagedDisplay *display);
ColorManagedView *colormanage_view_add(const char *name);
ColorManagedView *colormanage_view_get_indexed(int index);
ColorManagedView *colormanage_view_get_named(const char *name);
ColorManagedView *colormanage_view_get_named_for_display(const char *display_name,
                                                         const char *name);

ColorSpace *colormanage_colorspace_add(const char *name,
                                       const char *description,
                                       bool is_invertible,
                                       bool is_data);
ColorSpace *colormanage_colorspace_get_named(const char *name);
ColorSpace *colormanage_colorspace_get_roled(int role);
ColorSpace *colormanage_colorspace_get_indexed(int index);

ColorManagedLook *colormanage_look_add(const char *name, const char *process_space, bool is_noop);
ColorManagedLook *colormanage_look_get_named(const char *name);
ColorManagedLook *colormanage_look_get_indexed(int index);

void colorspace_set_default_role(char *colorspace, int size, int role);

void colormanage_imbuf_set_default_spaces(ImBuf *ibuf);
void colormanage_imbuf_make_linear(ImBuf *ibuf, const char *from_colorspace);
