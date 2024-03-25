/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

/**
 * \brief IMage Buffer module.
 *
 * This module offers import/export of several graphical file formats.
 * \ingroup imbuf
 *
 * \page IMB ImBuf module external interface
 * \section imb_about About the IMB module
 *
 * External interface of the IMage Buffer module. This module offers
 * import/export of several graphical file formats. It offers the
 * ImBuf type as a common structure to refer to different graphical
 * file formats, and to enable a uniform way of handling them.
 *
 * \section imb_issues Known issues with IMB
 *
 * - imbuf is written in C.
 * - Endianness issues are dealt with internally.
 * - File I/O must be done externally. The module uses FILE*'s to
 *   direct input/output.
 *
 * \section imb_dependencies Dependencies
 *
 * IMB needs:
 * - \ref DNA module
 *     The #ListBase types are used for handling the memory management.
 * - \ref blenlib module
 *     blenlib handles guarded memory management in blender-style.
 *     BLI_winstuff.h makes a few windows specific behaviors
 *     posix-compliant.
 */

#pragma once

#include "../gpu/GPU_texture.hh"

#include "BLI_utildefines.h"

#include "IMB_imbuf_types.hh"

#define IM_MAX_SPACE 64

struct ImBuf;
struct rctf;
struct rcti;

struct ImBufAnim;

struct ColorManagedDisplay;

struct GSet;
struct ImageFormatData;
struct Stereo3dFormat;

void IMB_init();
void IMB_exit();

ImBuf *IMB_ibImageFromMemory(const unsigned char *mem,
                             size_t size,
                             int flags,
                             char colorspace[IM_MAX_SPACE],
                             const char *descr);

ImBuf *IMB_testiffname(const char *filepath, int flags);

ImBuf *IMB_loadiffname(const char *filepath, int flags, char colorspace[IM_MAX_SPACE]);

ImBuf *IMB_thumb_load_image(const char *filepath,
                            const size_t max_thumb_size,
                            char colorspace[IM_MAX_SPACE]);

void IMB_freeImBuf(ImBuf *ibuf);

ImBuf *IMB_allocImBuf(unsigned int x, unsigned int y, unsigned char planes, unsigned int flags);

/**
 * Initialize given ImBuf.
 *
 * Use in cases when temporary image buffer is allocated on stack.
 */
bool IMB_initImBuf(
    ImBuf *ibuf, unsigned int x, unsigned int y, unsigned char planes, unsigned int flags);

/**
 * Create a copy of a pixel buffer and wrap it to a new ImBuf
 * (transferring ownership to the in imbuf).
 */
ImBuf *IMB_allocFromBufferOwn(uint8_t *byte_buffer,
                              float *float_buffer,
                              unsigned int w,
                              unsigned int h,
                              unsigned int channels);

/**
 * Create a copy of a pixel buffer and wrap it to a new ImBuf
 */
ImBuf *IMB_allocFromBuffer(const uint8_t *byte_buffer,
                           const float *float_buffer,
                           unsigned int w,
                           unsigned int h,
                           unsigned int channels);

/**
 * Assign the content of the corresponding buffer with the given data and ownership.
 * The current content of the buffer is released corresponding to its ownership configuration.
 *
 * \note Does not modify the topology (width, height, number of channels)
 * or the mipmaps in any way.
 */
void IMB_assign_byte_buffer(ImBuf *ibuf, uint8_t *buffer_data, ImBufOwnership ownership);
void IMB_assign_float_buffer(ImBuf *ibuf, float *buffer_data, ImBufOwnership ownership);

/**
 * Assign the content and the color space of the corresponding buffer the data from the given
 * buffer.
 *
 * \note Does not modify the topology (width, height, number of channels)
 * or the mipmaps in any way.
 *
 * \note The ownership of the data in the source buffer is ignored.
 */
