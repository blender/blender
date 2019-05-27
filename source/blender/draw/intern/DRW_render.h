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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

/* This is the Render Functions used by Realtime engines to draw with OpenGL */

#ifndef __DRW_RENDER_H__
#define __DRW_RENDER_H__

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_material.h"
#include "BKE_scene.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "GPU_framebuffer.h"
#include "GPU_primitive.h"
#include "GPU_texture.h"
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_cache.h"
#include "draw_view.h"

#include "draw_manager_profiling.h"
#include "draw_debug.h"

#include "MEM_guardedalloc.h"

#include "RE_engine.h"

#include "DEG_depsgraph.h"

struct DRWTextStore;
struct DefaultFramebufferList;
struct DefaultTextureList;
struct GPUBatch;
struct GPUFrameBuffer;
struct GPUMaterial;
struct GPUShader;
struct GPUTexture;
struct GPUUniformBuffer;
struct LightEngineData;
struct Object;
struct ParticleSystem;
struct RenderEngineType;
struct ViewportEngineData;
struct ViewportEngineData_Info;
struct bContext;
struct rcti;

typedef struct DRWInterface DRWInterface;
typedef struct DRWPass DRWPass;
typedef struct DRWView DRWView;
typedef struct DRWShadingGroup DRWShadingGroup;
typedef struct DRWUniform DRWUniform;

/* Opaque type to avoid usage as a DRWCall but it is exactly the same thing. */
typedef struct DRWCallBuffer DRWCallBuffer;

/* TODO Put it somewhere else? */
typedef struct BoundSphere {
  float center[3], radius;
} BoundSphere;

/* declare members as empty (unused) */
typedef char DRWViewportEmptyList;

#define DRW_VIEWPORT_LIST_SIZE(list) \
  (sizeof(list) == sizeof(DRWViewportEmptyList) ? 0 : ((sizeof(list)) / sizeof(void *)))

/* Unused members must be either pass list or 'char *' when not usd. */
#define DRW_VIEWPORT_DATA_SIZE(ty) \
  { \
    DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->fbl)), DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->txl)), \
        DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->psl)), \
        DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->stl)), \
  }

/* Use of multisample framebuffers. */
#define MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl) \
  { \
    if (dfbl->multisample_fb != NULL && DRW_state_is_fbo()) { \
      DRW_stats_query_start("Multisample Blit"); \
      GPU_framebuffer_bind(dfbl->multisample_fb); \
      /* TODO clear only depth but need to do alpha to coverage for transparencies. */ \
      GPU_framebuffer_clear_color_depth(dfbl->multisample_fb, (const float[4]){0.0f}, 1.0f); \
      DRW_stats_query_end(); \
    } \
  } \
  ((void)0)

#define MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl) \
  { \
    if (dfbl->multisample_fb != NULL && DRW_state_is_fbo()) { \
      DRW_stats_query_start("Multisample Resolve"); \
      GPU_framebuffer_bind(dfbl->default_fb); \
      DRW_multisamples_resolve(dtxl->multisample_depth, dtxl->multisample_color, true); \
      DRW_stats_query_end(); \
    } \
  } \
  ((void)0)

#define MULTISAMPLE_SYNC_DISABLE_NO_DEPTH(dfbl, dtxl) \
  { \
    if (dfbl->multisample_fb != NULL && DRW_state_is_fbo()) { \
      DRW_stats_query_start("Multisample Resolve"); \
      GPU_framebuffer_bind(dfbl->default_fb); \
      DRW_multisamples_resolve(dtxl->multisample_depth, dtxl->multisample_color, false); \
      DRW_stats_query_end(); \
    } \
  } \
  ((void)0)

typedef struct DrawEngineDataSize {
  int fbl_len;
  int txl_len;
  int psl_len;
  int stl_len;
} DrawEngineDataSize;

typedef struct DrawEngineType {
  struct DrawEngineType *next, *prev;

  char idname[32];

  const DrawEngineDataSize *vedata_size;

  void (*engine_init)(void *vedata);
  void (*engine_free)(void);

  void (*cache_init)(void *vedata);
  void (*cache_populate)(void *vedata, struct Object *ob);
  void (*cache_finish)(void *vedata);

  void (*draw_background)(void *vedata);
  void (*draw_scene)(void *vedata);

  void (*view_update)(void *vedata);
  void (*id_update)(void *vedata, struct ID *id);

  void (*render_to_image)(void *vedata,
                          struct RenderEngine *engine,
                          struct RenderLayer *layer,
                          const struct rcti *rect);
} DrawEngineType;

