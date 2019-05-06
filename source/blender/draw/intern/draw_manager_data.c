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

#include "draw_manager.h"

#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"

#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_mempool.h"

#include "GPU_buffers.h"

#include "intern/gpu_codegen.h"

struct GPUVertFormat *g_pos_format = NULL;

/* -------------------------------------------------------------------- */
/** \name Uniform Buffer Object (DRW_uniformbuffer)
 * \{ */

GPUUniformBuffer *DRW_uniformbuffer_create(int size, const void *data)
{
  return GPU_uniformbuffer_create(size, data, NULL);
}

void DRW_uniformbuffer_update(GPUUniformBuffer *ubo, const void *data)
{
  GPU_uniformbuffer_update(ubo, data);
}

void DRW_uniformbuffer_free(GPUUniformBuffer *ubo)
{
  GPU_uniformbuffer_free(ubo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms (DRW_shgroup_uniform)
 * \{ */

static void drw_shgroup_uniform_create_ex(DRWShadingGroup *shgroup,
                                          int loc,
                                          DRWUniformType type,
                                          const void *value,
                                          int length,
                                          int arraysize)
{
  DRWUniform *uni = BLI_mempool_alloc(DST.vmempool->uniforms);
  uni->location = loc;
  uni->type = type;
  uni->length = length;
  uni->arraysize = arraysize;

  switch (type) {
    case DRW_UNIFORM_INT_COPY:
      uni->ivalue = *((int *)value);
      break;
    case DRW_UNIFORM_BOOL_COPY:
      uni->ivalue = (int)*((bool *)value);
      break;
    case DRW_UNIFORM_FLOAT_COPY:
      uni->fvalue = *((float *)value);
      break;
    default:
      uni->pvalue = value;
      break;
  }

  BLI_LINKS_PREPEND(shgroup->uniforms, uni);
}

static void drw_shgroup_builtin_uniform(
    DRWShadingGroup *shgroup, int builtin, const void *value, int length, int arraysize)
{
  int loc = GPU_shader_get_builtin_uniform(shgroup->shader, builtin);

  if (loc != -1) {
    drw_shgroup_uniform_create_ex(shgroup, loc, DRW_UNIFORM_FLOAT, value, length, arraysize);
  }
}

static void drw_shgroup_uniform(DRWShadingGroup *shgroup,
                                const char *name,
                                DRWUniformType type,
                                const void *value,
                                int length,
                                int arraysize)
{
  int location;
  if (ELEM(type, DRW_UNIFORM_BLOCK, DRW_UNIFORM_BLOCK_PERSIST)) {
    location = GPU_shader_get_uniform_block(shgroup->shader, name);
  }
  else {
    location = GPU_shader_get_uniform(shgroup->shader, name);
  }

  if (location == -1) {
    /* Nice to enable eventually, for now eevee uses uniforms that might not exist. */
    // BLI_assert(0);
    return;
  }

  BLI_assert(arraysize > 0 && arraysize <= 16);
  BLI_assert(length >= 0 && length <= 16);

  drw_shgroup_uniform_create_ex(shgroup, location, type, value, length, arraysize);

  /* If location is -2, the uniform has not yet been queried.
   * We save the name for query just before drawing. */
  if (location == -2 || DRW_DEBUG_USE_UNIFORM_NAME) {
    int ofs = DST.uniform_names.buffer_ofs;
    int max_len = DST.uniform_names.buffer_len - ofs;
    size_t len = strlen(name) + 1;

    if (len >= max_len) {
      DST.uniform_names.buffer_len += DRW_UNIFORM_BUFFER_NAME_INC;
      DST.uniform_names.buffer = MEM_reallocN(DST.uniform_names.buffer,
                                              DST.uniform_names.buffer_len);
    }

    char *dst = DST.uniform_names.buffer + ofs;
    memcpy(dst, name, len); /* Copies NULL terminator. */

    DST.uniform_names.buffer_ofs += len;
    shgroup->uniforms->name_ofs = ofs;
  }
}

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex)
{
  BLI_assert(tex != NULL);
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_TEXTURE, tex, 0, 1);
}

/* Same as DRW_shgroup_uniform_texture but is guaranteed to be bound if shader does not change
 * between shgrp. */
void DRW_shgroup_uniform_texture_persistent(DRWShadingGroup *shgroup,
                                            const char *name,
                                            const GPUTexture *tex)
{
  BLI_assert(tex != NULL);
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_TEXTURE_PERSIST, tex, 0, 1);
}

void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup,
                               const char *name,
                               const GPUUniformBuffer *ubo)
{
  BLI_assert(ubo != NULL);
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_BLOCK, ubo, 0, 1);
}

/* Same as DRW_shgroup_uniform_block but is guaranteed to be bound if shader does not change
 * between shgrp. */
void DRW_shgroup_uniform_block_persistent(DRWShadingGroup *shgroup,
                                          const char *name,
                                          const GPUUniformBuffer *ubo)
{
  BLI_assert(ubo != NULL);
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_BLOCK_PERSIST, ubo, 0, 1);
}

void DRW_shgroup_uniform_texture_ref(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_TEXTURE_REF, tex, 0, 1);
}

void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup,
                              const char *name,
                              const int *value,
                              int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_BOOL, value, 1, arraysize);
}

void DRW_shgroup_uniform_float(DRWShadingGroup *shgroup,
                               const char *name,
                               const float *value,
                               int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 1, arraysize);
}

void DRW_shgroup_uniform_vec2(DRWShadingGroup *shgroup,
                              const char *name,
                              const float *value,
                              int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 2, arraysize);
}

void DRW_shgroup_uniform_vec3(DRWShadingGroup *shgroup,
                              const char *name,
                              const float *value,
                              int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 3, arraysize);
}

