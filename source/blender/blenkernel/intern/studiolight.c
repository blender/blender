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
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_studiolight.h"

#include "BKE_appdir.h"
#include "BKE_icons.h"

#include "BLI_dynstr.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_listBase.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_texture.h"

#include "MEM_guardedalloc.h"

#include "intern/openexr/openexr_multi.h"

/* Statics */
static ListBase studiolights;
static int last_studiolight_id = 0;
#define STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE 96
#define STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT 32
#define STUDIOLIGHT_IRRADIANCE_EQUIRECT_WIDTH (STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT * 2)
#define STUDIOLIGHT_PASSNAME_DIFFUSE "diffuse"
#define STUDIOLIGHT_PASSNAME_SPECULAR "specular"
/* Temporarily disabled due to the creation of textures with -nan(ind)s */
#define STUDIOLIGHT_SH_WINDOWING 0.0f /* 0.0 is disabled */

/*
 * Disable this option so caches are not loaded from disk
 * Do not checking with this commented out.
 */
#define STUDIOLIGHT_LOAD_CACHED_FILES

static const char *STUDIOLIGHT_LIGHTS_FOLDER = "studiolights/studio/";
static const char *STUDIOLIGHT_WORLD_FOLDER = "studiolights/world/";
static const char *STUDIOLIGHT_MATCAP_FOLDER = "studiolights/matcap/";

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
      p = NULL; \
    } \
  } while (0)

#define GPU_TEXTURE_SAFE_FREE(p) \
  do { \
    if (p) { \
      GPU_texture_free(p); \
      p = NULL; \
    } \
  } while (0)

static void studiolight_free(struct StudioLight *sl)
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

  for (int index = 0; index < 6; index++) {
    IMB_SAFE_FREE(sl->radiance_cubemap_buffers[index]);
  }
  GPU_TEXTURE_SAFE_FREE(sl->equirect_radiance_gputexture);
  GPU_TEXTURE_SAFE_FREE(sl->equirect_irradiance_gputexture);
  IMB_SAFE_FREE(sl->equirect_radiance_buffer);
  IMB_SAFE_FREE(sl->equirect_irradiance_buffer);
  GPU_TEXTURE_SAFE_FREE(sl->matcap_diffuse.gputexture);
  GPU_TEXTURE_SAFE_FREE(sl->matcap_specular.gputexture);
  IMB_SAFE_FREE(sl->matcap_diffuse.ibuf);
  IMB_SAFE_FREE(sl->matcap_specular.ibuf);
  MEM_SAFE_FREE(sl->path_irr_cache);
  MEM_SAFE_FREE(sl->path_sh_cache);
  MEM_SAFE_FREE(sl);
}

static struct StudioLight *studiolight_create(int flag)
{
  struct StudioLight *sl = MEM_callocN(sizeof(*sl), __func__);
  sl->path[0] = 0x00;
  sl->name[0] = 0x00;
  sl->path_irr_cache = NULL;
  sl->path_sh_cache = NULL;
  sl->free_function = NULL;
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

  for (int index = 0; index < 6; index++) {
    sl->radiance_cubemap_buffers[index] = NULL;
  }

  return sl;
}

#define STUDIOLIGHT_FILE_VERSION 1

