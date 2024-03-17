/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

/* Private functions / structs of the draw manager */

#pragma once

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "BLI_assert.h"
#include "BLI_linklist.h"
#include "BLI_memblock.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "GPU_batch.h"
#include "GPU_context.h"
#include "GPU_drawlist.h"
#include "GPU_framebuffer.h"
#include "GPU_shader.h"
#include "GPU_uniform_buffer.hh"
#include "GPU_viewport.h"

#include "draw_instance_data.h"
#include "draw_shader_shared.h"

struct DupliObject;
struct Object;
namespace blender::draw {
struct CurvesUniformBufPool;
}

/** Use draw manager to call GPU_select, see: #DRW_draw_select_loop */
#define USE_GPU_SELECT

/** Use draw-call batching using instanced rendering. */
#define USE_BATCHING 1

// #define DRW_DEBUG_CULLING
#define DRW_DEBUG_USE_UNIFORM_NAME 0
#define DRW_UNIFORM_BUFFER_NAME 64

/* -------------------------------------------------------------------- */
/** \name Profiling
 * \{ */

#define USE_PROFILE

#ifdef USE_PROFILE
#  include "BLI_time.h"

#  define PROFILE_TIMER_FALLOFF 0.04

#  define PROFILE_START(time_start) \
    double time_start = BLI_time_now_seconds(); \
    ((void)0)

#  define PROFILE_END_ACCUM(time_accum, time_start) \
    { \
      time_accum += (BLI_time_now_seconds() - time_start) * 1e3; \
    } \
    ((void)0)

/* exp average */
#  define PROFILE_END_UPDATE(time_update, time_start) \
    { \
      double _time_delta = (BLI_time_now_seconds() - time_start) * 1e3; \
      time_update = (time_update * (1.0 - PROFILE_TIMER_FALLOFF)) + \
                    (_time_delta * PROFILE_TIMER_FALLOFF); \
    } \
    ((void)0)

#else /* USE_PROFILE */

#  define PROFILE_START(time_start) ((void)0)
#  define PROFILE_END_ACCUM(time_accum, time_start) ((void)0)
#  define PROFILE_END_UPDATE(time_update, time_start) ((void)0)

