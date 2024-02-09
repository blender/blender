/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cerrno>
#include <cstring>

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_image_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_openexr.hh"

#include "BKE_colortools.hh"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_image_save.h"
#include "BKE_main.hh"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"

using blender::Vector;

static char imtype_best_depth(ImBuf *ibuf, const char imtype)
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

bool BKE_image_save_options_init(ImageSaveOptions *opts,
                                 Main *bmain,
                                 Scene *scene,
                                 Image *ima,
                                 ImageUser *iuser,
                                 const bool guess_path,
                                 const bool save_as_render)
{
  /* For saving a tiled image we need an iuser, so use a local one if there isn't already one. */
  ImageUser save_iuser;
  if (iuser == nullptr) {
    BKE_imageuser_default(&save_iuser);
    iuser = &save_iuser;
    iuser->scene = scene;
  }

  memset(opts, 0, sizeof(*opts));

  opts->bmain = bmain;
  opts->scene = scene;
  opts->save_as_render = ima->source == IMA_SRC_VIEWER || save_as_render;

  BKE_image_format_init(&opts->im_format, false);

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);

  if (ibuf) {
    Scene *scene = opts->scene;
    bool is_depth_set = false;
    const char *ima_colorspace = ima->colorspace_settings.name;

    if (opts->save_as_render) {
      /* Render/compositor output or user chose to save with render settings. */
      BKE_image_format_init_for_write(&opts->im_format, scene, nullptr);
      is_depth_set = true;
      if (!BKE_image_is_multiview(ima)) {
        /* In case multiview is disabled,
         * render settings would be invalid for render result in this area. */
        opts->im_format.stereo3d_format = *ima->stereo3d_format;
        opts->im_format.views_format = ima->views_format;
      }
    }
    else {
      BKE_image_format_from_imbuf(&opts->im_format, ibuf);
      if (ima->source == IMA_SRC_GENERATED &&
          !IMB_colormanagement_space_name_is_data(ima_colorspace))
      {
        ima_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);
      }

      /* use the multiview image settings as the default */
      opts->im_format.stereo3d_format = *ima->stereo3d_format;
      opts->im_format.views_format = ima->views_format;

      /* Render output: colorspace from render settings. */
      BKE_image_format_color_management_copy_from_scene(&opts->im_format, scene);
    }

    /* Default to saving in the same colorspace as the image setting. */
    if (!opts->save_as_render) {
      STRNCPY(opts->im_format.linear_colorspace_settings.name, ima_colorspace);
    }

    opts->im_format.color_management = R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE;

    /* Compute filepath, but don't resolve multiview and UDIM which are handled
     * by the image saving code itself. */
    BKE_image_user_file_path_ex(bmain, iuser, ima, opts->filepath, false, false);

    /* sanitize all settings */

    /* unlikely but just in case */
    if (ELEM(opts->im_format.planes, R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA) == 0) {
      opts->im_format.planes = R_IMF_PLANES_RGBA;
    }

    /* depth, account for float buffer and format support */
    if (is_depth_set == false) {
      opts->im_format.depth = imtype_best_depth(ibuf, opts->im_format.imtype);
    }

    /* some formats don't use quality so fallback to scenes quality */
    if (opts->im_format.quality == 0) {
      opts->im_format.quality = scene->r.im_format.quality;
    }

    /* check for empty path */
    if (guess_path && opts->filepath[0] == 0) {
      const bool is_prev_save = !STREQ(G.filepath_last_image, "//");
      if (opts->save_as_render) {
        if (is_prev_save) {
          STRNCPY(opts->filepath, G.filepath_last_image);
        }
        else {
          BLI_path_join(opts->filepath, sizeof(opts->filepath), "//", DATA_("untitled"));
          BLI_path_abs(opts->filepath, BKE_main_blendfile_path(bmain));
        }
      }
      else {
        BLI_path_join(opts->filepath, sizeof(opts->filepath), "//", ima->id.name + 2);
        BLI_path_make_safe(opts->filepath);
        BLI_path_abs(opts->filepath,
                     is_prev_save ? G.filepath_last_image : BKE_main_blendfile_path(bmain));
      }

      /* append UDIM marker if not present */
      if (ima->source == IMA_SRC_TILED && strstr(opts->filepath, "<UDIM>") == nullptr) {
        int len = strlen(opts->filepath);
        STR_CONCAT(opts->filepath, len, ".<UDIM>");
      }
    }
  }

  /* Copy for detecting UI changes. */
  opts->prev_save_as_render = opts->save_as_render;
  opts->prev_imtype = opts->im_format.imtype;

  BKE_image_release_ibuf(ima, ibuf, lock);

  return (ibuf != nullptr);
}

