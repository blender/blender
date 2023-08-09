/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#include "BLI_compiler_compat.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* -------------------------------------------------------------------- */
/** \name Generic Functions
 * \{ */

void IMB_colormanagement_check_file_config(struct Main *bmain);

void IMB_colormanagement_validate_settings(
    const struct ColorManagedDisplaySettings *display_settings,
    struct ColorManagedViewSettings *view_settings);

const char *IMB_colormanagement_role_colorspace_name_get(int role);
void IMB_colormanagement_check_is_data(struct ImBuf *ibuf, const char *name);
void IMB_colormanagegent_copy_settings(struct ImBuf *ibuf_src, struct ImBuf *ibuf_dst);
void IMB_colormanagement_assign_float_colorspace(struct ImBuf *ibuf, const char *name);
void IMB_colormanagement_assign_byte_colorspace(struct ImBuf *ibuf, const char *name);

const char *IMB_colormanagement_get_float_colorspace(struct ImBuf *ibuf);
const char *IMB_colormanagement_get_rect_colorspace(struct ImBuf *ibuf);

bool IMB_colormanagement_space_is_data(struct ColorSpace *colorspace);
bool IMB_colormanagement_space_is_scene_linear(struct ColorSpace *colorspace);
bool IMB_colormanagement_space_is_srgb(struct ColorSpace *colorspace);
bool IMB_colormanagement_space_name_is_data(const char *name);
bool IMB_colormanagement_space_name_is_scene_linear(const char *name);
bool IMB_colormanagement_space_name_is_srgb(const char *name);

BLI_INLINE void IMB_colormanagement_get_luminance_coefficients(float r_rgb[3]);

/**
 * Convert a float RGB triplet to the correct luminance weighted average.
 *
 * Gray-scale, or Luma is a distillation of RGB data values down to a weighted average
 * based on the luminance positions of the red, green, and blue primaries.
 * Given that the internal reference space may be arbitrarily set, any
 * effort to glean the luminance coefficients must be aware of the reference
 * space primaries.
 *
 * See http://wiki.blender.org/index.php/User:Nazg-gul/ColorManagement#Luminance
 */
BLI_INLINE float IMB_colormanagement_get_luminance(const float rgb[3]);
/**
 * Byte equivalent of #IMB_colormanagement_get_luminance().
 */
BLI_INLINE unsigned char IMB_colormanagement_get_luminance_byte(const unsigned char[3]);

/**
 * Conversion between scene linear and other color spaces.
 */
BLI_INLINE void IMB_colormanagement_xyz_to_scene_linear(float scene_linear[3], const float xyz[3]);
BLI_INLINE void IMB_colormanagement_scene_linear_to_xyz(float xyz[3], const float scene_linear[3]);
BLI_INLINE void IMB_colormanagement_rec709_to_scene_linear(float scene_linear[3],
                                                           const float rec709[3]);
BLI_INLINE void IMB_colormanagement_scene_linear_to_rec709(float rec709[3],
                                                           const float scene_linear[3]);
BLI_INLINE void IMB_colormanagement_aces_to_scene_linear(float scene_linear[3],
                                                         const float aces[3]);
BLI_INLINE void IMB_colormanagement_scene_linear_to_aces(float aces[3],
                                                         const float scene_linear[3]);