#define READ_VAL(type, parser, id, val, lines) \
  do { \
    for (LinkNode *line = lines; line; line = line->next) { \
      char *val_str, *str = line->link; \
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
  LinkNode *lines = BLI_file_read_as_lines(sl->path);
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

#define WRITE_FVAL(str, id, val) (BLI_dynstr_appendf(str, id " %f\n", val))
#define WRITE_IVAL(str, id, val) (BLI_dynstr_appendf(str, id " %d\n", val))

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
  FILE *fp = BLI_fopen(sl->path, "wb");
  if (fp) {
    DynStr *str = BLI_dynstr_new();

    /* Very dumb ascii format. One value per line separated by a space. */
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

static void equirect_to_direction(float r[3], float u, float v)
{
  float phi = (-(M_PI * 2)) * u + M_PI;
  float theta = -M_PI * v + M_PI;
  float sin_theta = sinf(theta);
  r[0] = sin_theta * cosf(phi);
  r[1] = sin_theta * sinf(phi);
  r[2] = cosf(theta);
}

static void UNUSED_FUNCTION(direction_to_cube_face_uv)(float r_uv[2],
                                                       int *r_face,
                                                       const float dir[3])
{
  if (fabsf(dir[0]) > fabsf(dir[1]) && fabsf(dir[0]) > fabsf(dir[2])) {
    bool is_pos = (dir[0] > 0.0f);
    *r_face = is_pos ? STUDIOLIGHT_X_POS : STUDIOLIGHT_X_NEG;
    r_uv[0] = dir[2] / fabsf(dir[0]) * (is_pos ? 1 : -1);
    r_uv[1] = dir[1] / fabsf(dir[0]) * (is_pos ? -1 : -1);
  }
  else if (fabsf(dir[1]) > fabsf(dir[0]) && fabsf(dir[1]) > fabsf(dir[2])) {
    bool is_pos = (dir[1] > 0.0f);
    *r_face = is_pos ? STUDIOLIGHT_Y_POS : STUDIOLIGHT_Y_NEG;
    r_uv[0] = dir[0] / fabsf(dir[1]) * (is_pos ? 1 : 1);
    r_uv[1] = dir[2] / fabsf(dir[1]) * (is_pos ? -1 : 1);
  }
  else {
    bool is_pos = (dir[2] > 0.0f);
    *r_face = is_pos ? STUDIOLIGHT_Z_NEG : STUDIOLIGHT_Z_POS;
    r_uv[0] = dir[0] / fabsf(dir[2]) * (is_pos ? -1 : 1);
    r_uv[1] = dir[1] / fabsf(dir[2]) * (is_pos ? -1 : -1);
  }
  r_uv[0] = r_uv[0] * 0.5f + 0.5f;
  r_uv[1] = r_uv[1] * 0.5f + 0.5f;
}

static void cube_face_uv_to_direction(float r_dir[3], float x, float y, int face)
{
  const float conversion_matrices[6][3][3] = {
      {{0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
      {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
      {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
      {{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
      {{-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
  };

  copy_v3_fl3(r_dir, x * 2.0f - 1.0f, y * 2.0f - 1.0f, 1.0f);
  mul_m3_v3(conversion_matrices[face], r_dir);
  normalize_v3(r_dir);
}

typedef struct MultilayerConvertContext {
  int num_diffuse_channels;
  float *diffuse_pass;
  int num_specular_channels;
  float *specular_pass;
} MultilayerConvertContext;

static void *studiolight_multilayer_addview(void *UNUSED(base), const char *UNUSED(view_name))
{
  return NULL;
}
static void *studiolight_multilayer_addlayer(void *base, const char *UNUSED(layer_name))
{
  return base;
}

/* Convert a multilayer pass to ImBuf channel 4 float buffer.
 * NOTE: Parameter rect will become invalid. Do not use rect after calling this
 * function */
static float *studiolight_multilayer_convert_pass(ImBuf *ibuf,
                                                  float *rect,
                                                  const unsigned int channels)
{
  if (channels == 4) {
    return rect;
  }
  else {
    float *new_rect = MEM_callocN(sizeof(float[4]) * ibuf->x * ibuf->y, __func__);

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
}

static void studiolight_multilayer_addpass(void *base,
                                           void *UNUSED(lay),
                                           const char *pass_name,
                                           float *rect,
                                           int num_channels,
                                           const char *UNUSED(chan_id),
                                           const char *UNUSED(view_name))
{
  MultilayerConvertContext *ctx = base;
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
    ImBuf *ibuf = IMB_loadiffname(sl->path, IB_multilayer, NULL);
    ImBuf *specular_ibuf = NULL;
    ImBuf *diffuse_ibuf = NULL;
    const bool failed = (ibuf == NULL);

    if (ibuf) {
      if (ibuf->ftype == IMB_FTYPE_OPENEXR && ibuf->userdata) {
        /* the read file is a multilayered openexr file (userdata != NULL)
         * This file is currently only supported for MATCAPS where
         * the first found 'diffuse' pass will be used for diffuse lighting
         * and the first found 'specular' pass will be used for specular lighting */
        MultilayerConvertContext ctx = {0};
        IMB_exr_multilayer_convert(ibuf->userdata,
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
        if (ctx.diffuse_pass != NULL) {
          float *converted_pass = studiolight_multilayer_convert_pass(
              ibuf, ctx.diffuse_pass, ctx.num_diffuse_channels);
          diffuse_ibuf = IMB_allocFromBuffer(
              NULL, converted_pass, ibuf->x, ibuf->y, ctx.num_diffuse_channels);
          MEM_freeN(converted_pass);
        }

        if (ctx.specular_pass != NULL) {
          float *converted_pass = studiolight_multilayer_convert_pass(
              ibuf, ctx.specular_pass, ctx.num_specular_channels);
          specular_ibuf = IMB_allocFromBuffer(
              NULL, converted_pass, ibuf->x, ibuf->y, ctx.num_specular_channels);
          MEM_freeN(converted_pass);
        }

        IMB_exr_close(ibuf->userdata);
        ibuf->userdata = NULL;
        IMB_freeImBuf(ibuf);
        ibuf = NULL;
      }
      else {
        /* read file is an single layer openexr file or the read file isn't
         * an openexr file */
        IMB_float_from_rect(ibuf);
        diffuse_ibuf = ibuf;
        ibuf = NULL;
      }
    }

    if (diffuse_ibuf == NULL) {
      /* Create 1x1 diffuse buffer, in case image failed to load or if there was
       * only a specular pass in the multilayer file or no passes were found. */
      const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
      const float magenta[4] = {1.0f, 0.0f, 1.0f, 1.0f};
      diffuse_ibuf = IMB_allocFromBuffer(
          NULL, (failed || (specular_ibuf == NULL)) ? magenta : black, 1, 1, 4);
    }

    if ((sl->flag & STUDIOLIGHT_TYPE_MATCAP)) {
      sl->matcap_diffuse.ibuf = diffuse_ibuf;
      sl->matcap_specular.ibuf = specular_ibuf;
      if (specular_ibuf != NULL) {
        sl->flag |= STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS;
      }
    }
    else {
      sl->equirect_radiance_buffer = diffuse_ibuf;
      if (specular_ibuf != NULL) {
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
        ibuf->x, ibuf->y, GPU_RGBA16F, ibuf->rect_float, NULL);
    GPUTexture *tex = sl->equirect_radiance_gputexture;
    GPU_texture_filter_mode(tex, true);
    GPU_texture_wrap_mode(tex, true, true);
  }
  sl->flag |= STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE;
}

static void studiolight_create_matcap_gputexture(StudioLightImage *sli)
{
  BLI_assert(sli->ibuf);
  ImBuf *ibuf = sli->ibuf;
  float *gpu_matcap_3components = MEM_callocN(sizeof(float[3]) * ibuf->x * ibuf->y, __func__);

  float(*offset4)[4] = (float(*)[4])ibuf->rect_float;
  float(*offset3)[3] = (float(*)[3])gpu_matcap_3components;
  for (int i = 0; i < ibuf->x * ibuf->y; i++, offset4++, offset3++) {
    copy_v3_v3(*offset3, *offset4);
  }

  sli->gputexture = GPU_texture_create_nD(ibuf->x,
                                          ibuf->y,
                                          0,
                                          2,
                                          gpu_matcap_3components,
                                          GPU_R11F_G11F_B10F,
                                          GPU_DATA_FLOAT,
                                          0,
                                          false,
                                          NULL);
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

static void studiolight_create_equirect_irradiance_gputexture(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECT_IRRADIANCE_IMAGE_CALCULATED);
    ImBuf *ibuf = sl->equirect_irradiance_buffer;
    sl->equirect_irradiance_gputexture = GPU_texture_create_2d(
        ibuf->x, ibuf->y, GPU_RGBA16F, ibuf->rect_float, NULL);
    GPUTexture *tex = sl->equirect_irradiance_gputexture;
    GPU_texture_filter_mode(tex, true);
    GPU_texture_wrap_mode(tex, true, true);
  }
  sl->flag |= STUDIOLIGHT_EQUIRECT_IRRADIANCE_GPUTEXTURE;
}

static void studiolight_calculate_radiance(ImBuf *ibuf, float color[4], const float direction[3])
{
  float uv[2];
  direction_to_equirect(uv, direction);
  nearest_interpolation_color_wrap(ibuf, NULL, color, uv[0] * ibuf->x, uv[1] * ibuf->y);
}

static void studiolight_calculate_radiance_buffer(ImBuf *ibuf,
                                                  float *colbuf,
                                                  const int index_x,
                                                  const int index_y,
                                                  const int index_z,
                                                  const float xsign,
                                                  const float ysign,
                                                  const float zsign)
{
  ITER_PIXELS (
      float, colbuf, 4, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) {
    float direction[3];
    direction[index_x] = xsign * (x - 0.5f);
    direction[index_y] = ysign * (y - 0.5f);
    direction[index_z] = zsign * 0.5f;
    normalize_v3(direction);
    studiolight_calculate_radiance(ibuf, pixel, direction);
  }
  ITER_PIXELS_END;
}

static void studiolight_calculate_radiance_cubemap_buffers(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
    ImBuf *ibuf = sl->equirect_radiance_buffer;
    if (ibuf) {
      float *colbuf = MEM_malloc_arrayN(
          square_i(STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE), sizeof(float[4]), __func__);

      /* front */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 2, 1, 1, -1, 1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, 4);

      /* back */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 2, 1, 1, 1, -1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, 4);

      /* left */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 2, 1, 0, 1, -1, 1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, 4);

      /* right */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 2, 1, 0, -1, -1, -1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, 4);

      /* top */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 1, 2, -1, -1, 1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, 4);

      /* bottom */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 1, 2, 1, -1, -1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, 4);

#if 0
      IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS],
                  "/tmp/studiolight_radiance_left.png",
                  IB_rectfloat);
      IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG],
                  "/tmp/studiolight_radiance_right.png",
                  IB_rectfloat);
      IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS],
                  "/tmp/studiolight_radiance_front.png",
                  IB_rectfloat);
      IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG],
                  "/tmp/studiolight_radiance_back.png",
                  IB_rectfloat);
      IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS],
                  "/tmp/studiolight_radiance_bottom.png",
                  IB_rectfloat);
      IMB_saveiff(sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG],
                  "/tmp/studiolight_radiance_top.png",
                  IB_rectfloat);