void BKE_image_save_options_update(ImageSaveOptions *opts, const Image *image)
{
  /* Auto update color space when changing save as render and file type. */
  if (opts->save_as_render) {
    if (!opts->prev_save_as_render) {
      if (ELEM(image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
        BKE_image_format_init_for_write(&opts->im_format, opts->scene, nullptr);
      }
      else {
        BKE_image_format_color_management_copy_from_scene(&opts->im_format, opts->scene);
      }
    }
  }
  else {
    if (opts->prev_save_as_render) {
      /* Copy colorspace from image settings. */
      BKE_color_managed_colorspace_settings_copy(&opts->im_format.linear_colorspace_settings,
                                                 &image->colorspace_settings);
    }
    else if (opts->im_format.imtype != opts->prev_imtype &&
             !IMB_colormanagement_space_name_is_data(
                 opts->im_format.linear_colorspace_settings.name))
    {
      const bool linear_float_output = BKE_imtype_requires_linear_float(opts->im_format.imtype);

      /* TODO: detect if the colorspace is linear, not just equal to scene linear. */
      const bool is_linear = IMB_colormanagement_space_name_is_scene_linear(
          opts->im_format.linear_colorspace_settings.name);

      /* If changing to a linear float or byte format, ensure we have a compatible color space. */
      if (linear_float_output && !is_linear) {
        STRNCPY(opts->im_format.linear_colorspace_settings.name,
                IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_FLOAT));
      }
      else if (!linear_float_output && is_linear) {
        STRNCPY(opts->im_format.linear_colorspace_settings.name,
                IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE));
      }
    }
  }

  opts->prev_save_as_render = opts->save_as_render;
  opts->prev_imtype = opts->im_format.imtype;
}

void BKE_image_save_options_free(ImageSaveOptions *opts)
{
  BKE_image_format_free(&opts->im_format);
}

static void image_save_update_filepath(Image *ima,
                                       const char *filepath,
                                       const ImageSaveOptions *opts)
{
  if (opts->do_newpath) {
    STRNCPY(ima->filepath, filepath);

    /* only image path, never ibuf */
    if (opts->relative) {
      const char *relbase = ID_BLEND_PATH(opts->bmain, &ima->id);
      BLI_path_rel(ima->filepath, relbase); /* only after saving */
    }
  }
}

static void image_save_post(ReportList *reports,
                            Image *ima,
                            ImBuf *ibuf,
                            int ok,
                            const ImageSaveOptions *opts,
                            const bool save_copy,
                            const char *filepath,
                            bool *r_colorspace_changed)
{
  if (!ok) {
    BKE_reportf(reports, RPT_ERROR, "Could not write image: %s", strerror(errno));
    return;
  }

  if (save_copy) {
    return;
  }

  if (opts->do_newpath) {
    STRNCPY(ibuf->filepath, filepath);
  }

  /* The tiled image code-path must call this on its own. */
  if (ima->source != IMA_SRC_TILED) {
    image_save_update_filepath(ima, filepath, opts);
  }

  ibuf->userflags &= ~IB_BITMAPDIRTY;

  /* change type? */
  if (ima->type == IMA_TYPE_R_RESULT) {
    ima->type = IMA_TYPE_IMAGE;

    /* workaround to ensure the render result buffer is no longer used
     * by this image, otherwise can crash when a new render result is
     * created. */
    imb_freerectImBuf(ibuf);
    imb_freerectfloatImBuf(ibuf);
  }
  if (ELEM(ima->source, IMA_SRC_GENERATED, IMA_SRC_VIEWER)) {
    ima->source = IMA_SRC_FILE;
    ima->type = IMA_TYPE_IMAGE;
    ImageTile *base_tile = BKE_image_get_tile(ima, 0);
    base_tile->gen_flag &= ~IMA_GEN_TILE;
  }

  /* Update image file color space when saving to another color space. */
  const bool linear_float_output = BKE_imtype_requires_linear_float(opts->im_format.imtype);

  if (!opts->save_as_render || linear_float_output) {
    if (opts->im_format.linear_colorspace_settings.name[0] &&
        !BKE_color_managed_colorspace_settings_equals(&ima->colorspace_settings,
                                                      &opts->im_format.linear_colorspace_settings))
    {
      BKE_color_managed_colorspace_settings_copy(&ima->colorspace_settings,
                                                 &opts->im_format.linear_colorspace_settings);
      *r_colorspace_changed = true;
    }
  }
}

