/* SPDX-FileCopyrightText: 2006-2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_studiolight.h"

#include "BKE_appdir.hh"
#include "BKE_icons.hh"

#include "BLI_dynstr.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DNA_listBase.h"

#include "IMB_imbuf.hh"
#include "IMB_interp.hh"
#include "IMB_openexr.hh"

#include "GPU_texture.hh"

#include "MEM_guardedalloc.h"

#include <cstring>

using blender::float4;

/* Statics */
static ListBase studiolights;
static int last_studiolight_id = 0;
#define STUDIOLIGHT_PASSNAME_DIFFUSE "diffuse"
#define STUDIOLIGHT_PASSNAME_SPECULAR "specular"

static const char *STUDIOLIGHT_LIGHTS_FOLDER = "studiolights" SEP_STR "studio" SEP_STR;
static const char *STUDIOLIGHT_WORLD_FOLDER = "studiolights" SEP_STR "world" SEP_STR;
static const char *STUDIOLIGHT_MATCAP_FOLDER = "studiolights" SEP_STR "matcap" SEP_STR;

static const char *STUDIOLIGHT_WORLD_DEFAULT = "forest.exr";
static const char *STUDIOLIGHT_MATCAP_DEFAULT = "basic_1.exr";

/* ITER MACRO */

/**
 * Iter on all pixel giving texel center position and pixel pointer.
 *
 * Arguments
 *   type : type of src.
 *   src : source buffer.
 *   channels : number of channels per pixel.
 *
 * Others
 *   x, y : normalized UV coordinate [0..1] of the current pixel center.
 *   texel_size[2] : UV size of a pixel in this texture.
 *   pixel[] : pointer to the current pixel.
 */
#define ITER_PIXELS(type, src, channels, width, height) \
  { \
    float texel_size[2]; \
    texel_size[0] = 1.0f / width; \
    texel_size[1] = 1.0f / height; \
    type(*pixel_)[channels] = (type(*)[channels])src; \
    for (float y = 0.5 * texel_size[1]; y < 1.0; y += texel_size[1]) { \
      for (float x = 0.5 * texel_size[0]; x < 1.0; x += texel_size[0], pixel_++) { \
        type *pixel = *pixel_;

#define ITER_PIXELS_END \
  } \
  } \
  } \
  ((void)0)

/* FUNCTIONS */
#define IMB_SAFE_FREE(p) \
  do { \
    if (p) { \
      IMB_freeImBuf(p); \
      p = nullptr; \
    } \
  } while (0)

#define GPU_TEXTURE_SAFE_FREE(p) \
  do { \
    if (p) { \
      GPU_texture_free(p); \
      p = nullptr; \
    } \
  } while (0)

static void studiolight_free_image_buffers(StudioLight *sl)
{
  sl->flag &= ~STUDIOLIGHT_EXTERNAL_IMAGE_LOADED;
  IMB_SAFE_FREE(sl->matcap_diffuse.ibuf);
  IMB_SAFE_FREE(sl->matcap_specular.ibuf);
  IMB_SAFE_FREE(sl->equirect_radiance_buffer);
}

static void studiolight_free(StudioLight *sl)
{
#define STUDIOLIGHT_DELETE_ICON(s) \
  do { \
    if (s != 0) { \
      BKE_icon_delete(s); \
      s = 0; \
    } \
  } while (0)

  if (sl->free_function) {
    sl->free_function(sl, sl->free_function_data);
  }
  STUDIOLIGHT_DELETE_ICON(sl->icon_id_radiance);
  STUDIOLIGHT_DELETE_ICON(sl->icon_id_irradiance);
  STUDIOLIGHT_DELETE_ICON(sl->icon_id_matcap);
  STUDIOLIGHT_DELETE_ICON(sl->icon_id_matcap_flipped);
#undef STUDIOLIGHT_DELETE_ICON

  studiolight_free_image_buffers(sl);

  GPU_TEXTURE_SAFE_FREE(sl->equirect_radiance_gputexture);
  GPU_TEXTURE_SAFE_FREE(sl->matcap_diffuse.gputexture);
  GPU_TEXTURE_SAFE_FREE(sl->matcap_specular.gputexture);
  MEM_SAFE_FREE(sl);
}

/**
 * Free temp resources when the studio light is only requested for icons.
 *
 * Only keeps around resources for studio lights that have been used in any viewport.
 */
static void studiolight_free_temp_resources(StudioLight *sl)
{
  const bool is_used_in_viewport = bool(sl->flag & (STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE |
                                                    STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE |
                                                    STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE));
  if (is_used_in_viewport) {
    return;
  }
  studiolight_free_image_buffers(sl);
}

