/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

/* This is the Render Functions used by Realtime engines to draw with OpenGL */

#pragma once

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_material.h"
#include "BKE_pbvh.hh"
#include "BKE_scene.h"

#include "BLT_translation.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "GPU_framebuffer.h"
#include "GPU_material.hh"
#include "GPU_primitive.h"
#include "GPU_shader.h"
#include "GPU_storage_buffer.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "draw_cache.hh"
#include "draw_common.h"
#include "draw_view.h"

#include "draw_debug.h"
#include "draw_manager_profiling.hh"
#include "draw_state.h"
#include "draw_view_data.h"

#include "MEM_guardedalloc.h"

#include "RE_engine.h"

#include "DEG_depsgraph.hh"

/* Uncomment to track unused resource bindings. */
// #define DRW_UNUSED_RESOURCE_TRACKING

#ifdef DRW_UNUSED_RESOURCE_TRACKING
#  define DRW_DEBUG_FILE_LINE_ARGS , const char *file, int line
#else
#  define DRW_DEBUG_FILE_LINE_ARGS
#endif

struct GPUBatch;
struct GPUMaterial;
struct GPUShader;
struct GPUTexture;
struct GPUUniformBuf;
struct Object;
struct ParticleSystem;
struct RenderEngineType;
struct bContext;
struct rcti;
struct TaskGraph;
namespace blender::draw {
struct DRW_Attributes;
struct DRW_MeshCDMask;
}  // namespace blender::draw

typedef struct DRWCallBuffer DRWCallBuffer;
typedef struct DRWInterface DRWInterface;
typedef struct DRWPass DRWPass;
typedef struct DRWShaderLibrary DRWShaderLibrary;
typedef struct DRWShadingGroup DRWShadingGroup;
typedef struct DRWUniform DRWUniform;
typedef struct DRWView DRWView;

/* TODO: Put it somewhere else? */
struct BoundSphere {
  float center[3], radius;
};

/* declare members as empty (unused) */
typedef char DRWViewportEmptyList;

#define DRW_VIEWPORT_LIST_SIZE(list) \
  (sizeof(list) == sizeof(DRWViewportEmptyList) ? 0 : (sizeof(list) / sizeof(void *)))

/* Unused members must be either pass list or 'char *' when not used. */
#define DRW_VIEWPORT_DATA_SIZE(ty) \
  { \
    DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->fbl)), DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->txl)), \
        DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->psl)), \
        DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->stl)), \
  }

struct DrawEngineDataSize {
  int fbl_len;
  int txl_len;
  int psl_len;
  int stl_len;
};

struct DrawEngineType {
  DrawEngineType *next, *prev;

  char idname[32];

  const DrawEngineDataSize *vedata_size;

  void (*engine_init)(void *vedata);
  void (*engine_free)();

  void (*instance_free)(void *instance_data);

  void (*cache_init)(void *vedata);
  void (*cache_populate)(void *vedata, Object *ob);
  void (*cache_finish)(void *vedata);

  void (*draw_scene)(void *vedata);

  void (*view_update)(void *vedata);
  void (*id_update)(void *vedata, ID *id);

  void (*render_to_image)(void *vedata,
                          RenderEngine *engine,
                          RenderLayer *layer,
                          const rcti *rect);
  void (*store_metadata)(void *vedata, RenderResult *render_result);
};

/* Textures */
enum DRWTextureFlag {
  DRW_TEX_FILTER = (1 << 0),
  DRW_TEX_WRAP = (1 << 1),
  DRW_TEX_COMPARE = (1 << 2),
  DRW_TEX_MIPMAP = (1 << 3),
};

/**
 * Textures from `DRW_texture_pool_query_*` have the options
 * #DRW_TEX_FILTER for color float textures, and no options
 * for depth textures and integer textures.
 */
GPUTexture *DRW_texture_pool_query_2d(int w,
                                      int h,
                                      eGPUTextureFormat format,
                                      DrawEngineType *engine_type);
GPUTexture *DRW_texture_pool_query_fullscreen(eGPUTextureFormat format,
                                              DrawEngineType *engine_type);

GPUTexture *DRW_texture_create_1d(int w,
                                  eGPUTextureFormat format,
                                  DRWTextureFlag flags,
                                  const float *fpixels);
GPUTexture *DRW_texture_create_2d(
    int w, int h, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);
GPUTexture *DRW_texture_create_2d_array(
    int w, int h, int d, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);
GPUTexture *DRW_texture_create_3d(
    int w, int h, int d, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);
GPUTexture *DRW_texture_create_cube(int w,
                                    eGPUTextureFormat format,
                                    DRWTextureFlag flags,
                                    const float *fpixels);