#endif
      MEM_freeN(colbuf);
    }
  }
  sl->flag |= STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED;
}

/*
 * Spherical Harmonics
 */
BLI_INLINE float area_element(float x, float y)
{
  return atan2(x * y, sqrtf(x * x + y * y + 1));
}

BLI_INLINE float texel_solid_angle(float x, float y, float halfpix)
{
  float v1x = (x - halfpix) * 2.0f - 1.0f;
  float v1y = (y - halfpix) * 2.0f - 1.0f;
  float v2x = (x + halfpix) * 2.0f - 1.0f;
  float v2y = (y + halfpix) * 2.0f - 1.0f;

  return area_element(v1x, v1y) - area_element(v1x, v2y) - area_element(v2x, v1y) +
         area_element(v2x, v2y);
}

static void studiolight_calculate_cubemap_vector_weight(
    float normal[3], float *weight, int face, float x, float y)
{
  const float halfpix = 0.5f / STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE;
  cube_face_uv_to_direction(normal, x, y, face);
  *weight = texel_solid_angle(x, y, halfpix);
}

static void studiolight_spherical_harmonics_calculate_coefficients(StudioLight *sl, float (*sh)[3])
{
  float weight_accum = 0.0f;
  memset(sh, 0, sizeof(float) * 3 * STUDIOLIGHT_SH_COEFS_LEN);

  for (int face = 0; face < 6; face++) {
    ITER_PIXELS (float,
                 sl->radiance_cubemap_buffers[face]->rect_float,
                 4,
                 STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE,
                 STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) {
      float color[3], cubevec[3], weight;
      studiolight_calculate_cubemap_vector_weight(cubevec, &weight, face, x, y);
      mul_v3_v3fl(color, pixel, weight);
      weight_accum += weight;

      int i = 0;
      /* L0 */
      madd_v3_v3fl(sh[i++], color, 0.2822095f);
#if STUDIOLIGHT_SH_BANDS > 1 /* L1 */
      const float nx = cubevec[0];
      const float ny = cubevec[1];
      const float nz = cubevec[2];
      madd_v3_v3fl(sh[i++], color, -0.488603f * nz);
      madd_v3_v3fl(sh[i++], color, 0.488603f * ny);
      madd_v3_v3fl(sh[i++], color, -0.488603f * nx);
#endif
#if STUDIOLIGHT_SH_BANDS > 2 /* L2 */
      const float nx2 = SQUARE(nx);
      const float ny2 = SQUARE(ny);
      const float nz2 = SQUARE(nz);
      madd_v3_v3fl(sh[i++], color, 1.092548f * nx * nz);
      madd_v3_v3fl(sh[i++], color, -1.092548f * nz * ny);
      madd_v3_v3fl(sh[i++], color, 0.315392f * (3.0f * ny2 - 1.0f));
      madd_v3_v3fl(sh[i++], color, 1.092548f * nx * ny);
      madd_v3_v3fl(sh[i++], color, 0.546274f * (nx2 - nz2));
#endif
      /* Bypass L3 Because final irradiance does not need it. */
#if STUDIOLIGHT_SH_BANDS > 4 /* L4 */
      const float nx4 = SQUARE(nx2);
      const float ny4 = SQUARE(ny2);
      const float nz4 = SQUARE(nz2);
      madd_v3_v3fl(sh[i++], color, 2.5033429417967046f * nx * nz * (nx2 - nz2));
      madd_v3_v3fl(sh[i++], color, -1.7701307697799304f * nz * ny * (3.0f * nx2 - nz2));
      madd_v3_v3fl(sh[i++], color, 0.9461746957575601f * nz * nx * (-1.0f + 7.0f * ny2));
      madd_v3_v3fl(sh[i++], color, -0.6690465435572892f * nz * ny * (-3.0f + 7.0f * ny2));
      madd_v3_v3fl(sh[i++], color, (105.0f * ny4 - 90.0f * ny2 + 9.0f) / 28.359261614f);
      madd_v3_v3fl(sh[i++], color, -0.6690465435572892f * nx * ny * (-3.0f + 7.0f * ny2));
      madd_v3_v3fl(sh[i++], color, 0.9461746957575601f * (nx2 - nz2) * (-1.0f + 7.0f * ny2));
      madd_v3_v3fl(sh[i++], color, -1.7701307697799304f * nx * ny * (nx2 - 3.0f * nz2));
      madd_v3_v3fl(sh[i++], color, 0.6258357354491761f * (nx4 - 6.0f * nz2 * nx2 + nz4));
#endif
    }
    ITER_PIXELS_END;
  }

  /* The sum of solid angle should be equal to the solid angle of the sphere (4 PI),
   * so normalize in order to make our weightAccum exactly match 4 PI. */
  for (int i = 0; i < STUDIOLIGHT_SH_COEFS_LEN; i++) {
    mul_v3_fl(sh[i], M_PI * 4.0f / weight_accum);
  }
}