static StudioLight *studiolight_create(int flag)
{
  StudioLight *sl = MEM_callocN<StudioLight>(__func__);
  sl->filepath[0] = 0x00;
  sl->name[0] = 0x00;
  sl->free_function = nullptr;
  sl->flag = flag;
  sl->index = ++last_studiolight_id;
  if (flag & STUDIOLIGHT_TYPE_STUDIO) {
    sl->icon_id_irradiance = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_IRRADIANCE);
  }
  else if (flag & STUDIOLIGHT_TYPE_MATCAP) {
    sl->icon_id_matcap = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_MATCAP);
    sl->icon_id_matcap_flipped = BKE_icon_ensure_studio_light(
        sl, STUDIOLIGHT_ICON_ID_TYPE_MATCAP_FLIPPED);
  }
  else {
    sl->icon_id_radiance = BKE_icon_ensure_studio_light(sl, STUDIOLIGHT_ICON_ID_TYPE_RADIANCE);
  }

  return sl;
}

#define STUDIOLIGHT_FILE_VERSION 1

#define READ_VAL(type, parser, id, val, lines) \
  do { \
    for (LinkNode *line = lines; line; line = line->next) { \
      char *val_str, *str = static_cast<char *>(line->link); \
      if ((val_str = strstr(str, id " "))) { \
        val_str += sizeof(id); /* Skip id + spacer. */ \
        val = parser(val_str); \
      } \
    } \
  } while (0)

#define READ_FVAL(id, val, lines) READ_VAL(float, atof, id, val, lines)
#define READ_IVAL(id, val, lines) READ_VAL(int, atoi, id, val, lines)

#define READ_VEC3(id, val, lines) \
  do { \
    READ_FVAL(id ".x", val[0], lines); \
    READ_FVAL(id ".y", val[1], lines); \
    READ_FVAL(id ".z", val[2], lines); \
  } while (0)

#define READ_SOLIDLIGHT(sl, i, lines) \
  do { \
    READ_IVAL("light[" STRINGIFY(i) "].flag", sl[i].flag, lines); \
    READ_FVAL("light[" STRINGIFY(i) "].smooth", sl[i].smooth, lines); \
    READ_VEC3("light[" STRINGIFY(i) "].col", sl[i].col, lines); \
    READ_VEC3("light[" STRINGIFY(i) "].spec", sl[i].spec, lines); \
    READ_VEC3("light[" STRINGIFY(i) "].vec", sl[i].vec, lines); \
  } while (0)

static void studiolight_load_solid_light(StudioLight *sl)
{
  LinkNode *lines = BLI_file_read_as_lines(sl->filepath);
  if (lines) {
    READ_VEC3("light_ambient", sl->light_ambient, lines);
    READ_SOLIDLIGHT(sl->light, 0, lines);
    READ_SOLIDLIGHT(sl->light, 1, lines);
    READ_SOLIDLIGHT(sl->light, 2, lines);
    READ_SOLIDLIGHT(sl->light, 3, lines);
  }
  BLI_file_free_lines(lines);
}

#undef READ_SOLIDLIGHT
#undef READ_VEC3
#undef READ_IVAL
#undef READ_FVAL

#define WRITE_FVAL(str, id, val) BLI_dynstr_appendf(str, id " %f\n", val)
#define WRITE_IVAL(str, id, val) BLI_dynstr_appendf(str, id " %d\n", val)

#define WRITE_VEC3(str, id, val) \
  do { \
    WRITE_FVAL(str, id ".x", val[0]); \
    WRITE_FVAL(str, id ".y", val[1]); \
    WRITE_FVAL(str, id ".z", val[2]); \
  } while (0)

#define WRITE_SOLIDLIGHT(str, sl, i) \
  do { \
    WRITE_IVAL(str, "light[" STRINGIFY(i) "].flag", sl[i].flag); \
    WRITE_FVAL(str, "light[" STRINGIFY(i) "].smooth", sl[i].smooth); \
    WRITE_VEC3(str, "light[" STRINGIFY(i) "].col", sl[i].col); \
    WRITE_VEC3(str, "light[" STRINGIFY(i) "].spec", sl[i].spec); \
    WRITE_VEC3(str, "light[" STRINGIFY(i) "].vec", sl[i].vec); \
  } while (0)

static void studiolight_write_solid_light(StudioLight *sl)
{
  FILE *fp = BLI_fopen(sl->filepath, "wb");
  if (fp) {
    DynStr *str = BLI_dynstr_new();

    /* Very dumb ASCII format. One value per line separated by a space. */
    WRITE_IVAL(str, "version", STUDIOLIGHT_FILE_VERSION);
    WRITE_VEC3(str, "light_ambient", sl->light_ambient);
    WRITE_SOLIDLIGHT(str, sl->light, 0);
    WRITE_SOLIDLIGHT(str, sl->light, 1);
    WRITE_SOLIDLIGHT(str, sl->light, 2);
    WRITE_SOLIDLIGHT(str, sl->light, 3);

    char *cstr = BLI_dynstr_get_cstring(str);

    fwrite(cstr, BLI_dynstr_get_len(str), 1, fp);
    fclose(fp);

    MEM_freeN(cstr);
    BLI_dynstr_free(str);
  }
}

