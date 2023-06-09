/* SPDX-FileCopyrightText: 2020 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"

#include "GPU_batch.h"

#include "image_engine.h"
#include "image_private.hh"

namespace blender::draw::image_engine {

struct IMAGE_Shaders {
  GPUShader *image_sh;
  GPUShader *depth_sh;
};

static struct {
  IMAGE_Shaders shaders;
} e_data = {{nullptr}}; /* Engine data */

GPUShader *IMAGE_shader_image_get()
{
  IMAGE_Shaders *sh_data = &e_data.shaders;
  if (sh_data->image_sh == nullptr) {
    sh_data->image_sh = GPU_shader_create_from_info_name("image_engine_color_shader");
  }
  return sh_data->image_sh;
}

GPUShader *IMAGE_shader_depth_get()
{
  IMAGE_Shaders *sh_data = &e_data.shaders;
  if (sh_data->depth_sh == nullptr) {
    sh_data->depth_sh = GPU_shader_create_from_info_name("image_engine_depth_shader");
  }
  return sh_data->depth_sh;
}

void IMAGE_shader_free()
{
  GPUShader **sh_data_as_array = (GPUShader **)&e_data.shaders;
  for (int i = 0; i < (sizeof(IMAGE_Shaders) / sizeof(GPUShader *)); i++) {
    DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
  }
}

}  // namespace blender::draw::image_engine
