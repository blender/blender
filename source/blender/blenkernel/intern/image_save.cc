/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cerrno>
#include <cstring>

#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_openexr.hh"

#include "BKE_colortools.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_image_save.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "RE_pipeline.h"

#include "CLG_log.h"

static CLG_LogRef LOG_RENDER = {"render"};

using blender::Vector;

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

  *opts = ImageSaveOptions{};

  opts->bmain = bmain;
  opts->scene = scene;
  opts->save_as_render = ima->source == IMA_SRC_VIEWER || save_as_render;

  BKE_image_format_init(&opts->im_format);

  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);

  if (ibuf) {
    const char *ima_colorspace = ima->colorspace_settings.name;

    if (opts->save_as_render) {
      /* Render/compositor output or user chose to save with render settings. */
      BKE_image_format_init_for_write(&opts->im_format, scene, nullptr);
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
      STRNCPY_UTF8(opts->im_format.linear_colorspace_settings.name, ima_colorspace);
    }

    opts->im_format.color_management = R_IMF_COLOR_MANAGEMENT_FOLLOW_SCENE;

    /* Compute filepath, but don't resolve multiview and UDIM which are handled
     * by the image saving code itself. */
    BKE_image_user_file_path_ex(bmain, iuser, ima, opts->filepath, false, false);

    /* For movies, replace extension and add the frame number to avoid writing over the movie file
     * itself and provide a good default file path. */
    if (ima->source == IMA_SRC_MOVIE) {
      char filepath_no_ext[FILE_MAX];
      STRNCPY(filepath_no_ext, opts->filepath);
      BLI_path_extension_strip(filepath_no_ext);
      SNPRINTF(opts->filepath, "%s_%.*d", filepath_no_ext, 4, ibuf->fileframe);
      BKE_image_path_ext_from_imformat_ensure(
          opts->filepath, sizeof(opts->filepath), &opts->im_format);
    }

    /* sanitize all settings */

    /* unlikely but just in case */
    if (ELEM(opts->im_format.planes, R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA) == 0) {
      opts->im_format.planes = R_IMF_PLANES_RGBA;
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
          BLI_path_join(opts->filepath, sizeof(opts->filepath), "//", DATA_("Untitled"));
          BLI_path_abs(opts->filepath, BKE_main_blendfile_path(bmain));
        }
      }
      else {
        BLI_path_join(opts->filepath, sizeof(opts->filepath), "//", ima->id.name + 2);
        BLI_path_make_safe_filename(opts->filepath + 2);
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
    else if (opts->im_format.imtype != opts->prev_imtype) {
      BKE_image_format_update_color_space_for_type(&opts->im_format);
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
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not write image: %s",
                errno ? strerror(errno) : "internal error, see console");
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
    IMB_free_byte_pixels(ibuf);
    IMB_free_float_pixels(ibuf);
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
  else if (opts->save_as_render) {
    /* Set the display colorspace that we converted to. */
    const ColorSpace *colorspace = IMB_colormangement_display_get_color_space(
        &opts->im_format.view_settings, &opts->im_format.display_settings);
    if (colorspace) {
      blender::StringRefNull colorspace_name = IMB_colormanagement_colorspace_get_name(colorspace);
      if (colorspace_name != ima->colorspace_settings.name) {
        STRNCPY(ima->colorspace_settings.name, colorspace_name.c_str());
      }
    }

    /* View transform is now baked in, so don't apply it a second time for viewing. */
    if (ima->flag & IMA_VIEW_AS_RENDER) {
      ima->flag &= ~IMA_VIEW_AS_RENDER;
    }

    *r_colorspace_changed = true;
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
  const bool is_mono = !(rr ? RE_ResultIsMultiView(rr) : BKE_image_is_multiview(ima));
  const bool is_exr_rr = rr && ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER) &&
                         RE_HasFloatPixels(rr);
  const bool is_multilayer = is_exr_rr && (imf->imtype == R_IMF_IMTYPE_MULTILAYER);
  const int layer = (iuser && !is_multilayer) ? iuser->layer : -1;

  /* error handling */
  if (rr == nullptr) {
    if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
      BKE_report(reports, RPT_ERROR, "Did not write, no Multilayer Image");
      BKE_image_release_renderresult(opts->scene, ima, rr);
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
        BKE_image_release_renderresult(opts->scene, ima, rr);
        BKE_image_release_ibuf(ima, ibuf, lock);
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
        BKE_image_release_renderresult(opts->scene, ima, rr);
        BKE_image_release_ibuf(ima, ibuf, lock);
        return ok;
      }
    }
    BKE_imbuf_stamp_info(rr, ibuf);
  }

  /* Don't write permanently into the render-result. */
  double rr_ppm_prev[2] = {0, 0};

  if (save_as_render && rr) {
    /* These could be used in the case of a null `rr`, currently they're not though.
     * Note that setting zero when there is no `rr` is intentional,
     * this signifies no valid PPM is set. */
    double ppm[2] = {0, 0};
    if (opts->scene) {
      BKE_scene_ppm_get(&opts->scene->r, ppm);
    }
    copy_v2_v2_db(rr_ppm_prev, rr->ppm);
    copy_v2_v2_db(rr->ppm, ppm);
  }

  /* From now on, calls to #BKE_image_release_renderresult must restore the PPM beforehand. */
  auto render_result_restore_ppm = [rr, save_as_render, rr_ppm_prev]() {
    if (save_as_render && rr) {
      copy_v2_v2_db(rr->ppm, rr_ppm_prev);
    }
  };

  /* fancy multiview OpenEXR */
  if (imf->views_format == R_IMF_VIEWS_MULTIVIEW && is_exr_rr) {
    /* save render result */
    ok = BKE_image_render_write_exr(
        reports, rr, opts->filepath, imf, save_as_render, nullptr, layer);

    render_result_restore_ppm();
    BKE_image_release_renderresult(opts->scene, ima, rr);
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

    render_result_restore_ppm();
    BKE_image_release_renderresult(opts->scene, ima, rr);
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

    render_result_restore_ppm();
    BKE_image_release_renderresult(opts->scene, ima, rr);

    if (is_exr_rr) {
      BKE_image_release_ibuf(ima, ibuf, lock);
    }
  }
  /* stereo (multiview) images */
  else if (opts->im_format.views_format == R_IMF_VIEWS_STEREO_3D) {
    if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
      ok = BKE_image_render_write_exr(
          reports, rr, opts->filepath, imf, save_as_render, nullptr, layer);

      render_result_restore_ppm();
      BKE_image_release_renderresult(opts->scene, ima, rr);
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
        if (ibuf) {
          ok = BKE_imbuf_write_as(ibuf, opts->filepath, imf, save_copy);

          IMB_freeImBuf(ibuf);
        }
      }

      for (int i = 0; i < 2; i++) {
        IMB_freeImBuf(ibuf_stereo[i]);
      }

      render_result_restore_ppm();
      BKE_image_release_renderresult(opts->scene, ima, rr);
    }
  }
  else {
    render_result_restore_ppm();
    BKE_image_release_renderresult(opts->scene, ima, rr);
    BKE_image_release_ibuf(ima, ibuf, lock);
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
    BKE_image_partial_update_mark_full_update(ima);
  }

  return ok;
}

