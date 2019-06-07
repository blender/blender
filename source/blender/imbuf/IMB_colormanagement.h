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

#ifndef __IMB_COLORMANAGEMENT_H__
#define __IMB_COLORMANAGEMENT_H__

/** \file
 * \ingroup imbuf
 */

#include "BLI_sys_types.h"
#include "BLI_compiler_compat.h"

#define BCM_CONFIG_FILE "config.ocio"

struct ColorManagedColorspaceSettings;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct ColormanageProcessor;
struct EnumPropertyItem;
struct ImBuf;
struct ImageFormatData;
struct Main;
struct bContext;

struct ColorManagedDisplay;
struct ColorSpace;

/* ** Generic functions ** */

void IMB_colormanagement_check_file_config(struct Main *bmain);

void IMB_colormanagement_validate_settings(
    const struct ColorManagedDisplaySettings *display_settings,
    struct ColorManagedViewSettings *view_settings);

const char *IMB_colormanagement_role_colorspace_name_get(int role);
void IMB_colormanagement_check_is_data(struct ImBuf *ibuf, const char *name);
void IMB_colormanagement_assign_float_colorspace(struct ImBuf *ibuf, const char *name);
void IMB_colormanagement_assign_rect_colorspace(struct ImBuf *ibuf, const char *name);

const char *IMB_colormanagement_get_float_colorspace(struct ImBuf *ibuf);
const char *IMB_colormanagement_get_rect_colorspace(struct ImBuf *ibuf);

bool IMB_colormanagement_space_is_data(struct ColorSpace *colorspace);
bool IMB_colormanagement_space_is_scene_linear(struct ColorSpace *colorspace);
bool IMB_colormanagement_space_is_srgb(struct ColorSpace *colorspace);
bool IMB_colormanagement_space_name_is_data(const char *name);

BLI_INLINE float IMB_colormanagement_get_luminance(const float rgb[3]);
BLI_INLINE unsigned char IMB_colormanagement_get_luminance_byte(const unsigned char[3]);
BLI_INLINE void IMB_colormangement_xyz_to_rgb(float rgb[3], const float xyz[3]);
BLI_INLINE void IMB_colormangement_rgb_to_xyz(float xyz[3], const float rgb[3]);

/* ** Color space transformation functions ** */
void IMB_colormanagement_transform(float *buffer,
                                   int width,
                                   int height,
                                   int channels,
                                   const char *from_colorspace,
                                   const char *to_colorspace,
                                   bool predivide);
void IMB_colormanagement_transform_threaded(float *buffer,
                                            int width,
                                            int height,
                                            int channels,
                                            const char *from_colorspace,
                                            const char *to_colorspace,
                                            bool predivide);
void IMB_colormanagement_transform_byte(unsigned char *buffer,
                                        int width,
                                        int height,
                                        int channels,
                                        const char *from_colorspace,
                                        const char *to_colorspace);
void IMB_colormanagement_transform_byte_threaded(unsigned char *buffer,
                                                 int width,
                                                 int height,
                                                 int channels,
                                                 const char *from_colorspace,
                                                 const char *to_colorspace);
void IMB_colormanagement_transform_from_byte(float *float_buffer,
                                             unsigned char *byte_buffer,
                                             int width,
                                             int height,
                                             int channels,
                                             const char *from_colorspace,
                                             const char *to_colorspace);
void IMB_colormanagement_transform_from_byte_threaded(float *float_buffer,
                                                      unsigned char *byte_buffer,
                                                      int width,
                                                      int height,
                                                      int channels,
                                                      const char *from_colorspace,
                                                      const char *to_colorspace);
void IMB_colormanagement_transform_v4(float pixel[4],
                                      const char *from_colorspace,
                                      const char *to_colorspace);

void IMB_colormanagement_colorspace_to_scene_linear_v3(float pixel[3],
                                                       struct ColorSpace *colorspace);
void IMB_colormanagement_colorspace_to_scene_linear_v4(float pixel[4],
                                                       bool predivide,
                                                       struct ColorSpace *colorspace);

void IMB_colormanagement_scene_linear_to_colorspace_v3(float pixel[3],
                                                       struct ColorSpace *colorspace);

void IMB_colormanagement_colorspace_to_scene_linear(float *buffer,
                                                    int width,
                                                    int height,
                                                    int channels,
                                                    struct ColorSpace *colorspace,
                                                    bool predivide);

