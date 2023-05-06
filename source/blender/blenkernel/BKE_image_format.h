/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendWriter;
struct ImbFormatOptions;
struct ImageFormatData;
struct ImBuf;
struct Scene;

/* Init/Copy/Free */

void BKE_image_format_init(struct ImageFormatData *imf, const bool render);
void BKE_image_format_copy(struct ImageFormatData *imf_dst, const struct ImageFormatData *imf_src);
void BKE_image_format_free(struct ImageFormatData *imf);

void BKE_image_format_blend_read_data(struct BlendDataReader *reader, struct ImageFormatData *imf);
void BKE_image_format_blend_write(struct BlendWriter *writer, struct ImageFormatData *imf);

/* File Paths */

void BKE_image_path_from_imformat(char *filepath,
                                  const char *base,
                                  const char *relbase,
                                  int frame,
                                  const struct ImageFormatData *im_format,
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
int BKE_image_path_ensure_ext_from_imformat(char *filepath,
                                            const struct ImageFormatData *im_format);
int BKE_image_path_ensure_ext_from_imtype(char *filepath, char imtype);

/* File Types */

#define IMA_CHAN_FLAG_BW 1
#define IMA_CHAN_FLAG_RGB 2
#define IMA_CHAN_FLAG_RGBA 4

char BKE_ftype_to_imtype(int ftype, const struct ImbFormatOptions *options);
int BKE_imtype_to_ftype(char imtype, struct ImbFormatOptions *r_options);

bool BKE_imtype_is_movie(char imtype);
bool BKE_imtype_supports_zbuf(char imtype);
bool BKE_imtype_supports_compress(char imtype);
bool BKE_imtype_supports_quality(char imtype);
bool BKE_imtype_requires_linear_float(char imtype);
char BKE_imtype_valid_channels(char imtype, bool write_file);
char BKE_imtype_valid_depths(char imtype);

/**
 * String is from command line `--render-format` argument,
 * keep in sync with `creator_args.c` help info.
 */
char BKE_imtype_from_arg(const char *imtype_arg);

/* Conversion between ImBuf settings. */

void BKE_image_format_from_imbuf(struct ImageFormatData *im_format, const struct ImBuf *imbuf);
void BKE_image_format_to_imbuf(struct ImBuf *ibuf, const struct ImageFormatData *imf);

bool BKE_image_format_is_byte(const struct ImageFormatData *imf);

/* Color Management */

void BKE_image_format_color_management_copy(struct ImageFormatData *imf,
                                            const struct ImageFormatData *imf_src);
void BKE_image_format_color_management_copy_from_scene(struct ImageFormatData *imf,
                                                       const struct Scene *scene);

/* Image Output
 *
 * Initialize an image format that can be used for file writing, including
 * color management settings from the scene. */

void BKE_image_format_init_for_write(struct ImageFormatData *imf,
                                     const struct Scene *scene_src,
                                     const struct ImageFormatData *imf_src);

#ifdef __cplusplus
}
#endif