/* Take monochrome SH as input */
static float studiolight_spherical_harmonics_lambda_get(float *sh, float max_laplacian)
{
  /* From Peter-Pike Sloan's Stupid SH Tricks http://www.ppsloan.org/publications/StupidSH36.pdf
   */
  float table_l[STUDIOLIGHT_SH_BANDS];
  float table_b[STUDIOLIGHT_SH_BANDS];

  float lambda = 0.0f;

  table_l[0] = 0.0f;
  table_b[0] = 0.0f;
  int index = 1;
  for (int level = 1; level < STUDIOLIGHT_SH_BANDS; level++) {
    table_l[level] = (float)(square_i(level) * square_i(level + 1));

    float b = 0.0f;
    for (int m = -1; m <= level; m++) {
      b += square_f(sh[index++]);
    }
    table_b[level] = b;
  }

  float squared_lamplacian = 0.0f;
  for (int level = 1; level < STUDIOLIGHT_SH_BANDS; level++) {
    squared_lamplacian += table_l[level] * table_b[level];
  }

  const float target_squared_laplacian = max_laplacian * max_laplacian;
  if (squared_lamplacian <= target_squared_laplacian) {
    return lambda;
  }

  const int no_iterations = 10000000;
  for (int i = 0; i < no_iterations; i++) {
    float f = 0.0f;
    float fd = 0.0f;

    for (int level = 1; level < STUDIOLIGHT_SH_BANDS; level++) {
      f += table_l[level] * table_b[level] / square_f(1.0f + lambda * table_l[level]);
      fd += (2.0f * square_f(table_l[level]) * table_b[level]) /
            cube_f(1.0f + lambda * table_l[level]);
    }

    f = target_squared_laplacian - f;

    float delta = -f / fd;
    lambda += delta;

    if (fabsf(delta) < 1e-6f) {
      break;
    }
  }

  return lambda;
}