GPUTexture *DRW_texture_create_cube_array(
    int w, int d, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels);

void DRW_texture_ensure_fullscreen_2d(GPUTexture **tex,
                                      eGPUTextureFormat format,
                                      DRWTextureFlag flags);
void DRW_texture_ensure_2d(
    GPUTexture **tex, int w, int h, eGPUTextureFormat format, DRWTextureFlag flags);

/* Explicit parameter variants. */
GPUTexture *DRW_texture_pool_query_2d_ex(
    int w, int h, eGPUTextureFormat format, eGPUTextureUsage usage, DrawEngineType *engine_type);
GPUTexture *DRW_texture_pool_query_fullscreen_ex(eGPUTextureFormat format,
                                                 eGPUTextureUsage usage,
                                                 DrawEngineType *engine_type);

GPUTexture *DRW_texture_create_1d_ex(int w,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage_flags,
                                     DRWTextureFlag flags,
                                     const float *fpixels);
GPUTexture *DRW_texture_create_2d_ex(int w,
                                     int h,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage_flags,
                                     DRWTextureFlag flags,
                                     const float *fpixels);
GPUTexture *DRW_texture_create_2d_array_ex(int w,
                                           int h,
                                           int d,
                                           eGPUTextureFormat format,
                                           eGPUTextureUsage usage_flags,
                                           DRWTextureFlag flags,
                                           const float *fpixels);
GPUTexture *DRW_texture_create_3d_ex(int w,
                                     int h,
                                     int d,
                                     eGPUTextureFormat format,
                                     eGPUTextureUsage usage_flags,
                                     DRWTextureFlag flags,
                                     const float *fpixels);
GPUTexture *DRW_texture_create_cube_ex(int w,
                                       eGPUTextureFormat format,
                                       eGPUTextureUsage usage_flags,
                                       DRWTextureFlag flags,
                                       const float *fpixels);
GPUTexture *DRW_texture_create_cube_array_ex(int w,
                                             int d,
                                             eGPUTextureFormat format,
                                             eGPUTextureUsage usage_flags,
                                             DRWTextureFlag flags,
                                             const float *fpixels);

void DRW_texture_ensure_fullscreen_2d_ex(GPUTexture **tex,
                                         eGPUTextureFormat format,
                                         eGPUTextureUsage usage,
                                         DRWTextureFlag flags);
void DRW_texture_ensure_2d_ex(GPUTexture **tex,
                              int w,
                              int h,
                              eGPUTextureFormat format,
                              eGPUTextureUsage usage,
                              DRWTextureFlag flags);

void DRW_texture_generate_mipmaps(GPUTexture *tex);
void DRW_texture_free(GPUTexture *tex);
#define DRW_TEXTURE_FREE_SAFE(tex) \
  do { \
    if (tex != NULL) { \
      DRW_texture_free(tex); \
      tex = NULL; \
    } \
  } while (0)

#define DRW_UBO_FREE_SAFE(ubo) \
  do { \
    if (ubo != NULL) { \
      GPU_uniformbuf_free(ubo); \
      ubo = NULL; \
    } \
  } while (0)

/* Shaders */
GPUShader *DRW_shader_create_from_info_name(const char *info_name);
GPUShader *DRW_shader_create_ex(
    const char *vert, const char *geom, const char *frag, const char *defines, const char *name);
GPUShader *DRW_shader_create_with_lib_ex(const char *vert,
                                         const char *geom,
                                         const char *frag,
                                         const char *lib,
                                         const char *defines,
                                         const char *name);
GPUShader *DRW_shader_create_with_shaderlib_ex(const char *vert,
                                               const char *geom,
                                               const char *frag,
                                               const DRWShaderLibrary *lib,
                                               const char *defines,
                                               const char *name);
GPUShader *DRW_shader_create_with_transform_feedback(const char *vert,
                                                     const char *geom,
                                                     const char *defines,
                                                     eGPUShaderTFBType prim_type,
                                                     const char **varying_names,
                                                     int varying_count);
GPUShader *DRW_shader_create_fullscreen_ex(const char *frag,
                                           const char *defines,
                                           const char *name);
GPUShader *DRW_shader_create_fullscreen_with_shaderlib_ex(const char *frag,
                                                          const DRWShaderLibrary *lib,
                                                          const char *defines,
                                                          const char *name);
#define DRW_shader_create(vert, geom, frag, defines) \
  DRW_shader_create_ex(vert, geom, frag, defines, __func__)
#define DRW_shader_create_with_lib(vert, geom, frag, lib, defines) \
  DRW_shader_create_with_lib_ex(vert, geom, frag, lib, defines, __func__)