#undef WRITE_SOLIDLIGHT
#undef WRITE_VEC3
#undef WRITE_IVAL
#undef WRITE_FVAL

static void direction_to_equirect(float r[2], const float dir[3])
{
  r[0] = (atan2f(dir[1], dir[0]) - M_PI) / -(M_PI * 2);
  r[1] = (acosf(dir[2] / 1.0) - M_PI) / -M_PI;
}

namespace {

struct MultilayerConvertContext {
  int num_diffuse_channels;
  float *diffuse_pass;
  int num_specular_channels;
  float *specular_pass;
};

}  // namespace

static void *studiolight_multilayer_addview(void * /*base*/, const char * /*view_name*/)
{
  return nullptr;
}
static void *studiolight_multilayer_addlayer(void *base, const char * /*layer_name*/)
{
  return base;
}

/* Convert a multilayer pass to ImBuf channel 4 float buffer.
 * NOTE: Parameter rect will become invalid. Do not use rect after calling this
 * function */
static float *studiolight_multilayer_convert_pass(const ImBuf *ibuf,
                                                  float *rect,
                                                  const uint channels)
{
  if (channels == 4) {
    return rect;
  }

  float *new_rect = MEM_calloc_arrayN<float>(4 * size_t(ibuf->x) * size_t(ibuf->y), __func__);

  IMB_buffer_float_from_float(new_rect,
                              rect,
                              channels,
                              IB_PROFILE_LINEAR_RGB,
                              IB_PROFILE_LINEAR_RGB,
                              false,
                              ibuf->x,
                              ibuf->y,
                              ibuf->x,
                              ibuf->x);

  MEM_freeN(rect);
  return new_rect;
}

static void studiolight_multilayer_addpass(void *base,
                                           void * /*lay*/,
                                           const char *pass_name,
                                           float *rect,
                                           int num_channels,
                                           const char * /*chan_id*/,
                                           const char * /*view_name*/)
{
  MultilayerConvertContext *ctx = static_cast<MultilayerConvertContext *>(base);
  /* NOTE: This function must free pass pixels data if it is not used, this
   * is how IMB_exr_multilayer_convert() is working. */
  /* If we've found a first combined pass, skip all the rest ones. */
  if (STREQ(pass_name, STUDIOLIGHT_PASSNAME_DIFFUSE)) {
    ctx->diffuse_pass = rect;
    ctx->num_diffuse_channels = num_channels;
  }
  else if (STREQ(pass_name, STUDIOLIGHT_PASSNAME_SPECULAR)) {
    ctx->specular_pass = rect;
    ctx->num_specular_channels = num_channels;
  }
  else {
    MEM_freeN(rect);
  }
}

static void studiolight_load_equirect_image(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    ImBuf *ibuf = IMB_load_image_from_filepath(sl->filepath, IB_multilayer | IB_alphamode_ignore);
    ImBuf *specular_ibuf = nullptr;
    ImBuf *diffuse_ibuf = nullptr;
    const bool failed = (ibuf == nullptr);

    if (ibuf) {
      if (ibuf->ftype == IMB_FTYPE_OPENEXR && ibuf->exrhandle) {
        /* the read file is a multilayered openexr file (exrhandle != nullptr)
         * This file is currently only supported for MATCAPS where
         * the first found 'diffuse' pass will be used for diffuse lighting
         * and the first found 'specular' pass will be used for specular lighting */
        MultilayerConvertContext ctx = {0};
        IMB_exr_multilayer_convert(ibuf->exrhandle,
                                   &ctx,
                                   &studiolight_multilayer_addview,
                                   &studiolight_multilayer_addlayer,
                                   &studiolight_multilayer_addpass);

        /* `ctx.diffuse_pass` and `ctx.specular_pass` can be freed inside
         * `studiolight_multilayer_convert_pass` when conversion happens.
         * When not converted we move the ownership of the buffer to the
         * `converted_pass`. We only need to free `converted_pass` as it holds
         * the unmodified allocation from the `ctx.*_pass` or the converted data.
         */
        if (ctx.diffuse_pass != nullptr) {
          float *converted_pass = studiolight_multilayer_convert_pass(
              ibuf, ctx.diffuse_pass, ctx.num_diffuse_channels);
          diffuse_ibuf = IMB_allocFromBufferOwn(
              nullptr, converted_pass, ibuf->x, ibuf->y, ctx.num_diffuse_channels);
        }

        if (ctx.specular_pass != nullptr) {
          float *converted_pass = studiolight_multilayer_convert_pass(
              ibuf, ctx.specular_pass, ctx.num_specular_channels);
          specular_ibuf = IMB_allocFromBufferOwn(
              nullptr, converted_pass, ibuf->x, ibuf->y, ctx.num_specular_channels);
        }

        IMB_exr_close(ibuf->exrhandle);
        ibuf->exrhandle = nullptr;
        IMB_freeImBuf(ibuf);
        ibuf = nullptr;
      }
      else {
        /* read file is an single layer openexr file or the read file isn't
         * an openexr file */
        IMB_float_from_byte(ibuf);
        diffuse_ibuf = ibuf;
        ibuf = nullptr;
      }
    }

    if (diffuse_ibuf == nullptr) {
      /* Create 1x1 diffuse buffer, in case image failed to load or if there was
       * only a specular pass in the multilayer file or no passes were found. */
      const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
      const float magenta[4] = {1.0f, 0.0f, 1.0f, 1.0f};
      diffuse_ibuf = IMB_allocFromBuffer(
          nullptr, (failed || (specular_ibuf == nullptr)) ? magenta : black, 1, 1, 4);
    }

    if (sl->flag & STUDIOLIGHT_TYPE_MATCAP) {
      sl->matcap_diffuse.ibuf = diffuse_ibuf;
      sl->matcap_specular.ibuf = specular_ibuf;
      if (specular_ibuf != nullptr) {
        sl->flag |= STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS;
      }
    }
    else {
      sl->equirect_radiance_buffer = diffuse_ibuf;
      if (specular_ibuf != nullptr) {
        IMB_freeImBuf(specular_ibuf);
      }
    }
  }

  sl->flag |= STUDIOLIGHT_EXTERNAL_IMAGE_LOADED;
}