static void studiolight_spherical_harmonics_apply_windowing(float (*sh)[3], float max_laplacian)
{
  if (max_laplacian <= 0.0f) {
    return;
  }

  float sh_r[STUDIOLIGHT_SH_COEFS_LEN];
  float sh_g[STUDIOLIGHT_SH_COEFS_LEN];
  float sh_b[STUDIOLIGHT_SH_COEFS_LEN];
  for (int i = 0; i < STUDIOLIGHT_SH_COEFS_LEN; i++) {
    sh_r[i] = sh[i][0];
    sh_g[i] = sh[i][1];
    sh_b[i] = sh[i][2];
  }
  float lambda_r = studiolight_spherical_harmonics_lambda_get(sh_r, max_laplacian);
  float lambda_g = studiolight_spherical_harmonics_lambda_get(sh_g, max_laplacian);
  float lambda_b = studiolight_spherical_harmonics_lambda_get(sh_b, max_laplacian);

  /* Apply windowing lambda */
  int index = 0;
  for (int level = 0; level < STUDIOLIGHT_SH_BANDS; level++) {
    float s[3];
    const int level_sq = square_i(level);
    const int level_1_sq = square_i(level + 1.0f);
    s[0] = 1.0f / (1.0f + lambda_r * level_sq * level_1_sq);
    s[1] = 1.0f / (1.0f + lambda_g * level_sq * level_1_sq);
    s[2] = 1.0f / (1.0f + lambda_b * level_sq * level_1_sq);

    for (int m = -1; m <= level; m++) {
      mul_v3_v3(sh[index++], s);
    }
  }
}

static float studiolight_spherical_harmonics_geomerics_eval(
    const float normal[3], float sh0, float sh1, float sh2, float sh3)
{
  /* Use Geomerics non-linear SH. */
  /* http://www.geomerics.com/wp-content/uploads/2015/08/CEDEC_Geomerics_ReconstructingDiffuseLighting1.pdf
   */
  float R0 = sh0 * M_1_PI;

  float R1[3] = {-sh3, sh2, -sh1};
  mul_v3_fl(R1, 0.5f * M_1_PI * 1.5f); /* 1.5f is to improve the contrast a bit. */
  float lenR1 = len_v3(R1);
  mul_v3_fl(R1, 1.0f / lenR1);
  float q = 0.5f * (1.0f + dot_v3v3(R1, normal));

  float p = 1.0f + 2.0f * lenR1 / R0;
  float a = (1.0f - lenR1 / R0) / (1.0f + lenR1 / R0);

  return R0 * (a + (1.0f - a) * (p + 1.0f) * powf(q, p));
}

BLI_INLINE void studiolight_spherical_harmonics_eval(StudioLight *sl,
                                                     float color[3],
                                                     const float normal[3])
{
#if STUDIOLIGHT_SH_BANDS == 2
  float(*sh)[3] = (float(*)[3])sl->spherical_harmonics_coefs;
  for (int i = 0; i < 3; i++) {
    color[i] = studiolight_spherical_harmonics_geomerics_eval(
        normal, sh[0][i], sh[1][i], sh[2][i], sh[3][i]);
  }
  return;
#else
  /* L0 */
  mul_v3_v3fl(color, sl->spherical_harmonics_coefs[0], 0.282095f);
#  if STUDIOLIGHT_SH_BANDS > 1 /* L1 */
  const float nx = normal[0];
  const float ny = normal[1];
  const float nz = normal[2];
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[1], -0.488603f * nz);
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[2], 0.488603f * ny);
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[3], -0.488603f * nx);
#  endif
#  if STUDIOLIGHT_SH_BANDS > 2 /* L2 */
  const float nx2 = SQUARE(nx);
  const float ny2 = SQUARE(ny);
  const float nz2 = SQUARE(nz);
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[4], 1.092548f * nx * nz);
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[5], -1.092548f * nz * ny);
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[6], 0.315392f * (3.0f * ny2 - 1.0f));
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[7], -1.092548 * nx * ny);
  madd_v3_v3fl(color, sl->spherical_harmonics_coefs[8], 0.546274 * (nx2 - nz2));
#  endif
  /* L3 coefs are 0 */