#define DRW_shader_create_with_shaderlib(vert, geom, frag, lib, defines) \
  DRW_shader_create_with_shaderlib_ex(vert, geom, frag, lib, defines, __func__)
#define DRW_shader_create_fullscreen(frag, defines) \
  DRW_shader_create_fullscreen_ex(frag, defines, __func__)
#define DRW_shader_create_fullscreen_with_shaderlib(frag, lib, defines) \
  DRW_shader_create_fullscreen_with_shaderlib_ex(frag, lib, defines, __func__)

GPUMaterial *DRW_shader_from_world(World *wo,
                                   bNodeTree *ntree,
                                   eGPUMaterialEngine engine,
                                   const uint64_t shader_id,
                                   const bool is_volume_shader,
                                   bool deferred,
                                   GPUCodegenCallbackFn callback,
                                   void *thunk);
GPUMaterial *DRW_shader_from_material(Material *ma,
                                      bNodeTree *ntree,
                                      eGPUMaterialEngine engine,
                                      const uint64_t shader_id,
                                      const bool is_volume_shader,
                                      bool deferred,
                                      GPUCodegenCallbackFn callback,
                                      void *thunk);
void DRW_shader_queue_optimize_material(GPUMaterial *mat);
void DRW_shader_free(GPUShader *shader);
#define DRW_SHADER_FREE_SAFE(shader) \
  do { \
    if (shader != NULL) { \
      DRW_shader_free(shader); \
      shader = NULL; \
    } \
  } while (0)

DRWShaderLibrary *DRW_shader_library_create();

/**
 * \warning Each library must be added after all its dependencies.
 */
void DRW_shader_library_add_file(DRWShaderLibrary *lib,
                                 const char *lib_code,
                                 const char *lib_name);
