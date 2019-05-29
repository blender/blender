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
#include "BLI_memblock.h"

#ifdef DRW_DEBUG_CULLING
#  include "BLI_math_bits.h"
#endif

#include "GPU_buffers.h"

#include "intern/gpu_codegen.h"

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
  DRWUniform *uni = BLI_memblock_alloc(DST.vmempool->uniforms);
  uni->location = loc;
  uni->type = type;
  uni->length = length;
  uni->arraysize = arraysize;

  switch (type) {
    case DRW_UNIFORM_INT_COPY:
      BLI_assert(length <= 2);
      memcpy(uni->ivalue, value, sizeof(int) * length);
      break;
    case DRW_UNIFORM_FLOAT_COPY:
      BLI_assert(length <= 2);
      memcpy(uni->fvalue, value, sizeof(float) * length);
      break;
    default:
      uni->pvalue = (const float *)value;
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
      DST.uniform_names.buffer_len += MAX2(DST.uniform_names.buffer_len, len);
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
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT, value, 1, arraysize);
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
  int ival = value;
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT_COPY, &ival, 1, 1);
}

void DRW_shgroup_uniform_float_copy(DRWShadingGroup *shgroup, const char *name, const float value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT_COPY, &value, 1, 1);
}

void DRW_shgroup_uniform_vec2_copy(DRWShadingGroup *shgroup, const char *name, const float *value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT_COPY, value, 2, 1);
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
  uchar new_flags = ((state->matflag ^ shgroup->matflag) & shgroup->matflag);

  /* HACK: Here we set the matflags bit to 1 when computing the value
   * so that it's not recomputed for other drawcalls.
   * This is the opposite of what draw_matrices_model_prepare() does. */
  state->matflag |= shgroup->matflag;

  if (new_flags & DRW_CALL_MODELINVERSE) {
    if (ob) {
      copy_m4_m4(state->modelinverse, ob->imat);
    }
    else {
      invert_m4_m4(state->modelinverse, state->model);
    }
  }

  /* Orco factors: We compute this at creation to not have to save the *ob_data */
  if (new_flags & DRW_CALL_ORCOTEXFAC) {
    drw_call_calc_orco(ob, state->orcotexfac);
  }

  if (new_flags & DRW_CALL_OBJECTINFO) {
    state->ob_index = ob ? ob->index : 0;
    uint random;
    if (DST.dupli_source) {
      random = DST.dupli_source->random_id;
    }
    else {
      random = BLI_hash_int_2d(BLI_hash_string(ob->id.name + 2), 0);
    }
    state->ob_random = random * (1.0f / (float)0xFFFFFFFF);
  }
}

static DRWCallState *drw_call_state_create(DRWShadingGroup *shgroup, float (*obmat)[4], Object *ob)
{
  DRWCallState *state = BLI_memblock_alloc(DST.vmempool->states);
  state->flag = 0;
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

  drw_call_state_update_matflag(state, shgroup, ob);

  DRWCullingState *cull = BLI_memblock_alloc(DST.vmempool->cullstates);
  state->culling = cull;

  if (ob != NULL) {
    float corner[3];
    BoundBox *bbox = BKE_object_boundbox_get(ob);
    /* Get BoundSphere center and radius from the BoundBox. */
    mid_v3_v3v3(cull->bsphere.center, bbox->vec[0], bbox->vec[6]);
    mul_v3_m4v3(corner, obmat, bbox->vec[0]);
    mul_m4_v3(obmat, cull->bsphere.center);
    cull->bsphere.radius = len_v3v3(cull->bsphere.center, corner);
  }
  else {
    /* TODO(fclem) Bypass alloc if we can (see if eevee's
     * probe visibility collection still works). */
    /* Bypass test. */
    cull->bsphere.radius = -1.0f;
  }

  return state;
}

static DRWCallState *drw_call_state_object(DRWShadingGroup *shgroup, float (*obmat)[4], Object *ob)
{
  if (ob == NULL) {
    if (obmat == NULL) {
      /* TODO return unitmat state. */
      return drw_call_state_create(shgroup, obmat, ob);
    }
    else {
      return drw_call_state_create(shgroup, obmat, ob);
    }
  }
  else {
    if (DST.ob_state == NULL) {
      DST.ob_state = drw_call_state_create(shgroup, obmat, ob);
    }
    else {
      /* If the DRWCallState is reused, add necessary matrices. */
      drw_call_state_update_matflag(DST.ob_state, shgroup, ob);
    }

    return DST.ob_state;
  }
}

