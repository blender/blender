/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DRW_pbvh.hh"

#include "draw_attributes.hh"
#include "draw_manager.h"
#include "draw_pbvh.h"

#include "BKE_curve.h"
#include "BKE_duplilist.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh_api.hh"
#include "BKE_volume.h"

/* For debug cursor position. */
#include "WM_api.h"
#include "wm_window.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"

#include "BLI_array.hh"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_memblock.h"
#include "BLI_mempool.h"

#ifdef DRW_DEBUG_CULLING
#  include "BLI_math_bits.h"
#endif

#include "GPU_capabilities.h"
#include "GPU_material.h"
#include "GPU_uniform_buffer.h"

#include "intern/gpu_codegen.h"

/**
 * IMPORTANT:
 * In order to be able to write to the same print buffer sequentially, we add a barrier to allow
 * multiple shader calls writing to the same buffer.
 * However, this adds explicit synchronization events which might change the rest of the
 * application behavior and hide some bugs. If you know you are using shader debug print in only
 * one shader pass, you can comment this out to remove the aforementioned barrier.
 */
#define DISABLE_DEBUG_SHADER_PRINT_BARRIER

/* -------------------------------------------------------------------- */
/** \name Uniform Buffer Object (DRW_uniformbuffer)
 * \{ */

static void draw_call_sort(DRWCommand *array, DRWCommand *array_tmp, int array_len)
{
  /* Count unique batches. It's not really important if
   * there is collisions. If there is a lot of different batches,
   * the sorting benefit will be negligible.
   * So at least sort fast! */
  uchar idx[128] = {0};
  /* Shift by 6 positions knowing each GPUBatch is > 64 bytes */
#define KEY(a) ((size_t((a).draw.batch) >> 6) % ARRAY_SIZE(idx))
  BLI_assert(array_len <= ARRAY_SIZE(idx));

  for (int i = 0; i < array_len; i++) {
    /* Early out if nothing to sort. */
    if (++idx[KEY(array[i])] == array_len) {
      return;
    }
  }
  /* Accumulate batch indices */
  for (int i = 1; i < ARRAY_SIZE(idx); i++) {
    idx[i] += idx[i - 1];
  }
  /* Traverse in reverse to not change the order of the resource ID's. */
  for (int src = array_len - 1; src >= 0; src--) {
    array_tmp[--idx[KEY(array[src])]] = array[src];
  }
#undef KEY

  memcpy(array, array_tmp, sizeof(*array) * array_len);
}

void drw_resource_buffer_finish(DRWData *vmempool)
{
  int chunk_id = DRW_handle_chunk_get(&DST.resource_handle);
  int elem_id = DRW_handle_id_get(&DST.resource_handle);
  int ubo_len = 1 + chunk_id - ((elem_id == 0) ? 1 : 0);
  size_t list_size = sizeof(GPUUniformBuf *) * ubo_len;

  /* TODO: find a better system. currently a lot of obinfos UBO are going to be unused
   * if not rendering with Eevee. */

  if (vmempool->matrices_ubo == nullptr) {
    vmempool->matrices_ubo = static_cast<GPUUniformBuf **>(MEM_callocN(list_size, __func__));
    vmempool->obinfos_ubo = static_cast<GPUUniformBuf **>(MEM_callocN(list_size, __func__));
    vmempool->ubo_len = ubo_len;
  }

  /* Remove unnecessary buffers */
  for (int i = ubo_len; i < vmempool->ubo_len; i++) {
    GPU_uniformbuf_free(vmempool->matrices_ubo[i]);
    GPU_uniformbuf_free(vmempool->obinfos_ubo[i]);
  }

  if (ubo_len != vmempool->ubo_len) {
    vmempool->matrices_ubo = static_cast<GPUUniformBuf **>(
        MEM_recallocN(vmempool->matrices_ubo, list_size));
    vmempool->obinfos_ubo = static_cast<GPUUniformBuf **>(
        MEM_recallocN(vmempool->obinfos_ubo, list_size));
    vmempool->ubo_len = ubo_len;
  }

  /* Create/Update buffers. */
  for (int i = 0; i < ubo_len; i++) {
    void *data_obmat = BLI_memblock_elem_get(vmempool->obmats, i, 0);
    void *data_infos = BLI_memblock_elem_get(vmempool->obinfos, i, 0);
    if (vmempool->matrices_ubo[i] == nullptr) {
      vmempool->matrices_ubo[i] = GPU_uniformbuf_create(sizeof(DRWObjectMatrix) *
                                                        DRW_RESOURCE_CHUNK_LEN);
      vmempool->obinfos_ubo[i] = GPU_uniformbuf_create(sizeof(DRWObjectInfos) *
                                                       DRW_RESOURCE_CHUNK_LEN);
    }
    GPU_uniformbuf_update(vmempool->matrices_ubo[i], data_obmat);
    GPU_uniformbuf_update(vmempool->obinfos_ubo[i], data_infos);
  }

  DRW_uniform_attrs_pool_flush_all(vmempool->obattrs_ubo_pool);

  /* Aligned alloc to avoid unaligned memcpy. */
  DRWCommandChunk *chunk_tmp = static_cast<DRWCommandChunk *>(
      MEM_mallocN_aligned(sizeof(DRWCommandChunk), 16, __func__));
  DRWCommandChunk *chunk;
  BLI_memblock_iter iter;
  BLI_memblock_iternew(vmempool->commands, &iter);
  while ((chunk = static_cast<DRWCommandChunk *>(BLI_memblock_iterstep(&iter)))) {
    bool sortable = true;
    /* We can only sort chunks that contain #DRWCommandDraw only. */
    for (int i = 0; i < ARRAY_SIZE(chunk->command_type) && sortable; i++) {
      if (chunk->command_type[i] != 0) {
        sortable = false;
      }
    }
    if (sortable) {
      draw_call_sort(chunk->commands, chunk_tmp->commands, chunk->command_used);
    }
  }
  MEM_freeN(chunk_tmp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms (DRW_shgroup_uniform)
 * \{ */

static void drw_shgroup_uniform_create_ex(DRWShadingGroup *shgroup,
                                          int loc,
                                          DRWUniformType type,
                                          const void *value,
                                          GPUSamplerState sampler_state,
                                          int length,
                                          int arraysize)
{
  if (loc == -1) {
    /* Nice to enable eventually, for now EEVEE uses uniforms that might not exist. */
    // BLI_assert(0);
    return;
  }

  DRWUniformChunk *unichunk = shgroup->uniforms;
  /* Happens on first uniform or if chunk is full. */
  if (!unichunk || unichunk->uniform_used == unichunk->uniform_len) {
    unichunk = static_cast<DRWUniformChunk *>(BLI_memblock_alloc(DST.vmempool->uniforms));
    unichunk->uniform_len = BOUNDED_ARRAY_TYPE_SIZE<decltype(shgroup->uniforms->uniforms)>();
    unichunk->uniform_used = 0;
    BLI_LINKS_PREPEND(shgroup->uniforms, unichunk);
  }

  DRWUniform *uni = unichunk->uniforms + unichunk->uniform_used++;

  uni->location = loc;
  uni->type = type;
  uni->length = length;
  uni->arraysize = arraysize;

  switch (type) {
    case DRW_UNIFORM_INT_COPY:
      BLI_assert(length <= 4);
      memcpy(uni->ivalue, value, sizeof(int) * length);
      break;
    case DRW_UNIFORM_FLOAT_COPY:
      BLI_assert(length <= 4);
      memcpy(uni->fvalue, value, sizeof(float) * length);
      break;
    case DRW_UNIFORM_BLOCK:
      uni->block = (GPUUniformBuf *)value;
      break;
    case DRW_UNIFORM_BLOCK_REF:
      uni->block_ref = (GPUUniformBuf **)value;
      break;
    case DRW_UNIFORM_IMAGE:
    case DRW_UNIFORM_TEXTURE:
      uni->texture = (GPUTexture *)value;
      uni->sampler_state = sampler_state;
      break;
    case DRW_UNIFORM_IMAGE_REF:
    case DRW_UNIFORM_TEXTURE_REF:
      uni->texture_ref = (GPUTexture **)value;
      uni->sampler_state = sampler_state;
      break;
    case DRW_UNIFORM_BLOCK_OBATTRS:
      uni->uniform_attrs = (GPUUniformAttrList *)value;
      break;
    default:
      uni->pvalue = (const float *)value;
      break;
  }
}

static void drw_shgroup_uniform(DRWShadingGroup *shgroup,
                                const char *name,
                                DRWUniformType type,
                                const void *value,
                                int length,
                                int arraysize)
{
  BLI_assert(arraysize > 0 && arraysize <= 16);
  BLI_assert(length >= 0 && length <= 16);
  BLI_assert(!ELEM(type,
                   DRW_UNIFORM_STORAGE_BLOCK,
                   DRW_UNIFORM_STORAGE_BLOCK_REF,
                   DRW_UNIFORM_BLOCK,
                   DRW_UNIFORM_BLOCK_REF,
                   DRW_UNIFORM_TEXTURE,
                   DRW_UNIFORM_TEXTURE_REF));
  int location = GPU_shader_get_uniform(shgroup->shader, name);
  drw_shgroup_uniform_create_ex(
      shgroup, location, type, value, GPUSamplerState::default_sampler(), length, arraysize);
}

void DRW_shgroup_uniform_texture_ex(DRWShadingGroup *shgroup,
                                    const char *name,
                                    const GPUTexture *tex,
                                    GPUSamplerState sampler_state)
{
  BLI_assert(tex != nullptr);
  int loc = GPU_shader_get_sampler_binding(shgroup->shader, name);
  drw_shgroup_uniform_create_ex(shgroup, loc, DRW_UNIFORM_TEXTURE, tex, sampler_state, 0, 1);
}

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex)
{
  DRW_shgroup_uniform_texture_ex(shgroup, name, tex, GPUSamplerState::internal_sampler());
}

void DRW_shgroup_uniform_texture_ref_ex(DRWShadingGroup *shgroup,
                                        const char *name,
                                        GPUTexture **tex,
                                        GPUSamplerState sampler_state)
{
  BLI_assert(tex != nullptr);
  int loc = GPU_shader_get_sampler_binding(shgroup->shader, name);
  drw_shgroup_uniform_create_ex(shgroup, loc, DRW_UNIFORM_TEXTURE_REF, tex, sampler_state, 0, 1);
}

void DRW_shgroup_uniform_texture_ref(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex)
{
  DRW_shgroup_uniform_texture_ref_ex(shgroup, name, tex, GPUSamplerState::internal_sampler());
}

void DRW_shgroup_uniform_image(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex)
{
  BLI_assert(tex != nullptr);
  int loc = GPU_shader_get_sampler_binding(shgroup->shader, name);
  drw_shgroup_uniform_create_ex(
      shgroup, loc, DRW_UNIFORM_IMAGE, tex, GPUSamplerState::default_sampler(), 0, 1);
}

void DRW_shgroup_uniform_image_ref(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex)
{
  BLI_assert(tex != nullptr);
  int loc = GPU_shader_get_sampler_binding(shgroup->shader, name);
  drw_shgroup_uniform_create_ex(
      shgroup, loc, DRW_UNIFORM_IMAGE_REF, tex, GPUSamplerState::default_sampler(), 0, 1);
}

void DRW_shgroup_uniform_block_ex(DRWShadingGroup *shgroup,
                                  const char *name,
                                  const GPUUniformBuf *ubo DRW_DEBUG_FILE_LINE_ARGS)
{
  BLI_assert(ubo != nullptr);
  int loc = GPU_shader_get_ubo_binding(shgroup->shader, name);
  if (loc == -1) {
#ifdef DRW_UNUSED_RESOURCE_TRACKING
    printf("%s:%d: Unable to locate binding of shader uniform buffer object: %s.\n",
           file,
           line,
           name);
#else
    /* TODO(@fclem): Would be good to have, but eevee has too much of this for the moment. */
    // BLI_assert_msg(0, "Unable to locate binding of shader uniform buffer objects.");
#endif
    return;
  }
  drw_shgroup_uniform_create_ex(
      shgroup, loc, DRW_UNIFORM_BLOCK, ubo, GPUSamplerState::default_sampler(), 0, 1);
}

void DRW_shgroup_uniform_block_ref_ex(DRWShadingGroup *shgroup,
                                      const char *name,
                                      GPUUniformBuf **ubo DRW_DEBUG_FILE_LINE_ARGS)
{
  BLI_assert(ubo != nullptr);
  int loc = GPU_shader_get_ubo_binding(shgroup->shader, name);
  if (loc == -1) {
#ifdef DRW_UNUSED_RESOURCE_TRACKING
    printf("%s:%d: Unable to locate binding of shader uniform buffer object: %s.\n",
           file,
           line,
           name);
#else
    /* TODO(@fclem): Would be good to have, but eevee has too much of this for the moment. */
    // BLI_assert_msg(0, "Unable to locate binding of shader uniform buffer objects.");
#endif
    return;
  }
  drw_shgroup_uniform_create_ex(
      shgroup, loc, DRW_UNIFORM_BLOCK_REF, ubo, GPUSamplerState::default_sampler(), 0, 1);
}

void DRW_shgroup_storage_block_ex(DRWShadingGroup *shgroup,
                                  const char *name,
                                  const GPUStorageBuf *ssbo DRW_DEBUG_FILE_LINE_ARGS)
{
  BLI_assert(ssbo != nullptr);
  /* TODO(@fclem): Fix naming inconsistency. */
  int loc = GPU_shader_get_ssbo_binding(shgroup->shader, name);
  if (loc == -1) {
#ifdef DRW_UNUSED_RESOURCE_TRACKING
    printf("%s:%d: Unable to locate binding of shader storage buffer object: %s.\n",
           file,
           line,
           name);
#else
    /* TODO(@fclem): Would be good to have, but eevee has too much of this for the moment. */
    // BLI_assert_msg(0, "Unable to locate binding of shader storage buffer objects.");
#endif
    return;
  }
  drw_shgroup_uniform_create_ex(
      shgroup, loc, DRW_UNIFORM_STORAGE_BLOCK, ssbo, GPUSamplerState::default_sampler(), 0, 1);
}

void DRW_shgroup_storage_block_ref_ex(DRWShadingGroup *shgroup,
                                      const char *name,
                                      GPUStorageBuf **ssbo DRW_DEBUG_FILE_LINE_ARGS)
{
  BLI_assert(ssbo != nullptr);
  /* TODO(@fclem): Fix naming inconsistency. */
  int loc = GPU_shader_get_ssbo_binding(shgroup->shader, name);
  if (loc == -1) {
#ifdef DRW_UNUSED_RESOURCE_TRACKING
    printf("%s:%d: Unable to locate binding of shader storage buffer object: %s.\n",
           file,
           line,
           name);
#else
    /* TODO(@fclem): Would be good to have, but eevee has too much of this for the moment. */
    // BLI_assert_msg(0, "Unable to locate binding of shader storage buffer objects.");
#endif
    return;
  }
  drw_shgroup_uniform_create_ex(
      shgroup, loc, DRW_UNIFORM_STORAGE_BLOCK_REF, ssbo, GPUSamplerState::default_sampler(), 0, 1);
}

void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup,
                              const char *name,
                              const int *value,
                              int arraysize)
{
  /* Boolean are expected to be 4bytes longs for OpenGL! */
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

void DRW_shgroup_uniform_int_copy(DRWShadingGroup *shgroup, const char *name, const int value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT_COPY, &value, 1, 1);
}

void DRW_shgroup_uniform_ivec2_copy(DRWShadingGroup *shgroup, const char *name, const int *value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT_COPY, value, 2, 1);
}