static void imbuf_save_post(ImBuf *ibuf, ImBuf *colormanaged_ibuf)
{
  if (colormanaged_ibuf != ibuf) {
    /* This guys might be modified by image buffer write functions,
     * need to copy them back from color managed image buffer to an
     * original one, so file type of image is being properly updated.
     */
    ibuf->ftype = colormanaged_ibuf->ftype;
    ibuf->foptions = colormanaged_ibuf->foptions;
    ibuf->planes = colormanaged_ibuf->planes;

    IMB_freeImBuf(colormanaged_ibuf);
  }
}

/**
 * \return success.
 * \note `ima->filepath` and `ibuf->filepath` will reference the same path
 * (although `ima->filepath` may be blend-file relative).
 * \note for multi-view the first `ibuf` is important to get the settings.
 */
static bool image_save_single(ReportList *reports,
                              Image *ima,
                              ImageUser *iuser,
                              const ImageSaveOptions *opts,
                              bool *r_colorspace_changed)
{
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  RenderResult *rr = nullptr;
  bool ok = false;

  if (ibuf == nullptr || (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr))
  {
    BKE_image_release_ibuf(ima, ibuf, lock);
    return ok;
  }

  ImBuf *colormanaged_ibuf = nullptr;
  const bool save_copy = opts->save_copy;
  const bool save_as_render = opts->save_as_render;
  const ImageFormatData *imf = &opts->im_format;

  if (ima->type == IMA_TYPE_R_RESULT) {
    /* enforce user setting for RGB or RGBA, but skip BW */
    if (opts->im_format.planes == R_IMF_PLANES_RGBA) {
      ibuf->planes = R_IMF_PLANES_RGBA;
    }
    else if (opts->im_format.planes == R_IMF_PLANES_RGB) {
      ibuf->planes = R_IMF_PLANES_RGB;
    }
  }
  else {
    /* TODO: better solution, if a 24bit image is painted onto it may contain alpha. */
    if ((opts->im_format.planes == R_IMF_PLANES_RGBA) &&
        /* it has been painted onto */
        (ibuf->userflags & IB_BITMAPDIRTY))
    {
      /* checks each pixel, not ideal */
      ibuf->planes = BKE_imbuf_alpha_test(ibuf) ? R_IMF_PLANES_RGBA : R_IMF_PLANES_RGB;
    }
  }

  /* we need renderresult for exr and rendered multiview */
  rr = BKE_image_acquire_renderresult(opts->scene, ima);
  const bool is_mono = rr ? BLI_listbase_count_at_most(&rr->views, 2) < 2 :
                            BLI_listbase_count_at_most(&ima->views, 2) < 2;
  const bool is_exr_rr = rr && ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER) &&
                         RE_HasFloatPixels(rr);
  const bool is_multilayer = is_exr_rr && (imf->imtype == R_IMF_IMTYPE_MULTILAYER);
  const int layer = (iuser && !is_multilayer) ? iuser->layer : -1;

  /* error handling */
  if (rr == nullptr) {
    if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
      BKE_report(reports, RPT_ERROR, "Did not write, no Multilayer Image");
      BKE_image_release_ibuf(ima, ibuf, lock);
      return ok;
    }
  }
  else {
    if (imf->views_format == R_IMF_VIEWS_STEREO_3D) {
      if (!BKE_image_is_stereo(ima)) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    R"(Did not write, the image doesn't have a "%s" and "%s" views)",
                    STEREO_LEFT_NAME,
                    STEREO_RIGHT_NAME);
        BKE_image_release_ibuf(ima, ibuf, lock);
        BKE_image_release_renderresult(opts->scene, ima);
        return ok;
      }

      /* It shouldn't ever happen. */
      if ((BLI_findstring(&rr->views, STEREO_LEFT_NAME, offsetof(RenderView, name)) == nullptr) ||
          (BLI_findstring(&rr->views, STEREO_RIGHT_NAME, offsetof(RenderView, name)) == nullptr))
      {
        BKE_reportf(reports,
                    RPT_ERROR,
                    R"(Did not write, the image doesn't have a "%s" and "%s" views)",
                    STEREO_LEFT_NAME,
                    STEREO_RIGHT_NAME);
        BKE_image_release_ibuf(ima, ibuf, lock);
        BKE_image_release_renderresult(opts->scene, ima);
        return ok;
      }
    }
    BKE_imbuf_stamp_info(rr, ibuf);
  }

  /* fancy multiview OpenEXR */
  if (imf->views_format == R_IMF_VIEWS_MULTIVIEW && is_exr_rr) {
    /* save render result */
    ok = BKE_image_render_write_exr(
        reports, rr, opts->filepath, imf, save_as_render, nullptr, layer);
    image_save_post(reports, ima, ibuf, ok, opts, true, opts->filepath, r_colorspace_changed);
    BKE_image_release_ibuf(ima, ibuf, lock);
  }
  /* regular mono pipeline */
  else if (is_mono) {
    if (is_exr_rr) {
      ok = BKE_image_render_write_exr(
          reports, rr, opts->filepath, imf, save_as_render, nullptr, layer);
    }
    else {
      colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, true, imf);
      ok = BKE_imbuf_write_as(colormanaged_ibuf, opts->filepath, imf, save_copy);
      imbuf_save_post(ibuf, colormanaged_ibuf);
    }
    image_save_post(reports,
                    ima,
                    ibuf,
                    ok,
                    opts,
                    (is_exr_rr ? true : save_copy),
                    opts->filepath,
                    r_colorspace_changed);
    BKE_image_release_ibuf(ima, ibuf, lock);
  }
  /* individual multiview images */
  else if (imf->views_format == R_IMF_VIEWS_INDIVIDUAL) {
    uchar planes = ibuf->planes;
    const int totviews = (rr ? BLI_listbase_count(&rr->views) : BLI_listbase_count(&ima->views));

    if (!is_exr_rr) {
      BKE_image_release_ibuf(ima, ibuf, lock);
    }

    for (int i = 0; i < totviews; i++) {
      char filepath[FILE_MAX];
      bool ok_view = false;
      const char *view = rr ? ((RenderView *)BLI_findlink(&rr->views, i))->name :
                              ((ImageView *)BLI_findlink(&ima->views, i))->name;

      if (is_exr_rr) {
        BKE_scene_multiview_view_filepath_get(&opts->scene->r, opts->filepath, view, filepath);
        ok_view = BKE_image_render_write_exr(
            reports, rr, filepath, imf, save_as_render, view, layer);
        image_save_post(reports, ima, ibuf, ok_view, opts, true, filepath, r_colorspace_changed);
      }
      else {
        /* copy iuser to get the correct ibuf for this view */
        ImageUser view_iuser;

        if (iuser) {
          /* copy iuser to get the correct ibuf for this view */
          view_iuser = *iuser;
        }
        else {
          BKE_imageuser_default(&view_iuser);
        }

        view_iuser.view = i;
        view_iuser.flag &= ~IMA_SHOW_STEREO;

        if (rr) {
          BKE_image_multilayer_index(rr, &view_iuser);
        }
        else {
          BKE_image_multiview_index(ima, &view_iuser);
        }

        ibuf = BKE_image_acquire_ibuf(ima, &view_iuser, &lock);
        ibuf->planes = planes;

        BKE_scene_multiview_view_filepath_get(&opts->scene->r, opts->filepath, view, filepath);

        colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, true, imf);
        ok_view = BKE_imbuf_write_as(colormanaged_ibuf, filepath, &opts->im_format, save_copy);
        imbuf_save_post(ibuf, colormanaged_ibuf);
        image_save_post(reports, ima, ibuf, ok_view, opts, true, filepath, r_colorspace_changed);
        BKE_image_release_ibuf(ima, ibuf, lock);
      }
      ok &= ok_view;
    }

    if (is_exr_rr) {
      BKE_image_release_ibuf(ima, ibuf, lock);
    }
  }
  /* stereo (multiview) images */
  else if (opts->im_format.views_format == R_IMF_VIEWS_STEREO_3D) {
    if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
      ok = BKE_image_render_write_exr(
          reports, rr, opts->filepath, imf, save_as_render, nullptr, layer);
      image_save_post(reports, ima, ibuf, ok, opts, true, opts->filepath, r_colorspace_changed);
      BKE_image_release_ibuf(ima, ibuf, lock);
    }
    else {
      ImBuf *ibuf_stereo[2] = {nullptr};

      uchar planes = ibuf->planes;
      const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

      /* we need to get the specific per-view buffers */
      BKE_image_release_ibuf(ima, ibuf, lock);
      bool stereo_ok = true;

      for (int i = 0; i < 2; i++) {
        ImageUser view_iuser;

        if (iuser) {
          view_iuser = *iuser;
        }
        else {
          BKE_imageuser_default(&view_iuser);
        }

        view_iuser.flag &= ~IMA_SHOW_STEREO;

        if (rr) {
          int id = BLI_findstringindex(&rr->views, names[i], offsetof(RenderView, name));
          view_iuser.view = id;
          BKE_image_multilayer_index(rr, &view_iuser);
        }
        else {
          view_iuser.view = i;
          BKE_image_multiview_index(ima, &view_iuser);
        }

        ibuf = BKE_image_acquire_ibuf(ima, &view_iuser, &lock);

        if (ibuf == nullptr) {
          BKE_report(
              reports, RPT_ERROR, "Did not write, unexpected error when saving stereo image");
          BKE_image_release_ibuf(ima, ibuf, lock);
          stereo_ok = false;
          break;
        }

        ibuf->planes = planes;

        /* color manage the ImBuf leaving it ready for saving */
        colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, true, imf);

        BKE_image_format_to_imbuf(colormanaged_ibuf, imf);

        /* duplicate buffer to prevent locker issue when using render result */
        ibuf_stereo[i] = IMB_dupImBuf(colormanaged_ibuf);

        imbuf_save_post(ibuf, colormanaged_ibuf);

        BKE_image_release_ibuf(ima, ibuf, lock);
      }

      if (stereo_ok) {
        ibuf = IMB_stereo3d_ImBuf(imf, ibuf_stereo[0], ibuf_stereo[1]);

        /* save via traditional path */
        ok = BKE_imbuf_write_as(ibuf, opts->filepath, imf, save_copy);

        IMB_freeImBuf(ibuf);
      }

      for (int i = 0; i < 2; i++) {
        IMB_freeImBuf(ibuf_stereo[i]);
      }
    }
  }

  if (rr) {
    BKE_image_release_renderresult(opts->scene, ima);
  }

  return ok;
}

