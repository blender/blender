/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#include "BLI_compiler_compat.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#define BCM_CONFIG_FILE "config.ocio"

struct ColorManagedColorspaceSettings;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct ColormanageProcessor;
struct ID;
struct EnumPropertyItem;
struct ImBuf;
struct ImageFormatData;
struct Main;
struct bContext;

namespace blender::ocio {
class ColorSpace;
class Display;
}  // namespace blender::ocio
using ColorSpace = blender::ocio::ColorSpace;
using ColorManagedDisplay = blender::ocio::Display;

enum ColorManagedDisplaySpace {
  /**
   * Convert to display space for drawing. This will included emulation of the
   * chosen display for an extended sRGB buffer.
   */
  DISPLAY_SPACE_DRAW,
  /**
   * Convert to display space for file output. Note image and video have different
   * conventions for HDR brightness, so there is a distinction.
   */
  DISPLAY_SPACE_IMAGE_OUTPUT,
  DISPLAY_SPACE_VIDEO_OUTPUT,
  /** Convert to display space for inspecting color values as text in the UI. */
  DISPLAY_SPACE_COLOR_INSPECTION,
};

enum class ColorManagedFileOutput { Image, Video };

/* -------------------------------------------------------------------- */
/** \name Generic Functions
 * \{ */

void IMB_colormanagement_check_file_config(Main *bmain);

void IMB_colormanagement_validate_settings(const ColorManagedDisplaySettings *display_settings,
                                           ColorManagedViewSettings *view_settings);

const char *IMB_colormanagement_role_colorspace_name_get(int role);
const char *IMB_colormanagement_srgb_colorspace_name_get();
void IMB_colormanagement_check_is_data(ImBuf *ibuf, const char *name);
void IMB_colormanagegent_copy_settings(ImBuf *ibuf_src, ImBuf *ibuf_dst);
void IMB_colormanagement_assign_float_colorspace(ImBuf *ibuf, const char *name);
void IMB_colormanagement_assign_byte_colorspace(ImBuf *ibuf, const char *name);

const char *IMB_colormanagement_get_float_colorspace(const ImBuf *ibuf);
const char *IMB_colormanagement_get_rect_colorspace(const ImBuf *ibuf);
const char *IMB_colormanagement_space_from_filepath_rules(const char *filepath);

const ColorSpace *IMB_colormanagement_space_get_named(const char *name);
bool IMB_colormanagement_space_is_data(const ColorSpace *colorspace);
bool IMB_colormanagement_space_is_scene_linear(const ColorSpace *colorspace);
bool IMB_colormanagement_space_is_srgb(const ColorSpace *colorspace);
bool IMB_colormanagement_space_name_is_data(const char *name);
bool IMB_colormanagement_space_name_is_scene_linear(const char *name);
bool IMB_colormanagement_space_name_is_srgb(const char *name);

/**
 * Get binary ICC profile contents for a color-space.
 * For describing the color-space for standard dynamic range image files.
 */
blender::Vector<char> IMB_colormanagement_space_to_icc_profile(const ColorSpace *colorspace);
/**
 * Get CICP code for color-space.
 * For describing the color-space of videos and high dynamic range image files.
 */
bool IMB_colormanagement_space_to_cicp(const ColorSpace *colorspace,
                                       const ColorManagedFileOutput output,
                                       const bool rgb_matrix,
                                       int cicp[4]);
const ColorSpace *IMB_colormanagement_space_from_cicp(const int cicp[4],
                                                      const ColorManagedFileOutput output);

/**
 * Get identifier for color-spaces that works with multiple OpenColorIO configurations,
 * as defined by the ASWF Color Interop Forum.
 */
blender::StringRefNull IMB_colormanagement_space_get_interop_id(const ColorSpace *colorspace);
const ColorSpace *IMB_colormanagement_space_from_interop_id(blender::StringRefNull interop_id);

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
BLI_INLINE unsigned char IMB_colormanagement_get_luminance_byte(const unsigned char rgb[3]);

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
BLI_INLINE void IMB_colormanagement_acescg_to_scene_linear(float scene_linear[3],
                                                           const float acescg[3]);
BLI_INLINE void IMB_colormanagement_scene_linear_to_acescg(float acescg[3],
                                                           const float scene_linear[3]);
BLI_INLINE void IMB_colormanagement_rec2020_to_scene_linear(float scene_linear[3],
                                                            const float rec2020[3]);
BLI_INLINE void IMB_colormanagement_scene_linear_to_rec2020(float rec2020[3],
                                                            const float scene_linear[3]);
blender::float3x3 IMB_colormanagement_get_xyz_to_scene_linear();
blender::float3x3 IMB_colormanagement_get_scene_linear_to_xyz();