void DRW_shgroup_uniform_ivec3_copy(DRWShadingGroup *shgroup, const char *name, const int *value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT_COPY, value, 3, 1);
}

void DRW_shgroup_uniform_ivec4_copy(DRWShadingGroup *shgroup, const char *name, const int *value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_INT_COPY, value, 4, 1);
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

void DRW_shgroup_uniform_vec3_copy(DRWShadingGroup *shgroup, const char *name, const float *value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT_COPY, value, 3, 1);
}

void DRW_shgroup_uniform_vec4_copy(DRWShadingGroup *shgroup, const char *name, const float *value)
{
  drw_shgroup_uniform(shgroup, name, DRW_UNIFORM_FLOAT_COPY, value, 4, 1);
}

void DRW_shgroup_uniform_mat4_copy(DRWShadingGroup *shgroup,
                                   const char *name,
                                   const float (*value)[4])
{
  int location = GPU_shader_get_uniform(shgroup->shader, name);

  if (location == -1) {
    /* Nice to enable eventually, for now EEVEE uses uniforms that might not exist. */
    // BLI_assert(0);
    return;
  }

  /* Each array element stored as an individual entry in the uniform list.
   * All entries from the same array share the same base location,
   * and array-size used to determine the number of elements
   * copied in draw_update_uniforms. */
  for (int i = 0; i < 4; i++) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  location,
                                  DRW_UNIFORM_FLOAT_COPY,
                                  &value[i],
                                  GPUSamplerState::default_sampler(),
                                  4,
                                  4);
  }
}

void DRW_shgroup_vertex_buffer_ex(DRWShadingGroup *shgroup,
                                  const char *name,
                                  GPUVertBuf *vertex_buffer DRW_DEBUG_FILE_LINE_ARGS)
{
  int location = GPU_shader_get_ssbo_binding(shgroup->shader, name);
  if (location == -1) {
#ifdef DRW_UNUSED_RESOURCE_TRACKING
    printf("%s:%d: Unable to locate binding of shader storage buffer object: %s.\n",
           file,
           line,
           name);
#else
    BLI_assert_msg(0, "Unable to locate binding of shader storage buffer objects.");
#endif
    return;
  }
  drw_shgroup_uniform_create_ex(shgroup,
                                location,
                                DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE,
                                vertex_buffer,
                                GPUSamplerState::default_sampler(),
                                0,
                                1);
}

void DRW_shgroup_vertex_buffer_ref_ex(DRWShadingGroup *shgroup,
                                      const char *name,
                                      GPUVertBuf **vertex_buffer DRW_DEBUG_FILE_LINE_ARGS)
{
  int location = GPU_shader_get_ssbo_binding(shgroup->shader, name);
  if (location == -1) {
#ifdef DRW_UNUSED_RESOURCE_TRACKING
    printf("%s:%d: Unable to locate binding of shader storage buffer object: %s.\n",
           file,
           line,
           name);
#else
    BLI_assert_msg(0, "Unable to locate binding of shader storage buffer objects.");
#endif
    return;
  }
  drw_shgroup_uniform_create_ex(shgroup,
                                location,
                                DRW_UNIFORM_VERTEX_BUFFER_AS_STORAGE_REF,
                                vertex_buffer,
                                GPUSamplerState::default_sampler(),
                                0,
                                1);
}

void DRW_shgroup_buffer_texture(DRWShadingGroup *shgroup,
                                const char *name,
                                GPUVertBuf *vertex_buffer)
{
  int location = GPU_shader_get_sampler_binding(shgroup->shader, name);
  if (location == -1) {
    return;
  }
  drw_shgroup_uniform_create_ex(shgroup,
                                location,
                                DRW_UNIFORM_VERTEX_BUFFER_AS_TEXTURE,
                                vertex_buffer,
                                GPUSamplerState::default_sampler(),
                                0,
                                1);
}

