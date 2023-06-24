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

/* for bool */
#include "../blenlib/BLI_sys_types.h"
#include "../gpu/GPU_texture.h"

#include "BLI_implicit_sharing.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IM_MAX_SPACE 64

/**
 * \attention defined in ???
 */
struct ImBuf;
struct rctf;
struct rcti;

/**
 * \attention defined in ???
 */
struct anim;

struct ColorManagedDisplay;

struct GSet;
/**
 * \attention defined in DNA_scene_types.h
 */
struct ImageFormatData;
struct Stereo3dFormat;

/**
 * \attention Defined in allocimbuf.c
 */
void IMB_init(void);
void IMB_exit(void);

/**
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_ibImageFromMemory(const unsigned char *mem,
                                    size_t size,
                                    int flags,
                                    char colorspace[IM_MAX_SPACE],
                                    const char *descr);

/**
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_testiffname(const char *filepath, int flags);

/**
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_loadiffname(const char *filepath, int flags, char colorspace[IM_MAX_SPACE]);

/**
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_thumb_load_image(const char *filepath,
                                   const size_t max_thumb_size,
                                   char colorspace[IM_MAX_SPACE]);

/**
 * \attention Defined in allocimbuf.c
 */
void IMB_freeImBuf(struct ImBuf *ibuf);

/**
 * \attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_allocImBuf(unsigned int x,
                             unsigned int y,
                             unsigned char planes,
                             unsigned int flags);

/**
 * Initialize given ImBuf.
 *
 * Use in cases when temporary image buffer is allocated on stack.
 *
 * \attention Defined in allocimbuf.c
 */
bool IMB_initImBuf(
    struct ImBuf *ibuf, unsigned int x, unsigned int y, unsigned char planes, unsigned int flags);

/**
 * Create a copy of a pixel buffer and wrap it to a new ImBuf
 * (transferring ownership to the in imbuf).
 * \attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_allocFromBufferOwn(uint8_t *byte_buffer,
                                     float *float_buffer,
                                     unsigned int w,
                                     unsigned int h,
                                     unsigned int channels);

/**
 * Create a copy of a pixel buffer and wrap it to a new ImBuf
 * \attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_allocFromBuffer(const uint8_t *byte_buffer,
                                  const float *float_buffer,
                                  unsigned int w,
                                  unsigned int h,
                                  unsigned int channels);

/**
 * Assign the content of the corresponding buffer using an implicitly shareable data pointer.
 *
 * \note Does not modify the topology (width, height, number of channels)
 * or the mipmaps in any way.
 */
void IMB_assign_shared_byte_buffer(struct ImBuf *ibuf,
                                   uint8_t *buffer_data,
                                   const ImplicitSharingInfoHandle *implicit_sharing);
void IMB_assign_shared_float_buffer(struct ImBuf *ibuf,
                                    float *buffer_data,
                                    const ImplicitSharingInfoHandle *implicit_sharing);
void IMB_assign_shared_float_z_buffer(struct ImBuf *ibuf,
                                      float *buffer_data,
                                      const ImplicitSharingInfoHandle *implicit_sharing);

/**
 * Assign the content of the corresponding buffer with the given data and ownership.
 * The current content of the buffer is released corresponding to its ownership configuration.
 *
 * \note Does not modify the topology (width, height, number of channels)
 * or the mipmaps in any way.
 */
void IMB_assign_byte_buffer(struct ImBuf *ibuf, uint8_t *buffer_data, ImBufOwnership ownership);
void IMB_assign_float_buffer(struct ImBuf *ibuf, float *buffer_data, ImBufOwnership ownership);
void IMB_assign_z_buffer(struct ImBuf *ibuf, int *buffer_data, ImBufOwnership ownership);
void IMB_assign_float_z_buffer(struct ImBuf *ibuf, float *buffer_data, ImBufOwnership ownership);

/**
 * Make corresponding buffers available for modification.
 * Is achieved by ensuring that the given ImBuf is the only owner of the underlying buffer data.
 */
