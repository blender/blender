/*
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
 * The Original Code is Copyright (C) 2012 by Blender Foundation.
 * All rights reserved.
 */

#ifndef __IMB_COLORMANAGEMENT_INTERN_H__
#define __IMB_COLORMANAGEMENT_INTERN_H__

/** \file
 * \ingroup imbuf
 */

#include "DNA_listBase.h"
#include "BLI_sys_types.h"

struct ImBuf;
struct OCIO_ConstProcessorRcPtr;

extern float imbuf_luma_coefficients[3];
extern float imbuf_xyz_to_rgb[3][3];
extern float imbuf_rgb_to_xyz[3][3];

#define MAX_COLORSPACE_NAME 64
#define MAX_COLORSPACE_DESCRIPTION 512

typedef struct ColorSpace {
  struct ColorSpace *next, *prev;
  int index;
  char name[MAX_COLORSPACE_NAME];
  char description[MAX_COLORSPACE_DESCRIPTION];

  struct OCIO_ConstProcessorRcPtr *to_scene_linear;
  struct OCIO_ConstProcessorRcPtr *from_scene_linear;

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

  struct OCIO_ConstProcessorRcPtr *to_scene_linear;
  struct OCIO_ConstProcessorRcPtr *from_scene_linear;
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

#endif /* __IMB_COLORMANAGEMENT_INTERN_H__ */