void DRW_shgroup_buffer_texture_ref(DRWShadingGroup *shgroup,
                                    const char *name,
                                    GPUVertBuf **vertex_buffer)
{
  int location = GPU_shader_get_sampler_binding(shgroup->shader, name);
  if (location == -1) {
    return;
  }
  drw_shgroup_uniform_create_ex(shgroup,
                                location,
                                DRW_UNIFORM_VERTEX_BUFFER_AS_TEXTURE_REF,
                                vertex_buffer,
                                GPUSamplerState::default_sampler(),
                                0,
                                1);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Call (DRW_calls)
 * \{ */

static void drw_call_calc_orco(Object *ob, float (*r_orcofacs)[4])
{
  ID *ob_data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  struct {
    float texspace_location[3], texspace_size[3];
  } static_buf;
  float *texspace_location = nullptr;
  float *texspace_size = nullptr;
  if (ob_data != nullptr) {
    switch (GS(ob_data->name)) {
      case ID_VO: {
        BoundBox *bbox = BKE_volume_boundbox_get(ob);
        mid_v3_v3v3(static_buf.texspace_location, bbox->vec[0], bbox->vec[6]);
        sub_v3_v3v3(static_buf.texspace_size, bbox->vec[0], bbox->vec[6]);
        texspace_location = static_buf.texspace_location;
        texspace_size = static_buf.texspace_size;
        break;
      }
      case ID_ME:
        BKE_mesh_texspace_get_reference(
            (Mesh *)ob_data, nullptr, &texspace_location, &texspace_size);
        break;
      case ID_CU_LEGACY: {
        Curve *cu = (Curve *)ob_data;
        BKE_curve_texspace_ensure(cu);
        texspace_location = cu->texspace_location;
        texspace_size = cu->texspace_size;
        break;
      }
      case ID_MB: {
        MetaBall *mb = (MetaBall *)ob_data;
        texspace_location = mb->texspace_location;
        texspace_size = mb->texspace_size;
        break;
      }
      default:
        break;
    }
  }

  if ((texspace_location != nullptr) && (texspace_size != nullptr)) {
    mul_v3_v3fl(r_orcofacs[1], texspace_size, 2.0f);
    invert_v3(r_orcofacs[1]);
    sub_v3_v3v3(r_orcofacs[0], texspace_location, texspace_size);
    negate_v3(r_orcofacs[0]);
    mul_v3_v3(r_orcofacs[0], r_orcofacs[1]); /* result in a nice MADD in the shader */
  }
  else {
    copy_v3_fl(r_orcofacs[0], 0.0f);
    copy_v3_fl(r_orcofacs[1], 1.0f);
  }
}

BLI_INLINE void drw_call_matrix_init(DRWObjectMatrix *ob_mats, Object *ob, float (*obmat)[4])
{
  copy_m4_m4(ob_mats->model, obmat);
  if (ob) {
    copy_m4_m4(ob_mats->modelinverse, ob->world_to_object);
  }
  else {
    /* WATCH: Can be costly. */
    invert_m4_m4(ob_mats->modelinverse, ob_mats->model);
  }
}

static void drw_call_obinfos_init(DRWObjectInfos *ob_infos, Object *ob)
{
  BLI_assert(ob);
  /* Index. */
  ob_infos->ob_index = ob->index;
  /* Orco factors. */
  drw_call_calc_orco(ob, ob_infos->orcotexfac);
  /* Random float value. */
  uint random = (DST.dupli_source) ?
                     DST.dupli_source->random_id :
                     /* TODO(fclem): this is rather costly to do at runtime. Maybe we can
                      * put it in ob->runtime and make depsgraph ensure it is up to date. */
                     BLI_hash_int_2d(BLI_hash_string(ob->id.name + 2), 0);
  ob_infos->ob_random = random * (1.0f / float(0xFFFFFFFF));
  /* Object State. */
  ob_infos->ob_flag = 1.0f; /* Required to have a correct sign */
  ob_infos->ob_flag += (ob->base_flag & BASE_SELECTED) ? (1 << 1) : 0;
  ob_infos->ob_flag += (ob->base_flag & BASE_FROM_DUPLI) ? (1 << 2) : 0;
  ob_infos->ob_flag += (ob->base_flag & BASE_FROM_SET) ? (1 << 3) : 0;
  if (ob->base_flag & BASE_FROM_DUPLI) {
    ob_infos->ob_flag += (DRW_object_get_dupli_parent(ob) == DST.draw_ctx.obact) ? (1 << 4) : 0;
  }
  else {
    ob_infos->ob_flag += (ob == DST.draw_ctx.obact) ? (1 << 4) : 0;
  }
  /* Negative scaling. */
  ob_infos->ob_flag *= (ob->transflag & OB_NEG_SCALE) ? -1.0f : 1.0f;
  /* Object Color. */
  copy_v4_v4(ob_infos->ob_color, ob->color);
}

static void drw_call_culling_init(DRWCullingState *cull, Object *ob)
{
  const BoundBox *bbox;
  if (ob != nullptr && (bbox = BKE_object_boundbox_get(ob))) {
    float corner[3];
    /* Get BoundSphere center and radius from the BoundBox. */
    mid_v3_v3v3(cull->bsphere.center, bbox->vec[0], bbox->vec[6]);
    mul_v3_m4v3(corner, ob->object_to_world, bbox->vec[0]);
    mul_m4_v3(ob->object_to_world, cull->bsphere.center);
    cull->bsphere.radius = len_v3v3(cull->bsphere.center, corner);

    /* Bypass test for very large objects (see #67319). */
    if (UNLIKELY(cull->bsphere.radius > 1e12)) {
      cull->bsphere.radius = -1.0f;
    }
  }
  else {
    /* Bypass test. */
    cull->bsphere.radius = -1.0f;
  }
  /* Reset user data */
  cull->user_data = nullptr;
}

static DRWResourceHandle drw_resource_handle_new(float (*obmat)[4], Object *ob)
{
  DRWCullingState *culling = static_cast<DRWCullingState *>(
      BLI_memblock_alloc(DST.vmempool->cullstates));
  DRWObjectMatrix *ob_mats = static_cast<DRWObjectMatrix *>(
      BLI_memblock_alloc(DST.vmempool->obmats));
  /* FIXME Meh, not always needed but can be accessed after creation.
   * Also it needs to have the same resource handle. */
  DRWObjectInfos *ob_infos = static_cast<DRWObjectInfos *>(
      BLI_memblock_alloc(DST.vmempool->obinfos));
  UNUSED_VARS(ob_infos);

  DRWResourceHandle handle = DST.resource_handle;
  DRW_handle_increment(&DST.resource_handle);

  if (ob && (ob->transflag & OB_NEG_SCALE)) {
    DRW_handle_negative_scale_enable(&handle);
  }

  drw_call_matrix_init(ob_mats, ob, obmat);
  drw_call_culling_init(culling, ob);
  /* ob_infos is init only if needed. */

  return handle;
}

uint32_t DRW_object_resource_id_get(Object * /*ob*/)
{
  DRWResourceHandle handle = DST.ob_handle;
  if (handle == 0) {
    /* Handle not yet allocated. Return next handle. */
    handle = DST.resource_handle;
  }
  return handle & ~(1u << 31);
}

static DRWResourceHandle drw_resource_handle(DRWShadingGroup *shgroup,
                                             float (*obmat)[4],
                                             Object *ob)
{
  if (ob == nullptr) {
    if (obmat == nullptr) {
      DRWResourceHandle handle = 0;
      return handle;
    }

    return drw_resource_handle_new(obmat, nullptr);
  }

  if (DST.ob_handle == 0) {
    DST.ob_handle = drw_resource_handle_new(obmat, ob);
    DST.ob_state_obinfo_init = false;
  }

  if (shgroup->objectinfo) {
    if (!DST.ob_state_obinfo_init) {
      DST.ob_state_obinfo_init = true;
      DRWObjectInfos *ob_infos = static_cast<DRWObjectInfos *>(
          DRW_memblock_elem_from_handle(DST.vmempool->obinfos, &DST.ob_handle));

      drw_call_obinfos_init(ob_infos, ob);
    }
  }

  if (shgroup->uniform_attrs) {
    drw_uniform_attrs_pool_update(DST.vmempool->obattrs_ubo_pool,
                                  shgroup->uniform_attrs,
                                  &DST.ob_handle,
                                  ob,
                                  DST.dupli_parent,
                                  DST.dupli_source);
  }

  return DST.ob_handle;
}

static void command_type_set(uint64_t *command_type_bits, int index, eDRWCommandType type)
{
  command_type_bits[index / 16] |= uint64_t(type) << ((index % 16) * 4);
}

eDRWCommandType command_type_get(const uint64_t *command_type_bits, int index)
{
  return eDRWCommandType((command_type_bits[index / 16] >> ((index % 16) * 4)) & 0xF);
}

static void *drw_command_create(DRWShadingGroup *shgroup, eDRWCommandType type)
{
  DRWCommandChunk *chunk = shgroup->cmd.last;

  if (chunk == nullptr) {
    DRWCommandSmallChunk *smallchunk = static_cast<DRWCommandSmallChunk *>(
        BLI_memblock_alloc(DST.vmempool->commands_small));
    smallchunk->command_len = ARRAY_SIZE(smallchunk->commands);
    smallchunk->command_used = 0;
    smallchunk->command_type[0] = 0x0lu;
    chunk = (DRWCommandChunk *)smallchunk;
    BLI_LINKS_APPEND(&shgroup->cmd, chunk);
  }
  else if (chunk->command_used == chunk->command_len) {
    chunk = static_cast<DRWCommandChunk *>(BLI_memblock_alloc(DST.vmempool->commands));
    chunk->command_len = ARRAY_SIZE(chunk->commands);
    chunk->command_used = 0;
    memset(chunk->command_type, 0x0, sizeof(chunk->command_type));
    BLI_LINKS_APPEND(&shgroup->cmd, chunk);
  }

  command_type_set(chunk->command_type, chunk->command_used, type);

  return chunk->commands + chunk->command_used++;
}

static void drw_command_draw(DRWShadingGroup *shgroup, GPUBatch *batch, DRWResourceHandle handle)
{
  DRWCommandDraw *cmd = static_cast<DRWCommandDraw *>(drw_command_create(shgroup, DRW_CMD_DRAW));
  cmd->batch = batch;
  cmd->handle = handle;
}

static void drw_command_draw_range(
    DRWShadingGroup *shgroup, GPUBatch *batch, DRWResourceHandle handle, uint start, uint count)
{
  DRWCommandDrawRange *cmd = static_cast<DRWCommandDrawRange *>(
      drw_command_create(shgroup, DRW_CMD_DRAW_RANGE));
  cmd->batch = batch;
  cmd->handle = handle;
  cmd->vert_first = start;
  cmd->vert_count = count;
}

static void drw_command_draw_instance(
    DRWShadingGroup *shgroup, GPUBatch *batch, DRWResourceHandle handle, uint count, bool use_attr)
{
  DRWCommandDrawInstance *cmd = static_cast<DRWCommandDrawInstance *>(
      drw_command_create(shgroup, DRW_CMD_DRAW_INSTANCE));
  cmd->batch = batch;
  cmd->handle = handle;
  cmd->inst_count = count;
  cmd->use_attrs = use_attr;
}

static void drw_command_draw_intance_range(
    DRWShadingGroup *shgroup, GPUBatch *batch, DRWResourceHandle handle, uint start, uint count)
{
  DRWCommandDrawInstanceRange *cmd = static_cast<DRWCommandDrawInstanceRange *>(
      drw_command_create(shgroup, DRW_CMD_DRAW_INSTANCE_RANGE));
  cmd->batch = batch;
  cmd->handle = handle;
  cmd->inst_first = start;
  cmd->inst_count = count;
}

static void drw_command_compute(DRWShadingGroup *shgroup,
                                int groups_x_len,
                                int groups_y_len,
                                int groups_z_len)
{
  DRWCommandCompute *cmd = static_cast<DRWCommandCompute *>(
      drw_command_create(shgroup, DRW_CMD_COMPUTE));
  cmd->groups_x_len = groups_x_len;
  cmd->groups_y_len = groups_y_len;
  cmd->groups_z_len = groups_z_len;
}

static void drw_command_compute_ref(DRWShadingGroup *shgroup, int groups_ref[3])
{
  DRWCommandComputeRef *cmd = static_cast<DRWCommandComputeRef *>(
      drw_command_create(shgroup, DRW_CMD_COMPUTE_REF));
  cmd->groups_ref = groups_ref;
}

static void drw_command_compute_indirect(DRWShadingGroup *shgroup, GPUStorageBuf *indirect_buf)
{
  DRWCommandComputeIndirect *cmd = static_cast<DRWCommandComputeIndirect *>(
      drw_command_create(shgroup, DRW_CMD_COMPUTE_INDIRECT));
  cmd->indirect_buf = indirect_buf;
}

static void drw_command_barrier(DRWShadingGroup *shgroup, eGPUBarrier type)
{
  DRWCommandBarrier *cmd = static_cast<DRWCommandBarrier *>(
      drw_command_create(shgroup, DRW_CMD_BARRIER));
  cmd->type = type;
}

static void drw_command_draw_procedural(DRWShadingGroup *shgroup,
                                        GPUBatch *batch,
                                        DRWResourceHandle handle,
                                        uint vert_count)
{
  DRWCommandDrawProcedural *cmd = static_cast<DRWCommandDrawProcedural *>(
      drw_command_create(shgroup, DRW_CMD_DRAW_PROCEDURAL));
  cmd->batch = batch;
  cmd->handle = handle;
  cmd->vert_count = vert_count;
}

static void drw_command_draw_indirect(DRWShadingGroup *shgroup,
                                      GPUBatch *batch,
                                      DRWResourceHandle handle,
                                      GPUStorageBuf *indirect_buf)
{
  DRWCommandDrawIndirect *cmd = static_cast<DRWCommandDrawIndirect *>(
      drw_command_create(shgroup, DRW_CMD_DRAW_INDIRECT));
  cmd->batch = batch;
  cmd->handle = handle;
  cmd->indirect_buf = indirect_buf;
}

static void drw_command_set_select_id(DRWShadingGroup *shgroup, GPUVertBuf *buf, uint select_id)
{
  /* Only one can be valid. */
  BLI_assert(buf == nullptr || select_id == -1);
  DRWCommandSetSelectID *cmd = static_cast<DRWCommandSetSelectID *>(
      drw_command_create(shgroup, DRW_CMD_SELECTID));
  cmd->select_buf = buf;
  cmd->select_id = select_id;
}

static void drw_command_set_stencil_mask(DRWShadingGroup *shgroup,
                                         uint write_mask,
                                         uint reference,
                                         uint compare_mask)
{
  BLI_assert(write_mask <= 0xFF);
  BLI_assert(reference <= 0xFF);
  BLI_assert(compare_mask <= 0xFF);
  DRWCommandSetStencil *cmd = static_cast<DRWCommandSetStencil *>(
      drw_command_create(shgroup, DRW_CMD_STENCIL));
  cmd->write_mask = write_mask;
  cmd->comp_mask = compare_mask;
  cmd->ref = reference;
}

static void drw_command_clear(DRWShadingGroup *shgroup,
                              eGPUFrameBufferBits channels,
                              uchar r,
                              uchar g,
                              uchar b,
                              uchar a,
                              float depth,
                              uchar stencil)
{
  DRWCommandClear *cmd = static_cast<DRWCommandClear *>(
      drw_command_create(shgroup, DRW_CMD_CLEAR));
  cmd->clear_channels = channels;
  cmd->r = r;
  cmd->g = g;
  cmd->b = b;
  cmd->a = a;
  cmd->depth = depth;
  cmd->stencil = stencil;
}

static void drw_command_set_mutable_state(DRWShadingGroup *shgroup,
                                          DRWState enable,
                                          DRWState disable)
{
  /* TODO: Restrict what state can be changed. */
  DRWCommandSetMutableState *cmd = static_cast<DRWCommandSetMutableState *>(
      drw_command_create(shgroup, DRW_CMD_DRWSTATE));
  cmd->enable = enable;
  cmd->disable = disable;
}

void DRW_shgroup_call_ex(DRWShadingGroup *shgroup,
                         Object *ob,
                         float (*obmat)[4],
                         GPUBatch *geom,
                         bool bypass_culling,
                         void *user_data)
{
  BLI_assert(geom != nullptr);
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : obmat, ob);
  drw_command_draw(shgroup, geom, handle);

  /* Culling data. */
  if (user_data || bypass_culling) {
    DRWCullingState *culling = static_cast<DRWCullingState *>(
        DRW_memblock_elem_from_handle(DST.vmempool->cullstates, &DST.ob_handle));

    if (user_data) {
      culling->user_data = user_data;
    }
    if (bypass_culling) {
      /* NOTE: this will disable culling for the whole object. */
      culling->bsphere.radius = -1.0f;
    }
  }
}

void DRW_shgroup_call_range(
    DRWShadingGroup *shgroup, Object *ob, GPUBatch *geom, uint v_sta, uint v_num)
{
  BLI_assert(geom != nullptr);
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : nullptr, ob);
  drw_command_draw_range(shgroup, geom, handle, v_sta, v_num);
}