static void studiolight_create_equirect_radiance_gputexture(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
    ImBuf *ibuf = sl->equirect_radiance_buffer;

    sl->equirect_radiance_gputexture = GPU_texture_create_2d(
        "studiolight_radiance",
        ibuf->x,
        ibuf->y,
        1,
        blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
        GPU_TEXTURE_USAGE_SHADER_READ,
        ibuf->float_buffer.data);
    blender::gpu::Texture *tex = sl->equirect_radiance_gputexture;
    GPU_texture_filter_mode(tex, true);
    GPU_texture_extend_mode(tex, GPU_SAMPLER_EXTEND_MODE_REPEAT);
  }
  sl->flag |= STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE;
}

static void studiolight_create_matcap_gputexture(StudioLightImage *sli)
{
  BLI_assert(sli->ibuf);
  ImBuf *ibuf = sli->ibuf;
  const size_t ibuf_pixel_count = IMB_get_pixel_count(ibuf);
  float *gpu_matcap_3components = MEM_calloc_arrayN<float>(3 * ibuf_pixel_count, __func__);

  const float (*offset4)[4] = (const float (*)[4])ibuf->float_buffer.data;
  float (*offset3)[3] = (float (*)[3])gpu_matcap_3components;
  for (size_t i = 0; i < ibuf_pixel_count; i++, offset4++, offset3++) {
    copy_v3_v3(*offset3, *offset4);
  }

  sli->gputexture = GPU_texture_create_2d("matcap",
                                          ibuf->x,
                                          ibuf->y,
                                          1,
                                          blender::gpu::TextureFormat::UFLOAT_11_11_10,
                                          GPU_TEXTURE_USAGE_SHADER_READ,
                                          nullptr);
  GPU_texture_update(sli->gputexture, GPU_DATA_FLOAT, gpu_matcap_3components);

  MEM_SAFE_FREE(gpu_matcap_3components);
}

static void studiolight_create_matcap_diffuse_gputexture(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    if (sl->flag & STUDIOLIGHT_TYPE_MATCAP) {
      BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
      studiolight_create_matcap_gputexture(&sl->matcap_diffuse);
    }
  }
  sl->flag |= STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE;
}
static void studiolight_create_matcap_specular_gputexture(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    if (sl->flag & STUDIOLIGHT_TYPE_MATCAP) {
      BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
      if (sl->matcap_specular.ibuf) {
        studiolight_create_matcap_gputexture(&sl->matcap_specular);
      }
    }
  }
  sl->flag |= STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE;
}

static float4 studiolight_calculate_radiance(const ImBuf *ibuf, const float direction[3])
{
  float uv[2];
  direction_to_equirect(uv, direction);
  return blender::imbuf::interpolate_nearest_border_fl(ibuf, uv[0] * ibuf->x, uv[1] * ibuf->y);
}

/*
 * Spherical Harmonics
 */
BLI_INLINE float area_element(float x, float y)
{
  return atan2(x * y, sqrtf(x * x + y * y + 1));
}

static float brdf_approx(float spec_color, float roughness, float NV)
{
  /* Very rough approximation. We don't need it to be correct, just fast.
   * Just simulate fresnel effect with roughness attenuation. */
  float fresnel = exp2(-8.35f * NV) * (1.0f - roughness);
  return spec_color * (1.0f - fresnel) + fresnel;
}

/* NL need to be unclamped. w in [0..1] range. */
static float wrapped_lighting(float NL, float w)
{
  float w_1 = w + 1.0f;
  return max_ff((NL + w) / (w_1 * w_1), 0.0f);
}