void IMB_assign_byte_buffer(ImBuf *ibuf, const ImBufByteBuffer &buffer, ImBufOwnership ownership);
void IMB_assign_float_buffer(ImBuf *ibuf,
                             const ImBufFloatBuffer &buffer,
                             ImBufOwnership ownership);
void IMB_assign_dds_data(ImBuf *ibuf, const DDSData &data, ImBufOwnership ownership);

/**
 * Make corresponding buffers available for modification.
 * Is achieved by ensuring that the given ImBuf is the only owner of the underlying buffer data.
 */
void IMB_make_writable_byte_buffer(ImBuf *ibuf);
void IMB_make_writable_float_buffer(ImBuf *ibuf);

/**
 * Steal the buffer data pointer: the ImBuf is no longer an owner of this data.
 * \note If the ImBuf does not own the data the behavior is undefined.
 * \note Stealing encoded buffer resets the encoded size.
 */
uint8_t *IMB_steal_byte_buffer(ImBuf *ibuf);
float *IMB_steal_float_buffer(ImBuf *ibuf);
uint8_t *IMB_steal_encoded_buffer(ImBuf *ibuf);

/**
 * Increase reference count to imbuf
 * (to delete an imbuf you have to call freeImBuf as many times as it
 * is referenced)
 */

void IMB_refImBuf(ImBuf *ibuf);
ImBuf *IMB_makeSingleUser(ImBuf *ibuf);

ImBuf *IMB_dupImBuf(const ImBuf *ibuf1);

/**
 * Approximate size of ImBuf in memory
 */
size_t IMB_get_size_in_memory(ImBuf *ibuf);

/**
 * \brief Get the length of the rect of the given image buffer in terms of pixels.
 *
 * This is the width * the height of the image buffer.
 * This function is preferred over `ibuf->x * ibuf->y` due to overflow issues when
 * working with large resolution images (30kx30k).
 */
size_t IMB_get_rect_len(const ImBuf *ibuf);

enum IMB_BlendMode {
  IMB_BLEND_MIX = 0,
  IMB_BLEND_ADD = 1,
  IMB_BLEND_SUB = 2,
  IMB_BLEND_MUL = 3,
  IMB_BLEND_LIGHTEN = 4,
  IMB_BLEND_DARKEN = 5,
  IMB_BLEND_ERASE_ALPHA = 6,
  IMB_BLEND_ADD_ALPHA = 7,
  IMB_BLEND_OVERLAY = 8,
  IMB_BLEND_HARDLIGHT = 9,
  IMB_BLEND_COLORBURN = 10,
  IMB_BLEND_LINEARBURN = 11,
  IMB_BLEND_COLORDODGE = 12,
  IMB_BLEND_SCREEN = 13,
  IMB_BLEND_SOFTLIGHT = 14,
  IMB_BLEND_PINLIGHT = 15,
  IMB_BLEND_VIVIDLIGHT = 16,
  IMB_BLEND_LINEARLIGHT = 17,
  IMB_BLEND_DIFFERENCE = 18,
  IMB_BLEND_EXCLUSION = 19,
  IMB_BLEND_HUE = 20,
  IMB_BLEND_SATURATION = 21,
  IMB_BLEND_LUMINOSITY = 22,
  IMB_BLEND_COLOR = 23,
  IMB_BLEND_INTERPOLATE = 24,

  IMB_BLEND_COPY = 1000,
  IMB_BLEND_COPY_RGB = 1001,
  IMB_BLEND_COPY_ALPHA = 1002,
};

void IMB_blend_color_byte(unsigned char dst[4],
                          const unsigned char src1[4],
                          const unsigned char src2[4],
                          IMB_BlendMode mode);
void IMB_blend_color_float(float dst[4],
                           const float src1[4],
                           const float src2[4],
                           IMB_BlendMode mode);

/**
 * In-place image crop.
 */
void IMB_rect_crop(ImBuf *ibuf, const rcti *crop);

/**
 * In-place size setting (caller must fill in buffer contents).
 */