void DRW_shgroup_call_instance_range(
    DRWShadingGroup *shgroup, Object *ob, GPUBatch *geom, uint i_sta, uint i_num)
{
  BLI_assert(geom != nullptr);
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : nullptr, ob);
  drw_command_draw_intance_range(shgroup, geom, handle, i_sta, i_num);
}

void DRW_shgroup_call_compute(DRWShadingGroup *shgroup,
                              int groups_x_len,
                              int groups_y_len,
                              int groups_z_len)
{
  BLI_assert(groups_x_len > 0 && groups_y_len > 0 && groups_z_len > 0);
  BLI_assert(GPU_compute_shader_support());

  drw_command_compute(shgroup, groups_x_len, groups_y_len, groups_z_len);
}

void DRW_shgroup_call_compute_ref(DRWShadingGroup *shgroup, int groups_ref[3])
{
  BLI_assert(GPU_compute_shader_support());

  drw_command_compute_ref(shgroup, groups_ref);
}

void DRW_shgroup_call_compute_indirect(DRWShadingGroup *shgroup, GPUStorageBuf *indirect_buf)
{
  BLI_assert(GPU_compute_shader_support());

  drw_command_compute_indirect(shgroup, indirect_buf);
}

void DRW_shgroup_barrier(DRWShadingGroup *shgroup, eGPUBarrier type)
{
  BLI_assert(GPU_compute_shader_support());

  drw_command_barrier(shgroup, type);
}

static void drw_shgroup_call_procedural_add_ex(DRWShadingGroup *shgroup,
                                               GPUBatch *geom,
                                               Object *ob,
                                               uint vert_count)
{
  BLI_assert(vert_count > 0);
  BLI_assert(geom != nullptr);
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : nullptr, ob);
  drw_command_draw_procedural(shgroup, geom, handle, vert_count);
}

void DRW_shgroup_call_procedural_points(DRWShadingGroup *shgroup, Object *ob, uint point_count)
{
  GPUBatch *geom = drw_cache_procedural_points_get();
  drw_shgroup_call_procedural_add_ex(shgroup, geom, ob, point_count);
}

void DRW_shgroup_call_procedural_lines(DRWShadingGroup *shgroup, Object *ob, uint line_count)
{
  GPUBatch *geom = drw_cache_procedural_lines_get();
  drw_shgroup_call_procedural_add_ex(shgroup, geom, ob, line_count * 2);
}

void DRW_shgroup_call_procedural_triangles(DRWShadingGroup *shgroup, Object *ob, uint tri_count)
{
  GPUBatch *geom = drw_cache_procedural_triangles_get();
  drw_shgroup_call_procedural_add_ex(shgroup, geom, ob, tri_count * 3);
}

void DRW_shgroup_call_procedural_indirect(DRWShadingGroup *shgroup,
                                          GPUPrimType primitive_type,
                                          Object *ob,
                                          GPUStorageBuf *indirect_buf)
{
  GPUBatch *geom = nullptr;
  switch (primitive_type) {
    case GPU_PRIM_POINTS:
      geom = drw_cache_procedural_points_get();
      break;
    case GPU_PRIM_LINES:
      geom = drw_cache_procedural_lines_get();
      break;
    case GPU_PRIM_TRIS:
      geom = drw_cache_procedural_triangles_get();
      break;
    case GPU_PRIM_TRI_STRIP:
      geom = drw_cache_procedural_triangle_strips_get();
      break;
    default:
      BLI_assert_msg(0,
                     "Unsupported primitive type in DRW_shgroup_call_procedural_indirect. Add new "
                     "one as needed.");
      break;
  }
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : nullptr, ob);
  drw_command_draw_indirect(shgroup, geom, handle, indirect_buf);
}

void DRW_shgroup_call_instances(DRWShadingGroup *shgroup, Object *ob, GPUBatch *geom, uint count)
{
  BLI_assert(geom != nullptr);
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : nullptr, ob);
  drw_command_draw_instance(shgroup, geom, handle, count, false);
}