void DRW_shgroup_call_ex(DRWShadingGroup *shgroup,
                         Object *ob,
                         float (*obmat)[4],
                         struct GPUBatch *geom,
                         uint v_sta,
                         uint v_ct,
                         bool bypass_culling,
                         void *user_data)
{
  BLI_assert(geom != NULL);

  DRWCall *call = BLI_memblock_alloc(DST.vmempool->calls);
  BLI_LINKS_APPEND(&shgroup->calls, call);

  call->state = drw_call_state_object(shgroup, ob ? ob->obmat : obmat, ob);
  call->batch = geom;
  call->vert_first = v_sta;
  call->vert_count = v_ct; /* 0 means auto from batch. */
  call->inst_count = 0;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
  call->inst_selectid = NULL;
#endif
  if (call->state->culling) {
    call->state->culling->user_data = user_data;
    if (bypass_culling) {
      /* NOTE this will disable culling for the whole object. */
      call->state->culling->bsphere.radius = -1.0f;
    }
  }
}

static void drw_shgroup_call_procedural_add_ex(DRWShadingGroup *shgroup,
                                               GPUBatch *geom,
                                               Object *ob,
                                               uint vert_count)
{

  DRWCall *call = BLI_memblock_alloc(DST.vmempool->calls);
  BLI_LINKS_APPEND(&shgroup->calls, call);

  call->state = drw_call_state_object(shgroup, ob ? ob->obmat : NULL, ob);
  call->batch = geom;
  call->vert_first = 0;
  call->vert_count = vert_count;
  call->inst_count = 0;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
  call->inst_selectid = NULL;
#endif
}

void DRW_shgroup_call_procedural_points(DRWShadingGroup *shgroup, Object *ob, uint point_len)
{
  struct GPUBatch *geom = drw_cache_procedural_points_get();
  drw_shgroup_call_procedural_add_ex(shgroup, geom, ob, point_len);
}

void DRW_shgroup_call_procedural_lines(DRWShadingGroup *shgroup, Object *ob, uint line_count)
{
  struct GPUBatch *geom = drw_cache_procedural_lines_get();
  drw_shgroup_call_procedural_add_ex(shgroup, geom, ob, line_count * 2);
}

void DRW_shgroup_call_procedural_triangles(DRWShadingGroup *shgroup, Object *ob, uint tria_count)
{
  struct GPUBatch *geom = drw_cache_procedural_triangles_get();
  drw_shgroup_call_procedural_add_ex(shgroup, geom, ob, tria_count * 3);
}

void DRW_shgroup_call_instances(DRWShadingGroup *shgroup,
                                Object *ob,
                                struct GPUBatch *geom,
                                uint count)
{
  BLI_assert(geom != NULL);

  DRWCall *call = BLI_memblock_alloc(DST.vmempool->calls);
  BLI_LINKS_APPEND(&shgroup->calls, call);

  call->state = drw_call_state_object(shgroup, ob ? ob->obmat : NULL, ob);
  call->batch = geom;
  call->vert_first = 0;
  call->vert_count = 0; /* Auto from batch. */
  call->inst_count = count;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
  call->inst_selectid = NULL;
#endif
}

void DRW_shgroup_call_instances_with_attribs(DRWShadingGroup *shgroup,
                                             Object *ob,
                                             struct GPUBatch *geom,
                                             struct GPUBatch *inst_attributes)
{
  BLI_assert(geom != NULL);
  BLI_assert(inst_attributes->verts[0] != NULL);

  GPUVertBuf *buf_inst = inst_attributes->verts[0];

  DRWCall *call = BLI_memblock_alloc(DST.vmempool->calls);
  BLI_LINKS_APPEND(&shgroup->calls, call);

  call->state = drw_call_state_object(shgroup, ob ? ob->obmat : NULL, ob);
  call->batch = DRW_temp_batch_instance_request(DST.idatalist, buf_inst, geom);
  call->vert_first = 0;
  call->vert_count = 0; /* Auto from batch. */
  call->inst_count = buf_inst->vertex_len;
#ifdef USE_GPU_SELECT
  call->select_id = DST.select_id;
  call->inst_selectid = NULL;
#endif
}

// #define SCULPT_DEBUG_BUFFERS

