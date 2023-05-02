/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

#pragma once

/** \file
 * \ingroup imbuf
 */

#include "BLI_sys_types.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct ColorSpace {
  struct ColorSpace *next, *prev;
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
} ColorSpace;

typedef struct ColorManagedDisplay {
  struct ColorManagedDisplay *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
  ListBase views; /* LinkData.data -> ColorManagedView */

  OCIO_ConstCPUProcessorRcPtr *to_scene_linear;
  OCIO_ConstCPUProcessorRcPtr *from_scene_linear;
} ColorManagedDisplay;

typedef struct ColorManagedView {
  struct ColorManagedView *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
} ColorManagedView;

typedef struct ColorManagedLook {
  struct ColorManagedLook *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
  char ui_name[MAX_COLORSPACE_NAME];
  char view[MAX_COLORSPACE_NAME];
  char process_space[MAX_COLORSPACE_NAME];
  bool is_noop;
} ColorManagedLook;

/* ** Initialization / De-initialization ** */

void colormanagement_init(void);
void colormanagement_exit(void);

void colormanage_cache_free(struct ImBuf *ibuf);

const char *colormanage_display_get_default_name(void);
struct ColorManagedDisplay *colormanage_display_get_default(void);
struct ColorManagedDisplay *colormanage_display_add(const char *name);
struct ColorManagedDisplay *colormanage_display_get_named(const char *name);
struct ColorManagedDisplay *colormanage_display_get_indexed(int index);

const char *colormanage_view_get_default_name(const ColorManagedDisplay *display);
struct ColorManagedView *colormanage_view_get_default(const ColorManagedDisplay *display);
struct ColorManagedView *colormanage_view_add(const char *name);
struct ColorManagedView *colormanage_view_get_indexed(int index);
struct ColorManagedView *colormanage_view_get_named(const char *name);
struct ColorManagedView *colormanage_view_get_named_for_display(const char *display_name,
                                                                const char *name);

struct ColorSpace *colormanage_colorspace_add(const char *name,
                                              const char *description,
                                              bool is_invertible,
                                              bool is_data);
struct ColorSpace *colormanage_colorspace_get_named(const char *name);
struct ColorSpace *colormanage_colorspace_get_roled(int role);
struct ColorSpace *colormanage_colorspace_get_indexed(int index);

struct ColorManagedLook *colormanage_look_add(const char *name,
                                              const char *process_space,
                                              bool is_noop);
struct ColorManagedLook *colormanage_look_get_named(const char *name);
struct ColorManagedLook *colormanage_look_get_indexed(int index);

void colorspace_set_default_role(char *colorspace, int size, int role);

void colormanage_imbuf_set_default_spaces(struct ImBuf *ibuf);
void colormanage_imbuf_make_linear(struct ImBuf *ibuf, const char *from_colorspace);

#ifdef __cplusplus
}
#endif
