/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct BlendDataReader;
struct BlendWriter;
struct ID;
struct ImbFormatOptions;
struct ImageFormatData;
struct ImBuf;
struct Scene;

/* Init/Copy/Free */

void BKE_image_format_init(ImageFormatData *imf, const bool render);
void BKE_image_format_copy(ImageFormatData *imf_dst, const ImageFormatData *imf_src);
void BKE_image_format_free(ImageFormatData *imf);

/* Updates the color space of the given image format based on its image type. This can be used to
 * set a good default color space when the user changes the image type. See the implementation for
 * more information on the logic. */
void BKE_image_format_update_color_space_for_type(ImageFormatData *format);

void BKE_image_format_blend_read_data(BlendDataReader *reader, ImageFormatData *imf);
void BKE_image_format_blend_write(BlendWriter *writer, ImageFormatData *imf);

/* File Paths */

void BKE_image_path_from_imformat(char *filepath,
                                  const char *base,
                                  const char *relbase,
                                  int frame,
                                  const ImageFormatData *im_format,
                                  bool use_ext,
                                  bool use_frames,
                                  const char *suffix);
void BKE_image_path_from_imtype(char *filepath,
                                const char *base,
                                const char *relbase,
                                int frame,
                                char imtype,
                                bool use_ext,
                                bool use_frames,
                                const char *suffix);

/**
 * The number of extensions an image may have (`.jpg`, `.jpeg` for example).
 * Add 1 as the array is nil terminated.
 */
#define BKE_IMAGE_PATH_EXT_MAX 3
/**
 * Fill in an array of acceptable image extensions for the image format.
 *
 * \note In the case a file has no valid extension,
 * the first extension should be used (`r_ext[0]`).
 * \return the number of extensions assigned to `r_ext`, 0 for unsupported formats.
 */
int BKE_image_path_ext_from_imformat(const ImageFormatData *im_format,
                                     const char *r_ext[BKE_IMAGE_PATH_EXT_MAX]);
int BKE_image_path_ext_from_imtype(const char imtype, const char *r_ext[BKE_IMAGE_PATH_EXT_MAX]);

int BKE_image_path_ext_from_imformat_ensure(char *filepath,
                                            size_t filepath_maxncpy,
                                            const ImageFormatData *im_format);
int BKE_image_path_ext_from_imtype_ensure(char *filepath, size_t filepath_maxncpy, char imtype);

/* File Types */

#define IMA_CHAN_FLAG_BW 1
#define IMA_CHAN_FLAG_RGB 2
#define IMA_CHAN_FLAG_RGBA 4

char BKE_ftype_to_imtype(int ftype, const ImbFormatOptions *options);
int BKE_imtype_to_ftype(char imtype, ImbFormatOptions *r_options);

bool BKE_imtype_is_movie(char imtype);
bool BKE_imtype_supports_compress(char imtype);
bool BKE_imtype_supports_quality(char imtype);
bool BKE_imtype_requires_linear_float(char imtype);
char BKE_imtype_valid_channels(char imtype, bool write_file);
char BKE_imtype_valid_depths(char imtype);
char BKE_imtype_valid_depths_with_video(char imtype, const ID *owner_id);

/**
 * String is from command line `--render-format` argument,
 * keep in sync with `creator_args.cc` help info.
 */
char BKE_imtype_from_arg(const char *imtype_arg);

/* Conversion between #ImBuf settings. */

void BKE_image_format_from_imbuf(ImageFormatData *im_format, const ImBuf *imbuf);
void BKE_image_format_to_imbuf(ImBuf *ibuf, const ImageFormatData *imf);

bool BKE_image_format_is_byte(const ImageFormatData *imf);

/* Color Management */

void BKE_image_format_color_management_copy(ImageFormatData *imf, const ImageFormatData *imf_src);
void BKE_image_format_color_management_copy_from_scene(ImageFormatData *imf, const Scene *scene);

/* Image Output
 *
 * Initialize an image format that can be used for file writing, including
 * color management settings from the scene. */

void BKE_image_format_init_for_write(ImageFormatData *imf,
                                     const Scene *scene_src,
                                     const ImageFormatData *imf_src);