typedef struct DRWSculptCallbackData {
  Object *ob;
  DRWShadingGroup **shading_groups;
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
  short index = 0;

  /* Meh... use_mask is a bit misleading here. */
  if (scd->use_mask && !GPU_pbvh_buffers_has_mask(buffers)) {
    return;
  }

  if (scd->use_mats) {
    index = GPU_pbvh_buffers_material_index_get(buffers);
  }

  DRWShadingGroup *shgrp = scd->shading_groups[index];
  if (geom != NULL && shgrp != NULL) {
#ifdef SCULPT_DEBUG_BUFFERS
    /* Color each buffers in different colors. Only work in solid/Xray mode. */
    shgrp = DRW_shgroup_create_sub(shgrp);
    DRW_shgroup_uniform_vec3(shgrp, "materialDiffuseColor", SCULPT_DEBUG_COLOR(scd->node_nr++), 1);
#endif
    /* DRW_shgroup_call_no_cull reuses matrices calculations for all the drawcalls of this
     * object. */
    DRW_shgroup_call_no_cull(shgrp, geom, scd->ob);
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

void DRW_shgroup_call_sculpt(
    DRWShadingGroup *shgroup, Object *ob, bool use_wire, bool use_mask, bool use_vcol)
{
  DRWSculptCallbackData scd = {
      .ob = ob,
      .shading_groups = &shgroup,
      .use_wire = use_wire,
      .use_mats = false,
      .use_mask = use_mask,
  };
  drw_sculpt_generate_calls(&scd, use_vcol);
}

void DRW_shgroup_call_sculpt_with_materials(DRWShadingGroup **shgroups, Object *ob, bool use_vcol)
{
  DRWSculptCallbackData scd = {
      .ob = ob,
      .shading_groups = shgroups,
      .use_wire = false,
      .use_mats = true,
      .use_mask = false,
  };
  drw_sculpt_generate_calls(&scd, use_vcol);
}

static GPUVertFormat inst_select_format = {0};

DRWCallBuffer *DRW_shgroup_call_buffer(DRWShadingGroup *shgroup,
                                       struct GPUVertFormat *format,
                                       GPUPrimType prim_type)
{
  BLI_assert(ELEM(prim_type, GPU_PRIM_POINTS, GPU_PRIM_LINES, GPU_PRIM_TRI_FAN));
  BLI_assert(format != NULL);

  DRWCall *call = BLI_memblock_alloc(DST.vmempool->calls);
  BLI_LINKS_APPEND(&shgroup->calls, call);

  call->state = drw_call_state_object(shgroup, NULL, NULL);
  GPUVertBuf *buf = DRW_temp_buffer_request(DST.idatalist, format, &call->vert_count);
  call->batch = DRW_temp_batch_request(DST.idatalist, buf, prim_type);
  call->vert_first = 0;
  call->vert_count = 0;
  call->inst_count = 0;

#ifdef USE_GPU_SELECT
  if (G.f & G_FLAG_PICKSEL) {
    /* Not actually used for rendering but alloced in one chunk. */
    if (inst_select_format.attr_len == 0) {
      GPU_vertformat_attr_add(&inst_select_format, "selectId", GPU_COMP_I32, 1, GPU_FETCH_INT);
    }
    call->inst_selectid = DRW_temp_buffer_request(
        DST.idatalist, &inst_select_format, &call->vert_count);
  }
#endif
  return (DRWCallBuffer *)call;
}

DRWCallBuffer *DRW_shgroup_call_buffer_instance(DRWShadingGroup *shgroup,
                                                struct GPUVertFormat *format,
                                                GPUBatch *geom)
{
  BLI_assert(geom != NULL);
  BLI_assert(format != NULL);

  DRWCall *call = BLI_memblock_alloc(DST.vmempool->calls);
  BLI_LINKS_APPEND(&shgroup->calls, call);

  call->state = drw_call_state_object(shgroup, NULL, NULL);
  GPUVertBuf *buf = DRW_temp_buffer_request(DST.idatalist, format, &call->inst_count);
  call->batch = DRW_temp_batch_instance_request(DST.idatalist, buf, geom);
  call->vert_first = 0;
  call->vert_count = 0; /* Auto from batch. */
  call->inst_count = 0;

#ifdef USE_GPU_SELECT
  if (G.f & G_FLAG_PICKSEL) {
    /* Not actually used for rendering but alloced in one chunk. */
    if (inst_select_format.attr_len == 0) {
      GPU_vertformat_attr_add(&inst_select_format, "selectId", GPU_COMP_I32, 1, GPU_FETCH_INT);
    }
    call->inst_selectid = DRW_temp_buffer_request(
        DST.idatalist, &inst_select_format, &call->inst_count);
  }
#endif
  return (DRWCallBuffer *)call;
}

void DRW_buffer_add_entry_array(DRWCallBuffer *callbuf, const void *attr[], uint attr_len)
{
  DRWCall *call = (DRWCall *)callbuf;
  const bool is_instance = call->batch->inst != NULL;
  GPUVertBuf *buf = is_instance ? call->batch->inst : call->batch->verts[0];
  uint count = is_instance ? call->inst_count++ : call->vert_count++;
  const bool resize = (count == buf->vertex_alloc);

  BLI_assert(attr_len == buf->format.attr_len);
  UNUSED_VARS_NDEBUG(attr_len);

  if (UNLIKELY(resize)) {
    GPU_vertbuf_data_resize(buf, count + DRW_BUFFER_VERTS_CHUNK);
  }

  for (int i = 0; i < attr_len; ++i) {
    GPU_vertbuf_attr_set(buf, i, count, attr[i]);
  }

#ifdef USE_GPU_SELECT
  if (G.f & G_FLAG_PICKSEL) {
    if (UNLIKELY(resize)) {
      GPU_vertbuf_data_resize(call->inst_selectid, count + DRW_BUFFER_VERTS_CHUNK);
    }
    GPU_vertbuf_attr_set(call->inst_selectid, 0, count, &DST.select_id);
  }
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shading Groups (DRW_shgroup)
 * \{ */

static void drw_shgroup_init(DRWShadingGroup *shgroup, GPUShader *shader)
{
  shgroup->uniforms = NULL;

  int view_ubo_location = GPU_shader_get_uniform_block(shader, "viewBlock");

  if (view_ubo_location != -1) {
    drw_shgroup_uniform_create_ex(
        shgroup, view_ubo_location, DRW_UNIFORM_BLOCK_PERSIST, G_draw.view_ubo, 0, 1);
  }
  else {
    /* Only here to support builtin shaders. This should not be used by engines. */
    /* TODO remove. */
    DRWViewUboStorage *storage = &DST.view_storage_cpy;
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_VIEW, storage->viewmat, 16, 1);
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_VIEW_INV, storage->viewinv, 16, 1);
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_VIEWPROJECTION, storage->persmat, 16, 1);
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_VIEWPROJECTION_INV, storage->persinv, 16, 1);
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_PROJECTION, storage->winmat, 16, 1);
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_PROJECTION_INV, storage->wininv, 16, 1);
    drw_shgroup_builtin_uniform(shgroup, GPU_UNIFORM_CLIPPLANES, storage->clipplanes, 4, 6);
  }

  /* Not supported. */
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW_INV) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_NORMAL) == -1);

  shgroup->model = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODEL);
  shgroup->modelinverse = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODEL_INV);
  shgroup->modelviewprojection = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MVP);
  shgroup->orcotexfac = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_ORCO);
  shgroup->objectinfo = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_OBJECT_INFO);
  shgroup->callid = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_CALLID);

  shgroup->matflag = 0;
  if (shgroup->modelinverse > -1) {
    shgroup->matflag |= DRW_CALL_MODELINVERSE;
  }
  if (shgroup->modelviewprojection > -1) {
    shgroup->matflag |= DRW_CALL_MODELVIEWPROJECTION;
  }
  if (shgroup->orcotexfac > -1) {
    shgroup->matflag |= DRW_CALL_ORCOTEXFAC;
  }
  if (shgroup->objectinfo > -1) {
    shgroup->matflag |= DRW_CALL_OBJECTINFO;
  }
}