void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup,
                              const char *name,
                              const float *value,
                              int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 4, arraysize);
}

void DRW_shgroup_uniform_short_to_int(DRWShadingGroup *shgroup,
                                      const char *name,
                                      const short *value,
                                      int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_SHORT_TO_INT, value, 1, arraysize);
}

void DRW_shgroup_uniform_short_to_float(DRWShadingGroup *shgroup,
                                        const char *name,
                                        const short *value,
                                        int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_SHORT_TO_FLOAT, value, 1, arraysize);
}

void DRW_shgroup_uniform_int(DRWShadingGroup *shgroup,
                             const char *name,
                             const int *value,
                             int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT, value, 1, arraysize);
}

void DRW_shgroup_uniform_ivec2(DRWShadingGroup *shgroup,
                               const char *name,
                               const int *value,
                               int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT, value, 2, arraysize);
}

void DRW_shgroup_uniform_ivec3(DRWShadingGroup *shgroup,
                               const char *name,
                               const int *value,
                               int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT, value, 3, arraysize);
}

void DRW_shgroup_uniform_ivec4(DRWShadingGroup *shgroup,
                               const char *name,
                               const int *value,
                               int arraysize)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT, value, 4, arraysize);
}

void DRW_shgroup_uniform_mat3(DRWShadingGroup *shgroup, const char *name, const float (*value)[3])
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT, (float *)value, 9, 1);
}

void DRW_shgroup_uniform_mat4(DRWShadingGroup *shgroup, const char *name, const float (*value)[4])
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT, (float *)value, 16, 1);
}

/* Stores the int instead of a pointer. */
void DRW_shgroup_uniform_int_copy(DRWShadingGroup *shgroup, const char *name, const int value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT_COPY, &value, 1, 1);
}

void DRW_shgroup_uniform_bool_copy(DRWShadingGroup *shgroup, const char *name, const bool value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_BOOL_COPY, &value, 1, 1);
}

