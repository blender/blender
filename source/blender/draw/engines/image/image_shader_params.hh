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
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "IMB_imbuf_types.h"

#include "BKE_image.h"

#include "BLI_math.h"

#include "image_space.hh"

struct ShaderParameters {
  constexpr static float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  int flags = 0;
  float shuffle[4];
  float far_near[2];
  bool use_premul_alpha = false;

  void update(AbstractSpaceAccessor *space, const Scene *scene, Image *image, ImBuf *image_buffer)
  {
    copy_v4_fl(shuffle, 1.0f);
    copy_v2_fl2(far_near, 100.0f, 0.0f);

    use_premul_alpha = BKE_image_has_gpu_texture_premultiplied_alpha(image, image_buffer);

    if (scene->camera && scene->camera->type == OB_CAMERA) {
      Camera *camera = static_cast<Camera *>(scene->camera->data);
      copy_v2_fl2(far_near, camera->clip_end, camera->clip_start);
    }
    const bool is_tiled_image = (image->source == IMA_SRC_TILED);
    space->get_shader_parameters(*this, image_buffer, is_tiled_image);
  }
};