#define DRW_SHADER_LIB_ADD(lib, lib_name) \
  DRW_shader_library_add_file(lib, datatoc_##lib_name##_glsl, STRINGIFY(lib_name) ".glsl")

#define DRW_SHADER_LIB_ADD_SHARED(lib, lib_name) \
  DRW_shader_library_add_file(lib, datatoc_##lib_name##_h, STRINGIFY(lib_name) ".h")

/**
 * \return an allocN'ed string containing the shader code with its dependencies prepended.
 * Caller must free the string with #MEM_freeN after use.
 */
char *DRW_shader_library_create_shader_string(const DRWShaderLibrary *lib,
                                              const char *shader_code);

void DRW_shader_library_free(DRWShaderLibrary *lib);
#define DRW_SHADER_LIB_FREE_SAFE(lib) \
  do { \
    if (lib != NULL) { \
      DRW_shader_library_free(lib); \
      lib = NULL; \
    } \
  } while (0)

/* Batches */

enum eDRWAttrType {
  DRW_ATTR_INT,
  DRW_ATTR_FLOAT,
};

struct DRWInstanceAttrFormat {
  char name[32];
  eDRWAttrType type;
  int components;
};

GPUVertFormat *DRW_shgroup_instance_format_array(const DRWInstanceAttrFormat attrs[],
                                                 int arraysize);
#define DRW_shgroup_instance_format(format, ...) \
  do { \
    if (format == NULL) { \
      DRWInstanceAttrFormat drw_format[] = __VA_ARGS__; \
      format = DRW_shgroup_instance_format_array( \
          drw_format, (sizeof(drw_format) / sizeof(DRWInstanceAttrFormat))); \
    } \
  } while (0)

DRWShadingGroup *DRW_shgroup_create(GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_create_sub(DRWShadingGroup *shgroup);
DRWShadingGroup *DRW_shgroup_material_create(GPUMaterial *material, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_transform_feedback_create(GPUShader *shader,
                                                       DRWPass *pass,
                                                       GPUVertBuf *tf_target);

void DRW_shgroup_add_material_resources(DRWShadingGroup *grp, GPUMaterial *material);

/**
 * Return final visibility.
 */
typedef bool(DRWCallVisibilityFn)(bool vis_in, void *user_data);

void DRW_shgroup_call_ex(DRWShadingGroup *shgroup,
                         const Object *ob,
                         const float (*obmat)[4],
                         GPUBatch *geom,
                         bool bypass_culling,
                         void *user_data);

/**
 * If ob is NULL, unit model-matrix is assumed and culling is bypassed.
 */
#define DRW_shgroup_call(shgroup, geom, ob) \
  DRW_shgroup_call_ex(shgroup, ob, NULL, geom, false, NULL)

/**
 * Same as #DRW_shgroup_call but override the `obmat`. Not culled.
 */
#define DRW_shgroup_call_obmat(shgroup, geom, obmat) \
  DRW_shgroup_call_ex(shgroup, NULL, obmat, geom, false, NULL)

/* TODO(fclem): remove this when we have #DRWView */
/* user_data is used by #DRWCallVisibilityFn defined in #DRWView. */
#define DRW_shgroup_call_with_callback(shgroup, geom, ob, user_data) \
  DRW_shgroup_call_ex(shgroup, ob, NULL, geom, false, user_data)

/**
 * Same as #DRW_shgroup_call but bypass culling even if ob is not NULL.
 */
#define DRW_shgroup_call_no_cull(shgroup, geom, ob) \
  DRW_shgroup_call_ex(shgroup, ob, NULL, geom, true, NULL)

void DRW_shgroup_call_range(
    DRWShadingGroup *shgroup, const Object *ob, GPUBatch *geom, uint v_sta, uint v_num);
/**
 * A count of 0 instance will use the default number of instance in the batch.
 */
void DRW_shgroup_call_instance_range(
    DRWShadingGroup *shgroup, const Object *ob, GPUBatch *geom, uint i_sta, uint i_num);

void DRW_shgroup_call_compute(DRWShadingGroup *shgroup,
                              int groups_x_len,
                              int groups_y_len,
                              int groups_z_len);
/**
 * \warning this keeps the ref to groups_ref until it actually dispatch.
 */
void DRW_shgroup_call_compute_ref(DRWShadingGroup *shgroup, int groups_ref[3]);
/**
 * \note No need for a barrier. \a indirect_buf is internally synchronized.
 */
void DRW_shgroup_call_compute_indirect(DRWShadingGroup *shgroup, GPUStorageBuf *indirect_buf);
void DRW_shgroup_call_procedural_points(DRWShadingGroup *sh, const Object *ob, uint point_count);
void DRW_shgroup_call_procedural_lines(DRWShadingGroup *sh, const Object *ob, uint line_count);
void DRW_shgroup_call_procedural_triangles(DRWShadingGroup *sh, const Object *ob, uint tri_count);
void DRW_shgroup_call_procedural_indirect(DRWShadingGroup *shgroup,
                                          GPUPrimType primitive_type,
                                          Object *ob,
                                          GPUStorageBuf *indirect_buf);
/**
 * \warning Only use with Shaders that have `IN_PLACE_INSTANCES` defined.
 * TODO: Should be removed.
 */
void DRW_shgroup_call_instances(DRWShadingGroup *shgroup,
                                const Object *ob,
                                GPUBatch *geom,
                                uint count);
/**
 * \warning Only use with Shaders that have INSTANCED_ATTR defined.
 */
void DRW_shgroup_call_instances_with_attrs(DRWShadingGroup *shgroup,
                                           const Object *ob,
                                           GPUBatch *geom,
                                           GPUBatch *inst_attributes);

void DRW_shgroup_call_sculpt(DRWShadingGroup *shgroup,
                             Object *ob,
                             bool use_wire,
                             bool use_mask,
                             bool use_fset,
                             bool use_color,
                             bool use_uv);

void DRW_shgroup_call_sculpt_with_materials(DRWShadingGroup **shgroups,
                                            GPUMaterial **gpumats,
                                            int num_shgroups,
                                            const Object *ob);

DRWCallBuffer *DRW_shgroup_call_buffer(DRWShadingGroup *shgroup,
                                       GPUVertFormat *format,
                                       GPUPrimType prim_type);
DRWCallBuffer *DRW_shgroup_call_buffer_instance(DRWShadingGroup *shgroup,
                                                GPUVertFormat *format,
                                                GPUBatch *geom);

void DRW_buffer_add_entry_struct(DRWCallBuffer *callbuf, const void *data);
void DRW_buffer_add_entry_array(DRWCallBuffer *callbuf, const void *attr[], uint attr_len);

#define DRW_buffer_add_entry(buffer, ...) \
  do { \
    const void *array[] = {__VA_ARGS__}; \
    DRW_buffer_add_entry_array(buffer, array, (sizeof(array) / sizeof(*array))); \
  } while (0)

/**
 * Can only be called during iteration phase.
 */
uint32_t DRW_object_resource_id_get(Object *ob);

/**
 * State is added to #Pass.state while drawing.
 * Use to temporarily enable draw options.
 */
void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state);
void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state);

/**
 * Reminders:
 * - (compare_mask & reference) is what is tested against (compare_mask & stencil_value)
 *   stencil_value being the value stored in the stencil buffer.
 * - (write-mask & reference) is what gets written if the test condition is fulfilled.
 */
void DRW_shgroup_stencil_set(DRWShadingGroup *shgroup,
                             uint write_mask,
                             uint reference,
                             uint compare_mask);
/**
 * TODO: remove this function. Obsolete version. mask is actually reference value.
 */
void DRW_shgroup_stencil_mask(DRWShadingGroup *shgroup, uint mask);

/**
 * Issue a barrier command.
 */
void DRW_shgroup_barrier(DRWShadingGroup *shgroup, eGPUBarrier type);

/**
 * Issue a clear command.
 */
void DRW_shgroup_clear_framebuffer(DRWShadingGroup *shgroup,
                                   eGPUFrameBufferBits channels,
                                   uchar r,
                                   uchar g,
                                   uchar b,
                                   uchar a,
                                   float depth,
                                   uchar stencil);

void DRW_shgroup_uniform_texture_ex(DRWShadingGroup *shgroup,
                                    const char *name,
                                    const GPUTexture *tex,
                                    GPUSamplerState sampler_state);
void DRW_shgroup_uniform_texture_ref_ex(DRWShadingGroup *shgroup,
                                        const char *name,
                                        GPUTexture **tex,
                                        GPUSamplerState sampler_state);
void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup,
                                 const char *name,
                                 const GPUTexture *tex);
void DRW_shgroup_uniform_texture_ref(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex);
void DRW_shgroup_uniform_block_ex(DRWShadingGroup *shgroup,
                                  const char *name,
                                  const GPUUniformBuf *ubo DRW_DEBUG_FILE_LINE_ARGS);
void DRW_shgroup_uniform_block_ref_ex(DRWShadingGroup *shgroup,
                                      const char *name,
                                      GPUUniformBuf **ubo DRW_DEBUG_FILE_LINE_ARGS);
void DRW_shgroup_storage_block_ex(DRWShadingGroup *shgroup,
                                  const char *name,
                                  const GPUStorageBuf *ssbo DRW_DEBUG_FILE_LINE_ARGS);
void DRW_shgroup_storage_block_ref_ex(DRWShadingGroup *shgroup,
                                      const char *name,
                                      GPUStorageBuf **ssbo DRW_DEBUG_FILE_LINE_ARGS);
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
void DRW_shgroup_uniform_image(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex);
void DRW_shgroup_uniform_image_ref(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex);

/* Store value instead of referencing it. */

void DRW_shgroup_uniform_int_copy(DRWShadingGroup *shgroup, const char *name, int value);
void DRW_shgroup_uniform_ivec2_copy(DRWShadingGroup *shgroup, const char *name, const int *value);
void DRW_shgroup_uniform_ivec3_copy(DRWShadingGroup *shgroup, const char *name, const int *value);
void DRW_shgroup_uniform_ivec4_copy(DRWShadingGroup *shgroup, const char *name, const int *value);
void DRW_shgroup_uniform_bool_copy(DRWShadingGroup *shgroup, const char *name, bool value);
void DRW_shgroup_uniform_float_copy(DRWShadingGroup *shgroup, const char *name, float value);
void DRW_shgroup_uniform_vec2_copy(DRWShadingGroup *shgroup, const char *name, const float *value);
void DRW_shgroup_uniform_vec3_copy(DRWShadingGroup *shgroup, const char *name, const float *value);
void DRW_shgroup_uniform_vec4_copy(DRWShadingGroup *shgroup, const char *name, const float *value);
void DRW_shgroup_uniform_mat4_copy(DRWShadingGroup *shgroup,
                                   const char *name,
                                   const float (*value)[4]);
void DRW_shgroup_vertex_buffer_ex(DRWShadingGroup *shgroup,
                                  const char *name,
                                  GPUVertBuf *vertex_buffer DRW_DEBUG_FILE_LINE_ARGS);
void DRW_shgroup_vertex_buffer_ref_ex(DRWShadingGroup *shgroup,
                                      const char *name,
                                      GPUVertBuf **vertex_buffer DRW_DEBUG_FILE_LINE_ARGS);
void DRW_shgroup_buffer_texture(DRWShadingGroup *shgroup,
                                const char *name,
                                GPUVertBuf *vertex_buffer);
void DRW_shgroup_buffer_texture_ref(DRWShadingGroup *shgroup,
                                    const char *name,
                                    GPUVertBuf **vertex_buffer);

#ifdef DRW_UNUSED_RESOURCE_TRACKING
#  define DRW_shgroup_vertex_buffer(shgroup, name, vert) \
    DRW_shgroup_vertex_buffer_ex(shgroup, name, vert, __FILE__, __LINE__)
#  define DRW_shgroup_vertex_buffer_ref(shgroup, name, vert) \
    DRW_shgroup_vertex_buffer_ref_ex(shgroup, name, vert, __FILE__, __LINE__)
#  define DRW_shgroup_uniform_block(shgroup, name, ubo) \
    DRW_shgroup_uniform_block_ex(shgroup, name, ubo, __FILE__, __LINE__)
#  define DRW_shgroup_uniform_block_ref(shgroup, name, ubo) \
    DRW_shgroup_uniform_block_ref_ex(shgroup, name, ubo, __FILE__, __LINE__)
#  define DRW_shgroup_storage_block(shgroup, name, ssbo) \
    DRW_shgroup_storage_block_ex(shgroup, name, ssbo, __FILE__, __LINE__)
#  define DRW_shgroup_storage_block_ref(shgroup, name, ssbo) \
    DRW_shgroup_storage_block_ref_ex(shgroup, name, ssbo, __FILE__, __LINE__)
#else
#  define DRW_shgroup_vertex_buffer(shgroup, name, vert) \
    DRW_shgroup_vertex_buffer_ex(shgroup, name, vert)
#  define DRW_shgroup_vertex_buffer_ref(shgroup, name, vert) \
    DRW_shgroup_vertex_buffer_ref_ex(shgroup, name, vert)
#  define DRW_shgroup_uniform_block(shgroup, name, ubo) \
    DRW_shgroup_uniform_block_ex(shgroup, name, ubo)
#  define DRW_shgroup_uniform_block_ref(shgroup, name, ubo) \
    DRW_shgroup_uniform_block_ref_ex(shgroup, name, ubo)
#  define DRW_shgroup_storage_block(shgroup, name, ssbo) \
    DRW_shgroup_storage_block_ex(shgroup, name, ssbo)
#  define DRW_shgroup_storage_block_ref(shgroup, name, ssbo) \
    DRW_shgroup_storage_block_ref_ex(shgroup, name, ssbo)
#endif

bool DRW_shgroup_is_empty(DRWShadingGroup *shgroup);

/* Passes. */

DRWPass *DRW_pass_create(const char *name, DRWState state);
/**
 * Create an instance of the original pass that will execute the same drawcalls but with its own
 * #DRWState.
 */
DRWPass *DRW_pass_create_instance(const char *name, DRWPass *original, DRWState state);
/**
 * Link two passes so that they are both rendered if the first one is being drawn.
 */
void DRW_pass_link(DRWPass *first, DRWPass *second);
void DRW_pass_foreach_shgroup(DRWPass *pass,
                              void (*callback)(void *user_data, DRWShadingGroup *shgroup),
                              void *user_data);
/**
 * Sort Shading groups by decreasing Z of their first draw call.
 * This is useful for order dependent effect such as alpha-blending.
 */
void DRW_pass_sort_shgroup_z(DRWPass *pass);
/**
 * Reverse Shading group submission order.
 */
void DRW_pass_sort_shgroup_reverse(DRWPass *pass);

bool DRW_pass_is_empty(DRWPass *pass);

#define DRW_PASS_CREATE(pass, state) (pass = DRW_pass_create(#pass, state))
#define DRW_PASS_INSTANCE_CREATE(pass, original, state) \
  (pass = DRW_pass_create_instance(#pass, (original), state))

/* Views. */

/**
 * Create a view with culling.
 */
DRWView *DRW_view_create(const float viewmat[4][4],
                         const float winmat[4][4],
                         const float (*culling_viewmat)[4],
                         const float (*culling_winmat)[4],
                         DRWCallVisibilityFn *visibility_fn);
/**
 * Create a view with culling done by another view.
 */
DRWView *DRW_view_create_sub(const DRWView *parent_view,
                             const float viewmat[4][4],
                             const float winmat[4][4]);

/**
 * Update matrices of a view created with #DRW_view_create.
 */
void DRW_view_update(DRWView *view,
                     const float viewmat[4][4],
                     const float winmat[4][4],
                     const float (*culling_viewmat)[4],
                     const float (*culling_winmat)[4]);
/**
 * Update matrices of a view created with #DRW_view_create_sub.
 */
void DRW_view_update_sub(DRWView *view, const float viewmat[4][4], const float winmat[4][4]);

/**
 * \return default view if it is a viewport render.
 */
const DRWView *DRW_view_default_get();
/**
 * MUST only be called once per render and only in render mode. Sets default view.
 */
void DRW_view_default_set(const DRWView *view);
/**
 * \warning Only use in render AND only if you are going to set view_default again.
 */
void DRW_view_reset();
/**
 * Set active view for rendering.
 */
void DRW_view_set_active(const DRWView *view);
const DRWView *DRW_view_get_active();

/**
 * This only works if DRWPasses have been tagged with DRW_STATE_CLIP_PLANES,
 * and if the shaders have support for it (see usage of gl_ClipDistance).
 * \note planes must be in world space.
 */
void DRW_view_clip_planes_set(DRWView *view, float (*planes)[4], int plane_len);

/* For all getters, if view is NULL, default view is assumed. */

void DRW_view_winmat_get(const DRWView *view, float mat[4][4], bool inverse);
void DRW_view_viewmat_get(const DRWView *view, float mat[4][4], bool inverse);
void DRW_view_persmat_get(const DRWView *view, float mat[4][4], bool inverse);

/**
 * \return world space frustum corners.
 */
void DRW_view_frustum_corners_get(const DRWView *view, BoundBox *corners);
/**
 * \return world space frustum sides as planes.
 * See #draw_frustum_culling_planes_calc() for the plane order.
 */
void DRW_view_frustum_planes_get(const DRWView *view, float planes[6][4]);

/**
 * These are in view-space, so negative if in perspective.
 * Extract near and far clip distance from the projection matrix.
 */
float DRW_view_near_distance_get(const DRWView *view);
float DRW_view_far_distance_get(const DRWView *view);
bool DRW_view_is_persp_get(const DRWView *view);

/* Culling, return true if object is inside view frustum. */

/**
 * \return True if the given BoundSphere intersect the current view frustum.
 * bsphere must be in world space.
 */
bool DRW_culling_sphere_test(const DRWView *view, const BoundSphere *bsphere);
/**
 * \return True if the given BoundBox intersect the current view frustum.
 * bbox must be in world space.
 */
bool DRW_culling_box_test(const DRWView *view, const BoundBox *bbox);
/**
 * \return True if the view frustum is inside or intersect the given plane.
 * plane must be in world space.
 */
bool DRW_culling_plane_test(const DRWView *view, const float plane[4]);
/**
 * Return True if the given box intersect the current view frustum.
 * This function will have to be replaced when world space bounding-box per objects is implemented.
 */
bool DRW_culling_min_max_test(const DRWView *view, float obmat[4][4], float min[3], float max[3]);

void DRW_culling_frustum_corners_get(const DRWView *view, BoundBox *corners);
void DRW_culling_frustum_planes_get(const DRWView *view, float planes[6][4]);

/* Viewport. */

const float *DRW_viewport_size_get();
const float *DRW_viewport_invert_size_get();
const float *DRW_viewport_pixelsize_get();

DefaultFramebufferList *DRW_viewport_framebuffer_list_get();
DefaultTextureList *DRW_viewport_texture_list_get();

void DRW_viewport_request_redraw();

void DRW_render_to_image(RenderEngine *engine, Depsgraph *depsgraph);
void DRW_render_object_iter(
    void *vedata,
    RenderEngine *engine,
    Depsgraph *depsgraph,
    void (*callback)(void *vedata, Object *ob, RenderEngine *engine, Depsgraph *depsgraph));
/**
 * Must run after all instance datas have been added.
 */
void DRW_render_instance_buffer_finish();
/**
 * \warning Changing frame might free the #ViewLayerEngineData.
 */
void DRW_render_set_time(RenderEngine *engine, Depsgraph *depsgraph, int frame, float subframe);
/**
 * \warning only use for custom pipeline. 99% of the time, you don't want to use this.
 */
void DRW_render_viewport_size_set(const int size[2]);

/**
 * Assume a valid GL context is bound (and that the gl_context_mutex has been acquired).
 * This function only setup DST and execute the given function.
 * \warning similar to DRW_render_to_image you cannot use default lists (`dfbl` & `dtxl`).
 */
void DRW_custom_pipeline(DrawEngineType *draw_engine_type,
                         Depsgraph *depsgraph,
                         void (*callback)(void *vedata, void *user_data),
                         void *user_data);
/**
 * Same as `DRW_custom_pipeline` but allow better code-flow than a callback.
 */
void DRW_custom_pipeline_begin(DrawEngineType *draw_engine_type, Depsgraph *depsgraph);
void DRW_custom_pipeline_end();

/**
 * Used when the render engine want to redo another cache populate inside the same render frame.
 */
void DRW_cache_restart();

/* ViewLayers */

void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type);
void **DRW_view_layer_engine_data_ensure_ex(ViewLayer *view_layer,
                                            DrawEngineType *engine_type,
                                            void (*callback)(void *storage));
void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type,
                                         void (*callback)(void *storage));

/* DrawData */

DrawData *DRW_drawdata_get(ID *id, DrawEngineType *engine_type);
DrawData *DRW_drawdata_ensure(ID *id,
                              DrawEngineType *engine_type,
                              size_t size,
                              DrawDataInitCb init_cb,
                              DrawDataFreeCb free_cb);
/**
 * Return NULL if not a dupli or a pointer of pointer to the engine data.
 */
void **DRW_duplidata_get(void *vedata);

/* Settings. */

bool DRW_object_is_renderable(const Object *ob);
/**
 * Does `ob` needs to be rendered in edit mode.
 *
 * When using duplicate linked meshes, objects that are not in edit-mode will be drawn as
 * it is in edit mode, when another object with the same mesh is in edit mode.
 * This will not be the case when one of the objects are influenced by modifiers.
 */
bool DRW_object_is_in_edit_mode(const Object *ob);
/**
 * Return whether this object is visible depending if
 * we are rendering or drawing in the viewport.
 */
int DRW_object_visibility_in_active_context(const Object *ob);
bool DRW_object_use_hide_faces(const Object *ob);

bool DRW_object_is_visible_psys_in_active_context(const Object *object,
                                                  const ParticleSystem *psys);

Object *DRW_object_get_dupli_parent(const Object *ob);
DupliObject *DRW_object_get_dupli(const Object *ob);

/* Draw commands */

void DRW_draw_pass(DRWPass *pass);
/**
 * Draw only a subset of shgroups. Used in special situations as grease pencil strokes.
 */
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group);

