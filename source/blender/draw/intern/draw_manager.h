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

/* Private functions / structs of the draw manager */

#ifndef __DRAW_MANAGER_H__
#define __DRAW_MANAGER_H__

#include "DRW_engine.h"
#include "DRW_render.h"

#include "BLI_linklist.h"
#include "BLI_threads.h"

#include "GPU_batch.h"
#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_shader.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"

#include "draw_instance_data.h"

/* Use draw manager to call GPU_select, see: DRW_draw_select_loop */
#define USE_GPU_SELECT

// #define DRW_DEBUG_CULLING
#define DRW_DEBUG_USE_UNIFORM_NAME 0
#define DRW_UNIFORM_BUFFER_NAME 64

/* ------------ Profiling --------------- */

#define USE_PROFILE

#ifdef USE_PROFILE
#  include "PIL_time.h"

#  define PROFILE_TIMER_FALLOFF 0.04

#  define PROFILE_START(time_start) \
    double time_start = PIL_check_seconds_timer(); \
    ((void)0)

#  define PROFILE_END_ACCUM(time_accum, time_start) \
    { \
      time_accum += (PIL_check_seconds_timer() - time_start) * 1e3; \
    } \
    ((void)0)

/* exp average */
#  define PROFILE_END_UPDATE(time_update, time_start) \
    { \
      double _time_delta = (PIL_check_seconds_timer() - time_start) * 1e3; \
      time_update = (time_update * (1.0 - PROFILE_TIMER_FALLOFF)) + \
                    (_time_delta * PROFILE_TIMER_FALLOFF); \
    } \
    ((void)0)

#else /* USE_PROFILE */

#  define PROFILE_START(time_start) ((void)0)
#  define PROFILE_END_ACCUM(time_accum, time_start) ((void)0)
#  define PROFILE_END_UPDATE(time_update, time_start) ((void)0)

#endif /* USE_PROFILE */

/* ------------ Data Structure --------------- */
/**
 * Data structure containing all drawcalls organized by passes and materials.
 * DRWPass > DRWShadingGroup > DRWCall > DRWCallState
 *                           > DRWUniform
 */

/* Used by DRWCallState.flag */
enum {
  DRW_CALL_NEGSCALE = (1 << 1),
};

/* Used by DRWCallState.matflag */
enum {
  DRW_CALL_MODELINVERSE = (1 << 0),
  DRW_CALL_MODELVIEWPROJECTION = (1 << 1),
  DRW_CALL_ORCOTEXFAC = (1 << 2),
  DRW_CALL_OBJECTINFO = (1 << 3),
};

typedef struct DRWCullingState {
  uint32_t mask;
  /* Culling: Using Bounding Sphere for now for faster culling.
   * Not ideal for planes. Could be extended. */
  BoundSphere bsphere;
  /* Grrr only used by EEVEE. */
  void *user_data;
} DRWCullingState;

typedef struct DRWCallState {
  DRWCullingState *culling;
  uchar flag;
  uchar matflag; /* Which matrices to compute. */
  short ob_index;
  /* Matrices */
  float model[4][4];
  float modelinverse[4][4];
  float orcotexfac[2][3];
  float ob_random;
} DRWCallState;

typedef struct DRWCall {
  struct DRWCall *next;
  DRWCallState *state;

  GPUBatch *batch;
  uint vert_first;
  uint vert_count;
  uint inst_count;

#ifdef USE_GPU_SELECT
  /* TODO(fclem) remove once we have a dedicated selection engine. */
  int select_id;
  GPUVertBuf *inst_selectid;
#endif
} DRWCall;

/* Used by DRWUniform.type */
typedef enum {
  DRW_UNIFORM_INT,
  DRW_UNIFORM_INT_COPY,
  DRW_UNIFORM_FLOAT,
  DRW_UNIFORM_FLOAT_COPY,
  DRW_UNIFORM_TEXTURE,
  DRW_UNIFORM_TEXTURE_PERSIST,
  DRW_UNIFORM_TEXTURE_REF,
  DRW_UNIFORM_BLOCK,
  DRW_UNIFORM_BLOCK_PERSIST,
} DRWUniformType;

struct DRWUniform {
  DRWUniform *next; /* single-linked list */
  union {
    /* For reference or array/vector types. */
    const void *pvalue;
    /* Single values. */
    float fvalue[2];
    int ivalue[2];
  };
  int name_ofs; /* name offset in name buffer. */
  int location;
  char type;      /* DRWUniformType */
  char length;    /* cannot be more than 16 */
  char arraysize; /* cannot be more than 16 too */
};

struct DRWShadingGroup {
  DRWShadingGroup *next;

  GPUShader *shader;    /* Shader to bind */
  DRWUniform *uniforms; /* Uniforms pointers */

  struct {
    DRWCall *first, *last; /* Linked list of DRWCall */
  } calls;

  /** TODO Maybe remove from here */
  struct GPUVertBuf *tfeedback_target;

  /** State changes for this batch only (or'd with the pass's state) */
  DRWState state_extra;
  /** State changes for this batch only (and'd with the pass's state) */
  DRWState state_extra_disable;
  /** Stencil mask to use for stencil test / write operations */
  uint stencil_mask;

  /* Builtin matrices locations */
  int model;
  int modelinverse;
  int modelviewprojection;
  int orcotexfac;
  int callid;
  int objectinfo;
  uchar matflag; /* Matrices needed, same as DRWCall.flag */

  DRWPass *pass_parent; /* backlink to pass we're in */
};

#define MAX_PASS_NAME 32

struct DRWPass {
  /* Linked list */
  struct {
    DRWShadingGroup *first;
    DRWShadingGroup *last;
  } shgroups;

