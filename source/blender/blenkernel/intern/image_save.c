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
 * \ingroup bke
 */

#include <errno.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_image_types.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_colortools.h"
#include "BKE_image.h"
#include "BKE_image_save.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"

void BKE_image_save_options_init(ImageSaveOptions *opts, Main *bmain, Scene *scene)
{
  memset(opts, 0, sizeof(*opts));

  opts->bmain = bmain;
  opts->scene = scene;

  BKE_imformat_defaults(&opts->im_format);
}

static void image_save_post(ReportList *reports,
                            Image *ima,
                            ImBuf *ibuf,
                            int ok,
                            ImageSaveOptions *opts,
                            int save_copy,
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
    BLI_strncpy(ibuf->name, filepath, sizeof(ibuf->name));
    BLI_strncpy(ima->filepath, filepath, sizeof(ima->filepath));
  }

  ibuf->userflags &= ~IB_BITMAPDIRTY;

  /* change type? */
  if (ima->type == IMA_TYPE_R_RESULT) {
    ima->type = IMA_TYPE_IMAGE;

    /* workaround to ensure the render result buffer is no longer used
     * by this image, otherwise can crash when a new render result is
     * created. */
    if (ibuf->rect && !(ibuf->mall & IB_rect)) {
      imb_freerectImBuf(ibuf);
    }
    if (ibuf->rect_float && !(ibuf->mall & IB_rectfloat)) {
      imb_freerectfloatImBuf(ibuf);
    }
    if (ibuf->zbuf && !(ibuf->mall & IB_zbuf)) {
      IMB_freezbufImBuf(ibuf);
    }
    if (ibuf->zbuf_float && !(ibuf->mall & IB_zbuffloat)) {
      IMB_freezbuffloatImBuf(ibuf);
    }
  }
  if (ELEM(ima->source, IMA_SRC_GENERATED, IMA_SRC_VIEWER)) {
    ima->source = IMA_SRC_FILE;
    ima->type = IMA_TYPE_IMAGE;
  }

  /* only image path, never ibuf */
  if (opts->relative) {
    const char *relbase = ID_BLEND_PATH(opts->bmain, &ima->id);
    BLI_path_rel(ima->filepath, relbase); /* only after saving */
  }

  ColorManagedColorspaceSettings old_colorspace_settings;
  BKE_color_managed_colorspace_settings_copy(&old_colorspace_settings, &ima->colorspace_settings);
  IMB_colormanagement_colorspace_from_ibuf_ftype(&ima->colorspace_settings, ibuf);
  if (!BKE_color_managed_colorspace_settings_equals(&old_colorspace_settings,
                                                    &ima->colorspace_settings)) {
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
 * \note ``ima->filepath`` and ``ibuf->name`` should end up the same.
 * \note for multiview the first ``ibuf`` is important to get the settings.
 */
static bool image_save_single(ReportList *reports,
                              Image *ima,
                              ImageUser *iuser,
                              ImageSaveOptions *opts,
                              bool *r_colorspace_changed)
{
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
  RenderResult *rr = NULL;
  bool ok = false;

  if (ibuf == NULL || (ibuf->rect == NULL && ibuf->rect_float == NULL)) {
    BKE_image_release_ibuf(ima, ibuf, lock);
    goto cleanup;
  }

  ImBuf *colormanaged_ibuf = NULL;
  const bool save_copy = opts->save_copy;
  const bool save_as_render = opts->save_as_render;
  ImageFormatData *imf = &opts->im_format;

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
    /* TODO, better solution, if a 24bit image is painted onto it may contain alpha */
    if ((opts->im_format.planes == R_IMF_PLANES_RGBA) &&
        /* it has been painted onto */
        (ibuf->userflags & IB_BITMAPDIRTY)) {
      /* checks each pixel, not ideal */
      ibuf->planes = BKE_imbuf_alpha_test(ibuf) ? R_IMF_PLANES_RGBA : R_IMF_PLANES_RGB;
    }
  }

  /* we need renderresult for exr and rendered multiview */
  rr = BKE_image_acquire_renderresult(opts->scene, ima);
  bool is_mono = rr ? BLI_listbase_count_at_most(&rr->views, 2) < 2 :
                      BLI_listbase_count_at_most(&ima->views, 2) < 2;
  bool is_exr_rr = rr && ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER) &&
                   RE_HasFloatPixels(rr);
  bool is_multilayer = is_exr_rr && (imf->imtype == R_IMF_IMTYPE_MULTILAYER);
  int layer = (iuser && !is_multilayer) ? iuser->layer : -1;

  /* error handling */
  if (!rr) {
    if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
      BKE_report(reports, RPT_ERROR, "Did not write, no Multilayer Image");
      BKE_image_release_ibuf(ima, ibuf, lock);
      goto cleanup;
    }
  }
  else {
    if (imf->views_format == R_IMF_VIEWS_STEREO_3D) {
      if (!BKE_image_is_stereo(ima)) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Did not write, the image doesn't have a \"%s\" and \"%s\" views",
                    STEREO_LEFT_NAME,
                    STEREO_RIGHT_NAME);
        BKE_image_release_ibuf(ima, ibuf, lock);
        goto cleanup;
      }

      /* it shouldn't ever happen*/
      if ((BLI_findstring(&rr->views, STEREO_LEFT_NAME, offsetof(RenderView, name)) == NULL) ||
          (BLI_findstring(&rr->views, STEREO_RIGHT_NAME, offsetof(RenderView, name)) == NULL)) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Did not write, the image doesn't have a \"%s\" and \"%s\" views",
                    STEREO_LEFT_NAME,
                    STEREO_RIGHT_NAME);
        BKE_image_release_ibuf(ima, ibuf, lock);
        goto cleanup;
      }
    }
    BKE_imbuf_stamp_info(rr, ibuf);
  }

  /* fancy multiview OpenEXR */
  if (imf->views_format == R_IMF_VIEWS_MULTIVIEW && is_exr_rr) {
    /* save render result */
    ok = RE_WriteRenderResult(reports, rr, opts->filepath, imf, NULL, layer);
    image_save_post(reports, ima, ibuf, ok, opts, true, opts->filepath, r_colorspace_changed);
    BKE_image_release_ibuf(ima, ibuf, lock);
  }
  /* regular mono pipeline */
  else if (is_mono) {
    if (is_exr_rr) {
      ok = RE_WriteRenderResult(reports, rr, opts->filepath, imf, NULL, layer);
    }
    else {
      colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(
          ibuf, save_as_render, true, &imf->view_settings, &imf->display_settings, imf);
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
    int i;
    unsigned char planes = ibuf->planes;
    const int totviews = (rr ? BLI_listbase_count(&rr->views) : BLI_listbase_count(&ima->views));

    if (!is_exr_rr) {
      BKE_image_release_ibuf(ima, ibuf, lock);
    }

    for (i = 0; i < totviews; i++) {
      char filepath[FILE_MAX];
      bool ok_view = false;
      const char *view = rr ? ((RenderView *)BLI_findlink(&rr->views, i))->name :
                              ((ImageView *)BLI_findlink(&ima->views, i))->name;

      if (is_exr_rr) {
        BKE_scene_multiview_view_filepath_get(&opts->scene->r, opts->filepath, view, filepath);
        ok_view = RE_WriteRenderResult(reports, rr, filepath, imf, view, layer);
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

        colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(
            ibuf, save_as_render, true, &imf->view_settings, &imf->display_settings, imf);
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
      ok = RE_WriteRenderResult(reports, rr, opts->filepath, imf, NULL, layer);
      image_save_post(reports, ima, ibuf, ok, opts, true, opts->filepath, r_colorspace_changed);
      BKE_image_release_ibuf(ima, ibuf, lock);
    }
    else {
      ImBuf *ibuf_stereo[2] = {NULL};

      unsigned char planes = ibuf->planes;
      const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
      int i;

      /* we need to get the specific per-view buffers */
      BKE_image_release_ibuf(ima, ibuf, lock);

      for (i = 0; i < 2; i++) {
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

        if (ibuf == NULL) {
          BKE_report(
              reports, RPT_ERROR, "Did not write, unexpected error when saving stereo image");
          goto cleanup;
        }

        ibuf->planes = planes;

        /* color manage the ImBuf leaving it ready for saving */
        colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(
            ibuf, save_as_render, true, &imf->view_settings, &imf->display_settings, imf);

        BKE_imbuf_write_prepare(colormanaged_ibuf, imf);
        IMB_prepare_write_ImBuf(IMB_isfloat(colormanaged_ibuf), colormanaged_ibuf);

        /* duplicate buffer to prevent locker issue when using render result */
        ibuf_stereo[i] = IMB_dupImBuf(colormanaged_ibuf);

        imbuf_save_post(ibuf, colormanaged_ibuf);
        BKE_image_release_ibuf(ima, ibuf, lock);
      }

      ibuf = IMB_stereo3d_ImBuf(imf, ibuf_stereo[0], ibuf_stereo[1]);

      /* save via traditional path */
      ok = BKE_imbuf_write_as(ibuf, opts->filepath, imf, save_copy);

      IMB_freeImBuf(ibuf);

      for (i = 0; i < 2; i++) {
        IMB_freeImBuf(ibuf_stereo[i]);
      }
    }
  }

cleanup:
  if (rr) {
    BKE_image_release_renderresult(opts->scene, ima);
  }

  return ok;
}

bool BKE_image_save(
    ReportList *reports, Main *bmain, Image *ima, ImageUser *iuser, ImageSaveOptions *opts)
{
  ImageUser save_iuser;
  BKE_imageuser_default(&save_iuser);

  bool colorspace_changed = false;

  if (ima->source == IMA_SRC_TILED) {
    /* Verify filepath for tiles images. */
    if (BLI_path_sequence_decode(opts->filepath, NULL, NULL, NULL) != 1001) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "When saving a tiled image, the path '%s' must contain the UDIM tag 1001",
                  opts->filepath);
      return false;
    }

    /* For saving a tiled image we need an iuser, so use a local one if there isn't already one. */
    if (iuser == NULL) {
      iuser = &save_iuser;
    }
  }

  /* Save image - or, for tiled images, the first tile. */
  bool ok = image_save_single(reports, ima, iuser, opts, &colorspace_changed);

  if (ok && ima->source == IMA_SRC_TILED) {
    char filepath[FILE_MAX];
    BLI_strncpy(filepath, opts->filepath, sizeof(filepath));

    char head[FILE_MAX], tail[FILE_MAX];
    unsigned short numlen;
    BLI_path_sequence_decode(filepath, head, tail, &numlen);

    /* Save all other tiles. */
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      /* Tile 1001 was already saved before the loop. */
      if (tile->tile_number == 1001 || !ok) {
        continue;
      }

      /* Build filepath of the tile. */
      BLI_path_sequence_encode(opts->filepath, head, tail, numlen, tile->tile_number);

      iuser->tile = tile->tile_number;
      ok = ok && image_save_single(reports, ima, iuser, opts, &colorspace_changed);
    }
    BLI_strncpy(opts->filepath, filepath, sizeof(opts->filepath));
  }

  if (colorspace_changed) {
    BKE_image_signal(bmain, ima, NULL, IMA_SIGNAL_COLORMANAGE);
  }

  return ok;
}