static DRWShadingGroup *drw_shgroup_create_ex(struct GPUShader *shader, DRWPass *pass)
{
  DRWShadingGroup *shgroup = BLI_memblock_alloc(DST.vmempool->shgroups);

  BLI_LINKS_APPEND(&pass->shgroups, shgroup);

  shgroup->shader = shader;
  shgroup->state_extra = 0;
  shgroup->state_extra_disable = ~0x0;
  shgroup->stencil_mask = 0;
  shgroup->calls.first = NULL;
  shgroup->calls.last = NULL;
  shgroup->tfeedback_target = NULL;
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
        GPUTexture **tex_ref = BLI_memblock_alloc(DST.vmempool->images);

        *tex_ref = tex = GPU_texture_from_blender(input->ima, input->iuser, GL_TEXTURE_2D);

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

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass)
{
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  drw_shgroup_init(shgroup, shader);
  return shgroup;
}

DRWShadingGroup *DRW_shgroup_transform_feedback_create(struct GPUShader *shader,
                                                       DRWPass *pass,
                                                       GPUVertBuf *tf_target)
{
  BLI_assert(tf_target != NULL);
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  drw_shgroup_init(shgroup, shader);
  shgroup->tfeedback_target = tf_target;
  return shgroup;
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
  return shgroup->calls.first == NULL;
}

