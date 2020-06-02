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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * EEVEE LUT generation:
 *
 * Routine to generate the LUT used by eevee stored in eevee_lut.h
 * Theses functions are not to be used in the final executable.
 */

#include "DRW_render.h"

#include "BLI_alloca.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"

extern char datatoc_bsdf_lut_frag_glsl[];
extern char datatoc_btdf_lut_frag_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_vert_glsl[];

static struct GPUTexture *create_ggx_lut_texture(int UNUSED(w), int UNUSED(h))
{
  struct GPUTexture *tex;
  struct GPUFrameBuffer *fb = NULL;
  static float samples_len = 8192.0f;
  static float inv_samples_len = 1.0f / 8192.0f;

  char *lib_str = BLI_string_joinN(datatoc_bsdf_common_lib_glsl, datatoc_bsdf_sampling_lib_glsl);

  struct GPUShader *sh = DRW_shader_create_with_lib(datatoc_lightprobe_vert_glsl,
                                                    datatoc_lightprobe_geom_glsl,
                                                    datatoc_bsdf_lut_frag_glsl,
                                                    lib_str,
                                                    "#define HAMMERSLEY_SIZE 8192\n"
                                                    "#define BRDF_LUT_SIZE 64\n"
                                                    "#define NOISE_SIZE 64\n");

  DRWPass *pass = DRW_pass_create("LightProbe Filtering", DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_float(grp, "sampleCount", &samples_len, 1);
  DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_len, 1);
  DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
  DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);

  struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
  DRW_shgroup_call(grp, geom, NULL);

  float *texels = MEM_mallocN(sizeof(float[2]) * w * h, "lut");

  tex = DRW_texture_create_2d(w, h, GPU_RG16F, DRW_TEX_FILTER, (float *)texels);

  DRWFboTexture tex_filter = {&tex, GPU_RG16F, DRW_TEX_FILTER};
  GPU_framebuffer_init(&fb, &draw_engine_eevee_type, w, h, &tex_filter, 1);

  GPU_framebuffer_bind(fb);
  DRW_draw_pass(pass);

  float *data = MEM_mallocN(sizeof(float[3]) * w * h, "lut");
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, data);

  printf("{");
  for (int i = 0; i < w * h * 3; i += 3) {
    printf("%ff, %ff, ", data[i], data[i + 1]);
    i += 3;
    printf("%ff, %ff, ", data[i], data[i + 1]);
    i += 3;
    printf("%ff, %ff, ", data[i], data[i + 1]);
    i += 3;
    printf("%ff, %ff, \n", data[i], data[i + 1]);
  }
  printf("}");

  MEM_freeN(texels);
  MEM_freeN(data);

  return tex;
}

static struct GPUTexture *create_ggx_refraction_lut_texture(int w, int h)
{
  struct GPUTexture *tex;
  struct GPUTexture *hammersley = create_hammersley_sample_texture(8192);
  struct GPUFrameBuffer *fb = NULL;
  static float samples_len = 8192.0f;
  static float a2 = 0.0f;
  static float inv_samples_len = 1.0f / 8192.0f;

  char *frag_str = BLI_string_joinN(
      datatoc_bsdf_common_lib_glsl, datatoc_bsdf_sampling_lib_glsl, datatoc_btdf_lut_frag_glsl);

  struct GPUShader *sh = DRW_shader_create_fullscreen(frag_str,
                                                      "#define HAMMERSLEY_SIZE 8192\n"
                                                      "#define BRDF_LUT_SIZE 64\n"
                                                      "#define NOISE_SIZE 64\n"
                                                      "#define LUT_SIZE 64\n");

  MEM_freeN(frag_str);

  DRWPass *pass = DRW_pass_create("LightProbe Filtering", DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_float(grp, "a2", &a2, 1);
  DRW_shgroup_uniform_float(grp, "sampleCount", &samples_len, 1);
  DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_len, 1);
  DRW_shgroup_uniform_texture(grp, "texHammersley", hammersley);
  DRW_shgroup_uniform_texture(grp, "utilTex", e_data.util_tex);

  struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
  DRW_shgroup_call(grp, geom, NULL);

  float *texels = MEM_mallocN(sizeof(float[2]) * w * h, "lut");

  tex = DRW_texture_create_2d(w, h, GPU_R16F, DRW_TEX_FILTER, (float *)texels);

  DRWFboTexture tex_filter = {&tex, GPU_R16F, DRW_TEX_FILTER};
  GPU_framebuffer_init(&fb, &draw_engine_eevee_type, w, h, &tex_filter, 1);

  GPU_framebuffer_bind(fb);

  float *data = MEM_mallocN(sizeof(float[3]) * w * h, "lut");

  float inc = 1.0f / 31.0f;
  float roughness = 1e-8f - inc;
  FILE *f = BLI_fopen("btdf_split_sum_ggx.h", "w");
  fprintf(f, "static float btdf_split_sum_ggx[32][64 * 64] = {\n");
  do {
    roughness += inc;
    CLAMP(roughness, 1e-4f, 1.0f);
    a2 = powf(roughness, 4.0f);
    DRW_draw_pass(pass);

    GPU_framebuffer_read_data(0, 0, w, h, 3, 0, data);

#if 1
    fprintf(f, "\t{\n\t\t");
    for (int i = 0; i < w * h * 3; i += 3) {
      fprintf(f, "%ff,", data[i]);
      if (((i / 3) + 1) % 12 == 0) {
        fprintf(f, "\n\t\t");
      }
      else {
        fprintf(f, " ");
      }
    }
    fprintf(f, "\n\t},\n");
#else
    for (int i = 0; i < w * h * 3; i += 3) {
      if (data[i] < 0.01) {
        printf(" ");
      }
      else if (data[i] < 0.3) {
        printf(".");
      }
      else if (data[i] < 0.6) {
        printf("+");
      }
      else if (data[i] < 0.9) {
        printf("%%");
      }
      else {
        printf("#");
      }
      if ((i / 3 + 1) % 64 == 0) {
        printf("\n");
      }
    }
#endif

  } while (roughness < 1.0f);
  fprintf(f, "\n};\n");

  fclose(f);

  MEM_freeN(texels);
  MEM_freeN(data);

  return tex;
}