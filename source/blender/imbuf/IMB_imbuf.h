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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

/**
 * \brief IMage Buffer module.
 *
 * This module offers import/export of several graphical file formats.
 * \ingroup imbuf
 *
 * \page IMB Imbuf module external interface
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
 *     The listbase types are used for handling the memory
 *     management.
 * - \ref blenlib module
 *     blenlib handles guarded memory management in blender-style.
 *     BLI_winstuff.h makes a few windows specific behaviors
 *     posix-compliant.
 */

#ifndef __IMB_IMBUF_H__
#define __IMB_IMBUF_H__

#define IM_MAX_SPACE 64

/* for bool */
#include "../blenlib/BLI_sys_types.h"

/**
 *
 * \attention defined in ???
 */
struct ImBuf;

/**
 *
 * \attention defined in ???
 */
struct anim;

struct ColorManagedDisplay;

struct GSet;
/**
 *
 * \attention defined in DNA_scene_types.h
 */
struct ImageFormatData;
struct Stereo3dFormat;

/**
 *
 * \attention Defined in allocimbuf.c
 */
void IMB_init(void);
void IMB_exit(void);

/**
 *
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_ibImageFromMemory(const unsigned char *mem,
                                    size_t size,
                                    int flags,
                                    char colorspace[IM_MAX_SPACE],
                                    const char *descr);

/**
 *
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_testiffname(const char *filepath, int flags);

/**
 *
 * \attention Defined in readimage.c
 */
struct ImBuf *IMB_loadiffname(const char *filepath, int flags, char colorspace[IM_MAX_SPACE]);

/**
 *
 * \attention Defined in allocimbuf.c
 */
void IMB_freeImBuf(struct ImBuf *ibuf);

/**
 *
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
 * \attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_allocFromBuffer(const unsigned int *rect,
                                  const float *rectf,
                                  unsigned int w,
                                  unsigned int h);

/**
 *
 * Increase reference count to imbuf
 * (to delete an imbuf you have to call freeImBuf as many times as it
 * is referenced)
 *
 * \attention Defined in allocimbuf.c
 */

void IMB_refImBuf(struct ImBuf *ibuf);
struct ImBuf *IMB_makeSingleUser(struct ImBuf *ibuf);

/**
 *
 * \attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_dupImBuf(const struct ImBuf *ibuf1);

/**
 *
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
 *
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
                          unsigned char src1[4],
                          unsigned char src2[4],
                          IMB_BlendMode mode);
void IMB_blend_color_float(float dst[4], float src1[4], float src2[4], IMB_BlendMode mode);

void IMB_rectclip(struct ImBuf *dbuf,
                  struct ImBuf *sbuf,
                  int *destx,
                  int *desty,
                  int *srcx,
                  int *srcy,
                  int *width,
                  int *height);
void IMB_rectcpy(struct ImBuf *drect,
                 struct ImBuf *srect,
                 int destx,
                 int desty,
                 int srcx,
                 int srcy,
                 int width,
                 int height);
void IMB_rectblend(struct ImBuf *dbuf,
                   struct ImBuf *obuf,
                   struct ImBuf *sbuf,
                   unsigned short *dmask,
                   unsigned short *curvemask,
                   unsigned short *mmask,
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
                            struct ImBuf *obuf,
                            struct ImBuf *sbuf,
                            unsigned short *dmask,
                            unsigned short *curvemask,
                            unsigned short *mmask,
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
 *
 * \attention Defined in indexer.c
 */