void IMB_colormanagement_imbuf_to_byte_texture(unsigned char *out_buffer,
                                               const int x,
                                               const int y,
                                               const int width,
                                               const int height,
                                               const struct ImBuf *ibuf,
                                               const bool compress_as_srgb,
                                               const bool store_premultiplied);
void IMB_colormanagement_imbuf_to_float_texture(float *out_buffer,
                                                const int offset_x,
                                                const int offset_y,
                                                const int width,
                                                const int height,
                                                const struct ImBuf *ibuf,
                                                const bool store_premultiplied);

void IMB_colormanagement_scene_linear_to_color_picking_v3(float pixel[3]);
void IMB_colormanagement_color_picking_to_scene_linear_v3(float pixel[3]);

void IMB_colormanagement_scene_linear_to_srgb_v3(float pixel[3]);
void IMB_colormanagement_srgb_to_scene_linear_v3(float pixel[3]);

void IMB_colormanagement_scene_linear_to_display_v3(float pixel[3],
                                                    struct ColorManagedDisplay *display);
void IMB_colormanagement_display_to_scene_linear_v3(float pixel[3],
                                                    struct ColorManagedDisplay *display);

void IMB_colormanagement_pixel_to_display_space_v4(
    float result[4],
    const float pixel[4],
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);

void IMB_colormanagement_pixel_to_display_space_v3(
    float result[3],
    const float pixel[3],
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);

void IMB_colormanagement_imbuf_make_display_space(
    struct ImBuf *ibuf,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);

struct ImBuf *IMB_colormanagement_imbuf_for_write(
    struct ImBuf *ibuf,
    bool save_as_render,
    bool allocate_result,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    struct ImageFormatData *image_format_data);

void IMB_colormanagement_buffer_make_display_space(
    float *buffer,
    unsigned char *display_buffer,
    int width,
    int height,
    int channels,
    float dither,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);

/* ** Public display buffers interfaces ** */

void IMB_colormanagement_display_settings_from_ctx(
    const struct bContext *C,
    struct ColorManagedViewSettings **view_settings_r,
    struct ColorManagedDisplaySettings **display_settings_r);

const char *IMB_colormanagement_get_display_colorspace_name(
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);

unsigned char *IMB_display_buffer_acquire(
    struct ImBuf *ibuf,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    void **cache_handle);
unsigned char *IMB_display_buffer_acquire_ctx(const struct bContext *C,
                                              struct ImBuf *ibuf,
                                              void **cache_handle);

void IMB_display_buffer_transform_apply(unsigned char *display_buffer,
                                        float *linear_buffer,
                                        int width,
                                        int height,
                                        int channels,
                                        const struct ColorManagedViewSettings *view_settings,
                                        const struct ColorManagedDisplaySettings *display_settings,
                                        bool predivide);

void IMB_display_buffer_release(void *cache_handle);

/* ** Display functions ** */
int IMB_colormanagement_display_get_named_index(const char *name);
const char *IMB_colormanagement_display_get_indexed_name(int index);
const char *IMB_colormanagement_display_get_default_name(void);
struct ColorManagedDisplay *IMB_colormanagement_display_get_named(const char *name);
const char *IMB_colormanagement_display_get_none_name(void);
const char *IMB_colormanagement_display_get_default_view_transform_name(
    struct ColorManagedDisplay *display);

/* ** View functions ** */
int IMB_colormanagement_view_get_named_index(const char *name);
const char *IMB_colormanagement_view_get_indexed_name(int index);

/* ** Look functions ** */
int IMB_colormanagement_look_get_named_index(const char *name);
const char *IMB_colormanagement_look_get_indexed_name(int index);

/* ** Color space functions ** */
int IMB_colormanagement_colorspace_get_named_index(const char *name);
const char *IMB_colormanagement_colorspace_get_indexed_name(int index);
const char *IMB_colormanagement_view_get_default_name(const char *display_name);

void IMB_colormanagement_colorspace_from_ibuf_ftype(
    struct ColorManagedColorspaceSettings *colorspace_settings, struct ImBuf *ibuf);

/* ** RNA helper functions ** */
void IMB_colormanagement_display_items_add(struct EnumPropertyItem **items, int *totitem);
void IMB_colormanagement_view_items_add(struct EnumPropertyItem **items,
                                        int *totitem,
                                        const char *display_name);