DRWShadingGroup *DRW_shgroup_create_sub(DRWShadingGroup *shgroup)
{
  DRWShadingGroup *shgroup_new = BLI_memblock_alloc(DST.vmempool->shgroups);

  *shgroup_new = *shgroup;
  shgroup_new->uniforms = NULL;
  shgroup_new->calls.first = NULL;
  shgroup_new->calls.last = NULL;

  BLI_LINKS_INSERT_AFTER(&shgroup->pass_parent->shgroups, shgroup, shgroup_new);

  return shgroup_new;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View (DRW_view)
 * \{ */

/* Extract the 8 corners from a Projection Matrix.
 * Although less accurate, this solution can be simplified as follows:
 * BKE_boundbox_init_from_minmax(&bbox, (const float[3]){-1.0f, -1.0f, -1.0f}, (const
 * float[3]){1.0f, 1.0f, 1.0f}); for (int i = 0; i < 8; i++) {mul_project_m4_v3(projinv,
 * bbox.vec[i]);}
 */
static void draw_frustum_boundbox_calc(const float (*viewinv)[4],
                                       const float (*projmat)[4],
                                       BoundBox *r_bbox)
{
  float left, right, bottom, top, near, far;
  bool is_persp = projmat[3][3] == 0.0f;

#if 0 /* Equivalent to this but it has accuracy problems. */
  BKE_boundbox_init_from_minmax(
      &bbox, (const float[3]){-1.0f, -1.0f, -1.0f}, (const float[3]){1.0f, 1.0f, 1.0f});
  for (int i = 0; i < 8; i++) {
    mul_project_m4_v3(projinv, bbox.vec[i]);
  }
#endif

  projmat_dimensions(projmat, &left, &right, &bottom, &top, &near, &far);

  if (is_persp) {
    left *= near;
    right *= near;
    bottom *= near;
    top *= near;
  }

  r_bbox->vec[0][2] = r_bbox->vec[3][2] = r_bbox->vec[7][2] = r_bbox->vec[4][2] = -near;
  r_bbox->vec[0][0] = r_bbox->vec[3][0] = left;
  r_bbox->vec[4][0] = r_bbox->vec[7][0] = right;
  r_bbox->vec[0][1] = r_bbox->vec[4][1] = bottom;
  r_bbox->vec[7][1] = r_bbox->vec[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  r_bbox->vec[1][2] = r_bbox->vec[2][2] = r_bbox->vec[6][2] = r_bbox->vec[5][2] = -far;
  r_bbox->vec[1][0] = r_bbox->vec[2][0] = left;
  r_bbox->vec[6][0] = r_bbox->vec[5][0] = right;
  r_bbox->vec[1][1] = r_bbox->vec[5][1] = bottom;
  r_bbox->vec[2][1] = r_bbox->vec[6][1] = top;

  /* Transform into world space. */
  for (int i = 0; i < 8; i++) {
    mul_m4_v3(viewinv, r_bbox->vec[i]);
  }
}

static void draw_frustum_culling_planes_calc(const BoundBox *bbox, float (*frustum_planes)[4])
{
  /* TODO See if planes_from_projmat cannot do the job. */

  /* Compute clip planes using the world space frustum corners. */
  for (int p = 0; p < 6; p++) {
    int q, r, s;
    switch (p) {
      case 0:
        q = 1;
        r = 2;
        s = 3;
        break; /* -X */
      case 1:
        q = 0;
        r = 4;
        s = 5;
        break; /* -Y */
      case 2:
        q = 1;
        r = 5;
        s = 6;
        break; /* +Z (far) */
      case 3:
        q = 2;
        r = 6;
        s = 7;
        break; /* +Y */
      case 4:
        q = 0;
        r = 3;
        s = 7;
        break; /* -Z (near) */
      default:
        q = 4;
        r = 7;
        s = 6;
        break; /* +X */
    }

    normal_quad_v3(frustum_planes[p], bbox->vec[p], bbox->vec[q], bbox->vec[r], bbox->vec[s]);
    /* Increase precision and use the mean of all 4 corners. */
    frustum_planes[p][3] = -dot_v3v3(frustum_planes[p], bbox->vec[p]);
    frustum_planes[p][3] += -dot_v3v3(frustum_planes[p], bbox->vec[q]);
    frustum_planes[p][3] += -dot_v3v3(frustum_planes[p], bbox->vec[r]);
    frustum_planes[p][3] += -dot_v3v3(frustum_planes[p], bbox->vec[s]);
    frustum_planes[p][3] *= 0.25f;
  }
}

static void draw_frustum_bound_sphere_calc(const BoundBox *bbox,
                                           const float (*viewinv)[4],
                                           const float (*projmat)[4],
                                           const float (*projinv)[4],
                                           BoundSphere *bsphere)
{
  /* Extract Bounding Sphere */
  if (projmat[3][3] != 0.0f) {
    /* Orthographic */
    /* The most extreme points on the near and far plane. (normalized device coords). */
    const float *nearpoint = bbox->vec[0];
    const float *farpoint = bbox->vec[6];

    /* just use median point */
    mid_v3_v3v3(bsphere->center, farpoint, nearpoint);
    bsphere->radius = len_v3v3(bsphere->center, farpoint);
  }
  else if (projmat[2][0] == 0.0f && projmat[2][1] == 0.0f) {
    /* Perspective with symmetrical frustum. */

    /* We obtain the center and radius of the circumscribed circle of the
     * isosceles trapezoid composed by the diagonals of the near and far clipping plane */

    /* center of each clipping plane */
    float mid_min[3], mid_max[3];
    mid_v3_v3v3(mid_min, bbox->vec[3], bbox->vec[4]);
    mid_v3_v3v3(mid_max, bbox->vec[2], bbox->vec[5]);

    /* square length of the diagonals of each clipping plane */
    float a_sq = len_squared_v3v3(bbox->vec[3], bbox->vec[4]);
    float b_sq = len_squared_v3v3(bbox->vec[2], bbox->vec[5]);

    /* distance squared between clipping planes */
    float h_sq = len_squared_v3v3(mid_min, mid_max);

    float fac = (4 * h_sq + b_sq - a_sq) / (8 * h_sq);

    /* The goal is to get the smallest sphere,
     * not the sphere that passes through each corner */
    CLAMP(fac, 0.0f, 1.0f);

    interp_v3_v3v3(bsphere->center, mid_min, mid_max, fac);

    /* distance from the center to one of the points of the far plane (1, 2, 5, 6) */
    bsphere->radius = len_v3v3(bsphere->center, bbox->vec[1]);
  }
  else {
    /* Perspective with asymmetrical frustum. */

    /* We put the sphere center on the line that goes from origin
     * to the center of the far clipping plane. */

    /* Detect which of the corner of the far clipping plane is the farthest to the origin */
    float nfar[4];               /* most extreme far point in NDC space */
    float farxy[2];              /* farpoint projection onto the near plane */
    float farpoint[3] = {0.0f};  /* most extreme far point in camera coordinate */
    float nearpoint[3];          /* most extreme near point in camera coordinate */
    float farcenter[3] = {0.0f}; /* center of far cliping plane in camera coordinate */
    float F = -1.0f, N;          /* square distance of far and near point to origin */
    float f, n; /* distance of far and near point to z axis. f is always > 0 but n can be < 0 */
    float e, s; /* far and near clipping distance (<0) */
    float c;    /* slope of center line = distance of far clipping center
                 * to z axis / far clipping distance. */
    float z;    /* projection of sphere center on z axis (<0) */

    /* Find farthest corner and center of far clip plane. */
    float corner[3] = {1.0f, 1.0f, 1.0f}; /* in clip space */
    for (int i = 0; i < 4; i++) {
      float point[3];
      mul_v3_project_m4_v3(point, projinv, corner);
      float len = len_squared_v3(point);
      if (len > F) {
        copy_v3_v3(nfar, corner);
        copy_v3_v3(farpoint, point);
        F = len;
      }
      add_v3_v3(farcenter, point);
      /* rotate by 90 degree to walk through the 4 points of the far clip plane */
      float tmp = corner[0];
      corner[0] = -corner[1];
      corner[1] = tmp;
    }

    /* the far center is the average of the far clipping points */
    mul_v3_fl(farcenter, 0.25f);
    /* the extreme near point is the opposite point on the near clipping plane */
    copy_v3_fl3(nfar, -nfar[0], -nfar[1], -1.0f);
    mul_v3_project_m4_v3(nearpoint, projinv, nfar);
    /* this is a frustum projection */
    N = len_squared_v3(nearpoint);
    e = farpoint[2];
    s = nearpoint[2];
    /* distance to view Z axis */
    f = len_v2(farpoint);
    /* get corresponding point on the near plane */
    mul_v2_v2fl(farxy, farpoint, s / e);
    /* this formula preserve the sign of n */
    sub_v2_v2(nearpoint, farxy);
    n = f * s / e - len_v2(nearpoint);
    c = len_v2(farcenter) / e;
    /* the big formula, it simplifies to (F-N)/(2(e-s)) for the symmetric case */
    z = (F - N) / (2.0f * (e - s + c * (f - n)));

    bsphere->center[0] = farcenter[0] * z / e;
    bsphere->center[1] = farcenter[1] * z / e;
    bsphere->center[2] = z;
    bsphere->radius = len_v3v3(bsphere->center, farpoint);

    /* Transform to world space. */
    mul_m4_v3(viewinv, bsphere->center);
  }
}

static void draw_view_matrix_state_update(DRWViewUboStorage *storage,
                                          const float viewmat[4][4],
                                          const float winmat[4][4])
{
  /* If only one the matrices is negative, then the
   * polygon winding changes and we don't want that.
   * By convention, the winmat is negative because
   * looking through the -Z axis. So this inverse the
   * changes the test for the winmat. */
  BLI_assert(is_negative_m4(viewmat) == !is_negative_m4(winmat));

  copy_m4_m4(storage->viewmat, viewmat);
  invert_m4_m4(storage->viewinv, storage->viewmat);

  copy_m4_m4(storage->winmat, winmat);
  invert_m4_m4(storage->wininv, storage->winmat);

  mul_m4_m4m4(storage->persmat, winmat, viewmat);
  invert_m4_m4(storage->persinv, storage->persmat);
}

/* Create a view with culling. */
DRWView *DRW_view_create(const float viewmat[4][4],
                         const float winmat[4][4],
                         const float (*culling_viewmat)[4],
                         const float (*culling_winmat)[4],
                         DRWCallVisibilityFn *visibility_fn)
{
  DRWView *view = BLI_memblock_alloc(DST.vmempool->views);

  if (DST.primary_view_ct < MAX_CULLED_VIEWS) {
    view->culling_mask = 1u << DST.primary_view_ct++;
  }
  else {
    BLI_assert(0);
    view->culling_mask = 0u;
  }
  view->clip_planes_len = 0;
  view->visibility_fn = visibility_fn;
  view->parent = NULL;

  copy_v4_fl4(view->storage.viewcamtexcofac, 1.0f, 1.0f, 0.0f, 0.0f);

  DRW_view_update(view, viewmat, winmat, culling_viewmat, culling_winmat);

  return view;
}

/* Create a view with culling done by another view. */
DRWView *DRW_view_create_sub(const DRWView *parent_view,
                             const float viewmat[4][4],
                             const float winmat[4][4])
{
  BLI_assert(parent_view && parent_view->parent == NULL);

  DRWView *view = BLI_memblock_alloc(DST.vmempool->views);

  /* Perform copy. */
  *view = *parent_view;
  view->parent = (DRWView *)parent_view;

  DRW_view_update_sub(view, viewmat, winmat);

  return view;
}

/**
 * DRWView Update:
 * This is meant to be done on existing views when rendering in a loop and there is no
 * need to allocate more DRWViews.
 **/

/* Update matrices of a view created with DRW_view_create_sub. */
void DRW_view_update_sub(DRWView *view, const float viewmat[4][4], const float winmat[4][4])
{
  BLI_assert(view->parent != NULL);

  view->is_dirty = true;

  draw_view_matrix_state_update(&view->storage, viewmat, winmat);
}

/* Update matrices of a view created with DRW_view_create. */
void DRW_view_update(DRWView *view,
                     const float viewmat[4][4],
                     const float winmat[4][4],
                     const float (*culling_viewmat)[4],
                     const float (*culling_winmat)[4])
{
  /* DO NOT UPDATE THE DEFAULT VIEW.
   * Create subviews instead, or a copy. */
  BLI_assert(view != DST.view_default);
  BLI_assert(view->parent == NULL);

  view->is_dirty = true;

  draw_view_matrix_state_update(&view->storage, viewmat, winmat);

  /* Prepare frustum culling. */

#ifdef DRW_DEBUG_CULLING
  static float mv[MAX_CULLED_VIEWS][4][4], mw[MAX_CULLED_VIEWS][4][4];

  /* Select view here. */
  if (view->culling_mask != 0) {
    uint index = bitscan_forward_uint(view->culling_mask);

    if (G.debug_value == 0) {
      copy_m4_m4(mv[index], culling_viewmat ? culling_viewmat : viewmat);
      copy_m4_m4(mw[index], culling_winmat ? culling_winmat : winmat);
    }
    else {
      culling_winmat = mw[index];
      culling_viewmat = mv[index];
    }
  }
#endif

  float wininv[4][4];
  if (culling_winmat) {
    winmat = culling_winmat;
    invert_m4_m4(wininv, winmat);
  }
  else {
    copy_m4_m4(wininv, view->storage.wininv);
  }

  float viewinv[4][4];
  if (culling_viewmat) {
    viewmat = culling_viewmat;
    invert_m4_m4(viewinv, viewmat);
  }
  else {
    copy_m4_m4(viewinv, view->storage.viewinv);
  }

  draw_frustum_boundbox_calc(viewinv, winmat, &view->frustum_corners);
  draw_frustum_culling_planes_calc(&view->frustum_corners, view->frustum_planes);
  draw_frustum_bound_sphere_calc(
      &view->frustum_corners, viewinv, winmat, wininv, &view->frustum_bsphere);

#ifdef DRW_DEBUG_CULLING
  if (G.debug_value != 0) {
    DRW_debug_sphere(
        view->frustum_bsphere.center, view->frustum_bsphere.radius, (const float[4]){1, 1, 0, 1});
    DRW_debug_bbox(&view->frustum_corners, (const float[4]){1, 1, 0, 1});
  }
#endif
}

/* Return default view if it is a viewport render. */
const DRWView *DRW_view_default_get(void)
{
  return DST.view_default;
}

/* MUST only be called once per render and only in render mode. Sets default view. */
void DRW_view_default_set(DRWView *view)
{
  BLI_assert(DST.view_default == NULL);
  DST.view_default = view;
}

/**
 * This only works if DRWPasses have been tagged with DRW_STATE_CLIP_PLANES,
 * and if the shaders have support for it (see usage of gl_ClipDistance).
 * NOTE: planes must be in world space.
 */
void DRW_view_clip_planes_set(DRWView *view, float (*planes)[4], int plane_len)
{
  BLI_assert(plane_len <= MAX_CLIP_PLANES);
  view->clip_planes_len = plane_len;
  if (plane_len > 0) {
    memcpy(view->storage.clipplanes, planes, sizeof(float) * 4 * plane_len);
  }
}

void DRW_view_camtexco_set(DRWView *view, float texco[4])
{
  copy_v4_v4(view->storage.viewcamtexcofac, texco);
}

/* Return world space frustum corners. */
void DRW_view_frustum_corners_get(const DRWView *view, BoundBox *corners)
{
  memcpy(corners, &view->frustum_corners, sizeof(view->frustum_corners));
}

/* Return world space frustum sides as planes.
 * See draw_frustum_culling_planes_calc() for the plane order. */
void DRW_view_frustum_planes_get(const DRWView *view, float planes[6][4])
{
  memcpy(planes, &view->frustum_planes, sizeof(view->frustum_planes));
}

bool DRW_view_is_persp_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  return view->storage.winmat[3][3] == 0.0f;
}

float DRW_view_near_distance_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  const float(*projmat)[4] = view->storage.winmat;

  if (DRW_view_is_persp_get(view)) {
    return -projmat[3][2] / (projmat[2][2] - 1.0f);
  }
  else {
    return -(projmat[3][2] + 1.0f) / projmat[2][2];
  }
}

float DRW_view_far_distance_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  const float(*projmat)[4] = view->storage.winmat;

  if (DRW_view_is_persp_get(view)) {
    return -projmat[3][2] / (projmat[2][2] + 1.0f);
  }
  else {
    return -(projmat[3][2] - 1.0f) / projmat[2][2];
  }
}

void DRW_view_viewmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const DRWViewUboStorage *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->viewinv : storage->viewmat);
}

void DRW_view_winmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const DRWViewUboStorage *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->wininv : storage->winmat);
}

void DRW_view_persmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const DRWViewUboStorage *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->persinv : storage->persmat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Passes (DRW_pass)
 * \{ */

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
  DRWPass *pass = BLI_memblock_alloc(DST.vmempool->passes);
  pass->state = state | DRW_STATE_PROGRAM_POINT_SIZE;
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
  pass->state = state | DRW_STATE_PROGRAM_POINT_SIZE;
}

void DRW_pass_state_add(DRWPass *pass, DRWState state)
{
  pass->state |= state;
}

void DRW_pass_state_remove(DRWPass *pass, DRWState state)
{
  pass->state &= ~state;
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
  const float *axis;
  const float *origin;
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
  const float(*viewinv)[4] = DST.view_active->storage.viewinv;

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