bool BKE_image_save(
    ReportList *reports, Main *bmain, Image *ima, ImageUser *iuser, const ImageSaveOptions *opts)
{
  /* For saving a tiled image we need an iuser, so use a local one if there isn't already one. */
  ImageUser save_iuser;
  if (iuser == nullptr) {
    BKE_imageuser_default(&save_iuser);
    iuser = &save_iuser;
    iuser->scene = opts->scene;
  }

  bool colorspace_changed = false;

  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern = nullptr;

  if (ima->source == IMA_SRC_TILED) {
    /* Verify filepath for tiled images contains a valid UDIM marker. */
    udim_pattern = BKE_image_get_tile_strformat(opts->filepath, &tile_format);
    if (tile_format == UDIM_TILE_FORMAT_NONE) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "When saving a tiled image, the path '%s' must contain a valid UDIM marker",
                  opts->filepath);
      return false;
    }
  }

  /* Save images */
  bool ok = false;
  if (ima->source != IMA_SRC_TILED) {
    ok = image_save_single(reports, ima, iuser, opts, &colorspace_changed);
  }
  else {
    /* Save all the tiles. */
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      ImageSaveOptions tile_opts = *opts;
      BKE_image_set_filepath_from_tile_number(
          tile_opts.filepath, udim_pattern, tile_format, tile->tile_number);

      iuser->tile = tile->tile_number;
      ok = image_save_single(reports, ima, iuser, &tile_opts, &colorspace_changed);
      if (!ok) {
        break;
      }
    }

    /* Set the image path and clear the per-tile generated flag only if all tiles were ok. */
    if (ok) {
      LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
        tile->gen_flag &= ~IMA_GEN_TILE;
      }
      image_save_update_filepath(ima, opts->filepath, opts);
    }
    MEM_freeN(udim_pattern);
  }

  if (colorspace_changed) {
    BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_COLORMANAGE);
  }

  return ok;
}

