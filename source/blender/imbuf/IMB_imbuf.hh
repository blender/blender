/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "../gpu/GPU_texture.hh"

#include "BLI_enum_flags.hh"
#include "BLI_math_matrix_types.hh"

#include "IMB_imbuf_types.hh"

struct ImBuf;
struct rctf;
struct rcti;

struct GSet;
struct ImageFormatData;
struct Stereo3dFormat;

/**
 * Module init/exit.
 */
void IMB_init();
void IMB_exit();

/**
 * Load image.
 */
ImBuf *IMB_load_image_from_memory(const unsigned char *mem,
                                  const size_t size,
                                  const int flags,
                                  const char *descr,
                                  const char *filepath = nullptr,
                                  char r_colorspace[IM_MAX_SPACE] = nullptr);

ImBuf *IMB_load_image_from_file_descriptor(const int file,
                                           const int flags,
                                           const char *filepath = nullptr,
                                           char r_colorspace[IM_MAX_SPACE] = nullptr);

ImBuf *IMB_load_image_from_filepath(const char *filepath,
                                    const int flags,
                                    char r_colorspace[IM_MAX_SPACE] = nullptr);

/**
 * Save image.
 */
bool IMB_save_image(ImBuf *ibuf, const char *filepath, const int flags);

/**
 * Test image file.
 */
bool IMB_test_image(const char *filepath);
bool IMB_test_image_type_matches(const char *filepath, int filetype);
int IMB_test_image_type_from_memory(const unsigned char *buf, size_t buf_size);
int IMB_test_image_type(const char *filepath);

/**
 * Load thumbnail image.
 */
enum class IMBThumbLoadFlags {
  Zero = 0,
  /** Normally files larger than 100MB are not loaded for thumbnails, except when this flag is set.
   */
  LoadLargeFiles = (1 << 0),
};
ENUM_OPERATORS(IMBThumbLoadFlags);

ImBuf *IMB_thumb_load_image(const char *filepath,
                            const size_t max_thumb_size,
                            char colorspace[IM_MAX_SPACE],
                            const IMBThumbLoadFlags load_flags = IMBThumbLoadFlags::Zero);

/**
 * Allocate and free image buffer.
 */
ImBuf *IMB_allocImBuf(unsigned int x, unsigned int y, unsigned char planes, unsigned int flags);
void IMB_freeImBuf(ImBuf *ibuf);

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
 * \note Does not modify the topology (width, height, number of channels).
 */
void IMB_assign_byte_buffer(ImBuf *ibuf, uint8_t *buffer_data, ImBufOwnership ownership);
void IMB_assign_float_buffer(ImBuf *ibuf, float *buffer_data, ImBufOwnership ownership);

/**
 * Assign the content and the color space of the corresponding buffer the data from the given
 * buffer.
 *
 * \note Does not modify the topology (width, height, number of channels).
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
size_t IMB_get_size_in_memory(const ImBuf *ibuf);

/**
 * \brief Get the length of the data of the given image buffer in pixels.
 *
 * This is the width * the height of the image buffer.
 * This function is preferred over `ibuf->x * ibuf->y` due to 32 bit int overflow
 * issues when working with very large resolution images.
 */
size_t IMB_get_pixel_count(const ImBuf *ibuf);

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

#define FILTER_MASK_NULL 0
#define FILTER_MASK_MARGIN 1
#define FILTER_MASK_USED 2

void IMB_mask_filter_extend(char *mask, int width, int height);
void IMB_mask_clear(ImBuf *ibuf, const char *mask, int val);
/**
 * If alpha is zero, it checks surrounding pixels and averages color. sets new alphas to 1.0
 * When a mask is given, the mask will be used instead of the alpha channel, where only
 * pixels with a mask value of 0 will be written to, and only pixels with a mask value of 1
 * will be used for the average. The mask will be set to one for the pixels which were written.
 */
void IMB_filter_extend(ImBuf *ibuf, char *mask, int filter);

void IMB_filtery(ImBuf *ibuf);

/** Interpolation filter used by `IMB_scale`. */
enum class IMBScaleFilter {
  /** No filtering (point sampling). This is fastest but lowest quality. */
  Nearest,
  /**
   * Bilinear filter: each pixel in result image interpolates between 2x2 pixels of source image.
   */
  Bilinear,
  /**
   * Box filter. Behaves exactly like Bilinear when scaling up,
   * better results when scaling down by more than 2x.
   */
  Box,
};

/**
 * Scale/resize image to new dimensions.
 * Return true if \a ibuf is modified.
 */
bool IMB_scale(ImBuf *ibuf,
               unsigned int newx,
               unsigned int newy,
               IMBScaleFilter filter,
               bool threaded = true);

/**
 * Scale/resize image to new dimensions, into a newly created result image.
 * Metadata of input image (if any) is copied into the result image.
 */