void IMB_rect_size_set(ImBuf *ibuf, const uint size[2]);

void IMB_rectclip(ImBuf *dbuf,
                  const ImBuf *sbuf,
                  int *destx,
                  int *desty,
                  int *srcx,
                  int *srcy,
                  int *width,
                  int *height);
void IMB_rectcpy(ImBuf *dbuf,
                 const ImBuf *sbuf,
                 int destx,
                 int desty,
                 int srcx,
                 int srcy,
                 int width,
                 int height);
void IMB_rectblend(ImBuf *dbuf,
                   const ImBuf *obuf,
                   const ImBuf *sbuf,
                   unsigned short *dmask,
                   const unsigned short *curvemask,
                   const unsigned short *texmask,
                   float mask_max,
                   int destx,
                   int desty,
                   int origx,
                   int origy,
                   int srcx,
                   int srcy,
                   int width,
                   int height,
                   IMB_BlendMode mode,
                   bool accumulate);
void IMB_rectblend_threaded(ImBuf *dbuf,
                            const ImBuf *obuf,
                            const ImBuf *sbuf,
                            unsigned short *dmask,
                            const unsigned short *curvemask,
                            const unsigned short *texmask,
                            float mask_max,
                            int destx,
                            int desty,
                            int origx,
                            int origy,
                            int srcx,
                            int srcy,
                            int width,
                            int height,
                            IMB_BlendMode mode,
                            bool accumulate);

enum eIMBInterpolationFilterMode {
  IMB_FILTER_NEAREST,
  IMB_FILTER_BILINEAR,
  IMB_FILTER_CUBIC_BSPLINE,
  IMB_FILTER_CUBIC_MITCHELL,
  IMB_FILTER_BOX,
};

/**
 * Defaults to BL_proxy within the directory of the animation.
 */
void IMB_anim_set_index_dir(ImBufAnim *anim, const char *dir);
void IMB_anim_get_filename(ImBufAnim *anim, char *filename, int filename_maxncpy);

int IMB_anim_index_get_frame_index(ImBufAnim *anim, IMB_Timecode_Type tc, int position);

int IMB_anim_proxy_get_existing(ImBufAnim *anim);

struct IndexBuildContext;

/**
 * Prepare context for proxies/time-codes builder
 */
IndexBuildContext *IMB_anim_index_rebuild_context(ImBufAnim *anim,
                                                  IMB_Timecode_Type tcs_in_use,
                                                  int proxy_sizes_in_use,
                                                  int quality,
                                                  const bool overwrite,
                                                  GSet *file_list,
                                                  bool build_only_on_bad_performance);

/**
 * Will rebuild all used indices and proxies at once.
 */
void IMB_anim_index_rebuild(IndexBuildContext *context,
                            bool *stop,
                            bool *do_update,
                            float *progress);

/**
 * Finish rebuilding proxies/time-codes and free temporary contexts used.
 */
void IMB_anim_index_rebuild_finish(IndexBuildContext *context, bool stop);

/**
 * Return the length (in frames) of the given \a anim.
 */
int IMB_anim_get_duration(ImBufAnim *anim, IMB_Timecode_Type tc);

/**
 * Return the encoded start offset (in seconds) of the given \a anim.
 */
double IMD_anim_get_offset(ImBufAnim *anim);

/**
 * Return the fps contained in movie files (function rval is false,
 * and frs_sec and frs_sec_base untouched if none available!)
 */
bool IMB_anim_get_fps(const ImBufAnim *anim,
                      bool no_av_base,
                      short *r_frs_sec,
                      float *r_frs_sec_base);

ImBufAnim *IMB_open_anim(const char *filepath,
                         int ib_flags,
                         int streamindex,
                         char colorspace[IM_MAX_SPACE]);
void IMB_suffix_anim(ImBufAnim *anim, const char *suffix);
void IMB_close_anim(ImBufAnim *anim);
void IMB_close_anim_proxies(ImBufAnim *anim);
bool IMB_anim_can_produce_frames(const ImBufAnim *anim);