static float blinn_specular(const float L[3],
                            const float I[3],
                            const float N[3],
                            const float R[3],
                            float NL,
                            float roughness,
                            float wrap)
{
  float half_dir[3];
  float wrapped_NL = dot_v3v3(L, R);
  add_v3_v3v3(half_dir, L, I);
  normalize_v3(half_dir);
  float spec_angle = max_ff(dot_v3v3(half_dir, N), 0.0f);

  float gloss = 1.0f - roughness;
  /* Reduce gloss for smooth light. (simulate bigger light) */
  gloss *= 1.0f - wrap;
  float shininess = exp2(10.0f * gloss + 1.0f);

  /* Pi is already divided in the light power.
   * normalization_factor = (shininess + 8.0) / (8.0 * M_PI) */
  float normalization_factor = shininess * 0.125f + 1.0f;
  float spec_light = powf(spec_angle, shininess) * max_ff(NL, 0.0f) * normalization_factor;

  /* Simulate Env. light. */
  float w = wrap * (1.0 - roughness) + roughness;
  float spec_env = wrapped_lighting(wrapped_NL, w);

  float w2 = wrap * wrap;

  return spec_light * (1.0 - w2) + spec_env * w2;
}

/* Keep in sync with the GLSL shader function `get_world_lighting()`. */
static void studiolight_lights_eval(StudioLight *sl, const float normal[3], float r_color[3])
{
  float R[3], I[3] = {0.0f, 0.0f, 1.0f}, N[3] = {normal[0], normal[2], -normal[1]};
  const float roughness = 0.5f;
  const float diffuse_color = 0.8f;
  const float specular_color = brdf_approx(0.05f, roughness, N[2]);
  float diff_light[3], spec_light[3];

  /* Ambient lighting */
  copy_v3_v3(diff_light, sl->light_ambient);
  copy_v3_v3(spec_light, sl->light_ambient);

  reflect_v3_v3v3(R, I, N);
  for (int i = 0; i < STUDIOLIGHT_MAX_LIGHT; i++) {
    SolidLight *light = &sl->light[i];
    if (light->flag) {
      /* Diffuse lighting */
      float NL = dot_v3v3(light->vec, N);
      float diff = wrapped_lighting(NL, light->smooth);
      madd_v3_v3fl(diff_light, light->col, diff);
      /* Specular lighting */
      float spec = blinn_specular(light->vec, I, N, R, NL, roughness, light->smooth);
      madd_v3_v3fl(spec_light, light->spec, spec);
    }
  }

  /* Multiply result by surface colors. */
  mul_v3_fl(diff_light, diffuse_color * (1.0 - specular_color));
  mul_v3_fl(spec_light, specular_color);

  add_v3_v3v3(r_color, diff_light, spec_light);
}

static StudioLight *studiolight_add_file(const char *filepath, int flag)
{
  char filename[FILE_MAXFILE];
  BLI_path_split_file_part(filepath, filename, FILE_MAXFILE);

  if ((((flag & STUDIOLIGHT_TYPE_STUDIO) != 0) && BLI_path_extension_check(filename, ".sl")) ||
      BLI_path_extension_check_array(filename, imb_ext_image))
  {
    StudioLight *sl = studiolight_create(STUDIOLIGHT_EXTERNAL_FILE | flag);
    STRNCPY(sl->name, filename);
    STRNCPY(sl->filepath, filepath);

    if ((flag & STUDIOLIGHT_TYPE_STUDIO) != 0) {
      studiolight_load_solid_light(sl);
    }
    BLI_addtail(&studiolights, sl);
    return sl;
  }
  return nullptr;
}

static void studiolight_add_files_from_datafolder(const int folder_id,
                                                  const char *subfolder,
                                                  int flag)
{
  const std::optional<std::string> folder = BKE_appdir_folder_id(folder_id, subfolder);
  if (!folder) {
    return;
  }

  direntry *dirs;
  const uint dirs_num = BLI_filelist_dir_contents(folder->c_str(), &dirs);
  int i;
  for (i = 0; i < dirs_num; i++) {
    if (dirs[i].type & S_IFREG) {
      studiolight_add_file(dirs[i].path, flag);
    }
  }
  BLI_filelist_free(dirs, dirs_num);
  dirs = nullptr;
}

static int studiolight_flag_cmp_order(const StudioLight *sl)
{
  /* Internal studiolights before external studio lights */
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    return 1;
  }
  return 0;
}

static int studiolight_cmp(const void *a, const void *b)
{
  const StudioLight *sl1 = static_cast<const StudioLight *>(a);
  const StudioLight *sl2 = static_cast<const StudioLight *>(b);

  const int flagorder1 = studiolight_flag_cmp_order(sl1);
  const int flagorder2 = studiolight_flag_cmp_order(sl2);

  if (flagorder1 < flagorder2) {
    return -1;
  }
  if (flagorder1 > flagorder2) {
    return 1;
  }

  return BLI_strcasecmp(sl1->name, sl2->name);
}

/* icons */

/* Takes normalized uvs as parameter (range from 0 to 1).
 * inner_edge and outer_edge are distances (from the center)
 * in uv space for the alpha mask falloff. */