#  if STUDIOLIGHT_SH_BANDS > 4 /* L4 */
  const float nx4 = SQUARE(nx2);
  const float ny4 = SQUARE(ny2);
  const float nz4 = SQUARE(nz2);
  madd_v3_v3fl(
      color, sl->spherical_harmonics_coefs[9], 2.5033429417967046f * nx * nz * (nx2 - nz2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[10],
               -1.7701307697799304f * nz * ny * (3.0f * nx2 - nz2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[11],
               0.9461746957575601f * nz * nx * (-1.0f + 7.0f * ny2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[12],
               -0.6690465435572892f * nz * ny * (-3.0f + 7.0f * ny2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[13],
               (105.0f * ny4 - 90.0f * ny2 + 9.0f) / 28.359261614f);
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[14],
               -0.6690465435572892f * nx * ny * (-3.0f + 7.0f * ny2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[15],
               0.9461746957575601f * (nx2 - nz2) * (-1.0f + 7.0f * ny2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[16],
               -1.7701307697799304f * nx * ny * (nx2 - 3.0f * nz2));
  madd_v3_v3fl(color,
               sl->spherical_harmonics_coefs[17],
               0.6258357354491761f * (nx4 - 6.0f * nz2 * nx2 + nz4));
#  endif
#endif
}

/* This modify the radiance into irradiance. */
static void studiolight_spherical_harmonics_apply_band_factors(StudioLight *sl, float (*sh)[3])
{
  static const float sl_sh_band_factors[5] = {
      1.0f,
      2.0f / 3.0f,
      1.0f / 4.0f,
      0.0f,
      -1.0f / 24.0f,
  };

  int index = 0, dst_idx = 0;
  for (int band = 0; band < STUDIOLIGHT_SH_BANDS; band++) {
    const int last_band = square_i(band + 1) - square_i(band);
    for (int m = 0; m < last_band; m++) {
      /* Skip L3 */
      if (band != 3) {
        mul_v3_v3fl(sl->spherical_harmonics_coefs[dst_idx++], sh[index], sl_sh_band_factors[band]);
      }
      index++;
    }
  }
}

static void studiolight_calculate_diffuse_light(StudioLight *sl)
{
  /* init light to black */
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED);

    float sh_coefs[STUDIOLIGHT_SH_COEFS_LEN][3];
    studiolight_spherical_harmonics_calculate_coefficients(sl, sh_coefs);
    studiolight_spherical_harmonics_apply_windowing(sh_coefs, STUDIOLIGHT_SH_WINDOWING);
    studiolight_spherical_harmonics_apply_band_factors(sl, sh_coefs);

    if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
      FILE *fp = BLI_fopen(sl->path_sh_cache, "wb");
      if (fp) {
        fwrite(sl->spherical_harmonics_coefs, sizeof(sl->spherical_harmonics_coefs), 1, fp);
        fclose(fp);
      }
    }
  }
  sl->flag |= STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED;
}

BLI_INLINE void studiolight_evaluate_specular_radiance_buffer(ImBuf *radiance_buffer,
                                                              const float normal[3],
                                                              float color[3],
                                                              int xoffset,
                                                              int yoffset,
                                                              int zoffset,
                                                              float zsign)
{
  if (radiance_buffer == NULL) {
    return;
  }

  float accum[3] = {0.0f, 0.0f, 0.0f};
  float accum_weight = 0.00001f;
  ITER_PIXELS (float,
               radiance_buffer->rect_float,
               4,
               STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE,
               STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) {
    float direction[3];
    direction[zoffset] = zsign * 0.5f;
    direction[xoffset] = x - 0.5f;
    direction[yoffset] = y - 0.5f;
    normalize_v3(direction);
    float weight = dot_v3v3(direction, normal) > 0.95f ? 1.0f : 0.0f;
    // float solid_angle = texel_solid_angle(x, y, texel_size[0] * 0.5f);
    madd_v3_v3fl(accum, pixel, weight);
    accum_weight += weight;
  }
  ITER_PIXELS_END;

  madd_v3_v3fl(color, accum, 1.0f / accum_weight);
}

static float brdf_approx(float spec_color, float roughness, float NV)
{
  /* Very rough own approx. We don't need it to be correct, just fast.
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

/* Keep in sync with the glsl shader function get_world_lighting() */
static void studiolight_lights_eval(StudioLight *sl, float color[3], const float normal[3])
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
  for (int i = 0; i < 3; i++) {
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

  add_v3_v3v3(color, diff_light, spec_light);
}

static bool studiolight_load_irradiance_equirect_image(StudioLight *sl)
{
#ifdef STUDIOLIGHT_LOAD_CACHED_FILES
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    ImBuf *ibuf = NULL;
    ibuf = IMB_loadiffname(sl->path_irr_cache, 0, NULL);
    if (ibuf) {
      IMB_float_from_rect(ibuf);
      sl->equirect_irradiance_buffer = ibuf;
      sl->flag |= STUDIOLIGHT_EQUIRECT_IRRADIANCE_IMAGE_CALCULATED;
      return true;
    }
  }
#else
  UNUSED_VARS(sl);
#endif
  return false;
}

static bool studiolight_load_spherical_harmonics_coefficients(StudioLight *sl)
{
#ifdef STUDIOLIGHT_LOAD_CACHED_FILES
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    FILE *fp = BLI_fopen(sl->path_sh_cache, "rb");
    if (fp) {
      if (fread((void *)(sl->spherical_harmonics_coefs),
                sizeof(sl->spherical_harmonics_coefs),
                1,
                fp)) {
        sl->flag |= STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED;
        fclose(fp);
        return true;
      }
      fclose(fp);
    }
  }
#else
  UNUSED_VARS(sl);
#endif
  return false;
}