ImBuf *IMB_scale_into_new(const ImBuf *ibuf,
                          unsigned int newx,
                          unsigned int newy,
                          IMBScaleFilter filter,
                          bool threaded = true);

/**
 * Test if color-space conversions of pixels in buffer need to take into account alpha.
 */
bool IMB_alpha_affects_rgb(const ImBuf *ibuf);

/**
 * Create char buffer, color corrected if necessary, for ImBufs that lack one.
 */
void IMB_byte_from_float(ImBuf *ibuf);
void IMB_float_from_byte_ex(ImBuf *dst, const ImBuf *src, const rcti *region_to_update);
void IMB_float_from_byte(ImBuf *ibuf);
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
                                int stride_from,
                                int start_y = 0);
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

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3]);
void IMB_alpha_under_color_byte(unsigned char *rect, int x, int y, const float backcol[3]);

void IMB_flipx(ImBuf *ibuf);
void IMB_flipy(ImBuf *ibuf);

/** Rotate by 90 degree increments. Returns true if the ImBuf is altered. */
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
 * \param ibuf: an image to be filled with color. It must be 4 channel image.
 * \param scene_linear_color: RGBA color in scene linear colorspace. For byte buffers, this is
 * converted to the byte buffer colorspace.
 * \param x1, y1, x2, y2: (x1, y1) defines starting point
 * of the rectangular area to be filled, (x2, y2) is the end point. Note that values are allowed to
 * be loosely ordered, which means that x2 is allowed to be lower than x1, as well as y2 is allowed
 * to be lower than y1. No matter the order the area between x1 and x2, and y1 and y2 is filled.
 * \param colorspace: color-space reference for display space.
 */
void IMB_rectfill_area(
    ImBuf *ibuf, const float scene_linear_color[4], int x1, int y1, int x2, int y2);
void IMB_rectfill_alpha(ImBuf *ibuf, float value);

/**
 * Exported for image tools in blender, to quickly allocate 32 bits rect.
 */
void *imb_alloc_pixels(unsigned int x,
                       unsigned int y,
                       unsigned int channels,
                       size_t typesize,
                       bool initialize_pixels,
                       const char *alloc_name);

/**
 * Allocate storage for byte type pixels.
 * If the image already contains byte data storage, it is freed first.
 */
bool IMB_alloc_byte_pixels(ImBuf *ibuf, bool initialize_pixels = true);

/**
 * Deallocate image byte storage.
 */
void IMB_free_byte_pixels(ImBuf *ibuf);

/**
 * Allocate storage for float type pixels.
 * If the image already contains float data storage, it is freed first.
 */
bool IMB_alloc_float_pixels(ImBuf *ibuf,
                            const unsigned int channels,
                            bool initialize_pixels = true);
/**
 * Deallocate image float storage.
 */
void IMB_free_float_pixels(ImBuf *ibuf);

/** Deallocate all CPU side data storage (byte, float, encoded). */
void IMB_free_all_data(ImBuf *ibuf);

/**
 * Free the GPU textures of the given image buffer, leaving the CPU buffers unchanged.
 * The ibuf can be nullptr, in which case the function does nothing.
 */
void IMB_free_gpu_textures(ImBuf *ibuf);

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
                   const blender::float3x3 &transform_matrix,
                   const rctf *src_crop);

blender::gpu::Texture *IMB_create_gpu_texture(const char *name,
                                              ImBuf *ibuf,
                                              bool use_high_bitdepth,
                                              bool use_premult);

blender::gpu::TextureFormat IMB_gpu_get_texture_format(const ImBuf *ibuf,
                                                       bool high_bitdepth,
                                                       bool use_grayscale);

bool IMB_gpu_get_compressed_format(const ImBuf *ibuf,
                                   blender::gpu::TextureFormat *r_texture_format);

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
blender::gpu::Texture *IMB_touch_gpu_texture(const char *name,
                                             ImBuf *ibuf,
                                             int w,
                                             int h,
                                             int layers,
                                             bool use_high_bitdepth,
                                             bool use_grayscale);

/**
 * Will update a #blender::gpu::Texture using the content of the #ImBuf. Only one layer will be
 * updated. Will resize the ibuf if needed. Z is the layer to update. Unused if the texture is 2D.
 */
void IMB_update_gpu_texture_sub(blender::gpu::Texture *tex,
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
/**
 * Left/right are always float.
 */
ImBuf *IMB_stereo3d_ImBuf(const ImageFormatData *im_format, ImBuf *ibuf_left, ImBuf *ibuf_right);
/**
 * Reading a stereo encoded ibuf (*left) and generating two ibufs from it (*left and *right).
 */
void IMB_ImBufFromStereo3d(const Stereo3dFormat *s3d,
                           ImBuf *ibuf_stereo3d,
                           ImBuf **r_ibuf_left,
                           ImBuf **r_ibuf_right);