void DRW_draw_callbacks_pre_scene();
void DRW_draw_callbacks_post_scene();

/**
 * Reset state to not interfere with other UI draw-call.
 */
void DRW_state_reset_ex(DRWState state);
void DRW_state_reset();
/**
 * Use with care, intended so selection code can override passes depth settings,
 * which is important for selection to work properly.
 *
 * Should be set in main draw loop, cleared afterwards
 */
void DRW_state_lock(DRWState state);

/* Selection. */

void DRW_select_load_id(uint id);

/* Draw State. */

/**
 * When false, drawing doesn't output to a pixel buffer
 * eg: Occlusion queries, or when we have setup a context to draw in already.
 */
bool DRW_state_is_fbo();
/**
 * For when engines need to know if this is drawing for selection or not.
 */
bool DRW_state_is_select();
bool DRW_state_is_material_select();
bool DRW_state_is_depth();
/**
 * Whether we are rendering for an image
 */
bool DRW_state_is_image_render();
/**
 * Whether we are rendering only the render engine,
 * or if we should also render the mode engines.
 */
bool DRW_state_is_scene_render();
/**
 * Whether we are rendering simple opengl render
 */
bool DRW_state_is_viewport_image_render();
bool DRW_state_is_playback();
/**
 * Is the user navigating the region.
 */