/* OpenEXR saving, single and multilayer. */

static float *image_exr_from_scene_linear_to_output(float *rect,
                                                    const int width,
                                                    const int height,
                                                    const int channels,
                                                    const ImageFormatData *imf,
                                                    Vector<float *> &tmp_output_rects)
{
  if (imf == nullptr) {
    return rect;
  }

  const char *to_colorspace = imf->linear_colorspace_settings.name;
  if (to_colorspace[0] == '\0' || IMB_colormanagement_space_name_is_scene_linear(to_colorspace)) {
    return rect;
  }

  float *output_rect = (float *)MEM_dupallocN(rect);
  tmp_output_rects.append(output_rect);

  const char *from_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  IMB_colormanagement_transform(
      output_rect, width, height, channels, from_colorspace, to_colorspace, false);

  return output_rect;
}

bool BKE_image_render_write_exr(ReportList *reports,
                                const RenderResult *rr,
                                const char *filepath,
                                const ImageFormatData *imf,
                                const bool save_as_render,
                                const char *view,
                                int layer)
{
  void *exrhandle = IMB_exr_get_handle();
  const bool half_float = (imf && imf->depth == R_IMF_CHAN_DEPTH_16);
  const bool multi_layer = !(imf && imf->imtype == R_IMF_IMTYPE_OPENEXR);
  const int channels = (!multi_layer && imf && imf->planes == R_IMF_PLANES_RGB) ? 3 : 4;
  Vector<float *> tmp_output_rects;

  /* Write first layer if not multilayer and no layer was specified. */
  if (!multi_layer && layer == -1) {
    layer = 0;
  }

  /* First add views since IMB_exr_add_channel checks number of views. */
  const RenderView *first_rview = (const RenderView *)rr->views.first;
  if (first_rview && (first_rview->next || first_rview->name[0])) {
    LISTBASE_FOREACH (RenderView *, rview, &rr->views) {
      if (!view || STREQ(view, rview->name)) {
        IMB_exr_add_view(exrhandle, rview->name);
      }
    }
  }

  /* Compositing result. */
  if (rr->have_combined) {
    LISTBASE_FOREACH (RenderView *, rview, &rr->views) {
      if (!rview->ibuf || !rview->ibuf->float_buffer.data) {
        continue;
      }

      const char *viewname = rview->name;
      if (view) {
        if (!STREQ(view, viewname)) {
          continue;
        }

        viewname = "";
      }

      /* Skip compositing if only a single other layer is requested. */
      if (!multi_layer && layer != 0) {
        continue;
      }

      float *output_rect =
          (save_as_render) ?
              image_exr_from_scene_linear_to_output(
                  rview->ibuf->float_buffer.data, rr->rectx, rr->recty, 4, imf, tmp_output_rects) :
              rview->ibuf->float_buffer.data;

      for (int a = 0; a < channels; a++) {
        char passname[EXR_PASS_MAXNAME];
        char layname[EXR_PASS_MAXNAME];
        /* "A" is not used if only "RGB" channels are output. */
        const char *chan_id = "RGBA";

        if (multi_layer) {
          RE_render_result_full_channel_name(passname, nullptr, "Combined", nullptr, chan_id, a);
          STRNCPY(layname, "Composite");
        }
        else {
          passname[0] = chan_id[a];
          passname[1] = '\0';
          layname[0] = '\0';
        }

        IMB_exr_add_channel(
            exrhandle, layname, passname, viewname, 4, 4 * rr->rectx, output_rect + a, half_float);
      }
    }
  }

  /* Other render layers. */
  int nr = (rr->have_combined) ? 1 : 0;
  const bool has_multiple_layers = BLI_listbase_count_at_most(&rr->layers, 2) > 1;
  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    /* Skip other render layers if requested. */
    if (!multi_layer && nr != layer) {
      nr++;
      continue;
    }
    nr++;

    LISTBASE_FOREACH (RenderPass *, rp, &rl->passes) {
      /* Skip non-RGBA and Z passes if not using multi layer. */
      if (!multi_layer && !STR_ELEM(rp->name, RE_PASSNAME_COMBINED, "")) {
        continue;
      }

      /* Skip pass if it does not match the requested view(s). */
      const char *viewname = rp->view;
      if (view) {
        if (!STREQ(view, viewname)) {
          continue;
        }

        viewname = "";
      }

      /* We only store RGBA passes as half float, for
       * others precision loss can be problematic. */
      const bool pass_RGBA = RE_RenderPassIsColor(rp);
      const bool pass_half_float = half_float && pass_RGBA;

      /* Color-space conversion only happens on RGBA passes. */
      float *output_rect = (save_as_render && pass_RGBA) ?
                               image_exr_from_scene_linear_to_output(rp->ibuf->float_buffer.data,
                                                                     rr->rectx,
                                                                     rr->recty,
                                                                     rp->channels,
                                                                     imf,
                                                                     tmp_output_rects) :
                               rp->ibuf->float_buffer.data;

      for (int a = 0; a < std::min(channels, rp->channels); a++) {
        /* Save Combined as RGBA or RGB if single layer save. */
        char passname[EXR_PASS_MAXNAME];
        char layname[EXR_PASS_MAXNAME];

        if (multi_layer) {
          /* A single unnamed layer indicates that the pass name should be used as the layer name,
           * while the pass name should be the channel ID. */
          if (!has_multiple_layers && rl->name[0] == '\0') {
            passname[0] = rp->chan_id[a];
            passname[1] = '\0';
            STRNCPY(layname, rp->name);
          }
          else {
            RE_render_result_full_channel_name(
                passname, nullptr, rp->name, nullptr, rp->chan_id, a);
            STRNCPY(layname, rl->name);
          }
        }
        else {
          passname[0] = rp->chan_id[a];
          passname[1] = '\0';
          layname[0] = '\0';
        }

        IMB_exr_add_channel(exrhandle,
                            layname,
                            passname,
                            viewname,
                            rp->channels,
                            rp->channels * rr->rectx,
                            output_rect + a,
                            pass_half_float);
      }
    }
  }

  errno = 0;

  BLI_file_ensure_parent_dir_exists(filepath);

  int compress = (imf ? imf->exr_codec : 0);
  bool success = IMB_exr_begin_write(
      exrhandle, filepath, rr->rectx, rr->recty, compress, rr->stamp_data);
  if (success) {
    IMB_exr_write_channels(exrhandle);
  }
  else {
    /* TODO: get the error from openexr's exception. */
    BKE_reportf(
        reports, RPT_ERROR, "Error writing render result, %s (see console)", strerror(errno));
  }

  for (float *rect : tmp_output_rects) {
    MEM_freeN(rect);
  }

  IMB_exr_close(exrhandle);
  return success;
}

