/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "DNA_defaults.h"
#include "DNA_scene_types.h"

#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_util.hh"

#include "BKE_colortools.hh"
#include "BKE_image_format.hh"
#include "BKE_path_templates.hh"

namespace path_templates = blender::bke::path_templates;

/* Init/Copy/Free */

void BKE_image_format_init(ImageFormatData *imf)
{
  *imf = *DNA_struct_default_get(ImageFormatData);

  BKE_color_managed_display_settings_init(&imf->display_settings);

  BKE_color_managed_view_settings_init(&imf->view_settings, &imf->display_settings, "AgX");

  BKE_color_managed_colorspace_settings_init(&imf->linear_colorspace_settings);
}

void BKE_image_format_copy(ImageFormatData *imf_dst, const ImageFormatData *imf_src)
{
  memcpy(imf_dst, imf_src, sizeof(*imf_dst));
  BKE_color_managed_display_settings_copy(&imf_dst->display_settings, &imf_src->display_settings);
  BKE_color_managed_view_settings_copy(&imf_dst->view_settings, &imf_src->view_settings);
  BKE_color_managed_colorspace_settings_copy(&imf_dst->linear_colorspace_settings,
                                             &imf_src->linear_colorspace_settings);
}

void BKE_image_format_free(ImageFormatData *imf)
{
  BKE_color_managed_view_settings_free(&imf->view_settings);
}

void BKE_image_format_update_color_space_for_type(ImageFormatData *format)
{
  /* If the color space is set to a data space, this is probably the user's intention, so leave it
   * as is. */
  if (IMB_colormanagement_space_name_is_data(format->linear_colorspace_settings.name)) {
    return;
  }

  const bool image_requires_linear = BKE_imtype_requires_linear_float(format->imtype);
  const bool is_linear = IMB_colormanagement_space_name_is_scene_linear(
      format->linear_colorspace_settings.name);

  /* The color space is either not set or is linear but the image requires non-linear or vice
   * versa. So set to the default for the image type. */
  if (format->linear_colorspace_settings.name[0] == '\0' || image_requires_linear != is_linear) {
    const int role = image_requires_linear ? COLOR_ROLE_DEFAULT_FLOAT : COLOR_ROLE_DEFAULT_BYTE;
    const char *default_color_space = IMB_colormanagement_role_colorspace_name_get(role);
    STRNCPY_UTF8(format->linear_colorspace_settings.name, default_color_space);
  }
}

void BKE_image_format_blend_read_data(BlendDataReader *reader, ImageFormatData *imf)
{
  BKE_color_managed_view_settings_blend_read_data(reader, &imf->view_settings);
}

void BKE_image_format_blend_write(BlendWriter *writer, ImageFormatData *imf)
{
  BKE_color_managed_view_settings_blend_write(writer, &imf->view_settings);
}

void BKE_image_format_media_type_set(ImageFormatData *format,
                                     ID *owner_id,
                                     const MediaType media_type)
{
  format->media_type = media_type;

  switch (media_type) {
    case MEDIA_TYPE_IMAGE:
      if (!BKE_imtype_is_image(format->imtype)) {
        BKE_image_format_set(format, owner_id, R_IMF_IMTYPE_PNG);
      }
      break;
    case MEDIA_TYPE_MULTI_LAYER_IMAGE:
      if (!BKE_imtype_is_multi_layer_image(format->imtype)) {
        BKE_image_format_set(format, owner_id, R_IMF_IMTYPE_MULTILAYER);
      }
      break;
    case MEDIA_TYPE_VIDEO:
      if (!BKE_imtype_is_movie(format->imtype)) {
        BKE_image_format_set(format, owner_id, R_IMF_IMTYPE_FFMPEG);
      }
      break;
  }
}

void BKE_image_format_set(ImageFormatData *imf, ID *owner_id, const char imtype)
{
  imf->imtype = imtype;

  /* Update media type in case it doesn't match the new imtype. Note that normally, one would use
   * the BKE_image_format_media_type_set function to set the media type, but that function itself
   * calls this function to update the imtype, and while this wouldn't case recursion since the
   * imtype is already conforming, it is better to err on the side of caution and set the media
   * type manually. */
  if (BKE_imtype_is_image(imf->imtype)) {
    imf->media_type = MEDIA_TYPE_IMAGE;
  }
  else if (BKE_imtype_is_multi_layer_image(imf->imtype)) {
    imf->media_type = MEDIA_TYPE_MULTI_LAYER_IMAGE;
  }
  else if (BKE_imtype_is_movie(imf->imtype)) {
    imf->media_type = MEDIA_TYPE_VIDEO;
  }
  else {
    BLI_assert_unreachable();
  }

  const bool is_render = (owner_id && GS(owner_id->name) == ID_SCE);
  /* see note below on why this is */
  const char chan_flag = BKE_imtype_valid_channels(imf->imtype) |
                         (is_render ? IMA_CHAN_FLAG_BW : 0);

  /* ensure depth and color settings match */
  if ((imf->planes == R_IMF_PLANES_BW) && !(chan_flag & IMA_CHAN_FLAG_BW)) {
    imf->planes = R_IMF_PLANES_RGBA;
  }
  if ((imf->planes == R_IMF_PLANES_RGBA) && !(chan_flag & IMA_CHAN_FLAG_RGBA)) {
    imf->planes = R_IMF_PLANES_RGB;
  }

  /* ensure usable depth */
  {
    const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
    if ((imf->depth & depth_ok) == 0) {
      imf->depth = BKE_imtype_first_valid_depth(depth_ok);
    }
  }

  if (owner_id && GS(owner_id->name) == ID_SCE) {
    Scene *scene = reinterpret_cast<Scene *>(owner_id);
    RenderData *rd = &scene->r;
    MOV_validate_output_settings(rd, imf);
  }

  /* Verify `imf->views_format`. */
  if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
    if (imf->views_format == R_IMF_VIEWS_STEREO_3D) {
      imf->views_format = R_IMF_VIEWS_MULTIVIEW;
    }
  }
  else if (imf->imtype != R_IMF_IMTYPE_OPENEXR) {
    if (imf->views_format == R_IMF_VIEWS_MULTIVIEW) {
      imf->views_format = R_IMF_VIEWS_INDIVIDUAL;
    }
  }

  BKE_image_format_update_color_space_for_type(imf);
}