void IMB_make_writable_byte_buffer(struct ImBuf *ibuf);
void IMB_make_writable_float_buffer(struct ImBuf *ibuf);

/**
 * Steal the buffer data pointer: the ImBuf is no longer an owner of this data.
 * \note If the ImBuf does not own the data the behavior is undefined.
 * \note Stealing encoded buffer resets the encoded size.
 */
uint8_t *IMB_steal_byte_buffer(struct ImBuf *ibuf);
float *IMB_steal_float_buffer(struct ImBuf *ibuf);
uint8_t *IMB_steal_encoded_buffer(struct ImBuf *ibuf);

/**
 * Increase reference count to imbuf
 * (to delete an imbuf you have to call freeImBuf as many times as it
 * is referenced)
 *
 * \attention Defined in allocimbuf.c
 */

void IMB_refImBuf(struct ImBuf *ibuf);
struct ImBuf *IMB_makeSingleUser(struct ImBuf *ibuf);

/**
 * \attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_dupImBuf(const struct ImBuf *ibuf1);

/**
 * \attention Defined in allocimbuf.c
 */
bool addzbufImBuf(struct ImBuf *ibuf);
bool addzbuffloatImBuf(struct ImBuf *ibuf);

/**
 * Approximate size of ImBuf in memory
 *
 * \attention Defined in allocimbuf.c
 */
size_t IMB_get_size_in_memory(struct ImBuf *ibuf);

/**
 * \brief Get the length of the rect of the given image buffer in terms of pixels.
 *
 * This is the width * the height of the image buffer.
 * This function is preferred over `ibuf->x * ibuf->y` due to overflow issues when
 * working with large resolution images (30kx30k).
 *
 * \attention Defined in allocimbuf.c
 */
size_t IMB_get_rect_len(const struct ImBuf *ibuf);

/**
 * \attention Defined in rectop.c
 */

typedef enum IMB_BlendMode {
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
} IMB_BlendMode;

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
void IMB_rect_crop(struct ImBuf *ibuf, const struct rcti *crop);

/**
 * In-place size setting (caller must fill in buffer contents).
 */
void IMB_rect_size_set(struct ImBuf *ibuf, const uint size[2]);

void IMB_rectclip(struct ImBuf *dbuf,
                  const struct ImBuf *sbuf,
                  int *destx,
                  int *desty,
                  int *srcx,
                  int *srcy,
                  int *width,
                  int *height);
void IMB_rectcpy(struct ImBuf *dbuf,
                 const struct ImBuf *sbuf,
                 int destx,
                 int desty,
                 int srcx,
                 int srcy,
                 int width,
                 int height);