static uint alpha_circle_mask(float u, float v, float inner_edge, float outer_edge)
{
  /* Coords from center. */
  const float co[2] = {u - 0.5f, v - 0.5f};
  float dist = len_v2(co);
  float alpha = 1.0f + (inner_edge - dist) / (outer_edge - inner_edge);
  uint mask = uint(floorf(255.0f * min_ff(max_ff(alpha, 0.0f), 1.0f)));
  return mask << 24;
}

/* Percentage of the icon that the preview sphere covers. */
#define STUDIOLIGHT_DIAMETER 0.95f
/* Rescale coord around (0.5, 0.5) by STUDIOLIGHT_DIAMETER. */
#define RESCALE_COORD(x) (x / STUDIOLIGHT_DIAMETER - (1.0f - STUDIOLIGHT_DIAMETER) / 2.0f)

/* Remaps normalized UV [0..1] to a sphere normal around (0.5, 0.5) */
static void sphere_normal_from_uv(float normal[3], float u, float v)
{
  normal[0] = u * 2.0f - 1.0f;
  normal[1] = v * 2.0f - 1.0f;
  float dist = len_v2(normal);
  normal[2] = sqrtf(1.0f - square_f(dist));
}

static void studiolight_radiance_preview(uint *icon_buffer, StudioLight *sl)
{
  BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);

  ITER_PIXELS (uint, icon_buffer, 1, STUDIOLIGHT_ICON_SIZE, STUDIOLIGHT_ICON_SIZE) {
    float dy = RESCALE_COORD(y);
    float dx = RESCALE_COORD(x);

    uint alphamask = alpha_circle_mask(dx, dy, 0.5f - texel_size[0], 0.5f);
    if (alphamask != 0) {
      float normal[3], direction[3];
      const float incoming[3] = {0.0f, 0.0f, -1.0f};
      sphere_normal_from_uv(normal, dx, dy);
      reflect_v3_v3v3(direction, incoming, normal);
      /* We want to see horizon not poles. */
      std::swap(direction[1], direction[2]);
      direction[1] = -direction[1];

      float4 color = studiolight_calculate_radiance(sl->equirect_radiance_buffer, direction);

      *pixel = rgb_to_cpack(linearrgb_to_srgb(color[0]),
                            linearrgb_to_srgb(color[1]),
                            linearrgb_to_srgb(color[2])) |
               alphamask;
    }
    else {
      *pixel = 0x0;
    }
  }
  ITER_PIXELS_END;
}

static void studiolight_matcap_preview(uint *icon_buffer, StudioLight *sl, bool flipped)
{
  using namespace blender;

  BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);

  ImBuf *diffuse_buffer = sl->matcap_diffuse.ibuf;
  ImBuf *specular_buffer = sl->matcap_specular.ibuf;

  ITER_PIXELS (uint, icon_buffer, 1, STUDIOLIGHT_ICON_SIZE, STUDIOLIGHT_ICON_SIZE) {
    float dy = RESCALE_COORD(y);
    float dx = RESCALE_COORD(x);
    if (flipped) {
      dx = 1.0f - dx;
    }

    float u = dx * diffuse_buffer->x - 1.0f;
    float v = dy * diffuse_buffer->y - 1.0f;
    float4 color = imbuf::interpolate_nearest_border_fl(diffuse_buffer, u, v);

    if (specular_buffer) {
      float4 specular = imbuf::interpolate_nearest_border_fl(specular_buffer, u, v);
      add_v3_v3(color, specular);
    }

    uint alphamask = alpha_circle_mask(dx, dy, 0.5f - texel_size[0], 0.5f);

    *pixel = rgb_to_cpack(linearrgb_to_srgb(color[0]),
                          linearrgb_to_srgb(color[1]),
                          linearrgb_to_srgb(color[2])) |
             alphamask;
  }
  ITER_PIXELS_END;
}

static void studiolight_irradiance_preview(uint *icon_buffer, StudioLight *sl)
{
  ITER_PIXELS (uint, icon_buffer, 1, STUDIOLIGHT_ICON_SIZE, STUDIOLIGHT_ICON_SIZE) {
    float dy = RESCALE_COORD(y);
    float dx = RESCALE_COORD(x);

    uint alphamask = alpha_circle_mask(dx, dy, 0.5f - texel_size[0], 0.5f);
    if (alphamask != 0) {
      float normal[3], color[3];
      sphere_normal_from_uv(normal, dx, dy);
      /* We want to see horizon not poles. */
      std::swap(normal[1], normal[2]);
      normal[1] = -normal[1];

      studiolight_lights_eval(sl, normal, color);

      *pixel = rgb_to_cpack(linearrgb_to_srgb(color[0]),
                            linearrgb_to_srgb(color[1]),
                            linearrgb_to_srgb(color[2])) |
               alphamask;
    }
    else {
      *pixel = 0x0;
    }
  }
  ITER_PIXELS_END;
}