/* File Types */

int BKE_imtype_to_ftype(const char imtype, ImbFormatOptions *r_options)
{
  memset(r_options, 0, sizeof(*r_options));

  if (imtype == R_IMF_IMTYPE_TARGA) {
    return IMB_FTYPE_TGA;
  }
  if (imtype == R_IMF_IMTYPE_RAWTGA) {
    r_options->flag = RAWTGA;
    return IMB_FTYPE_TGA;
  }
  if (imtype == R_IMF_IMTYPE_IRIS) {
    return IMB_FTYPE_IRIS;
  }
  if (imtype == R_IMF_IMTYPE_RADHDR) {
    return IMB_FTYPE_RADHDR;
  }
  if (imtype == R_IMF_IMTYPE_PNG) {
    r_options->quality = 15;
    return IMB_FTYPE_PNG;
  }
  if (imtype == R_IMF_IMTYPE_DDS) {
    return IMB_FTYPE_DDS;
  }
  if (imtype == R_IMF_IMTYPE_BMP) {
    return IMB_FTYPE_BMP;
  }
  if (imtype == R_IMF_IMTYPE_TIFF) {
    return IMB_FTYPE_TIF;
  }
  if (ELEM(imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    r_options->quality = 90;
    return IMB_FTYPE_OPENEXR;
  }
#ifdef WITH_IMAGE_CINEON
  if (imtype == R_IMF_IMTYPE_CINEON) {
    return IMB_FTYPE_CINEON;
  }
  if (imtype == R_IMF_IMTYPE_DPX) {
    return IMB_FTYPE_DPX;
  }
#endif
#ifdef WITH_IMAGE_OPENJPEG
  if (imtype == R_IMF_IMTYPE_JP2) {
    r_options->flag |= JP2_JP2;
    r_options->quality = 90;
    return IMB_FTYPE_JP2;
  }
#endif
#ifdef WITH_IMAGE_WEBP
  if (imtype == R_IMF_IMTYPE_WEBP) {
    r_options->quality = 90;
    return IMB_FTYPE_WEBP;
  }
#endif

  r_options->quality = 90;
  return IMB_FTYPE_JPG;
}

char BKE_ftype_to_imtype(const int ftype, const ImbFormatOptions *options)
{
  if (ftype == IMB_FTYPE_NONE) {
    return R_IMF_IMTYPE_TARGA;
  }
  if (ftype == IMB_FTYPE_IRIS) {
    return R_IMF_IMTYPE_IRIS;
  }
  if (ftype == IMB_FTYPE_RADHDR) {
    return R_IMF_IMTYPE_RADHDR;
  }
  if (ftype == IMB_FTYPE_PNG) {
    return R_IMF_IMTYPE_PNG;
  }
  if (ftype == IMB_FTYPE_DDS) {
    return R_IMF_IMTYPE_DDS;
  }
  if (ftype == IMB_FTYPE_BMP) {
    return R_IMF_IMTYPE_BMP;
  }
  if (ftype == IMB_FTYPE_TIF) {
    return R_IMF_IMTYPE_TIFF;
  }
  if (ftype == IMB_FTYPE_OPENEXR) {
    return R_IMF_IMTYPE_OPENEXR;
  }
#ifdef WITH_IMAGE_CINEON
  if (ftype == IMB_FTYPE_CINEON) {
    return R_IMF_IMTYPE_CINEON;
  }
  if (ftype == IMB_FTYPE_DPX) {
    return R_IMF_IMTYPE_DPX;
  }
#endif
  if (ftype == IMB_FTYPE_TGA) {
    if (options && (options->flag & RAWTGA)) {
      return R_IMF_IMTYPE_RAWTGA;
    }

    return R_IMF_IMTYPE_TARGA;
  }
#ifdef WITH_IMAGE_OPENJPEG
  if (ftype == IMB_FTYPE_JP2) {
    return R_IMF_IMTYPE_JP2;
  }
#endif
#ifdef WITH_IMAGE_WEBP
  if (ftype == IMB_FTYPE_WEBP) {
    return R_IMF_IMTYPE_WEBP;
  }
#endif

  return R_IMF_IMTYPE_JPEG90;
}

bool BKE_imtype_is_image(const char imtype)
{
  return !BKE_imtype_is_multi_layer_image(imtype) && !BKE_imtype_is_movie(imtype);
}

bool BKE_imtype_is_multi_layer_image(const char imtype)
{
  return imtype == R_IMF_IMTYPE_MULTILAYER;
}

bool BKE_imtype_is_movie(const char imtype)
{
  return imtype == R_IMF_IMTYPE_FFMPEG;
}

bool BKE_imtype_supports_compress(const char imtype)
{
  switch (imtype) {
    case R_IMF_IMTYPE_PNG:
      return true;
  }
  return false;
}

bool BKE_imtype_supports_quality(const char imtype)
{
  switch (imtype) {
    case R_IMF_IMTYPE_JPEG90:
    case R_IMF_IMTYPE_JP2:
    case R_IMF_IMTYPE_WEBP:
      return true;
  }
  return false;
}

bool BKE_imtype_requires_linear_float(const char imtype)
{
  switch (imtype) {
    case R_IMF_IMTYPE_CINEON:
    case R_IMF_IMTYPE_DPX:
    case R_IMF_IMTYPE_RADHDR:
    case R_IMF_IMTYPE_OPENEXR:
    case R_IMF_IMTYPE_MULTILAYER:
      return true;
  }
  return false;
}

char BKE_imtype_valid_channels(const char imtype)
{
  char chan_flag = IMA_CHAN_FLAG_RGB; /* Assume all support RGB. */

  /* Alpha. */
  switch (imtype) {
    case R_IMF_IMTYPE_BMP:
    case R_IMF_IMTYPE_TARGA:
    case R_IMF_IMTYPE_RAWTGA:
    case R_IMF_IMTYPE_IRIS:
    case R_IMF_IMTYPE_PNG:
    case R_IMF_IMTYPE_TIFF:
    case R_IMF_IMTYPE_OPENEXR:
    case R_IMF_IMTYPE_MULTILAYER:
    case R_IMF_IMTYPE_DDS:
    case R_IMF_IMTYPE_JP2:
    case R_IMF_IMTYPE_DPX:
    case R_IMF_IMTYPE_WEBP:
      chan_flag |= IMA_CHAN_FLAG_RGBA;
      break;
  }

  /* BW. */
  switch (imtype) {
    case R_IMF_IMTYPE_BMP:
    case R_IMF_IMTYPE_PNG:
    case R_IMF_IMTYPE_JPEG90:
    case R_IMF_IMTYPE_TARGA:
    case R_IMF_IMTYPE_RAWTGA:
    case R_IMF_IMTYPE_TIFF:
    case R_IMF_IMTYPE_IRIS:
    case R_IMF_IMTYPE_OPENEXR:
      chan_flag |= IMA_CHAN_FLAG_BW;
      break;
  }

  return chan_flag;
}

char BKE_imtype_valid_depths(const char imtype)
{
  switch (imtype) {
    case R_IMF_IMTYPE_RADHDR:
      return R_IMF_CHAN_DEPTH_32;
    case R_IMF_IMTYPE_TIFF:
      return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_16;
    case R_IMF_IMTYPE_OPENEXR:
      return R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_32;
    case R_IMF_IMTYPE_MULTILAYER:
      return R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_32;
    /* NOTE: CINEON uses an unusual 10bits-LOG per channel. */
    case R_IMF_IMTYPE_DPX:
      return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_10 | R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16;
    case R_IMF_IMTYPE_CINEON:
      return R_IMF_CHAN_DEPTH_10;
    case R_IMF_IMTYPE_JP2:
      return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16;
    case R_IMF_IMTYPE_PNG:
      return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_16;
    /* Most formats are 8bit only. */
    default:
      return R_IMF_CHAN_DEPTH_8;
  }
}

char BKE_imtype_valid_depths_with_video(char imtype, const ID *owner_id)
{
  UNUSED_VARS(owner_id); /* Might be unused depending on build options. */

  int depths = BKE_imtype_valid_depths(imtype);
  /* Depending on video codec selected, valid color bit depths might vary. */
  if (imtype == R_IMF_IMTYPE_FFMPEG) {
    const bool is_render_out = (owner_id && GS(owner_id->name) == ID_SCE);
    if (is_render_out) {
      const Scene *scene = (const Scene *)owner_id;
      depths |= MOV_codec_valid_bit_depths(scene->r.ffcodecdata.codec_id_get());
    }
  }
  return depths;
}

char BKE_imtype_first_valid_depth(const char valid_depths)
{
  /* set first available depth */
  const char depth_ls[] = {
      R_IMF_CHAN_DEPTH_32,
      R_IMF_CHAN_DEPTH_24,
      R_IMF_CHAN_DEPTH_16,
      R_IMF_CHAN_DEPTH_12,
      R_IMF_CHAN_DEPTH_10,
      R_IMF_CHAN_DEPTH_8,
      R_IMF_CHAN_DEPTH_1,
      0,
  };
  for (int i = 0; depth_ls[i]; i++) {
    if (valid_depths & depth_ls[i]) {
      return depth_ls[i];
    }
  }
  return R_IMF_CHAN_DEPTH_8;
}

char BKE_imtype_from_arg(const char *imtype_arg)
{
  if (STREQ(imtype_arg, "TGA")) {
    return R_IMF_IMTYPE_TARGA;
  }
  if (STREQ(imtype_arg, "IRIS")) {
    return R_IMF_IMTYPE_IRIS;
  }
  if (STREQ(imtype_arg, "JPEG")) {
    return R_IMF_IMTYPE_JPEG90;
  }
  if (STREQ(imtype_arg, "RAWTGA")) {
    return R_IMF_IMTYPE_RAWTGA;
  }
  if (STREQ(imtype_arg, "PNG")) {
    return R_IMF_IMTYPE_PNG;
  }
  if (STREQ(imtype_arg, "BMP")) {
    return R_IMF_IMTYPE_BMP;
  }
  if (STREQ(imtype_arg, "HDR")) {
    return R_IMF_IMTYPE_RADHDR;
  }
  if (STREQ(imtype_arg, "TIFF")) {
    return R_IMF_IMTYPE_TIFF;
  }
#ifdef WITH_IMAGE_OPENEXR
  if (STREQ(imtype_arg, "OPEN_EXR")) {
    return R_IMF_IMTYPE_OPENEXR;
  }
  if (STREQ(imtype_arg, "OPEN_EXR_MULTILAYER")) {
    return R_IMF_IMTYPE_MULTILAYER;
  }
  if (STREQ(imtype_arg, "EXR")) {
    return R_IMF_IMTYPE_OPENEXR;
  }
  if (STREQ(imtype_arg, "MULTILAYER")) {
    return R_IMF_IMTYPE_MULTILAYER;
  }
#endif
#ifdef WITH_FFMPEG
  if (STREQ(imtype_arg, "FFMPEG")) {
    return R_IMF_IMTYPE_FFMPEG;
  }
#endif
#ifdef WITH_IMAGE_CINEON
  if (STREQ(imtype_arg, "CINEON")) {
    return R_IMF_IMTYPE_CINEON;
  }
  if (STREQ(imtype_arg, "DPX")) {
    return R_IMF_IMTYPE_DPX;
  }
#endif
#ifdef WITH_IMAGE_OPENJPEG
  if (STREQ(imtype_arg, "JP2")) {
    return R_IMF_IMTYPE_JP2;
  }
#endif
#ifdef WITH_IMAGE_WEBP
  if (STREQ(imtype_arg, "WEBP")) {
    return R_IMF_IMTYPE_WEBP;
  }
#endif

  return R_IMF_IMTYPE_INVALID;
}

/* File Paths */

static int image_path_ext_from_imformat_impl(const char imtype,
                                             const ImageFormatData *im_format,
                                             const char *r_ext[BKE_IMAGE_PATH_EXT_MAX])
{
  int ext_num = 0;
  (void)im_format; /* may be unused, depends on build options */

  if (imtype == R_IMF_IMTYPE_IRIS) {
    r_ext[ext_num++] = ".rgb";
  }
  else if (imtype == R_IMF_IMTYPE_IRIZ) {
    r_ext[ext_num++] = ".rgb";
  }
  else if (imtype == R_IMF_IMTYPE_RADHDR) {
    r_ext[ext_num++] = ".hdr";
  }
  else if (ELEM(imtype, R_IMF_IMTYPE_PNG, R_IMF_IMTYPE_FFMPEG)) {
    r_ext[ext_num++] = ".png";
  }
  else if (imtype == R_IMF_IMTYPE_DDS) {
    r_ext[ext_num++] = ".dds";
  }
  else if (ELEM(imtype, R_IMF_IMTYPE_TARGA, R_IMF_IMTYPE_RAWTGA)) {
    r_ext[ext_num++] = ".tga";
  }
  else if (imtype == R_IMF_IMTYPE_BMP) {
    r_ext[ext_num++] = ".bmp";
  }
  else if (imtype == R_IMF_IMTYPE_TIFF) {
    r_ext[ext_num++] = ".tif";
    r_ext[ext_num++] = ".tiff";
  }
  else if (imtype == R_IMF_IMTYPE_PSD) {
    r_ext[ext_num++] = ".psd";
  }
#ifdef WITH_IMAGE_OPENEXR
  else if (ELEM(imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    r_ext[ext_num++] = ".exr";
  }
#endif
#ifdef WITH_IMAGE_CINEON
  else if (imtype == R_IMF_IMTYPE_CINEON) {
    r_ext[ext_num++] = ".cin";
  }
  else if (imtype == R_IMF_IMTYPE_DPX) {
    r_ext[ext_num++] = ".dpx";
  }
#endif
#ifdef WITH_IMAGE_OPENJPEG
  else if (imtype == R_IMF_IMTYPE_JP2) {
    if (im_format) {
      if (im_format->jp2_codec == R_IMF_JP2_CODEC_JP2) {
        r_ext[ext_num++] = ".jp2";
      }
      else if (im_format->jp2_codec == R_IMF_JP2_CODEC_J2K) {
        r_ext[ext_num++] = ".j2c";
      }
      else {
        BLI_assert_msg(0, "Unsupported jp2 codec was specified in im_format->jp2_codec");
      }
    }
    else {
      r_ext[ext_num++] = ".jp2";
    }
  }
#endif
#ifdef WITH_IMAGE_WEBP
  else if (imtype == R_IMF_IMTYPE_WEBP) {
    r_ext[ext_num++] = ".webp";
  }
#endif
  else {
    /* Handles: #R_IMF_IMTYPE_JPEG90 etc. */
    r_ext[ext_num++] = ".jpg";
    r_ext[ext_num++] = ".jpeg";
  }
  BLI_assert(ext_num < BKE_IMAGE_PATH_EXT_MAX);
  r_ext[ext_num] = nullptr;
  return ext_num;
}

int BKE_image_path_ext_from_imformat(const ImageFormatData *im_format,
                                     const char *r_ext[BKE_IMAGE_PATH_EXT_MAX])
{
  return image_path_ext_from_imformat_impl(im_format->imtype, im_format, r_ext);
}

int BKE_image_path_ext_from_imtype(const char imtype, const char *r_ext[BKE_IMAGE_PATH_EXT_MAX])
{
  return image_path_ext_from_imformat_impl(imtype, nullptr, r_ext);
}

static bool do_ensure_image_extension(char *filepath,
                                      const size_t filepath_maxncpy,
                                      const char imtype,
                                      const ImageFormatData *im_format)
{
  const char *ext_array[BKE_IMAGE_PATH_EXT_MAX];
  int ext_array_num = image_path_ext_from_imformat_impl(imtype, im_format, ext_array);
  if (ext_array_num && !BLI_path_extension_check_array(filepath, ext_array)) {
    /* Removing *any* extension may remove part of the user defined name (if they include '.')
     * however in the case there is already a known image extension,
     * remove it to avoid`.png.tga`, for example. */
    if (BLI_path_extension_check_array(filepath, imb_ext_image)) {
      return BLI_path_extension_replace(filepath, filepath_maxncpy, ext_array[0]);
    }
    return BLI_path_extension_ensure(filepath, filepath_maxncpy, ext_array[0]);
  }

  return false;
}

int BKE_image_path_ext_from_imformat_ensure(char *filepath,
                                            const size_t filepath_maxncpy,
                                            const ImageFormatData *im_format)
{
  return do_ensure_image_extension(filepath, filepath_maxncpy, im_format->imtype, im_format);
}

int BKE_image_path_ext_from_imtype_ensure(char *filepath,
                                          const size_t filepath_maxncpy,
                                          const char imtype)
{
  return do_ensure_image_extension(filepath, filepath_maxncpy, imtype, nullptr);
}

static blender::Vector<path_templates::Error> do_makepicstring(
    char filepath[FILE_MAX],
    const char *base,
    const char *relbase,
    const path_templates::VariableMap *template_variables,
    int frame,
    const char imtype,
    const ImageFormatData *im_format,
    const bool use_ext,
    const bool use_frames,
    const char *suffix)
{
  if (filepath == nullptr) {
    return {};
  }
  BLI_strncpy(filepath, base, FILE_MAX - 10); /* weak assumption */

  if (template_variables) {
    const blender::Vector<path_templates::Error> variable_errors = BKE_path_apply_template(
        filepath, FILE_MAX, *template_variables);
    if (!variable_errors.is_empty()) {
      return variable_errors;
    }
  }

  BLI_path_abs(filepath, relbase);

  if (use_frames) {
    BLI_path_frame(filepath, FILE_MAX, frame, 4);
  }

  if (suffix) {
    BLI_path_suffix(filepath, FILE_MAX, suffix, "");
  }

  if (use_ext) {
    do_ensure_image_extension(filepath, FILE_MAX, imtype, im_format);
  }

  return {};
}

blender::Vector<path_templates::Error> BKE_image_path_from_imformat(
    char *filepath,
    const char *base,
    const char *relbase,
    const path_templates::VariableMap *template_variables,
    int frame,
    const ImageFormatData *im_format,
    const bool use_ext,
    const bool use_frames,
    const char *suffix)
{
  return do_makepicstring(filepath,
                          base,
                          relbase,
                          template_variables,
                          frame,
                          im_format->imtype,
                          im_format,
                          use_ext,
                          use_frames,
                          suffix);
}

blender::Vector<path_templates::Error> BKE_image_path_from_imtype(
    char *filepath,
    const char *base,
    const char *relbase,
    const path_templates::VariableMap *template_variables,
    int frame,
    const char imtype,
    const bool use_ext,
    const bool use_frames,
    const char *suffix)
{
  return do_makepicstring(filepath,
                          base,
                          relbase,
                          template_variables,
                          frame,
                          imtype,
                          nullptr,
                          use_ext,
                          use_frames,
                          suffix);
}

/* ImBuf Conversion */

void BKE_image_format_to_imbuf(ImBuf *ibuf, const ImageFormatData *imf)
{
  /* Write to ImBuf in preparation for file writing. */
  char imtype = imf->imtype;
  char compress = imf->compress;
  char quality = imf->quality;

  /* initialize all from image format */
  ibuf->foptions.flag = 0;

  if (imtype == R_IMF_IMTYPE_IRIS) {
    ibuf->ftype = IMB_FTYPE_IRIS;
  }
  else if (imtype == R_IMF_IMTYPE_RADHDR) {
    ibuf->ftype = IMB_FTYPE_RADHDR;
  }
  else if (ELEM(imtype, R_IMF_IMTYPE_PNG, R_IMF_IMTYPE_FFMPEG)) {
    ibuf->ftype = IMB_FTYPE_PNG;

    if (imtype == R_IMF_IMTYPE_PNG) {
      if (imf->depth == R_IMF_CHAN_DEPTH_16) {
        ibuf->foptions.flag |= PNG_16BIT;
      }

      ibuf->foptions.quality = compress;
    }
  }
  else if (imtype == R_IMF_IMTYPE_DDS) {
    ibuf->ftype = IMB_FTYPE_DDS;
  }
  else if (imtype == R_IMF_IMTYPE_BMP) {
    ibuf->ftype = IMB_FTYPE_BMP;
  }
  else if (imtype == R_IMF_IMTYPE_TIFF) {
    ibuf->ftype = IMB_FTYPE_TIF;

    if (imf->depth == R_IMF_CHAN_DEPTH_16) {
      ibuf->foptions.flag |= TIF_16BIT;
    }
    if (imf->tiff_codec == R_IMF_TIFF_CODEC_NONE) {
      ibuf->foptions.flag |= TIF_COMPRESS_NONE;
    }
    else if (imf->tiff_codec == R_IMF_TIFF_CODEC_DEFLATE) {
      ibuf->foptions.flag |= TIF_COMPRESS_DEFLATE;
    }
    else if (imf->tiff_codec == R_IMF_TIFF_CODEC_LZW) {
      ibuf->foptions.flag |= TIF_COMPRESS_LZW;
    }
    else if (imf->tiff_codec == R_IMF_TIFF_CODEC_PACKBITS) {
      ibuf->foptions.flag |= TIF_COMPRESS_PACKBITS;
    }
  }
#ifdef WITH_IMAGE_OPENEXR
  else if (ELEM(imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
    ibuf->ftype = IMB_FTYPE_OPENEXR;
    if (imf->depth == R_IMF_CHAN_DEPTH_16) {
      ibuf->foptions.flag |= OPENEXR_HALF;
    }
    ibuf->foptions.flag |= (imf->exr_codec & OPENEXR_CODEC_MASK);
    ibuf->foptions.quality = quality;
    if (imf->exr_flag & R_IMF_EXR_FLAG_MULTIPART) {
      ibuf->foptions.flag |= OPENEXR_MULTIPART;
    }
  }
#endif
#ifdef WITH_IMAGE_CINEON
  else if (imtype == R_IMF_IMTYPE_CINEON) {
    ibuf->ftype = IMB_FTYPE_CINEON;
    if (imf->cineon_flag & R_IMF_CINEON_FLAG_LOG) {
      ibuf->foptions.flag |= CINEON_LOG;
    }
    if (imf->depth == R_IMF_CHAN_DEPTH_16) {
      ibuf->foptions.flag |= CINEON_16BIT;
    }
    else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
      ibuf->foptions.flag |= CINEON_12BIT;
    }
    else if (imf->depth == R_IMF_CHAN_DEPTH_10) {
      ibuf->foptions.flag |= CINEON_10BIT;
    }
  }
  else if (imtype == R_IMF_IMTYPE_DPX) {
    ibuf->ftype = IMB_FTYPE_DPX;
    if (imf->cineon_flag & R_IMF_CINEON_FLAG_LOG) {
      ibuf->foptions.flag |= CINEON_LOG;
    }
    if (imf->depth == R_IMF_CHAN_DEPTH_16) {
      ibuf->foptions.flag |= CINEON_16BIT;
    }
    else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
      ibuf->foptions.flag |= CINEON_12BIT;
    }
    else if (imf->depth == R_IMF_CHAN_DEPTH_10) {
      ibuf->foptions.flag |= CINEON_10BIT;
    }
  }
#endif
  else if (imtype == R_IMF_IMTYPE_TARGA) {
    ibuf->ftype = IMB_FTYPE_TGA;
  }
  else if (imtype == R_IMF_IMTYPE_RAWTGA) {
    ibuf->ftype = IMB_FTYPE_TGA;
    ibuf->foptions.flag = RAWTGA;
  }
#ifdef WITH_IMAGE_OPENJPEG
  else if (imtype == R_IMF_IMTYPE_JP2) {
    if (quality < 10) {
      quality = 90;
    }
    ibuf->ftype = IMB_FTYPE_JP2;
    ibuf->foptions.quality = quality;

    if (imf->depth == R_IMF_CHAN_DEPTH_16) {
      ibuf->foptions.flag |= JP2_16BIT;
    }
    else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
      ibuf->foptions.flag |= JP2_12BIT;
    }

    if (imf->jp2_flag & R_IMF_JP2_FLAG_YCC) {
      ibuf->foptions.flag |= JP2_YCC;
    }

    if (imf->jp2_flag & R_IMF_JP2_FLAG_CINE_PRESET) {
      ibuf->foptions.flag |= JP2_CINE;
      if (imf->jp2_flag & R_IMF_JP2_FLAG_CINE_48) {
        ibuf->foptions.flag |= JP2_CINE_48FPS;
      }
    }

    if (imf->jp2_codec == R_IMF_JP2_CODEC_JP2) {
      ibuf->foptions.flag |= JP2_JP2;
    }
    else if (imf->jp2_codec == R_IMF_JP2_CODEC_J2K) {
      ibuf->foptions.flag |= JP2_J2K;
    }
    else {
      BLI_assert_msg(0, "Unsupported jp2 codec was specified in im_format->jp2_codec");
    }
  }
#endif
#ifdef WITH_IMAGE_WEBP
  else if (imtype == R_IMF_IMTYPE_WEBP) {
    ibuf->ftype = IMB_FTYPE_WEBP;
    ibuf->foptions.quality = quality;
  }
#endif
  else {
    /* #R_IMF_IMTYPE_JPEG90, etc. default to JPEG. */
    if (quality < 10) {
      quality = 90;
    }
    ibuf->ftype = IMB_FTYPE_JPG;
    ibuf->foptions.quality = quality;
  }
}