/* OpenEXR saving, single and multilayer. */

static float *image_exr_from_scene_linear_to_output(float *rect,
                                                    const int width,
                                                    const int height,
                                                    const int channels,
                                                    const ImageFormatData *imf,
                                                    Vector<float *> &tmp_output_rects,
                                                    blender::StringRefNull &r_colorspace)
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
  IMB_colormanagement_transform_float(
      output_rect, width, height, channels, from_colorspace, to_colorspace, false);

  r_colorspace = to_colorspace;

  return output_rect;
}

static float *image_exr_from_rgb_to_bw(
    float *input_buffer, int width, int height, int channels, Vector<float *> &temporary_buffers)
{
  float *gray_scale_output = MEM_malloc_arrayN<float>(size_t(width) * size_t(height),
                                                      "Gray Scale Buffer For EXR");
  temporary_buffers.append(gray_scale_output);

  blender::threading::parallel_for(
      blender::IndexRange(height), 1, [&](const blender::IndexRange sub_y_range) {
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : blender::IndexRange(width)) {
            const int64_t index = y * int64_t(width) + x;
            gray_scale_output[index] = IMB_colormanagement_get_luminance(input_buffer +
                                                                         index * channels);
          }
        }
      });

  return gray_scale_output;
}

static float *image_exr_opaque_alpha_buffer(int width,
                                            int height,
                                            Vector<float *> &temporary_buffers)
{
  float *alpha_output = MEM_malloc_arrayN<float>(size_t(width) * size_t(height),
                                                 "Opaque Alpha Buffer For EXR");
  temporary_buffers.append(alpha_output);

  blender::threading::parallel_for(
      blender::IndexRange(height), 1, [&](const blender::IndexRange sub_y_range) {
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : blender::IndexRange(width)) {
            alpha_output[y * int64_t(width) + x] = 1.0;
          }
        }
      });

  return alpha_output;
}