static void studiolight_calculate_irradiance_equirect_image(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED);

    float *colbuf = MEM_mallocN(STUDIOLIGHT_IRRADIANCE_EQUIRECT_WIDTH *
                                    STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT * sizeof(float[4]),
                                __func__);

    ITER_PIXELS (float,
                 colbuf,
                 4,
                 STUDIOLIGHT_IRRADIANCE_EQUIRECT_WIDTH,
                 STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT) {
      float dir[3];
      equirect_to_direction(dir, x, y);
      studiolight_spherical_harmonics_eval(sl, pixel, dir);
      pixel[3] = 1.0f;
    }
    ITER_PIXELS_END;

    sl->equirect_irradiance_buffer = IMB_allocFromBuffer(NULL,
                                                         colbuf,
                                                         STUDIOLIGHT_IRRADIANCE_EQUIRECT_WIDTH,
                                                         STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT,
                                                         4);
    MEM_freeN(colbuf);
  }
  sl->flag |= STUDIOLIGHT_EQUIRECT_IRRADIANCE_IMAGE_CALCULATED;
}

static StudioLight *studiolight_add_file(const char *path, int flag)
{
  char filename[FILE_MAXFILE];
  BLI_split_file_part(path, filename, FILE_MAXFILE);

  if ((((flag & STUDIOLIGHT_TYPE_STUDIO) != 0) && BLI_path_extension_check(filename, ".sl")) ||
      BLI_path_extension_check_array(filename, imb_ext_image)) {
    StudioLight *sl = studiolight_create(STUDIOLIGHT_EXTERNAL_FILE | flag);
    BLI_strncpy(sl->name, filename, FILE_MAXFILE);
    BLI_strncpy(sl->path, path, FILE_MAXFILE);

    if ((flag & STUDIOLIGHT_TYPE_STUDIO) != 0) {
      studiolight_load_solid_light(sl);
    }
    else {
      sl->path_irr_cache = BLI_string_joinN(path, ".irr");
      sl->path_sh_cache = BLI_string_joinN(path, ".sh2");
    }
    BLI_addtail(&studiolights, sl);
    return sl;
  }
  return NULL;
}

static void studiolight_add_files_from_datafolder(const int folder_id,
                                                  const char *subfolder,
                                                  int flag)
{
  struct direntry *dir;
  const char *folder = BKE_appdir_folder_id(folder_id, subfolder);
  if (folder) {
    uint totfile = BLI_filelist_dir_contents(folder, &dir);
    int i;
    for (i = 0; i < totfile; i++) {
      if ((dir[i].type & S_IFREG)) {
        studiolight_add_file(dir[i].path, flag);
      }
    }
    BLI_filelist_free(dir, totfile);
    dir = NULL;
  }
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
  const StudioLight *sl1 = a;
  const StudioLight *sl2 = b;

  const int flagorder1 = studiolight_flag_cmp_order(sl1);
  const int flagorder2 = studiolight_flag_cmp_order(sl2);

  if (flagorder1 < flagorder2) {
    return -1;
  }
  else if (flagorder1 > flagorder2) {
    return 1;
  }
  else {
    return BLI_strcasecmp(sl1->name, sl2->name);
  }
}

/* icons */

/* Takes normalized uvs as parameter (range from 0 to 1).
 * inner_edge and outer_edge are distances (from the center)
 * in uv space for the alpha mask falloff. */