void IMB_colormanagement_look_items_add(struct EnumPropertyItem **items,
                                        int *totitem,
                                        const char *view_name);
void IMB_colormanagement_colorspace_items_add(struct EnumPropertyItem **items, int *totitem);

/* ** Tile-based buffer management ** */
void IMB_partial_display_buffer_update(struct ImBuf *ibuf,
                                       const float *linear_buffer,
                                       const unsigned char *buffer_byte,
                                       int stride,
                                       int offset_x,
                                       int offset_y,
                                       const struct ColorManagedViewSettings *view_settings,
                                       const struct ColorManagedDisplaySettings *display_settings,
                                       int xmin,
                                       int ymin,
                                       int xmax,
                                       int ymax,
                                       bool copy_display_to_byte_buffer);

void IMB_partial_display_buffer_update_threaded(
    struct ImBuf *ibuf,
    const float *linear_buffer,
    const unsigned char *buffer_byte,
    int stride,
    int offset_x,
    int offset_y,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    int xmin,
    int ymin,
    int xmax,
    int ymax,
    bool copy_display_to_byte_buffer);

void IMB_partial_display_buffer_update_delayed(
    struct ImBuf *ibuf, int xmin, int ymin, int xmax, int ymax);

/* ** Pixel processor functions ** */
struct ColormanageProcessor *IMB_colormanagement_display_processor_new(
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);
struct ColormanageProcessor *IMB_colormanagement_colorspace_processor_new(
    const char *from_colorspace, const char *to_colorspace);
void IMB_colormanagement_processor_apply_v4(struct ColormanageProcessor *cm_processor,
                                            float pixel[4]);
void IMB_colormanagement_processor_apply_v4_predivide(struct ColormanageProcessor *cm_processor,
                                                      float pixel[4]);
void IMB_colormanagement_processor_apply_v3(struct ColormanageProcessor *cm_processor,
                                            float pixel[3]);
void IMB_colormanagement_processor_apply_pixel(struct ColormanageProcessor *cm_processor,
                                               float *pixel,
                                               int channels);
void IMB_colormanagement_processor_apply(struct ColormanageProcessor *cm_processor,
                                         float *buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         bool predivide);
void IMB_colormanagement_processor_apply_byte(struct ColormanageProcessor *cm_processor,
                                              unsigned char *buffer,
                                              int width,
                                              int height,
                                              int channels);
void IMB_colormanagement_processor_free(struct ColormanageProcessor *cm_processor);

/* ** OpenGL drawing routines using GLSL for color space transform ** */

/* Test if GLSL drawing is supported for combination of graphics card and this configuration */
bool IMB_colormanagement_support_glsl_draw(const struct ColorManagedViewSettings *view_settings);
/* Configures GLSL shader for conversion from scene linear to display space */
bool IMB_colormanagement_setup_glsl_draw(
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    float dither,
    bool predivide);
/* Same as above, but display space conversion happens from a specified space */
bool IMB_colormanagement_setup_glsl_draw_from_space(
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    struct ColorSpace *colorspace,
    float dither,
    bool predivide);
/* Same as setup_glsl_draw, but color management settings are guessing from a given context */
bool IMB_colormanagement_setup_glsl_draw_ctx(const struct bContext *C,
                                             float dither,
                                             bool predivide);
/* Same as setup_glsl_draw_from_space,
 * but color management settings are guessing from a given context. */
bool IMB_colormanagement_setup_glsl_draw_from_space_ctx(const struct bContext *C,
                                                        struct ColorSpace *colorspace,
                                                        float dither,
                                                        bool predivide);
/* Finish GLSL-based display space conversion */
void IMB_colormanagement_finish_glsl_draw(void);

/* ** View transform ** */
void IMB_colormanagement_init_default_view_settings(
    struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings);

/* Roles */
enum {
  COLOR_ROLE_SCENE_LINEAR = 0,
  COLOR_ROLE_COLOR_PICKING,
  COLOR_ROLE_TEXTURE_PAINTING,
  COLOR_ROLE_DEFAULT_SEQUENCER,
  COLOR_ROLE_DEFAULT_BYTE,
  COLOR_ROLE_DEFAULT_FLOAT,
  COLOR_ROLE_DATA,
};

#include "intern/colormanagement_inline.c"

#endif /* __IMB_COLORMANAGEMENT_H__ */