void DRW_shgroup_call_instances_with_attrs(DRWShadingGroup *shgroup,
                                           Object *ob,
                                           GPUBatch *geom,
                                           GPUBatch *inst_attributes)
{
  BLI_assert(geom != nullptr);
  BLI_assert(inst_attributes != nullptr);
  if (G.f & G_FLAG_PICKSEL) {
    drw_command_set_select_id(shgroup, nullptr, DST.select_id);
  }
  DRWResourceHandle handle = drw_resource_handle(shgroup, ob ? ob->object_to_world : nullptr, ob);
  GPUBatch *batch = DRW_temp_batch_instance_request(
      DST.vmempool->idatalist, nullptr, inst_attributes, geom);
  drw_command_draw_instance(shgroup, batch, handle, 0, true);
}

#define SCULPT_DEBUG_BUFFERS (G.debug_value == 889)
struct DRWSculptCallbackData {
  Object *ob;
  DRWShadingGroup **shading_groups;
  int num_shading_groups;
  bool use_wire;
  bool use_mats;
  bool use_mask;
  bool use_fsets;
  bool fast_mode; /* Set by draw manager. Do not init. */

  int debug_node_nr;
  PBVHAttrReq *attrs;
  int attrs_num;
};

#define SCULPT_DEBUG_COLOR(id) (sculpt_debug_colors[id % 9])
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

static void sculpt_draw_cb(DRWSculptCallbackData *scd,
                           PBVHBatches *batches,
                           PBVH_GPU_Args *pbvh_draw_args)
{
  if (!batches) {
    return;
  }

  int primcount;
  GPUBatch *geom;

  if (!scd->use_wire) {
    geom = DRW_pbvh_tris_get(
        batches, scd->attrs, scd->attrs_num, pbvh_draw_args, &primcount, scd->fast_mode);
  }
  else {
    geom = DRW_pbvh_lines_get(
        batches, scd->attrs, scd->attrs_num, pbvh_draw_args, &primcount, scd->fast_mode);
  }

  short index = 0;

  if (scd->use_mats) {
    index = drw_pbvh_material_index_get(batches);
    index = clamp_i(index, 0, scd->num_shading_groups - 1);
  }

  DRWShadingGroup *shgrp = scd->shading_groups[index];
  if (geom != nullptr && shgrp != nullptr) {
    if (SCULPT_DEBUG_BUFFERS) {
      /* Color each buffers in different colors. Only work in solid/X-ray mode. */
      shgrp = DRW_shgroup_create_sub(shgrp);
      DRW_shgroup_uniform_vec3(
          shgrp, "materialDiffuseColor", SCULPT_DEBUG_COLOR(scd->debug_node_nr++), 1);
    }

    /* DRW_shgroup_call_no_cull reuses matrices calculations for all the drawcalls of this
     * object. */
    DRW_shgroup_call_no_cull(shgrp, geom, scd->ob);
  }
}

void DRW_sculpt_debug_cb(
    PBVHNode *node, void *user_data, const float bmin[3], const float bmax[3], PBVHNodeFlags flag)
{
  int *debug_node_nr = (int *)user_data;
  BoundBox bb;
  BKE_boundbox_init_from_minmax(&bb, bmin, bmax);

#if 0 /* Nodes hierarchy. */
  if (flag & PBVH_Leaf) {
    DRW_debug_bbox(&bb, (float[4]){0.0f, 1.0f, 0.0f, 1.0f});
  }
  else {
    DRW_debug_bbox(&bb, (float[4]){0.5f, 0.5f, 0.5f, 0.6f});
  }
#else /* Color coded leaf bounds. */
  if (flag & (PBVH_Leaf | PBVH_TexLeaf)) {
    DRW_debug_bbox(&bb, SCULPT_DEBUG_COLOR((*debug_node_nr)++));
    int color = (*debug_node_nr)++;
    color += BKE_pbvh_debug_draw_gen_get(node);

    DRW_debug_bbox(&bb, SCULPT_DEBUG_COLOR(color));
  }
#endif
}

static void drw_sculpt_get_frustum_planes(Object *ob, float planes[6][4])
{
  /* TODO: take into account partial redraw for clipping planes. */
  DRW_view_frustum_planes_get(DRW_view_default_get(), planes);

  /* Transform clipping planes to object space. Transforming a plane with a
   * 4x4 matrix is done by multiplying with the transpose inverse.
   * The inverse cancels out here since we transform by inverse(obmat). */
  float tmat[4][4];
  transpose_m4_m4(tmat, ob->object_to_world);
  for (int i = 0; i < 6; i++) {
    mul_m4_v4(tmat, planes[i]);
  }
}

static void drw_sculpt_generate_calls(DRWSculptCallbackData *scd)
{
  /* PBVH should always exist for non-empty meshes, created by depsgraph eval. */
  PBVH *pbvh = (scd->ob->sculpt) ? scd->ob->sculpt->pbvh : nullptr;
  if (!pbvh) {
    return;
  }

  const DRWContextState *drwctx = DRW_context_state_get();
  RegionView3D *rv3d = drwctx->rv3d;
  const bool navigating = rv3d && (rv3d->rflag & RV3D_NAVIGATING);

  Paint *p = nullptr;
  if (drwctx->evil_C != nullptr) {
    p = BKE_paint_get_active_from_context(drwctx->evil_C);
  }

  /* Frustum planes to show only visible PBVH nodes. */
  float update_planes[6][4];
  float draw_planes[6][4];
  PBVHFrustumPlanes update_frustum;
  PBVHFrustumPlanes draw_frustum;

  if (p && (p->flags & PAINT_SCULPT_DELAY_UPDATES)) {
    update_frustum.planes = update_planes;
    update_frustum.num_planes = 6;
    BKE_pbvh_get_frustum_planes(pbvh, &update_frustum);
    if (!navigating) {
      drw_sculpt_get_frustum_planes(scd->ob, update_planes);
      update_frustum.planes = update_planes;
      update_frustum.num_planes = 6;
      BKE_pbvh_set_frustum_planes(pbvh, &update_frustum);
    }
  }
  else {
    drw_sculpt_get_frustum_planes(scd->ob, update_planes);
    update_frustum.planes = update_planes;
    update_frustum.num_planes = 6;
  }

  drw_sculpt_get_frustum_planes(scd->ob, draw_planes);
  draw_frustum.planes = draw_planes;
  draw_frustum.num_planes = 6;

  /* Fast mode to show low poly multires while navigating. */
  scd->fast_mode = false;
  if (p && (p->flags & PAINT_FAST_NAVIGATE)) {
    scd->fast_mode = rv3d && (rv3d->rflag & RV3D_NAVIGATING);
  }

  /* Update draw buffers only for visible nodes while painting.
   * But do update them otherwise so navigating stays smooth. */
  bool update_only_visible = rv3d && !(rv3d->rflag & RV3D_PAINTING);
  if (p && (p->flags & PAINT_SCULPT_DELAY_UPDATES)) {
    update_only_visible = true;
  }

  Mesh *mesh = static_cast<Mesh *>(scd->ob->data);
  BKE_pbvh_update_normals(pbvh, mesh->runtime->subdiv_ccg);

  BKE_pbvh_draw_cb(pbvh,
                   update_only_visible,
                   &update_frustum,
                   &draw_frustum,
                   (void (*)(void *, PBVHBatches *, PBVH_GPU_Args *))sculpt_draw_cb,
                   scd,
                   scd->use_mats,
                   scd->attrs,
                   scd->attrs_num);

  if (SCULPT_DEBUG_BUFFERS) {
    int debug_node_nr = 0;
    DRW_debug_modelmat(scd->ob->object_to_world);
    BKE_pbvh_draw_debug_cb(
        pbvh,
        (void (*)(PBVHNode * n, void *d, const float min[3], const float max[3], PBVHNodeFlags f))
            DRW_sculpt_debug_cb,
        &debug_node_nr);
  }
}

void DRW_shgroup_call_sculpt(DRWShadingGroup *shgroup,
                             Object *ob,
                             bool use_wire,
                             bool use_mask,
                             bool use_fset,
                             bool use_color,
                             bool use_uv)
{
  DRWSculptCallbackData scd{};
  scd.ob = ob;
  scd.shading_groups = &shgroup;
  scd.num_shading_groups = 1;
  scd.use_wire = use_wire;
  scd.use_mats = false;
  scd.use_mask = use_mask;

  PBVHAttrReq attrs[16];
  int attrs_num = 0;

  memset(attrs, 0, sizeof(attrs));

  /* NOTE: these are NOT #eCustomDataType, they are extended values, ASAN may warn about this. */
  attrs[attrs_num++].type = (eCustomDataType)CD_PBVH_CO_TYPE;
  attrs[attrs_num++].type = (eCustomDataType)CD_PBVH_NO_TYPE;

  if (use_mask) {
    attrs[attrs_num++].type = (eCustomDataType)CD_PBVH_MASK_TYPE;
  }

  if (use_fset) {
    attrs[attrs_num++].type = (eCustomDataType)CD_PBVH_FSET_TYPE;
  }

  Mesh *me = BKE_object_get_original_mesh(ob);

  if (use_color) {
    const CustomDataLayer *layer = BKE_id_attributes_color_find(&me->id,
                                                                me->active_color_attribute);
    if (layer) {
      eAttrDomain domain = BKE_id_attribute_domain(&me->id, layer);

      attrs[attrs_num].type = eCustomDataType(layer->type);
      attrs[attrs_num].domain = domain;

      STRNCPY(attrs[attrs_num].name, layer->name);
      attrs_num++;
    }
  }

  if (use_uv) {
    int layer_i = CustomData_get_active_layer_index(&me->loop_data, CD_PROP_FLOAT2);
    if (layer_i != -1) {
      CustomDataLayer *layer = me->loop_data.layers + layer_i;

      attrs[attrs_num].type = CD_PROP_FLOAT2;
      attrs[attrs_num].domain = ATTR_DOMAIN_CORNER;
      STRNCPY(attrs[attrs_num].name, layer->name);

      attrs_num++;
    }
  }

  scd.attrs = attrs;
  scd.attrs_num = attrs_num;

  drw_sculpt_generate_calls(&scd);
}