#ifndef __DRW_ENGINE_H__
/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
  struct GPUFrameBuffer *default_fb;
  struct GPUFrameBuffer *color_only_fb;
  struct GPUFrameBuffer *depth_only_fb;
  struct GPUFrameBuffer *multisample_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
  struct GPUTexture *color;
  struct GPUTexture *depth;
  struct GPUTexture *multisample_color;
  struct GPUTexture *multisample_depth;
} DefaultTextureList;
#endif

/* Textures */
typedef enum {
  DRW_TEX_FILTER = (1 << 0),
  DRW_TEX_WRAP = (1 << 1),
  DRW_TEX_COMPARE = (1 << 2),
  DRW_TEX_MIPMAP = (1 << 3),
} DRWTextureFlag;

/* Textures from DRW_texture_pool_query_* have the options
 * DRW_TEX_FILTER for color float textures, and no options
 * for depth textures and integer textures. */
struct GPUTexture *DRW_texture_pool_query_2d(int w,
                                             int h,
                                             eGPUTextureFormat format,
                                             DrawEngineType *engine_type);

struct GPUTexture *DRW_texture_create_1d(int w,
                                         eGPUTextureFormat format,
                                         DRWTextureFlag flags,
                                         const float *fpixels);
struct GPUTexture *DRW_texture_create_2d(
    int w, int h, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_2d_array(
    int w, int h, int d, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_3d(
    int w, int h, int d, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_cube(int w,
                                           eGPUTextureFormat format,
                                           DRWTextureFlag flags,
                                           const float *fpixels);

void DRW_texture_ensure_fullscreen_2d(struct GPUTexture **tex,
                                      eGPUTextureFormat format,
                                      DRWTextureFlag flags);
void DRW_texture_ensure_2d(
    struct GPUTexture **tex, int w, int h, eGPUTextureFormat format, DRWTextureFlag flags);

void DRW_texture_generate_mipmaps(struct GPUTexture *tex);
void DRW_texture_free(struct GPUTexture *tex);
#define DRW_TEXTURE_FREE_SAFE(tex) \
  do { \
    if (tex != NULL) { \
      DRW_texture_free(tex); \
      tex = NULL; \
    } \
  } while (0)

/* UBOs */
struct GPUUniformBuffer *DRW_uniformbuffer_create(int size, const void *data);
void DRW_uniformbuffer_update(struct GPUUniformBuffer *ubo, const void *data);
void DRW_uniformbuffer_free(struct GPUUniformBuffer *ubo);
#define DRW_UBO_FREE_SAFE(ubo) \
  do { \
    if (ubo != NULL) { \
      DRW_uniformbuffer_free(ubo); \
      ubo = NULL; \
    } \
  } while (0)

void DRW_transform_to_display(struct GPUTexture *tex,
                              bool use_view_transform,
                              bool use_render_settings);
void DRW_transform_none(struct GPUTexture *tex);
void DRW_multisamples_resolve(struct GPUTexture *src_depth,
                              struct GPUTexture *src_color,
                              bool use_depth);

/* Shaders */
struct GPUShader *DRW_shader_create(const char *vert,
                                    const char *geom,
                                    const char *frag,
                                    const char *defines);
struct GPUShader *DRW_shader_create_with_lib(
    const char *vert, const char *geom, const char *frag, const char *lib, const char *defines);
struct GPUShader *DRW_shader_create_with_transform_feedback(const char *vert,
                                                            const char *geom,
                                                            const char *defines,
                                                            const eGPUShaderTFBType prim_type,
                                                            const char **varying_names,
                                                            const int varying_count);
struct GPUShader *DRW_shader_create_2d(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3d(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_fullscreen(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3d_depth_only(eGPUShaderConfig slot);
struct GPUMaterial *DRW_shader_find_from_world(struct World *wo,
                                               const void *engine_type,
                                               int options,
                                               bool deferred);
struct GPUMaterial *DRW_shader_find_from_material(struct Material *ma,
                                                  const void *engine_type,
                                                  int options,
                                                  bool deferred);
struct GPUMaterial *DRW_shader_create_from_world(struct Scene *scene,
                                                 struct World *wo,
                                                 const void *engine_type,
                                                 int options,
                                                 const char *vert,
                                                 const char *geom,
                                                 const char *frag_lib,
                                                 const char *defines,
                                                 bool deferred);
struct GPUMaterial *DRW_shader_create_from_material(struct Scene *scene,
                                                    struct Material *ma,
                                                    const void *engine_type,
                                                    int options,
                                                    const char *vert,
                                                    const char *geom,
                                                    const char *frag_lib,
                                                    const char *defines,
                                                    bool deferred);
void DRW_shader_free(struct GPUShader *shader);
#define DRW_SHADER_FREE_SAFE(shader) \
  do { \
    if (shader != NULL) { \
      DRW_shader_free(shader); \
      shader = NULL; \
    } \
  } while (0)

/* Batches */

typedef enum {
  /** Wrtie mask */
  DRW_STATE_WRITE_DEPTH = (1 << 0),
  DRW_STATE_WRITE_COLOR = (1 << 1),
  DRW_STATE_WRITE_STENCIL = (1 << 2),
  DRW_STATE_WRITE_STENCIL_SHADOW_PASS = (1 << 3),
  DRW_STATE_WRITE_STENCIL_SHADOW_FAIL = (1 << 4),

  /** Depth test */
  DRW_STATE_DEPTH_ALWAYS = (1 << 5),
  DRW_STATE_DEPTH_LESS = (1 << 6),
  DRW_STATE_DEPTH_LESS_EQUAL = (1 << 7),
  DRW_STATE_DEPTH_EQUAL = (1 << 8),
  DRW_STATE_DEPTH_GREATER = (1 << 9),
  DRW_STATE_DEPTH_GREATER_EQUAL = (1 << 10),
  /** Culling test */
  DRW_STATE_CULL_BACK = (1 << 11),
  DRW_STATE_CULL_FRONT = (1 << 12),
  /** Stencil test */
  DRW_STATE_STENCIL_EQUAL = (1 << 13),
  DRW_STATE_STENCIL_NEQUAL = (1 << 14),

  /** Blend state */
  DRW_STATE_BLEND_ADD = (1 << 15),
  /** Same as additive but let alpha accumulate without premult. */
  DRW_STATE_BLEND_ADD_FULL = (1 << 16),
  /** Standard alpha blending. */
  DRW_STATE_BLEND_ALPHA = (1 << 17),
  /** Use that if color is already premult by alpha. */
  DRW_STATE_BLEND_ALPHA_PREMUL = (1 << 18),
  DRW_STATE_BLEND_ALPHA_UNDER_PREMUL = (1 << 19),
  DRW_STATE_BLEND_OIT = (1 << 20),
  DRW_STATE_BLEND_MUL = (1 << 21),

  DRW_STATE_CLIP_PLANES = (1 << 22),
  DRW_STATE_WIRE_SMOOTH = (1 << 23),
  DRW_STATE_FIRST_VERTEX_CONVENTION = (1 << 24),
} DRWState;

#define DRW_STATE_DEFAULT \
  (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL)
#define DRW_STATE_RASTERIZER_ENABLED \
  (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_STENCIL | \
   DRW_STATE_WRITE_STENCIL_SHADOW_PASS | DRW_STATE_WRITE_STENCIL_SHADOW_FAIL)

typedef enum {
  DRW_ATTR_INT,
  DRW_ATTR_FLOAT,
} eDRWAttrType;

typedef struct DRWInstanceAttrFormat {
  char name[32];
  eDRWAttrType type;
  int components;
} DRWInstanceAttrFormat;

struct GPUVertFormat *DRW_shgroup_instance_format_array(const DRWInstanceAttrFormat attrs[],
                                                        int arraysize);
#define DRW_shgroup_instance_format(format, ...) \
  do { \
    if (format == NULL) { \
      DRWInstanceAttrFormat drw_format[] = __VA_ARGS__; \
      format = DRW_shgroup_instance_format_array( \
          drw_format, (sizeof(drw_format) / sizeof(DRWInstanceAttrFormat))); \
    } \
  } while (0)

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_create_sub(DRWShadingGroup *shgroup);
DRWShadingGroup *DRW_shgroup_material_create(struct GPUMaterial *material, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_transform_feedback_create(struct GPUShader *shader,
                                                       DRWPass *pass,
                                                       struct GPUVertBuf *tf_target);

/* return final visibility */
typedef bool(DRWCallVisibilityFn)(bool vis_in, void *user_data);

void DRW_shgroup_call(DRWShadingGroup *sh, struct GPUBatch *geom, float (*obmat)[4]);
void DRW_shgroup_call_range(
    DRWShadingGroup *sh, struct GPUBatch *geom, float (*obmat)[4], uint v_sta, uint v_ct);

void DRW_shgroup_call_procedural_points(DRWShadingGroup *sh, uint point_ct, float (*obmat)[4]);
void DRW_shgroup_call_procedural_lines(DRWShadingGroup *sh, uint line_ct, float (*obmat)[4]);
void DRW_shgroup_call_procedural_triangles(DRWShadingGroup *sh, uint tri_ct, float (*obmat)[4]);

void DRW_shgroup_call_object_ex(DRWShadingGroup *shgroup,
                                struct GPUBatch *geom,
                                struct Object *ob,
                                bool bypass_culling);
#define DRW_shgroup_call_object(shgroup, geom, ob) \
  DRW_shgroup_call_object_ex(shgroup, geom, ob, false)
#define DRW_shgroup_call_object_no_cull(shgroup, geom, ob) \
  DRW_shgroup_call_object_ex(shgroup, geom, ob, true)

/* TODO(fclem) remove this when we have DRWView */
/* user_data is used by DRWCallVisibilityFn defined in DRWView. */
void DRW_shgroup_call_object_with_callback(DRWShadingGroup *shgroup,
                                           struct GPUBatch *geom,
                                           struct Object *ob,
                                           void *user_data);

void DRW_shgroup_call_instances(DRWShadingGroup *shgroup,
                                struct GPUBatch *geom,
                                float (*obmat)[4],
                                uint count);
void DRW_shgroup_call_instances_with_attribs(DRWShadingGroup *shgroup,
                                             struct GPUBatch *geom,
                                             float (*obmat)[4],
                                             struct GPUBatch *inst_attributes);

void DRW_shgroup_call_sculpt(DRWShadingGroup *sh, Object *ob, bool wire, bool mask, bool vcol);
void DRW_shgroup_call_sculpt_with_materials(DRWShadingGroup **sh, Object *ob, bool vcol);

DRWCallBuffer *DRW_shgroup_call_buffer(DRWShadingGroup *shading_group,
                                       struct GPUVertFormat *format,
                                       GPUPrimType prim_type);
DRWCallBuffer *DRW_shgroup_call_buffer_instance(DRWShadingGroup *shading_group,
                                                struct GPUVertFormat *format,
                                                struct GPUBatch *geom);

void DRW_buffer_add_entry_array(DRWCallBuffer *buffer, const void *attr[], uint attr_len);

#define DRW_buffer_add_entry(buffer, ...) \
  do { \
    const void *array[] = {__VA_ARGS__}; \
    DRW_buffer_add_entry_array(buffer, array, (sizeof(array) / sizeof(*array))); \
  } while (0)

void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state);
void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state);
void DRW_shgroup_stencil_mask(DRWShadingGroup *shgroup, uint mask);

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup,
                                 const char *name,
                                 const struct GPUTexture *tex);
void DRW_shgroup_uniform_texture_persistent(DRWShadingGroup *shgroup,
                                            const char *name,
                                            const struct GPUTexture *tex);
void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup,
                               const char *name,
                               const struct GPUUniformBuffer *ubo);
void DRW_shgroup_uniform_block_persistent(DRWShadingGroup *shgroup,
                                          const char *name,
                                          const struct GPUUniformBuffer *ubo);
void DRW_shgroup_uniform_texture_ref(DRWShadingGroup *shgroup,
                                     const char *name,
                                     struct GPUTexture **tex);
void DRW_shgroup_uniform_float(DRWShadingGroup *shgroup,
                               const char *name,
                               const float *value,
                               int arraysize);
void DRW_shgroup_uniform_vec2(DRWShadingGroup *shgroup,
                              const char *name,
                              const float *value,
                              int arraysize);
void DRW_shgroup_uniform_vec3(DRWShadingGroup *shgroup,
                              const char *name,
                              const float *value,
                              int arraysize);
void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup,
                              const char *name,
                              const float *value,
                              int arraysize);
/* Boolean are expected to be 4bytes longs for opengl! */
void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup,
                              const char *name,
                              const int *value,
                              int arraysize);
void DRW_shgroup_uniform_int(DRWShadingGroup *shgroup,
                             const char *name,
                             const int *value,
                             int arraysize);
void DRW_shgroup_uniform_ivec2(DRWShadingGroup *shgroup,
                               const char *name,
                               const int *value,
                               int arraysize);
void DRW_shgroup_uniform_ivec3(DRWShadingGroup *shgroup,
                               const char *name,
                               const int *value,
                               int arraysize);
void DRW_shgroup_uniform_ivec4(DRWShadingGroup *shgroup,
                               const char *name,
                               const int *value,
                               int arraysize);
void DRW_shgroup_uniform_mat3(DRWShadingGroup *shgroup, const char *name, const float (*value)[3]);
void DRW_shgroup_uniform_mat4(DRWShadingGroup *shgroup, const char *name, const float (*value)[4]);
/* Store value instead of referencing it. */
void DRW_shgroup_uniform_int_copy(DRWShadingGroup *shgroup, const char *name, const int value);
void DRW_shgroup_uniform_bool_copy(DRWShadingGroup *shgroup, const char *name, const bool value);
void DRW_shgroup_uniform_float_copy(DRWShadingGroup *shgroup, const char *name, const float value);
void DRW_shgroup_uniform_vec2_copy(DRWShadingGroup *shgroup, const char *name, const float *value);

bool DRW_shgroup_is_empty(DRWShadingGroup *shgroup);

/* Passes */
DRWPass *DRW_pass_create(const char *name, DRWState state);
/* TODO Replace with passes inheritance. */
void DRW_pass_state_set(DRWPass *pass, DRWState state);
void DRW_pass_state_add(DRWPass *pass, DRWState state);
void DRW_pass_state_remove(DRWPass *pass, DRWState state);
void DRW_pass_foreach_shgroup(DRWPass *pass,
                              void (*callback)(void *userData, DRWShadingGroup *shgrp),
                              void *userData);
void DRW_pass_sort_shgroup_z(DRWPass *pass);

bool DRW_pass_is_empty(DRWPass *pass);

#define DRW_PASS_CREATE(pass, state) (pass = DRW_pass_create(#pass, state))

/* Views */
DRWView *DRW_view_create(const float viewmat[4][4],
                         const float winmat[4][4],
                         const float (*culling_viewmat)[4],
                         const float (*culling_winmat)[4],
                         DRWCallVisibilityFn *visibility_fn);
DRWView *DRW_view_create_sub(const DRWView *parent_view,
                             const float viewmat[4][4],
                             const float winmat[4][4]);

void DRW_view_update(DRWView *view,
                     const float viewmat[4][4],
                     const float winmat[4][4],
                     const float (*culling_viewmat)[4],
                     const float (*culling_winmat)[4]);
void DRW_view_update_sub(DRWView *view, const float viewmat[4][4], const float winmat[4][4]);

const DRWView *DRW_view_default_get(void);
void DRW_view_default_set(DRWView *view);

void DRW_view_set_active(DRWView *view);

void DRW_view_clip_planes_set(DRWView *view, float (*planes)[4], int plane_len);
void DRW_view_camtexco_set(DRWView *view, float texco[4]);

/* For all getters, if view is NULL, default view is assumed. */
void DRW_view_winmat_get(const DRWView *view, float mat[4][4], bool inverse);
void DRW_view_viewmat_get(const DRWView *view, float mat[4][4], bool inverse);
void DRW_view_persmat_get(const DRWView *view, float mat[4][4], bool inverse);

void DRW_view_frustum_corners_get(const DRWView *view, BoundBox *corners);
void DRW_view_frustum_planes_get(const DRWView *view, float planes[6][4]);

/* These are in view-space, so negative if in perspective.
 * Extract near and far clip distance from the projection matrix. */
float DRW_view_near_distance_get(const DRWView *view);
float DRW_view_far_distance_get(const DRWView *view);
bool DRW_view_is_persp_get(const DRWView *view);

/* Culling, return true if object is inside view frustum. */
bool DRW_culling_sphere_test(const DRWView *view, const BoundSphere *bsphere);
bool DRW_culling_box_test(const DRWView *view, const BoundBox *bbox);
bool DRW_culling_plane_test(const DRWView *view, const float plane[4]);

void DRW_culling_frustum_corners_get(const DRWView *view, BoundBox *corners);
void DRW_culling_frustum_planes_get(const DRWView *view, float planes[6][4]);

/* Viewport */

const float *DRW_viewport_size_get(void);
const float *DRW_viewport_invert_size_get(void);
const float *DRW_viewport_screenvecs_get(void);
const float *DRW_viewport_pixelsize_get(void);

struct DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void);
struct DefaultTextureList *DRW_viewport_texture_list_get(void);