void DRW_shgroup_uniform_float_copy(DRWShadingGroup *shgroup, const char *name, const float value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT_COPY, &value, 1, 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Call (DRW_calls)
 * \{ */

static void drw_call_calc_orco(Object *ob, float (*r_orcofacs)[3])
{
  ID *ob_data = (ob) ? ob->data : NULL;
  float *texcoloc = NULL;
  float *texcosize = NULL;
  if (ob_data != NULL) {
    switch (GS(ob_data->name)) {
      case ID_ME:
        BKE_mesh_texspace_get_reference((Mesh *)ob_data, NULL, &texcoloc, NULL, &texcosize);
        break;
      case ID_CU: {
        Curve *cu = (Curve *)ob_data;
        if (cu->bb == NULL || (cu->bb->flag & BOUNDBOX_DIRTY)) {
          BKE_curve_texspace_calc(cu);
        }
        texcoloc = cu->loc;
        texcosize = cu->size;
        break;
      }
      case ID_MB: {
        MetaBall *mb = (MetaBall *)ob_data;
        texcoloc = mb->loc;
        texcosize = mb->size;
        break;
      }
      default:
        break;
    }
  }

  if ((texcoloc != NULL) && (texcosize != NULL)) {
    mul_v3_v3fl(r_orcofacs[1], texcosize, 2.0f);
    invert_v3(r_orcofacs[1]);
    sub_v3_v3v3(r_orcofacs[0], texcoloc, texcosize);
    negate_v3(r_orcofacs[0]);
    mul_v3_v3(r_orcofacs[0], r_orcofacs[1]); /* result in a nice MADD in the shader */
  }
  else {
    copy_v3_fl(r_orcofacs[0], 0.0f);
    copy_v3_fl(r_orcofacs[1], 1.0f);
  }
}

static void drw_call_state_update_matflag(DRWCallState *state,
                                          DRWShadingGroup *shgroup,
                                          Object *ob)
{
  uint16_t new_flags = ((state->matflag ^ shgroup->matflag) & shgroup->matflag);

  /* HACK: Here we set the matflags bit to 1 when computing the value
   * so that it's not recomputed for other drawcalls.
   * This is the opposite of what draw_matrices_model_prepare() does. */
  state->matflag |= shgroup->matflag;

  /* Orco factors: We compute this at creation to not have to save the *ob_data */
  if ((new_flags & DRW_CALL_ORCOTEXFAC) != 0) {
    drw_call_calc_orco(ob, state->orcotexfac);
  }

  if ((new_flags & DRW_CALL_OBJECTINFO) != 0) {
    state->objectinfo[0] = ob ? ob->index : 0;
    uint random;
    if (DST.dupli_source) {
      random = DST.dupli_source->random_id;
    }
    else {
      random = BLI_hash_int_2d(BLI_hash_string(ob->id.name + 2), 0);
    }
    state->objectinfo[1] = random * (1.0f / (float)0xFFFFFFFF);
  }
}

static DRWCallState *drw_call_state_create(DRWShadingGroup *shgroup, float (*obmat)[4], Object *ob)
{
  DRWCallState *state = BLI_mempool_alloc(DST.vmempool->states);
  state->flag = 0;
  state->cache_id = 0;
  state->visibility_cb = NULL;
  state->matflag = 0;

  /* Matrices */
  if (obmat != NULL) {
    copy_m4_m4(state->model, obmat);

    if (ob && (ob->transflag & OB_NEG_SCALE)) {
      state->flag |= DRW_CALL_NEGSCALE;
    }
  }
  else {
    unit_m4(state->model);
  }

  if (ob != NULL) {
    float corner[3];
    BoundBox *bbox = BKE_object_boundbox_get(ob);
    /* Get BoundSphere center and radius from the BoundBox. */
    mid_v3_v3v3(state->bsphere.center, bbox->vec[0], bbox->vec[6]);
    mul_v3_m4v3(corner, obmat, bbox->vec[0]);
    mul_m4_v3(obmat, state->bsphere.center);
    state->bsphere.radius = len_v3v3(state->bsphere.center, corner);
  }
  else {
    /* Bypass test. */
    state->bsphere.radius = -1.0f;
  }

  drw_call_state_update_matflag(state, shgroup, ob);

  return state;
}

static DRWCallState *drw_call_state_object(DRWShadingGroup *shgroup, float (*obmat)[4], Object *ob)
{
  if (DST.ob_state == NULL) {
    DST.ob_state = drw_call_state_create(shgroup, obmat, ob);
  }
  else {
    /* If the DRWCallState is reused, add necessary matrices. */
    drw_call_state_update_matflag(DST.ob_state, shgroup, ob);
  }

  return DST.ob_state;
}

void DRW_shgroup_call_add(DRWShadingGroup *shgroup, GPUBatch *geom, float (*obmat)[4])
{
  BLI_assert(geom != NULL);
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  call->state = drw_call_state_create(shgroup, obmat, NULL);
  call->type = DRW_CALL_SINGLE;
  call->single.geometry = geom;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

void DRW_shgroup_call_range_add(
    DRWShadingGroup *shgroup, GPUBatch *geom, float (*obmat)[4], uint v_sta, uint v_count)
{
  BLI_assert(geom != NULL);
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));
  BLI_assert(v_count);

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  call->state = drw_call_state_create(shgroup, obmat, NULL);
  call->type = DRW_CALL_RANGE;
  call->range.geometry = geom;
  call->range.start = v_sta;
  call->range.count = v_count;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

static void drw_shgroup_call_procedural_add_ex(DRWShadingGroup *shgroup,
                                               GPUPrimType prim_type,
                                               uint vert_count,
                                               float (*obmat)[4],
                                               Object *ob)
{
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  if (ob) {
    call->state = drw_call_state_object(shgroup, ob->obmat, ob);
  }
  else {
    call->state = drw_call_state_create(shgroup, obmat, NULL);
  }
  call->type = DRW_CALL_PROCEDURAL;
  call->procedural.prim_type = prim_type;
  call->procedural.vert_count = vert_count;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

void DRW_shgroup_call_procedural_points_add(DRWShadingGroup *shgroup,
                                            uint point_len,
                                            float (*obmat)[4])
{
  drw_shgroup_call_procedural_add_ex(shgroup, GPU_PRIM_POINTS, point_len, obmat, NULL);
}

void DRW_shgroup_call_procedural_lines_add(DRWShadingGroup *shgroup,
                                           uint line_count,
                                           float (*obmat)[4])
{
  drw_shgroup_call_procedural_add_ex(shgroup, GPU_PRIM_LINES, line_count * 2, obmat, NULL);
}

void DRW_shgroup_call_procedural_triangles_add(DRWShadingGroup *shgroup,
                                               uint tria_count,
                                               float (*obmat)[4])
{
  drw_shgroup_call_procedural_add_ex(shgroup, GPU_PRIM_TRIS, tria_count * 3, obmat, NULL);
}

/* These calls can be culled and are optimized for redraw */
void DRW_shgroup_call_object_add_ex(
    DRWShadingGroup *shgroup, GPUBatch *geom, Object *ob, Material *ma, bool bypass_culling)
{
  BLI_assert(geom != NULL);
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  call->state = drw_call_state_object(shgroup, ob->obmat, ob);
  call->type = DRW_CALL_SINGLE;
  call->single.geometry = geom;
  call->single.ma_index = ma ? ma->index : 0;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  /* NOTE this will disable culling for the whole object. */
  call->state->flag |= (bypass_culling) ? DRW_CALL_BYPASS_CULLING : 0;

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

void DRW_shgroup_call_object_add_with_callback(DRWShadingGroup *shgroup,
                                               GPUBatch *geom,
                                               Object *ob,
                                               Material *ma,
                                               DRWCallVisibilityFn *callback,
                                               void *user_data)
{
  BLI_assert(geom != NULL);
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  call->state = drw_call_state_object(shgroup, ob->obmat, ob);
  call->state->visibility_cb = callback;
  call->state->user_data = user_data;
  call->type = DRW_CALL_SINGLE;
  call->single.geometry = geom;
  call->single.ma_index = ma ? ma->index : 0;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

void DRW_shgroup_call_instances_add(DRWShadingGroup *shgroup,
                                    GPUBatch *geom,
                                    float (*obmat)[4],
                                    uint *count)
{
  BLI_assert(geom != NULL);
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  call->state = drw_call_state_create(shgroup, obmat, NULL);
  call->type = DRW_CALL_INSTANCES;
  call->instances.geometry = geom;
  call->instances.count = count;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

/* These calls can be culled and are optimized for redraw */
void DRW_shgroup_call_object_instances_add(DRWShadingGroup *shgroup,
                                           GPUBatch *geom,
                                           Object *ob,
                                           uint *count)
{
  BLI_assert(geom != NULL);
  BLI_assert(ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM));

  DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);
  call->state = drw_call_state_object(shgroup, ob->obmat, ob);
  call->type = DRW_CALL_INSTANCES;
  call->instances.geometry = geom;
  call->instances.count = count;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
#endif

  BLI_LINKS_APPEND(&shgroup->calls, call);
}

// #define SCULPT_DEBUG_BUFFERS

typedef struct DRWSculptCallbackData {
  Object *ob;
  DRWShadingGroup **shading_groups;
  Material **materials;
  bool use_wire;
  bool use_mats;
  bool use_mask;
  bool fast_mode; /* Set by draw manager. Do not init. */
#ifdef SCULPT_DEBUG_BUFFERS
  int node_nr;
#endif
} DRWSculptCallbackData;

#ifdef SCULPT_DEBUG_BUFFERS
#  define SCULPT_DEBUG_COLOR(id) (sculpt_debug_colors[id % 9])
static float sculpt_debug_colors[9][4] = {
    {1.0f, 0.2f, 0.2f, 1.0f},
    {0.2f, 1.0f, 0.2f, 1.0f},
    {0.2f, 0.2f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.2f, 1.0f},
    {0.2f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.2f, 1.0f, 1.0f},
    {1.0f, 0.7f, 0.2f, 1.0f},
    {0.2f, 1.0f, 0.7f, 1.0f},
    {0.7f, 0.2f, 1.0f, 1.0f},
};
#endif

static void sculpt_draw_cb(DRWSculptCallbackData *scd, GPU_PBVH_Buffers *buffers)
{
  GPUBatch *geom = GPU_pbvh_buffers_batch_get(buffers, scd->fast_mode, scd->use_wire);
  Material *ma = NULL;
  short index = 0;

  /* Meh... use_mask is a bit misleading here. */
  if (scd->use_mask && !GPU_pbvh_buffers_has_mask(buffers)) {
    return;
  }

  if (scd->use_mats) {
    index = GPU_pbvh_buffers_material_index_get(buffers);
    ma = scd->materials[index];
  }

  DRWShadingGroup *shgrp = scd->shading_groups[index];
  if (geom != NULL && shgrp != NULL) {
#ifdef SCULPT_DEBUG_BUFFERS
    /* Color each buffers in different colors. Only work in solid/Xray mode. */
    shgrp = DRW_shgroup_create_sub(shgrp);
    DRW_shgroup_uniform_vec3(shgrp, "materialDiffuseColor", SCULPT_DEBUG_COLOR(scd->node_nr++), 1);
#endif
    /* DRW_shgroup_call_object_add_ex reuses matrices calculations for all the drawcalls of this
     * object. */
    DRW_shgroup_call_object_add_ex(shgrp, geom, scd->ob, ma, true);
  }
}

#ifdef SCULPT_DEBUG_BUFFERS
static void sculpt_debug_cb(void *user_data,
                            const float bmin[3],
                            const float bmax[3],
                            PBVHNodeFlags flag)
{
  int *node_nr = (int *)user_data;
  BoundBox bb;
  BKE_boundbox_init_from_minmax(&bb, bmin, bmax);

#  if 0 /* Nodes hierarchy. */
  if (flag & PBVH_Leaf) {
    DRW_debug_bbox(&bb, (float[4]){0.0f, 1.0f, 0.0f, 1.0f});
  }
  else {
    DRW_debug_bbox(&bb, (float[4]){0.5f, 0.5f, 0.5f, 0.6f});
  }
#  else /* Color coded leaf bounds. */
  if (flag & PBVH_Leaf) {
    DRW_debug_bbox(&bb, SCULPT_DEBUG_COLOR((*node_nr)++));
  }
#  endif
}
#endif

static void drw_sculpt_generate_calls(DRWSculptCallbackData *scd, bool use_vcol)
{
  /* XXX should be ensured before but sometime it's not... go figure (see T57040). */
  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(DST.draw_ctx.depsgraph, scd->ob);
  if (!pbvh) {
    return;
  }

  float(*planes)[4] = NULL; /* TODO proper culling. */
  scd->fast_mode = false;

  const DRWContextState *drwctx = DRW_context_state_get();
  if (drwctx->evil_C != NULL) {
    Paint *p = BKE_paint_get_active_from_context(drwctx->evil_C);
    if (p && (p->flags & PAINT_FAST_NAVIGATE)) {
      scd->fast_mode = (drwctx->rv3d->rflag & RV3D_NAVIGATING) != 0;
    }
  }

  BKE_pbvh_draw_cb(
      pbvh, planes, NULL, use_vcol, (void (*)(void *, GPU_PBVH_Buffers *))sculpt_draw_cb, scd);

#ifdef SCULPT_DEBUG_BUFFERS
  int node_nr = 0;
  DRW_debug_modelmat(scd->ob->obmat);
  BKE_pbvh_draw_debug_cb(
      pbvh,
      (void (*)(void *d, const float min[3], const float max[3], PBVHNodeFlags f))sculpt_debug_cb,
      &node_nr);
#endif
}

void DRW_shgroup_call_sculpt_add(
    DRWShadingGroup *shgroup, Object *ob, bool use_wire, bool use_mask, bool use_vcol)
{
  DRWSculptCallbackData scd = {
      .ob = ob,
      .shading_groups = &shgroup,
      .materials = NULL,
      .use_wire = use_wire,
      .use_mats = false,
      .use_mask = use_mask,
  };
  drw_sculpt_generate_calls(&scd, use_vcol);
}

void DRW_shgroup_call_sculpt_with_materials_add(DRWShadingGroup **shgroups,
                                                Material **materials,
                                                Object *ob,
                                                bool use_vcol)
{
  DRWSculptCallbackData scd = {
      .ob = ob,
      .shading_groups = shgroups,
      .materials = materials,
      .use_wire = false,
      .use_mats = true,
      .use_mask = false,
  };
  drw_sculpt_generate_calls(&scd, use_vcol);
}

void DRW_shgroup_call_dynamic_add_array(DRWShadingGroup *shgroup,
                                        const void *attr[],
                                        uint attr_len)
{
#ifdef USE_GPU_SELECT
  if (G.f & G_FLAG_PICKSEL) {
    if (shgroup->instance_count == shgroup->inst_selectid->vertex_len) {
      GPU_vertbuf_data_resize(shgroup->inst_selectid, shgroup->instance_count + 32);
    }
    GPU_vertbuf_attr_set(shgroup->inst_selectid, 0, shgroup->instance_count, &DST.select_id);
  }
#endif

  BLI_assert(attr_len == shgroup->attrs_count);
  UNUSED_VARS_NDEBUG(attr_len);

  for (int i = 0; i < attr_len; ++i) {
    if (shgroup->instance_count == shgroup->instance_vbo->vertex_len) {
      GPU_vertbuf_data_resize(shgroup->instance_vbo, shgroup->instance_count + 32);
    }
    GPU_vertbuf_attr_set(shgroup->instance_vbo, i, shgroup->instance_count, attr[i]);
  }

  shgroup->instance_count += 1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shading Groups (DRW_shgroup)
 * \{ */

static void drw_shgroup_init(DRWShadingGroup *shgroup, GPUShader *shader)
{
  shgroup->instance_geom = NULL;
  shgroup->instance_vbo = NULL;
  shgroup->instance_count = 0;
  shgroup->uniforms = NULL;
#ifdef USE_GPU_SELECT
  shgroup->inst_selectid = NULL;
  shgroup->override_selectid = -1;
#endif
#ifndef NDEBUG
  shgroup->attrs_count = 0;
#endif

  int view_ubo_location = GPU_shader_get_uniform_block(shader, "viewBlock");

  if (view_ubo_location != -1) {
    drw_shgroup_uniform_create_ex(
        shgroup, view_ubo_location, DRW_UNIFORM_BLOCK_PERSIST, G_draw.view_ubo, 0, 1);
  }
  else {
    /* Only here to support builtin shaders. This should not be used by engines. */
    drw_shgroup_builtin_uniform(
        shgroup, GPU_UNIFORM_VIEW, DST.view_data.matstate.mat[DRW_MAT_VIEW], 16, 1);
    drw_shgroup_builtin_uniform(
        shgroup, GPU_UNIFORM_VIEW_INV, DST.view_data.matstate.mat[DRW_MAT_VIEWINV], 16, 1);
    drw_shgroup_builtin_uniform(
        shgroup, GPU_UNIFORM_VIEWPROJECTION, DST.view_data.matstate.mat[DRW_MAT_PERS], 16, 1);
    drw_shgroup_builtin_uniform(shgroup,
                                GPU_UNIFORM_VIEWPROJECTION_INV,
                                DST.view_data.matstate.mat[DRW_MAT_PERSINV],
                                16,
                                1);
    drw_shgroup_builtin_uniform(
        shgroup, GPU_UNIFORM_PROJECTION, DST.view_data.matstate.mat[DRW_MAT_WIN], 16, 1);
    drw_shgroup_builtin_uniform(
        shgroup, GPU_UNIFORM_PROJECTION_INV, DST.view_data.matstate.mat[DRW_MAT_WININV], 16, 1);
    drw_shgroup_builtin_uniform(
        shgroup, GPU_UNIFORM_CAMERATEXCO, DST.view_data.viewcamtexcofac, 3, 2);
  }

  shgroup->model = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODEL);
  shgroup->modelinverse = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODEL_INV);
  shgroup->modelview = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW);
  shgroup->modelviewinverse = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW_INV);
  shgroup->modelviewprojection = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MVP);
  shgroup->normalview = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_NORMAL);
  shgroup->normalviewinverse = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_NORMAL_INV);
  shgroup->normalworld = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_WORLDNORMAL);
  shgroup->orcotexfac = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_ORCO);
  shgroup->objectinfo = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_OBJECT_INFO);
  shgroup->eye = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_EYE);
  shgroup->callid = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_CALLID);

  shgroup->matflag = 0;
  if (shgroup->modelinverse > -1) {
    shgroup->matflag |= DRW_CALL_MODELINVERSE;
  }
  if (shgroup->modelview > -1) {
    shgroup->matflag |= DRW_CALL_MODELVIEW;
  }
  if (shgroup->modelviewinverse > -1) {
    shgroup->matflag |= DRW_CALL_MODELVIEWINVERSE;
  }
  if (shgroup->modelviewprojection > -1) {
    shgroup->matflag |= DRW_CALL_MODELVIEWPROJECTION;
  }
  if (shgroup->normalview > -1) {
    shgroup->matflag |= DRW_CALL_NORMALVIEW;
  }
  if (shgroup->normalviewinverse > -1) {
    shgroup->matflag |= DRW_CALL_NORMALVIEWINVERSE;
  }
  if (shgroup->normalworld > -1) {
    shgroup->matflag |= DRW_CALL_NORMALWORLD;
  }
  if (shgroup->orcotexfac > -1) {
    shgroup->matflag |= DRW_CALL_ORCOTEXFAC;
  }
  if (shgroup->objectinfo > -1) {
    shgroup->matflag |= DRW_CALL_OBJECTINFO;
  }
  if (shgroup->eye > -1) {
    shgroup->matflag |= DRW_CALL_EYEVEC;
  }
}