static char imtype_best_depth(const ImBuf *ibuf, const char imtype)
{
  const char depth_ok = BKE_imtype_valid_depths(imtype);

  if (ibuf->float_buffer.data) {
    if (depth_ok & R_IMF_CHAN_DEPTH_32) {
      return R_IMF_CHAN_DEPTH_32;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_24) {
      return R_IMF_CHAN_DEPTH_24;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_16) {
      return R_IMF_CHAN_DEPTH_16;
    }
    if (depth_ok & R_IMF_CHAN_DEPTH_12) {
      return R_IMF_CHAN_DEPTH_12;
    }
    return R_IMF_CHAN_DEPTH_8;
  }

  if (depth_ok & R_IMF_CHAN_DEPTH_8) {
    return R_IMF_CHAN_DEPTH_8;
  }
  if (depth_ok & R_IMF_CHAN_DEPTH_12) {
    return R_IMF_CHAN_DEPTH_12;
  }
  if (depth_ok & R_IMF_CHAN_DEPTH_16) {
    return R_IMF_CHAN_DEPTH_16;
  }
  if (depth_ok & R_IMF_CHAN_DEPTH_24) {
    return R_IMF_CHAN_DEPTH_24;
  }
  if (depth_ok & R_IMF_CHAN_DEPTH_32) {
    return R_IMF_CHAN_DEPTH_32;
  }
  return R_IMF_CHAN_DEPTH_8; /* fallback, should not get here */
}