typedef enum IMB_Timecode_Type {
  /** Don't use timecode files at all. */
  IMB_TC_NONE = 0,
  /** use images in the order as they are recorded
   * (currently, this is the only one implemented
   * and is a sane default) */
  IMB_TC_RECORD_RUN = 1,
  /** Use global timestamp written by recording
   * device (prosumer camcorders e.g. can do that). */
  IMB_TC_FREE_RUN = 2,
  /** Interpolate a global timestamp using the
   * record date and time written by recording
   * device (*every* consumer camcorder can do
   * that :) )*/
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

/* defaults to BL_proxy within the directory of the animation */
void IMB_anim_set_index_dir(struct anim *anim, const char *dir);
void IMB_anim_get_fname(struct anim *anim, char *file, int size);

int IMB_anim_index_get_frame_index(struct anim *anim, IMB_Timecode_Type tc, int position);

IMB_Proxy_Size IMB_anim_proxy_get_existing(struct anim *anim);

struct IndexBuildContext;

/* prepare context for proxies/imecodes builder */
struct IndexBuildContext *IMB_anim_index_rebuild_context(struct anim *anim,
                                                         IMB_Timecode_Type tcs_in_use,
                                                         IMB_Proxy_Size proxy_sizes_in_use,
                                                         int quality,
                                                         const bool overwite,
                                                         struct GSet *file_list);

/* will rebuild all used indices and proxies at once */
void IMB_anim_index_rebuild(struct IndexBuildContext *context,
                            short *stop,
                            short *do_update,
                            float *progress);

/* finish rebuilding proxises/timecodes and free temporary contexts used */
void IMB_anim_index_rebuild_finish(struct IndexBuildContext *context, short stop);

/**
 * Return the length (in frames) of the given \a anim.
 */
int IMB_anim_get_duration(struct anim *anim, IMB_Timecode_Type tc);

/**
 * Return the fps contained in movie files (function rval is false,
 * and frs_sec and frs_sec_base untouched if none available!)
 */
bool IMB_anim_get_fps(struct anim *anim, short *frs_sec, float *frs_sec_base, bool no_av_base);

/**
 *
 * \attention Defined in anim_movie.c
 */
struct anim *IMB_open_anim(const char *name,
                           int ib_flags,
                           int streamindex,
                           char colorspace[IM_MAX_SPACE]);
void IMB_suffix_anim(struct anim *anim, const char *suffix);
void IMB_close_anim(struct anim *anim);
void IMB_close_anim_proxies(struct anim *anim);

/**
 *
 * \attention Defined in anim_movie.c
 */

int ismovie(const char *filepath);
void IMB_anim_set_preseek(struct anim *anim, int preseek);
int IMB_anim_get_preseek(struct anim *anim);

/**
 *
 * \attention Defined in anim_movie.c
 */

struct ImBuf *IMB_anim_absolute(struct anim *anim,
                                int position,
                                IMB_Timecode_Type tc /* = 1 = IMB_TC_RECORD_RUN */,
                                IMB_Proxy_Size preview_size /* = 0 = IMB_PROXY_NONE */);

/**
 *
 * \attention Defined in anim_movie.c
 * fetches a define previewframe, usually half way into the movie
 */
struct ImBuf *IMB_anim_previewframe(struct anim *anim);

/**
 *
 * \attention Defined in anim_movie.c
 */
void IMB_free_anim(struct anim *anim);

/**
 *
 * \attention Defined in filter.c
 */

#define FILTER_MASK_NULL 0
#define FILTER_MASK_MARGIN 1
#define FILTER_MASK_USED 2

void IMB_filter(struct ImBuf *ibuf);
void IMB_mask_filter_extend(char *mask, int width, int height);
void IMB_mask_clear(struct ImBuf *ibuf, char *mask, int val);
void IMB_filter_extend(struct ImBuf *ibuf, char *mask, int filter);
void IMB_makemipmap(struct ImBuf *ibuf, int use_filter);
void IMB_remakemipmap(struct ImBuf *ibuf, int use_filter);
struct ImBuf *IMB_getmipmap(struct ImBuf *ibuf, int level);

/**
 *
 * \attention Defined in cache.c
 */

void IMB_tile_cache_params(int totthread, int maxmem);
unsigned int *IMB_gettile(struct ImBuf *ibuf, int tx, int ty, int thread);
void IMB_tiles_to_rect(struct ImBuf *ibuf);

/**
 *
 * \attention Defined in filter.c
 */
void IMB_filtery(struct ImBuf *ibuf);

/**
 *
 * \attention Defined in scaling.c
 */
struct ImBuf *IMB_onehalf(struct ImBuf *ibuf1);

/**
 *
 * \attention Defined in scaling.c
 */
bool IMB_scaleImBuf(struct ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 *
 * \attention Defined in scaling.c
 */
bool IMB_scalefastImBuf(struct ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 *
 * \attention Defined in scaling.c
 */
void IMB_scaleImBuf_threaded(struct ImBuf *ibuf, unsigned int newx, unsigned int newy);

/**
 *
 * \attention Defined in writeimage.c
 */
short IMB_saveiff(struct ImBuf *ibuf, const char *filepath, int flags);
bool IMB_prepare_write_ImBuf(const bool isfloat, struct ImBuf *ibuf);

/**
 *
 * \attention Defined in util.c
 */
bool IMB_ispic(const char *name);
int IMB_ispic_type(const char *name);

/**
 *
 * \attention Defined in util.c
 */
bool IMB_isanim(const char *name);

/**
 *
 * \attention Defined in util.c
 */
int imb_get_anim_type(const char *name);

/**
 *
 * \attention Defined in util.c
 */
bool IMB_isfloat(struct ImBuf *ibuf);

/* Do byte/float and colorspace conversions need to take alpha into account? */
bool IMB_alpha_affects_rgb(const struct ImBuf *ibuf);

/* create char buffer, color corrected if necessary, for ImBufs that lack one */
void IMB_rect_from_float(struct ImBuf *ibuf);
void IMB_float_from_rect(struct ImBuf *ibuf);
void IMB_color_to_bw(struct ImBuf *ibuf);
void IMB_saturation(struct ImBuf *ibuf, float sat);

/* converting pixel buffers */
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
void IMB_buffer_float_from_byte(float *rect_to,
                                const unsigned char *rect_from,
                                int profile_to,
                                int profile_from,
                                bool predivide,
                                int width,
                                int height,
                                int stride_to,
                                int stride_from);
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
void IMB_buffer_float_from_float_mask(float *rect_to,
                                      const float *rect_from,
                                      int channels_from,
                                      int width,
                                      int height,
                                      int stride_to,
                                      int stride_from,
                                      char *mask);
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
 */
void IMB_convert_rgba_to_abgr(struct ImBuf *ibuf);

/**
 *
 * \attention defined in imageprocess.c
 */
void bicubic_interpolation(
    struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);
void nearest_interpolation(
    struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);
void bilinear_interpolation(
    struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);

void bicubic_interpolation_color(
    struct ImBuf *in, unsigned char col[4], float col_float[4], float u, float v);
void nearest_interpolation_color(
    struct ImBuf *in, unsigned char col[4], float col_float[4], float u, float v);
void nearest_interpolation_color_wrap(
    struct ImBuf *in, unsigned char col[4], float col_float[4], float u, float v);
void bilinear_interpolation_color(
    struct ImBuf *in, unsigned char col[4], float col_float[4], float u, float v);
void bilinear_interpolation_color_wrap(
    struct ImBuf *in, unsigned char col[4], float col_float[4], float u, float v);

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3]);
void IMB_alpha_under_color_byte(unsigned char *rect, int x, int y, float backcol[3]);

/**
 *
 * \attention defined in readimage.c
 */
struct ImBuf *IMB_loadifffile(
    int file, const char *filepath, int flags, char colorspace[IM_MAX_SPACE], const char *descr);

/**
 *
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_half_x(struct ImBuf *ibuf1);

/**
 *
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_fast_x(struct ImBuf *ibuf1);

/**
 *
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_x(struct ImBuf *ibuf1);

/**
 *
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_half_y(struct ImBuf *ibuf1);

/**
 *
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_fast_y(struct ImBuf *ibuf1);

/**
 *
 * \attention defined in scaling.c
 */
struct ImBuf *IMB_double_y(struct ImBuf *ibuf1);

/**
 *
 * \attention Defined in rotate.c
 */
void IMB_flipx(struct ImBuf *ibuf);
void IMB_flipy(struct ImBuf *ibuf);

/* Premultiply alpha */

void IMB_premultiply_alpha(struct ImBuf *ibuf);
void IMB_unpremultiply_alpha(struct ImBuf *ibuf);

/**
 *
 * \attention Defined in allocimbuf.c
 */
void IMB_freezbufImBuf(struct ImBuf *ibuf);
void IMB_freezbuffloatImBuf(struct ImBuf *ibuf);

/**
 *
 * \attention Defined in rectop.c
 */
void IMB_rectfill(struct ImBuf *drect, const float col[4]);
void IMB_rectfill_area(struct ImBuf *ibuf,
                       const float col[4],
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       struct ColorManagedDisplay *display);
void IMB_rectfill_alpha(struct ImBuf *ibuf, const float value);

/* This should not be here, really,
 * we needed it for operating on render data, IMB_rectfill_area calls it. */
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

/* exported for image tools in blender, to quickly allocate 32 bits rect */
void *imb_alloc_pixels(
    unsigned int x, unsigned int y, unsigned int channels, size_t typesize, const char *name);

bool imb_addrectImBuf(struct ImBuf *ibuf);
void imb_freerectImBuf(struct ImBuf *ibuf);

bool imb_addrectfloatImBuf(struct ImBuf *ibuf);
void imb_freerectfloatImBuf(struct ImBuf *ibuf);
void imb_freemipmapImBuf(struct ImBuf *ibuf);

bool imb_addtilesImBuf(struct ImBuf *ibuf);
void imb_freetilesImBuf(struct ImBuf *ibuf);

/* threaded processors */
void IMB_processor_apply_threaded(
    int buffer_lines,
    int handle_size,
    void *init_customdata,
    void(init_handle)(void *handle, int start_line, int tot_line, void *customdata),
    void *(do_thread)(void *));

typedef void (*ScanlineThreadFunc)(void *custom_data, int start_scanline, int num_scanlines);
void IMB_processor_apply_threaded_scanlines(int total_scanlines,
                                            ScanlineThreadFunc do_thread,
                                            void *custom_data);

/* ffmpeg */
void IMB_ffmpeg_init(void);
const char *IMB_ffmpeg_last_error(void);

/**
 *
 * \attention defined in stereoimbuf.c
 */
void IMB_stereo3d_write_dimensions(const char mode,
                                   const bool is_squeezed,
                                   const size_t width,
                                   const size_t height,
                                   size_t *r_width,
                                   size_t *r_height);
void IMB_stereo3d_read_dimensions(const char mode,
                                  const bool is_squeezed,
                                  const size_t width,
                                  const size_t height,
                                  size_t *r_width,
                                  size_t *r_height);
int *IMB_stereo3d_from_rect(struct ImageFormatData *im_format,
                            const size_t x,
                            const size_t y,
                            const size_t channels,
                            int *rect_left,
                            int *rect_right);
float *IMB_stereo3d_from_rectf(struct ImageFormatData *im_format,
                               const size_t x,
                               const size_t y,
                               const size_t channels,
                               float *rectf_left,
                               float *rectf_right);
struct ImBuf *IMB_stereo3d_ImBuf(struct ImageFormatData *im_format,
                                 struct ImBuf *ibuf_left,
                                 struct ImBuf *ibuf_right);
void IMB_ImBufFromStereo3d(struct Stereo3dFormat *s3d,
                           struct ImBuf *ibuf_stereo,
                           struct ImBuf **r_ibuf_left,
                           struct ImBuf **r_ibuf_right);

#endif