void DRW_shgroup_call_sculpt_with_materials(DRWShadingGroup **shgroups,
                                            GPUMaterial **gpumats,
                                            int num_shgroups,
                                            Object *ob)
{
  DRW_Attributes draw_attrs;
  DRW_MeshCDMask cd_needed;

  if (gpumats) {
    DRW_mesh_get_attributes(ob, (Mesh *)ob->data, gpumats, num_shgroups, &draw_attrs, &cd_needed);
  }
  else {
    memset(&draw_attrs, 0, sizeof(draw_attrs));
    memset(&cd_needed, 0, sizeof(cd_needed));
  }

  int attrs_num = 2 + draw_attrs.num_requests;

  /* UV maps are not in attribute requests. */
  attrs_num += count_bits_i(cd_needed.uv);

  blender::Array<PBVHAttrReq, 16> attrs(attrs_num, PBVHAttrReq{});

  int attrs_i = 0;

  /* NOTE: these are NOT #eCustomDataType, they are extended values, ASAN may warn about this. */
  attrs[attrs_i++].type = (eCustomDataType)CD_PBVH_CO_TYPE;
  attrs[attrs_i++].type = (eCustomDataType)CD_PBVH_NO_TYPE;

  for (int i = 0; i < draw_attrs.num_requests; i++) {
    DRW_AttributeRequest *req = draw_attrs.requests + i;

    attrs[attrs_i].type = req->cd_type;
    attrs[attrs_i].domain = req->domain;
    STRNCPY(attrs[attrs_i].name, req->attribute_name);
    attrs_i++;
  }

  /* UV maps are not in attribute requests. */
  Mesh *me = (Mesh *)ob->data;

  for (uint i = 0; i < 32; i++) {
    if (cd_needed.uv & (1 << i)) {
      int layer_i = CustomData_get_layer_index_n(&me->loop_data, CD_PROP_FLOAT2, i);
      CustomDataLayer *layer = layer_i != -1 ? me->loop_data.layers + layer_i : nullptr;

      if (layer) {
        attrs[attrs_i].type = CD_PROP_FLOAT2;
        attrs[attrs_i].domain = ATTR_DOMAIN_CORNER;
        STRNCPY(attrs[attrs_i].name, layer->name);
        attrs_i++;
      }
    }
  }

  attrs_num = attrs_i;

  DRWSculptCallbackData scd{};
  scd.ob = ob;
  scd.shading_groups = shgroups;
  scd.num_shading_groups = num_shgroups;
  scd.use_wire = false;
  scd.use_mats = true;
  scd.use_mask = false;
  scd.attrs = attrs.data();
  scd.attrs_num = attrs_num;

  drw_sculpt_generate_calls(&scd);
}

static GPUVertFormat inst_select_format = {0};

DRWCallBuffer *DRW_shgroup_call_buffer(DRWShadingGroup *shgroup,
                                       GPUVertFormat *format,
                                       GPUPrimType prim_type)
{
  BLI_assert(ELEM(prim_type, GPU_PRIM_POINTS, GPU_PRIM_LINES, GPU_PRIM_TRI_FAN));
  BLI_assert(format != nullptr);

  DRWCallBuffer *callbuf = static_cast<DRWCallBuffer *>(
      BLI_memblock_alloc(DST.vmempool->callbuffers));
  callbuf->buf = DRW_temp_buffer_request(DST.vmempool->idatalist, format, &callbuf->count);
  callbuf->buf_select = nullptr;
  callbuf->count = 0;

  if (G.f & G_FLAG_PICKSEL) {
    /* Not actually used for rendering but allocated in one chunk. */
    if (inst_select_format.attr_len == 0) {
      GPU_vertformat_attr_add(&inst_select_format, "selectId", GPU_COMP_I32, 1, GPU_FETCH_INT);
    }
    callbuf->buf_select = DRW_temp_buffer_request(
        DST.vmempool->idatalist, &inst_select_format, &callbuf->count);
    drw_command_set_select_id(shgroup, callbuf->buf_select, -1);
  }

  DRWResourceHandle handle = drw_resource_handle(shgroup, nullptr, nullptr);
  GPUBatch *batch = DRW_temp_batch_request(DST.vmempool->idatalist, callbuf->buf, prim_type);
  drw_command_draw(shgroup, batch, handle);

  return callbuf;
}

DRWCallBuffer *DRW_shgroup_call_buffer_instance(DRWShadingGroup *shgroup,
                                                GPUVertFormat *format,
                                                GPUBatch *geom)
{
  BLI_assert(geom != nullptr);
  BLI_assert(format != nullptr);

  DRWCallBuffer *callbuf = static_cast<DRWCallBuffer *>(
      BLI_memblock_alloc(DST.vmempool->callbuffers));
  callbuf->buf = DRW_temp_buffer_request(DST.vmempool->idatalist, format, &callbuf->count);
  callbuf->buf_select = nullptr;
  callbuf->count = 0;

  if (G.f & G_FLAG_PICKSEL) {
    /* Not actually used for rendering but allocated in one chunk. */
    if (inst_select_format.attr_len == 0) {
      GPU_vertformat_attr_add(&inst_select_format, "selectId", GPU_COMP_I32, 1, GPU_FETCH_INT);
    }
    callbuf->buf_select = DRW_temp_buffer_request(
        DST.vmempool->idatalist, &inst_select_format, &callbuf->count);
    drw_command_set_select_id(shgroup, callbuf->buf_select, -1);
  }

  DRWResourceHandle handle = drw_resource_handle(shgroup, nullptr, nullptr);
  GPUBatch *batch = DRW_temp_batch_instance_request(
      DST.vmempool->idatalist, callbuf->buf, nullptr, geom);
  drw_command_draw(shgroup, batch, handle);

  return callbuf;
}

void DRW_buffer_add_entry_struct(DRWCallBuffer *callbuf, const void *data)
{
  GPUVertBuf *buf = callbuf->buf;
  const bool resize = (callbuf->count == GPU_vertbuf_get_vertex_alloc(buf));

  if (UNLIKELY(resize)) {
    GPU_vertbuf_data_resize(buf, callbuf->count + DRW_BUFFER_VERTS_CHUNK);
  }

  GPU_vertbuf_vert_set(buf, callbuf->count, data);

  if (G.f & G_FLAG_PICKSEL) {
    if (UNLIKELY(resize)) {
      GPU_vertbuf_data_resize(callbuf->buf_select, callbuf->count + DRW_BUFFER_VERTS_CHUNK);
    }
    GPU_vertbuf_attr_set(callbuf->buf_select, 0, callbuf->count, &DST.select_id);
  }

  callbuf->count++;
}

void DRW_buffer_add_entry_array(DRWCallBuffer *callbuf, const void *attr[], uint attr_len)
{
  GPUVertBuf *buf = callbuf->buf;
  const bool resize = (callbuf->count == GPU_vertbuf_get_vertex_alloc(buf));

  BLI_assert(attr_len == GPU_vertbuf_get_format(buf)->attr_len);
  UNUSED_VARS_NDEBUG(attr_len);

  if (UNLIKELY(resize)) {
    GPU_vertbuf_data_resize(buf, callbuf->count + DRW_BUFFER_VERTS_CHUNK);
  }

  for (int i = 0; i < attr_len; i++) {
    GPU_vertbuf_attr_set(buf, i, callbuf->count, attr[i]);
  }

  if (G.f & G_FLAG_PICKSEL) {
    if (UNLIKELY(resize)) {
      GPU_vertbuf_data_resize(callbuf->buf_select, callbuf->count + DRW_BUFFER_VERTS_CHUNK);
    }
    GPU_vertbuf_attr_set(callbuf->buf_select, 0, callbuf->count, &DST.select_id);
  }

  callbuf->count++;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shading Groups (DRW_shgroup)
 * \{ */

static void drw_shgroup_init(DRWShadingGroup *shgroup, GPUShader *shader)
{
  shgroup->uniforms = nullptr;
  shgroup->uniform_attrs = nullptr;

  int clipping_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_DRW_CLIPPING);
  int view_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_VIEW);
  int model_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_MODEL);
  int info_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_INFO);
  int baseinst_location = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_BASE_INSTANCE);
  int chunkid_location = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_RESOURCE_CHUNK);
  int resourceid_location = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_RESOURCE_ID);

  /* TODO(@fclem): Will take the place of the above after the GPUShaderCreateInfo port. */
  if (view_ubo_location == -1) {
    view_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_DRW_VIEW);
  }
  if (model_ubo_location == -1) {
    model_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_DRW_MODEL);
  }
  if (info_ubo_location == -1) {
    info_ubo_location = GPU_shader_get_builtin_block(shader, GPU_UNIFORM_BLOCK_DRW_INFOS);
  }

  if (chunkid_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  chunkid_location,
                                  DRW_UNIFORM_RESOURCE_CHUNK,
                                  nullptr,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);
  }

  if (resourceid_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  resourceid_location,
                                  DRW_UNIFORM_RESOURCE_ID,
                                  nullptr,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);
  }

  if (baseinst_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  baseinst_location,
                                  DRW_UNIFORM_BASE_INSTANCE,
                                  nullptr,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);
  }

  if (model_ubo_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  model_ubo_location,
                                  DRW_UNIFORM_BLOCK_OBMATS,
                                  nullptr,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);
  }
  else {
    /* NOTE: This is only here to support old hardware fallback where uniform buffer is still
     * too slow or buggy. */
    int model = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODEL);
    int modelinverse = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODEL_INV);
    if (model != -1) {
      drw_shgroup_uniform_create_ex(shgroup,
                                    model,
                                    DRW_UNIFORM_MODEL_MATRIX,
                                    nullptr,
                                    GPUSamplerState::default_sampler(),
                                    0,
                                    1);
    }
    if (modelinverse != -1) {
      drw_shgroup_uniform_create_ex(shgroup,
                                    modelinverse,
                                    DRW_UNIFORM_MODEL_MATRIX_INVERSE,
                                    nullptr,
                                    GPUSamplerState::default_sampler(),
                                    0,
                                    1);
    }
  }

  if (info_ubo_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  info_ubo_location,
                                  DRW_UNIFORM_BLOCK_OBINFOS,
                                  nullptr,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);

    /* Abusing this loc to tell shgroup we need the obinfos. */
    shgroup->objectinfo = 1;
  }
  else {
    shgroup->objectinfo = 0;
  }

  if (view_ubo_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  view_ubo_location,
                                  DRW_UNIFORM_BLOCK,
                                  G_draw.view_ubo,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);
  }

  if (clipping_ubo_location != -1) {
    drw_shgroup_uniform_create_ex(shgroup,
                                  clipping_ubo_location,
                                  DRW_UNIFORM_BLOCK,
                                  G_draw.clipping_ubo,
                                  GPUSamplerState::default_sampler(),
                                  0,
                                  1);
  }

  /* Not supported. */
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW_INV) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_NORMAL) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_VIEW) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_VIEW_INV) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_VIEWPROJECTION) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_VIEWPROJECTION_INV) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_PROJECTION) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_PROJECTION_INV) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_CLIPPLANES) == -1);
  BLI_assert(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MVP) == -1);
}