#endif /* USE_PROFILE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Structure
 *
 * Data structure to for registered draw engines that can store draw manager
 * specific data.
 * \{ */

typedef struct DRWRegisteredDrawEngine {
  void /*DRWRegisteredDrawEngine*/ *next, *prev;
  DrawEngineType *draw_engine;
  /** Index of the type in the lists. Index is used for dupli data. */
  int index;
} DRWRegisteredDrawEngine;

/**
 * Data structure containing all drawcalls organized by passes and materials.
 * DRWPass > DRWShadingGroup > DRWCall > DRWCallState
 *                           > DRWUniform
 */

typedef struct DRWCullingState {
  uint32_t mask;
  /* Culling: Using Bounding Sphere for now for faster culling.
   * Not ideal for planes. Could be extended. */
  BoundSphere bsphere;
  /* Grrr only used by EEVEE. */
  void *user_data;
} DRWCullingState;

/* Minimum max UBO size is 64KiB. We take the largest
 * UBO struct and alloc the max number.
 * `((1 << 16) / sizeof(DRWObjectMatrix)) = 512`
 * Keep in sync with `common_view_lib.glsl`. */
#define DRW_RESOURCE_CHUNK_LEN 512

/**
 * Identifier used to sort similar drawcalls together.
 * Also used to reference elements inside memory blocks.
 *
 * From MSB to LSB
 * 1 bit for negative scale.
 * 22 bits for chunk id.
 * 9 bits for resource id inside the chunk. (can go up to 511)
 * |-|----------------------|---------|
 *
 * Use manual bit-shift and mask instead of bit-fields to avoid
 * compiler dependent behavior that would mess the ordering of
 * the members thus changing the sorting order.
 */
typedef uint32_t DRWResourceHandle;

BLI_INLINE uint32_t DRW_handle_negative_scale_get(const DRWResourceHandle *handle)
{
  return (*handle & 0x80000000) != 0;
}

BLI_INLINE uint32_t DRW_handle_chunk_get(const DRWResourceHandle *handle)
{
  return (*handle & 0x7FFFFFFF) >> 9;
}

BLI_INLINE uint32_t DRW_handle_id_get(const DRWResourceHandle *handle)
{
  return (*handle & 0x000001FF);
}

BLI_INLINE void DRW_handle_increment(DRWResourceHandle *handle)
{
  *handle += 1;
}

BLI_INLINE void DRW_handle_negative_scale_enable(DRWResourceHandle *handle)
{
  *handle |= 0x80000000;
}

BLI_INLINE void *DRW_memblock_elem_from_handle(struct BLI_memblock *memblock,
                                               const DRWResourceHandle *handle)
{
  int elem = DRW_handle_id_get(handle);
  int chunk = DRW_handle_chunk_get(handle);
  return BLI_memblock_elem_get(memblock, chunk, elem);
}

typedef struct DRWObjectMatrix {
  float model[4][4];
  float modelinverse[4][4];
} DRWObjectMatrix;

typedef struct DRWObjectInfos {
  float orcotexfac[2][4];
  float ob_color[4];
  float ob_index;
  float pad; /*UNUSED*/
  float ob_random;
  float ob_flag; /* Sign is negative scaling. */
} DRWObjectInfos;

BLI_STATIC_ASSERT_ALIGN(DRWObjectMatrix, 16)
BLI_STATIC_ASSERT_ALIGN(DRWObjectInfos, 16)

typedef enum {
  /* Draw Commands */
  DRW_CMD_DRAW = 0, /* Only sortable type. Must be 0. */
  DRW_CMD_DRAW_RANGE = 1,
  DRW_CMD_DRAW_INSTANCE = 2,
  DRW_CMD_DRAW_INSTANCE_RANGE = 3,
  DRW_CMD_DRAW_PROCEDURAL = 4,
  DRW_CMD_DRAW_INDIRECT = 5,

  /* Compute Commands. */
  DRW_CMD_COMPUTE = 8,
  DRW_CMD_COMPUTE_REF = 9,
  DRW_CMD_COMPUTE_INDIRECT = 10,

  /* Other Commands */
  DRW_CMD_BARRIER = 11,
  DRW_CMD_CLEAR = 12,
  DRW_CMD_DRWSTATE = 13,
  DRW_CMD_STENCIL = 14,
  DRW_CMD_SELECTID = 15,
  /* Needs to fit in 4bits */
} eDRWCommandType;

#define DRW_MAX_DRAW_CMD_TYPE DRW_CMD_DRAW_INDIRECT

typedef struct DRWCommandDraw {
  GPUBatch *batch;
  DRWResourceHandle handle;
} DRWCommandDraw;

/* Assume DRWResourceHandle to be 0. */
typedef struct DRWCommandDrawRange {
  GPUBatch *batch;
  DRWResourceHandle handle;
  uint vert_first;
  uint vert_count;
} DRWCommandDrawRange;

typedef struct DRWCommandDrawInstance {
  GPUBatch *batch;
  DRWResourceHandle handle;
  uint inst_count;
  uint use_attrs; /* bool */
} DRWCommandDrawInstance;

typedef struct DRWCommandDrawInstanceRange {
  GPUBatch *batch;
  DRWResourceHandle handle;
  uint inst_first;
  uint inst_count;
} DRWCommandDrawInstanceRange;

typedef struct DRWCommandDrawIndirect {
  GPUBatch *batch;
  DRWResourceHandle handle;
  GPUStorageBuf *indirect_buf;
} DRWCommandDrawIndirect;

typedef struct DRWCommandCompute {
  int groups_x_len;
  int groups_y_len;
  int groups_z_len;
} DRWCommandCompute;

typedef struct DRWCommandComputeRef {
  int *groups_ref;
} DRWCommandComputeRef;

typedef struct DRWCommandComputeIndirect {
  GPUStorageBuf *indirect_buf;
} DRWCommandComputeIndirect;

typedef struct DRWCommandBarrier {
  eGPUBarrier type;
} DRWCommandBarrier;

typedef struct DRWCommandDrawProcedural {
  GPUBatch *batch;
  DRWResourceHandle handle;
  uint vert_count;
} DRWCommandDrawProcedural;

typedef struct DRWCommandSetMutableState {
  /** State changes (or'd or and'd with the pass's state) */
  DRWState enable;
  DRWState disable;
} DRWCommandSetMutableState;

typedef struct DRWCommandSetStencil {
  uint write_mask;
  uint comp_mask;
  uint ref;
} DRWCommandSetStencil;

typedef struct DRWCommandSetSelectID {
  GPUVertBuf *select_buf;
  uint select_id;
} DRWCommandSetSelectID;

typedef struct DRWCommandClear {
  eGPUFrameBufferBits clear_channels;
  uchar r, g, b, a; /* [0..1] for each channels. Normalized. */
  float depth;      /* [0..1] for depth. Normalized. */
  uchar stencil;    /* Stencil value [0..255] */
} DRWCommandClear;

typedef union DRWCommand {
  DRWCommandDraw draw;
  DRWCommandDrawRange range;
  DRWCommandDrawInstance instance;
  DRWCommandDrawInstanceRange instance_range;
  DRWCommandDrawProcedural procedural;
  DRWCommandDrawIndirect draw_indirect;
  DRWCommandCompute compute;
  DRWCommandComputeRef compute_ref;
  DRWCommandComputeIndirect compute_indirect;
  DRWCommandBarrier barrier;
  DRWCommandSetMutableState state;
  DRWCommandSetStencil stencil;
  DRWCommandSetSelectID select_id;
  DRWCommandClear clear;
} DRWCommand;

/** Used for aggregating calls into #GPUVertBuf's. */
struct DRWCallBuffer {
  GPUVertBuf *buf;
  GPUVertBuf *buf_select;
  int count;
};

/** Used by #DRWUniform.type */
/* TODO(@jbakker): rename to DRW_RESOURCE/DRWResourceType. */
typedef enum {
  DRW_UNIFORM_INT = 0,
  DRW_UNIFORM_INT_COPY,
  DRW_UNIFORM_FLOAT,
  DRW_UNIFORM_FLOAT_COPY,
  DRW_UNIFORM_TEXTURE,
  DRW_UNIFORM_TEXTURE_REF,
  DRW_UNIFORM_IMAGE,
  DRW_UNIFORM_IMAGE_REF,
  DRW_UNIFORM_BLOCK,
  DRW_UNIFORM_BLOCK_REF,
  DRW_UNIFORM_STORAGE_BLOCK,
  DRW_UNIFORM_STORAGE_BLOCK_REF,
  DRW_UNIFORM_TFEEDBACK_TARGET,
  DRW_UNIFORM_VERTEX_BUFFER_AS_TEXTURE,
  DRW_UNIFORM_VERTEX_BUFFER_AS_TEXTURE_REF,
  DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE,
  DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE_REF,
  /** Per drawcall uniforms/UBO */
  DRW_UNIFORM_BLOCK_OBMATS,
  DRW_UNIFORM_BLOCK_OBINFOS,
  DRW_UNIFORM_BLOCK_OBATTRS,
  DRW_UNIFORM_BLOCK_VLATTRS,
  DRW_UNIFORM_RESOURCE_CHUNK,
  DRW_UNIFORM_RESOURCE_ID,
  /** Legacy / Fallback */
  DRW_UNIFORM_BASE_INSTANCE,
  DRW_UNIFORM_MODEL_MATRIX,
  DRW_UNIFORM_MODEL_MATRIX_INVERSE,
  /* WARNING: set DRWUniform->type
   * bit length accordingly. */
} DRWUniformType;

struct DRWUniform {
  union {
    /* For reference or array/vector types. */
    const void *pvalue;
    /* DRW_UNIFORM_TEXTURE */
    struct {
      union {
        GPUTexture *texture;
        GPUTexture **texture_ref;
      };
      GPUSamplerState sampler_state;
    };
    /* DRW_UNIFORM_BLOCK */
    union {
      GPUUniformBuf *block;
      GPUUniformBuf **block_ref;
    };
    /* DRW_UNIFORM_STORAGE_BLOCK */
    union {
      GPUStorageBuf *ssbo;
      GPUStorageBuf **ssbo_ref;
    };
    /* DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE */
    union {
      GPUVertBuf *vertbuf;
      GPUVertBuf **vertbuf_ref;
    };
    /* DRW_UNIFORM_FLOAT_COPY */
    float fvalue[4];
    /* DRW_UNIFORM_INT_COPY */
    int ivalue[4];
    /* DRW_UNIFORM_BLOCK_OBATTRS */
    const struct GPUUniformAttrList *uniform_attrs;
  };
  int location;      /* Uniform location or binding point for textures and UBO's. */
  uint8_t type;      /* #DRWUniformType */
  uint8_t length;    /* Length of vector types. */
  uint8_t arraysize; /* Array size of scalar/vector types. */
};

struct DRWShadingGroup {
  DRWShadingGroup *next;

  GPUShader *shader;                /* Shader to bind */
  struct DRWUniformChunk *uniforms; /* Uniforms pointers */

  struct {
    /* Chunks of draw calls. */
    struct DRWCommandChunk *first, *last;
  } cmd;

  union {
    /* This struct is used during cache populate. */
    struct {
      int objectinfo;                /* Equal to 1 if the shader needs obinfos. */
      DRWResourceHandle pass_handle; /* Memblock key to parent pass. */

      /* Set of uniform attributes used by this shader. */
      const struct GPUUniformAttrList *uniform_attrs;
    };
    /* This struct is used after cache populate if using the Z sorting.
     * It will not conflict with the above struct. */
    struct {
      float distance;      /* Distance from camera. */
      uint original_index; /* Original position inside the shgroup list. */
    } z_sorting;
  };
};

#define MAX_PASS_NAME 32

struct DRWPass {
  /* Linked list */
  struct {
    DRWShadingGroup *first;
    DRWShadingGroup *last;
  } shgroups;

  /* Draw the shgroups of this pass instead.
   * This avoid duplicating drawcalls/shgroups
   * for similar passes. */
  DRWPass *original;
  /* Link list of additional passes to render. */
  DRWPass *next;

  DRWResourceHandle handle;
  DRWState state;
  char name[MAX_PASS_NAME];
};

#define MAX_CULLED_VIEWS 32

struct DRWView {
  /**
   * These float4x4 (as well as the ViewMatrices) have alignment requirements in C++
   * (see math::MatBase) that isn't fulfilled in C. So they need to be manually aligned.
   * Since the DRWView are allocated using BLI_memblock, the chunks are given to be 16 bytes
   * aligned (equal to the alignment of float4x4). We then assert that the DRWView itself is 16
   * bytes aligned.
   */
  float4x4 persmat;
  float4x4 persinv;
  ViewMatrices storage;

  /** Parent view if this is a sub view. NULL otherwise. */
  struct DRWView *parent;

  float4 clip_planes[6];

  /** Number of active clip planes. */
  int clip_planes_len;
  /** Does culling result needs to be updated. */
  bool is_dirty;
  /** Does facing needs to be reversed? */
  bool is_inverted;
  /** Culling */
  uint32_t culling_mask;
  BoundBox frustum_corners;
  BoundSphere frustum_bsphere;
  float frustum_planes[6][4];
  /** Custom visibility function. */
  DRWCallVisibilityFn *visibility_fn;
  void *user_data;
};
/* Needed to assert that alignment is the same in C++ and C. */
BLI_STATIC_ASSERT_ALIGN(DRWView, 16);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Chunks
 *
 * In order to keep a cache friendly data structure,
 * we allocate most of our little data into chunks of multiple item.
 * Iteration, allocation and memory usage are better.
 * We lose a bit of memory by allocating more than what we need
 * but it's counterbalanced by not needing the linked-list pointers
 * for each item.
 * \{ */

typedef struct DRWUniformChunk {
  struct DRWUniformChunk *next; /* single-linked list */
  uint32_t uniform_len;
  uint32_t uniform_used;
  DRWUniform uniforms[10];
} DRWUniformChunk;

typedef struct DRWCommandChunk {
  struct DRWCommandChunk *next;
  uint32_t command_len;
  uint32_t command_used;
  /* 4bits for each command. */
  uint64_t command_type[6];
  /* -- 64 bytes aligned -- */
  DRWCommand commands[96];
  /* -- 64 bytes aligned -- */
} DRWCommandChunk;

typedef struct DRWCommandSmallChunk {
  struct DRWCommandChunk *next;
  uint32_t command_len;
  uint32_t command_used;
  /* 4bits for each command. */
  /* TODO: reduce size of command_type. */
  uint64_t command_type[6];
  DRWCommand commands[6];
} DRWCommandSmallChunk;

/* Only true for 64-bit platforms. */
#ifdef __LP64__
BLI_STATIC_ASSERT_ALIGN(DRWCommandChunk, 16);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory Pools
 * \{ */

/** Contains memory pools information. */
typedef struct DRWData {
  /** Instance data. */
  DRWInstanceDataList *idatalist;
  /** Memory-pools for draw-calls. */
  struct BLI_memblock *commands;
  struct BLI_memblock *commands_small;
  struct BLI_memblock *callbuffers;
  struct BLI_memblock *obmats;
  struct BLI_memblock *obinfos;
  struct BLI_memblock *cullstates;
  struct BLI_memblock *shgroups;
  struct BLI_memblock *uniforms;
  struct BLI_memblock *views;
  struct BLI_memblock *passes;
  struct BLI_memblock *images;
  struct GPUUniformBuf **matrices_ubo;
  struct GPUUniformBuf **obinfos_ubo;
  struct GHash *obattrs_ubo_pool;
  struct GHash *vlattrs_name_cache;
  struct ListBase vlattrs_name_list;
  struct LayerAttribute *vlattrs_buf;
  struct GPUUniformBuf *vlattrs_ubo;
  bool vlattrs_ubo_ready;
  uint ubo_len;
  /** Per draw-call volume object data. */
  void *volume_grids_ubos; /* VolumeUniformBufPool */
  /** List of smoke textures to free after drawing. */
  ListBase smoke_textures;
  /**
   * Texture pool to reuse temp texture across engines.
   * TODO(@fclem): The pool could be shared even between view-ports.
   */
  struct DRWTexturePool *texture_pool;
  /** Per stereo view data. Contains engine data and default frame-buffers. */
  struct DRWViewData *view_data[2];
  /** Per draw-call curves object data. */
  blender::draw::CurvesUniformBufPool *curves_ubos;
} DRWData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Manager
 * \{ */

typedef struct DupliKey {
  struct Object *ob;
  struct ID *ob_data;
} DupliKey;

#define DST_MAX_SLOTS 64  /* Cannot be changed without modifying RST.bound_tex_slots */
#define MAX_CLIP_PLANES 6 /* GL_MAX_CLIP_PLANES is at least 6 */
#define STENCIL_UNDEFINED 256
#define DRW_DRAWLIST_LEN 256
typedef struct DRWManager {
  /* TODO: clean up this struct a bit. */
  /* Cache generation */
  /* TODO(@fclem): Rename to data. */
  DRWData *vmempool;
  /** Active view data structure for one of the 2 stereo view. Not related to DRWView. */
  struct DRWViewData *view_data_active;
  /* State of the object being evaluated if already allocated. */
  DRWResourceHandle ob_handle;
  /** True if current DST.ob_state has its matching DRWObjectInfos init. */
  bool ob_state_obinfo_init;
  /** Handle of current object resource in object resource arrays (DRWObjectMatrices/Infos). */
  DRWResourceHandle resource_handle;
  /** Handle of next DRWPass to be allocated. */
  DRWResourceHandle pass_handle;

  /** Dupli object that corresponds to the current object. */
  struct DupliObject *dupli_source;
  /** Object that created the dupli-list the current object is part of. */
  struct Object *dupli_parent;
  /** Object referenced by the current dupli object. */
  struct Object *dupli_origin;
  /** Object-data referenced by the current dupli object. */
  struct ID *dupli_origin_data;
  /** Hash-map: #DupliKey -> void pointer for each enabled engine. */
  struct GHash *dupli_ghash;
  /** TODO(@fclem): try to remove usage of this. */
  DRWInstanceData *object_instance_data[MAX_INSTANCE_DATA_SIZE];
  /* Dupli data for the current dupli for each enabled engine. */
  void **dupli_datas;

  /* Rendering state */
  GPUShader *shader;
  GPUBatch *batch;

  /* Managed by `DRW_state_set`, `DRW_state_reset` */
  DRWState state;
  DRWState state_lock;

  /* Per viewport */
  GPUViewport *viewport;
  struct GPUFrameBuffer *default_framebuffer;
  float size[2];
  float inv_size[2];
  float pixsize;

  struct {
    uint is_select : 1;
    uint is_material_select : 1;
    uint is_depth : 1;
    uint is_image_render : 1;
    uint is_scene_render : 1;
    uint draw_background : 1;
    uint draw_text : 1;
  } options;

  /* Current rendering context */
  DRWContextState draw_ctx;

  /* Convenience pointer to text_store owned by the viewport */
  struct DRWTextStore **text_store_p;

  bool buffer_finish_called; /* Avoid bad usage of DRW_render_instance_buffer_finish */

  /** True, when drawing is in progress, see #DRW_draw_in_progress. */
  bool in_progress;

  DRWView *view_default;
  DRWView *view_active;
  DRWView *view_previous;
  uint primary_view_num;

#ifdef USE_GPU_SELECT
  uint select_id;
#endif

  struct TaskGraph *task_graph;
  /* Contains list of objects that needs to be extracted from other objects. */
  struct GSet *delayed_extraction;

  /* ---------- Nothing after this point is cleared after use ----------- */

  /* system_gpu_context serves as the offset for clearing only
   * the top portion of the struct so DO NOT MOVE IT! */
  /** Unique ghost context used by the draw manager. */
  void *system_gpu_context;
  GPUContext *blender_gpu_context;
  /** Mutex to lock the drw manager and avoid concurrent context usage. */
  TicketMutex *system_gpu_context_mutex;

  GPUDrawList *draw_list;

  DRWDebugModule *debug;
} DRWManager;

extern DRWManager DST; /* TODO: get rid of this and allow multi-threaded rendering. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Functions
 * \{ */

void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags);

void *drw_viewport_engine_data_ensure(void *engine_type);

void drw_state_set(DRWState state);

void drw_debug_draw(void);
void drw_debug_init(void);
void drw_debug_module_free(DRWDebugModule *module);
GPUStorageBuf *drw_debug_gpu_draw_buf_get(void);
GPUStorageBuf *drw_debug_gpu_print_buf_get(void);

eDRWCommandType command_type_get(const uint64_t *command_type_bits, int index);

void drw_batch_cache_validate(Object *ob);
void drw_batch_cache_generate_requested(struct Object *ob);

/**
 * \warning Only evaluated mesh data is handled by this delayed generation.
 */
void drw_batch_cache_generate_requested_delayed(Object *ob);
void drw_batch_cache_generate_requested_evaluated_mesh_or_curve(Object *ob);

void drw_resource_buffer_finish(DRWData *vmempool);

/* Procedural Drawing */
GPUBatch *drw_cache_procedural_points_get(void);
GPUBatch *drw_cache_procedural_lines_get(void);
GPUBatch *drw_cache_procedural_triangles_get(void);
GPUBatch *drw_cache_procedural_triangle_strips_get(void);

void drw_uniform_attrs_pool_update(struct GHash *table,
                                   const struct GPUUniformAttrList *key,
                                   DRWResourceHandle *handle,
                                   const struct Object *ob,
                                   const struct Object *dupli_parent,
                                   const struct DupliObject *dupli_source);

GPUUniformBuf *drw_ensure_layer_attribute_buffer(void);

double *drw_engine_data_cache_time_get(GPUViewport *viewport);
void *drw_engine_data_engine_data_create(GPUViewport *viewport, void *engine_type);
void *drw_engine_data_engine_data_get(GPUViewport *viewport, void *engine_handle);
bool drw_engine_data_engines_data_validate(GPUViewport *viewport, void **engine_handle_array);
void drw_engine_data_cache_release(GPUViewport *viewport);
void drw_engine_data_free(GPUViewport *viewport);

struct GPUMaterial;

namespace blender::draw {

struct DRW_Attributes;
struct DRW_MeshCDMask;

void DRW_mesh_get_attributes(const struct Object *object,
                             const struct Mesh *mesh,
                             const struct GPUMaterial *const *gpumat_array,
                             int gpumat_array_len,
                             DRW_Attributes *r_attrs,
                             DRW_MeshCDMask *r_cd_needed);

}  // namespace blender::draw

void DRW_manager_begin_sync(void);
void DRW_manager_end_sync(void);

/** \} */