static void add_exr_compositing_result(ExrHandle *exr_handle,
                                       const RenderResult *render_result,
                                       const ImageFormatData *imf,
                                       bool save_as_render,
                                       const char *view_name,
                                       int layer,
                                       Vector<float *> &temporary_buffers)
{
  /* Render result has no compositing result. */
  if (!render_result->have_combined) {
    return;
  }

  /* Skip compositing result if we are saving a single layer EXR that is not the compositing
   * layer, which always has the layer index of 0. */
  const bool is_multi_layer = !(imf && imf->imtype == R_IMF_IMTYPE_OPENEXR);
  if (!is_multi_layer && layer != 0) {
    return;
  }

  /* Write the compositing result for the view with the given view name, or for all views if no
   * view name is given. */
  LISTBASE_FOREACH (RenderView *, render_view, &render_result->views) {
    if (!render_view->ibuf || !render_view->ibuf->float_buffer.data) {
      continue;
    }

    /* If a view name is given, then we skip views that do not match the given view name.
     * Otherwise, we always add the views. */
    if (view_name && !STREQ(view_name, render_view->name)) {
      continue;
    }

    /* If a view name is given, that means we are writing a single view, so no need to identify the
     * channel by the view name, and we supply an empty view to the rest of the code. */
    const char *render_view_name = view_name ? "" : render_view->name;

    /* Compositing results is always a 4-channel RGBA. */
    const int channels_count_in_buffer = 4;
    float *output_buffer = render_view->ibuf->float_buffer.data;
    blender::StringRefNull colorspace = IMB_colormanagement_role_colorspace_name_get(
        COLOR_ROLE_SCENE_LINEAR);

    if (save_as_render) {
      output_buffer = image_exr_from_scene_linear_to_output(output_buffer,
                                                            render_result->rectx,
                                                            render_result->recty,
                                                            channels_count_in_buffer,
                                                            imf,
                                                            temporary_buffers,
                                                            colorspace);
    }

    /* For multi-layer EXRs, we write the buffer as is with all its 4 channels. */
    const bool half_float = (imf && imf->depth == R_IMF_CHAN_DEPTH_16);
    if (is_multi_layer) {
      IMB_exr_add_channels(exr_handle,
                           "Composite.Combined",
                           "RGBA",
                           render_view_name,
                           colorspace,
                           channels_count_in_buffer,
                           channels_count_in_buffer * render_result->rectx,
                           output_buffer,
                           half_float);
      continue;
    }

    /* For single layer EXR, we only add the channels specified in the image format and do any
     * needed color format conversion.
     *
     * In case of a single required channel, we need to do RGBA to BW conversion. */
    const int required_channels = imf ? imf->planes / 8 : 4;
    if (required_channels == 1) {
      float *gray_scale_output = image_exr_from_rgb_to_bw(output_buffer,
                                                          render_result->rectx,
                                                          render_result->recty,
                                                          channels_count_in_buffer,
                                                          temporary_buffers);
      IMB_exr_add_channels(exr_handle,
                           "",
                           "V",
                           render_view_name,
                           colorspace,
                           1,
                           render_result->rectx,
                           gray_scale_output,
                           half_float);
      continue;
    }

    /* Add RGB[A] channels. This will essentially skip the alpha channel if only three channels
     * were required. */
    std::string channelnames = blender::StringRef("RGBA").substr(0, required_channels);
    IMB_exr_add_channels(exr_handle,
                         "",
                         channelnames,
                         render_view_name,
                         colorspace,
                         channels_count_in_buffer,
                         channels_count_in_buffer * render_result->rectx,
                         output_buffer,
                         half_float);
  }
}