static void drw_shgroup_instance_init(DRWShadingGroup *shgroup,
                                      GPUShader *shader,
                                      GPUBatch *batch,
                                      GPUVertFormat *format)
{
  BLI_assert(shgroup->type == DRW_SHG_INSTANCE);
  BLI_assert(batch != NULL);
  BLI_assert(format != NULL);

  drw_shgroup_init(shgroup, shader);

  shgroup->instance_geom = batch;
#ifndef NDEBUG
  shgroup->attrs_count = format->attr_len;
#endif

  DRW_instancing_buffer_request(
      DST.idatalist, format, batch, shgroup, &shgroup->instance_geom, &shgroup->instance_vbo);

#ifdef USE_GPU_SELECT
  if (G.f & G_FLAG_PICKSEL) {
    /* Not actually used for rendering but alloced in one chunk.
     * Plus we don't have to care about ownership. */
    static GPUVertFormat inst_select_format = {0};
    if (inst_select_format.attr_len == 0) {
      GPU_vertformat_attr_add(&inst_select_format, "selectId", GPU_COMP_I32, 1, GPU_FETCH_INT);
    }
    GPUBatch *batch_dummy; /* Not used */
    DRW_batching_buffer_request(DST.idatalist,
                                &inst_select_format,
                                GPU_PRIM_POINTS,
                                shgroup,
                                &batch_dummy,
                                &shgroup->inst_selectid);
  }
#endif
}