void BKE_studiolight_default(SolidLight lights[4], float light_ambient[3])
{
  copy_v3_fl3(light_ambient, 0.0, 0.0, 0.0);

  lights[0].flag = 1;
  lights[0].smooth = 0.526620f;
  lights[0].col[0] = 0.033103f;
  lights[0].col[1] = 0.033103f;
  lights[0].col[2] = 0.033103f;
  lights[0].spec[0] = 0.266761f;
  lights[0].spec[1] = 0.266761f;
  lights[0].spec[2] = 0.266761f;
  lights[0].vec[0] = -0.352546f;
  lights[0].vec[1] = 0.170931f;
  lights[0].vec[2] = -0.920051f;

  lights[1].flag = 1;
  lights[1].smooth = 0.000000f;
  lights[1].col[0] = 0.521083f;
  lights[1].col[1] = 0.538226f;
  lights[1].col[2] = 0.538226f;
  lights[1].spec[0] = 0.599030f;
  lights[1].spec[1] = 0.599030f;
  lights[1].spec[2] = 0.599030f;
  lights[1].vec[0] = -0.408163f;
  lights[1].vec[1] = 0.346939f;
  lights[1].vec[2] = 0.844415f;

  lights[2].flag = 1;
  lights[2].smooth = 0.478261f;
  lights[2].col[0] = 0.038403f;
  lights[2].col[1] = 0.034357f;
  lights[2].col[2] = 0.049530f;
  lights[2].spec[0] = 0.106102f;
  lights[2].spec[1] = 0.125981f;
  lights[2].spec[2] = 0.158523f;
  lights[2].vec[0] = 0.521739f;
  lights[2].vec[1] = 0.826087f;
  lights[2].vec[2] = 0.212999f;

  lights[3].flag = 1;
  lights[3].smooth = 0.200000f;
  lights[3].col[0] = 0.090838f;
  lights[3].col[1] = 0.082080f;
  lights[3].col[2] = 0.072255f;
  lights[3].spec[0] = 0.106535f;
  lights[3].spec[1] = 0.084771f;
  lights[3].spec[2] = 0.066080f;
  lights[3].vec[0] = 0.624519f;
  lights[3].vec[1] = -0.562067f;
  lights[3].vec[2] = -0.542269f;
}

void BKE_studiolight_init()
{
  /* Add default studio light */
  StudioLight *sl = studiolight_create(STUDIOLIGHT_INTERNAL | STUDIOLIGHT_TYPE_STUDIO |
                                       STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS);
  STRNCPY(sl->name, "Default");

  BLI_addtail(&studiolights, sl);

  /* Go over the preset folder and add a studio-light for every image with its path. */
  /* Also reserve icon space for it. */
  studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,
                                        STUDIOLIGHT_LIGHTS_FOLDER,
                                        STUDIOLIGHT_TYPE_STUDIO | STUDIOLIGHT_USER_DEFINED |
                                            STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS);
  studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,
                                        STUDIOLIGHT_WORLD_FOLDER,
                                        STUDIOLIGHT_TYPE_WORLD | STUDIOLIGHT_USER_DEFINED);
  studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,
                                        STUDIOLIGHT_MATCAP_FOLDER,
                                        STUDIOLIGHT_TYPE_MATCAP | STUDIOLIGHT_USER_DEFINED);
  studiolight_add_files_from_datafolder(BLENDER_SYSTEM_DATAFILES,
                                        STUDIOLIGHT_LIGHTS_FOLDER,
                                        STUDIOLIGHT_TYPE_STUDIO |
                                            STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS);
  studiolight_add_files_from_datafolder(
      BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_WORLD_FOLDER, STUDIOLIGHT_TYPE_WORLD);
  studiolight_add_files_from_datafolder(
      BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_MATCAP_FOLDER, STUDIOLIGHT_TYPE_MATCAP);

  /* sort studio lights on filename. */
  BLI_listbase_sort(&studiolights, studiolight_cmp);

  BKE_studiolight_default(sl->light, sl->light_ambient);
}

void BKE_studiolight_free()
{
  while (StudioLight *sl = static_cast<StudioLight *>(BLI_pophead(&studiolights))) {
    studiolight_free(sl);
  }
}

StudioLight *BKE_studiolight_find_default(int flag)
{
  const char *default_name = "";

  if (flag & STUDIOLIGHT_TYPE_WORLD) {
    default_name = STUDIOLIGHT_WORLD_DEFAULT;
  }
  else if (flag & STUDIOLIGHT_TYPE_MATCAP) {
    default_name = STUDIOLIGHT_MATCAP_DEFAULT;
  }

  LISTBASE_FOREACH (StudioLight *, sl, &studiolights) {
    if ((sl->flag & flag) && STREQ(sl->name, default_name)) {
      return sl;
    }
  }

  LISTBASE_FOREACH (StudioLight *, sl, &studiolights) {
    if (sl->flag & flag) {
      return sl;
    }
  }
  return nullptr;
}