int IMB_anim_get_image_width(ImBufAnim *anim);
int IMB_anim_get_image_height(ImBufAnim *anim);
bool IMB_get_gop_decode_time(ImBufAnim *anim);

ImBuf *IMB_anim_absolute(ImBufAnim *anim,
                         int position,
                         IMB_Timecode_Type tc /* = 1 = IMB_TC_RECORD_RUN */,
                         IMB_Proxy_Size preview_size /* = 0 = IMB_PROXY_NONE */);

/**
 * fetches a define preview-frame, usually half way into the movie.
 */
ImBuf *IMB_anim_previewframe(ImBufAnim *anim);

void IMB_free_anim(ImBufAnim *anim);

#define FILTER_MASK_NULL 0
#define FILTER_MASK_MARGIN 1
#define FILTER_MASK_USED 2

void IMB_filter(ImBuf *ibuf);
void IMB_mask_filter_extend(char *mask, int width, int height);
void IMB_mask_clear(ImBuf *ibuf, const char *mask, int val);
/**
 * If alpha is zero, it checks surrounding pixels and averages color. sets new alphas to 1.0
 * When a mask is given, the mask will be used instead of the alpha channel, where only
 * pixels with a mask value of 0 will be written to, and only pixels with a mask value of 1
 * will be used for the average. The mask will be set to one for the pixels which were written.
 */
void IMB_filter_extend(ImBuf *ibuf, char *mask, int filter);
/**
 * Frees too (if there) and recreates new data.
 */
void IMB_makemipmap(ImBuf *ibuf, int use_filter);
/**
 * Thread-safe version, only recreates existing maps.
 */
void IMB_remakemipmap(ImBuf *ibuf, int use_filter);
ImBuf *IMB_getmipmap(ImBuf *ibuf, int level);

void IMB_filtery(ImBuf *ibuf);

ImBuf *IMB_onehalf(ImBuf *ibuf1);

/**
 * Return true if \a ibuf is modified.
 */