/* Render output. */

static void image_render_print_save_message(ReportList *reports,
                                            const char *filepath,
                                            int ok,
                                            int err)
{
  if (ok) {
    /* no need to report, just some helpful console info */
    printf("Saved: '%s'\n", filepath);
  }
  else {
    /* report on error since users will want to know what failed */
    BKE_reportf(
        reports, RPT_ERROR, "Render error (%s) cannot save: '%s'", strerror(err), filepath);
  }
}

static int image_render_write_stamp_test(ReportList *reports,
                                         const Scene *scene,
                                         const RenderResult *rr,
                                         ImBuf *ibuf,
                                         const char *filepath,
                                         const ImageFormatData *imf,
                                         const bool stamp)
{
  int ok;

  if (stamp) {
    /* writes the name of the individual cameras */
    ok = BKE_imbuf_write_stamp(scene, rr, ibuf, filepath, imf);
  }
  else {
    ok = BKE_imbuf_write(ibuf, filepath, imf);
  }

  image_render_print_save_message(reports, filepath, ok, errno);

  return ok;
}

bool BKE_image_render_write(ReportList *reports,
                            RenderResult *rr,
                            const Scene *scene,
                            const bool stamp,
                            const char *filepath_basis,
                            const ImageFormatData *format,
                            bool save_as_render)
{
  bool ok = true;

  if (!rr) {
    return false;
  }

  ImageFormatData image_format;
  BKE_image_format_init_for_write(&image_format, scene, format);

  const bool is_mono = BLI_listbase_count_at_most(&rr->views, 2) < 2;
  const bool is_exr_rr = ELEM(
                             image_format.imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER) &&
                         RE_HasFloatPixels(rr);
  const float dither = scene->r.dither_intensity;

  if (image_format.views_format == R_IMF_VIEWS_MULTIVIEW && is_exr_rr) {
    ok = BKE_image_render_write_exr(
        reports, rr, filepath_basis, &image_format, save_as_render, nullptr, -1);
    image_render_print_save_message(reports, filepath_basis, ok, errno);
  }

  /* mono, legacy code */
  else if (is_mono || (image_format.views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    int view_id = 0;
    for (const RenderView *rv = (const RenderView *)rr->views.first; rv; rv = rv->next, view_id++)
    {
      char filepath[FILE_MAX];
      if (is_mono) {
        STRNCPY(filepath, filepath_basis);
      }
      else {
        BKE_scene_multiview_view_filepath_get(&scene->r, filepath_basis, rv->name, filepath);
      }

      if (is_exr_rr) {
        ok = BKE_image_render_write_exr(
            reports, rr, filepath, &image_format, save_as_render, rv->name, -1);
        image_render_print_save_message(reports, filepath, ok, errno);

        /* optional preview images for exr */
        if (ok && (image_format.flag & R_IMF_FLAG_PREVIEW_JPG)) {
          image_format.imtype = R_IMF_IMTYPE_JPEG90;
          image_format.depth = R_IMF_CHAN_DEPTH_8;

          if (BLI_path_extension_check(filepath, ".exr")) {
            filepath[strlen(filepath) - 4] = 0;
          }
          BKE_image_path_ext_from_imformat_ensure(filepath, sizeof(filepath), &image_format);

          ImBuf *ibuf = RE_render_result_rect_to_ibuf(rr, &image_format, dither, view_id);
          ibuf->planes = 24;
          IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, false, &image_format);

          ok = image_render_write_stamp_test(
              reports, scene, rr, ibuf, filepath, &image_format, stamp);

          IMB_freeImBuf(ibuf);
        }
      }
      else {
        ImBuf *ibuf = RE_render_result_rect_to_ibuf(rr, &image_format, dither, view_id);

        IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, false, &image_format);

        ok = image_render_write_stamp_test(
            reports, scene, rr, ibuf, filepath, &image_format, stamp);

        /* imbuf knows which rects are not part of ibuf */
        IMB_freeImBuf(ibuf);
      }
    }
  }
  else { /* R_IMF_VIEWS_STEREO_3D */
    BLI_assert(image_format.views_format == R_IMF_VIEWS_STEREO_3D);

    char filepath[FILE_MAX];
    STRNCPY(filepath, filepath_basis);

    if (image_format.imtype == R_IMF_IMTYPE_MULTILAYER) {
      printf("Stereo 3D not supported for MultiLayer image: %s\n", filepath);
    }
    else {
      ImBuf *ibuf_arr[3] = {nullptr};
      const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
      int i;

      for (i = 0; i < 2; i++) {
        int view_id = BLI_findstringindex(&rr->views, names[i], offsetof(RenderView, name));
        ibuf_arr[i] = RE_render_result_rect_to_ibuf(rr, &image_format, dither, view_id);
        IMB_colormanagement_imbuf_for_write(ibuf_arr[i], save_as_render, false, &image_format);
      }

      ibuf_arr[2] = IMB_stereo3d_ImBuf(&image_format, ibuf_arr[0], ibuf_arr[1]);

      ok = image_render_write_stamp_test(
          reports, scene, rr, ibuf_arr[2], filepath, &image_format, stamp);

      /* optional preview images for exr */
      if (ok && is_exr_rr && (image_format.flag & R_IMF_FLAG_PREVIEW_JPG)) {
        image_format.imtype = R_IMF_IMTYPE_JPEG90;
        image_format.depth = R_IMF_CHAN_DEPTH_8;

        if (BLI_path_extension_check(filepath, ".exr")) {
          filepath[strlen(filepath) - 4] = 0;
        }

        BKE_image_path_ext_from_imformat_ensure(filepath, sizeof(filepath), &image_format);
        ibuf_arr[2]->planes = 24;

        ok = image_render_write_stamp_test(
            reports, scene, rr, ibuf_arr[2], filepath, &image_format, stamp);
      }

      /* imbuf knows which rects are not part of ibuf */
      for (i = 0; i < 3; i++) {
        IMB_freeImBuf(ibuf_arr[i]);
      }
    }
  }

  BKE_image_format_free(&image_format);

  return ok;
}