void BKE_image_format_from_imbuf(ImageFormatData *im_format, const ImBuf *imbuf)
{
  /* Read from ImBuf after file read. */
  int ftype = imbuf->ftype;
  int custom_flags = imbuf->foptions.flag;
  char quality = imbuf->foptions.quality;
  bool is_depth_set = false;

  BKE_image_format_init(im_format);
  im_format->media_type = MEDIA_TYPE_IMAGE;

  /* file type */
  if (ftype == IMB_FTYPE_IRIS) {
    im_format->imtype = R_IMF_IMTYPE_IRIS;
  }
  else if (ftype == IMB_FTYPE_RADHDR) {
    im_format->imtype = R_IMF_IMTYPE_RADHDR;
  }
  else if (ftype == IMB_FTYPE_PNG) {
    im_format->imtype = R_IMF_IMTYPE_PNG;

    if (custom_flags & PNG_16BIT) {
      im_format->depth = R_IMF_CHAN_DEPTH_16;
      is_depth_set = true;
    }

    im_format->compress = quality;
  }
  else if (ftype == IMB_FTYPE_DDS) {
    im_format->imtype = R_IMF_IMTYPE_DDS;
  }
  else if (ftype == IMB_FTYPE_BMP) {
    im_format->imtype = R_IMF_IMTYPE_BMP;
  }
  else if (ftype == IMB_FTYPE_TIF) {
    im_format->imtype = R_IMF_IMTYPE_TIFF;
    if (custom_flags & TIF_16BIT) {
      im_format->depth = R_IMF_CHAN_DEPTH_16;
      is_depth_set = true;
    }
    if (custom_flags & TIF_COMPRESS_NONE) {
      im_format->tiff_codec = R_IMF_TIFF_CODEC_NONE;
    }
    if (custom_flags & TIF_COMPRESS_DEFLATE) {
      im_format->tiff_codec = R_IMF_TIFF_CODEC_DEFLATE;
    }
    if (custom_flags & TIF_COMPRESS_LZW) {
      im_format->tiff_codec = R_IMF_TIFF_CODEC_LZW;
    }
    if (custom_flags & TIF_COMPRESS_PACKBITS) {
      im_format->tiff_codec = R_IMF_TIFF_CODEC_PACKBITS;
    }
  }

#ifdef WITH_IMAGE_OPENEXR
  else if (ftype == IMB_FTYPE_OPENEXR) {
    im_format->imtype = R_IMF_IMTYPE_OPENEXR;
    char exr_codec = custom_flags & OPENEXR_CODEC_MASK;
    if (custom_flags & OPENEXR_HALF) {
      im_format->depth = R_IMF_CHAN_DEPTH_16;
      is_depth_set = true;
    }
    else if (ELEM(exr_codec, R_IMF_EXR_CODEC_B44, R_IMF_EXR_CODEC_B44A)) {
      /* B44 and B44A are only selectable for half precision images, default to ZIP compression */
      exr_codec = R_IMF_EXR_CODEC_ZIP;
    }
    if (exr_codec < R_IMF_EXR_CODEC_MAX) {
      im_format->exr_codec = exr_codec;
    }
    if (custom_flags & OPENEXR_MULTIPART) {
      im_format->exr_flag |= R_IMF_EXR_FLAG_MULTIPART;
    }
  }
#endif

#ifdef WITH_IMAGE_CINEON
  else if (ftype == IMB_FTYPE_CINEON) {
    im_format->imtype = R_IMF_IMTYPE_CINEON;
  }
  else if (ftype == IMB_FTYPE_DPX) {
    im_format->imtype = R_IMF_IMTYPE_DPX;
  }
#endif
  else if (ftype == IMB_FTYPE_TGA) {
    if (custom_flags & RAWTGA) {
      im_format->imtype = R_IMF_IMTYPE_RAWTGA;
    }
    else {
      im_format->imtype = R_IMF_IMTYPE_TARGA;
    }
  }
#ifdef WITH_IMAGE_OPENJPEG
  else if (ftype == IMB_FTYPE_JP2) {
    im_format->imtype = R_IMF_IMTYPE_JP2;
    im_format->quality = quality;

    if (custom_flags & JP2_16BIT) {
      im_format->depth = R_IMF_CHAN_DEPTH_16;
      is_depth_set = true;
    }
    else if (custom_flags & JP2_12BIT) {
      im_format->depth = R_IMF_CHAN_DEPTH_12;
      is_depth_set = true;
    }

    if (custom_flags & JP2_YCC) {
      im_format->jp2_flag |= R_IMF_JP2_FLAG_YCC;
    }

    if (custom_flags & JP2_CINE) {
      im_format->jp2_flag |= R_IMF_JP2_FLAG_CINE_PRESET;
      if (custom_flags & JP2_CINE_48FPS) {
        im_format->jp2_flag |= R_IMF_JP2_FLAG_CINE_48;
      }
    }

    if (custom_flags & JP2_JP2) {
      im_format->jp2_codec = R_IMF_JP2_CODEC_JP2;
    }
    else if (custom_flags & JP2_J2K) {
      im_format->jp2_codec = R_IMF_JP2_CODEC_J2K;
    }
    else {
      BLI_assert_msg(0, "Unsupported jp2 codec was specified in file type");
    }
  }
#endif
#ifdef WITH_IMAGE_WEBP
  else if (ftype == IMB_FTYPE_WEBP) {
    im_format->imtype = R_IMF_IMTYPE_WEBP;
    im_format->quality = quality;
  }
#endif

  else {
    im_format->imtype = R_IMF_IMTYPE_JPEG90;
    im_format->quality = quality;
  }

  /* Default depth, accounting for float buffer and format support */
  if (!is_depth_set) {
    im_format->depth = imtype_best_depth(imbuf, im_format->imtype);
  }

  /* planes */
  im_format->planes = imbuf->planes;
}