static void drw_shgroup_batching_init(DRWShadingGroup *shgroup,
                                      GPUShader *shader,
                                      GPUVertFormat *format)
{
  drw_shgroup_init(shgroup, shader);

#ifndef NDEBUG
  shgroup->attrs_count = (format != NULL) ? format->attr_len : 0;
#endif
  BLI_assert(format != NULL);

  GPUPrimType type;
  switch (shgroup->type) {
    case DRW_SHG_POINT_BATCH:
      type = GPU_PRIM_POINTS;
      break;
    case DRW_SHG_LINE_BATCH:
      type = GPU_PRIM_LINES;
      break;
    case DRW_SHG_TRIANGLE_BATCH:
      type = GPU_PRIM_TRIS;
      break;
    default:
      type = GPU_PRIM_NONE;
      BLI_assert(0);
      break;
  }

  DRW_batching_buffer_request(
      DST.idatalist, format, type, shgroup, &shgroup->batch_geom, &shgroup->batch_vbo);

#ifdef USE_GPU_SELECT
  if (G.f & G_FLAG_PICKSEL) {
    /* Not actually used for rendering but alloced in one chunk. */
    static GPUVertFormat inst_select_format = {0};
    if (inst_select_format.attr_len == 0) {
      GPU_vertformat_attr_add(&inst_select_format, "selectId", GPU_COMP_I32, 1, GPU_FETCH_INT);
    }
    GPUBatch *batch; /* Not used */
    DRW_batching_buffer_request(DST.idatalist,
                                &inst_select_format,
                                GPU_PRIM_POINTS,
                                shgroup,
                                &batch,
                                &shgroup->inst_selectid);
  }
#endif
}