  DRWState state;
  char name[MAX_PASS_NAME];
};

/* keep in sync with viewBlock */
typedef struct DRWViewUboStorage {
  /* View matrices */
  float persmat[4][4];
  float persinv[4][4];
  float viewmat[4][4];
  float viewinv[4][4];
  float winmat[4][4];
  float wininv[4][4];

  float clipplanes[6][4];
  /* Should not be here. Not view dependant (only main view). */
  float viewcamtexcofac[4];
} DRWViewUboStorage;

#define MAX_CULLED_VIEWS 32

struct DRWView {
  /** Parent view if this is a sub view. NULL otherwise. */
  struct DRWView *parent;

  DRWViewUboStorage storage;
  /** Number of active clipplanes. */
  int clip_planes_len;
  /** Does culling result needs to be updated. */
  bool is_dirty;
  /** Culling */
  uint32_t culling_mask;
  BoundBox frustum_corners;
  BoundSphere frustum_bsphere;
  float frustum_planes[6][4];
  /** Custom visibility function. */
  DRWCallVisibilityFn *visibility_fn;
  void *user_data;
};

/* TODO(fclem): Future awaits */
#if 0
typedef struct ModelUboStorage {
  float model[4][4];
  float modelinverse[4][4];
} ModelUboStorage;
#endif

/* ------------- DRAW DEBUG ------------ */

typedef struct DRWDebugLine {
  struct DRWDebugLine *next; /* linked list */
  float pos[2][3];
  float color[4];
} DRWDebugLine;

typedef struct DRWDebugSphere {
  struct DRWDebugSphere *next; /* linked list */
  float mat[4][4];
  float color[4];
} DRWDebugSphere;

/* ------------- DRAW MANAGER ------------ */

#define DST_MAX_SLOTS 64  /* Cannot be changed without modifying RST.bound_tex_slots */
#define MAX_CLIP_PLANES 6 /* GL_MAX_CLIP_PLANES is at least 6 */
#define STENCIL_UNDEFINED 256
typedef struct DRWManager {
  /* TODO clean up this struct a bit */
  /* Cache generation */
  ViewportMemoryPool *vmempool;
  DRWInstanceDataList *idatalist;
  DRWInstanceData *object_instance_data[MAX_INSTANCE_DATA_SIZE];
  /* State of the object being evaluated if already allocated. */
  DRWCallState *ob_state;
  struct DupliObject *dupli_source;
  struct Object *dupli_parent;
  struct Object *dupli_origin;
  struct GHash *dupli_ghash;
  void **dupli_datas; /* Array of dupli_data (one for each enabled engine) to handle duplis. */

  /* Rendering state */
  GPUShader *shader;

  /* Managed by `DRW_state_set`, `DRW_state_reset` */
  DRWState state;
  DRWState state_lock;
  uint stencil_mask;

  /* Per viewport */
  GPUViewport *viewport;
  struct GPUFrameBuffer *default_framebuffer;
  float size[2];
  float inv_size[2];
  float screenvecs[2][3];
  float pixsize;

  struct {
    uint is_select : 1;
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

  ListBase enabled_engines; /* RenderEngineType */
  void **vedata_array;      /* ViewportEngineData */
  int enabled_engine_count; /* Length of enabled_engines list. */

  bool buffer_finish_called; /* Avoid bad usage of DRW_render_instance_buffer_finish */

  DRWView *view_default;
  DRWView *view_active;
  DRWView *view_previous;
  uint primary_view_ct;
  /** TODO(fclem) Remove this. Only here to support
   * shaders without common_view_lib.glsl */
  DRWViewUboStorage view_storage_cpy;

#ifdef USE_GPU_SELECT
  uint select_id;
#endif

  /* ---------- Nothing after this point is cleared after use ----------- */

  /* gl_context serves as the offset for clearing only
   * the top portion of the struct so DO NOT MOVE IT! */
  /** Unique ghost context used by the draw manager. */
  void *gl_context;
  GPUContext *gpu_context;
  /** Mutex to lock the drw manager and avoid concurrent context usage. */
  TicketMutex *gl_context_mutex;

  /** GPU Resource State: Memory storage between drawing. */
  struct {
    /* High end GPUs supports up to 32 binds per shader stage.
     * We only use textures during the vertex and fragment stage,
     * so 2 * 32 slots is a nice limit. */
    GPUTexture *bound_texs[DST_MAX_SLOTS];
    uint64_t bound_tex_slots;
    uint64_t bound_tex_slots_persist;

    GPUUniformBuffer *bound_ubos[DST_MAX_SLOTS];
    uint64_t bound_ubo_slots;
    uint64_t bound_ubo_slots_persist;
  } RST;

  struct {
    /* TODO(fclem) optimize: use chunks. */
    DRWDebugLine *lines;
    DRWDebugSphere *spheres;
  } debug;

  struct {
    char *buffer;
    uint buffer_len;
    uint buffer_ofs;
  } uniform_names;
} DRWManager;

extern DRWManager DST; /* TODO : get rid of this and allow multithreaded rendering */

/* --------------- FUNCTIONS ------------- */

void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags);

void *drw_viewport_engine_data_ensure(void *engine_type);

void drw_state_set(DRWState state);

void drw_debug_draw(void);
void drw_debug_init(void);

void drw_batch_cache_validate(Object *ob);
void drw_batch_cache_generate_requested(struct Object *ob);

/* Procedural Drawing */
GPUBatch *drw_cache_procedural_points_get(void);
GPUBatch *drw_cache_procedural_lines_get(void);
GPUBatch *drw_cache_procedural_triangles_get(void);

#endif /* __DRAW_MANAGER_H__ */