static DRWShadingGroup *drw_shgroup_create_ex(GPUShader *shader, DRWPass *pass)
{
  DRWShadingGroup *shgroup = static_cast<DRWShadingGroup *>(
      BLI_memblock_alloc(DST.vmempool->shgroups));

  BLI_LINKS_APPEND(&pass->shgroups, shgroup);

  shgroup->shader = shader;
  shgroup->cmd.first = nullptr;
  shgroup->cmd.last = nullptr;
  shgroup->pass_handle = pass->handle;

  return shgroup;
}

static DRWShadingGroup *drw_shgroup_material_create_ex(GPUPass *gpupass, DRWPass *pass)
{
  if (!gpupass) {
    /* Shader compilation error */
    return nullptr;
  }

  GPUShader *sh = GPU_pass_shader_get(gpupass);

  if (!sh) {
    /* Shader not yet compiled */
    return nullptr;
  }

  DRWShadingGroup *grp = drw_shgroup_create_ex(sh, pass);
  return grp;
}

static void drw_shgroup_material_texture(DRWShadingGroup *grp,
                                         GPUTexture *gputex,
                                         const char *name,
                                         GPUSamplerState state)
{
  DRW_shgroup_uniform_texture_ex(grp, name, gputex, state);

  GPUTexture **gputex_ref = static_cast<GPUTexture **>(BLI_memblock_alloc(DST.vmempool->images));
  *gputex_ref = gputex;
  GPU_texture_ref(gputex);
}

void DRW_shgroup_add_material_resources(DRWShadingGroup *grp, GPUMaterial *material)
{
  ListBase textures = GPU_material_textures(material);

  /* Bind all textures needed by the material. */
  LISTBASE_FOREACH (GPUMaterialTexture *, tex, &textures) {
    if (tex->ima) {
      /* Image */
      GPUTexture *gputex;
      ImageUser *iuser = tex->iuser_available ? &tex->iuser : nullptr;
      if (tex->tiled_mapping_name[0]) {
        gputex = BKE_image_get_gpu_tiles(tex->ima, iuser, nullptr);
        drw_shgroup_material_texture(grp, gputex, tex->sampler_name, tex->sampler_state);
        gputex = BKE_image_get_gpu_tilemap(tex->ima, iuser, nullptr);
        drw_shgroup_material_texture(grp, gputex, tex->tiled_mapping_name, tex->sampler_state);
      }
      else {
        gputex = BKE_image_get_gpu_texture(tex->ima, iuser, nullptr);
        drw_shgroup_material_texture(grp, gputex, tex->sampler_name, tex->sampler_state);
      }
    }
    else if (tex->colorband) {
      /* Color Ramp */
      DRW_shgroup_uniform_texture(grp, tex->sampler_name, *tex->colorband);
    }
    else if (tex->sky) {
      /* Sky */
      DRW_shgroup_uniform_texture_ex(grp, tex->sampler_name, *tex->sky, tex->sampler_state);
    }
  }

  GPUUniformBuf *ubo = GPU_material_uniform_buffer_get(material);
  if (ubo != nullptr) {
    DRW_shgroup_uniform_block(grp, GPU_UBO_BLOCK_NAME, ubo);
  }

  const GPUUniformAttrList *uattrs = GPU_material_uniform_attributes(material);
  if (uattrs != nullptr) {
    int loc = GPU_shader_get_ubo_binding(grp->shader, GPU_ATTRIBUTE_UBO_BLOCK_NAME);
    drw_shgroup_uniform_create_ex(
        grp, loc, DRW_UNIFORM_BLOCK_OBATTRS, uattrs, GPUSamplerState::default_sampler(), 0, 1);
    grp->uniform_attrs = uattrs;
  }

  if (GPU_material_layer_attributes(material) != nullptr) {
    int loc = GPU_shader_get_ubo_binding(grp->shader, GPU_LAYER_ATTRIBUTE_UBO_BLOCK_NAME);
    drw_shgroup_uniform_create_ex(
        grp, loc, DRW_UNIFORM_BLOCK_VLATTRS, nullptr, GPUSamplerState::default_sampler(), 0, 1);
  }
}

GPUVertFormat *DRW_shgroup_instance_format_array(const DRWInstanceAttrFormat attrs[],
                                                 int arraysize)
{
  GPUVertFormat *format = MEM_cnew<GPUVertFormat>(__func__);

  for (int i = 0; i < arraysize; i++) {
    GPU_vertformat_attr_add(format,
                            attrs[i].name,
                            (attrs[i].type == DRW_ATTR_INT) ? GPU_COMP_I32 : GPU_COMP_F32,
                            attrs[i].components,
                            (attrs[i].type == DRW_ATTR_INT) ? GPU_FETCH_INT : GPU_FETCH_FLOAT);
  }
  return format;
}

DRWShadingGroup *DRW_shgroup_material_create(GPUMaterial *material, DRWPass *pass)
{
  GPUPass *gpupass = GPU_material_get_pass(material);
  DRWShadingGroup *shgroup = drw_shgroup_material_create_ex(gpupass, pass);

  if (shgroup) {
    drw_shgroup_init(shgroup, GPU_pass_shader_get(gpupass));
    DRW_shgroup_add_material_resources(shgroup, material);
  }
  return shgroup;
}

DRWShadingGroup *DRW_shgroup_create(GPUShader *shader, DRWPass *pass)
{
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  drw_shgroup_init(shgroup, shader);
  return shgroup;
}

DRWShadingGroup *DRW_shgroup_transform_feedback_create(GPUShader *shader,
                                                       DRWPass *pass,
                                                       GPUVertBuf *tf_target)
{
  BLI_assert(tf_target != nullptr);
  DRWShadingGroup *shgroup = drw_shgroup_create_ex(shader, pass);
  drw_shgroup_init(shgroup, shader);
  drw_shgroup_uniform_create_ex(shgroup,
                                0,
                                DRW_UNIFORM_TFEEDBACK_TARGET,
                                tf_target,
                                GPUSamplerState::default_sampler(),
                                0,
                                1);
  return shgroup;
}

void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state)
{
  drw_command_set_mutable_state(shgroup, state, DRW_STATE_NO_DRAW);
}

void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state)
{
  drw_command_set_mutable_state(shgroup, DRW_STATE_NO_DRAW, state);
}

void DRW_shgroup_stencil_set(DRWShadingGroup *shgroup,
                             uint write_mask,
                             uint reference,
                             uint compare_mask)
{
  drw_command_set_stencil_mask(shgroup, write_mask, reference, compare_mask);
}

void DRW_shgroup_stencil_mask(DRWShadingGroup *shgroup, uint mask)
{
  drw_command_set_stencil_mask(shgroup, 0xFF, mask, 0xFF);
}

void DRW_shgroup_clear_framebuffer(DRWShadingGroup *shgroup,
                                   eGPUFrameBufferBits channels,
                                   uchar r,
                                   uchar g,
                                   uchar b,
                                   uchar a,
                                   float depth,
                                   uchar stencil)
{
  drw_command_clear(shgroup, channels, r, g, b, a, depth, stencil);
}

bool DRW_shgroup_is_empty(DRWShadingGroup *shgroup)
{
  DRWCommandChunk *chunk = shgroup->cmd.first;
  for (; chunk; chunk = chunk->next) {
    for (int i = 0; i < chunk->command_used; i++) {
      if (command_type_get(chunk->command_type, i) <= DRW_MAX_DRAW_CMD_TYPE) {
        return false;
      }
    }
  }
  return true;
}