static uint alpha_circle_mask(float u, float v, float inner_edge, float outer_edge)
{
  /* Coords from center. */
  float co[2] = {u - 0.5f, v - 0.5f};
  float dist = len_v2(co);
  float alpha = 1.0f + (inner_edge - dist) / (outer_edge - inner_edge);
  uint mask = (uint)floorf(255.0f * min_ff(max_ff(alpha, 0.0f), 1.0f));
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
      float normal[3], direction[3], color[4];
      float incoming[3] = {0.0f, 0.0f, -1.0f};
      sphere_normal_from_uv(normal, dx, dy);
      reflect_v3_v3v3(direction, incoming, normal);
      /* We want to see horizon not poles. */
      SWAP(float, direction[1], direction[2]);
      direction[1] = -direction[1];

      studiolight_calculate_radiance(sl->equirect_radiance_buffer, color, direction);

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
  BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);

  ImBuf *diffuse_buffer = sl->matcap_diffuse.ibuf;
  ImBuf *specular_buffer = sl->matcap_specular.ibuf;

  ITER_PIXELS (uint, icon_buffer, 1, STUDIOLIGHT_ICON_SIZE, STUDIOLIGHT_ICON_SIZE) {
    float dy = RESCALE_COORD(y);
    float dx = RESCALE_COORD(x);
    if (flipped) {
      dx = 1.0f - dx;
    }

    float color[4];
    float u = dx * diffuse_buffer->x - 1.0f;
    float v = dy * diffuse_buffer->y - 1.0f;
    nearest_interpolation_color(diffuse_buffer, NULL, color, u, v);

    if (specular_buffer) {
      float specular[4];
      nearest_interpolation_color(specular_buffer, NULL, specular, u, v);
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
      SWAP(float, normal[1], normal[2]);
      normal[1] = -normal[1];

      studiolight_lights_eval(sl, color, normal);

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

void BKE_studiolight_default(SolidLight lights[4], float light_ambient[4])
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

/* API */
void BKE_studiolight_init(void)
{
  /* Add default studio light */
  StudioLight *sl = studiolight_create(
      STUDIOLIGHT_INTERNAL | STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED |
      STUDIOLIGHT_TYPE_STUDIO | STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS);
  BLI_strncpy(sl->name, "Default", FILE_MAXFILE);

  BLI_addtail(&studiolights, sl);

  /* Go over the preset folder and add a studio-light for every image with its path. */
  /* For portable installs (where USER and SYSTEM paths are the same),
   * only go over LOCAL data-files once. */
  /* Also reserve icon space for it. */
  if (!BKE_appdir_app_is_portable_install()) {
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
  }
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

void BKE_studiolight_free(void)
{
  struct StudioLight *sl;
  while ((sl = BLI_pophead(&studiolights))) {
    studiolight_free(sl);
  }
}

struct StudioLight *BKE_studiolight_find_default(int flag)
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
    if ((sl->flag & flag)) {
      return sl;
    }
  }
  return NULL;
}

struct StudioLight *BKE_studiolight_find(const char *name, int flag)
{
  LISTBASE_FOREACH (StudioLight *, sl, &studiolights) {
    if (STREQLEN(sl->name, name, FILE_MAXFILE)) {
      if ((sl->flag & flag)) {
        return sl;
      }
      else {
        /* flags do not match, so use default */
        return BKE_studiolight_find_default(flag);
      }
    }
  }
  /* When not found, use the default studio light */
  return BKE_studiolight_find_default(flag);
}

struct StudioLight *BKE_studiolight_findindex(int index, int flag)
{
  LISTBASE_FOREACH (StudioLight *, sl, &studiolights) {
    if (sl->index == index) {
      return sl;
    }
  }
  /* When not found, use the default studio light */
  return BKE_studiolight_find_default(flag);
}

struct ListBase *BKE_studiolight_listbase(void)
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
}

/* Ensure state of Studiolights */
void BKE_studiolight_ensure_flag(StudioLight *sl, int flag)
{
  if ((sl->flag & flag) == flag) {
    return;
  }

  if ((flag & STUDIOLIGHT_EXTERNAL_IMAGE_LOADED)) {
    studiolight_load_equirect_image(sl);
  }
  if ((flag & STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED)) {
    studiolight_calculate_radiance_cubemap_buffers(sl);
  }
  if ((flag & STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED)) {
    if (!studiolight_load_spherical_harmonics_coefficients(sl)) {
      studiolight_calculate_diffuse_light(sl);
    }
  }
  if ((flag & STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE)) {
    studiolight_create_equirect_radiance_gputexture(sl);
  }
  if ((flag & STUDIOLIGHT_EQUIRECT_IRRADIANCE_GPUTEXTURE)) {
    studiolight_create_equirect_irradiance_gputexture(sl);
  }
  if ((flag & STUDIOLIGHT_EQUIRECT_IRRADIANCE_IMAGE_CALCULATED)) {
    if (!studiolight_load_irradiance_equirect_image(sl)) {
      studiolight_calculate_irradiance_equirect_image(sl);
    }
  }
  if ((flag & STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE)) {
    studiolight_create_matcap_diffuse_gputexture(sl);
  }
  if ((flag & STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE)) {
    studiolight_create_matcap_specular_gputexture(sl);
  }
}

/*
 * Python API Functions
 */
void BKE_studiolight_remove(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
    BLI_remlink(&studiolights, sl);
    studiolight_free(sl);
  }
}

StudioLight *BKE_studiolight_load(const char *path, int type)
{
  StudioLight *sl = studiolight_add_file(path, type | STUDIOLIGHT_USER_DEFINED);
  return sl;
}

StudioLight *BKE_studiolight_create(const char *path,
                                    const SolidLight light[4],
                                    const float light_ambient[3])
{
  StudioLight *sl = studiolight_create(STUDIOLIGHT_EXTERNAL_FILE | STUDIOLIGHT_USER_DEFINED |
                                       STUDIOLIGHT_TYPE_STUDIO |
                                       STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS);

  char filename[FILE_MAXFILE];
  BLI_split_file_part(path, filename, FILE_MAXFILE);
  STRNCPY(sl->path, path);
  STRNCPY(sl->name, filename);

  memcpy(sl->light, light, sizeof(*light) * 4);
  memcpy(sl->light_ambient, light_ambient, sizeof(*light_ambient) * 3);

  studiolight_write_solid_light(sl);

  BLI_addtail(&studiolights, sl);
  return sl;
}

/* Only useful for workbench while editing the userprefs. */
StudioLight *BKE_studiolight_studio_edit_get(void)
{
  static StudioLight sl = {0};
  sl.flag = STUDIOLIGHT_TYPE_STUDIO | STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS;

  memcpy(sl.light, U.light_param, sizeof(*sl.light) * 4);
  memcpy(sl.light_ambient, U.light_ambient, sizeof(*sl.light_ambient) * 3);

  return &sl;
}

void BKE_studiolight_refresh(void)
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
  BLI_assert(sl != NULL);
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