void IMB_rectblend(struct ImBuf *dbuf,
                   const struct ImBuf *obuf,
                   const struct ImBuf *sbuf,
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
void IMB_rectblend_threaded(struct ImBuf *dbuf,
                            const struct ImBuf *obuf,
                            const struct ImBuf *sbuf,
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

/**
 * \attention Defined in indexer.c
 */

typedef enum IMB_Timecode_Type {
  /** Don't use time-code files at all. */
  IMB_TC_NONE = 0,
  /**
   * Use images in the order as they are recorded
   * (currently, this is the only one implemented
   * and is a sane default).
   */
  IMB_TC_RECORD_RUN = 1,
  /**
   * Use global timestamp written by recording
   * device (prosumer camcorders e.g. can do that).
   */
  IMB_TC_FREE_RUN = 2,
  /**
   * Interpolate a global timestamp using the
   * record date and time written by recording
   * device (*every* consumer camcorder can do that).
   */
  IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN = 4,
  IMB_TC_RECORD_RUN_NO_GAPS = 8,
  IMB_TC_MAX_SLOT = 4,
} IMB_Timecode_Type;

typedef enum IMB_Proxy_Size {
  IMB_PROXY_NONE = 0,
  IMB_PROXY_25 = 1,
  IMB_PROXY_50 = 2,
  IMB_PROXY_75 = 4,
  IMB_PROXY_100 = 8,
  IMB_PROXY_MAX_SLOT = 4,
} IMB_Proxy_Size;
ENUM_OPERATORS(IMB_Proxy_Size, IMB_PROXY_100);

typedef enum eIMBInterpolationFilterMode {
  IMB_FILTER_NEAREST,
  IMB_FILTER_BILINEAR,
} eIMBInterpolationFilterMode;

/**
 * Defaults to BL_proxy within the directory of the animation.
 */
void IMB_anim_set_index_dir(struct anim *anim, const char *dir);
void IMB_anim_get_filename(struct anim *anim, char *filename, int filename_maxncpy);

int IMB_anim_index_get_frame_index(struct anim *anim, IMB_Timecode_Type tc, int position);

int IMB_anim_proxy_get_existing(struct anim *anim);

struct IndexBuildContext;

/**
 * Prepare context for proxies/time-codes builder
 */
struct IndexBuildContext *IMB_anim_index_rebuild_context(struct anim *anim,
                                                         IMB_Timecode_Type tcs_in_use,
                                                         int proxy_sizes_in_use,
                                                         int quality,
                                                         const bool overwrite,
                                                         struct GSet *file_list,
                                                         bool build_only_on_bad_performance);

/**
 * Will rebuild all used indices and proxies at once.
 */
void IMB_anim_index_rebuild(struct IndexBuildContext *context,
                            bool *stop,
                            bool *do_update,
                            float *progress);

/**
 * Finish rebuilding proxies/time-codes and free temporary contexts used.
 */
void IMB_anim_index_rebuild_finish(struct IndexBuildContext *context, bool stop);

/**
 * Return the length (in frames) of the given \a anim.
 */
int IMB_anim_get_duration(struct anim *anim, IMB_Timecode_Type tc);

/**
 * Return the encoded start offset (in seconds) of the given \a anim.
 */
double IMD_anim_get_offset(struct anim *anim);

/**
 * Return the fps contained in movie files (function rval is false,
 * and frs_sec and frs_sec_base untouched if none available!)
 */
bool IMB_anim_get_fps(struct anim *anim, short *frs_sec, float *frs_sec_base, bool no_av_base);

/**
 * \attention Defined in anim_movie.c
 */
struct anim *IMB_open_anim(const char *filepath,
                           int ib_flags,
                           int streamindex,
                           char colorspace[IM_MAX_SPACE]);
void IMB_suffix_anim(struct anim *anim, const char *suffix);
void IMB_close_anim(struct anim *anim);
void IMB_close_anim_proxies(struct anim *anim);
bool IMB_anim_can_produce_frames(const struct anim *anim);

/**
 * \attention Defined in anim_movie.c
 */

int ismovie(const char *filepath);
int IMB_anim_get_image_width(struct anim *anim);
int IMB_anim_get_image_height(struct anim *anim);
bool IMB_get_gop_decode_time(struct anim *anim);

/**
 * \attention Defined in anim_movie.c
 */

struct ImBuf *IMB_anim_absolute(struct anim *anim,
                                int position,
                                IMB_Timecode_Type tc /* = 1 = IMB_TC_RECORD_RUN */,
                                IMB_Proxy_Size preview_size /* = 0 = IMB_PROXY_NONE */);

/**
 * \attention Defined in anim_movie.c
 * fetches a define preview-frame, usually half way into the movie.
 */
struct ImBuf *IMB_anim_previewframe(struct anim *anim);

/**
 * \attention Defined in anim_movie.c
 */
void IMB_free_anim(struct anim *anim);

/**
 * \attention Defined in filter.c
 */

#define FILTER_MASK_NULL 0
#define FILTER_MASK_MARGIN 1
#define FILTER_MASK_USED 2

void IMB_filter(struct ImBuf *ibuf);
void IMB_mask_filter_extend(char *mask, int width, int height);
void IMB_mask_clear(struct ImBuf *ibuf, const char *mask, int val);
/**
 * If alpha is zero, it checks surrounding pixels and averages color. sets new alphas to 1.0
 * When a mask is given, the mask will be used instead of the alpha channel, where only
 * pixels with a mask value of 0 will be written to, and only pixels with a mask value of 1
 * will be used for the average. The mask will be set to one for the pixels which were written.
 */
void IMB_filter_extend(struct ImBuf *ibuf, char *mask, int filter);
/**
 * Frees too (if there) and recreates new data.
 */
void IMB_makemipmap(struct ImBuf *ibuf, int use_filter);
/**
 * Thread-safe version, only recreates existing maps.
 */
void IMB_remakemipmap(struct ImBuf *ibuf, int use_filter);
struct ImBuf *IMB_getmipmap(struct ImBuf *ibuf, int level);

/**
 * \attention Defined in filter.c
 */
void IMB_filtery(struct ImBuf *ibuf);

/**
 * \attention Defined in scaling.c
 */
struct ImBuf *IMB_onehalf(struct ImBuf *ibuf1);

/**
 * \attention Defined in scaling.c
 *
 * Return true if \a ibuf is modified.
 */
bool IMB_scaleImBuf(struct ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 * \attention Defined in scaling.c
 */
/**
 * Return true if \a ibuf is modified.
 */
bool IMB_scalefastImBuf(struct ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 * \attention Defined in scaling.c
 */
void IMB_scaleImBuf_threaded(struct ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 * \attention Defined in writeimage.c
 */
bool IMB_saveiff(struct ImBuf *ibuf, const char *filepath, int flags);

/**
 * \attention Defined in util.c
 */
bool IMB_ispic(const char *filepath);
bool IMB_ispic_type_matches(const char *filepath, int filetype);
int IMB_ispic_type_from_memory(const unsigned char *buf, size_t buf_size);
int IMB_ispic_type(const char *filepath);

/**
 * \attention Defined in util.c
 */
bool IMB_isanim(const char *filepath);

/**
 * \attention Defined in util.c
 */
int imb_get_anim_type(const char *filepath);

/**
 * Test if color-space conversions of pixels in buffer need to take into account alpha.
 */
bool IMB_alpha_affects_rgb(const struct ImBuf *ibuf);

/**
 * Create char buffer, color corrected if necessary, for ImBufs that lack one.
 */
void IMB_rect_from_float(struct ImBuf *ibuf);
void IMB_float_from_rect_ex(struct ImBuf *dst,
                            const struct ImBuf *src,
                            const struct rcti *region_to_update);
void IMB_float_from_rect(struct ImBuf *ibuf);
/**
 * No profile conversion.
 */
void IMB_color_to_bw(struct ImBuf *ibuf);
void IMB_saturation(struct ImBuf *ibuf, float sat);

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
void IMB_buffer_float_unpremultiply(float *buf, int width, int height);
void IMB_buffer_float_premultiply(float *buf, int width, int height);

/**
 * Change the ordering of the color bytes pointed to by rect from
 * rgba to abgr. size * 4 color bytes are reordered.
 *
 * \attention Defined in imageprocess.c
 *
 * Only this one is used liberally here, and in imbuf.
 */
void IMB_convert_rgba_to_abgr(struct ImBuf *ibuf);

/**
 * \attention defined in imageprocess.c
 */

void bicubic_interpolation(
    const struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);
void nearest_interpolation(
    const struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);
void bilinear_interpolation(
    const struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);

typedef void (*InterpolationColorFunction)(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void bicubic_interpolation_color(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);

/* Functions assumes out to be zeroed, only does RGBA. */

void nearest_interpolation_color_char(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void nearest_interpolation_color_fl(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void nearest_interpolation_color(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void nearest_interpolation_color_wrap(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void bilinear_interpolation_color(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void bilinear_interpolation_color_char(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
void bilinear_interpolation_color_fl(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
/**
 * Note about wrapping, the u/v still needs to be within the image bounds,
 * just the interpolation is wrapped.
 * This the same as bilinear_interpolation_color except it wraps
 * rather than using empty and emptyI.
 */
void bilinear_interpolation_color_wrap(
    const struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3]);
void IMB_alpha_under_color_byte(unsigned char *rect, int x, int y, const float backcol[3]);

/**
 * Sample pixel of image using NEAREST method.
 */
void IMB_sampleImageAtLocation(
    struct ImBuf *ibuf, float x, float y, bool make_linear_rgb, float color[4]);

/**
 * \attention defined in readimage.c
 */
struct ImBuf *IMB_loadifffile(int file,
                              int flags,
                              char colorspace[IM_MAX_SPACE],
                              const char *descr);

/**
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_half_x(struct ImBuf *ibuf1);

/**
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_fast_x(struct ImBuf *ibuf1);

/**
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_x(struct ImBuf *ibuf1);

/**
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_half_y(struct ImBuf *ibuf1);

/**
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_fast_y(struct ImBuf *ibuf1);

/**
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_y(struct ImBuf *ibuf1);

/**
 * \attention Defined in rotate.c
 */
void IMB_flipx(struct ImBuf *ibuf);
void IMB_flipy(struct ImBuf *ibuf);

/* Pre-multiply alpha. */

void IMB_premultiply_alpha(struct ImBuf *ibuf);
void IMB_unpremultiply_alpha(struct ImBuf *ibuf);

/**
 * \attention Defined in allocimbuf.c
 */
void IMB_freezbufImBuf(struct ImBuf *ibuf);
void IMB_freezbuffloatImBuf(struct ImBuf *ibuf);

/**
 * \attention Defined in rectop.c
 */
/**
 * Replace pixels of entire image with solid color.
 * \param drect: An image to be filled with color. It must be 4 channel image.
 * \param col: RGBA color, which is assigned directly to both byte (via scaling) and float buffers.
 */
void IMB_rectfill(struct ImBuf *drect, const float col[4]);
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
void IMB_rectfill_area(struct ImBuf *ibuf,
                       const float col[4],
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       struct ColorManagedDisplay *display);
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
    const struct ImBuf *ibuf, const float col[4], int x1, int y1, int x2, int y2);
void IMB_rectfill_alpha(struct ImBuf *ibuf, float value);

/**
 * This should not be here, really,
 * we needed it for operating on render data, #IMB_rectfill_area calls it.
 */
void buf_rectfill_area(unsigned char *rect,
                       float *rectf,
                       int width,
                       int height,
                       const float col[4],
                       struct ColorManagedDisplay *display,
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
                       const char *alloc_name);

bool imb_addrectImBuf(struct ImBuf *ibuf);
/**
 * Any free `ibuf->rect` frees mipmaps to be sure, creation is in render on first request.
 */
void imb_freerectImBuf(struct ImBuf *ibuf);

bool imb_addrectfloatImBuf(struct ImBuf *ibuf, const unsigned int channels);
/**
 * Any free `ibuf->rect` frees mipmaps to be sure, creation is in render on first request.
 */
void imb_freerectfloatImBuf(struct ImBuf *ibuf);
void imb_freemipmapImBuf(struct ImBuf *ibuf);

/** Free all pixel data (associated with image size). */
void imb_freerectImbuf_all(struct ImBuf *ibuf);

/**
 * Threaded processors.
 */
void IMB_processor_apply_threaded(
    int buffer_lines,
    int handle_size,
    void *init_customdata,
    void(init_handle)(void *handle, int start_line, int tot_line, void *customdata),
    void *(do_thread)(void *));

typedef void (*ScanlineThreadFunc)(void *custom_data, int scanline);
void IMB_processor_apply_threaded_scanlines(int total_scanlines,
                                            ScanlineThreadFunc do_thread,
                                            void *custom_data);

/**
 * \brief Transform modes to use for IMB_transform function.
 *
 * These are not flags as the combination of cropping and repeat can lead to different expectation.
 */
typedef enum eIMBTransformMode {
  /** \brief Do not crop or repeat. */
  IMB_TRANSFORM_MODE_REGULAR = 0,
  /** \brief Crop the source buffer. */
  IMB_TRANSFORM_MODE_CROP_SRC = 1,
  /** \brief Wrap repeat the source buffer. Only supported in with nearest filtering. */
  IMB_TRANSFORM_MODE_WRAP_REPEAT = 2,
} eIMBTransformMode;

/**
 * \brief Transform source image buffer onto destination image buffer using a transform matrix.
 *
 * \param src: Image buffer to read from.
 * \param dst: Image buffer to write to. rect or rect_float must already be initialized.
 * - dst buffer must be a 4 channel buffers.
 * - Only one data type buffer will be used (rect_float has priority over rect)
 * \param mode: Cropping/Wrap repeat effect to apply during transformation.
 * \param filter: Interpolation to use during sampling.
 * \param num_subsamples: Number of subsamples to use. Increasing this would improve the quality,
 * but reduces the performance.
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
void IMB_transform(const struct ImBuf *src,
                   struct ImBuf *dst,
                   eIMBTransformMode mode,
                   eIMBInterpolationFilterMode filter,
                   const int num_subsamples,
                   const float transform_matrix[4][4],
                   const struct rctf *src_crop);

/* FFMPEG */

void IMB_ffmpeg_init(void);
const char *IMB_ffmpeg_last_error(void);

/**
 * \attention defined in util_gpu.c
 */
GPUTexture *IMB_create_gpu_texture(const char *name,
                                   struct ImBuf *ibuf,
                                   bool use_high_bitdepth,
                                   bool use_premult);

eGPUTextureFormat IMB_gpu_get_texture_format(const struct ImBuf *ibuf,
                                             bool high_bitdepth,
                                             bool use_grayscale);

/**
 * Ensures that values stored in the float rect can safely loaded into half float gpu textures.
 *
 * Does nothing when given image_buffer doesn't contain a float rect.
 */
void IMB_gpu_clamp_half_float(struct ImBuf *image_buffer);

/**
 * The `ibuf` is only here to detect the storage type. The produced texture will have undefined
 * content. It will need to be populated by using #IMB_update_gpu_texture_sub().
 */
GPUTexture *IMB_touch_gpu_texture(const char *name,
                                  struct ImBuf *ibuf,
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
                                struct ImBuf *ibuf,
                                int x,
                                int y,
                                int z,
                                int w,
                                int h,
                                bool use_high_bitdepth,
                                bool use_grayscale,
                                bool use_premult);

/**
 * \attention defined in stereoimbuf.c
 */
void IMB_stereo3d_write_dimensions(
    char mode, bool is_squeezed, size_t width, size_t height, size_t *r_width, size_t *r_height);
void IMB_stereo3d_read_dimensions(
    char mode, bool is_squeezed, size_t width, size_t height, size_t *r_width, size_t *r_height);
int *IMB_stereo3d_from_rect(const struct ImageFormatData *im_format,
                            size_t x,
                            size_t y,
                            size_t channels,
                            int *rect_left,
                            int *rect_right);
float *IMB_stereo3d_from_rectf(const struct ImageFormatData *im_format,
                               size_t x,
                               size_t y,
                               size_t channels,
                               float *rectf_left,
                               float *rectf_right);
/**
 * Left/right are always float.
 */
struct ImBuf *IMB_stereo3d_ImBuf(const struct ImageFormatData *im_format,
                                 struct ImBuf *ibuf_left,
                                 struct ImBuf *ibuf_right);
/**
 * Reading a stereo encoded ibuf (*left) and generating two ibufs from it (*left and *right).
 */
void IMB_ImBufFromStereo3d(const struct Stereo3dFormat *s3d,
                           struct ImBuf *ibuf_stereo,
                           struct ImBuf **r_ibuf_left,
                           struct ImBuf **r_ibuf_right);

#ifdef __cplusplus
}
#endif