DRWShadingGroup *DRW_shgroup_create_sub(DRWShadingGroup *shgroup)
{
  DRWShadingGroup *shgroup_new = static_cast<DRWShadingGroup *>(
      BLI_memblock_alloc(DST.vmempool->shgroups));

  *shgroup_new = *shgroup;
  drw_shgroup_init(shgroup_new, shgroup_new->shader);
  shgroup_new->cmd.first = nullptr;
  shgroup_new->cmd.last = nullptr;

  DRWPass *parent_pass = static_cast<DRWPass *>(
      DRW_memblock_elem_from_handle(DST.vmempool->passes, &shgroup->pass_handle));

  BLI_LINKS_INSERT_AFTER(&parent_pass->shgroups, shgroup, shgroup_new);

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

static void draw_frustum_culling_planes_calc(const float (*persmat)[4], float (*frustum_planes)[4])
{
  planes_from_projmat(persmat,
                      frustum_planes[0],
                      frustum_planes[5],
                      frustum_planes[1],
                      frustum_planes[3],
                      frustum_planes[4],
                      frustum_planes[2]);

  /* Normalize. */
  for (int p = 0; p < 6; p++) {
    frustum_planes[p][3] /= normalize_v3(frustum_planes[p]);
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
    float farxy[2];              /* far-point projection onto the near plane */
    float farpoint[3] = {0.0f};  /* most extreme far point in camera coordinate */
    float nearpoint[3];          /* most extreme near point in camera coordinate */
    float farcenter[3] = {0.0f}; /* center of far clipping plane in camera coordinate */
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

    /* For XR, the view matrix may contain a scale factor. Then, transforming only the center
     * into world space after calculating the radius will result in incorrect behavior. */
    mul_m4_v3(viewinv, bsphere->center); /* Transform to world space. */
    mul_m4_v3(viewinv, farpoint);
    bsphere->radius = len_v3v3(bsphere->center, farpoint);
  }
}

static void draw_view_matrix_state_update(DRWView *view,
                                          const float viewmat[4][4],
                                          const float winmat[4][4])
{
  ViewMatrices *storage = &view->storage;

  copy_m4_m4(storage->viewmat.ptr(), viewmat);
  invert_m4_m4(storage->viewinv.ptr(), storage->viewmat.ptr());

  copy_m4_m4(storage->winmat.ptr(), winmat);
  invert_m4_m4(storage->wininv.ptr(), storage->winmat.ptr());

  mul_m4_m4m4(view->persmat.ptr(), winmat, viewmat);
  invert_m4_m4(view->persinv.ptr(), view->persmat.ptr());
}

DRWView *DRW_view_create(const float viewmat[4][4],
                         const float winmat[4][4],
                         const float (*culling_viewmat)[4],
                         const float (*culling_winmat)[4],
                         DRWCallVisibilityFn *visibility_fn)
{
  DRWView *view = static_cast<DRWView *>(BLI_memblock_alloc(DST.vmempool->views));

  if (DST.primary_view_num < MAX_CULLED_VIEWS) {
    view->culling_mask = 1u << DST.primary_view_num++;
  }
  else {
    BLI_assert(0);
    view->culling_mask = 0u;
  }
  view->clip_planes_len = 0;
  view->visibility_fn = visibility_fn;
  view->parent = nullptr;

  DRW_view_update(view, viewmat, winmat, culling_viewmat, culling_winmat);

  return view;
}

DRWView *DRW_view_create_sub(const DRWView *parent_view,
                             const float viewmat[4][4],
                             const float winmat[4][4])
{
  /* Search original parent. */
  const DRWView *ori_view = parent_view;
  while (ori_view->parent != nullptr) {
    ori_view = ori_view->parent;
  }

  DRWView *view = static_cast<DRWView *>(BLI_memblock_alloc(DST.vmempool->views));

  /* Perform copy. */
  *view = *ori_view;
  view->parent = (DRWView *)ori_view;

  DRW_view_update_sub(view, viewmat, winmat);

  return view;
}

/* DRWView Update:
 * This is meant to be done on existing views when rendering in a loop and there is no
 * need to allocate more DRWViews. */

void DRW_view_update_sub(DRWView *view, const float viewmat[4][4], const float winmat[4][4])
{
  BLI_assert(view->parent != nullptr);

  view->is_dirty = true;
  view->is_inverted = (is_negative_m4(viewmat) == is_negative_m4(winmat));

  draw_view_matrix_state_update(view, viewmat, winmat);
}

void DRW_view_update(DRWView *view,
                     const float viewmat[4][4],
                     const float winmat[4][4],
                     const float (*culling_viewmat)[4],
                     const float (*culling_winmat)[4])
{
  /* DO NOT UPDATE THE DEFAULT VIEW.
   * Create sub-views instead, or a copy. */
  BLI_assert(view != DST.view_default);
  BLI_assert(view->parent == nullptr);

  view->is_dirty = true;
  view->is_inverted = (is_negative_m4(viewmat) == is_negative_m4(winmat));

  draw_view_matrix_state_update(view, viewmat, winmat);

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
    copy_m4_m4(wininv, view->storage.wininv.ptr());
  }

  float viewinv[4][4];
  if (culling_viewmat) {
    viewmat = culling_viewmat;
    invert_m4_m4(viewinv, viewmat);
  }
  else {
    copy_m4_m4(viewinv, view->storage.viewinv.ptr());
  }

  draw_frustum_boundbox_calc(viewinv, winmat, &view->frustum_corners);
  draw_frustum_culling_planes_calc(view->persmat.ptr(), view->frustum_planes);
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

const DRWView *DRW_view_default_get()
{
  return DST.view_default;
}

void DRW_view_reset()
{
  DST.view_default = nullptr;
  DST.view_active = nullptr;
  DST.view_previous = nullptr;
}

void DRW_view_default_set(const DRWView *view)
{
  BLI_assert(DST.view_default == nullptr);
  DST.view_default = (DRWView *)view;
}

void DRW_view_clip_planes_set(DRWView *view, float (*planes)[4], int plane_len)
{
  BLI_assert(plane_len <= MAX_CLIP_PLANES);
  view->clip_planes_len = plane_len;
  if (plane_len > 0) {
    memcpy(view->clip_planes, planes, sizeof(float[4]) * plane_len);
  }
}

void DRW_view_frustum_corners_get(const DRWView *view, BoundBox *corners)
{
  memcpy(corners, &view->frustum_corners, sizeof(view->frustum_corners));
}

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
  const float4x4 &projmat = view->storage.winmat;

  if (DRW_view_is_persp_get(view)) {
    return -projmat[3][2] / (projmat[2][2] - 1.0f);
  }

  return -(projmat[3][2] + 1.0f) / projmat[2][2];
}

float DRW_view_far_distance_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  const float4x4 &projmat = view->storage.winmat;

  if (DRW_view_is_persp_get(view)) {
    return -projmat[3][2] / (projmat[2][2] + 1.0f);
  }

  return -(projmat[3][2] - 1.0f) / projmat[2][2];
}

void DRW_view_viewmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const ViewMatrices *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->viewinv.ptr() : storage->viewmat.ptr());
}

void DRW_view_winmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const ViewMatrices *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->wininv.ptr() : storage->winmat.ptr());
}

void DRW_view_persmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  copy_m4_m4(mat, (inverse) ? view->persinv.ptr() : view->persmat.ptr());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Passes (DRW_pass)
 * \{ */

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
  DRWPass *pass = static_cast<DRWPass *>(BLI_memblock_alloc(DST.vmempool->passes));
  pass->state = state | DRW_STATE_PROGRAM_POINT_SIZE;
  if (G.debug & G_DEBUG_GPU) {
    STRNCPY(pass->name, name);
  }

  pass->shgroups.first = nullptr;
  pass->shgroups.last = nullptr;
  pass->handle = DST.pass_handle;
  DRW_handle_increment(&DST.pass_handle);

  pass->original = nullptr;
  pass->next = nullptr;

  return pass;
}

DRWPass *DRW_pass_create_instance(const char *name, DRWPass *original, DRWState state)
{
  DRWPass *pass = DRW_pass_create(name, state);
  pass->original = original;

  return pass;
}

void DRW_pass_link(DRWPass *first, DRWPass *second)
{
  BLI_assert(first != second);
  BLI_assert(first->next == nullptr);
  first->next = second;
}

bool DRW_pass_is_empty(DRWPass *pass)
{
  if (pass->original) {
    return DRW_pass_is_empty(pass->original);
  }

  LISTBASE_FOREACH (DRWShadingGroup *, shgroup, &pass->shgroups) {
    if (!DRW_shgroup_is_empty(shgroup)) {
      return false;
    }
  }
  return true;
}

void DRW_pass_foreach_shgroup(DRWPass *pass,
                              void (*callback)(void *user_data, DRWShadingGroup *shgrp),
                              void *user_data)
{
  LISTBASE_FOREACH (DRWShadingGroup *, shgroup, &pass->shgroups) {
    callback(user_data, shgroup);
  }
}

static int pass_shgroup_dist_sort(const void *a, const void *b)
{
  const DRWShadingGroup *shgrp_a = (const DRWShadingGroup *)a;
  const DRWShadingGroup *shgrp_b = (const DRWShadingGroup *)b;

  if (shgrp_a->z_sorting.distance < shgrp_b->z_sorting.distance) {
    return 1;
  }
  if (shgrp_a->z_sorting.distance > shgrp_b->z_sorting.distance) {
    return -1;
  }

  /* If distances are the same, keep original order. */
  if (shgrp_a->z_sorting.original_index > shgrp_b->z_sorting.original_index) {
    return -1;
  }

  return 0;
}

/* ------------------ Shading group sorting --------------------- */

#define SORT_IMPL_LINKTYPE DRWShadingGroup

#define SORT_IMPL_FUNC shgroup_sort_fn_r
#include "../../blenlib/intern/list_sort_impl.h"
#undef SORT_IMPL_FUNC

#undef SORT_IMPL_LINKTYPE

void DRW_pass_sort_shgroup_z(DRWPass *pass)
{
  const float4x4 &viewinv = DST.view_active->storage.viewinv;

  if (!(pass->shgroups.first && pass->shgroups.first->next)) {
    /* Nothing to sort */
    return;
  }

  uint index = 0;
  DRWShadingGroup *shgroup = pass->shgroups.first;
  do {
    DRWResourceHandle handle = 0;
    /* Find first DRWCommandDraw. */
    DRWCommandChunk *cmd_chunk = shgroup->cmd.first;
    for (; cmd_chunk && handle == 0; cmd_chunk = cmd_chunk->next) {
      for (int i = 0; i < cmd_chunk->command_used && handle == 0; i++) {
        if (DRW_CMD_DRAW == command_type_get(cmd_chunk->command_type, i)) {
          handle = cmd_chunk->commands[i].draw.handle;
        }
      }
    }
    /* To be sorted a shgroup needs to have at least one draw command. */
    /* FIXME(fclem): In some case, we can still have empty shading group to sort. However their
     * final order is not well defined.
     * (see #76730 & D7729). */
    // BLI_assert(handle != 0);

    DRWObjectMatrix *obmats = static_cast<DRWObjectMatrix *>(
        DRW_memblock_elem_from_handle(DST.vmempool->obmats, &handle));

    /* Compute distance to camera. */
    float tmp[3];
    sub_v3_v3v3(tmp, viewinv[3], obmats->model[3]);
    shgroup->z_sorting.distance = dot_v3v3(viewinv[2], tmp);
    shgroup->z_sorting.original_index = index++;

  } while ((shgroup = shgroup->next));

  /* Sort using computed distances. */
  pass->shgroups.first = shgroup_sort_fn_r(pass->shgroups.first, pass_shgroup_dist_sort);

  /* Find the new last */
  DRWShadingGroup *last = pass->shgroups.first;
  while ((last = last->next)) {
    /* Reset the pass id for debugging. */
    last->pass_handle = pass->handle;
  }
  pass->shgroups.last = last;
}

void DRW_pass_sort_shgroup_reverse(DRWPass *pass)
{
  pass->shgroups.last = pass->shgroups.first;
  /* WARNING: Assume that DRWShadingGroup->next is the first member. */
  BLI_linklist_reverse((LinkNode **)&pass->shgroups.first);
}

/** \} */