const float *IMB_colormanagement_get_xyz_to_scene_linear(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Space Transformation Functions
 * \{ */

/**
 * Convert the whole buffer from specified by name color space to another.
 */
void IMB_colormanagement_transform(float *buffer,
                                   int width,
                                   int height,
                                   int channels,
                                   const char *from_colorspace,
                                   const char *to_colorspace,
                                   bool predivide);
/**
 * Convert the whole buffer from specified by name color space to another
 * will do threaded conversion.
 */
void IMB_colormanagement_transform_threaded(float *buffer,
                                            int width,
                                            int height,
                                            int channels,
                                            const char *from_colorspace,
                                            const char *to_colorspace,
                                            bool predivide);
/**
 * Similar to #IMB_colormanagement_transform_threaded, but operates on byte buffer.
 */
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
/**
 * Similar to #IMB_colormanagement_transform_byte_threaded, but gets float buffer from display one.
 */
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

/**
 * Convert pixel from specified by descriptor color space to scene linear
 * used by performance-critical areas such as renderer and baker.
 */
void IMB_colormanagement_colorspace_to_scene_linear_v3(float pixel[3],
                                                       struct ColorSpace *colorspace);
void IMB_colormanagement_colorspace_to_scene_linear_v4(float pixel[4],
                                                       bool predivide,
                                                       struct ColorSpace *colorspace);

/**
 * Same as #IMB_colormanagement_colorspace_to_scene_linear_v4,
 * but converts colors in opposite direction.
 */
void IMB_colormanagement_scene_linear_to_colorspace_v3(float pixel[3],
                                                       struct ColorSpace *colorspace);

void IMB_colormanagement_colorspace_to_scene_linear(float *buffer,
                                                    int width,
                                                    int height,
                                                    int channels,
                                                    struct ColorSpace *colorspace,
                                                    bool predivide);

void IMB_colormanagement_imbuf_to_byte_texture(unsigned char *out_buffer,
                                               int x,
                                               int y,
                                               int width,
                                               int height,
                                               const struct ImBuf *ibuf,
                                               bool store_premultiplied);
void IMB_colormanagement_imbuf_to_float_texture(float *out_buffer,
                                                int offset_x,
                                                int offset_y,
                                                int width,
                                                int height,
                                                const struct ImBuf *ibuf,
                                                bool store_premultiplied);

/**
 * Conversion between color picking role. Typically we would expect such a
 * requirements:
 * - It is approximately perceptually linear, so that the HSV numbers and
 *   the HSV cube/circle have an intuitive distribution.
 * - It has the same gamut as the scene linear color space.
 * - Color picking values 0..1 map to scene linear values in the 0..1 range,
 *   so that picked albedo values are energy conserving.
 */
void IMB_colormanagement_scene_linear_to_color_picking_v3(float color_picking[3],
                                                          const float scene_linear[3]);
void IMB_colormanagement_color_picking_to_scene_linear_v3(float scene_linear[3],
                                                          const float color_picking[3]);

/**
 * Conversion between sRGB, for rare cases like hex color or copy/pasting
 * between UI theme and scene linear colors.
 */
BLI_INLINE void IMB_colormanagement_scene_linear_to_srgb_v3(float srgb[3],
                                                            const float scene_linear[3]);
BLI_INLINE void IMB_colormanagement_srgb_to_scene_linear_v3(float scene_linear[3],
                                                            const float srgb[3]);

/**
 * Convert pixel from scene linear to display space using default view
 * used by performance-critical areas such as color-related widgets where we want to reduce
 * amount of per-widget allocations.
 */
void IMB_colormanagement_scene_linear_to_display_v3(float pixel[3],
                                                    struct ColorManagedDisplay *display);
/**
 * Same as #IMB_colormanagement_scene_linear_to_display_v3,
 * but converts color in opposite direction.
 */
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

/**
 * Prepare image buffer to be saved on disk, applying color management if needed
 * color management would be applied if image is saving as render result and if
 * file format is not expecting float buffer to be in linear space (currently
 * JPEG2000 and TIFF are such formats -- they're storing image as float but
 * file itself stores applied color space).
 *
 * Both byte and float buffers would contain applied color space, and result's
 * float_colorspace would be set to display color space. This should be checked
 * in image format write callback and if float_colorspace is not NULL, no color
 * space transformation should be applied on this buffer.
 */
struct ImBuf *IMB_colormanagement_imbuf_for_write(struct ImBuf *ibuf,
                                                  bool save_as_render,
                                                  bool allocate_result,
                                                  const struct ImageFormatData *image_format);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Display Buffers Interfaces
 * \{ */

void IMB_colormanagement_display_settings_from_ctx(
    const struct bContext *C,
    struct ColorManagedViewSettings **r_view_settings,
    struct ColorManagedDisplaySettings **r_display_settings);

/**
 * Acquire display buffer for given image buffer using specified view and display settings.
 */
unsigned char *IMB_display_buffer_acquire(
    struct ImBuf *ibuf,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    void **cache_handle);
/**
 * Same as #IMB_display_buffer_acquire but gets view and display settings from context.
 */
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
void IMB_display_buffer_transform_apply_float(
    float *float_display_buffer,
    float *linear_buffer,
    int width,
    int height,
    int channels,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    bool predivide);

void IMB_display_buffer_release(void *cache_handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display Functions
 * \{ */

int IMB_colormanagement_display_get_named_index(const char *name);
const char *IMB_colormanagement_display_get_indexed_name(int index);
const char *IMB_colormanagement_display_get_default_name(void);
/**
 * Used by performance-critical pixel processing areas, such as color widgets.
 */
struct ColorManagedDisplay *IMB_colormanagement_display_get_named(const char *name);
const char *IMB_colormanagement_display_get_none_name(void);
const char *IMB_colormanagement_display_get_default_view_transform_name(
    struct ColorManagedDisplay *display);

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Functions
 * \{ */

int IMB_colormanagement_view_get_named_index(const char *name);
const char *IMB_colormanagement_view_get_indexed_name(int index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Look Functions
 * \{ */

int IMB_colormanagement_look_get_named_index(const char *name);
const char *IMB_colormanagement_look_get_indexed_name(int index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Space Functions
 * \{ */

int IMB_colormanagement_colorspace_get_named_index(const char *name);
const char *IMB_colormanagement_colorspace_get_indexed_name(int index);
const char *IMB_colormanagement_colorspace_get_name(const struct ColorSpace *colorspace);
const char *IMB_colormanagement_view_get_default_name(const char *display_name);
const char *IMB_colormanagement_view_get_raw_or_default_name(const char *display_name);

void IMB_colormanagement_colorspace_from_ibuf_ftype(
    struct ColorManagedColorspaceSettings *colorspace_settings, struct ImBuf *ibuf);

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Helper Functions
 * \{ */

void IMB_colormanagement_display_items_add(struct EnumPropertyItem **items, int *totitem);
void IMB_colormanagement_view_items_add(struct EnumPropertyItem **items,
                                        int *totitem,
                                        const char *display_name);
void IMB_colormanagement_look_items_add(struct EnumPropertyItem **items,
                                        int *totitem,
                                        const char *view_name);
void IMB_colormanagement_colorspace_items_add(struct EnumPropertyItem **items, int *totitem);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tile-based Buffer Management
 * \{ */

void IMB_partial_display_buffer_update(struct ImBuf *ibuf,
                                       const float *linear_buffer,
                                       const unsigned char *byte_buffer,
                                       int stride,
                                       int offset_x,
                                       int offset_y,
                                       const struct ColorManagedViewSettings *view_settings,
                                       const struct ColorManagedDisplaySettings *display_settings,
                                       int xmin,
                                       int ymin,
                                       int xmax,
                                       int ymax);

void IMB_partial_display_buffer_update_threaded(
    struct ImBuf *ibuf,
    const float *linear_buffer,
    const unsigned char *byte_buffer,
    int stride,
    int offset_x,
    int offset_y,
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    int xmin,
    int ymin,
    int xmax,
    int ymax);

void IMB_partial_display_buffer_update_delayed(
    struct ImBuf *ibuf, int xmin, int ymin, int xmax, int ymax);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pixel Processor Functions
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpenGL Drawing Routines Using GLSL for Color Space Transform
 * \{ */

/**
 * Test if GLSL drawing is supported for combination of graphics card and this configuration.
 */
bool IMB_colormanagement_support_glsl_draw(const struct ColorManagedViewSettings *view_settings);
/**
 * Configures GLSL shader for conversion from scene linear to display space.
 */
bool IMB_colormanagement_setup_glsl_draw(
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    float dither,
    bool predivide);
/**
 * \note Same as IMB_colormanagement_setup_glsl_draw,
 * but display space conversion happens from a specified space.
 *
 * Configures GLSL shader for conversion from specified to
 * display color space
 *
 * Will create appropriate OCIO processor and setup GLSL shader,
 * so further 2D texture usage will use this conversion.
 *
 * When there's no need to apply transform on 2D textures, use
 * IMB_colormanagement_finish_glsl_draw().
 *
 * This is low-level function, use ED_draw_imbuf_ctx if you
 * only need to display given image buffer
 */
bool IMB_colormanagement_setup_glsl_draw_from_space(
    const struct ColorManagedViewSettings *view_settings,
    const struct ColorManagedDisplaySettings *display_settings,
    struct ColorSpace *colorspace,
    float dither,
    bool predivide,
    bool do_overlay_merge);
/**
 * Same as setup_glsl_draw, but color management settings are guessing from a given context.
 */
bool IMB_colormanagement_setup_glsl_draw_ctx(const struct bContext *C,
                                             float dither,
                                             bool predivide);
/**
 * Same as `setup_glsl_draw_from_space`,
 * but color management settings are guessing from a given context.
 */
bool IMB_colormanagement_setup_glsl_draw_from_space_ctx(const struct bContext *C,
                                                        struct ColorSpace *colorspace,
                                                        float dither,
                                                        bool predivide);
/**
 * Finish GLSL-based display space conversion.
 */
void IMB_colormanagement_finish_glsl_draw(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Transform
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering Tables
 * \{ */

void IMB_colormanagement_blackbody_temperature_to_rgb_table(float *r_table,
                                                            int width,
                                                            float min,
                                                            float max);
void IMB_colormanagement_wavelength_to_rgb_table(float *r_table, int width);

/** \} */

#ifdef __cplusplus
}
#endif

#include "intern/colormanagement_inline.h"