void DRW_viewport_request_redraw(void);

void DRW_render_to_image(struct RenderEngine *engine, struct Depsgraph *depsgraph);
void DRW_render_object_iter(void *vedata,
                            struct RenderEngine *engine,
                            struct Depsgraph *depsgraph,
                            void (*callback)(void *vedata,
                                             struct Object *ob,
                                             struct RenderEngine *engine,
                                             struct Depsgraph *depsgraph));
void DRW_render_instance_buffer_finish(void);
void DRW_render_viewport_size_set(int size[2]);

void DRW_custom_pipeline(DrawEngineType *draw_engine_type,
                         struct Depsgraph *depsgraph,
                         void (*callback)(void *vedata, void *user_data),
                         void *user_data);

/* ViewLayers */
void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type);
void **DRW_view_layer_engine_data_ensure_ex(struct ViewLayer *view_layer,
                                            DrawEngineType *engine_type,
                                            void (*callback)(void *storage));
void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type,
                                         void (*callback)(void *storage));

/* DrawData */
DrawData *DRW_drawdata_get(ID *ib, DrawEngineType *engine_type);
DrawData *DRW_drawdata_ensure(ID *id,
                              DrawEngineType *engine_type,
                              size_t size,
                              DrawDataInitCb init_cb,
                              DrawDataFreeCb free_cb);