bool DRW_state_is_navigating();
/**
 * Should text draw in this mode?
 */
bool DRW_state_show_text();
/**
 * Should draw support elements
 * Objects center, selection outline, probe data, ...
 */
bool DRW_state_draw_support();
/**
 * Whether we should render the background
 */
bool DRW_state_draw_background();

/* Avoid too many lookups while drawing */
struct DRWContextState {

  ARegion *region;       /* 'CTX_wm_region(C)' */
  RegionView3D *rv3d;    /* 'CTX_wm_region_view3d(C)' */
  View3D *v3d;           /* 'CTX_wm_view3d(C)' */
  SpaceLink *space_data; /* 'CTX_wm_space_data(C)' */

  Scene *scene;          /* 'CTX_data_scene(C)' */
  ViewLayer *view_layer; /* 'CTX_data_view_layer(C)' */

  /* Use 'object_edit' for edit-mode */
  Object *obact;

  RenderEngineType *engine_type;

  Depsgraph *depsgraph;

  TaskGraph *task_graph;

  eObjectMode object_mode;

  eGPUShaderConfig sh_cfg;

  /** Last resort (some functions take this as an arg so we can't easily avoid).
   * May be NULL when used for selection or depth buffer. */
  const bContext *evil_C;

  /* ---- */

  /* Cache: initialized by 'drw_context_state_init'. */
  Object *object_pose;
  Object *object_edit;
};

const DRWContextState *DRW_context_state_get();

void DRW_mesh_batch_cache_get_attributes(Object *object,
                                         Mesh *mesh,
                                         blender::draw::DRW_Attributes **r_attrs,
                                         blender::draw::DRW_MeshCDMask **r_cd_needed);

void DRW_sculpt_debug_cb(
    PBVHNode *node, void *user_data, const float bmin[3], const float bmax[3], PBVHNodeFlags flag);