bool BKE_image_render_write_exr(ReportList *reports,
                                const RenderResult *rr,
                                const char *filepath,
                                const ImageFormatData *imf,
                                const bool save_as_render,
                                const char *view,
                                int layer)
{
  const int write_multipart = (imf ? imf->exr_flag & R_IMF_EXR_FLAG_MULTIPART : true);
  ExrHandle *exrhandle = IMB_exr_get_handle(write_multipart);
  const bool multi_layer = !(imf && imf->imtype == R_IMF_IMTYPE_OPENEXR);

  /* Write first layer if not multilayer and no layer was specified. */
  if (!multi_layer && layer == -1) {
    layer = 0;
  }

  /* First add views since IMB_exr_add_channels checks number of views. */
  const RenderView *first_rview = (const RenderView *)rr->views.first;
  if (first_rview && (first_rview->next || first_rview->name[0])) {
    LISTBASE_FOREACH (RenderView *, rview, &rr->views) {
      if (!view || STREQ(view, rview->name)) {
        IMB_exr_add_view(exrhandle, rview->name);
      }
    }
  }

  Vector<float *> tmp_output_rects;
  add_exr_compositing_result(exrhandle, rr, imf, save_as_render, view, layer, tmp_output_rects);

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

    LISTBASE_FOREACH (RenderPass *, render_pass, &rl->passes) {
      /* Skip non-RGBA and Z passes if not using multi layer. */
      if (!multi_layer && !STR_ELEM(render_pass->name, RE_PASSNAME_COMBINED, "")) {
        continue;
      }

      /* Skip pass if it does not match the requested view(s). */
      const char *viewname = render_pass->view;
      if (view) {
        if (!STREQ(view, viewname)) {
          continue;
        }

        viewname = "";
      }

      /* We only store RGBA passes as half float, for
       * others precision loss can be problematic. */
      const bool pass_RGBA = RE_RenderPassIsColor(render_pass);
      const bool half_float = (imf && imf->depth == R_IMF_CHAN_DEPTH_16);
      const bool pass_half_float = half_float && pass_RGBA;

      /* Color-space conversion only happens on RGBA passes. */
      float *output_rect = render_pass->ibuf->float_buffer.data;
      blender::StringRefNull colorspace = IMB_colormanagement_role_colorspace_name_get(
          (pass_RGBA) ? COLOR_ROLE_SCENE_LINEAR : COLOR_ROLE_DATA);

      if (save_as_render && pass_RGBA) {
        output_rect = image_exr_from_scene_linear_to_output(output_rect,
                                                            rr->rectx,
                                                            rr->recty,
                                                            render_pass->channels,
                                                            imf,
                                                            tmp_output_rects,
                                                            colorspace);
      }

      /* For multi-layer EXRs, we write the pass as is with all of its channels. */
      if (multi_layer) {
        std::string layer_pass_name = render_pass->name;

        /* Unless we have a single unnamed layer, include the layer name. */
        if (has_multiple_layers || rl->name[0] != '\0') {
          layer_pass_name = rl->name + ("." + layer_pass_name);
        }

        std::string channelnames = blender::StringRef(render_pass->chan_id, render_pass->channels);
        IMB_exr_add_channels(exrhandle,
                             layer_pass_name,
                             channelnames,
                             viewname,
                             colorspace,
                             render_pass->channels,
                             render_pass->channels * rr->rectx,
                             output_rect,
                             pass_half_float);
        continue;
      }

      /* For single layer EXR, we only add the channels specified in the image format and do any
       * needed color format conversion.
       *
       * First, if the required channels equal the pass channels, we add the channels as is. Or,
       * we add the RGB[A] channels if the pass is RGB[A] and we require RGB[A]. If the alpha
       * channel is required but does not exist in the pass, it will be added below. */
      const int required_channels = imf ? imf->planes / 8 : 4;
      if (required_channels == render_pass->channels ||
          (required_channels != 1 && render_pass->channels != 1))
      {
        std::string channelnames = blender::StringRef(
            render_pass->chan_id, std::min(required_channels, render_pass->channels));
        IMB_exr_add_channels(exrhandle,
                             "",
                             channelnames,
                             viewname,
                             colorspace,
                             render_pass->channels,
                             render_pass->channels * rr->rectx,
                             output_rect,
                             pass_half_float);
      }
      else if (required_channels == 1) {
        /* In case of a single required channel, we need to do RGB[A] to BW conversion. We know
         * the input is RGB[A] and not single channel because it filed the condition above. */
        float *gray_scale_output = image_exr_from_rgb_to_bw(
            output_rect, rr->rectx, rr->recty, render_pass->channels, tmp_output_rects);
        IMB_exr_add_channels(exrhandle,
                             "",
                             "V",
                             viewname,
                             colorspace,
                             1,
                             rr->rectx,
                             gray_scale_output,
                             pass_half_float);
      }
      else if (render_pass->channels == 1) {
        /* In case of a single channel pass, we need to broadcast the same channel for each of
         * the RGB channels that are required. We know the RGB is required because single channel
         * requirement was handled above. The alpha channel will be added later. */
        for (int i = 0; i < 3; i++) {
          IMB_exr_add_channels(exrhandle,
                               "",
                               std::string(1, "RGB"[i]).c_str(),
                               viewname,
                               colorspace,
                               1,
                               rr->rectx,
                               output_rect,
                               pass_half_float);
        }
      }

      /* Add an opaque alpha channel if the pass contains no alpha channel but an alpha channel
       * is required. */
      if (required_channels == 4 && render_pass->channels < 4) {
        float *alpha_output = image_exr_opaque_alpha_buffer(
            rr->rectx, rr->recty, tmp_output_rects);
        IMB_exr_add_channels(
            exrhandle, "", "A", viewname, colorspace, 1, rr->rectx, alpha_output, pass_half_float);
      }
    }
  }

  errno = 0;

  BLI_file_ensure_parent_dir_exists(filepath);

  const int compress = (imf ? imf->exr_codec : 0);
  const int quality = (imf ? imf->quality : 90);
  bool success = IMB_exr_begin_write(
      exrhandle, filepath, rr->rectx, rr->recty, rr->ppm, compress, quality, rr->stamp_data);
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
    CLOG_INFO_NOCHECK(&LOG_RENDER, "Saved: '%s'", filepath);
  }
  else {
    /* report on error since users will want to know what failed */
    BKE_reportf(
        reports, RPT_ERROR, "Render error (%s) cannot save: '%s'", strerror(err), filepath);
  }
}

static bool image_render_write_stamp_test(ReportList *reports,
                                          const Scene *scene,
                                          const RenderResult *rr,
                                          ImBuf *ibuf,
                                          const char *filepath,
                                          const ImageFormatData *imf,
                                          const bool stamp)
{
  bool ok;

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

  if (!save_as_render) {
    BKE_color_managed_colorspace_settings_copy(&image_format.linear_colorspace_settings,
                                               &format->linear_colorspace_settings);
  }

  const bool is_mono = !RE_ResultIsMultiView(rr);
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

      if (ibuf_arr[2]) {
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
      }
      else {
        BKE_reportf(reports, RPT_ERROR, "Failed to create stereo image buffer");
        ok = false;
      }

      /* imbuf knows which rects are not part of ibuf */
      for (i = 0; i < 3; i++) {
        if (ibuf_arr[i]) {
          IMB_freeImBuf(ibuf_arr[i]);
        }
      }
    }
  }

  BKE_image_format_free(&image_format);

  return ok;
}
