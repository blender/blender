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
#include "BLI_listbase.h"
#include "BLI_linklist.h"
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

/* Statics */
static ListBase studiolights;
static int last_studiolight_id = 0;
#define STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE 96
#define STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT 32
#define STUDIOLIGHT_IRRADIANCE_EQUIRECT_WIDTH (STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT * 2)

/*
 * The method to calculate the irradiance buffers
 * The irradiance buffer is only shown in the background when in LookDev.
 *
 * STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE is very slow, but very accurate
 * STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS is faster but has artifacts
 * Cannot have both enabled at the same time!!!
 */
// #define STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
#define STUDIOLIGHT_IRRADIANCE_METHOD_SPHERICAL_HARMONICS

/* Temporarily disabled due to the creation of textures with -nan(ind)s */
#define STUDIOLIGHT_SH_WINDOWING 0.0f /* 0.0 is disabled */

/*
 * Disable this option so caches are not loaded from disk
 * Do not checkin with this commented out
 */
#define STUDIOLIGHT_LOAD_CACHED_FILES

static const char *STUDIOLIGHT_LIGHTS_FOLDER = "studiolights/studio/";
static const char *STUDIOLIGHT_WORLD_FOLDER = "studiolights/world/";
static const char *STUDIOLIGHT_MATCAP_FOLDER = "studiolights/matcap/";

static const char *STUDIOLIGHT_WORLD_DEFAULT = "forest.exr";
static const char *STUDIOLIGHT_MATCAP_DEFAULT = "basic_1.exr";

/* ITER MACRO */

/** Iter on all pixel giving texel center position and pixel pointer.
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

static void studiolight_load_equirect_image(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    ImBuf *ibuf = NULL;
    ibuf = IMB_loadiffname(sl->path, 0, NULL);
    if (ibuf == NULL) {
      float *colbuf = MEM_mallocN(sizeof(float[4]), __func__);
      copy_v4_fl4(colbuf, 1.0f, 0.0f, 1.0f, 1.0f);
      ibuf = IMB_allocFromBuffer(NULL, colbuf, 1, 1);
    }
    IMB_float_from_rect(ibuf);
    sl->equirect_radiance_buffer = ibuf;
  }
  sl->flag |= STUDIOLIGHT_EXTERNAL_IMAGE_LOADED;
}

static void studiolight_create_equirect_radiance_gputexture(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    char error[256];
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EXTERNAL_IMAGE_LOADED);
    ImBuf *ibuf = sl->equirect_radiance_buffer;

    if (sl->flag & STUDIOLIGHT_TYPE_MATCAP) {
      float *gpu_matcap_3components = MEM_callocN(sizeof(float[3]) * ibuf->x * ibuf->y, __func__);

      float(*offset4)[4] = (float(*)[4])ibuf->rect_float;
      float(*offset3)[3] = (float(*)[3])gpu_matcap_3components;
      for (int i = 0; i < ibuf->x * ibuf->y; i++, offset4++, offset3++) {
        copy_v3_v3(*offset3, *offset4);
      }

      sl->equirect_radiance_gputexture = GPU_texture_create_nD(ibuf->x,
                                                               ibuf->y,
                                                               0,
                                                               2,
                                                               gpu_matcap_3components,
                                                               GPU_R11F_G11F_B10F,
                                                               GPU_DATA_FLOAT,
                                                               0,
                                                               false,
                                                               error);

      MEM_SAFE_FREE(gpu_matcap_3components);
    }
    else {
      sl->equirect_radiance_gputexture = GPU_texture_create_2d(
          ibuf->x, ibuf->y, GPU_RGBA16F, ibuf->rect_float, error);
      GPUTexture *tex = sl->equirect_radiance_gputexture;
      GPU_texture_bind(tex, 0);
      GPU_texture_filter_mode(tex, true);
      GPU_texture_wrap_mode(tex, true);
      GPU_texture_unbind(tex);
    }
  }
  sl->flag |= STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE;
}

static void studiolight_create_equirect_irradiance_gputexture(StudioLight *sl)
{
  if (sl->flag & STUDIOLIGHT_EXTERNAL_FILE) {
    char error[256];
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECT_IRRADIANCE_IMAGE_CALCULATED);
    ImBuf *ibuf = sl->equirect_irradiance_buffer;
    sl->equirect_irradiance_gputexture = GPU_texture_create_2d(
        ibuf->x, ibuf->y, GPU_RGBA16F, ibuf->rect_float, error);
    GPUTexture *tex = sl->equirect_irradiance_gputexture;
    GPU_texture_bind(tex, 0);
    GPU_texture_filter_mode(tex, true);
    GPU_texture_wrap_mode(tex, true);
    GPU_texture_unbind(tex);
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
      float *colbuf = MEM_mallocN(SQUARE(STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE) * sizeof(float[4]),
                                  __func__);

      /* front */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 2, 1, 1, -1, 1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

      /* back */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 2, 1, 1, 1, -1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

      /* left */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 2, 1, 0, 1, -1, 1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

      /* right */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 2, 1, 0, -1, -1, -1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

      /* top */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 1, 2, -1, -1, 1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

      /* bottom */
      studiolight_calculate_radiance_buffer(ibuf, colbuf, 0, 1, 2, 1, -1, -1);
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS] = IMB_allocFromBuffer(
          NULL, colbuf, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE, STUDIOLIGHT_RADIANCE_CUBEMAP_SIZE);

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
  for (int i = 0; i < STUDIOLIGHT_SH_COEFS_LEN; ++i) {
    mul_v3_fl(sh[i], M_PI * 4.0f / weight_accum);
  }
}

