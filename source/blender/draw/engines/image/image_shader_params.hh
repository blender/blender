/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"

#include "IMB_imbuf_types.hh"

#include "BKE_image.hh"

#include "image_enums.hh"
#include "image_space.hh"

namespace blender::image_engine {

struct ShaderParameters {
  ImageDrawFlags flags = ImageDrawFlags::DEFAULT;
  float4 shuffle;
  float2 far_near;
  bool use_premul_alpha = false;

  void update(AbstractSpaceAccessor *space,
              const Scene *scene,
              ::Image *image,
              ImBuf *image_buffer)
  {
    flags = ImageDrawFlags::DEFAULT;
    shuffle = float4(1.0f);
    far_near = float2(100.0f, 0.0f);

    use_premul_alpha = BKE_image_has_gpu_texture_premultiplied_alpha(image, image_buffer);

    if (scene->camera && scene->camera->type == OB_CAMERA) {
      const Camera *camera = static_cast<const Camera *>(scene->camera->data);
      far_near = float2(camera->clip_end, camera->clip_start);
    }
    space->get_shader_parameters(*this, image_buffer);
  }
};

}  // namespace blender::image_engine