bool IMB_scaleImBuf(ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 * Return true if \a ibuf is modified.
 */
bool IMB_scalefastImBuf(ImBuf *ibuf, unsigned int newx, unsigned int newy);

void IMB_scaleImBuf_threaded(ImBuf *ibuf, unsigned int newx, unsigned int newy);

bool IMB_saveiff(ImBuf *ibuf, const char *filepath, int flags);

bool IMB_ispic(const char *filepath);
bool IMB_ispic_type_matches(const char *filepath, int filetype);
int IMB_ispic_type_from_memory(const unsigned char *buf, size_t buf_size);
int IMB_ispic_type(const char *filepath);

/**
 * Test if the file is a video file (known format, has a video stream and
 * supported video codec).
 */
bool IMB_isanim(const char *filepath);

/**
 * Test if color-space conversions of pixels in buffer need to take into account alpha.
 */
bool IMB_alpha_affects_rgb(const ImBuf *ibuf);

/**
 * Create char buffer, color corrected if necessary, for ImBufs that lack one.
 */
void IMB_rect_from_float(ImBuf *ibuf);
void IMB_float_from_rect_ex(ImBuf *dst, const ImBuf *src, const rcti *region_to_update);
void IMB_float_from_rect(ImBuf *ibuf);
/**
 * No profile conversion.
 */
void IMB_color_to_bw(ImBuf *ibuf);
void IMB_saturation(ImBuf *ibuf, float sat);

/* Converting pixel buffers. */

/**
 * Float to byte pixels, output 4-channel RGBA.
 */
void IMB_buffer_byte_from_float(unsigned char *rect_to,
                                const float *rect_from,
                                int channels_from,
                                float dither,
                                int profile_to,
                                int profile_from,
                                bool predivide,
                                int width,
                                int height,
                                int stride_to,
                                int stride_from);
/**
 * Float to byte pixels, output 4-channel RGBA.
 */
void IMB_buffer_byte_from_float_mask(unsigned char *rect_to,
                                     const float *rect_from,
                                     int channels_from,
                                     float dither,
                                     bool predivide,
                                     int width,
                                     int height,
                                     int stride_to,
                                     int stride_from,
                                     char *mask);
/**
 * Byte to float pixels, input and output 4-channel RGBA.
 */
void IMB_buffer_float_from_byte(float *rect_to,
                                const unsigned char *rect_from,
                                int profile_to,
                                int profile_from,
                                bool predivide,
                                int width,
                                int height,
                                int stride_to,
                                int stride_from);
/**
 * Float to float pixels, output 4-channel RGBA.
 */
void IMB_buffer_float_from_float(float *rect_to,
                                 const float *rect_from,
                                 int channels_from,
                                 int profile_to,
                                 int profile_from,
                                 bool predivide,
                                 int width,
                                 int height,
                                 int stride_to,
                                 int stride_from);
void IMB_buffer_float_from_float_threaded(float *rect_to,
                                          const float *rect_from,
                                          int channels_from,
                                          int profile_to,
                                          int profile_from,
                                          bool predivide,
                                          int width,
                                          int height,
                                          int stride_to,
                                          int stride_from);
/**
 * Float to float pixels, output 4-channel RGBA.
 */
void IMB_buffer_float_from_float_mask(float *rect_to,
                                      const float *rect_from,
                                      int channels_from,
                                      int width,
                                      int height,
                                      int stride_to,
                                      int stride_from,
                                      char *mask);
/**
 * Byte to byte pixels, input and output 4-channel RGBA.
 */
void IMB_buffer_byte_from_byte(unsigned char *rect_to,
                               const unsigned char *rect_from,
                               int profile_to,
                               int profile_from,
                               bool predivide,
                               int width,
                               int height,
                               int stride_to,
                               int stride_from);

/**
 * Change the ordering of the color bytes pointed to by rect from
 * RGBA to ABGR. size * 4 color bytes are reordered.
 *
 * Only this one is used liberally here, and in imbuf.
 */
void IMB_convert_rgba_to_abgr(ImBuf *ibuf);

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3]);
void IMB_alpha_under_color_byte(unsigned char *rect, int x, int y, const float backcol[3]);

ImBuf *IMB_loadifffile(int file, int flags, char colorspace[IM_MAX_SPACE], const char *descr);

ImBuf *IMB_half_x(ImBuf *ibuf1);
ImBuf *IMB_half_y(ImBuf *ibuf1);

void IMB_flipx(ImBuf *ibuf);
void IMB_flipy(ImBuf *ibuf);

/* Rotate by 90 degree increments. Returns true if the ImBuf is altered. */
bool IMB_rotate_orthogonal(ImBuf *ibuf, int degrees);

/* Pre-multiply alpha. */

void IMB_premultiply_alpha(ImBuf *ibuf);
void IMB_unpremultiply_alpha(ImBuf *ibuf);

/**
 * Replace pixels of entire image with solid color.
 * \param drect: An image to be filled with color. It must be 4 channel image.
 * \param col: RGBA color, which is assigned directly to both byte (via scaling) and float buffers.
 */
void IMB_rectfill(ImBuf *drect, const float col[4]);
/**
 * Blend pixels of image area with solid color.
 *
 * For images with `uchar` buffer use color matching image color-space.
 * For images with float buffer use color display color-space.
 * If display color-space can not be referenced, use color in SRGB color-space.
 *
 * \param ibuf: an image to be filled with color. It must be 4 channel image.
 * \param col: RGBA color.
 * \param x1, y1, x2, y2: (x1, y1) defines starting point of the rectangular area to be filled,
 * (x2, y2) is the end point. Note that values are allowed to be loosely ordered, which means that
 * x2 is allowed to be lower than x1, as well as y2 is allowed to be lower than y1. No matter the
 * order the area between x1 and x2, and y1 and y2 is filled.
 * \param display: color-space reference for display space.
 */