/* Take monochrome SH as input */
static float studiolight_spherical_harmonics_lambda_get(float *sh, float max_laplacian)
{
  /* From Peter-Pike Sloan's Stupid SH Tricks http://www.ppsloan.org/publications/StupidSH36.pdf */
  float table_l[STUDIOLIGHT_SH_BANDS];
  float table_b[STUDIOLIGHT_SH_BANDS];

  float lambda = 0.0f;

  table_l[0] = 0.0f;
  table_b[0] = 0.0f;
  int index = 1;
  for (int level = 1; level < STUDIOLIGHT_SH_BANDS; level++) {
    table_l[level] = (float)(SQUARE(level) * SQUARE(level + 1));

    float b = 0.0f;
    for (int m = -1; m <= level; m++) {
      b += SQUARE(sh[index++]);
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
  for (int i = 0; i < no_iterations; ++i) {
    float f = 0.0f;
    float fd = 0.0f;

    for (int level = 1; level < STUDIOLIGHT_SH_BANDS; ++level) {
      f += table_l[level] * table_b[level] / SQUARE(1.0f + lambda * table_l[level]);
      fd += (2.0f * SQUARE(table_l[level]) * table_b[level]) /
            CUBE(1.0f + lambda * table_l[level]);
    }

    f = target_squared_laplacian - f;

    float delta = -f / fd;
    lambda += delta;

    if (ABS(delta) < 1e-6f) {
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
    s[0] = 1.0f / (1.0f + lambda_r * SQUARE(level) * SQUARE(level + 1.0f));
    s[1] = 1.0f / (1.0f + lambda_g * SQUARE(level) * SQUARE(level + 1.0f));
    s[2] = 1.0f / (1.0f + lambda_b * SQUARE(level) * SQUARE(level + 1.0f));

    for (int m = -1; m <= level; m++) {
      mul_v3_v3(sh[index++], s);
    }
  }
}

static float studiolight_spherical_harmonics_geomerics_eval(
    const float normal[3], float sh0, float sh1, float sh2, float sh3)
{
  /* Use Geomerics non-linear SH. */
  /* http://www.geomerics.com/wp-content/uploads/2015/08/CEDEC_Geomerics_ReconstructingDiffuseLighting1.pdf */
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
  for (int i = 0; i < 3; ++i) {
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
  static float sl_sh_band_factors[5] = {
      1.0f,
      2.0f / 3.0f,
      1.0f / 4.0f,
      0.0f,
      -1.0f / 24.0f,
  };

  int index = 0, dst_idx = 0;
  for (int band = 0; band < STUDIOLIGHT_SH_BANDS; band++) {
    for (int m = 0; m < SQUARE(band + 1) - SQUARE(band); m++) {
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

#ifdef STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
static void studiolight_irradiance_eval(StudioLight *sl, float color[3], const float normal[3])
{
  copy_v3_fl(color, 0.0f);

  /* XXX: This is madness, iterating over all cubemap pixels for each destination pixels
   * even if their weight is 0.0f.
   * It should use hemisphere, cosine sampling at least. */

  /* back */
  studiolight_evaluate_specular_radiance_buffer(
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_POS], normal, color, 0, 2, 1, 1);
  /* front */
  studiolight_evaluate_specular_radiance_buffer(
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Y_NEG], normal, color, 0, 2, 1, -1);

  /* left */
  studiolight_evaluate_specular_radiance_buffer(
      sl->radiance_cubemap_buffers[STUDIOLIGHT_X_POS], normal, color, 1, 2, 0, 1);
  /* right */
  studiolight_evaluate_specular_radiance_buffer(
      sl->radiance_cubemap_buffers[STUDIOLIGHT_X_NEG], normal, color, 1, 2, 0, -1);

  /* top */
  studiolight_evaluate_specular_radiance_buffer(
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_POS], normal, color, 0, 1, 2, 1);
  /* bottom */
  studiolight_evaluate_specular_radiance_buffer(
      sl->radiance_cubemap_buffers[STUDIOLIGHT_Z_NEG], normal, color, 0, 1, 2, -1);

  mul_v3_fl(color, 1.0 / M_PI);
}
#endif

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
                            float R[3],
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
  for (int i = 0; i < 3; ++i) {
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
#ifdef STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_RADIANCE_BUFFERS_CALCULATED);
#else
    BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED);
#endif

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
#ifdef STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
      studiolight_irradiance_eval(sl, pixel, dir);
#else
      studiolight_spherical_harmonics_eval(sl, pixel, dir);
#endif
      pixel[3] = 1.0f;
    }
    ITER_PIXELS_END;

    sl->equirect_irradiance_buffer = IMB_allocFromBuffer(NULL,
                                                         colbuf,
                                                         STUDIOLIGHT_IRRADIANCE_EQUIRECT_WIDTH,
                                                         STUDIOLIGHT_IRRADIANCE_EQUIRECT_HEIGHT);
    MEM_freeN(colbuf);

#ifdef STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
    /*
     * Only store cached files when using STUDIOLIGHT_IRRADIANCE_METHOD_RADIANCE
     */
    if (sl->flag & STUDIOLIGHT_USER_DEFINED) {
      IMB_saveiff(sl->equirect_irradiance_buffer, sl->path_irr_cache, IB_rectfloat);
    }
#endif
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
  normal[2] = sqrtf(1.0f - SQUARE(dist));
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

  ImBuf *ibuf = sl->equirect_radiance_buffer;

  ITER_PIXELS (uint, icon_buffer, 1, STUDIOLIGHT_ICON_SIZE, STUDIOLIGHT_ICON_SIZE) {
    float dy = RESCALE_COORD(y);
    float dx = RESCALE_COORD(x);
    if (flipped) {
      dx = 1.0f - dx;
    }

    float color[4];
    nearest_interpolation_color(ibuf, NULL, color, dx * ibuf->x - 1.0f, dy * ibuf->y - 1.0f);

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

/* API */
void BKE_studiolight_init(void)
{
  /* Add default studio light */
  StudioLight *sl = studiolight_create(STUDIOLIGHT_INTERNAL |
                                       STUDIOLIGHT_SPHERICAL_HARMONICS_COEFFICIENTS_CALCULATED |
                                       STUDIOLIGHT_TYPE_STUDIO);
  BLI_strncpy(sl->name, "Default", FILE_MAXFILE);

  copy_v3_fl3(sl->light_ambient, 0.025000, 0.025000, 0.025000);

  copy_v4_fl4(sl->light[0].vec, -0.580952, 0.228571, 0.781185, 0.0);
  copy_v4_fl4(sl->light[0].col, 0.900000, 0.900000, 0.900000, 1.000000);
  copy_v4_fl4(sl->light[0].spec, 0.318547, 0.318547, 0.318547, 1.000000);
  sl->light[0].flag = 1;
  sl->light[0].smooth = 0.1;

  copy_v4_fl4(sl->light[1].vec, 0.788218, 0.593482, -0.162765, 0.0);
  copy_v4_fl4(sl->light[1].col, 0.267115, 0.269928, 0.358840, 1.000000);
  copy_v4_fl4(sl->light[1].spec, 0.090838, 0.090838, 0.090838, 1.000000);
  sl->light[1].flag = 1;
  sl->light[1].smooth = 0.25;

  copy_v4_fl4(sl->light[2].vec, 0.696472, -0.696472, -0.172785, 0.0);
  copy_v4_fl4(sl->light[2].col, 0.293216, 0.304662, 0.401968, 1.000000);
  copy_v4_fl4(sl->light[2].spec, 0.069399, 0.020331, 0.020331, 1.000000);
  sl->light[2].flag = 1;
  sl->light[2].smooth = 0.5;

  copy_v4_fl4(sl->light[3].vec, 0.021053, -0.989474, 0.143173, 0.0);
  copy_v4_fl4(sl->light[3].col, 0.0, 0.0, 0.0, 1.0);
  copy_v4_fl4(sl->light[3].spec, 0.072234, 0.082253, 0.162642, 1.000000);
  sl->light[3].flag = 1;
  sl->light[3].smooth = 0.7;

  BLI_addtail(&studiolights, sl);

  /* go over the preset folder and add a studiolight for every image with its path */
  /* for portable installs (where USER and SYSTEM paths are the same),
   * only go over LOCAL datafiles once */
  /* Also reserve icon space for it. */
  if (!BKE_appdir_app_is_portable_install()) {
    studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,
                                          STUDIOLIGHT_LIGHTS_FOLDER,
                                          STUDIOLIGHT_TYPE_STUDIO | STUDIOLIGHT_USER_DEFINED);
    studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,
                                          STUDIOLIGHT_WORLD_FOLDER,
                                          STUDIOLIGHT_TYPE_WORLD | STUDIOLIGHT_USER_DEFINED);
    studiolight_add_files_from_datafolder(BLENDER_USER_DATAFILES,
                                          STUDIOLIGHT_MATCAP_FOLDER,
                                          STUDIOLIGHT_TYPE_MATCAP | STUDIOLIGHT_USER_DEFINED);
  }
  studiolight_add_files_from_datafolder(
      BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_LIGHTS_FOLDER, STUDIOLIGHT_TYPE_STUDIO);
  studiolight_add_files_from_datafolder(
      BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_WORLD_FOLDER, STUDIOLIGHT_TYPE_WORLD);
  studiolight_add_files_from_datafolder(
      BLENDER_SYSTEM_DATAFILES, STUDIOLIGHT_MATCAP_FOLDER, STUDIOLIGHT_TYPE_MATCAP);

  /* sort studio lights on filename. */
  BLI_listbase_sort(&studiolights, studiolight_cmp);
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
                                       STUDIOLIGHT_TYPE_STUDIO);

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
  sl.flag = STUDIOLIGHT_TYPE_STUDIO;

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
