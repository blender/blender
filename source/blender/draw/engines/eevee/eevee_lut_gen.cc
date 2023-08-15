/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * EEVEE LUT generation:
 *
 * Routine to generate the LUT used by eevee stored in eevee_lut.h
 * These functions are not to be used in the final executable.
 */

#include "DRW_render.h"

#include "BLI_fileops.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"

#include "eevee_private.h"

#define DO_FILE_OUTPUT 0

float *EEVEE_lut_update_ggx_brdf(int lut_size)
{
  DRWPass *pass = DRW_pass_create(__func__, DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_ggx_lut_sh_get(), pass);
  DRW_shgroup_uniform_float_copy(grp, "sampleCount", 64.0f); /* Actual sample count is squared. */
  DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);

  GPUTexture *tex = DRW_texture_create_2d(
      lut_size, lut_size, GPU_RG16F, DRWTextureFlag(0), nullptr);
  GPUFrameBuffer *fb = nullptr;
  GPU_framebuffer_ensure_config(&fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(tex),
                                });
  GPU_framebuffer_bind(fb);
  DRW_draw_pass(pass);
  GPU_FRAMEBUFFER_FREE_SAFE(fb);

  float *data = static_cast<float *>(GPU_texture_read(tex, GPU_DATA_FLOAT, 0));
  GPU_texture_free(tex);
#if DO_FILE_OUTPUT
  /* Content is to be put inside `eevee_lut.cc`. */
  FILE *f = BLI_fopen("bsdf_split_sum_ggx.h", "w");
  fprintf(f, "const float bsdf_split_sum_ggx[%d * %d * 2] = {", lut_size, lut_size);
  for (int i = 0; i < lut_size * lut_size * 2;) {
    fprintf(f, "\n    ");
    for (int j = 0; j < 4; j++, i += 2) {
      fprintf(f, "%ff, %ff, ", data[i], data[i + 1]);
    }
  }
  fprintf(f, "\n};\n");
  fclose(f);
#endif

  return data;
}

float *EEVEE_lut_update_ggx_btdf(int lut_size, int lut_depth)
{
  float roughness;
  DRWPass *pass = DRW_pass_create(__func__, DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_ggx_refraction_lut_sh_get(), pass);
  DRW_shgroup_uniform_float_copy(grp, "sampleCount", 64.0f); /* Actual sample count is squared. */
  DRW_shgroup_uniform_float(grp, "z_factor", &roughness, 1);
  DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);

  GPUTexture *tex = DRW_texture_create_2d_array(
      lut_size, lut_size, lut_depth, GPU_RG16F, DRWTextureFlag(0), nullptr);
  GPUFrameBuffer *fb = nullptr;
  for (int i = 0; i < lut_depth; i++) {
    GPU_framebuffer_ensure_config(&fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE_LAYER(tex, i),
                                  });
    GPU_framebuffer_bind(fb);
    roughness = i / (lut_depth - 1.0f);
    DRW_draw_pass(pass);
  }

  GPU_FRAMEBUFFER_FREE_SAFE(fb);

  float *data = static_cast<float *>(GPU_texture_read(tex, GPU_DATA_FLOAT, 0));
  GPU_texture_free(tex);

#if DO_FILE_OUTPUT
  /* Content is to be put inside `eevee_lut.cc`. Don't forget to format the output. */
  FILE *f = BLI_fopen("btdf_split_sum_ggx.h", "w");
  fprintf(f, "const float btdf_split_sum_ggx[%d][%d * %d * 2] = {", lut_depth, lut_size, lut_size);
  fprintf(f, "\n    ");
  int ofs = 0;
  for (int d = 0; d < lut_depth; d++) {
    fprintf(f, "{\n");
    for (int i = 0; i < lut_size * lut_size * 2;) {
      for (int j = 0; j < 4; j++, i += 2, ofs += 2) {
        fprintf(f, "%ff, %ff, ", data[ofs], data[ofs + 1]);
      }
      fprintf(f, "\n    ");
    }
    fprintf(f, "},\n");
  }
  fprintf(f, "};\n");
  fclose(f);
#endif

  return data;
}