StudioLight *BKE_studiolight_find(const char *name, int flag)
{
  LISTBASE_FOREACH (StudioLight *, sl, &studiolights) {
    if (STREQLEN(sl->name, name, FILE_MAXFILE)) {
      if (sl->flag & flag) {
        return sl;
      }

      /* flags do not match, so use default */
      return BKE_studiolight_find_default(flag);
    }
  }
  /* When not found, use the default studio light */
  return BKE_studiolight_find_default(flag);
}

StudioLight *BKE_studiolight_findindex(int index, int flag)
{
  LISTBASE_FOREACH (StudioLight *, sl, &studiolights) {
    if (sl->index == index) {
      return sl;
    }
  }
  /* When not found, use the default studio light */
  return BKE_studiolight_find_default(flag);
}

ListBase *BKE_studiolight_listbase()
{
  return &studiolights;
}

void BKE_studiolight_preview(uint *icon_buffer, StudioLight *sl, int icon_id_type)
{
  switch (icon_id_type) {
    case STUDIOLIGHT_ICON_ID_TYPE_RADIANCE:
    default: {
      studiolight_radiance_preview(icon_buffer, sl);
      break;
    }
    case STUDIOLIGHT_ICON_ID_TYPE_IRRADIANCE: {
      studiolight_irradiance_preview(icon_buffer, sl);
      break;
    }
    case STUDIOLIGHT_ICON_ID_TYPE_MATCAP: {
      studiolight_matcap_preview(icon_buffer, sl, false);
      break;
    }
    case STUDIOLIGHT_ICON_ID_TYPE_MATCAP_FLIPPED: {
      studiolight_matcap_preview(icon_buffer, sl, true);
      break;
    }
  }
  studiolight_free_temp_resources(sl);
}

void BKE_studiolight_ensure_flag(StudioLight *sl, int flag)
{
  if ((sl->flag & flag) == flag) {
    return;
  }

  if (flag & STUDIOLIGHT_EXTERNAL_IMAGE_LOADED) {
    studiolight_load_equirect_image(sl);
  }
  if (flag & STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE) {
    studiolight_create_equirect_radiance_gputexture(sl);
  }
  if (flag & STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE) {
    studiolight_create_matcap_diffuse_gputexture(sl);
  }
  if (flag & STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE) {
    studiolight_create_matcap_specular_gputexture(sl);
  }
}

/*
 * Python API Functions.
 */

void BKE_studiolight_remove(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
    BLI_remlink(&studiolights, sl);
    studiolight_free(sl);
  }
}

StudioLight *BKE_studiolight_load(const char *filepath, int type)
{
  StudioLight *sl = studiolight_add_file(filepath, type | STUDIOLIGHT_USER_DEFINED);
  return sl;
}

StudioLight *BKE_studiolight_create(const char *filepath,
                                    const SolidLight light[4],
                                    const float light_ambient[3])
{
  StudioLight *sl = studiolight_create(STUDIOLIGHT_EXTERNAL_FILE | STUDIOLIGHT_USER_DEFINED |
                                       STUDIOLIGHT_TYPE_STUDIO |
                                       STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS);

  char filename[FILE_MAXFILE];
  BLI_path_split_file_part(filepath, filename, FILE_MAXFILE);
  STRNCPY(sl->filepath, filepath);
  STRNCPY(sl->name, filename);

  memcpy(sl->light, light, sizeof(*light) * 4);
  memcpy(sl->light_ambient, light_ambient, sizeof(*light_ambient) * 3);

  studiolight_write_solid_light(sl);

  BLI_addtail(&studiolights, sl);
  return sl;
}

StudioLight *BKE_studiolight_studio_edit_get()
{
  static StudioLight sl = {nullptr};
  sl.flag = STUDIOLIGHT_TYPE_STUDIO | STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS;

  memcpy(sl.light, U.light_param, sizeof(*sl.light) * 4);
  memcpy(sl.light_ambient, U.light_ambient, sizeof(*sl.light_ambient) * 3);

  return &sl;
}

void BKE_studiolight_refresh()
{
  BKE_studiolight_free();
  BKE_studiolight_init();
}

void BKE_studiolight_set_free_function(StudioLight *sl,
                                       StudioLightFreeFunction *free_function,
                                       void *data)
{
  sl->free_function = free_function;
  sl->free_function_data = data;
}

void BKE_studiolight_unset_icon_id(StudioLight *sl, int icon_id)
{
  BLI_assert(sl != nullptr);
  if (sl->icon_id_radiance == icon_id) {
    sl->icon_id_radiance = 0;
  }
  if (sl->icon_id_irradiance == icon_id) {
    sl->icon_id_irradiance = 0;
  }
  if (sl->icon_id_matcap == icon_id) {
    sl->icon_id_matcap = 0;
  }
  if (sl->icon_id_matcap_flipped == icon_id) {
    sl->icon_id_matcap_flipped = 0;
  }
}