static DRWShadingGroup *drw_shgroup_create_ex(struct GPUShader *shader, DRWPass *pass)
{
  DRWShadingGroup *shgroup = BLI_mempool_alloc(DST.vmempool->shgroups);

  BLI_LINKS_APPEND(&pass->shgroups, shgroup);

  shgroup->type = DRW_SHG_NORMAL;
  shgroup->shader = shader;
  shgroup->state_extra = 0;
  shgroup->state_extra_disable = ~0x0;
  shgroup->stencil_mask = 0;
  shgroup->calls.first = NULL;
  shgroup->calls.last = NULL;
#if 0 /* All the same in the union! */
  shgroup->batch_geom = NULL;
  shgroup->batch_vbo = NULL;

  shgroup->instance_geom = NULL;
  shgroup->instance_vbo = NULL;
#endif
  shgroup->pass_parent = pass;

  return shgroup;
}

static DRWShadingGroup *drw_shgroup_material_create_ex(GPUPass *gpupass, DRWPass *pass)
{
  if (!gpupass) {
    /* Shader compilation error */
    return NULL;
  }

  GPUShader *sh = GPU_pass_shader_get(gpupass);

  if (!sh) {
    /* Shader not yet compiled */
    return NULL;
  }

  DRWShadingGroup *grp = drw_shgroup_create_ex(sh, pass);
  return grp;
}

static DRWShadingGroup *drw_shgroup_material_inputs(DRWShadingGroup *grp,
                                                    struct GPUMaterial *material)
{
  ListBase *inputs = GPU_material_get_inputs(material);

  /* Converting dynamic GPUInput to DRWUniform */
  for (GPUInput *input = inputs->first; input; input = input->next) {
    /* Textures */
    if (input->source == GPU_SOURCE_TEX) {
      GPUTexture *tex = NULL;

      if (input->ima) {
        GPUTexture **tex_ref = BLI_mempool_alloc(DST.vmempool->images);

        *tex_ref = tex = GPU_texture_from_blender(
            input->ima, input->iuser, GL_TEXTURE_2D, input->image_isdata);

        GPU_texture_ref(tex);
      }
      else {
        /* Color Ramps */
        tex = *input->coba;
      }

      if (input->bindtex) {
        drw_shgroup_uniform_create_ex(grp, input->shaderloc, DRW_UNIFORM_TEXTURE, tex, 0, 1);
      }
    }
  }

  GPUUniformBuffer *ubo = GPU_material_uniform_buffer_get(material);
  if (ubo != NULL) {
    DRW_shgroup_uniform_block(grp, GPU_UBO_BLOCK_NAME, ubo);
  }

  return grp;
}

GPUVertFormat *DRW_shgroup_instance_format_array(const DRWInstanceAttrFormat attrs[],
                                                 int arraysize)
{
  GPUVertFormat *format = MEM_callocN(sizeof(GPUVertFormat), "GPUVertFormat");

  for (int i = 0; i < arraysize; ++i) {
    GPU_vertformat_attr_add(format,
                            attrs[i].name,
                            (attrs[i].type == DRW_ATTR_INT) ? GPU_COMP_I32 : GPU_COMP_F32,
                            attrs[i].components,
                            (attrs[i].type == DRW_ATTR_INT) ? GPU_FETCH_INT : GPU_FETCH_FLOAT);
  }
  return format;
}