/**
 * Functions for converting between color temperature/tint and RGB white points.
 */
void IMB_colormanagement_get_whitepoint(const float temperature,
                                        const float tint,
                                        float whitepoint[3]);
bool IMB_colormanagement_set_whitepoint(const float whitepoint[3],
                                        float &temperature,
                                        float &tint);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Space Transformation Functions
 * \{ */

/**
 * Convert a float image buffer from one color space to another.
 */
void IMB_colormanagement_transform_float(float *buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         const char *from_colorspace,
                                         const char *to_colorspace,
                                         bool predivide);
/**
 * Convert a byte image buffer from one color space to another.
 */
void IMB_colormanagement_transform_byte(unsigned char *buffer,
                                        int width,
                                        int height,
                                        int channels,
                                        const char *from_colorspace,
                                        const char *to_colorspace);

/**
 * Convert a byte image buffer into a float buffer, changing the color spaces too.
 */
void IMB_colormanagement_transform_byte_to_float(float *float_buffer,
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
 * Convert pixel from specified color space to scene linear space.
 * For performance, use #IMB_colormanagement_colorspace_to_scene_linear
 * when converting an array of pixels.
 */
void IMB_colormanagement_colorspace_to_scene_linear_v3(float pixel[3],
                                                       const ColorSpace *colorspace);
void IMB_colormanagement_colorspace_to_scene_linear_v4(float pixel[4],
                                                       bool predivide,
                                                       const ColorSpace *colorspace);

/**
 * Convert pixel from scene linear space to specified color space.
 * For performance, use #IMB_colormanagement_scene_linear_to_colorspace
 * when converting an array of pixels.
 */
void IMB_colormanagement_scene_linear_to_colorspace_v3(float pixel[3],
                                                       const ColorSpace *colorspace);

/**
 * Converts a (width)x(height) block of float pixels from given color space to
 * scene linear space. This is much higher performance than converting pixels
 * one by one.
 */
void IMB_colormanagement_colorspace_to_scene_linear(float *buffer,
                                                    int width,
                                                    int height,
                                                    int channels,
                                                    const ColorSpace *colorspace,
                                                    bool predivide);

/**
 * Converts a (width)x(height) block of float pixels from scene linear space
 * to given color space. This is much higher performance than converting pixels
 * one by one.
 */
void IMB_colormanagement_scene_linear_to_colorspace(
    float *buffer, int width, int height, int channels, const ColorSpace *colorspace);

void IMB_colormanagement_imbuf_to_byte_texture(unsigned char *out_buffer,
                                               int offset_x,
                                               int offset_y,
                                               int width,
                                               int height,
                                               const ImBuf *ibuf,
                                               bool store_premultiplied);
void IMB_colormanagement_imbuf_to_float_texture(float *out_buffer,
                                                int offset_x,
                                                int offset_y,
                                                int width,
                                                int height,
                                                const ImBuf *ibuf,
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
void IMB_colormanagement_scene_linear_to_display_v3(
    float pixel[3],
    const ColorManagedDisplay *display,
    const ColorManagedDisplaySpace display_space = DISPLAY_SPACE_DRAW);
/**
 * Same as #IMB_colormanagement_scene_linear_to_display_v3,
 * but converts color in opposite direction.
 */
void IMB_colormanagement_display_to_scene_linear_v3(
    float pixel[3],
    const ColorManagedDisplay *display,
    const ColorManagedDisplaySpace display_space = DISPLAY_SPACE_DRAW);

void IMB_colormanagement_pixel_to_display_space_v4(
    float result[4],
    const float pixel[4],
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space = DISPLAY_SPACE_DRAW);

void IMB_colormanagement_imbuf_make_display_space(
    ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space = DISPLAY_SPACE_DRAW);

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
ImBuf *IMB_colormanagement_imbuf_for_write(ImBuf *ibuf,
                                           bool save_as_render,
                                           bool allocate_result,
                                           const ImageFormatData *image_format);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Display Buffers Interfaces
 * \{ */

void IMB_colormanagement_display_settings_from_ctx(
    const bContext *C,
    ColorManagedViewSettings **r_view_settings,
    ColorManagedDisplaySettings **r_display_settings);

/**
 * Acquire display buffer for given image buffer using specified view and display settings.
 */
unsigned char *IMB_display_buffer_acquire(ImBuf *ibuf,
                                          const ColorManagedViewSettings *view_settings,
                                          const ColorManagedDisplaySettings *display_settings,
                                          void **cache_handle);
/**
 * Same as #IMB_display_buffer_acquire but gets view and display settings from context.
 */
unsigned char *IMB_display_buffer_acquire_ctx(const bContext *C, ImBuf *ibuf, void **cache_handle);

void IMB_display_buffer_transform_apply(unsigned char *display_buffer,
                                        float *linear_buffer,
                                        int width,
                                        int height,
                                        int channels,
                                        const ColorManagedViewSettings *view_settings,
                                        const ColorManagedDisplaySettings *display_settings,
                                        bool predivide);

void IMB_display_buffer_release(void *cache_handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display Functions
 * \{ */

int IMB_colormanagement_display_get_named_index(const char *name);
const char *IMB_colormanagement_display_get_indexed_name(int index);
const char *IMB_colormanagement_display_get_default_name();
/**
 * Used by performance-critical pixel processing areas, such as color widgets.
 */
const ColorManagedDisplay *IMB_colormanagement_display_get_named(const char *name);
const char *IMB_colormanagement_display_get_none_name();
const char *IMB_colormanagement_display_get_default_view_transform_name(
    const ColorManagedDisplay *display);

const ColorSpace *IMB_colormangement_display_get_color_space(
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings);
bool IMB_colormanagement_display_is_hdr(const ColorManagedDisplaySettings *display_settings,
                                        const char *view_name);
bool IMB_colormanagement_display_is_wide_gamut(const ColorManagedDisplaySettings *display_settings,
                                               const char *view_name);
bool IMB_colormanagement_display_support_emulation(
    const ColorManagedDisplaySettings *display_settings, const char *view_name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Functions
 * \{ */

int IMB_colormanagement_view_get_id_by_name(const char *name);
const char *IMB_colormanagement_view_get_name_by_id(int index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Look Functions
 * \{ */

int IMB_colormanagement_look_get_named_index(const char *name);
const char *IMB_colormanagement_look_get_indexed_name(int index);
const char *IMB_colormanagement_look_get_default_name();
const char *IMB_colormanagement_look_validate_for_view(const char *view_name,
                                                       const char *look_name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Space Functions
 * \{ */

int IMB_colormanagement_colorspace_get_named_index(const char *name);
const char *IMB_colormanagement_colorspace_get_indexed_name(int index);
const char *IMB_colormanagement_colorspace_get_name(const ColorSpace *colorspace);
const char *IMB_colormanagement_view_get_default_name(const char *display_name);
const char *IMB_colormanagement_view_get_raw_or_default_name(const char *display_name);

void IMB_colormanagement_colorspace_from_ibuf_ftype(
    ColorManagedColorspaceSettings *colorspace_settings, ImBuf *ibuf);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Working Space Functions
 * \{ */

const char *IMB_colormanagement_working_space_get_default();
const char *IMB_colormanagement_working_space_get();

bool IMB_colormanagement_working_space_set_from_name(const char *name);
void IMB_colormanagement_working_space_check(Main *bmain,
                                             const bool for_undo,
                                             const bool have_editable_assets);

void IMB_colormanagement_working_space_init_default(Main *bmain);
void IMB_colormanagement_working_space_init_startup(Main *bmain);
void IMB_colormanagement_working_space_convert(
    Main *bmain,
    const blender::float3x3 &current_scene_linear_to_xyz,
    const blender::float3x3 &new_xyz_to_scene_linear,
    const bool depsgraph_tag = false,
    const bool linked_only = false,
    const bool editable_assets_only = false);
void IMB_colormanagement_working_space_convert(Main *bmain, const Main *reference_bmain);

int IMB_colormanagement_working_space_get_named_index(const char *name);
const char *IMB_colormanagement_working_space_get_indexed_name(int index);
void IMB_colormanagement_working_space_items_add(EnumPropertyItem **items, int *totitem);

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Helper Functions
 * \{ */

void IMB_colormanagement_display_items_add(EnumPropertyItem **items, int *totitem);
void IMB_colormanagement_view_items_add(EnumPropertyItem **items,
                                        int *totitem,
                                        const char *display_name);
void IMB_colormanagement_look_items_add(EnumPropertyItem **items,
                                        int *totitem,
                                        const char *view_name);
void IMB_colormanagement_colorspace_items_add(EnumPropertyItem **items, int *totitem);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tile-based Buffer Management
 * \{ */

void IMB_partial_display_buffer_update(ImBuf *ibuf,
                                       const float *linear_buffer,
                                       const unsigned char *byte_buffer,
                                       int stride,
                                       int offset_x,
                                       int offset_y,
                                       const ColorManagedViewSettings *view_settings,
                                       const ColorManagedDisplaySettings *display_settings,
                                       int xmin,
                                       int ymin,
                                       int xmax,
                                       int ymax);

void IMB_partial_display_buffer_update_threaded(
    ImBuf *ibuf,
    const float *linear_buffer,
    const unsigned char *byte_buffer,
    int stride,
    int offset_x,
    int offset_y,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    int xmin,
    int ymin,
    int xmax,
    int ymax);

void IMB_partial_display_buffer_update_delayed(
    ImBuf *ibuf, int xmin, int ymin, int xmax, int ymax);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pixel Processor Functions
 * \{ */

ColormanageProcessor *IMB_colormanagement_display_processor_new(
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space = DISPLAY_SPACE_DRAW,
    const bool inverse = false);

ColormanageProcessor *IMB_colormanagement_display_processor_for_imbuf(
    const ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space = DISPLAY_SPACE_DRAW);

bool IMB_colormanagement_display_processor_needed(
    const ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings);

ColormanageProcessor *IMB_colormanagement_colorspace_processor_new(const char *from_colorspace,
                                                                   const char *to_colorspace);
bool IMB_colormanagement_processor_is_noop(ColormanageProcessor *cm_processor);
void IMB_colormanagement_processor_apply_v4(ColormanageProcessor *cm_processor, float pixel[4]);
void IMB_colormanagement_processor_apply_v4_predivide(ColormanageProcessor *cm_processor,
                                                      float pixel[4]);
void IMB_colormanagement_processor_apply_v3(ColormanageProcessor *cm_processor, float pixel[3]);
void IMB_colormanagement_processor_apply_pixel(ColormanageProcessor *cm_processor,
                                               float *pixel,
                                               int channels);
void IMB_colormanagement_processor_apply(ColormanageProcessor *cm_processor,
                                         float *buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         bool predivide);
void IMB_colormanagement_processor_apply_byte(ColormanageProcessor *cm_processor,
                                              unsigned char *buffer,
                                              int width,
                                              int height,
                                              int channels);
void IMB_colormanagement_processor_free(ColormanageProcessor *cm_processor);

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpenGL Drawing Routines Using GLSL for Color Space Transform
 * \{ */

/**
 * Configures GLSL shader for conversion from scene linear to display space.
 */
bool IMB_colormanagement_setup_glsl_draw(const ColorManagedViewSettings *view_settings,
                                         const ColorManagedDisplaySettings *display_settings,
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
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorSpace *from_colorspace,
    float dither,
    bool predivide,
    bool do_overlay_merge);
/**
 * Same as setup_glsl_draw, but color management settings are guessing from a given context.
 */
bool IMB_colormanagement_setup_glsl_draw_ctx(const bContext *C, float dither, bool predivide);
/**
 * Same as `setup_glsl_draw_from_space`,
 * but color management settings are guessing from a given context.
 */
bool IMB_colormanagement_setup_glsl_draw_from_space_ctx(const bContext *C,
                                                        const ColorSpace *from_colorspace,
                                                        float dither,
                                                        bool predivide);

/**
 * Configures GPU shader for conversion from the given space to scene linear.
 * Drawing happens in the same immediate mode as when GPU_SHADER_3D_IMAGE_COLOR shader is used.
 *
 * Returns true if the GPU shader was successfully bound.
 */
bool IMB_colormanagement_setup_glsl_draw_to_scene_linear(const char *from_colorspace_name,
                                                         bool predivide);

/**
 * Finish GLSL-based display space conversion.
 */
void IMB_colormanagement_finish_glsl_draw();

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Transform
 * \{ */

void IMB_colormanagement_init_untonemapped_view_settings(
    ColorManagedViewSettings *view_settings, const ColorManagedDisplaySettings *display_settings);

/* Roles */
enum {
  COLOR_ROLE_SCENE_LINEAR = 0,
  COLOR_ROLE_COLOR_PICKING,
  COLOR_ROLE_TEXTURE_PAINTING,
  COLOR_ROLE_DEFAULT_SEQUENCER,
  COLOR_ROLE_DEFAULT_BYTE,
  COLOR_ROLE_DEFAULT_FLOAT,
  COLOR_ROLE_ACES_INTERCHANGE,
  COLOR_ROLE_DATA,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering Tables
 * \{ */

void IMB_colormanagement_blackbody_temperature_to_rgb(float r_dest[4], float value);
void IMB_colormanagement_blackbody_temperature_to_rgb_table(float *r_table,
                                                            int width,
                                                            float min,
                                                            float max);
void IMB_colormanagement_wavelength_to_rgb(float r_dest[4], float value);
void IMB_colormanagement_wavelength_to_rgb_table(float *r_table, int width);

/** \} */

#include "intern/colormanagement_inline.h"  // IWYU pragma: export