bool BKE_image_format_is_byte(const ImageFormatData *imf)
{
  return (imf->depth == R_IMF_CHAN_DEPTH_8) && (BKE_imtype_valid_depths(imf->imtype) & imf->depth);
}

/* Color Management */

void BKE_image_format_color_management_copy(ImageFormatData *imf, const ImageFormatData *imf_src)
{
  BKE_color_managed_view_settings_free(&imf->view_settings);

  BKE_color_managed_display_settings_copy(&imf->display_settings, &imf_src->display_settings);
  BKE_color_managed_view_settings_copy(&imf->view_settings, &imf_src->view_settings);
  BKE_color_managed_colorspace_settings_copy(&imf->linear_colorspace_settings,
                                             &imf_src->linear_colorspace_settings);
}

void BKE_image_format_color_management_copy_from_scene(ImageFormatData *imf, const Scene *scene)
{
  BKE_color_managed_view_settings_free(&imf->view_settings);

  BKE_color_managed_display_settings_copy(&imf->display_settings, &scene->display_settings);
  BKE_color_managed_view_settings_copy(&imf->view_settings, &scene->view_settings);
  STRNCPY_UTF8(imf->linear_colorspace_settings.name,
               IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR));
}

/* Output */

void BKE_image_format_init_for_write(ImageFormatData *imf,
                                     const Scene *scene_src,
                                     const ImageFormatData *imf_src,
                                     const bool allow_video)
{
  *imf = (imf_src) ? *imf_src : scene_src->r.im_format;

  /* For image saving we can not have use media type video. */
  if (!allow_video) {
    if (scene_src && imf->media_type == MEDIA_TYPE_VIDEO) {
      BKE_image_format_media_type_set(imf, const_cast<ID *>(&scene_src->id), MEDIA_TYPE_IMAGE);
    }
  }

  if (imf_src && imf_src->color_management == R_IMF_COLOR_MANAGEMENT_OVERRIDE) {
    /* Use settings specific to one node, image save operation, etc. */
    BKE_color_managed_display_settings_copy(&imf->display_settings, &imf_src->display_settings);
    BKE_color_managed_view_settings_copy(&imf->view_settings, &imf_src->view_settings);
    BKE_color_managed_colorspace_settings_copy(&imf->linear_colorspace_settings,
                                               &imf_src->linear_colorspace_settings);
  }
  else if (scene_src->r.im_format.color_management == R_IMF_COLOR_MANAGEMENT_OVERRIDE) {
    /* Use scene settings specific to render output. */
    BKE_color_managed_display_settings_copy(&imf->display_settings,
                                            &scene_src->r.im_format.display_settings);
    BKE_color_managed_view_settings_copy(&imf->view_settings,
                                         &scene_src->r.im_format.view_settings);
    BKE_color_managed_colorspace_settings_copy(&imf->linear_colorspace_settings,
                                               &scene_src->r.im_format.linear_colorspace_settings);
  }
  else {
    /* Use general scene settings also used for display. */
    BKE_color_managed_display_settings_copy(&imf->display_settings, &scene_src->display_settings);
    BKE_color_managed_view_settings_copy(&imf->view_settings, &scene_src->view_settings);
    STRNCPY_UTF8(imf->linear_colorspace_settings.name,
                 IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR));
  }
}