DRWShadingGroup *DRW_shgroup_material_create(struct GPUMaterial *material, DRWPass *pass)
{
  GPUPass *gpupass = GPU_material_get_pass(material);
  DRWShadingGroup *shgroup = drw_shgroup_material_create_ex(gpupass, pass);

  if (shgroup) {
    drw_shgroup_init(shgroup, GPU_pass_shader_get(gpupass));
    drw_shgroup_material_inputs(shgroup, material);
  }

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_material_instance_create(
    struct GPUMaterial *material, DRWPass *pass, GPUBatch *geom, Object *ob, GPUVertFormat *format)
{
  GPUPass *gpupass = GPU_material_get_pass(material);
  DRWShadingGroup *shgroup = drw_shgroup_material_create_ex(gpupass, pass);

  if (shgroup) {
    shgroup->type = DRW_SHG_INSTANCE;
    shgroup->instance_geom = geom;
    drw_call_calc_orco(ob, shgroup->instance_orcofac);
    drw_shgroup_instance_init(shgroup, GPU_pass_shader_get(gpupass), geom, format);
    drw_shgroup_material_inputs(shgroup, material);
  }

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_material_empty_tri_batch_create(struct GPUMaterial *material,
                                                             DRWPass *pass,
                                                             int tri_count)
{
#ifdef USE_GPU_SELECT
  BLI_assert((G.f & G_FLAG_PICKSEL) == 0);
#endif
  GPUPass *gpupass = GPU_material_get_pass(material);
  DRWShadingGroup *shgroup = drw_shgroup_material_create_ex(gpupass, pass);

  if (shgroup) {
    /* Calling drw_shgroup_init will cause it to call GPU_draw_primitive(). */
    drw_shgroup_init(shgroup, GPU_pass_shader_get(gpupass));
    shgroup->type = DRW_SHG_TRIANGLE_BATCH;
    shgroup->instance_count = tri_count * 3;
    drw_shgroup_material_inputs(shgroup, material);
  }

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass)
{
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  drw_shgroup_init(shgroup, shader);
  return shgroup;
}

DRWShadingGroup *DRW_shgroup_instance_create(struct GPUShader *shader,
                                             DRWPass *pass,
                                             GPUBatch *geom,
                                             GPUVertFormat *format)
{
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  shgroup->type = DRW_SHG_INSTANCE;
  shgroup->instance_geom = geom;
  drw_call_calc_orco(NULL, shgroup->instance_orcofac);
  drw_shgroup_instance_init(shgroup, shader, geom, format);

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_point_batch_create(struct GPUShader *shader, DRWPass *pass)
{
  DRW_shgroup_instance_format(g_pos_format, {{"pos", DRW_ATTR_FLOAT, 3}});

  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  shgroup->type = DRW_SHG_POINT_BATCH;

  drw_shgroup_batching_init(shgroup, shader, g_pos_format);

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_line_batch_create_with_format(struct GPUShader *shader,
                                                           DRWPass *pass,
                                                           GPUVertFormat *format)
{
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  shgroup->type = DRW_SHG_LINE_BATCH;

  drw_shgroup_batching_init(shgroup, shader, format);

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_line_batch_create(struct GPUShader *shader, DRWPass *pass)
{
  DRW_shgroup_instance_format(g_pos_format, {{"pos", DRW_ATTR_FLOAT, 3}});

  return DRW_shgroup_line_batch_create_with_format(shader, pass, g_pos_format);
}

/**
 * Very special batch. Use this if you position
 * your vertices with the vertex shader
 * and dont need any VBO attribute.
 */
DRWShadingGroup *DRW_shgroup_empty_tri_batch_create(struct GPUShader *shader,
                                                    DRWPass *pass,
                                                    int tri_count)
{
#ifdef USE_GPU_SELECT
  BLI_assert((G.f & G_FLAG_PICKSEL) == 0);
#endif
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);

  /* Calling drw_shgroup_init will cause it to call GPU_draw_primitive(). */
  drw_shgroup_init(shgroup, shader);

  shgroup->type = DRW_SHG_TRIANGLE_BATCH;
  shgroup->instance_count = tri_count * 3;

  return shgroup;
}

DRWShadingGroup *DRW_shgroup_transform_feedback_create(struct GPUShader *shader,
                                                       DRWPass *pass,
                                                       GPUVertBuf *tf_target)
{
  BLI_assert(tf_target != NULL);
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  shgroup->type = DRW_SHG_FEEDBACK_TRANSFORM;

  drw_shgroup_init(shgroup, shader);

  shgroup->tfeedback_target = tf_target;

  return shgroup;
}

/**
 * Specify an external batch instead of adding each attribute one by one.
 */
void DRW_shgroup_instance_batch(DRWShadingGroup *shgroup, struct GPUBatch *batch)
{
  BLI_assert(shgroup->type == DRW_SHG_INSTANCE);
  BLI_assert(shgroup->instance_count == 0);
  /* You cannot use external instancing batch without a dummy format. */
  BLI_assert(shgroup->attrs_count != 0);

  shgroup->type = DRW_SHG_INSTANCE_EXTERNAL;
  drw_call_calc_orco(NULL, shgroup->instance_orcofac);
  /* PERF : This destroys the vaos cache so better check if it's necessary. */
  /* Note: This WILL break if batch->verts[0] is destroyed and reallocated
   * at the same address. Bindings/VAOs would remain obsolete. */
  // if (shgroup->instancing_geom->inst != batch->verts[0])
  GPU_batch_instbuf_set(shgroup->instance_geom, batch->verts[0], false);

#ifdef USE_GPU_SELECT
  shgroup->override_selectid = DST.select_id;
#endif
}

uint DRW_shgroup_get_instance_count(const DRWShadingGroup *shgroup)
{
  return shgroup->instance_count;
}

/**
 * State is added to #Pass.state while drawing.
 * Use to temporarily enable draw options.
 */
void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state)
{
  shgroup->state_extra |= state;
}

void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state)
{
  shgroup->state_extra_disable &= ~state;
}

void DRW_shgroup_stencil_mask(DRWShadingGroup *shgroup, uint mask)
{
  BLI_assert(mask <= 255);
  shgroup->stencil_mask = mask;
}

bool DRW_shgroup_is_empty(DRWShadingGroup *shgroup)
{
  switch (shgroup->type) {
    case DRW_SHG_NORMAL:
    case DRW_SHG_FEEDBACK_TRANSFORM:
      return shgroup->calls.first == NULL;
    case DRW_SHG_POINT_BATCH:
    case DRW_SHG_LINE_BATCH:
    case DRW_SHG_TRIANGLE_BATCH:
    case DRW_SHG_INSTANCE:
    case DRW_SHG_INSTANCE_EXTERNAL:
      return shgroup->instance_count == 0;
  }
  BLI_assert(!"Shading Group type not supported");
  return true;
}

DRWShadingGroup *DRW_shgroup_create_sub(DRWShadingGroup *shgroup)
{
  /* Remove this assertion if needed but implement the other cases first! */
  BLI_assert(shgroup->type == DRW_SHG_NORMAL);

  DRWShadingGroup *shgroup_new = BLI_mempool_alloc(DST.vmempool->shgroups);

  *shgroup_new = *shgroup;
  shgroup_new->uniforms = NULL;
  shgroup_new->calls.first = NULL;
  shgroup_new->calls.last = NULL;

  BLI_LINKS_INSERT_AFTER(&shgroup->pass_parent->shgroups, shgroup, shgroup_new);

  return shgroup_new;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Passes (DRW_pass)
 * \{ */

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
  DRWPass *pass = BLI_mempool_alloc(DST.vmempool->passes);
  pass->state = state;
  if (((G.debug_value > 20) && (G.debug_value < 30)) || (G.debug & G_DEBUG)) {
    BLI_strncpy(pass->name, name, MAX_PASS_NAME);
  }

  pass->shgroups.first = NULL;
  pass->shgroups.last = NULL;

  return pass;
}

bool DRW_pass_is_empty(DRWPass *pass)
{
  for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
    if (!DRW_shgroup_is_empty(shgroup)) {
      return false;
    }
  }
  return true;
}

void DRW_pass_state_set(DRWPass *pass, DRWState state)
{
  pass->state = state;
}

void DRW_pass_state_add(DRWPass *pass, DRWState state)
{
  pass->state |= state;
}

void DRW_pass_state_remove(DRWPass *pass, DRWState state)
{
  pass->state &= ~state;
}

void DRW_pass_free(DRWPass *pass)
{
  pass->shgroups.first = NULL;
  pass->shgroups.last = NULL;
}

void DRW_pass_foreach_shgroup(DRWPass *pass,
                              void (*callback)(void *userData, DRWShadingGroup *shgrp),
                              void *userData)
{
  for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
    callback(userData, shgroup);
  }
}

typedef struct ZSortData {
  float *axis;
  float *origin;
} ZSortData;

static int pass_shgroup_dist_sort(void *thunk, const void *a, const void *b)
{
  const ZSortData *zsortdata = (ZSortData *)thunk;
  const DRWShadingGroup *shgrp_a = (const DRWShadingGroup *)a;
  const DRWShadingGroup *shgrp_b = (const DRWShadingGroup *)b;

  const DRWCall *call_a = (DRWCall *)shgrp_a->calls.first;
  const DRWCall *call_b = (DRWCall *)shgrp_b->calls.first;

  if (call_a == NULL) {
    return -1;
  }
  if (call_b == NULL) {
    return -1;
  }

  float tmp[3];
  sub_v3_v3v3(tmp, zsortdata->origin, call_a->state->model[3]);
  const float a_sq = dot_v3v3(zsortdata->axis, tmp);
  sub_v3_v3v3(tmp, zsortdata->origin, call_b->state->model[3]);
  const float b_sq = dot_v3v3(zsortdata->axis, tmp);

  if (a_sq < b_sq) {
    return 1;
  }
  else if (a_sq > b_sq) {
    return -1;
  }
  else {
    /* If there is a depth prepass put it before */
    if ((shgrp_a->state_extra & DRW_STATE_WRITE_DEPTH) != 0) {
      return -1;
    }
    else if ((shgrp_b->state_extra & DRW_STATE_WRITE_DEPTH) != 0) {
      return 1;
    }
    else {
      return 0;
    }
  }
}

/* ------------------ Shading group sorting --------------------- */

#define SORT_IMPL_LINKTYPE DRWShadingGroup

#define SORT_IMPL_USE_THUNK
#define SORT_IMPL_FUNC shgroup_sort_fn_r
#include "../../blenlib/intern/list_sort_impl.h"
#undef SORT_IMPL_FUNC
#undef SORT_IMPL_USE_THUNK

#undef SORT_IMPL_LINKTYPE

/**
 * Sort Shading groups by decreasing Z of their first draw call.
 * This is useful for order dependent effect such as transparency.
 */
void DRW_pass_sort_shgroup_z(DRWPass *pass)
{
  float(*viewinv)[4];
  viewinv = DST.view_data.matstate.mat[DRW_MAT_VIEWINV];

  ZSortData zsortdata = {viewinv[2], viewinv[3]};

  if (pass->shgroups.first && pass->shgroups.first->next) {
    pass->shgroups.first = shgroup_sort_fn_r(
        pass->shgroups.first, pass_shgroup_dist_sort, &zsortdata);

    /* Find the next last */
    DRWShadingGroup *last = pass->shgroups.first;
    while ((last = last->next)) {
      /* Do nothing */
    }
    pass->shgroups.last = last;
  }
}

/** \} */
