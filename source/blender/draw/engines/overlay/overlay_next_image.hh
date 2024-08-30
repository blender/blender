/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Images {

 public:
  static eStereoViews images_stereo_eye(const Scene *scene, const View3D *v3d)
  {
    if ((scene->r.scemode & R_MULTIVIEW) == 0) {
      return STEREO_LEFT_ID;
    }
    if (v3d->stereo3d_camera != STEREO_3D_ID) {
      /* show only left or right camera */
      return eStereoViews(v3d->stereo3d_camera);
    }

    return eStereoViews(v3d->multiview_eye);
  }

  static void stereo_setup(const Scene *scene, const View3D *v3d, ::Image *ima, ImageUser *iuser)
  {
    if (BKE_image_is_stereo(ima)) {
      iuser->flag |= IMA_SHOW_STEREO;
      iuser->multiview_eye = images_stereo_eye(scene, v3d);
      BKE_image_multiview_index(ima, iuser);
    }
    else {
      iuser->flag &= ~IMA_SHOW_STEREO;
    }
  }
};

}  // namespace blender::draw::overlay