void IMB_rectfill_area(
    ImBuf *ibuf, const float col[4], int x1, int y1, int x2, int y2, ColorManagedDisplay *display);
/**
 * Replace pixels of image area with solid color.
 * \param ibuf: an image to be filled with color. It must be 4 channel image.
 * \param col: RGBA color, which is assigned directly to both byte (via scaling) and float buffers.
 * \param x1, y1, x2, y2: (x1, y1) defines starting point of the rectangular area to be filled,
 * (x2, y2) is the end point. Note that values are allowed to be loosely ordered, which means that
 * x2 is allowed to be lower than x1, as well as y2 is allowed to be lower than y1. No matter the
 * order the area between x1 and x2, and y1 and y2 is filled.
 */
void IMB_rectfill_area_replace(
    const ImBuf *ibuf, const float col[4], int x1, int y1, int x2, int y2);
void IMB_rectfill_alpha(ImBuf *ibuf, float value);

/**
 * This should not be here, really,
 * we needed it for operating on render data, #IMB_rectfill_area calls it.
 */
void buf_rectfill_area(unsigned char *rect,
                       float *rectf,
                       int width,
                       int height,
                       const float col[4],
                       ColorManagedDisplay *display,
                       int x1,
                       int y1,
                       int x2,
                       int y2);

/**
 * Exported for image tools in blender, to quickly allocate 32 bits rect.
 */
void *imb_alloc_pixels(unsigned int x,
                       unsigned int y,
                       unsigned int channels,
                       size_t typesize,
                       bool initialize_pixels,
                       const char *alloc_name);

bool imb_addrectImBuf(ImBuf *ibuf, bool initialize_pixels = true);
/**
 * Any free `ibuf->rect` frees mipmaps to be sure, creation is in render on first request.
 */
void imb_freerectImBuf(ImBuf *ibuf);

bool imb_addrectfloatImBuf(ImBuf *ibuf,
                           const unsigned int channels,
                           bool initialize_pixels = true);
/**
 * Any free `ibuf->rect` frees mipmaps to be sure, creation is in render on first request.
 */
void imb_freerectfloatImBuf(ImBuf *ibuf);
void imb_freemipmapImBuf(ImBuf *ibuf);

/** Free all CPU pixel data (associated with image size). */
void imb_freerectImbuf_all(ImBuf *ibuf);

/* Free the GPU textures of the given image buffer, leaving the CPU buffers unchanged.
 * The ibuf can be nullptr, in which case the function does nothing. */
void IMB_free_gpu_textures(ImBuf *ibuf);

/**
 * Threaded processors.
 */
void IMB_processor_apply_threaded(
    int buffer_lines,
    int handle_size,
    void *init_customdata,
    void(init_handle)(void *handle, int start_line, int tot_line, void *customdata),
    void *(do_thread)(void *));

using ScanlineThreadFunc = void (*)(void *custom_data, int scanline);
void IMB_processor_apply_threaded_scanlines(int total_scanlines,
                                            ScanlineThreadFunc do_thread,
                                            void *custom_data);

/**
 * \brief Transform modes to use for IMB_transform function.
 *
 * These are not flags as the combination of cropping and repeat can lead to different expectation.
 */
enum eIMBTransformMode {
  /** \brief Do not crop or repeat. */
  IMB_TRANSFORM_MODE_REGULAR = 0,
  /** \brief Crop the source buffer. */
  IMB_TRANSFORM_MODE_CROP_SRC = 1,
  /** \brief Wrap repeat the source buffer. Only supported in with nearest filtering. */
  IMB_TRANSFORM_MODE_WRAP_REPEAT = 2,
};