void **DRW_duplidata_get(void *vedata);

/* Settings */
bool DRW_object_is_renderable(const struct Object *ob);
int DRW_object_visibility_in_active_context(const struct Object *ob);
bool DRW_object_is_flat_normal(const struct Object *ob);
bool DRW_object_use_hide_faces(const struct Object *ob);
bool DRW_object_use_pbvh_drawing(const struct Object *ob);

bool DRW_object_is_visible_psys_in_active_context(const struct Object *object,
                                                  const struct ParticleSystem *psys);

struct Object *DRW_object_get_dupli_parent(const struct Object *ob);
struct DupliObject *DRW_object_get_dupli(const struct Object *ob);

/* Draw commands */
void DRW_draw_pass(DRWPass *pass);
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group);

void DRW_draw_callbacks_pre_scene(void);
void DRW_draw_callbacks_post_scene(void);

void DRW_state_reset_ex(DRWState state);
void DRW_state_reset(void);
void DRW_state_lock(DRWState state);

/* Selection */
void DRW_select_load_id(uint id);

/* Draw State */
void DRW_state_dfdy_factors_get(float dfdyfac[2]);
bool DRW_state_is_fbo(void);
bool DRW_state_is_select(void);
bool DRW_state_is_depth(void);
bool DRW_state_is_image_render(void);
bool DRW_state_is_scene_render(void);
bool DRW_state_is_opengl_render(void);
bool DRW_state_is_playback(void);
bool DRW_state_show_text(void);
bool DRW_state_draw_support(void);
bool DRW_state_draw_background(void);

/* Avoid too many lookups while drawing */
typedef struct DRWContextState {

  struct ARegion *ar;        /* 'CTX_wm_region(C)' */
  struct RegionView3D *rv3d; /* 'CTX_wm_region_view3d(C)' */
  struct View3D *v3d;        /* 'CTX_wm_view3d(C)' */

  struct Scene *scene;          /* 'CTX_data_scene(C)' */
  struct ViewLayer *view_layer; /* 'CTX_data_view_layer(C)' */

  /* Use 'object_edit' for edit-mode */
  struct Object *obact; /* 'OBACT' */

  struct RenderEngineType *engine_type;

  struct Depsgraph *depsgraph;

  eObjectMode object_mode;

  eGPUShaderConfig sh_cfg;

  /** Last resort (some functions take this as an arg so we can't easily avoid).
   * May be NULL when used for selection or depth buffer. */
  const struct bContext *evil_C;

  /* ---- */

  /* Cache: initialized by 'drw_context_state_init'. */
  struct Object *object_pose;
  struct Object *object_edit;

} DRWContextState;

const DRWContextState *DRW_context_state_get(void);

#endif /* __DRW_RENDER_H__ */