/**
 * \brief Transform source image buffer onto destination image buffer using a transform matrix.
 *
 * \param src: Image buffer to read from.
 * \param dst: Image buffer to write to. rect or rect_float must already be initialized.
 * - dst buffer must be a 4 channel buffers.
 * - Only one data type buffer will be used (rect_float has priority over rect)
 * \param mode: Cropping/Wrap repeat effect to apply during transformation.
 * \param filter: Interpolation to use during sampling.
 * \param transform_matrix: Transformation matrix to use.
 * The given matrix should transform between dst pixel space to src pixel space.
 * One unit is one pixel.
 * \param src_crop: Cropping region how to crop the source buffer. Should only be passed when mode
 * is set to #IMB_TRANSFORM_MODE_CROP_SRC. For any other mode this should be empty.
 *
 * During transformation no data/color conversion will happens.
 * When transforming between float images the number of channels of the source buffer may be
 * between 1 and 4. When source buffer has one channel the data will be read as a gray scale value.
 */
void IMB_transform(const ImBuf *src,
                   ImBuf *dst,
                   eIMBTransformMode mode,
                   eIMBInterpolationFilterMode filter,
                   const float transform_matrix[4][4],
                   const rctf *src_crop);

/* FFMPEG */

void IMB_ffmpeg_init();
const char *IMB_ffmpeg_last_error();

GPUTexture *IMB_create_gpu_texture(const char *name,
                                   ImBuf *ibuf,
                                   bool use_high_bitdepth,
                                   bool use_premult);

eGPUTextureFormat IMB_gpu_get_texture_format(const ImBuf *ibuf,
                                             bool high_bitdepth,
                                             bool use_grayscale);

/**
 * Ensures that values stored in the float rect can safely loaded into half float gpu textures.
 *
 * Does nothing when given image_buffer doesn't contain a float rect.
 */
void IMB_gpu_clamp_half_float(ImBuf *image_buffer);

/**
 * The `ibuf` is only here to detect the storage type. The produced texture will have undefined
 * content. It will need to be populated by using #IMB_update_gpu_texture_sub().
 */
GPUTexture *IMB_touch_gpu_texture(const char *name,
                                  ImBuf *ibuf,
                                  int w,
                                  int h,
                                  int layers,
                                  bool use_high_bitdepth,
                                  bool use_grayscale);

/**
 * Will update a #GPUTexture using the content of the #ImBuf. Only one layer will be updated.
 * Will resize the ibuf if needed.
 * Z is the layer to update. Unused if the texture is 2D.
 */
void IMB_update_gpu_texture_sub(GPUTexture *tex,
                                ImBuf *ibuf,
                                int x,
                                int y,
                                int z,
                                int w,
                                int h,
                                bool use_high_bitdepth,
                                bool use_grayscale,
                                bool use_premult);

void IMB_stereo3d_write_dimensions(
    char mode, bool is_squeezed, size_t width, size_t height, size_t *r_width, size_t *r_height);
void IMB_stereo3d_read_dimensions(
    char mode, bool is_squeezed, size_t width, size_t height, size_t *r_width, size_t *r_height);
int *IMB_stereo3d_from_rect(const ImageFormatData *im_format,
                            size_t x,
                            size_t y,
                            size_t channels,
                            int *rect_left,
                            int *rect_right);
float *IMB_stereo3d_from_rectf(const ImageFormatData *im_format,
                               size_t x,
                               size_t y,
                               size_t channels,
                               float *rectf_left,
                               float *rectf_right);
/**
 * Left/right are always float.
 */
ImBuf *IMB_stereo3d_ImBuf(const ImageFormatData *im_format, ImBuf *ibuf_left, ImBuf *ibuf_right);
/**
 * Reading a stereo encoded ibuf (*left) and generating two ibufs from it (*left and *right).
 */
void IMB_ImBufFromStereo3d(const Stereo3dFormat *s3d,
                           ImBuf *ibuf_stereo,
                           ImBuf **r_ibuf_left,
                           ImBuf **r_ibuf_right);
